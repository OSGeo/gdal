/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2021, CLS
 * Copyright (c) 2022, Planet Labs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdalgeoloc.h"
#include "gdalgeolocquadtree.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_quad_tree.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "memdataset.h"

//#define DEBUG_GEOLOC

#ifdef DEBUG_GEOLOC
#include "ogrsf_frmts.h"
#endif

#ifdef DEBUG_GEOLOC
#warning "Remove me before committing"
#endif

CPL_CVSID("$Id$")

CPL_C_START
CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg );
void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree );
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                         GDALGeoLocTransformer                        */
/* ==================================================================== */
/************************************************************************/

constexpr float INVALID_BMXY = -10.0f;

/************************************************************************/
/*                           UnshiftGeoX()                              */
/************************************************************************/

// Renormalize longitudes to [-180,180] range
static double UnshiftGeoX(const GDALGeoLocTransformInfo *psTransform,
                          double dfX)
{
    if( !psTransform->bGeographicSRSWithMinus180Plus180LongRange )
        return dfX;
    if( dfX > 180 )
        return dfX - 360;
    if( dfX < -180 )
        return dfX + 360;
    return dfX;
}

/************************************************************************/
/*                           UpdateMinMax()                             */
/************************************************************************/

inline void UpdateMinMax(GDALGeoLocTransformInfo *psTransform,
                         double dfGeoLocX,
                         double dfGeoLocY)
{
    if( dfGeoLocX < psTransform->dfMinX )
    {
        psTransform->dfMinX = dfGeoLocX;
        psTransform->dfYAtMinX = dfGeoLocY;
    }
    if( dfGeoLocX > psTransform->dfMaxX )
    {
        psTransform->dfMaxX = dfGeoLocX;
        psTransform->dfYAtMaxX = dfGeoLocY;
    }
    if( dfGeoLocY < psTransform->dfMinY )
    {
        psTransform->dfMinY = dfGeoLocY;
        psTransform->dfXAtMinY = dfGeoLocX;
    }
    if( dfGeoLocY > psTransform->dfMaxY )
    {
        psTransform->dfMaxY = dfGeoLocY;
        psTransform->dfXAtMaxY = dfGeoLocX;
    }
}

/************************************************************************/
/*                                Clamp()                               */
/************************************************************************/

inline double Clamp(double v, double minV, double maxV)
{
    return std::min(std::max(v, minV), maxV);
}

/************************************************************************/
/*                         GeoLocLoadFullData()                         */
/************************************************************************/

static bool GeoLocLoadFullData( GDALGeoLocTransformInfo *psTransform,
                                CSLConstList papszGeolocationInfo )

{
    const int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    const int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    const int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    const int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );

    // Is it a regular grid ? That is:
    // The XBAND contains the x coordinates for all lines.
    // The YBAND contains the y coordinates for all columns.
    const bool bIsRegularGrid = ( nYSize_XBand == 1 && nYSize_YBand == 1 );

    const int nXSize = nXSize_XBand;
    int nYSize = 0;
    if( bIsRegularGrid )
    {
        nYSize = nXSize_YBand;
    }
    else
    {
        nYSize = nYSize_XBand;
    }

    psTransform->nGeoLocXSize = static_cast<size_t>(nXSize);
    psTransform->nGeoLocYSize = static_cast<size_t>(nYSize);

    psTransform->padfGeoLocY = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));
    psTransform->padfGeoLocX = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));

    if( psTransform->padfGeoLocX == nullptr ||
        psTransform->padfGeoLocY == nullptr )
    {
        return false;
    }

    if( bIsRegularGrid )
    {
        // Case of regular grid.
        // The XBAND contains the x coordinates for all lines.
        // The YBAND contains the y coordinates for all columns.

        double* padfTempX = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nXSize, sizeof(double)));
        double* padfTempY = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nYSize, sizeof(double)));
        if( padfTempX == nullptr || padfTempY == nullptr )
        {
            CPLFree(padfTempX);
            CPLFree(padfTempY);
            return false;
        }

        CPLErr eErr =
            GDALRasterIO( psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, 1,
                          padfTempX, nXSize, 1,
                          GDT_Float64, 0, 0 );

        for( size_t j = 0; j < static_cast<size_t>(nYSize); j++ )
        {
            memcpy( psTransform->padfGeoLocX + j * nXSize,
                    padfTempX,
                    nXSize * sizeof(double) );
        }

        if( eErr == CE_None )
        {
            eErr = GDALRasterIO( psTransform->hBand_Y, GF_Read,
                                 0, 0, nYSize, 1,
                                 padfTempY, nYSize, 1,
                                 GDT_Float64, 0, 0 );

            for( size_t j = 0; j < static_cast<size_t>(nYSize); j++ )
            {
                for( size_t i = 0; i < static_cast<size_t>(nXSize); i++ )
                {
                    psTransform->padfGeoLocY[j * nXSize + i] = padfTempY[j];
                }
            }
        }

        CPLFree(padfTempX);
        CPLFree(padfTempY);

        if( eErr != CE_None )
            return false;
    }
    else
    {
        if( GDALRasterIO( psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, nYSize,
                          psTransform->padfGeoLocX, nXSize, nYSize,
                          GDT_Float64, 0, 0 ) != CE_None
            || GDALRasterIO( psTransform->hBand_Y, GF_Read,
                             0, 0, nXSize, nYSize,
                             psTransform->padfGeoLocY, nXSize, nYSize,
                             GDT_Float64, 0, 0 ) != CE_None )
            return false;
    }

    psTransform->dfNoDataX =
        GDALGetRasterNoDataValue( psTransform->hBand_X,
                                  &(psTransform->bHasNoData) );

/* -------------------------------------------------------------------- */
/*      Scan forward map for lat/long extents.                          */
/* -------------------------------------------------------------------- */
    psTransform->dfMinX = std::numeric_limits<double>::max();
    psTransform->dfMaxX = -std::numeric_limits<double>::max();
    psTransform->dfMinY = std::numeric_limits<double>::max();
    psTransform->dfMaxY = -std::numeric_limits<double>::max();

    const size_t nXYCount = psTransform->nGeoLocXSize * psTransform->nGeoLocYSize;
    for( size_t i = 0; i < nXYCount; i++ )
    {
        if( !psTransform->bHasNoData ||
            psTransform->padfGeoLocX[i] != psTransform->dfNoDataX )
        {
            UpdateMinMax(psTransform,
                         psTransform->padfGeoLocX[i],
                         psTransform->padfGeoLocY[i]);
        }
    }

    // Check if the SRS is geographic and the geoloc longitudes are in [-180,180]
    psTransform->bGeographicSRSWithMinus180Plus180LongRange = false;
    const char* pszSRS = CSLFetchNameValue( papszGeolocationInfo, "SRS" );
    if( pszSRS && psTransform->dfMinX >= -180.0 && psTransform->dfMaxX <= 180.0 &&
        !psTransform->bSwapXY )
    {
        OGRSpatialReference oSRS;
        psTransform->bGeographicSRSWithMinus180Plus180LongRange =
            oSRS.importFromWkt(pszSRS) == OGRERR_NONE &&
            CPL_TO_BOOL(oSRS.IsGeographic());
    }


