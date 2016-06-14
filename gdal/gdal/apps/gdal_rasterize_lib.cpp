/******************************************************************************
 * $Id$
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

#include "gdal.h"
#include "gdal_alg.h"
#include "cpl_conv.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static bool ArgIsNumeric( const char *pszArg )

{
    char* pszEnd = NULL;
    CPLStrtod(pszArg, &pszEnd);
    return pszEnd != NULL && pszEnd[0] == '\0';
}


/************************************************************************/
/*                          InvertGeometries()                          */
/************************************************************************/

static void InvertGeometries( GDALDatasetH hDstDS,
                              std::vector<OGRGeometryH> &ahGeometries )

{
    OGRGeometryH hCollection =
        OGR_G_CreateGeometry( wkbGeometryCollection );

/* -------------------------------------------------------------------- */
/*      Create a ring that is a bit outside the raster dataset.         */
/* -------------------------------------------------------------------- */
    OGRGeometryH hUniversePoly, hUniverseRing;
    double adfGeoTransform[6];
    int brx = GDALGetRasterXSize( hDstDS ) + 2;
    int bry = GDALGetRasterYSize( hDstDS ) + 2;

    GDALGetGeoTransform( hDstDS, adfGeoTransform );

    hUniverseRing = OGR_G_CreateGeometry( wkbLinearRing );

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

    hUniversePoly = OGR_G_CreateGeometry( wkbPolygon );
    OGR_G_AddGeometryDirectly( hUniversePoly, hUniverseRing );

    OGR_G_AddGeometryDirectly( hCollection, hUniversePoly );

/* -------------------------------------------------------------------- */
/*      Add the rest of the geometries into our collection.             */
/* -------------------------------------------------------------------- */
    unsigned int iGeom;

    for( iGeom = 0; iGeom < ahGeometries.size(); iGeom++ )
        OGR_G_AddGeometryDirectly( hCollection, ahGeometries[iGeom] );

    ahGeometries.resize(1);
    ahGeometries[0] = hCollection;
}

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static CPLErr ProcessLayer(
    OGRLayerH hSrcLayer, int bSRSIsSet,
    GDALDatasetH hDstDS, std::vector<int> anBandList,
    const std::vector<double> &adfBurnValues, int b3D, int bInverse,
    const char *pszBurnAttribute, char **papszRasterizeOptions,
    GDALProgressFunc pfnProgress, void* pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/*      If -a_srs is specified, skip the test                           */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformationH hCT = NULL;
    if (!bSRSIsSet)
    {
        OGRSpatialReferenceH  hDstSRS = NULL;
        if( GDALGetProjectionRef( hDstDS ) != NULL )
        {
            char *pszProjection;

            pszProjection = (char *) GDALGetProjectionRef( hDstDS );

            hDstSRS = OSRNewSpatialReference(NULL);
            if( OSRImportFromWkt( hDstSRS, &pszProjection ) != OGRERR_NONE )
            {
                OSRDestroySpatialReference(hDstSRS);
                hDstSRS = NULL;
            }
        }

        OGRSpatialReferenceH hSrcSRS = OGR_L_GetSpatialRef(hSrcLayer);
        if( hDstSRS != NULL && hSrcSRS != NULL )
        {
            if( OSRIsSame(hSrcSRS, hDstSRS) == FALSE )
            {
                hCT = OCTNewCoordinateTransformation(hSrcSRS, hDstSRS);
                if( hCT == NULL )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "The output raster dataset and the input vector layer do not have the same SRS.\n"
                        "And reprojection of input data did not work. Results might be incorrect.");
                }
            }
        }
        else if( hDstSRS != NULL && hSrcSRS == NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The output raster dataset has a SRS, but the input vector layer SRS is unknown.\n"
                    "Ensure input vector has the same SRS, otherwise results might be incorrect.");
        }
        else if( hDstSRS == NULL && hSrcSRS != NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The input vector layer has a SRS, but the output raster dataset SRS is unknown.\n"
                    "Ensure output raster dataset has the same SRS, otherwise results might be incorrect.");
        }

        if( hDstSRS != NULL )
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
            if( hCT != NULL )
                OCTDestroyCoordinateTransformation(hCT);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat;
    std::vector<OGRGeometryH> ahGeometries;
    std::vector<double> adfFullBurnValues;

    OGR_L_ResetReading( hSrcLayer );

    while( (hFeat = OGR_L_GetNextFeature( hSrcLayer )) != NULL )
    {
        OGRGeometryH hGeom;

        if( OGR_F_GetGeometryRef( hFeat ) == NULL )
        {
            OGR_F_Destroy( hFeat );
            continue;
        }

        hGeom = OGR_G_Clone( OGR_F_GetGeometryRef( hFeat ) );
        if( hCT != NULL )
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
            if( adfBurnValues.size() > 0 )
                adfFullBurnValues.push_back(
                    adfBurnValues[MIN(iBand,adfBurnValues.size()-1)] );
            else if( pszBurnAttribute )
            {
                adfFullBurnValues.push_back( OGR_F_GetFieldAsDouble( hFeat, iBurnField ) );
            }
            /* I have made the 3D option exclusive to other options since it
               can be used to modify the value from "-burn value" or
               "-a attribute_name" */
            if( b3D )
            {
                // TODO: get geometry "z" value
                /* Points and Lines will have their "z" values collected at the
                   point and line levels respectively. However filled polygons
                   (GDALdllImageFilledPolygon) can use some help by getting
                   their "z" values here. */
                adfFullBurnValues.push_back( 0.0 );
            }
        }

        OGR_F_Destroy( hFeat );
    }

    if( hCT != NULL )
        OCTDestroyCoordinateTransformation(hCT);

