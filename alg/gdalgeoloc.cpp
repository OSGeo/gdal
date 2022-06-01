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

constexpr float INVALID_BMXY = -10.0f;

#include "gdalgeoloc_carray_accessor.h"
#include "gdalgeoloc_dataset_accessor.h"

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
/*                    GDALGeoLoc::LoadGeolocFinish()                    */
/************************************************************************/

/*! @cond Doxygen_Suppress */

template<class Accessors>
bool GDALGeoLoc<Accessors>::LoadGeolocFinish( GDALGeoLocTransformInfo *psTransform )
{
    auto pAccessors = static_cast<Accessors*>(psTransform->pAccessors);
    CSLConstList papszGeolocationInfo = psTransform->papszGeolocationInfo;

/* -------------------------------------------------------------------- */
/*      Scan forward map for lat/long extents.                          */
/* -------------------------------------------------------------------- */
    psTransform->dfMinX = std::numeric_limits<double>::max();
    psTransform->dfMaxX = -std::numeric_limits<double>::max();
    psTransform->dfMinY = std::numeric_limits<double>::max();
    psTransform->dfMaxY = -std::numeric_limits<double>::max();

    for( int iY = 0; iY < psTransform->nGeoLocYSize; iY++ )
    {
        for( int iX = 0; iX < psTransform->nGeoLocXSize; iX++ )
        {
            const auto dfX = pAccessors->geolocXAccessor.Get(iX, iY);
            if( !psTransform->bHasNoData ||
                 dfX!= psTransform->dfNoDataX )
            {
                UpdateMinMax(psTransform,
                             dfX,
                             pAccessors->geolocYAccessor.Get(iX, iY));
            }
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
        for( int iY = 0; iY < psTransform->nGeoLocYSize - 1; iY++ )
        {
            for( int iX = 0; iX < psTransform->nGeoLocXSize - 1; iX++ )
            {
                double x0, y0, x1, y1, x2, y2, x3, y3;
                if( !PixelLineToXY(psTransform, iX, iY, x0, y0) ||
                    !PixelLineToXY(psTransform, iX+1, iY, x2, y2) ||
                    !PixelLineToXY(psTransform, iX, iY+1, x1, y1) ||
                    !PixelLineToXY(psTransform, iX+1, iY+1, x3, y3) )
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
        for( int iX = 0; iX <= psTransform->nGeoLocXSize; iX++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    static_cast<double>(psTransform->nGeoLocYSize),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        // Add "virtual" edge at X=nGeoLocXSize
        for( int iY = 0; iY <= psTransform->nGeoLocYSize; iY++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
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

        for( int iX = 0; iX <= psTransform->nGeoLocXSize; iX ++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    -0.5,
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        for( int iX = 0; iX <= psTransform->nGeoLocXSize; iX ++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
                    static_cast<double>(iX),
                    static_cast<double>(psTransform->nGeoLocYSize-1 + 0.5),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        for( int iY = 0; iY <= psTransform->nGeoLocYSize; iY++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
                    -0.5,
                    static_cast<double>(iY),
                    dfGeoLocX, dfGeoLocY) )
                continue;
            if( psTransform->bGeographicSRSWithMinus180Plus180LongRange )
                dfGeoLocX = Clamp(dfGeoLocX, -180.0, 180.0);
            UpdateMinMax(psTransform, dfGeoLocX, dfGeoLocY);
        }

        for( int iY = 0; iY <= psTransform->nGeoLocYSize; iY++ )
        {
            double dfGeoLocX;
            double dfGeoLocY;
            if( !PixelLineToXY(psTransform,
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
/*                     GDALGeoLoc::PixelLineToXY()                      */
/************************************************************************/

/** Interpolate a position expressed as (floating point) pixel/line in the
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

template<class Accessors>
bool GDALGeoLoc<Accessors>::PixelLineToXY(const GDALGeoLocTransformInfo *psTransform,
                                const double dfGeoLocPixel,
                                const double dfGeoLocLine,
                                double& dfX,
                                double& dfY)
{
    int iX = static_cast<int>(std::min(std::max(0.0, dfGeoLocPixel),
                              static_cast<double>(psTransform->nGeoLocXSize-1)));
    int iY = static_cast<int>(std::min(std::max(0.0, dfGeoLocLine),
                              static_cast<double>(psTransform->nGeoLocYSize-1)));

    auto pAccessors = static_cast<Accessors*>(psTransform->pAccessors);

    for( int iAttempt = 0; iAttempt < 2; ++iAttempt )
    {
        const double dfGLX_0_0 = pAccessors->geolocXAccessor.Get(iX, iY);
        const double dfGLY_0_0 = pAccessors->geolocYAccessor.Get(iX, iY);
        if( psTransform->bHasNoData &&
            dfGLX_0_0 == psTransform->dfNoDataX )
        {
            return false;
        }

        // This assumes infinite extension beyond borders of available
        // data based on closest grid square.
        if( iX + 1 < psTransform->nGeoLocXSize &&
            iY + 1 < psTransform->nGeoLocYSize )
        {
            const double dfGLX_1_0 = pAccessors->geolocXAccessor.Get(iX+1, iY);
            const double dfGLY_1_0 = pAccessors->geolocYAccessor.Get(iX+1, iY);
            const double dfGLX_0_1 = pAccessors->geolocXAccessor.Get(iX, iY+1);
            const double dfGLY_0_1 = pAccessors->geolocYAccessor.Get(iX, iY+1);
            const double dfGLX_1_1 = pAccessors->geolocXAccessor.Get(iX+1, iY+1);
            const double dfGLY_1_1 = pAccessors->geolocYAccessor.Get(iX+1, iY+1);
            if( !psTransform->bHasNoData ||
                (dfGLX_1_0 != psTransform->dfNoDataX &&
                 dfGLX_0_1 != psTransform->dfNoDataX &&
                 dfGLX_1_1 != psTransform->dfNoDataX) )
            {
                const double dfGLX_1_0_adjusted = ShiftGeoX(psTransform, dfGLX_0_0, dfGLX_1_0);
                const double dfGLX_0_1_adjusted = ShiftGeoX(psTransform, dfGLX_0_0, dfGLX_0_1);
                const double dfGLX_1_1_adjusted = ShiftGeoX(psTransform, dfGLX_0_0, dfGLX_1_1);
                dfX =
                    (1 - (dfGeoLocLine -iY))
                    * (dfGLX_0_0 + (dfGeoLocPixel-iX) * (dfGLX_1_0_adjusted - dfGLX_0_0))
                    + (dfGeoLocLine -iY)
                    * (dfGLX_0_1_adjusted + (dfGeoLocPixel-iX) * (dfGLX_1_1_adjusted - dfGLX_0_1_adjusted));
                dfX = UnshiftGeoX(psTransform, dfX);

                dfY = (1 - (dfGeoLocLine -iY)) * (
                        dfGLY_0_0 + (dfGeoLocPixel-iX) * (dfGLY_1_0 - dfGLY_0_0)) +
                      (dfGeoLocLine -iY) * (
                        dfGLY_0_1 + (dfGeoLocPixel-iX) * (dfGLY_1_1 - dfGLY_0_1));
                break;
            }
        }

        if( iX == psTransform->nGeoLocXSize -1 &&
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
                    pAccessors->geolocXAccessor.Get(iX+1, iY) != psTransform->dfNoDataX) )
        {
            const double dfGLX_1_0 = pAccessors->geolocXAccessor.Get(iX+1, iY);
            const double dfGLY_1_0 = pAccessors->geolocYAccessor.Get(iX+1, iY);
            dfX =
                dfGLX_0_0 + (dfGeoLocPixel-iX) * (ShiftGeoX(psTransform, dfGLX_0_0, dfGLX_1_0) - dfGLX_0_0);
            dfX = UnshiftGeoX(psTransform, dfX);
            dfY = dfGLY_0_0 + (dfGeoLocPixel-iX) * (dfGLY_1_0 - dfGLY_0_0);
        }
        else if( iY + 1 < psTransform->nGeoLocYSize &&
                 (!psTransform->bHasNoData ||
                    pAccessors->geolocXAccessor.Get(iX, iY+1) != psTransform->dfNoDataX) )
        {
            const double dfGLX_0_1 = pAccessors->geolocXAccessor.Get(iX, iY+1);
            const double dfGLY_0_1 = pAccessors->geolocYAccessor.Get(iX, iY+1);
            dfX = dfGLX_0_0
                + (dfGeoLocLine -iY) * (ShiftGeoX(psTransform, dfGLX_0_0, dfGLX_0_1) - dfGLX_0_0);
            dfX = UnshiftGeoX(psTransform, dfX);
            dfY = dfGLY_0_0 + (dfGeoLocLine -iY) * (dfGLY_0_1 - dfGLY_0_0);
        }
        else
        {
            dfX = dfGLX_0_0;
            dfY = dfGLY_0_0;
        }
        break;
    }
    return true;
}

template<class Accessors>
bool GDALGeoLoc<Accessors>::PixelLineToXY(const GDALGeoLocTransformInfo *psTransform,
                                          const int nGeoLocPixel,
                                          const int nGeoLocLine,
                                          double& dfX,
                                          double& dfY)
{
    if( nGeoLocPixel >= 0 && nGeoLocPixel < psTransform->nGeoLocXSize &&
        nGeoLocLine >= 0  && nGeoLocLine  < psTransform->nGeoLocYSize )
    {
        auto pAccessors = static_cast<Accessors*>(psTransform->pAccessors);
        const double dfGLX = pAccessors->geolocXAccessor.Get(nGeoLocPixel, nGeoLocLine);
        const double dfGLY = pAccessors->geolocYAccessor.Get(nGeoLocPixel, nGeoLocLine);
        if( psTransform->bHasNoData &&
            dfGLX == psTransform->dfNoDataX )
        {
            return false;
        }
        dfX = dfGLX;
        dfY = dfGLY;
        return true;
    }
    return PixelLineToXY(psTransform,
                         static_cast<double>(nGeoLocPixel),
                         static_cast<double>(nGeoLocLine),
                         dfX, dfY);
}

/************************************************************************/
/*                     GDALGeoLoc::ExtractSquare()                      */
/************************************************************************/

template<class Accessors>
bool GDALGeoLoc<Accessors>::ExtractSquare(const GDALGeoLocTransformInfo *psTransform,
                                          int nX, int nY,
                                          double& dfX_0_0, double& dfY_0_0,
                                          double& dfX_1_0, double& dfY_1_0,
                                          double& dfX_0_1, double& dfY_0_1,
                                          double& dfX_1_1, double& dfY_1_1)
{
        return PixelLineToXY(psTransform, nX, nY, dfX_0_0, dfY_0_0) &&
               PixelLineToXY(psTransform, nX+1, nY, dfX_1_0, dfY_1_0) &&
               PixelLineToXY(psTransform, nX, nY+1, dfX_0_1, dfY_0_1) &&
               PixelLineToXY(psTransform, nX+1, nY+1, dfX_1_1, dfY_1_1);
}

bool GDALGeoLocExtractSquare(const GDALGeoLocTransformInfo *psTransform,
                             int nX, int nY,
                             double& dfX_0_0, double& dfY_0_0,
                             double& dfX_1_0, double& dfY_1_0,
                             double& dfX_0_1, double& dfY_0_1,
                             double& dfX_1_1, double& dfY_1_1)
{
    if( psTransform->bUseArray )
    {
        return GDALGeoLoc<GDALGeoLocCArrayAccessors>::ExtractSquare(
                psTransform, nX, nY,
                dfX_0_0, dfY_0_0, dfX_1_0, dfY_1_0, dfX_0_1, dfY_0_1, dfX_1_1, dfY_1_1);
    }
    else
    {
        return GDALGeoLoc<GDALGeoLocDatasetAccessors>::ExtractSquare(
                psTransform, nX, nY,
                dfX_0_0, dfY_0_0, dfX_1_0, dfY_1_0, dfX_0_1, dfY_0_1, dfX_1_1, dfY_1_1);
    }
}

/************************************************************************/
/*                        GDALGeoLocTransform()                         */
/************************************************************************/

template<class Accessors>
int GDALGeoLoc<Accessors>::Transform( void *pTransformArg,
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

            if( !PixelLineToXY(psTransform,
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

        auto pAccessors = static_cast<Accessors*>(psTransform->pAccessors);

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

            const int iBMX = static_cast<int>(dfBMX);
            const int iBMY = static_cast<int>(dfBMY);

            const auto fBMX_0_0 = pAccessors->backMapXAccessor.Get(iBMX, iBMY);
            const auto fBMY_0_0 = pAccessors->backMapYAccessor.Get(iBMX, iBMY);
            if( fBMX_0_0 == INVALID_BMXY )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            const auto fBMX_1_0 = pAccessors->backMapXAccessor.Get(iBMX+1, iBMY);
            const auto fBMY_1_0 = pAccessors->backMapYAccessor.Get(iBMX+1, iBMY);
            const auto fBMX_0_1 = pAccessors->backMapXAccessor.Get(iBMX, iBMY+1);
            const auto fBMY_0_1 = pAccessors->backMapYAccessor.Get(iBMX, iBMY+1);
            const auto fBMX_1_1 = pAccessors->backMapXAccessor.Get(iBMX+1, iBMY+1);
            const auto fBMY_1_1 = pAccessors->backMapYAccessor.Get(iBMX+1, iBMY+1);
            if( fBMX_1_0 != INVALID_BMXY &&
                fBMX_0_1 != INVALID_BMXY &&
                fBMX_1_1 != INVALID_BMXY)
            {
                padfX[i] =
                    (1-(dfBMY - iBMY))
                    * (fBMX_0_0 + (dfBMX - iBMX) * (fBMX_1_0 - fBMX_0_0))
                    + (dfBMY - iBMY)
                    * (fBMX_0_1 + (dfBMX - iBMX) * (fBMX_1_1 - fBMX_0_1));
                padfY[i] =
                    (1-(dfBMY - iBMY))
                    * (fBMY_0_0 + (dfBMX - iBMX) * (fBMY_1_0 - fBMY_0_0))
                    + (dfBMY - iBMY)
                    * (fBMY_0_1 + (dfBMX - iBMX) * (fBMY_1_1 - fBMY_0_1));
            }
            else if( fBMX_1_0 != INVALID_BMXY)
            {
                padfX[i] = fBMX_0_0 + (dfBMX - iBMX) * (fBMX_1_0 - fBMX_0_0);
                padfY[i] = fBMY_0_0 + (dfBMX - iBMX) * (fBMY_1_0 - fBMY_0_0);
            }
            else if( fBMX_0_1 != INVALID_BMXY)
            {
                padfX[i] = fBMX_0_0 + (dfBMY - iBMY) * (fBMX_0_1 - fBMX_0_0);
                padfY[i] = fBMY_0_0 + (dfBMY - iBMY) * (fBMY_0_1 - fBMY_0_0);
            }
            else
            {
                padfX[i] = fBMX_0_0;
                padfY[i] = fBMY_0_0;
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
            // Amended with the test case of https://github.com/OSGeo/gdal/issues/5823
            const int nSearchRadius =
                psTransform->bGeographicSRSWithMinus180Plus180LongRange && fabs(dfGeoY) >= 85 ? 5 : 3;
            const int nGeoLocPixel = static_cast<int>(std::floor(dfGeoLocPixel));
            const int nGeoLocLine = static_cast<int>(std::floor(dfGeoLocLine));

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
                    if( nGeoLocPixel >= static_cast<int>(psTransform->nGeoLocXSize) - sx ||
                        nGeoLocLine  >= static_cast<int>(psTransform->nGeoLocYSize) - sy )
                    {
                        continue;
                    }
                    const int iX = nGeoLocPixel + sx;
                    const int iY = nGeoLocLine + sy;
                    if( iX >= -1 || iY >= -1 )
                    {
                        double x0, y0, x1, y1, x2, y2, x3, y3;

                        if( !PixelLineToXY(psTransform,
                                iX, iY,
                                x0, y0) ||
                            !PixelLineToXY(psTransform,
                                iX+1, iY,
                                x2, y2) ||
                            !PixelLineToXY(psTransform,
                                iX, iY+1,
                                x1, y1) ||
                            !PixelLineToXY(psTransform,
                                iX+1, iY+1,
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
/*! @endcond */

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

/*! @cond Doxygen_Suppress */

template<class Accessors>
bool GDALGeoLoc<Accessors>::GenerateBackMap( GDALGeoLocTransformInfo *psTransform )

{
    CPLDebug("GEOLOC", "Starting backmap generation");
    const int nXSize = psTransform->nGeoLocXSize;
    const int nYSize = psTransform->nGeoLocYSize;

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

    // +2 : +1 due to afterwards nBMXSize++, and another +1 as security margin
    // for other computations.
    if( !(dfBMXSize > 0 && dfBMXSize + 2 < INT_MAX) ||
        !(dfBMYSize > 0 && dfBMYSize + 2 < INT_MAX) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %f x %f",
                 dfBMXSize, dfBMYSize);
        return false;
    }

    int nBMXSize = static_cast<int>(dfBMXSize);
    int nBMYSize = static_cast<int>(dfBMYSize);

    if( static_cast<size_t>(1 + nBMYSize) >
            std::numeric_limits<size_t>::max() / static_cast<size_t>(1 + nBMXSize) )
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
    auto pAccessors = static_cast<Accessors*>(psTransform->pAccessors);
    if( !pAccessors->AllocateBackMap())
        return false;

    const double dfGeorefConventionOffset = psTransform->bOriginIsTopLeftCorner ? 0 : 0.5;

    const auto UpdateBackmap = [&](int iBMX, int iBMY,
                                   double dfX, double dfY,
                                   double tempwt)
    {
        const auto fBMX = pAccessors->backMapXAccessor.Get(iBMX, iBMY);
        const auto fBMY = pAccessors->backMapYAccessor.Get(iBMX, iBMY);
        const float fUpdatedBMX = fBMX +
            static_cast<float>( tempwt * (
                (dfX + dfGeorefConventionOffset) * psTransform->dfPIXEL_STEP +
                psTransform->dfPIXEL_OFFSET));
        const float fUpdatedBMY = fBMY +
            static_cast<float>( tempwt * (
                (dfY + dfGeorefConventionOffset) * psTransform->dfLINE_STEP +
                psTransform->dfLINE_OFFSET));
        const float fUpdatedWeight = pAccessors->backMapWeightAccessor.Get(iBMX, iBMY) +
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
            int iXAvg = static_cast<int>(std::max(0.0, dfGeoLocPixel));
            iXAvg = std::min(iXAvg, psTransform->nGeoLocXSize-1);
            int iYAvg = static_cast<int>(std::max(0.0, dfGeoLocLine));
            iYAvg = std::min(iYAvg, psTransform->nGeoLocYSize-1);
            const double dfGLX = pAccessors->geolocXAccessor.Get(iXAvg, iYAvg);
            const double dfGLY = pAccessors->geolocYAccessor.Get(iXAvg, iYAvg);

            const unsigned iX = static_cast<unsigned>(dfX);
            const unsigned iY = static_cast<unsigned>(dfY);
            if( !(psTransform->bHasNoData &&
                  dfGLX == psTransform->dfNoDataX ) &&
                ((iX >= static_cast<unsigned>(nXSize - 1) || iY >= static_cast<unsigned>(nYSize - 1)) ||
                 (fabs(dfGLX - pAccessors->geolocXAccessor.Get(iX, iY)) <= 2 * dfPixelXSize &&
                  fabs(dfGLY - pAccessors->geolocYAccessor.Get(iX, iY)) <= 2 * dfPixelYSize)) )
            {
                pAccessors->backMapXAccessor.Set(iBMX, iBMY, fUpdatedBMX);
                pAccessors->backMapYAccessor.Set(iBMX, iBMY, fUpdatedBMY);
                pAccessors->backMapWeightAccessor.Set(iBMX, iBMY, fUpdatedWeight);
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
            if( !PixelLineToXY(psTransform, dfX, dfY, dfGeoLocX, dfGeoLocY) )
                continue;

            // Compute the floating point coordinates in the pixel space of the backmap
            const double dBMX = static_cast<double>(
                    (dfGeoLocX - dfMinX) / dfPixelXSize);

            const double dBMY = static_cast<double>(
                (dfMaxY - dfGeoLocY) / dfPixelYSize);

            //Get top left index by truncation
            const int iBMX = static_cast<int>(std::floor(dBMX));
            const int iBMY = static_cast<int>(std::floor(dBMY));

            if( iBMX >= 0 && iBMX < nBMXSize &&
                iBMY >= 0 && iBMY < nBMYSize )
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
                    const int nX = static_cast<int>(std::floor(dfX));
                    const int nY = static_cast<int>(std::floor(dfY));
                    for( int sx = -1; !bMatchingGeoLocCellFound && sx <= 0; sx++ )
                    {
                        for(int sy = -1; !bMatchingGeoLocCellFound && sy <= 0; sy++)
                        {
                            const int pixel = nX + sx;
                            const int line = nY + sy;
                            double x0, y0, x1, y1, x2, y2, x3, y3;
                            if( !PixelLineToXY(psTransform, pixel, line, x0, y0) ||
                                !PixelLineToXY(psTransform, pixel+1, line, x2, y2) ||
                                !PixelLineToXY(psTransform, pixel, line+1, x1, y1) ||
                                !PixelLineToXY(psTransform, pixel+1, line+1, x3, y3) )
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

                                    pAccessors->backMapXAccessor.Set(iBMX, iBMY, static_cast<float>(dfBMXValue));
                                    pAccessors->backMapYAccessor.Set(iBMX, iBMY, static_cast<float>(dfBMYValue));
                                    pAccessors->backMapWeightAccessor.Set(iBMX, iBMY, 1.0f);
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
            if( iBMX < -1 || iBMY < -1 || iBMX > nBMXSize || iBMY > nBMYSize )
                continue;

            const double fracBMX = dBMX - iBMX;
            const double fracBMY = dBMY - iBMY;

            //Check logic for top left pixel
            if ((iBMX >= 0) && (iBMY >= 0) &&
                (iBMX < nBMXSize) &&
                (iBMY < nBMYSize) &&
                pAccessors->backMapWeightAccessor.Get(iBMX, iBMY) != 1.0f )
            {
                const double tempwt = (1.0 - fracBMX) * (1.0 - fracBMY);
                UpdateBackmap(iBMX, iBMY, dfX, dfY, tempwt);
            }

            //Check logic for top right pixel
            if ((iBMY >= 0) &&
                (iBMX+1 < nBMXSize) &&
                (iBMY < nBMYSize) &&
                pAccessors->backMapWeightAccessor.Get(iBMX + 1, iBMY) != 1.0f )
            {
                const double tempwt = fracBMX * (1.0 - fracBMY);
                UpdateBackmap(iBMX + 1, iBMY, dfX, dfY, tempwt);
            }

            //Check logic for bottom right pixel
            if ((iBMX+1 < nBMXSize) &&
                (iBMY+1 < nBMYSize) &&
                pAccessors->backMapWeightAccessor.Get(iBMX + 1, iBMY + 1) != 1.0f )
            {
                const double tempwt = fracBMX * fracBMY;
                UpdateBackmap(iBMX + 1, iBMY + 1, dfX, dfY, tempwt);
            }

            //Check logic for bottom left pixel
            if ((iBMX >= 0) &&
                (iBMX < nBMXSize) &&
                (iBMY+1 < nBMYSize) &&
                pAccessors->backMapWeightAccessor.Get(iBMX, iBMY + 1) != 1.0f )
            {
                const double tempwt = (1.0 - fracBMX) * fracBMY;
                UpdateBackmap(iBMX, iBMY + 1, dfX, dfY, tempwt);
            }

        }
    }


    //Each pixel in the backmap may have multiple entries.
    //We now go in average it out using the weights
    for( int iY = 0; iY < nBMYSize; iY++ )
    {
        for( int iX = 0; iX < nBMXSize; iX++ )
        {
            //Check if pixel was only touch during neighbor scan
            //But no real weight was added as source point matched
            //backmap grid node
            const auto weight = pAccessors->backMapWeightAccessor.Get(iX, iY);
            if (weight > 0)
            {
                pAccessors->backMapXAccessor.Set(iX, iY,
                    pAccessors->backMapXAccessor.Get(iX, iY) / weight);
                pAccessors->backMapYAccessor.Set(iX, iY,
                    pAccessors->backMapYAccessor.Get(iX, iY) / weight);
            }
            else
            {
                pAccessors->backMapXAccessor.Set(iX, iY, INVALID_BMXY);
                pAccessors->backMapYAccessor.Set(iX, iY, INVALID_BMXY);
            }
        }
    }

    pAccessors->FreeWghtsBackMap();

    // Fill holes in backmap
    auto poBackmapDS = pAccessors->GetBackmapDataset();

    pAccessors->FlushBackmapCaches();

#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        poBackmapDS->SetGeoTransform(psTransform->adfBackMapGeoTransform);
        GDALClose(GDALCreateCopy(GDALGetDriverByName("GTiff"),
                              "/tmp/geoloc_before_fill.tif",
                              poBackmapDS,
                              false, nullptr, nullptr, nullptr));
    }
#endif

    constexpr double dfMaxSearchDist = 3.0;
    constexpr int nSmoothingIterations = 1;
    for( int i = 1; i <= 2; i++ )
    {
        GDALFillNodata( GDALRasterBand::ToHandle(poBackmapDS->GetRasterBand(i)),
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
                              poBackmapDS,
                              false, nullptr, nullptr, nullptr));
    }
#endif

    // A final hole filling logic, proceeding line by line, and feeling
    // holes when the backmap values surrounding the hole are close enough.
    for( int iBMY = 0; iBMY < nBMYSize; iBMY++ )
    {
        int iLastValidIX = -1;
        for( int iBMX = 0; iBMX < nBMXSize; iBMX++ )
        {
            if( pAccessors->backMapXAccessor.Get(iBMX, iBMY) == INVALID_BMXY )
                continue;
            if( iLastValidIX != -1 &&
                iBMX > iLastValidIX + 1 &&
                fabs( pAccessors->backMapXAccessor.Get(iBMX, iBMY) -
                      pAccessors->backMapXAccessor.Get(iLastValidIX, iBMY)) <= 2 &&
                fabs( pAccessors->backMapYAccessor.Get(iBMX, iBMY) -
                      pAccessors->backMapYAccessor.Get(iLastValidIX, iBMY)) <= 2 )
            {
                for( int iBMXInner = iLastValidIX + 1; iBMXInner < iBMX; ++iBMXInner )
                {
                    const float alpha = static_cast<float>(iBMXInner - iLastValidIX) / (iBMX - iLastValidIX);
                    pAccessors->backMapXAccessor.Set(iBMXInner, iBMY,
                        (1.0f - alpha) * pAccessors->backMapXAccessor.Get(iLastValidIX, iBMY) +
                        alpha * pAccessors->backMapXAccessor.Get(iBMX, iBMY));
                    pAccessors->backMapYAccessor.Set(iBMXInner, iBMY,
                        (1.0f - alpha) * pAccessors->backMapYAccessor.Get(iLastValidIX, iBMY) +
                        alpha * pAccessors->backMapYAccessor.Get(iBMX, iBMY));
                }
            }
            iLastValidIX = iBMX;
        }
    }

#ifdef DEBUG_GEOLOC
    if( CPLTestBool(CPLGetConfigOption("GEOLOC_DUMP", "NO")) )
    {
        pAccessors->FlushBackmapCaches();

        GDALClose(GDALCreateCopy(GDALGetDriverByName("GTiff"),
                              "/tmp/geoloc_after_line_fill.tif",
                              poBackmapDS,
                              false, nullptr, nullptr, nullptr));
    }
#endif

    pAccessors->ReleaseBackmapDataset(poBackmapDS);
    CPLDebug("GEOLOC", "Ending backmap generation");

    return true;
}

/*! @endcond */

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

    psTransform->dfNoDataX =
        GDALGetRasterNoDataValue( psTransform->hBand_X,
                                  &(psTransform->bHasNoData) );

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

    if( nXSize_XBand <= 0 || nYSize_XBand <= 0 ||
        nXSize_YBand <= 0 || nYSize_YBand <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid X_BAND / Y_BAND size");
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

    // Is it a regular grid ? That is:
    // The XBAND contains the x coordinates for all lines.
    // The YBAND contains the y coordinates for all columns.
    const bool bIsRegularGrid = ( nYSize_XBand == 1 && nYSize_YBand == 1 );

    const int nXSize = nXSize_XBand;
    const int nYSize = bIsRegularGrid ? nXSize_YBand : nYSize_XBand;

    if( static_cast<size_t>(nXSize) >
            std::numeric_limits<size_t>::max() / static_cast<size_t>(nYSize) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nXSize, nYSize);
        GDALDestroyGeoLocTransformer( psTransform );
        return nullptr;
    }

    psTransform->nGeoLocXSize = nXSize;
    psTransform->nGeoLocYSize = nYSize;

/* -------------------------------------------------------------------- */
/*      Load the geolocation array.                                     */
/* -------------------------------------------------------------------- */

    // The quadtree method is experimental. It simplifies the code significantly,
    // but unfortunately burns more RAM and is slower.
    const bool bUseQuadtree =
        EQUAL(CPLGetConfigOption("GDAL_GEOLOC_INVERSE_METHOD", "BACKMAP"), "QUADTREE");

    // Decide if we should C-arrays for geoloc and backmap, or on-disk
    // temporary datasets.
    const char* pszUseTempDatasets = CSLFetchNameValueDef(papszTransformOptions,
        "GEOLOC_USE_TEMP_DATASETS",
        CPLGetConfigOption("GDAL_GEOLOC_USE_TEMP_DATASETS", nullptr));
    if( pszUseTempDatasets )
        psTransform->bUseArray = !CPLTestBool(pszUseTempDatasets);
    else
        psTransform->bUseArray = nXSize < 16 * 1000 * 1000 / nYSize;

    if( psTransform->bUseArray )
    {
        auto pAccessors = new GDALGeoLocCArrayAccessors(psTransform);
        psTransform->pAccessors = pAccessors;
        if( !pAccessors->Load(bIsRegularGrid, bUseQuadtree) )
        {
            GDALDestroyGeoLocTransformer( psTransform );
            return nullptr;
        }
    }
    else
    {
        auto pAccessors = new GDALGeoLocDatasetAccessors(psTransform);
        psTransform->pAccessors = pAccessors;
        if( !pAccessors->Load(bIsRegularGrid, bUseQuadtree) )
        {
            GDALDestroyGeoLocTransformer( psTransform );
            return nullptr;
        }
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

    CSLDestroy( psTransform->papszGeolocationInfo );

    if( psTransform->bUseArray )
        delete static_cast<GDALGeoLocCArrayAccessors*>(psTransform->pAccessors);
    else
        delete static_cast<GDALGeoLocDatasetAccessors*>(psTransform->pAccessors);

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

/** Use GeoLocation transformer */
int GDALGeoLocTransform( void *pTransformArg,
                         int bDstToSrc,
                         int nPointCount,
                         double *padfX, double *padfY,
                         double *padfZ,
                         int *panSuccess )
{
    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(pTransformArg);
    if( psTransform->bUseArray )
    {
        return GDALGeoLoc<GDALGeoLocCArrayAccessors>::Transform(pTransformArg,
                                                               bDstToSrc,
                                                               nPointCount,
                                                               padfX,
                                                               padfY,
                                                               padfZ,
                                                               panSuccess);
    }
    else
    {
        return GDALGeoLoc<GDALGeoLocDatasetAccessors>::Transform(pTransformArg,
                                                               bDstToSrc,
                                                               nPointCount,
                                                               padfX,
                                                               padfY,
                                                               padfZ,
                                                               panSuccess);
    }
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