#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        auto poDS = std::unique_ptr<GDALDataset>(GDALDriver::FromHandle(
            GDALGetDriverByName("ESRI Shapefile"))->
                Create("/tmp/geoloc_poly.shp", 0, 0, 0, GDT_Unknown, nullptr));
        auto poLayer = poDS->CreateLayer("geoloc_poly", nullptr, wkbPolygon, nullptr);
        auto poLayerDefn = poLayer->GetLayerDefn();
        OGRFieldDefn fieldX("x", OFTInteger);
        poLayer->CreateField(&fieldX);
        OGRFieldDefn fieldY("y", OFTInteger);
        poLayer->CreateField(&fieldY);
        for( size_t iY = 0; iY + 1 < psTransform->nGeoLocYSize; iY++ )
        {
            for( size_t iX = 0; iX + 1 < psTransform->nGeoLocXSize; iX++ )
            {
                double x0, y0, x1, y1, x2, y2, x3, y3;
                if( !GDALGeoLocPosPixelLineToXY(psTransform, iX, iY, x0, y0) ||
                    !GDALGeoLocPosPixelLineToXY(psTransform, iX+1, iY, x2, y2) ||
                    !GDALGeoLocPosPixelLineToXY(psTransform, iX, iY+1, x1, y1) ||
                    !GDALGeoLocPosPixelLineToXY(psTransform, iX+1, iY+1, x3, y3) )
                {
                    break;
                }
                if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
                    std::fabs(x0) > 170 &&
                    std::fabs(x1) > 170 &&
                    std::fabs(x2) > 170 &&
                    std::fabs(x3) > 170 &&
                    (std::fabs(x1-x0) > 180 ||
                     std::fabs(x2-x0) > 180 ||
                     std::fabs(x3-x0) > 180) )
                {
                    OGRPolygon* poPoly = new OGRPolygon();
                    OGRLinearRing* poRing = new OGRLinearRing();
                    poRing->addPoint(x0 > 0 ? x0 : x0+360, y0);
                    poRing->addPoint(x2 > 0 ? x2 : x2+360, y2);
                    poRing->addPoint(x3 > 0 ? x3 : x3+360, y3);
                    poRing->addPoint(x1 > 0 ? x1 : x1+360, y1);
                    poRing->addPoint(x0 > 0 ? x0 : x0+360, y0);
                    poPoly->addRingDirectly(poRing);
                    auto poFeature = cpl::make_unique<OGRFeature>(poLayerDefn);
                    poFeature->SetField(0, static_cast<int>(iX));
                    poFeature->SetField(1, static_cast<int>(iY));
                    poFeature->SetGeometryDirectly(poPoly);
                    CPL_IGNORE_RET_VAL(poLayer->CreateFeature(poFeature.get()));
                    if( x0 > 0 ) x0 -= 360;
                    if( x1 > 0 ) x1 -= 360;
                    if( x2 > 0 ) x2 -= 360;
                    if( x3 > 0 ) x3 -= 360;
                }

                OGRPolygon* poPoly = new OGRPolygon();
                OGRLinearRing* poRing = new OGRLinearRing();
                poRing->addPoint(x0, y0);
                poRing->addPoint(x2, y2);
                poRing->addPoint(x3, y3);
                poRing->addPoint(x1, y1);
                poRing->addPoint(x0, y0);
                poPoly->addRingDirectly(poRing);
                auto poFeature = cpl::make_unique<OGRFeature>(poLayerDefn);
                poFeature->SetField(0, static_cast<int>(iX));
                poFeature->SetField(1, static_cast<int>(iY));
                poFeature->SetGeometryDirectly(poPoly);
                CPL_IGNORE_RET_VAL(poLayer->CreateFeature(poFeature.get()));
            }
        }
    }
#endif

    if( psTransform->bOriginIsTopLeftCorner )
    {
        // Add "virtual" edge at Y=nGeoLocYSize
        for( size_t iX = 0; iX <= psTransform->nGeoLocXSize; iX++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    static_cast<double>(psTransform->nGeoLocYSize),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        // Add "virtual" edge at X=nGeoLocXSize
        for( size_t iY = 0; iY <= psTransform->nGeoLocYSize; iY++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    static_cast<double>(psTransform->nGeoLocXSize),
                    static_cast<double>(iY),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }
    }
    else
    {
        // Extend by half-pixel on 4 edges for pixel-center convention

        for( size_t iX = 0; iX <= psTransform->nGeoLocXSize; iX ++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    -0.5,
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);

            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    static_cast<double>(psTransform->nGeoLocYSize-1 + 0.5),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        for( size_t iY = 0; iY <= psTransform->nGeoLocYSize; iY++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    -0.5,
                    static_cast<double>(iY),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);

            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                    psTransform->nGeoLocXSize-1+0.5,
                    static_cast<double>(iY),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }
    }

    return true;
}

/************************************************************************/
/*                  GDALInverseBilinearInterpolation()                  */
/************************************************************************/

// (i,j) before the call should correspond to the input coordinates that give
// (x0,y0) as output of the forward interpolation
// After the call it will be updated to the input coordinates that give (x,y)
// This assumes that (x,y) is within the polygon formed by
// (x0, y0), (x2, y2), (x3, y3), (x1, y1), (x0, y0)
void GDALInverseBilinearInterpolation(const double x,
                                      const double y,
                                      const double x0,
                                      const double y0,
                                      const double x1,
                                      const double y1,
                                      const double x2,
                                      const double y2,
                                      const double x3,
                                      const double y3,
                                      double& i,
                                      double& j)
{
    // Exact inverse bilinear interpolation method.
    // Maths from https://stackoverflow.com/a/812077

    const double A = (x0 - x) * (y0 - y2) - (y0 - y) * (x0 - x2);
    const double B = ( ((x0 - x) * (y1 - y3) - (y0 - y) * (x1 - x3)) +
                       ((x1 - x) * (y0 - y2) - (y1 - y) * (x0 - x2)) ) / 2;
    const double C = (x1 - x) * (y1 - y3) - (y1 - y) * (x1 - x3);
    const double denom = A - 2 * B + C;
    double s;
    if( fabs(denom) < 1e-12 )
    {
        // Happens typically when the x_i,y_i points form a rectangle
        s = A / (A-C);
    }
    else
    {
        const double sqrtTerm = sqrt(B * B - A * C);
        const double s1 = ((A-B) + sqrtTerm) / denom;
        const double s2 = ((A-B) - sqrtTerm) / denom;
        if( s1 < 0 || s1 > 1 )
            s = s2;
        else
            s = s1;
    }
    const double t = ( (1-s)*(x0-x) + s*(x1-x) ) / ( (1-s)*(x0-x2) + s*(x1-x3) );

    i += t;
    j += s;
}

/************************************************************************/
/*                       GeoLocGenerateBackMap()                        */
/************************************************************************/

static bool GeoLocGenerateBackMap( GDALGeoLocTransformInfo *psTransform )

{
    CPLDebug("GEOLOC", "Starting backmap generation");
    const size_t nXSize = psTransform->nGeoLocXSize;
    const size_t nYSize = psTransform->nGeoLocYSize;

/* -------------------------------------------------------------------- */
/*      Decide on resolution for backmap.  We aim for slightly          */
/*      higher resolution than the source but we can't easily           */
/*      establish how much dead space there is in the backmap, so it    */
/*      is approximate.                                                 */
/* -------------------------------------------------------------------- */
    const double dfTargetPixels =
        static_cast<double>(nXSize) * nYSize * psTransform->dfOversampleFactor;
    const double dfPixelSizeSquare = sqrt(
        (psTransform->dfMaxX - psTransform->dfMinX) *
        (psTransform->dfMaxY - psTransform->dfMinY) / dfTargetPixels);
    if( dfPixelSizeSquare == 0.0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid pixel size for backmap");
        return false;
    }

    const double dfMinX = psTransform->dfMinX - dfPixelSizeSquare / 2.0;
    const double dfMaxX = psTransform->dfMaxX + dfPixelSizeSquare / 2.0;
    const double dfMaxY = psTransform->dfMaxY + dfPixelSizeSquare / 2.0;
    const double dfMinY = psTransform->dfMinY - dfPixelSizeSquare / 2.0;
    const double dfBMXSize = std::ceil((dfMaxX - dfMinX) / dfPixelSizeSquare);
    const double dfBMYSize = std::ceil((dfMaxY - dfMinY) / dfPixelSizeSquare);

    if( !(dfBMXSize > 0 && dfBMXSize + 1 < INT_MAX) ||
        !(dfBMYSize > 0 && dfBMYSize + 1 < INT_MAX) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %f x %f",
                 dfBMXSize, dfBMYSize);
        return false;
    }

    size_t nBMXSize = static_cast<size_t>(dfBMXSize);
    size_t nBMYSize = static_cast<size_t>(dfBMYSize);

    if( 1 + nBMYSize > std::numeric_limits<size_t>::max() / (1 + nBMXSize) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %f x %f",
                 dfBMXSize, dfBMYSize);
        return false;
    }

    const double dfPixelXSize = (dfMaxX - dfMinX) / nBMXSize;
    const double dfPixelYSize = (dfMaxY - dfMinY) / nBMYSize;

    // Extra pixel for right-edge and bottom-edge extensions in TOP_LEFT_CORNER
    // convention.
    nBMXSize ++;
    nBMYSize ++;
    psTransform->nBackMapWidth = nBMXSize;
    psTransform->nBackMapHeight = nBMYSize;

    psTransform->adfBackMapGeoTransform[0] = dfMinX;
    psTransform->adfBackMapGeoTransform[1] = dfPixelXSize;
    psTransform->adfBackMapGeoTransform[2] = 0.0;
    psTransform->adfBackMapGeoTransform[3] = dfMaxY;
    psTransform->adfBackMapGeoTransform[4] = 0.0;
    psTransform->adfBackMapGeoTransform[5] = -dfPixelYSize;