/* -------------------------------------------------------------------- */
/*      If we are in inverse mode, we add one extra ring around the     */
/*      whole dataset to invert the concept of insideness and then      */
/*      merge everything into one geometry collection.                  */
/* -------------------------------------------------------------------- */
    if( bInverse )
    {
        if( ahGeometries.size() == 0 )
        {
            for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
            {
                if( adfBurnValues.size() > 0 )
                    adfFullBurnValues.push_back(
                        adfBurnValues[MIN(iBand,adfBurnValues.size()-1)] );
                else /* FIXME? Not sure what to do exactly in the else case, but we must insert a value */
                    adfFullBurnValues.push_back( 0.0 );
            }
        }

        InvertGeometries( hDstDS, ahGeometries );
    }

/* -------------------------------------------------------------------- */
/*      Perform the burn.                                               */
/* -------------------------------------------------------------------- */
    CPLErr eErr = GDALRasterizeGeometries( hDstDS, static_cast<int>(anBandList.size()), &(anBandList[0]),
                             static_cast<int>(ahGeometries.size()), &(ahGeometries[0]),
                             NULL, NULL, &(adfFullBurnValues[0]),
                             papszRasterizeOptions,
                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Cleanup geometries.                                             */
/* -------------------------------------------------------------------- */
    int iGeom;

    for( iGeom = static_cast<int>(ahGeometries.size())-1; iGeom >= 0; iGeom-- )
        OGR_G_DestroyGeometry( ahGeometries[iGeom] );

    return eErr;
}

/************************************************************************/
/*                  CreateOutputDataset()                               */
/************************************************************************/

