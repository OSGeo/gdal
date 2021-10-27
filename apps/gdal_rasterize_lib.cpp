/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Rasterize OGR shapes into a GDAL raster.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys dot com>
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
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static bool ArgIsNumeric( const char *pszArg )

{
    char* pszEnd = nullptr;
    CPLStrtod(pszArg, &pszEnd);
    return pszEnd != nullptr && pszEnd[0] == '\0';
}

/************************************************************************/
/*                          InvertGeometries()                          */
/************************************************************************/

static void InvertGeometries( GDALDatasetH hDstDS,
                              std::vector<OGRGeometryH> &ahGeometries )

{
    OGRGeometryH hInvertMultiPolygon =
        OGR_G_CreateGeometry( wkbMultiPolygon );

/* -------------------------------------------------------------------- */
/*      Create a ring that is a bit outside the raster dataset.         */
/* -------------------------------------------------------------------- */
    const int brx = GDALGetRasterXSize(hDstDS) + 2;
    const int bry = GDALGetRasterYSize(hDstDS) + 2;

    double adfGeoTransform[6] = {};
    GDALGetGeoTransform( hDstDS, adfGeoTransform );

    OGRGeometryH hUniverseRing = OGR_G_CreateGeometry(wkbLinearRing);

    OGR_G_AddPoint_2D(
        hUniverseRing,
        adfGeoTransform[0] + -2*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + -2*adfGeoTransform[5] );

    OGR_G_AddPoint_2D(
        hUniverseRing,
        adfGeoTransform[0] + brx*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + brx*adfGeoTransform[4] + -2*adfGeoTransform[5] );

    OGR_G_AddPoint_2D(
        hUniverseRing,
        adfGeoTransform[0] + brx*adfGeoTransform[1] + bry*adfGeoTransform[2],
        adfGeoTransform[3] + brx*adfGeoTransform[4] + bry*adfGeoTransform[5] );

    OGR_G_AddPoint_2D(
        hUniverseRing,
        adfGeoTransform[0] + -2*adfGeoTransform[1] + bry*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + bry*adfGeoTransform[5] );

    OGR_G_AddPoint_2D(
        hUniverseRing,
        adfGeoTransform[0] + -2*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + -2*adfGeoTransform[5] );

    OGRGeometryH hUniversePoly = OGR_G_CreateGeometry(wkbPolygon);
    OGR_G_AddGeometryDirectly( hUniversePoly, hUniverseRing );

    OGR_G_AddGeometryDirectly( hInvertMultiPolygon, hUniversePoly );

/* -------------------------------------------------------------------- */
/*      Add outer rings of polygons as inner rings of hUniversePoly     */
/*      and inner rings as sub-polygons.                                */
/* -------------------------------------------------------------------- */
    bool bFoundNonPoly = false;
    for( unsigned int iGeom = 0; iGeom < ahGeometries.size(); iGeom++ )
    {
        const auto eGType = OGR_GT_Flatten(OGR_G_GetGeometryType( ahGeometries[iGeom] ));
        if( eGType != wkbPolygon && eGType != wkbMultiPolygon )
        {
            if( !bFoundNonPoly )
            {
                bFoundNonPoly = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Ignoring non-polygon geometries in -i mode");
            }
            OGR_G_DestroyGeometry( ahGeometries[iGeom] );
            continue;
        }

        const auto ProcessPoly = [hUniversePoly, hInvertMultiPolygon](OGRPolygon* poPoly)
        {
            for( int i = poPoly->getNumInteriorRings() - 1; i >= 0; --i )
            {
                auto poNewPoly = new OGRPolygon();
                poNewPoly->addRingDirectly(poPoly->stealInteriorRing(i));
                OGRGeometry::FromHandle(hInvertMultiPolygon)->toMultiPolygon()->
                    addGeometryDirectly(poNewPoly);
            }
            OGRGeometry::FromHandle(hUniversePoly)->toPolygon()->addRingDirectly(
                poPoly->stealExteriorRing());
        };

        if( eGType == wkbPolygon )
        {
            auto poPoly = OGRGeometry::FromHandle(ahGeometries[iGeom])->toPolygon();
            ProcessPoly(poPoly);
            delete poPoly;
        }
        else
        {
            auto poMulti = OGRGeometry::FromHandle(ahGeometries[iGeom])->toMultiPolygon();
            for( int i = 0; i < poMulti->getNumGeometries(); i++ )
            {
                ProcessPoly( poMulti->getGeometryRef(i) );
            }
            delete poMulti;
        }
    }

    ahGeometries.resize(1);
    ahGeometries[0] = hInvertMultiPolygon;
}

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static CPLErr ProcessLayer(
    OGRLayerH hSrcLayer, bool bSRSIsSet,
    GDALDatasetH hDstDS, std::vector<int> anBandList,
    const std::vector<double> &adfBurnValues, bool b3D, bool bInverse,
    const char *pszBurnAttribute, char **papszRasterizeOptions,
    char** papszTO,
    GDALProgressFunc pfnProgress, void* pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/*      If -a_srs is specified, skip the test                           */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformationH hCT = nullptr;
    if (!bSRSIsSet)
    {
        OGRSpatialReferenceH hDstSRS = GDALGetSpatialRef(hDstDS);

        if( hDstSRS )
            hDstSRS = OSRClone(hDstSRS);
        else if( GDALGetMetadata(hDstDS, "RPC") != nullptr )
        {
            hDstSRS = OSRNewSpatialReference(nullptr);
            CPL_IGNORE_RET_VAL( OSRSetFromUserInput(hDstSRS, SRS_WKT_WGS84_LAT_LONG) );
            OSRSetAxisMappingStrategy(hDstSRS, OAMS_TRADITIONAL_GIS_ORDER);
        }

        OGRSpatialReferenceH hSrcSRS = OGR_L_GetSpatialRef(hSrcLayer);
        if( hDstSRS != nullptr && hSrcSRS != nullptr )
        {
            if( OSRIsSame(hSrcSRS, hDstSRS) == FALSE )
            {
                hCT = OCTNewCoordinateTransformation(hSrcSRS, hDstSRS);
                if( hCT == nullptr )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "The output raster dataset and the input vector layer do not have the same SRS.\n"
                        "And reprojection of input data did not work. Results might be incorrect.");
                }
            }
        }
        else if( hDstSRS != nullptr && hSrcSRS == nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The output raster dataset has a SRS, but the input vector layer SRS is unknown.\n"
                    "Ensure input vector has the same SRS, otherwise results might be incorrect.");
        }
        else if( hDstSRS == nullptr && hSrcSRS != nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The input vector layer has a SRS, but the output raster dataset SRS is unknown.\n"
                    "Ensure output raster dataset has the same SRS, otherwise results might be incorrect.");
        }

        if( hDstSRS != nullptr )
        {
            OSRDestroySpatialReference(hDstSRS);
        }
    }