/* -------------------------------------------------------------------- */
/*      Allocate backmap.                                               */
/* -------------------------------------------------------------------- */
    psTransform->pafBackMapX = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nBMXSize, nBMYSize, sizeof(float)));
    psTransform->pafBackMapY = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nBMXSize, nBMYSize, sizeof(float)));

    float *wgtsBackMap = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nBMXSize, nBMYSize, sizeof(float)));

    if( psTransform->pafBackMapX == nullptr ||
        psTransform->pafBackMapY == nullptr ||
        wgtsBackMap == nullptr)
    {
        CPLFree( wgtsBackMap );
        return false;
    }

    const size_t nBMXYCount = nBMXSize * nBMYSize;
    for( size_t i = 0; i < nBMXYCount; i++ )
    {
        psTransform->pafBackMapX[i] = 0;
        psTransform->pafBackMapY[i] = 0;
        wgtsBackMap[i] = 0.0;
    }

    const double dfGeorefConventionOffset = psTransform->bOriginIsTopLeftCorner ? 0 : 0.5;

    const auto UpdateBackmap = [&](std::ptrdiff_t iBMX, std::ptrdiff_t iBMY,
                                   double dfX, double dfY,
                                   double tempwt)
    {
        const float fUpdatedBMX = psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] +
            static_cast<float>( tempwt * (
                (dfX + dfGeorefConventionOffset) * psTransform->dfPIXEL_STEP +
                psTransform->dfPIXEL_OFFSET));
        const float fUpdatedBMY = psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] +
            static_cast<float>( tempwt * (
                (dfY + dfGeorefConventionOffset) * psTransform->dfLINE_STEP +
                psTransform->dfLINE_OFFSET));
        const float fUpdatedWeight = wgtsBackMap[iBMX + iBMY * nBMXSize] +
                               static_cast<float>(tempwt);

        // Only update the backmap if the updated averaged value results in a
        // geoloc position that isn't too different from the original one.
        // (there's no guarantee that if padfGeoLocX[i] ~= padfGeoLoc[j],
        //  padfGeoLoc[alpha * i + (1 - alpha) * j] ~= padfGeoLoc[i] )
        if( fUpdatedWeight > 0 )
        {
            const float fX = fUpdatedBMX / fUpdatedWeight;
            const float fY = fUpdatedBMY / fUpdatedWeight;
            const double dfGeoLocPixel = (fX - psTransform->dfPIXEL_OFFSET) / psTransform->dfPIXEL_STEP -
                dfGeorefConventionOffset;
            const double dfGeoLocLine = (fY - psTransform->dfLINE_OFFSET) / psTransform->dfLINE_STEP -
                dfGeorefConventionOffset;
            size_t iXAvg = static_cast<size_t>(std::max(0.0, dfGeoLocPixel));
            iXAvg = std::min(iXAvg, psTransform->nGeoLocXSize-1);
            size_t iYAvg = static_cast<size_t>(std::max(0.0, dfGeoLocLine));
            iYAvg = std::min(iYAvg, psTransform->nGeoLocYSize-1);
            const double dfGLX = psTransform->padfGeoLocX[iXAvg + iYAvg * nXSize];
            const double dfGLY = psTransform->padfGeoLocY[iXAvg + iYAvg * nXSize];

            const size_t iX = static_cast<size_t>(dfX);
            const size_t iY = static_cast<size_t>(dfY);
            if( !(psTransform->bHasNoData &&
                  dfGLX == psTransform->dfNoDataX ) &&
                ((iX >= nXSize - 1 || iY >= nYSize - 1) ||
                 (fabs(dfGLX - psTransform->padfGeoLocX[iX + iY * nXSize]) <= 2 * dfPixelXSize &&
                  fabs(dfGLY - psTransform->padfGeoLocY[iX + iY * nXSize]) <= 2 * dfPixelYSize)) )
            {
                psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] = fUpdatedBMX;
                psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] = fUpdatedBMY;
                wgtsBackMap[iBMX + iBMY * nBMXSize] = fUpdatedWeight;
            }
        }
    };

    // Keep those objects in this outer scope, so they are re-used, to
    // save memory allocations.
    OGRPoint oPoint;
    OGRLinearRing oRing;
    oRing.setNumPoints(5);