static
GDALDatasetH CreateOutputDataset(std::vector<OGRLayerH> ahLayers,
                                 OGRSpatialReferenceH hSRS,
                                 int bGotBounds, OGREnvelope sEnvelop,
                                 GDALDriverH hDriver, const char* pszDest,
                                 int nXSize, int nYSize, double dfXRes, double dfYRes,
                                 int bTargetAlignedPixels,
                                 int nBandCount, GDALDataType eOutputType,
                                 char** papszCreationOptions, std::vector<double> adfInitVals,
                                 int bNoDataSet, double dfNoData)
{
    int bFirstLayer = TRUE;
    char* pszWKT = NULL;
    GDALDatasetH hDstDS = NULL;
    unsigned int i;

    for( i = 0; i < ahLayers.size(); i++ )
    {
        OGRLayerH hLayer = ahLayers[i];

        if (!bGotBounds)
        {
            OGREnvelope sLayerEnvelop;

            if (OGR_L_GetExtent(hLayer, &sLayerEnvelop, TRUE) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot get layer extent");
                return NULL;
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

                if (hSRS == NULL)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = FALSE;
            }
            else
            {
                sEnvelop.MinX = MIN(sEnvelop.MinX, sLayerEnvelop.MinX);
                sEnvelop.MinY = MIN(sEnvelop.MinY, sLayerEnvelop.MinY);
                sEnvelop.MaxX = MAX(sEnvelop.MaxX, sLayerEnvelop.MaxX);
                sEnvelop.MaxY = MAX(sEnvelop.MaxY, sLayerEnvelop.MaxY);
            }
        }
        else
        {
            if (bFirstLayer)
            {
                if (hSRS == NULL)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = FALSE;
            }
        }
    }

    if (dfXRes == 0 && dfYRes == 0)
    {
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

    double adfProjection[6];
    adfProjection[0] = sEnvelop.MinX;
    adfProjection[1] = dfXRes;
    adfProjection[2] = 0;
    adfProjection[3] = sEnvelop.MaxY;
    adfProjection[4] = 0;
    adfProjection[5] = -dfYRes;

    if (nXSize == 0 && nYSize == 0)
    {
        nXSize = (int)(0.5 + (sEnvelop.MaxX - sEnvelop.MinX) / dfXRes);
        nYSize = (int)(0.5 + (sEnvelop.MaxY - sEnvelop.MinY) / dfYRes);
    }

    hDstDS = GDALCreate(hDriver, pszDest, nXSize, nYSize,
                        nBandCount, eOutputType, papszCreationOptions);
    if (hDstDS == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszDest);
        return NULL;
    }

    GDALSetGeoTransform(hDstDS, adfProjection);

    if (hSRS)
        OSRExportToWkt(hSRS, &pszWKT);
    if (pszWKT)
        GDALSetProjection(hDstDS, pszWKT);
    CPLFree(pszWKT);

    int iBand;
    /*if( nBandCount == 3 || nBandCount == 4 )
    {
        for(iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterColorInterpretation(hBand, (GDALColorInterp)(GCI_RedBand + iBand));
        }
    }*/

    if (bNoDataSet)
    {
        for(iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterNoDataValue(hBand, dfNoData);
        }
    }

    if (adfInitVals.size() != 0)
    {
        for(iBand = 0; iBand < MIN(nBandCount,(int)adfInitVals.size()); iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALFillRaster(hBand, adfInitVals[iBand], 0);
        }
    }

    return hDstDS;
}


struct GDALRasterizeOptions
{
    /*! output format. The default is GeoTIFF(GTiff). Use the short format name. */
    char *pszFormat;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    int bCreateOutput;
    int b3D ;
    int bInverse;
    char **papszLayers;
    char *pszSQL;
    char *pszDialect;
    char *pszBurnAttribute;
    char *pszWHERE;
    std::vector<int> anBandList;
    std::vector<double> adfBurnValues;
    char **papszRasterizeOptions;
    double dfXRes;
    double dfYRes;
    char **papszCreationOptions;
    GDALDataType eOutputType;
    std::vector<double> adfInitVals;
    int bNoDataSet;
    double dfNoData;
    OGREnvelope sEnvelop;
    int bGotBounds;
    int nXSize, nYSize;
    OGRSpatialReferenceH hSRS;
    int bTargetAlignedPixels;
};


/************************************************************************/
/*                             GDALRasterize()                          */
/************************************************************************/

/**
 * Burns vector geometries into a raster
 *
 * This is the equivalent of the <a href="gdal_rasterize.html">gdal_rasterize</a> utility.
 *
 * GDALRasterizeOptions* must be allocated and freed with GDALRasterizeOptionsNew()
 * and GDALRasterizeOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALRasterizeOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALRasterize( const char *pszDest, GDALDatasetH hDstDS,
                            GDALDatasetH hSrcDataset,
                            const GDALRasterizeOptions *psOptionsIn, int *pbUsageError )
{
    if( pszDest == NULL && hDstDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( hSrcDataset == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hSrcDataset== NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( hDstDS != NULL && psOptionsIn && psOptionsIn->bCreateOutput )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hDstDS != NULL but options that imply creating a new dataset have been set.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALRasterizeOptions* psOptionsToFree = NULL;
    const GDALRasterizeOptions* psOptions;
    if( psOptionsIn )
        psOptions = psOptionsIn;
    else
    {
        psOptionsToFree = GDALRasterizeOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    int bCloseOutDSOnError = (hDstDS == NULL);
    if( pszDest == NULL )
        pszDest = GDALGetDescription(hDstDS);

    if( psOptions->pszSQL == NULL && psOptions->papszLayers == NULL &&
        GDALDatasetGetLayerCount(hSrcDataset) != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset has not one single layer.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALRasterizeOptionsFree(psOptionsToFree);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open target raster file.  Eventually we will add optional       */