/* -------------------------------------------------------------------- */
/*      Get field index, and check.                                     */
/* -------------------------------------------------------------------- */
    int iBurnField = -1;

    if( pszBurnAttribute )
    {
        iBurnField = OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hSrcLayer ),
                                           pszBurnAttribute );
        if( iBurnField == -1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to find field %s on layer %s, skipping.",
                    pszBurnAttribute,
                    OGR_FD_GetName( OGR_L_GetLayerDefn( hSrcLayer ) ) );
            if( hCT != nullptr )
                OCTDestroyCoordinateTransformation(hCT);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat = nullptr;
    std::vector<OGRGeometryH> ahGeometries;
    std::vector<double> adfFullBurnValues;

    OGR_L_ResetReading( hSrcLayer );

    while( (hFeat = OGR_L_GetNextFeature( hSrcLayer )) != nullptr )
    {
        OGRGeometryH hGeom = OGR_F_StealGeometry( hFeat );
        if( hGeom == nullptr )
        {
            OGR_F_Destroy( hFeat );
            continue;
        }

        if( hCT != nullptr )
        {
            if( OGR_G_Transform(hGeom, hCT) != OGRERR_NONE )
            {
                OGR_F_Destroy( hFeat );
                OGR_G_DestroyGeometry(hGeom);
                continue;
            }
        }
        ahGeometries.push_back( hGeom );

        for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
        {
            if( !adfBurnValues.empty() )
                adfFullBurnValues.push_back(
                    adfBurnValues[
                        std::min(iBand,
                                 static_cast<unsigned int>(
                                     adfBurnValues.size()) - 1)] );
            else if( pszBurnAttribute )
            {
                adfFullBurnValues.push_back( OGR_F_GetFieldAsDouble( hFeat, iBurnField ) );
            }
            else if( b3D )
            {
                /* Points and Lines will have their "z" values collected at the
                   point and line levels respectively. Not implemented for polygons */
                adfFullBurnValues.push_back( 0.0 );
            }
        }

        OGR_F_Destroy( hFeat );
    }

    if( hCT != nullptr )
        OCTDestroyCoordinateTransformation(hCT);