/* -------------------------------------------------------------------- */
/*      Run through the whole geoloc array forward projecting and       */
/*      pushing into the backmap.                                       */
/* -------------------------------------------------------------------- */

    // Iterate over the (i,j) pixel space of the geolocation array, in a sufficiently
    // dense way that if the geolocation array expressed an affine transformation,
    // we would hit every node of the backmap.
    const double dfStep = 1. / psTransform->dfOversampleFactor;
    for( double dfY = -dfStep; dfY <= static_cast<double>(nYSize) + 2 * dfStep; dfY += dfStep )
    {
        for( double dfX = -dfStep; dfX <= static_cast<double>(nXSize) + 2 * dfStep; dfX += dfStep )
        {
            // Use forward geolocation array interpolation to compute the
            // georeferenced position corresponding to (dfX, dfY)
            double dfGeoLocX;
            double dfGeoLocY;
            if( !GDALGeoLocPosPixelLineToXY(psTransform, dfX, dfY, dfGeoLocX, dfGeoLocY) )
                continue;

            // Compute the floating point coordinates in the pixel space of the backmap
            const double dBMX = static_cast<double>(
                    (dfGeoLocX - dfMinX) / dfPixelXSize);

            const double dBMY = static_cast<double>(
                (dfMaxY - dfGeoLocY) / dfPixelYSize);

            //Get top left index by truncation
            const std::ptrdiff_t iBMX = static_cast<std::ptrdiff_t>(std::floor(dBMX));
            const std::ptrdiff_t iBMY = static_cast<std::ptrdiff_t>(std::floor(dBMY));

            if( iBMX >= 0 && static_cast<size_t>(iBMX) < nBMXSize &&
                iBMY >= 0 && static_cast<size_t>(iBMY) < nBMYSize )
            {
                // Compute the georeferenced position of the top-left index of
                // the backmap
                double dfGeoX = dfMinX + iBMX * dfPixelXSize;
                const double dfGeoY = dfMaxY - iBMY * dfPixelYSize;

                bool bMatchingGeoLocCellFound = false;

                const int nOuterIters = psTransform->bGeographicSRSWithMinus180Plus180LongRange && fabs(dfGeoX) >= 180 ? 2 : 1;

                for( int iOuterIter = 0; iOuterIter < nOuterIters; ++iOuterIter)
                {
                    if( iOuterIter == 1 && dfGeoX >= 180 )
                        dfGeoX -= 360;
                    else if( iOuterIter == 1 && dfGeoX <= -180 )
                        dfGeoX += 360;

                    // Identify a cell (quadrilateral in georeferenced space) in
                    // the geolocation array in which dfGeoX, dfGeoY falls into.
                    oPoint.setX(dfGeoX);
                    oPoint.setY(dfGeoY);
                    for( int sx = -1; !bMatchingGeoLocCellFound && sx <= 0; sx++ )
                    {
                        for(int sy = -1; !bMatchingGeoLocCellFound && sy <= 0; sy++)
                        {
                            const double pixel = std::floor(dfX) + sx;
                            const double line = std::floor(dfY) + sy;
                            double x0, y0, x1, y1, x2, y2, x3, y3;
                            if( !GDALGeoLocPosPixelLineToXY(psTransform, pixel, line, x0, y0) ||
                                !GDALGeoLocPosPixelLineToXY(psTransform, pixel+1, line, x2, y2) ||
                                !GDALGeoLocPosPixelLineToXY(psTransform, pixel, line+1, x1, y1) ||
                                !GDALGeoLocPosPixelLineToXY(psTransform, pixel+1, line+1, x3, y3) )
                            {
                                break;
                            }

                            int nIters = 1;
                            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
                                std::fabs(x0) > 170 &&
                                std::fabs(x1) > 170 &&
                                std::fabs(x2) > 170 &&
                                std::fabs(x3) > 170 &&
                                (std::fabs(x1-x0) > 180 ||
                                 std::fabs(x2-x0) > 180 ||
                                 std::fabs(x3-x0) > 180) )
                            {
                                nIters = 2;
                                if( x0 > 0 ) x0 -= 360;
                                if( x1 > 0 ) x1 -= 360;
                                if( x2 > 0 ) x2 -= 360;
                                if( x3 > 0 ) x3 -= 360;
                            }
                            for( int iIter = 0; iIter < nIters; ++iIter )
                            {
                                if( iIter == 1 )
                                {
                                    x0 += 360;
                                    x1 += 360;
                                    x2 += 360;
                                    x3 += 360;
                                }

                                oRing.setPoint(0, x0, y0);
                                oRing.setPoint(1, x2, y2);
                                oRing.setPoint(2, x3, y3);
                                oRing.setPoint(3, x1, y1);
                                oRing.setPoint(4, x0, y0);
                                if( oRing.isPointInRing( &oPoint ) ||
                                    oRing.isPointOnRingBoundary( &oPoint ) )
                                {
                                    bMatchingGeoLocCellFound = true;
                                    double dfBMXValue = pixel;
                                    double dfBMYValue = line;
                                    GDALInverseBilinearInterpolation(dfGeoX, dfGeoY,
                                                                 x0, y0,
                                                                 x1, y1,
                                                                 x2, y2,
                                                                 x3, y3,
                                                                 dfBMXValue, dfBMYValue);

                                    dfBMXValue = (dfBMXValue + dfGeorefConventionOffset) *
                                        psTransform->dfPIXEL_STEP + psTransform->dfPIXEL_OFFSET ;
                                    dfBMYValue = (dfBMYValue + dfGeorefConventionOffset) *
                                        psTransform->dfLINE_STEP + psTransform->dfLINE_OFFSET ;

                                    psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] = static_cast<float>(dfBMXValue);
                                    psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] = static_cast<float>(dfBMYValue);
                                    wgtsBackMap[iBMX + iBMY * nBMXSize] = 1.0f;
                                }
                            }
                        }
                    }
                }
                if( bMatchingGeoLocCellFound )
                    continue;
            }

            // We will end up here in non-nominal cases, with nodata, holes,
            // etc.

            //Check if the center is in range
            if( iBMX < -1 || iBMY < -1 ||
                (iBMX > 0 && static_cast<size_t>(iBMX) > nBMXSize) ||
                (iBMY > 0 && static_cast<size_t>(iBMY) > nBMYSize) )
                continue;

            const double fracBMX = dBMX - iBMX;
            const double fracBMY = dBMY - iBMY;

            //Check logic for top left pixel
            if ((iBMX >= 0) && (iBMY >= 0) &&
                (static_cast<size_t>(iBMX) < nBMXSize) &&
                (static_cast<size_t>(iBMY) < nBMYSize) &&
                wgtsBackMap[iBMX + iBMY * nBMXSize] != 1.0f )
            {
                const double tempwt = (1.0 - fracBMX) * (1.0 - fracBMY);
                UpdateBackmap(iBMX, iBMY, dfX, dfY, tempwt);
            }

            //Check logic for top right pixel
            if ((iBMY >= 0) &&
                (static_cast<size_t>(iBMX+1) < nBMXSize) &&
                (static_cast<size_t>(iBMY) < nBMYSize) &&
                wgtsBackMap[iBMX + 1 + iBMY * nBMXSize] != 1.0f )
            {
                const double tempwt = fracBMX * (1.0 - fracBMY);
                UpdateBackmap(iBMX + 1, iBMY, dfX, dfY, tempwt);
            }

            //Check logic for bottom right pixel
            if ((static_cast<size_t>(iBMX+1) < nBMXSize) &&
                (static_cast<size_t>(iBMY+1) < nBMYSize) &&
                wgtsBackMap[(iBMX + 1) + (iBMY + 1) * nBMXSize] != 1.0f )
            {
                const double tempwt = fracBMX * fracBMY;
                UpdateBackmap(iBMX + 1, iBMY + 1, dfX, dfY, tempwt);
            }

            //Check logic for bottom left pixel
            if ((iBMX >= 0) &&
                (static_cast<size_t>(iBMX) < nBMXSize) &&
                (static_cast<size_t>(iBMY+1) < nBMYSize) &&
                wgtsBackMap[iBMX + (iBMY + 1) * nBMXSize] != 1.0f )
            {
                const double tempwt = (1.0 - fracBMX) * fracBMY;
                UpdateBackmap(iBMX, iBMY + 1, dfX, dfY, tempwt);
            }

        }
    }


    //Each pixel in the backmap may have multiple entries.
    //We now go in average it out using the weights
    for( size_t i = 0; i < nBMXYCount; i++ )
    {
        //Check if pixel was only touch during neighbor scan
        //But no real weight was added as source point matched
        //backmap grid node
        if (wgtsBackMap[i] > 0)
        {
            psTransform->pafBackMapX[i] /= wgtsBackMap[i];
            psTransform->pafBackMapY[i] /= wgtsBackMap[i];
        }
        else
        {
            psTransform->pafBackMapX[i] = INVALID_BMXY;
            psTransform->pafBackMapY[i] = INVALID_BMXY;
        }
    }

    // Fill holes in backmap
    auto poMEMDS = std::unique_ptr<GDALDataset>(
          MEMDataset::Create( "",
                              static_cast<int>(nBMXSize),
                              static_cast<int>(nBMYSize),
                              0, GDT_Float32, nullptr ));

    for( int i = 1; i <= 2; i++ )
    {
        char szBuffer[32] = { '\0' };
        char szBuffer0[64] = { '\0' };
        char* apszOptions[] = { szBuffer0, nullptr };

        void* ptr = (i == 1) ? psTransform->pafBackMapX : psTransform->pafBackMapY;
        szBuffer[CPLPrintPointer(szBuffer, ptr, sizeof(szBuffer))] = '\0';
        snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);
        poMEMDS->AddBand(GDT_Float32, apszOptions);
        poMEMDS->GetRasterBand(i)->SetNoDataValue(INVALID_BMXY);
    }

#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        poMEMDS->SetGeoTransform(psTransform->adfBackMapGeoTransform);
        GDALClose(GDALCreateCopy(GDALGetDriverByName("GTiff"),
                              "/tmp/geoloc_before_fill.tif",
                              poMEMDS.get(),
                              false, nullptr, nullptr, nullptr));
    }
#endif

    constexpr double dfMaxSearchDist = 3.0;
    constexpr int nSmoothingIterations = 1;
    for( int i = 1; i <= 2; i++ )
    {
        GDALFillNodata( GDALRasterBand::ToHandle(poMEMDS->GetRasterBand(i)),
                        nullptr,
                        dfMaxSearchDist,
                        0, // unused parameter
                        nSmoothingIterations,
                        nullptr,
                        nullptr,
                        nullptr );
    }

#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        GDALClose(GDALCreateCopy(GDALGetDriverByName("GTiff"),
                              "/tmp/geoloc_after_fill.tif",
                              poMEMDS.get(),
                              false, nullptr, nullptr, nullptr));
    }