/*      creation.                                                       */
/* -------------------------------------------------------------------- */
    int bCreateOutput = psOptions->bCreateOutput;
    if( hDstDS == NULL )
        bCreateOutput = TRUE;

    GDALDriverH hDriver = NULL;
    if (psOptions->bCreateOutput)
    {
/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
        hDriver = GDALGetDriverByName( psOptions->pszFormat );
        if( hDriver == NULL
            || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL )
        {
            int	iDr;

            CPLError( CE_Failure, CPLE_NotSupported,
                      "Output driver `%s' not recognised or does not support "
                      " direct output file creation.", psOptions->pszFormat);
            fprintf(stderr, "The following format drivers are configured\n"
                    "and support direct output:\n" );

            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                hDriver = GDALGetDriver(iDr);

                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL) != NULL )
                {
                    fprintf(stderr, "  %s: %s\n",
                            GDALGetDriverShortName( hDriver  ),
                            GDALGetDriverLongName( hDriver ) );
                }
            }
            fprintf(stderr, "\n" );
            GDALRasterizeOptionsFree(psOptionsToFree);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_Failure;

    if( psOptions->pszSQL != NULL )
    {
        OGRLayerH hLayer;

        hLayer = GDALDatasetExecuteSQL( hSrcDataset, psOptions->pszSQL, NULL, psOptions->pszDialect );
        if( hLayer != NULL )
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
                if( hDstDS == NULL )
                {
                    GDALDatasetReleaseResultSet( hSrcDataset, hLayer );
                    GDALRasterizeOptionsFree(psOptionsToFree);
                    return NULL;
                }
            }

            eErr = ProcessLayer( hLayer, psOptions->hSRS != NULL, hDstDS, psOptions->anBandList,
                          psOptions->adfBurnValues, psOptions->b3D, psOptions->bInverse, psOptions->pszBurnAttribute,
                          psOptions->papszRasterizeOptions, psOptions->pfnProgress, psOptions->pProgressData );

            GDALDatasetReleaseResultSet( hSrcDataset, hLayer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create output file if necessary.                                */
/* -------------------------------------------------------------------- */
    int nLayerCount = (psOptions->pszSQL == NULL && psOptions->papszLayers == NULL) ? 1 : CSLCount(psOptions->papszLayers);

    if (psOptions->bCreateOutput && hDstDS == NULL)
    {
        std::vector<OGRLayerH> ahLayers;

        for( int i = 0; i < nLayerCount; i++ )
        {
            OGRLayerH hLayer;
            if( psOptions->papszLayers )
                hLayer = GDALDatasetGetLayerByName( hSrcDataset, psOptions->papszLayers[i] );
            else
                hLayer = GDALDatasetGetLayer(hSrcDataset, 0);
            if( hLayer == NULL )
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
        if( hDstDS == NULL )
        {
            GDALRasterizeOptionsFree(psOptionsToFree);
            return NULL;
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
        if( hLayer == NULL )
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

        void *pScaledProgress;
        pScaledProgress =
            GDALCreateScaledProgress( 0.0, 1.0 * (i + 1) / nLayerCount,
                                      psOptions->pfnProgress, psOptions->pProgressData );

        eErr = ProcessLayer( hLayer, psOptions->hSRS != NULL, hDstDS, psOptions->anBandList,
                      psOptions->adfBurnValues, psOptions->b3D, psOptions->bInverse, psOptions->pszBurnAttribute,
                      psOptions->papszRasterizeOptions, GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress( pScaledProgress );
        if( eErr != CE_None )
            break;
    }

    GDALRasterizeOptionsFree(psOptionsToFree);

    if( eErr != CE_None )
    {
        if( bCloseOutDSOnError )
            GDALClose(hDstDS);
        return NULL;
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
 *                  The accepted options are the ones of the <a href="gdal_rasterize.html">gdal_rasterize</a> utility.
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

    psOptions->pszFormat = CPLStrdup("GTiff");
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->bCreateOutput = FALSE;
    psOptions->b3D = FALSE;
    psOptions->bInverse = FALSE;
    memset(&(psOptions->sEnvelop), 0, sizeof(psOptions->sEnvelop));
    psOptions->papszCreationOptions = NULL;
    psOptions->papszLayers = NULL;
    psOptions->pszSQL = NULL;
    psOptions->pszDialect = NULL;
    psOptions->pszBurnAttribute = NULL;
    psOptions->pszWHERE = NULL;
    psOptions->papszRasterizeOptions = NULL;
    psOptions->dfXRes = 0;
    psOptions->dfYRes = 0;
    psOptions->bCreateOutput = FALSE;
    psOptions->eOutputType = GDT_Float64;
    psOptions->bNoDataSet = FALSE;
    psOptions->dfNoData = 0;
    psOptions->bGotBounds = FALSE;
    psOptions->nXSize = 0;
    psOptions->nYSize = 0;
    psOptions->hSRS = NULL;
    psOptions->bTargetAlignedPixels = FALSE;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; i < argc; i++ )
    {
        if( EQUAL(papszArgv[i],"-of") && i < argc-1 )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
            psOptions->bCreateOutput = TRUE;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }

        else if( EQUAL(papszArgv[i],"-a") && i < argc-1 )
        {
            CPLFree(psOptions->pszBurnAttribute);
            psOptions->pszBurnAttribute = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-b") && i < argc-1 )
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
            psOptions->b3D = TRUE;
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "BURN_VALUE_FROM", "Z");
        }
        else if( EQUAL(papszArgv[i],"-add")  )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "MERGE_ALG", "ADD");
        }
        else if( EQUAL(papszArgv[i],"-chunkysize") && i < argc-1 )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "CHUNKYSIZE",
                                 papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-i")  )
        {
            psOptions->bInverse = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-at")  )
        {
            psOptions->papszRasterizeOptions =
                CSLSetNameValue( psOptions->papszRasterizeOptions, "ALL_TOUCHED", "TRUE" );
        }
        else if( EQUAL(papszArgv[i],"-burn") && i < argc-1 )
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
        else if( EQUAL(papszArgv[i],"-where") && i < argc-1 )
        {
            CPLFree(psOptions->pszWHERE);
            psOptions->pszWHERE = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-l") && i < argc-1 )
        {
            psOptions->papszLayers = CSLAddString( psOptions->papszLayers, papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-sql") && i < argc-1 )
        {
            CPLFree(psOptions->pszSQL);
            psOptions->pszSQL = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-dialect") && i < argc-1 )
        {
            CPLFree(psOptions->pszDialect);
            psOptions->pszDialect = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-init") && i < argc - 1 )
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
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-a_nodata") && i < argc - 1 )
        {
            psOptions->dfNoData = CPLAtof(papszArgv[i+1]);
            psOptions->bNoDataSet = TRUE;
            i += 1;
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-a_srs") && i < argc-1 )
        {
            OSRDestroySpatialReference(psOptions->hSRS);
            psOptions->hSRS = OSRNewSpatialReference( NULL );

            if( OSRSetFromUserInput(psOptions->hSRS, papszArgv[i+1]) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process SRS definition: %s",
                         papszArgv[i+1] );
                GDALRasterizeOptionsFree(psOptions);
                return NULL;
            }

            i++;
            psOptions->bCreateOutput = TRUE;
        }

        else if( EQUAL(papszArgv[i],"-te") && i < argc - 4 )
        {
            psOptions->sEnvelop.MinX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MinY = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxY = CPLAtof(papszArgv[++i]);
            psOptions->bGotBounds = TRUE;
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-a_ullr") && i < argc - 4 )
        {
            psOptions->sEnvelop.MinX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxY = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MaxX = CPLAtof(papszArgv[++i]);
            psOptions->sEnvelop.MinY = CPLAtof(papszArgv[++i]);
            psOptions->bGotBounds = TRUE;
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-co") && i < argc-1 )
        {
            psOptions->papszCreationOptions = CSLAddString( psOptions->papszCreationOptions, papszArgv[++i] );
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-ot") && i < argc-1 )
        {
            int iType;

            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = (GDALDataType) iType;
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown output pixel type: %s", papszArgv[i+1] );
                GDALRasterizeOptionsFree(psOptions);
                return NULL;
            }
            i++;
            psOptions->bCreateOutput = TRUE;
        }
        else if( (EQUAL(papszArgv[i],"-ts") || EQUAL(papszArgv[i],"-outsize")) && i < argc-2 )
        {
            psOptions->nXSize = atoi(papszArgv[++i]);
            psOptions->nYSize = atoi(papszArgv[++i]);
            if (psOptions->nXSize <= 0 || psOptions->nYSize <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -outsize parameter.");
                GDALRasterizeOptionsFree(psOptions);
                return NULL;
            }
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-tr") && i < argc-2 )
        {
            psOptions->dfXRes = CPLAtof(papszArgv[++i]);
            psOptions->dfYRes = fabs(CPLAtof(papszArgv[++i]));
            if( psOptions->dfXRes == 0 || psOptions->dfYRes == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -tr parameter.");
                GDALRasterizeOptionsFree(psOptions);
                return NULL;
            }
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-tap") )
        {
            psOptions->bTargetAlignedPixels = TRUE;
            psOptions->bCreateOutput = TRUE;
        }

        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALRasterizeOptionsFree(psOptions);
            return NULL;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszSource == NULL )
        {
            psOptionsForBinary->pszSource = CPLStrdup(papszArgv[i]);
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDest == NULL )
        {
            psOptionsForBinary->pszDest = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALRasterizeOptionsFree(psOptions);
            return NULL;
        }
    }

    if( psOptions->adfBurnValues.size() == 0 &&
        psOptions->pszBurnAttribute == NULL && !(psOptions->b3D) )
    {
        if( psOptionsForBinary == NULL )
            psOptions->adfBurnValues.push_back(255);
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "At least one of -3d, -burn or -a required." );
            GDALRasterizeOptionsFree(psOptions);
            return NULL;
        }
    }

    if( psOptions->bCreateOutput )
    {
        if( psOptions->dfXRes == 0 && psOptions->dfYRes == 0 && psOptions->nXSize == 0 && psOptions->nYSize == 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "'-tr xres yres' or '-ts xsize ysize' is required." );
            GDALRasterizeOptionsFree(psOptions);
            return NULL;
        }

        if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 && psOptions->dfYRes == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "-tap option cannot be used without using -tr.");
            GDALRasterizeOptionsFree(psOptions);
            return NULL;;
        }

        if( psOptions->anBandList.size() != 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "-b option cannot be used when creating a GDAL dataset." );
            GDALRasterizeOptionsFree(psOptions);
            return NULL;
        }

        int nBandCount = 1;

        if (psOptions->adfBurnValues.size() != 0)
            nBandCount = static_cast<int>(psOptions->adfBurnValues.size());

        if ((int)psOptions->adfInitVals.size() > nBandCount)
            nBandCount = static_cast<int>(psOptions->adfInitVals.size());

        if (psOptions->adfInitVals.size() == 1)
        {
            for(int i=1;i<=nBandCount - 1;i++)
                psOptions->adfInitVals.push_back( psOptions->adfInitVals[0] );
        }

        int i;
        for(i=1;i<=nBandCount;i++)
            psOptions->anBandList.push_back( i );
    }
    else
    {
        if( psOptions->anBandList.size() == 0 )
            psOptions->anBandList.push_back( 1 );
    }

    if( psOptions->pszDialect != NULL && psOptions->pszWHERE != NULL && psOptions->pszSQL == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "-dialect is ignored with -where. Use -sql instead" );
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;
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
    if( psOptions )
    {
        CPLFree(psOptions->pszFormat);
        CSLDestroy(psOptions->papszCreationOptions);
        CSLDestroy(psOptions->papszLayers);
        CSLDestroy(psOptions->papszRasterizeOptions);
        CPLFree(psOptions->pszSQL);
        CPLFree(psOptions->pszDialect);
        CPLFree(psOptions->pszBurnAttribute);
        CPLFree(psOptions->pszWHERE);
        OSRDestroySpatialReference(psOptions->hSRS);

        delete psOptions;
    }
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
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}