/* -------------------------------------------------------------------- */
/*      If we are in inverse mode, we add one extra ring around the     */
/*      whole dataset to invert the concept of insideness and then      */
/*      merge everything into one geometry collection.                  */
/* -------------------------------------------------------------------- */
    if( bInverse )
    {
        if( ahGeometries.empty() )
        {
            for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
            {
                if( !adfBurnValues.empty() )
                    adfFullBurnValues.push_back(
                        adfBurnValues[
                            std::min(iBand,
                                     static_cast<unsigned int>(
                                        adfBurnValues.size()) - 1)] );
                else /* FIXME? Not sure what to do exactly in the else case, but we must insert a value */
                    adfFullBurnValues.push_back( 0.0 );
            }
        }

        InvertGeometries( hDstDS, ahGeometries );
    }

/* -------------------------------------------------------------------- */
/*      If we have transformer options, create the transformer here     */
/*      Coordinate transformation to the target SRS has already been    */
/*      done, so we just need to convert to target raster space.        */
/*      Note: this is somewhat identical to what is done in             */
/*      GDALRasterizeGeometries() itself, except we can pass transformer*/
/*      options.                                                        */
/* -------------------------------------------------------------------- */

    void* pTransformArg = nullptr;
    GDALTransformerFunc pfnTransformer = nullptr;
    CPLErr eErr = CE_None;
    if( papszTO != nullptr )
    {
        GDALDataset* poDS = reinterpret_cast<GDALDataset*>(hDstDS);
        char** papszTransformerOptions = CSLDuplicate(papszTO);
        double adfGeoTransform[6] = { 0.0 };
        if( poDS->GetGeoTransform( adfGeoTransform ) != CE_None &&
            poDS->GetGCPCount() == 0 &&
            poDS->GetMetadata("RPC") == nullptr )
        {
            papszTransformerOptions = CSLSetNameValue(
                papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
        }

        pTransformArg =
            GDALCreateGenImgProjTransformer2( nullptr, hDstDS,
                                              papszTransformerOptions );
        CSLDestroy( papszTransformerOptions );

        pfnTransformer = GDALGenImgProjTransform;
        if( pTransformArg == nullptr )
        {
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform the burn.                                               */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        eErr = GDALRasterizeGeometries(
            hDstDS,
            static_cast<int>(anBandList.size()), &(anBandList[0]),
            static_cast<int>(ahGeometries.size()), &(ahGeometries[0]),
            pfnTransformer, pTransformArg,
            &(adfFullBurnValues[0]),
            papszRasterizeOptions,
            pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

    if( pTransformArg )
        GDALDestroyTransformer( pTransformArg );

    for( int iGeom = static_cast<int>(ahGeometries.size()) - 1;
         iGeom >= 0; iGeom-- )
        OGR_G_DestroyGeometry( ahGeometries[iGeom] );

    return eErr;
}

/************************************************************************/
/*                  CreateOutputDataset()                               */
/************************************************************************/

static
GDALDatasetH CreateOutputDataset(std::vector<OGRLayerH> ahLayers,
                                 OGRSpatialReferenceH hSRS,
                                 bool bGotBounds, OGREnvelope sEnvelop,
                                 GDALDriverH hDriver, const char* pszDest,
                                 int nXSize, int nYSize, double dfXRes, double dfYRes,
                                 bool bTargetAlignedPixels,
                                 int nBandCount, GDALDataType eOutputType,
                                 char** papszCreationOptions, std::vector<double> adfInitVals,
                                 int bNoDataSet, double dfNoData)
{
    bool bFirstLayer = true;
    char* pszWKT = nullptr;

    for( unsigned int i = 0; i < ahLayers.size(); i++ )
    {
        OGRLayerH hLayer = ahLayers[i];

        if (!bGotBounds)
        {
            OGREnvelope sLayerEnvelop;

            if (OGR_L_GetExtent(hLayer, &sLayerEnvelop, TRUE) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot get layer extent");
                return nullptr;
            }

            /* Voluntarily increase the extent by a half-pixel size to avoid */
            /* missing points on the border */
            if (!bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
            {
                sLayerEnvelop.MinX -= dfXRes / 2;
                sLayerEnvelop.MaxX += dfXRes / 2;
                sLayerEnvelop.MinY -= dfYRes / 2;
                sLayerEnvelop.MaxY += dfYRes / 2;
            }

            if (bFirstLayer)
            {
                sEnvelop.MinX = sLayerEnvelop.MinX;
                sEnvelop.MinY = sLayerEnvelop.MinY;
                sEnvelop.MaxX = sLayerEnvelop.MaxX;
                sEnvelop.MaxY = sLayerEnvelop.MaxY;

                if (hSRS == nullptr)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = false;
            }
            else
            {
                sEnvelop.MinX = std::min(sEnvelop.MinX, sLayerEnvelop.MinX);
                sEnvelop.MinY = std::min(sEnvelop.MinY, sLayerEnvelop.MinY);
                sEnvelop.MaxX = std::max(sEnvelop.MaxX, sLayerEnvelop.MaxX);
                sEnvelop.MaxY = std::max(sEnvelop.MaxY, sLayerEnvelop.MaxY);
            }
        }
        else
        {
            if (bFirstLayer)
            {
                if (hSRS == nullptr)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = false;
            }
        }
    }

    if (dfXRes == 0 && dfYRes == 0)
    {
        if( nXSize == 0 || nYSize == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Size and resolutions are missing");
            return nullptr;
        }
        dfXRes = (sEnvelop.MaxX - sEnvelop.MinX) / nXSize;
        dfYRes = (sEnvelop.MaxY - sEnvelop.MinY) / nYSize;
    }
    else if (bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
    {
        sEnvelop.MinX = floor(sEnvelop.MinX / dfXRes) * dfXRes;
        sEnvelop.MaxX = ceil(sEnvelop.MaxX / dfXRes) * dfXRes;
        sEnvelop.MinY = floor(sEnvelop.MinY / dfYRes) * dfYRes;
        sEnvelop.MaxY = ceil(sEnvelop.MaxY / dfYRes) * dfYRes;
    }

    double adfProjection[6] = {
        sEnvelop.MinX, dfXRes, 0.0, sEnvelop.MaxY, 0.0, -dfYRes };

    if (nXSize == 0 && nYSize == 0)
    {
        nXSize =
            static_cast<int>(0.5 + (sEnvelop.MaxX - sEnvelop.MinX) / dfXRes);
        nYSize =
            static_cast<int>(0.5 + (sEnvelop.MaxY - sEnvelop.MinY) / dfYRes);
    }

    GDALDatasetH hDstDS = GDALCreate(hDriver, pszDest, nXSize, nYSize,
                                     nBandCount, eOutputType,
                                     papszCreationOptions);
    if (hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszDest);
        return nullptr;
    }

    GDALSetGeoTransform(hDstDS, adfProjection);

    if (hSRS)
        OSRExportToWkt(hSRS, &pszWKT);
    if (pszWKT)
        GDALSetProjection(hDstDS, pszWKT);
    CPLFree(pszWKT);

    /*if( nBandCount == 3 || nBandCount == 4 )
    {
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterColorInterpretation(hBand, (GDALColorInterp)(GCI_RedBand + iBand));
        }
    }*/

    if (bNoDataSet)
    {
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterNoDataValue(hBand, dfNoData);
        }
    }

    if (!adfInitVals.empty())
    {
        for( int iBand = 0;
             iBand < std::min(nBandCount, static_cast<int>(adfInitVals.size()));
             iBand++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALFillRaster(hBand, adfInitVals[iBand], 0);
        }
    }

    return hDstDS;
}

struct GDALRasterizeOptions
{
    /*! output format. Use the short format name. */
    char *pszFormat;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    bool bCreateOutput;
    bool b3D;
    bool bInverse;
    char **papszLayers;
    char *pszSQL;
    char *pszDialect;
    char *pszBurnAttribute;
    char *pszWHERE;
    std::vector<int> anBandList;
    std::vector<double> adfBurnValues;
    char **papszRasterizeOptions;
    char **papszTO;
    double dfXRes;
    double dfYRes;
    char **papszCreationOptions;
    GDALDataType eOutputType;
    std::vector<double> adfInitVals;
    int bNoDataSet;
    double dfNoData;
    OGREnvelope sEnvelop;
    bool bGotBounds;
    int nXSize, nYSize;
    OGRSpatialReferenceH hSRS;
    bool bTargetAlignedPixels;
};

/************************************************************************/
/*                             GDALRasterize()                          */
/************************************************************************/

/**
 * Burns vector geometries into a raster
 *
 * This is the equivalent of the <a href="/programs/gdal_rasterize.html">gdal_rasterize</a> utility.
 *
 * GDALRasterizeOptions* must be allocated and freed with GDALRasterizeOptionsNew()
 * and GDALRasterizeOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALRasterizeOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALRasterize( const char *pszDest, GDALDatasetH hDstDS,
                            GDALDatasetH hSrcDataset,
                            const GDALRasterizeOptions *psOptionsIn, int *pbUsageError )
{
    if( pszDest == nullptr && hDstDS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( hSrcDataset == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hSrcDataset== NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( hDstDS != nullptr && psOptionsIn && psOptionsIn->bCreateOutput )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hDstDS != NULL but options that imply creating a new dataset have been set.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALRasterizeOptions* psOptionsToFree = nullptr;
    const GDALRasterizeOptions* psOptions = psOptionsIn;
    if( psOptions == nullptr )
    {
        psOptionsToFree = GDALRasterizeOptionsNew(nullptr, nullptr);
        psOptions = psOptionsToFree;
    }

    const bool bCloseOutDSOnError = hDstDS == nullptr;
    if( pszDest == nullptr )
        pszDest = GDALGetDescription(hDstDS);

    if( psOptions->pszSQL == nullptr && psOptions->papszLayers == nullptr &&
        GDALDatasetGetLayerCount(hSrcDataset) != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset has not one single layer.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALRasterizeOptionsFree(psOptionsToFree);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open target raster file.  Eventually we will add optional       */
/*      creation.                                                       */
/* -------------------------------------------------------------------- */
    const bool bCreateOutput = psOptions->bCreateOutput || hDstDS == nullptr;

    GDALDriverH hDriver = nullptr;
    if (bCreateOutput)
    {
        CPLString osFormat;
        if( psOptions->pszFormat == nullptr )
        {
            osFormat = GetOutputDriverForRaster(pszDest);
            if( osFormat.empty() )
            {
                GDALRasterizeOptionsFree(psOptionsToFree);
                return nullptr;
            }
        }
        else
        {
            osFormat = psOptions->pszFormat;
        }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
        hDriver = GDALGetDriverByName( osFormat );
        char** papszDriverMD = hDriver ? GDALGetMetadata(hDriver, nullptr): nullptr;
        if( hDriver == nullptr
            || !CPLTestBool( CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER, "FALSE") )
            || !CPLTestBool( CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE") ) )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Output driver `%s' not recognised or does not support "
                      "direct output file creation.", osFormat.c_str());
            GDALRasterizeOptionsFree(psOptionsToFree);
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_Failure;

    if( psOptions->pszSQL != nullptr )
    {
        OGRLayerH hLayer = GDALDatasetExecuteSQL(
            hSrcDataset, psOptions->pszSQL, nullptr, psOptions->pszDialect);
        if( hLayer != nullptr )
        {
            if (bCreateOutput)
            {
                std::vector<OGRLayerH> ahLayers;
                ahLayers.push_back(hLayer);

                hDstDS = CreateOutputDataset(ahLayers, psOptions->hSRS,
                                 psOptions->bGotBounds, psOptions->sEnvelop,
                                 hDriver, pszDest,
                                 psOptions->nXSize, psOptions->nYSize, psOptions->dfXRes, psOptions->dfYRes,
                                 psOptions->bTargetAlignedPixels,
                                 static_cast<int>(psOptions->anBandList.size()), psOptions->eOutputType,
                                 psOptions->papszCreationOptions, psOptions->adfInitVals,
                                 psOptions->bNoDataSet, psOptions->dfNoData);
                if( hDstDS == nullptr )
                {
                    GDALDatasetReleaseResultSet( hSrcDataset, hLayer );
                    GDALRasterizeOptionsFree(psOptionsToFree);
                    return nullptr;
                }
            }

            eErr = ProcessLayer( hLayer, psOptions->hSRS != nullptr, hDstDS, psOptions->anBandList,
                          psOptions->adfBurnValues, psOptions->b3D, psOptions->bInverse, psOptions->pszBurnAttribute,
                          psOptions->papszRasterizeOptions,
                          psOptions->papszTO,
                          psOptions->pfnProgress,
                          psOptions->pProgressData );

            GDALDatasetReleaseResultSet( hSrcDataset, hLayer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create output file if necessary.                                */
/* -------------------------------------------------------------------- */
    const int nLayerCount =
        (psOptions->pszSQL == nullptr && psOptions->papszLayers == nullptr)
        ? 1 : CSLCount(psOptions->papszLayers);

    if (bCreateOutput && hDstDS == nullptr)
    {
        std::vector<OGRLayerH> ahLayers;

        for( int i = 0; i < nLayerCount; i++ )
        {
            OGRLayerH hLayer;
            if( psOptions->papszLayers )
                hLayer = GDALDatasetGetLayerByName( hSrcDataset, psOptions->papszLayers[i] );
            else
                hLayer = GDALDatasetGetLayer(hSrcDataset, 0);
            if( hLayer == nullptr )
            {
                continue;
            }
            ahLayers.push_back(hLayer);
        }

        hDstDS = CreateOutputDataset(ahLayers, psOptions->hSRS,
                                psOptions->bGotBounds, psOptions->sEnvelop,
                                hDriver, pszDest,
                                psOptions->nXSize, psOptions->nYSize, psOptions->dfXRes, psOptions->dfYRes,
                                psOptions->bTargetAlignedPixels,
                                static_cast<int>(psOptions->anBandList.size()), psOptions->eOutputType,
                                psOptions->papszCreationOptions, psOptions->adfInitVals,
                                psOptions->bNoDataSet, psOptions->dfNoData);
        if( hDstDS == nullptr )
        {
            GDALRasterizeOptionsFree(psOptionsToFree);
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each layer.                                             */
/* -------------------------------------------------------------------- */

    for( int i = 0; i < nLayerCount; i++ )
    {
        OGRLayerH hLayer;
        if( psOptions->papszLayers )
            hLayer = GDALDatasetGetLayerByName( hSrcDataset, psOptions->papszLayers[i] );
        else
            hLayer = GDALDatasetGetLayer(hSrcDataset, 0);
        if( hLayer == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unable to find layer \"%s\", skipping.",
                     psOptions->papszLayers ? psOptions->papszLayers[i] : "0" );
            continue;
        }

        if( psOptions->pszWHERE )
        {
            if( OGR_L_SetAttributeFilter( hLayer, psOptions->pszWHERE ) != OGRERR_NONE )
                break;
        }

        void *pScaledProgress =
            GDALCreateScaledProgress( 0.0, 1.0 * (i + 1) / nLayerCount,
                                      psOptions->pfnProgress, psOptions->pProgressData );

        eErr = ProcessLayer( hLayer, psOptions->hSRS != nullptr, hDstDS, psOptions->anBandList,
                      psOptions->adfBurnValues, psOptions->b3D, psOptions->bInverse, psOptions->pszBurnAttribute,
                      psOptions->papszRasterizeOptions,
                      psOptions->papszTO,
                      GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress( pScaledProgress );
        if( eErr != CE_None )
            break;
    }

    GDALRasterizeOptionsFree(psOptionsToFree);

    if( eErr != CE_None )
    {
        if( bCloseOutDSOnError )
            GDALClose(hDstDS);
        return nullptr;
    }

    return hDstDS;
}

/************************************************************************/
/*                           GDALRasterizeOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALRasterizeOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/gdal_rasterize.html">gdal_rasterize</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALRasterizeOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALRasterizeOptions struct. Must be freed with GDALRasterizeOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALRasterizeOptions *GDALRasterizeOptionsNew(char** papszArgv,
                                                      GDALRasterizeOptionsForBinary* psOptionsForBinary)
{
    GDALRasterizeOptions *psOptions = new GDALRasterizeOptions;

    psOptions->pszFormat = nullptr;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = nullptr;
    psOptions->bCreateOutput = false;
    psOptions->b3D = false;
    psOptions->bInverse = false;
    // sEnvelop implicitly initialized
    psOptions->papszCreationOptions = nullptr;
    psOptions->papszLayers = nullptr;
    psOptions->pszSQL = nullptr;
    psOptions->pszDialect = nullptr;
    psOptions->pszBurnAttribute = nullptr;
    psOptions->pszWHERE = nullptr;
    psOptions->papszRasterizeOptions = nullptr;
    psOptions->papszTO = nullptr;
    psOptions->dfXRes = 0;
    psOptions->dfYRes = 0;
    psOptions->eOutputType = GDT_Float64;
    psOptions->bNoDataSet = FALSE;
    psOptions->dfNoData = 0;
    psOptions->bGotBounds = false;
    psOptions->nXSize = 0;
    psOptions->nYSize = 0;
    psOptions->hSRS = nullptr;
    psOptions->bTargetAlignedPixels = false;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != nullptr && i < argc; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
            psOptions->bCreateOutput = true;
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-a") )
        {
            CPLFree(psOptions->pszBurnAttribute);
            psOptions->pszBurnAttribute = CPLStrdup(papszArgv[++i]);
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-b") )
        {
            if (strchr(papszArgv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( papszArgv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    psOptions->anBandList.push_back(atoi(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(papszArgv[i+1]))
                {
                    psOptions->anBandList.push_back(atoi(papszArgv[i+1]));
                    i += 1;
                }
            }
        }
        else if( EQUAL(papszArgv[i],"-3d")  )
        {
            psOptions->b3D = true;
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "BURN_VALUE_FROM", "Z");
        }
        else if( EQUAL(papszArgv[i],"-add")  )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "MERGE_ALG", "ADD");
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-chunkysize") )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "CHUNKYSIZE",
                                 papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-i")  )
        {
            psOptions->bInverse = true;
        }
        else if( EQUAL(papszArgv[i],"-at")  )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "ALL_TOUCHED", "TRUE" );
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-optim") )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "OPTIM", papszArgv[++i] );
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-burn") )
        {
            if (strchr(papszArgv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( papszArgv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    psOptions->adfBurnValues.push_back(CPLAtof(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(papszArgv[i+1]))
                {
                    psOptions->adfBurnValues.push_back(CPLAtof(papszArgv[i+1]));
                    i += 1;
                }
            }
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-where") )
        {
            CPLFree(psOptions->pszWHERE);
            psOptions->pszWHERE = CPLStrdup(papszArgv[++i]);
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-l") )
        {
            psOptions->papszLayers = CSLAddString( psOptions->papszLayers, papszArgv[++i] );
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-sql") )
        {
            CPLFree(psOptions->pszSQL);
            psOptions->pszSQL = CPLStrdup(papszArgv[++i]);
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-dialect") )
        {
            CPLFree(psOptions->pszDialect);
            psOptions->pszDialect = CPLStrdup(papszArgv[++i]);
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-init") )
        {
            if (strchr(papszArgv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( papszArgv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    psOptions->adfInitVals.push_back(CPLAtof(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(papszArgv[i+1]))
                {
                    psOptions->adfInitVals.push_back(CPLAtof(papszArgv[i+1]));
                    i += 1;
                }
            }
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-a_nodata") )
        {
            psOptions->dfNoData = CPLAtof(papszArgv[i+1]);
            psOptions->bNoDataSet = TRUE;
            i += 1;
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-a_srs") )
        {
            OSRDestroySpatialReference(psOptions->hSRS);
            psOptions->hSRS = OSRNewSpatialReference( nullptr );

            if( OSRSetFromUserInput(psOptions->hSRS, papszArgv[i+1]) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process SRS definition: %s",
                         papszArgv[i+1] );
                GDALRasterizeOptionsFree(psOptions);
                return nullptr;
            }

            i++;
            psOptions->bCreateOutput = true;
        }

        else if( i < argc-4 && EQUAL(papszArgv[i],"-te") )
        {
            psOptions->sEnvelop.MinX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MinY = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxY = CPLAtof(papszArgv[++i]);
            psOptions->bGotBounds = true;
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-4 && EQUAL(papszArgv[i],"-a_ullr") )
        {
            psOptions->sEnvelop.MinX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxY = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MinY = CPLAtof(papszArgv[++i]);
            psOptions->bGotBounds = true;
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-co") )
        {
            psOptions->papszCreationOptions = CSLAddString( psOptions->papszCreationOptions, papszArgv[++i] );
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-ot") )
        {
            for( int iType = 1; iType < GDT_TypeCount; iType++ )
            {
                const GDALDataType eType = static_cast<GDALDataType>(iType);
                if( GDALGetDataTypeName(eType) != nullptr
                    && EQUAL(GDALGetDataTypeName(eType),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = eType;
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown output pixel type: %s", papszArgv[i+1] );
                GDALRasterizeOptionsFree(psOptions);
                return nullptr;
            }
            i++;
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-2 && (EQUAL(papszArgv[i],"-ts") || EQUAL(papszArgv[i],"-outsize")) )
        {
            psOptions->nXSize = atoi(papszArgv[++i]);
            psOptions->nYSize = atoi(papszArgv[++i]);
            if (psOptions->nXSize <= 0 || psOptions->nYSize <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -ts parameter.");
                GDALRasterizeOptionsFree(psOptions);
                return nullptr;
            }
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-2 && EQUAL(papszArgv[i],"-tr") )
        {
            psOptions->dfXRes = CPLAtof(papszArgv[++i]);
            psOptions->dfYRes = fabs(CPLAtof(papszArgv[++i]));
            if( psOptions->dfXRes == 0 || psOptions->dfYRes == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -tr parameter.");
                GDALRasterizeOptionsFree(psOptions);
                return nullptr;
            }
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-tap") )
        {
            psOptions->bTargetAlignedPixels = true;
            psOptions->bCreateOutput = true;
        }
        else if( i < argc-1 && EQUAL(papszArgv[i],"-to") )
        {
            psOptions->papszTO =
                CSLAddString( psOptions->papszTO, papszArgv[++i] );
        }

        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszSource == nullptr )
        {
            psOptionsForBinary->pszSource = CPLStrdup(papszArgv[i]);
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDest == nullptr )
        {
            psOptionsForBinary->pszDest = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }
    }

    int nExclusiveOptionsCount = 0;
    nExclusiveOptionsCount += !psOptions->adfBurnValues.empty() ? 1 : 0;
    nExclusiveOptionsCount += psOptions->pszBurnAttribute != nullptr ? 1 : 0;
    nExclusiveOptionsCount += psOptions->b3D ? 1 : 0;
    if( nExclusiveOptionsCount != 1 )
    {
        if( nExclusiveOptionsCount == 0 && psOptionsForBinary == nullptr )
        {
            psOptions->adfBurnValues.push_back(255);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "One and only one of -3d, -burn or -a is required." );
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }
    }

    if( psOptions->bCreateOutput )
    {
        if( psOptions->dfXRes == 0 && psOptions->dfYRes == 0 && psOptions->nXSize == 0 && psOptions->nYSize == 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "'-tr xres yres' or '-ts xsize ysize' is required." );
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }

        if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 && psOptions->dfYRes == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "-tap option cannot be used without using -tr.");
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }

        if( !psOptions->anBandList.empty() )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "-b option cannot be used when creating a GDAL dataset." );
            GDALRasterizeOptionsFree(psOptions);
            return nullptr;
        }

        int nBandCount = 1;

        if (!psOptions->adfBurnValues.empty())
            nBandCount = static_cast<int>(psOptions->adfBurnValues.size());

        if( static_cast<int>(psOptions->adfInitVals.size()) > nBandCount )
            nBandCount = static_cast<int>(psOptions->adfInitVals.size());

        if (psOptions->adfInitVals.size() == 1)
        {
            for(int i=1;i<=nBandCount - 1;i++)
                psOptions->adfInitVals.push_back( psOptions->adfInitVals[0] );
        }

        for( int i = 1; i <= nBandCount; i++ )
            psOptions->anBandList.push_back( i );
    }
    else
    {
        if( psOptions->anBandList.empty() )
            psOptions->anBandList.push_back( 1 );
    }

    if( psOptions->pszDialect != nullptr && psOptions->pszWHERE != nullptr && psOptions->pszSQL == nullptr )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "-dialect is ignored with -where. Use -sql instead" );
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;
        if( psOptions->pszFormat )
            psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
    }

    return psOptions;
}

/************************************************************************/
/*                       GDALRasterizeOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALRasterizeOptions struct.
 *
 * @param psOptions the options struct for GDALRasterize().
 *
 * @since GDAL 2.1
 */

void GDALRasterizeOptionsFree(GDALRasterizeOptions *psOptions)
{
    if( psOptions == nullptr )
        return;

    CPLFree(psOptions->pszFormat);
    CSLDestroy(psOptions->papszCreationOptions);
    CSLDestroy(psOptions->papszLayers);
    CSLDestroy(psOptions->papszRasterizeOptions);
    CSLDestroy(psOptions->papszTO);
    CPLFree(psOptions->pszSQL);
    CPLFree(psOptions->pszDialect);
    CPLFree(psOptions->pszBurnAttribute);
    CPLFree(psOptions->pszWHERE);
    OSRDestroySpatialReference(psOptions->hSRS);

    delete psOptions;
}

/************************************************************************/
/*                  GDALRasterizeOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALRasterize().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALRasterizeOptionsSetProgress( GDALRasterizeOptions *psOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}