#endif

    // A final hole filling logic, proceeding line by line, and feeling
    // holes when the backmap values surrounding the hole are close enough.
    for( size_t iBMY = 0; iBMY < nBMYSize; iBMY++ )
    {
        size_t iLastValidIX = static_cast<size_t>(-1);
        for( size_t iBMX = 0; iBMX < nBMXSize; iBMX++ )
        {
            const size_t iBM = iBMX + iBMY * nBMXSize;
            if( psTransform->pafBackMapX[iBM] == INVALID_BMXY )
                continue;
            if( iLastValidIX != static_cast<size_t>(-1) &&
                iBMX > iLastValidIX + 1 &&
                fabs( psTransform->pafBackMapX[iBM] -
                    psTransform->pafBackMapX[iLastValidIX + iBMY * nBMXSize]) <= 2 &&
                fabs( psTransform->pafBackMapY[iBM] -
                    psTransform->pafBackMapY[iLastValidIX + iBMY * nBMXSize]) <= 2 )
            {
                for( size_t iBMXInner = iLastValidIX + 1; iBMXInner < iBMX; ++iBMXInner )
                {
                    const float alpha = static_cast<float>(iBMXInner - iLastValidIX) / (iBMX - iLastValidIX);
                    psTransform->pafBackMapX[iBMXInner + iBMY * nBMXSize] =
                        (1.0f - alpha) * psTransform->pafBackMapX[iLastValidIX + iBMY * nBMXSize] +
                        alpha * psTransform->pafBackMapX[iBM];
                    psTransform->pafBackMapY[iBMXInner + iBMY * nBMXSize] =
                        (1.0f - alpha) * psTransform->pafBackMapY[iLastValidIX + iBMY * nBMXSize] +
                        alpha * psTransform->pafBackMapY[iBM];
                }
            }
            iLastValidIX = iBMX;
        }
    }

#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        GDALClose(GDALCreateCopy(GDALGetDriverByName("GTiff"),
                              "/tmp/geoloc_after_line_fill.tif",
                              poMEMDS.get(),
                              false, nullptr, nullptr, nullptr));
    }
#endif

    CPLFree( wgtsBackMap );
    CPLDebug("GEOLOC", "Ending backmap generation");

    return true;
}

/************************************************************************/
/*                       GDALGeoLocRescale()                            */
/************************************************************************/

static void GDALGeoLocRescale( char**& papszMD, const char* pszItem,
                               double dfRatio, double dfDefaultVal )
{
    const double dfVal =
        dfRatio *
        CPLAtofM(CSLFetchNameValueDef(papszMD, pszItem,
                                      CPLSPrintf("%.18g", dfDefaultVal)));

    papszMD = CSLSetNameValue(papszMD, pszItem, CPLSPrintf("%.18g", dfVal));

}

/************************************************************************/
/*                 GDALCreateSimilarGeoLocTransformer()                 */
/************************************************************************/

static
void* GDALCreateSimilarGeoLocTransformer( void *hTransformArg,
                                          double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1(hTransformArg, "GDALCreateSimilarGeoLocTransformer",
                      nullptr);

    GDALGeoLocTransformInfo *psInfo =
        static_cast<GDALGeoLocTransformInfo *>(hTransformArg);

    char** papszGeolocationInfo = CSLDuplicate(psInfo->papszGeolocationInfo);

    if( dfRatioX != 1.0 || dfRatioY != 1.0 )
    {
        GDALGeoLocRescale(papszGeolocationInfo, "PIXEL_OFFSET", dfRatioX, 0.0);
        GDALGeoLocRescale(papszGeolocationInfo, "LINE_OFFSET", dfRatioY, 0.0);
        GDALGeoLocRescale(
            papszGeolocationInfo, "PIXEL_STEP", 1.0 / dfRatioX, 1.0);
        GDALGeoLocRescale(papszGeolocationInfo,
            "LINE_STEP", 1.0 / dfRatioY, 1.0);
    }

    auto psInfoNew = static_cast<GDALGeoLocTransformInfo*>(
        GDALCreateGeoLocTransformer(
            nullptr, papszGeolocationInfo, psInfo->bReversed));
    psInfoNew->dfOversampleFactor = psInfo->dfOversampleFactor;

    CSLDestroy(papszGeolocationInfo);

    return psInfoNew;
}

/************************************************************************/
/*                    GDALCreateGeoLocTransformer()                     */
/************************************************************************/

void *GDALCreateGeoLocTransformerEx( GDALDatasetH hBaseDS,
                                     char **papszGeolocationInfo,
                                     int bReversed,
                                     const char* pszSourceDataset,
                                     CSLConstList papszTransformOptions )

{

    if( CSLFetchNameValue(papszGeolocationInfo, "PIXEL_OFFSET") == nullptr
        || CSLFetchNameValue(papszGeolocationInfo, "LINE_OFFSET") == nullptr
        || CSLFetchNameValue(papszGeolocationInfo, "PIXEL_STEP") == nullptr
        || CSLFetchNameValue(papszGeolocationInfo, "LINE_STEP") == nullptr
        || CSLFetchNameValue(papszGeolocationInfo, "X_BAND") == nullptr
        || CSLFetchNameValue(papszGeolocationInfo, "Y_BAND") == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing some geolocation fields in "
                  "GDALCreateGeoLocTransformer()" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Initialize core info.                                           */
/* -------------------------------------------------------------------- */
    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(
            CPLCalloc(sizeof(GDALGeoLocTransformInfo), 1));

    psTransform->bReversed = CPL_TO_BOOL(bReversed);
    psTransform->dfOversampleFactor = std::max(0.1, std::min(2.0, CPLAtof(
        CSLFetchNameValueDef(papszTransformOptions, "GEOLOC_BACKMAP_OVERSAMPLE_FACTOR",
                             CPLGetConfigOption("GDAL_GEOLOC_BACKMAP_OVERSAMPLE_FACTOR", "1.3")))));

    memcpy( psTransform->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
    psTransform->sTI.pszClassName = "GDALGeoLocTransformer";
    psTransform->sTI.pfnTransform = GDALGeoLocTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyGeoLocTransformer;
    psTransform->sTI.pfnSerialize = GDALSerializeGeoLocTransformer;
    psTransform->sTI.pfnCreateSimilar = GDALCreateSimilarGeoLocTransformer;

    psTransform->papszGeolocationInfo = CSLDuplicate( papszGeolocationInfo );

/* -------------------------------------------------------------------- */
/*      Pull geolocation info from the options/metadata.                */
/* -------------------------------------------------------------------- */
    psTransform->dfPIXEL_OFFSET =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "PIXEL_OFFSET" ));
    psTransform->dfLINE_OFFSET =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "LINE_OFFSET" ));
    psTransform->dfPIXEL_STEP =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "PIXEL_STEP" ));
    psTransform->dfLINE_STEP =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "LINE_STEP" ));

    psTransform->bOriginIsTopLeftCorner =
        EQUAL( CSLFetchNameValueDef(papszGeolocationInfo,
                                    "GEOREFERENCING_CONVENTION",
                                    "TOP_LEFT_CORNER"), "TOP_LEFT_CORNER" );

/* -------------------------------------------------------------------- */
/*      Establish access to geolocation dataset(s).                     */
/* -------------------------------------------------------------------- */
    const char *pszDSName = CSLFetchNameValue( papszGeolocationInfo,
                                               "X_DATASET" );
    if( pszDSName != nullptr )
    {
        CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
        if( CPLTestBool(CSLFetchNameValueDef(papszGeolocationInfo,
                            "X_DATASET_RELATIVE_TO_SOURCE", "NO")) &&
            (hBaseDS != nullptr || pszSourceDataset) )
        {
            CPLString osFilename = CPLProjectRelativeFilename(
                CPLGetDirname(pszSourceDataset ? pszSourceDataset : GDALGetDescription(hBaseDS)),
                   pszDSName);
            psTransform->hDS_X = GDALOpenShared( osFilename.c_str(), GA_ReadOnly );
        }
        else
        {
            psTransform->hDS_X = GDALOpenShared( pszDSName, GA_ReadOnly );
        }
    }
    else
    {
        psTransform->hDS_X = hBaseDS;
        if( hBaseDS )
        {
            GDALReferenceDataset( psTransform->hDS_X );
            psTransform->papszGeolocationInfo =
                CSLSetNameValue( psTransform->papszGeolocationInfo,
                                 "X_DATASET",
                                 GDALGetDescription( hBaseDS ) );
        }
    }

    pszDSName = CSLFetchNameValue( papszGeolocationInfo, "Y_DATASET" );
    if( pszDSName != nullptr )
    {
        CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
        if( CPLTestBool(CSLFetchNameValueDef(papszGeolocationInfo,
                            "Y_DATASET_RELATIVE_TO_SOURCE", "NO")) &&
            (hBaseDS != nullptr || pszSourceDataset) )
        {
            CPLString osFilename = CPLProjectRelativeFilename(
                CPLGetDirname(pszSourceDataset ? pszSourceDataset : GDALGetDescription(hBaseDS)),
                   pszDSName);
            psTransform->hDS_Y = GDALOpenShared( osFilename.c_str(), GA_ReadOnly );
        }
        else
        {
            psTransform->hDS_Y = GDALOpenShared( pszDSName, GA_ReadOnly );
        }
    }
    else
    {
        psTransform->hDS_Y = hBaseDS;
        if( hBaseDS )
        {
            GDALReferenceDataset( psTransform->hDS_Y );
            psTransform->papszGeolocationInfo =
                CSLSetNameValue( psTransform->papszGeolocationInfo,
                                 "Y_DATASET",
                                 GDALGetDescription( hBaseDS ) );
        }
    }

    if( psTransform->hDS_X == nullptr ||
        psTransform->hDS_Y == nullptr )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get the band handles.                                           */
/* -------------------------------------------------------------------- */
    const int nXBand =
        std::max(1, atoi(CSLFetchNameValue( papszGeolocationInfo, "X_BAND" )));
    psTransform->hBand_X = GDALGetRasterBand( psTransform->hDS_X, nXBand );

    const int nYBand =
        std::max(1, atoi(CSLFetchNameValue( papszGeolocationInfo, "Y_BAND" )));
    psTransform->hBand_Y = GDALGetRasterBand( psTransform->hDS_Y, nYBand );

    if( psTransform->hBand_X == nullptr ||
        psTransform->hBand_Y == nullptr )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

    psTransform->bSwapXY = CPLTestBool(CSLFetchNameValueDef(
        papszGeolocationInfo, "SWAP_XY", "NO"));

/* -------------------------------------------------------------------- */
/*     Check that X and Y bands have the same dimensions                */
/* -------------------------------------------------------------------- */
    const int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    const int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    const int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    const int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );
    if( nYSize_XBand == 1 || nYSize_YBand == 1 )
    {
        if( nYSize_XBand != 1 || nYSize_YBand != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "X_BAND and Y_BAND should have both nYSize == 1");
            GDALDestroyGeoLocTransformer( psTransform );
            return nullptr;
        }
    }
    else if( nXSize_XBand != nXSize_YBand ||
             nYSize_XBand != nYSize_YBand )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "X_BAND and Y_BAND do not have the same dimensions");
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

    if( static_cast<size_t>(nXSize_XBand) >
            std::numeric_limits<size_t>::max() / nYSize_XBand )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nXSize_XBand, nYSize_XBand);
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Load the geolocation array.                                     */
/* -------------------------------------------------------------------- */

    // The quadtree method is experimental. It simplifies the code significantly,
    // but unfortunately burns more RAM and is slower.
    const bool bUseQuadtree =
        EQUAL(CPLGetConfigOption("GDAL_GEOLOC_INVERSE_METHOD", "BACKMAP"), "QUADTREE");
    if( !GeoLocLoadFullData( psTransform, papszGeolocationInfo )
        || (bUseQuadtree && !GDALGeoLocBuildQuadTree( psTransform ))
        || (!bUseQuadtree && !GeoLocGenerateBackMap( psTransform )) )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

    return psTransform;
}

/** Create GeoLocation transformer */
void *GDALCreateGeoLocTransformer( GDALDatasetH hBaseDS,
                                   char **papszGeolocationInfo,
                                   int bReversed )

{
    return GDALCreateGeoLocTransformerEx( hBaseDS, papszGeolocationInfo,
                                          bReversed, nullptr, nullptr );
}

/************************************************************************/
/*                    GDALDestroyGeoLocTransformer()                    */
/************************************************************************/

/** Destroy GeoLocation transformer */
void GDALDestroyGeoLocTransformer( void *pTransformAlg )

{
    if( pTransformAlg == nullptr )
        return;

    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(pTransformAlg);

    CPLFree( psTransform->pafBackMapX );
    CPLFree( psTransform->pafBackMapY );
    CSLDestroy( psTransform->papszGeolocationInfo );
    CPLFree( psTransform->padfGeoLocX );
    CPLFree( psTransform->padfGeoLocY );

    if( psTransform->hDS_X != nullptr
        && GDALDereferenceDataset( psTransform->hDS_X ) == 0 )
            GDALClose( psTransform->hDS_X );

    if( psTransform->hDS_Y != nullptr
        && GDALDereferenceDataset( psTransform->hDS_Y ) == 0 )
            GDALClose( psTransform->hDS_Y );

    if( psTransform->hQuadTree != nullptr )
        CPLQuadTreeDestroy( psTransform->hQuadTree );

    CPLFree( pTransformAlg );
}

/************************************************************************/
/*                      GDALGeoLocPosPixelLineToXY()                    */
/************************************************************************/

/** Interpolate a position expessed as (floating point) pixel/line in the
 * geolocation array to the corresponding bilinearly-interpolated georeferenced
 * position.
 *
 * The interpolation assumes infinite extension beyond borders of available
 * data based on closest grid square.
 *
 * @param psTransform Transformation info
 * @param dfGeoLocPixel Position along the column/pixel axis of the geolocation array
 * @param dfGeoLocLine  Position along the row/line axis of the geolocation array
 * @param[out] dfX      Output X of georeferenced position.
 * @param[out] dfY      Output Y of georeferenced position.
 * @return true if success
 */
bool GDALGeoLocPosPixelLineToXY(const GDALGeoLocTransformInfo *psTransform,
                                const double dfGeoLocPixel,
                                const double dfGeoLocLine,
                                double& dfX,
                                double& dfY)
{
    const size_t nXSize = psTransform->nGeoLocXSize;
    size_t iX = static_cast<size_t>(std::max(0.0, dfGeoLocPixel));
    iX = std::min(iX, psTransform->nGeoLocXSize-1);
    size_t iY = static_cast<size_t>(std::max(0.0, dfGeoLocLine));
    iY = std::min(iY, psTransform->nGeoLocYSize-1);

    for( int iAttempt = 0; iAttempt < 2; ++iAttempt )
    {
        const double *padfGLX = psTransform->padfGeoLocX + iX + iY * nXSize;
        const double *padfGLY = psTransform->padfGeoLocY + iX + iY * nXSize;

        if( psTransform->bHasNoData &&
            padfGLX[0] == psTransform->dfNoDataX )
        {
            return false;
        }

        // This assumes infinite extension beyond borders of available
        // data based on closest grid square.
        const double dfRefX = padfGLX[0];
        if( iX + 1 < psTransform->nGeoLocXSize &&
            iY + 1 < psTransform->nGeoLocYSize &&
            (!psTransform->bHasNoData ||
                (padfGLX[1] != psTransform->dfNoDataX &&
                 padfGLX[nXSize] != psTransform->dfNoDataX &&
                 padfGLX[nXSize + 1] != psTransform->dfNoDataX) ))
        {
            dfX =
                (1 - (dfGeoLocLine -iY))
                * (padfGLX[0] +
                   (dfGeoLocPixel-iX) * (ShiftGeoX(psTransform, dfRefX, padfGLX[1]) - padfGLX[0]))
                + (dfGeoLocLine -iY)
                * (ShiftGeoX(psTransform, dfRefX, padfGLX[nXSize]) + (dfGeoLocPixel-iX) *
                   (ShiftGeoX(psTransform, dfRefX, padfGLX[nXSize+1]) -
                       ShiftGeoX(psTransform, dfRefX, padfGLX[nXSize])));
            dfX = UnshiftGeoX(psTransform, dfX);

            dfY =
                (1 - (dfGeoLocLine -iY))
                * (padfGLY[0] +
                   (dfGeoLocPixel-iX) * (padfGLY[1] - padfGLY[0]))
                + (dfGeoLocLine -iY)
                * (padfGLY[nXSize] + (dfGeoLocPixel-iX) *
                   (padfGLY[nXSize+1] - padfGLY[nXSize]));
        }
        else if( iX == psTransform->nGeoLocXSize -1 &&
                 iX >= 1 && iY + 1 < psTransform->nGeoLocYSize )
        {
            // If we are after the right edge, then go one pixel left
            // and retry
            iX --;
            continue;
        }
        else if( iY == psTransform->nGeoLocYSize - 1 &&
                 iY >= 1 && iX + 1 < psTransform->nGeoLocXSize )
        {
            // If we are after the bottom edge, then go one pixel up
            // and retry
            iY --;
            continue;
        }
        else if( iX == psTransform->nGeoLocXSize -1 &&
                 iY == psTransform->nGeoLocYSize - 1 &&
                 iX >= 1 && iY >= 1 )
        {
            // If we are after the right and bottom edge, then go one pixel left and up
            // and retry
            iX --;
            iY --;
            continue;
        }
        else if( iX + 1 < psTransform->nGeoLocXSize &&
                 (!psTransform->bHasNoData ||
                    padfGLX[1] != psTransform->dfNoDataX) )
        {
            dfX =
                padfGLX[0] + (dfGeoLocPixel-iX) * (ShiftGeoX(psTransform, dfRefX, padfGLX[1]) - padfGLX[0]);
            dfX = UnshiftGeoX(psTransform, dfX);
            dfY =
                padfGLY[0] + (dfGeoLocPixel-iX) * (padfGLY[1] - padfGLY[0]);
        }
        else if( iY + 1 < psTransform->nGeoLocYSize &&
                 (!psTransform->bHasNoData ||
                    padfGLX[nXSize] != psTransform->dfNoDataX) )
        {
            dfX = padfGLX[0]
                + (dfGeoLocLine -iY) * (ShiftGeoX(psTransform, dfRefX, padfGLX[nXSize]) - padfGLX[0]);
            dfX = UnshiftGeoX(psTransform, dfX);
            dfY = padfGLY[0]
                + (dfGeoLocLine -iY) * (padfGLY[nXSize] - padfGLY[0]);
        }
        else
        {
            dfX = padfGLX[0];
            dfY = padfGLY[0];
        }
        break;
    }
    return true;
}

/************************************************************************/
/*                        GDALGeoLocTransform()                         */
/************************************************************************/


/** Use GeoLocation transformer */
int GDALGeoLocTransform( void *pTransformArg,
                         int bDstToSrc,
                         int nPointCount,
                         double *padfX, double *padfY,
                         double * /* padfZ */,
                         int *panSuccess )
{
    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(pTransformArg);

    if( psTransform->bReversed )
        bDstToSrc = !bDstToSrc;

    const double dfGeorefConventionOffset = psTransform->bOriginIsTopLeftCorner ? 0 : 0.5;

/* -------------------------------------------------------------------- */
/*      Do original pixel line to target geox/geoy.                     */
/* -------------------------------------------------------------------- */
    if( !bDstToSrc )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            const double dfGeoLocPixel =
                (padfX[i] - psTransform->dfPIXEL_OFFSET)
                / psTransform->dfPIXEL_STEP - dfGeorefConventionOffset;
            const double dfGeoLocLine =
                (padfY[i] - psTransform->dfLINE_OFFSET)
                / psTransform->dfLINE_STEP - dfGeorefConventionOffset;

            if( !GDALGeoLocPosPixelLineToXY(psTransform,
                                         dfGeoLocPixel,
                                         dfGeoLocLine,
                                         padfX[i],
                                         padfY[i]) )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            if( psTransform->bSwapXY )
            {
                std::swap(padfX[i], padfY[i]);
            }

            panSuccess[i] = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      geox/geoy to pixel/line using backmap.                          */
/* -------------------------------------------------------------------- */
    else
    {
        if( psTransform->hQuadTree )
        {
            GDALGeoLocInverseTransformQuadtree(psTransform,
                                               nPointCount,
                                               padfX,
                                               padfY,
                                               panSuccess);
            return TRUE;
        }

        const bool bGeolocMaxAccuracy = CPLTestBool(
            CPLGetConfigOption("GDAL_GEOLOC_USE_MAX_ACCURACY", "YES"));

        // Keep those objects in this outer scope, so they are re-used, to
        // save memory allocations.
        OGRPoint oPoint;
        OGRLinearRing oRing;
        oRing.setNumPoints(5);

        for( int i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            if( psTransform->bSwapXY )
            {
                std::swap(padfX[i], padfY[i]);
            }

            const double dfGeoX = padfX[i];
            const double dfGeoY = padfY[i];

            const double dfBMX =
                ((padfX[i] - psTransform->adfBackMapGeoTransform[0])
                 / psTransform->adfBackMapGeoTransform[1]);
            const double dfBMY =
                ((padfY[i] - psTransform->adfBackMapGeoTransform[3])
                 / psTransform->adfBackMapGeoTransform[5]);

            if( !(dfBMX >= 0 && dfBMY >= 0 &&
                  dfBMX + 1 < psTransform->nBackMapWidth &&
                  dfBMY + 1 < psTransform->nBackMapHeight) )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            const std::ptrdiff_t iBMX = static_cast<std::ptrdiff_t>(dfBMX);
            const std::ptrdiff_t iBMY = static_cast<std::ptrdiff_t>(dfBMY);

            const size_t iBM = iBMX + iBMY * psTransform->nBackMapWidth;
            if( psTransform->pafBackMapX[iBM] == INVALID_BMXY )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            const float* pafBMX = psTransform->pafBackMapX + iBM;
            const float* pafBMY = psTransform->pafBackMapY + iBM;
            if( pafBMX[1] != INVALID_BMXY &&
                pafBMX[psTransform->nBackMapWidth] != INVALID_BMXY &&
                pafBMX[psTransform->nBackMapWidth+1] != INVALID_BMXY)
            {
                padfX[i] =
                    (1-(dfBMY - iBMY))
                    * (pafBMX[0] + (dfBMX - iBMX) * (pafBMX[1] - pafBMX[0]))
                    + (dfBMY - iBMY)
                    * (pafBMX[psTransform->nBackMapWidth] +
                       (dfBMX - iBMX) * (pafBMX[psTransform->nBackMapWidth+1] -
                                         pafBMX[psTransform->nBackMapWidth]));
                padfY[i] =
                    (1-(dfBMY - iBMY))
                    * (pafBMY[0] + (dfBMX - iBMX) * (pafBMY[1] - pafBMY[0]))
                    + (dfBMY - iBMY)
                    * (pafBMY[psTransform->nBackMapWidth] +
                       (dfBMX - iBMX) * (pafBMY[psTransform->nBackMapWidth+1] -
                                         pafBMY[psTransform->nBackMapWidth]));
            }
            else if( pafBMX[1] != INVALID_BMXY)
            {
                padfX[i] = pafBMX[0] +
                            (dfBMX - iBMX) * (pafBMX[1] - pafBMX[0]);
                padfY[i] = pafBMY[0] +
                            (dfBMX - iBMX) * (pafBMY[1] - pafBMY[0]);
            }
            else if( pafBMX[psTransform->nBackMapWidth] != INVALID_BMXY)
            {
                padfX[i] =
                    pafBMX[0] +
                    (dfBMY - iBMY) * (pafBMX[psTransform->nBackMapWidth] -
                                      pafBMX[0]);
                padfY[i] =
                    pafBMY[0] +
                    (dfBMY - iBMY) * (pafBMY[psTransform->nBackMapWidth] -
                                      pafBMY[0]);
            }
            else
            {
                padfX[i] = pafBMX[0];
                padfY[i] = pafBMY[0];
            }

            const double dfGeoLocPixel =
                (padfX[i] - psTransform->dfPIXEL_OFFSET)
                / psTransform->dfPIXEL_STEP - dfGeorefConventionOffset;
            const double dfGeoLocLine =
                (padfY[i] - psTransform->dfLINE_OFFSET)
                / psTransform->dfLINE_STEP - dfGeorefConventionOffset;
#if 0
            CPLDebug("GEOLOC", "%f %f %f %f", padfX[i], padfY[i], dfGeoLocPixel, dfGeoLocLine);
            if( !psTransform->bOriginIsTopLeftCorner )
            {
                if( dfGeoLocPixel + dfGeorefConventionOffset > psTransform->nGeoLocXSize-1 ||
                    dfGeoLocLine + dfGeorefConventionOffset > psTransform->nGeoLocYSize-1 )
                {
                    panSuccess[i] = FALSE;
                    padfX[i] = HUGE_VAL;
                    padfY[i] = HUGE_VAL;
                    continue;
                }
            }
#endif
            if( !bGeolocMaxAccuracy )
            {
                panSuccess[i] = TRUE;
                continue;
            }

            // Now that we have an approximate solution, identify a matching
            // cell in the geolocation array, where we can use inverse bilinear
            // interpolation to find the exact solution.

            // NOTE: if the geolocation array is an affine transformation,
            // the approximate solution should match the exact one, if the
            // backmap has correctly been built.

            oPoint.setX(dfGeoX);
            oPoint.setY(dfGeoY);
            // The thresholds and radius are rather empirical and have been tuned
            // on the product S5P_TEST_L2__NO2____20190509T220707_20190509T234837_08137_01_010400_20200220T091343.nc
            // that includes the north pole.
            const int nSearchRadius =
                psTransform->bGeographicSRSWithMinus180Plus180LongRange && fabs(dfGeoY) >= 85 ? 5 :
                psTransform->bGeographicSRSWithMinus180Plus180LongRange && fabs(dfGeoY) >= 75 ? 3 :
                psTransform->bGeographicSRSWithMinus180Plus180LongRange && fabs(dfGeoY) >= 65 ? 2 : 1;
            const ptrdiff_t nGeoLocPixel = static_cast<ptrdiff_t>(std::floor(dfGeoLocPixel));
            const ptrdiff_t nGeoLocLine = static_cast<ptrdiff_t>(std::floor(dfGeoLocLine));

            bool bDone = false;
            // Using the above approximate nGeoLocPixel, nGeoLocLine, try to
            // find a forward cell that includes (dfGeoX, dfGeoY), with an increasing
            // search radius, up to nSearchRadius.
            for( int r = 0; !bDone && r <= nSearchRadius; r++ )
            {
                for( int iter = 0; !bDone && iter < (r == 0 ? 1 : 8 * r); ++iter)
                {
                    // For r=1, the below formulas will give the following offsets:
                    // (-1,1), (0,1), (1,1), (1,0), (1,-1), (0,-1), (1,-1)
                    const int sx = (r == 0) ? 0 :
                             (iter < 2 *r) ? -r + iter :
                             (iter < 4 * r) ? r :
                             (iter < 6 * r) ? r - (iter - 4 * r):
                             -r;
                    const int sy = (r == 0) ? 0 :
                             (iter < 2 *r) ? r :
                             (iter < 4 * r) ? r - (iter - 2 * r) :
                             (iter < 6 * r) ? -r:
                             -r + (iter - 6 * r);
                    const ptrdiff_t iX = nGeoLocPixel + sx;
                    const ptrdiff_t iY = nGeoLocLine + sy;
                    if( (iX == -1 || (iX >= 0 && static_cast<size_t>(iX) < psTransform->nGeoLocXSize)) &&
                        (iY == -1 || (iY >= 0 && static_cast<size_t>(iY) < psTransform->nGeoLocYSize)) )
                    {
                        double x0, y0, x1, y1, x2, y2, x3, y3;

                        if( !GDALGeoLocPosPixelLineToXY(psTransform,
                                static_cast<double>(iX), static_cast<double>(iY),
                                x0, y0) ||
                            !GDALGeoLocPosPixelLineToXY(psTransform,
                                static_cast<double>(iX+1), static_cast<double>(iY),
                                x2, y2) ||
                            !GDALGeoLocPosPixelLineToXY(psTransform,
                                static_cast<double>(iX), static_cast<double>(iY+1),
                                x1, y1) ||
                            !GDALGeoLocPosPixelLineToXY(psTransform,
                                static_cast<double>(iX+1), static_cast<double>(iY+1),
                                x3, y3) )
                        {
                            continue;
                        }

                        int nIters = 1;
                        // For a bounding box crossing the anti-meridian, check
                        // both around -180 and +180 deg.
                        if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
                            std::fabs(x0) > 170 &&
                            std::fabs(x1) > 170 &&
                            std::fabs(x2) > 170 &&
                            std::fabs(x3) > 170 &&
                            (std::fabs(x1-x0) > 180 ||
                             std::fabs(x2-x0) > 180 ||
                             std::fabs(x3-x0) > 180) )
                        {
                            nIters = 2;
                            if( x0 > 0 ) x0 -= 360;
                            if( x1 > 0 ) x1 -= 360;
                            if( x2 > 0 ) x2 -= 360;
                            if( x3 > 0 ) x3 -= 360;
                        }
                        for( int iIter = 0; !bDone && iIter < nIters; ++iIter )
                        {
                            if( iIter == 1 )
                            {
                                x0 += 360;
                                x1 += 360;
                                x2 += 360;
                                x3 += 360;
                            }
                            oRing.setPoint(0, x0, y0);
                            oRing.setPoint(1, x2, y2);
                            oRing.setPoint(2, x3, y3);
                            oRing.setPoint(3, x1, y1);
                            oRing.setPoint(4, x0, y0);
                            if( oRing.isPointInRing( &oPoint ) ||
                                oRing.isPointOnRingBoundary( &oPoint ) )
                            {
                                double dfX = static_cast<double>(iX);
                                double dfY = static_cast<double>(iY);
                                GDALInverseBilinearInterpolation(dfGeoX, dfGeoY,
                                                             x0, y0,
                                                             x1, y1,
                                                             x2, y2,
                                                             x3, y3,
                                                             dfX, dfY);

                                dfX = (dfX + dfGeorefConventionOffset) *
                                    psTransform->dfPIXEL_STEP + psTransform->dfPIXEL_OFFSET;
                                dfY = (dfY + dfGeorefConventionOffset) *
                                    psTransform->dfLINE_STEP + psTransform->dfLINE_OFFSET;

#ifdef DEBUG_GEOLOC_REALLY_VERBOSE
                                CPLDebug("GEOLOC",
                                         "value before adjustment: %f %f, "
                                         "after adjustment: %f %f",
                                         padfX[i], padfY[i], dfX, dfY);
#endif

                                padfX[i] = dfX;
                                padfY[i] = dfY;

                                bDone = true;
                            }
                        }
                    }
                }
            }
            if( !bDone )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            panSuccess[i] = TRUE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                   GDALSerializeGeoLocTransformer()                   */
/************************************************************************/

CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeGeoLocTransformer", nullptr );

    GDALGeoLocTransformInfo *psInfo =
        static_cast<GDALGeoLocTransformInfo *>(pTransformArg);

    CPLXMLNode *psTree =
        CPLCreateXMLNode( nullptr, CXT_Element, "GeoLocTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(
        psTree, "Reversed",
        CPLString().Printf( "%d", static_cast<int>(psInfo->bReversed) ) );

/* -------------------------------------------------------------------- */
/*      geoloc metadata.                                                */
/* -------------------------------------------------------------------- */
    char **papszMD = psInfo->papszGeolocationInfo;
    CPLXMLNode *psMD= CPLCreateXMLNode( psTree, CXT_Element, "Metadata" );

    for( int i = 0; papszMD != nullptr && papszMD[i] != nullptr; i++ )
    {
        char *pszKey = nullptr;
        const char *pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );

        CPLXMLNode *psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
        CPLSetXMLValue( psMDI, "#key", pszKey );
        CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );

        CPLFree( pszKey );
    }

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeGeoLocTransformer()                 */
/************************************************************************/

void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree )

{
/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMetadata = CPLGetXMLNode( psTree, "Metadata" );

    if( psMetadata == nullptr ||
        psMetadata->eType != CXT_Element
        || !EQUAL(psMetadata->pszValue, "Metadata") )
        return nullptr;

    char **papszMD = nullptr;

    for( CPLXMLNode *psMDI = psMetadata->psChild;
         psMDI != nullptr;
         psMDI = psMDI->psNext )
    {
        if( !EQUAL(psMDI->pszValue, "MDI")
            || psMDI->eType != CXT_Element
            || psMDI->psChild == nullptr
            || psMDI->psChild->psNext == nullptr
            || psMDI->psChild->eType != CXT_Attribute
            || psMDI->psChild->psChild == nullptr )
            continue;

        papszMD =
            CSLSetNameValue( papszMD,
                             psMDI->psChild->psChild->pszValue,
                             psMDI->psChild->psNext->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    const int bReversed = atoi(CPLGetXMLValue(psTree, "Reversed", "0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */

    const char* pszSourceDataset = CPLGetXMLValue(psTree,"SourceDataset",nullptr);

    void *pResult = GDALCreateGeoLocTransformerEx( nullptr, papszMD, bReversed, pszSourceDataset, nullptr );

/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszMD );

    return pResult;
}
