/* ****************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL scattered data gridding (interpolation) tool
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 * ****************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdalgrid.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          GDALGridOptions                             */
/************************************************************************/

/** Options for use with GDALGrid(). GDALGridOptions* must be allocated
 * and freed with GDALGridOptionsNew() and GDALGridOptionsFree() respectively.
 */
struct GDALGridOptions
{

    /*! output format. The default is GeoTIFF(GTiff). Use the short format name. */
    char *pszFormat;

    /*! allow or suppress progress monitor and other non-error output */
    int bQuiet;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    char            **papszLayers;
    char            *pszBurnAttribute;
    double          dfIncreaseBurnValue;
    double          dfMultiplyBurnValue;
    char            *pszWHERE;
    char            *pszSQL;
    GDALDataType    eOutputType;
    char            **papszCreateOptions;
    int             nXSize;
    int             nYSize;
    double          dfXMin;
    double          dfXMax;
    double          dfYMin;
    double          dfYMax;
    int             bIsXExtentSet;
    int             bIsYExtentSet;
    GDALGridAlgorithm eAlgorithm;
    void            *pOptions;
    char            *pszOutputSRS;
    OGRGeometry     *poSpatialFilter;
    int             bClipSrc;
    OGRGeometry     *poClipSrc;
    char            *pszClipSrcDS;
    char            *pszClipSrcSQL;
    char            *pszClipSrcLayer;
    char            *pszClipSrcWhere;
    int              bNoDataSet;
    double           dfNoDataValue;
};

/************************************************************************/
/*                          GetAlgorithmName()                          */
/*                                                                      */
/*      Grids algorithm code into mnemonic name.                        */
/************************************************************************/

static void PrintAlgorithmAndOptions( GDALGridAlgorithm eAlgorithm,
                                      void *pOptions )
{
    switch ( eAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
            printf( "Algorithm name: \"%s\".\n", szAlgNameInvDist );
            CPLprintf( "Options are "
                    "\"power=%f:smoothing=%f:radius1=%f:radius2=%f:angle=%f"
                    ":max_points=%lu:min_points=%lu:nodata=%f\"\n",
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfPower,
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfSmoothing,
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfRadius1,
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfRadius2,
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridInverseDistanceToAPowerOptions *)pOptions)->nMaxPoints,
                (unsigned long)((GDALGridInverseDistanceToAPowerOptions *)pOptions)->nMinPoints,
                ((GDALGridInverseDistanceToAPowerOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_InverseDistanceToAPowerNearestNeighbor:
            printf( "Algorithm name: \"%s\".\n", szAlgNameInvDistNearestNeighbor );
            CPLprintf( "Options are "
                        "\"power=%f:smoothing=%f:radius=%f"
                    ":max_points=%lu:min_points=%lu:nodata=%f\"\n",
                ((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->dfPower,
                ((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->dfSmoothing,
                ((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->dfRadius,
                (unsigned long)((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->nMaxPoints,
                (unsigned long)((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->nMinPoints,
                ((GDALGridInverseDistanceToAPowerNearestNeighborOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MovingAverage:
            printf( "Algorithm name: \"%s\".\n", szAlgNameAverage );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridMovingAverageOptions *)pOptions)->dfRadius1,
                ((GDALGridMovingAverageOptions *)pOptions)->dfRadius2,
                ((GDALGridMovingAverageOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridMovingAverageOptions *)pOptions)->nMinPoints,
                ((GDALGridMovingAverageOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_NearestNeighbor:
            printf( "Algorithm name: \"%s\".\n", szAlgNameNearest );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:nodata=%f\"\n",
                ((GDALGridNearestNeighborOptions *)pOptions)->dfRadius1,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfRadius2,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfAngle,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricMinimum:
            printf( "Algorithm name: \"%s\".\n", szAlgNameMinimum );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricMaximum:
            printf( "Algorithm name: \"%s\".\n", szAlgNameMaximum );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricRange:
            printf( "Algorithm name: \"%s\".\n", szAlgNameRange );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricCount:
            printf( "Algorithm name: \"%s\".\n", szAlgNameCount );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricAverageDistance:
            printf( "Algorithm name: \"%s\".\n", szAlgNameAverageDistance );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricAverageDistancePts:
            printf( "Algorithm name: \"%s\".\n", szAlgNameAverageDistancePts );
            CPLprintf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_Linear:
            printf( "Algorithm name: \"%s\".\n", szAlgNameLinear );
            CPLprintf( "Options are "
                    "\"radius=%f:nodata=%f\"\n",
                ((GDALGridLinearOptions *)pOptions)->dfRadius,
                ((GDALGridLinearOptions *)pOptions)->dfNoDataValue);
            break;
        default:
            printf( "Algorithm is unknown.\n" );
            break;
    }
}

/************************************************************************/
/*                          ProcessGeometry()                           */
/*                                                                      */
/*  Extract point coordinates from the geometry reference and set the   */
/*  Z value as requested. Test whether we are in the clipped region     */
/*  before processing.                                                  */
/************************************************************************/

static void ProcessGeometry( OGRPoint *poGeom, OGRGeometry *poClipSrc,
                             int iBurnField, double dfBurnValue,
                             const double dfIncreaseBurnValue,
                             const double dfMultiplyBurnValue,
                             std::vector<double> &adfX,
                             std::vector<double> &adfY,
                             std::vector<double> &adfZ )

{
    if ( poClipSrc && !poGeom->Within(poClipSrc) )
        return;

    adfX.push_back( poGeom->getX() );
    adfY.push_back( poGeom->getY() );
    if ( iBurnField < 0 )
        adfZ.push_back(  (poGeom->getZ() + dfIncreaseBurnValue) * dfMultiplyBurnValue  );
    else
        adfZ.push_back( (dfBurnValue + dfIncreaseBurnValue) * dfMultiplyBurnValue );
}

/************************************************************************/
/*                       ProcessCommonGeometry()                        */
/*                                                                      */
/*  Process recursively geometry and extract points.                    */
/************************************************************************/

static void ProcessCommonGeometry(OGRGeometry* poGeom, OGRGeometry *poClipSrc,
                                int iBurnField, double dfBurnValue,
                                const double dfIncreaseBurnValue,
                                const double dfMultiplyBurnValue,
                                std::vector<double> &adfX,
                                std::vector<double> &adfY,
                                std::vector<double> &adfZ)
{
    if (NULL == poGeom)
        return;

    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch (eType)
    {
    case wkbPoint:
        return ProcessGeometry((OGRPoint *)poGeom, poClipSrc,
            iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
    case wkbLinearRing:
    case wkbLineString:
        {
            OGRLineString *poLS = (OGRLineString*)poGeom;
            OGRPoint point;
            for (int pointIndex = 0; pointIndex < poLS->getNumPoints(); pointIndex++)
            {
                poLS->getPoint(pointIndex, &point);
                ProcessCommonGeometry((OGRGeometry*)&point, poClipSrc,
                    iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
            }
        }
        break;
    case wkbPolygon:
        {
            int nRings(0);
            OGRPolygon* poPoly = (OGRPolygon*)poGeom;
            OGRLinearRing* poRing = poPoly->getExteriorRing();
            ProcessCommonGeometry((OGRGeometry*)poRing, poClipSrc,
                iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);

            nRings = poPoly->getNumInteriorRings();
            if (nRings > 0)
            {
                for (int ir = 0; ir < nRings; ++ir)
                {
                    poRing = poPoly->getInteriorRing(ir);
                    ProcessCommonGeometry((OGRGeometry*)poRing, poClipSrc,
                        iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
                }
            }
        }
        break;
    case wkbMultiPoint:
    case wkbMultiPolygon:
    case wkbMultiLineString:
    case wkbGeometryCollection:
        {
            OGRGeometryCollection* pOGRGeometryCollection = (OGRGeometryCollection*)poGeom;
            for (int i = 0; i < pOGRGeometryCollection->getNumGeometries(); ++i)
            {
                ProcessCommonGeometry(pOGRGeometryCollection->getGeometryRef(i), poClipSrc,
                    iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
            }
        }
        break;
    case wkbUnknown:
    case wkbNone:
    default:
        break;
    }
}

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static CPLErr ProcessLayer( OGRLayerH hSrcLayer, GDALDatasetH hDstDS,
                          OGRGeometry *poClipSrc,
                          int nXSize, int nYSize, int nBand,
                          int& bIsXExtentSet, int& bIsYExtentSet,
                          double& dfXMin, double& dfXMax,
                          double& dfYMin, double& dfYMax,
                          const char *pszBurnAttribute,
                          const double dfIncreaseBurnValue,
                          const double dfMultiplyBurnValue,
                          GDALDataType eType,
                          GDALGridAlgorithm eAlgorithm, void *pOptions,
                          int bQuiet, GDALProgressFunc pfnProgress, void* pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Get field index, and check.                                     */
/* -------------------------------------------------------------------- */
    int iBurnField = -1;

    if ( pszBurnAttribute )
    {
        iBurnField = OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hSrcLayer ),
                                           pszBurnAttribute );
        if( iBurnField == -1 )
        {
            printf( "Failed to find field %s on layer %s, skipping.\n",
                    pszBurnAttribute,
                    OGR_FD_GetName( OGR_L_GetLayerDefn( hSrcLayer ) ) );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      values to be interpolated.                                      */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeat;
    std::vector<double> adfX, adfY, adfZ;

    OGR_L_ResetReading( hSrcLayer );

    while( (poFeat = (OGRFeature *)OGR_L_GetNextFeature( hSrcLayer )) != NULL )
    {
        OGRGeometry *poGeom = poFeat->GetGeometryRef();
        double  dfBurnValue = 0.0;

        if ( iBurnField >= 0 )
            dfBurnValue = poFeat->GetFieldAsDouble( iBurnField );

        ProcessCommonGeometry(poGeom, poClipSrc, iBurnField, dfBurnValue,
            dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);

        OGRFeature::DestroyFeature( poFeat );
    }

    if ( adfX.empty() )
    {
        printf( "No point geometry found on layer %s, skipping.\n",
                OGR_FD_GetName( OGR_L_GetLayerDefn( hSrcLayer ) ) );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Compute grid geometry.                                          */
/* -------------------------------------------------------------------- */
    if ( !bIsXExtentSet || !bIsYExtentSet )
    {
        OGREnvelope sEnvelope;
        OGR_L_GetExtent( hSrcLayer, &sEnvelope, TRUE );

        if ( !bIsXExtentSet )
        {
            dfXMin = sEnvelope.MinX;
            dfXMax = sEnvelope.MaxX;
            bIsXExtentSet = TRUE;
        }

        if ( !bIsYExtentSet )
        {
            dfYMin = sEnvelope.MinY;
            dfYMax = sEnvelope.MaxY;
            bIsYExtentSet = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform gridding.                                               */
/* -------------------------------------------------------------------- */

    const double    dfDeltaX = ( dfXMax - dfXMin ) / nXSize;
    const double    dfDeltaY = ( dfYMax - dfYMin ) / nYSize;

    if ( !bQuiet )
    {
        printf( "Grid data type is \"%s\"\n", GDALGetDataTypeName(eType) );
        printf( "Grid size = (%lu %lu).\n",
                (unsigned long)nXSize, (unsigned long)nYSize );
        CPLprintf( "Corner coordinates = (%f %f)-(%f %f).\n",
                dfXMin - dfDeltaX / 2, dfYMax + dfDeltaY / 2,
                dfXMax + dfDeltaX / 2, dfYMin - dfDeltaY / 2 );
        CPLprintf( "Grid cell size = (%f %f).\n", dfDeltaX, dfDeltaY );
        printf( "Source point count = %lu.\n", (unsigned long)adfX.size() );
        PrintAlgorithmAndOptions( eAlgorithm, pOptions );
        printf("\n");
    }

    GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, nBand );

    if (adfX.empty())
    {
        // FIXME: Should have set to nodata value instead
        GDALFillRaster( hBand, 0.0 , 0.0 );
        return CE_None;
    }

    int     nXOffset, nYOffset;
    int     nBlockXSize, nBlockYSize;
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);

    // Try to grow the work buffer up to 16 MB if it is smaller
    GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
    const int nDesiredBufferSize = 16*1024*1024;
    if( nBlockXSize < nXSize && nBlockYSize < nYSize &&
        nBlockXSize < nDesiredBufferSize / (nBlockYSize * nDataTypeSize) )
    {
        int nNewBlockXSize  = nDesiredBufferSize / (nBlockYSize * nDataTypeSize);
        nBlockXSize = (nNewBlockXSize / nBlockXSize) * nBlockXSize;
        if( nBlockXSize > nXSize )
            nBlockXSize = nXSize;
    }
    else if( nBlockXSize == nXSize && nBlockYSize < nYSize &&
             nBlockYSize < nDesiredBufferSize / (nXSize * nDataTypeSize) )
    {
        int nNewBlockYSize = nDesiredBufferSize / (nXSize * nDataTypeSize);
        nBlockYSize = (nNewBlockYSize / nBlockYSize) * nBlockYSize;
        if( nBlockYSize > nYSize )
            nBlockYSize = nYSize;
    }
    CPLDebug("GDAL_GRID", "Work buffer: %d * %d", nBlockXSize, nBlockYSize);

    void *pData = VSIMalloc3(nBlockXSize, nBlockYSize, nDataTypeSize);
    if( pData == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate work buffer");
        return CE_Failure;
    }

    int nBlock = 0;
    int nBlockCount = ((nXSize + nBlockXSize - 1) / nBlockXSize)
        * ((nYSize + nBlockYSize - 1) / nBlockYSize);

    GDALGridContext* psContext = GDALGridContextCreate( eAlgorithm, pOptions,
                                                        static_cast<int>(adfX.size()),
                                                        &(adfX[0]), &(adfY[0]), &(adfZ[0]),
                                                        TRUE );
    if( psContext == NULL )
    {
        CPLFree( pData );
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    for ( nYOffset = 0; nYOffset < nYSize && eErr == CE_None; nYOffset += nBlockYSize )
    {
        for ( nXOffset = 0; nXOffset < nXSize && eErr == CE_None; nXOffset += nBlockXSize )
        {
            void *pScaledProgress;
            pScaledProgress =
                GDALCreateScaledProgress( (double)nBlock / nBlockCount,
                                          (double)(nBlock + 1) / nBlockCount,
                                          pfnProgress, pProgressData );
            nBlock ++;

            int nXRequest = nBlockXSize;
            if (nXOffset + nXRequest > nXSize)
                nXRequest = nXSize - nXOffset;

            int nYRequest = nBlockYSize;
            if (nYOffset + nYRequest > nYSize)
                nYRequest = nYSize - nYOffset;

            eErr = GDALGridContextProcess( psContext,
                            dfXMin + dfDeltaX * nXOffset,
                            dfXMin + dfDeltaX * (nXOffset + nXRequest),
                            dfYMin + dfDeltaY * nYOffset,
                            dfYMin + dfDeltaY * (nYOffset + nYRequest),
                            nXRequest, nYRequest, eType, pData,
                            GDALScaledProgress, pScaledProgress );

            if( eErr == CE_None )
                eErr = GDALRasterIO( hBand, GF_Write, nXOffset, nYOffset,
                          nXRequest, nYRequest, pData,
                          nXRequest, nYRequest, eType, 0, 0 );

            GDALDestroyScaledProgress( pScaledProgress );
        }
    }

    GDALGridContextFree(psContext);

    CPLFree( pData );
    return eErr;
}

/************************************************************************/
/*                            LoadGeometry()                            */
/*                                                                      */
/*  Read geometries from the given dataset using specified filters and  */
/*  returns a collection of read geometries.                            */
/************************************************************************/

static OGRGeometryCollection* LoadGeometry( const char* pszDS,
                                            const char* pszSQL,
                                            const char* pszLyr,
                                            const char* pszWhere )
{
    GDALDataset         *poDS;
    OGRLayer            *poLyr;
    OGRFeature          *poFeat;
    OGRGeometryCollection *poGeom = NULL;

    poDS = (GDALDataset*) GDALOpen( pszDS, GA_ReadOnly );
    if ( poDS == NULL )
        return NULL;

    if ( pszSQL != NULL )
        poLyr = poDS->ExecuteSQL( pszSQL, NULL, NULL );
    else if ( pszLyr != NULL )
        poLyr = poDS->GetLayerByName( pszLyr );
    else
        poLyr = poDS->GetLayer(0);

    if ( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource." );
        GDALClose(poDS);
        return NULL;
    }

    if ( pszWhere )
        poLyr->SetAttributeFilter( pszWhere );

    while ( (poFeat = poLyr->GetNextFeature()) != NULL )
    {
        OGRGeometry* poSrcGeom = poFeat->GetGeometryRef();
        if ( poSrcGeom )
        {
            OGRwkbGeometryType eType =
                wkbFlatten( poSrcGeom->getGeometryType() );

            if ( poGeom == NULL )
                poGeom = new OGRMultiPolygon();

            if ( eType == wkbPolygon )
                poGeom->addGeometry( poSrcGeom );
            else if ( eType == wkbMultiPolygon )
            {
                int iGeom;
                int nGeomCount =
                    ((OGRMultiPolygon *)poSrcGeom)->getNumGeometries();

                for ( iGeom = 0; iGeom < nGeomCount; iGeom++ )
                {
                    poGeom->addGeometry(
                        ((OGRMultiPolygon *)poSrcGeom)->getGeometryRef(iGeom) );
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Geometry not of polygon type." );
                OGRGeometryFactory::destroyGeometry( poGeom );
                OGRFeature::DestroyFeature( poFeat );
                if ( pszSQL != NULL )
                    poDS->ReleaseResultSet( poLyr );
                GDALClose(poDS);
                return NULL;
            }
        }

        OGRFeature::DestroyFeature( poFeat );
    }

    if( pszSQL != NULL )
        poDS->ReleaseResultSet( poLyr );
    GDALClose(poDS);

    return poGeom;
}

/************************************************************************/
/*                               GDALGrid()                             */
/************************************************************************/

/**
 * Create raster from the scattered data.
 *
 * This is the equivalent of the <a href="gdal_grid.html">gdal_grid</a> utility.
 *
 * GDALGridOptions* must be allocated and freed with GDALGridOptionsNew()
 * and GDALGridOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALGridOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALGrid( const char *pszDest, GDALDatasetH hSrcDataset,
                       const GDALGridOptions *psOptionsIn, int *pbUsageError )

{
    if( hSrcDataset == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( pszDest == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALGridOptions* psOptionsToFree = NULL;
    const GDALGridOptions* psOptions;
    if( psOptionsIn )
        psOptions = psOptionsIn;
    else
    {
        psOptionsToFree = GDALGridOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    GDALDataset* poSrcDS = (GDALDataset*) hSrcDataset;

    if( psOptions->pszSQL == NULL && psOptions->papszLayers == NULL &&
        poSrcDS->GetLayerCount() != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset has not one single layer.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALGridOptionsFree(psOptionsToFree);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    GDALDriverH hDriver = GDALGetDriverByName( psOptions->pszFormat );
    if( hDriver == NULL )
    {
        int iDr;

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output driver `%s' not recognised.", psOptions->pszFormat );
        fprintf( stderr,
        "The following format drivers are configured and support output:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                ( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
                || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
            {
                fprintf( stderr, "  %s: %s\n",
                         GDALGetDriverShortName( hDriver  ),
                         GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        GDALGridOptionsFree(psOptionsToFree);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create target raster file.                                      */
/* -------------------------------------------------------------------- */
    GDALDatasetH    hDstDS;
    int             nLayerCount = CSLCount(psOptions->papszLayers);
    if( nLayerCount == 0 && psOptions->pszSQL == NULL )
        nLayerCount = 1; /* due to above check */
    int             nBands = nLayerCount;

    if ( psOptions->pszSQL )
        nBands++;

    // FIXME
    int nXSize = psOptions->nXSize;
    if ( nXSize == 0 )
        nXSize = 256;
    int nYSize = psOptions->nYSize;
    if ( nYSize == 0 )
        nYSize = 256;

    hDstDS = GDALCreate( hDriver, pszDest, nXSize, nYSize, nBands,
                         psOptions->eOutputType, psOptions->papszCreateOptions );
    if ( hDstDS == NULL )
    {
        GDALGridOptionsFree(psOptionsToFree);
        return NULL;
    }

    if( psOptions->bNoDataSet )
    {
        for( int i = 1; i <= nBands; i++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i );
            GDALSetRasterNoDataValue( hBand, psOptions->dfNoDataValue );
        }
    }

    double dfXMin = psOptions->dfXMin;
    double dfYMin = psOptions->dfYMin;
    double dfXMax = psOptions->dfXMax;
    double dfYMax = psOptions->dfYMax;
    int bIsXExtentSet = psOptions->bIsXExtentSet;
    int bIsYExtentSet = psOptions->bIsYExtentSet;
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */

    if( psOptions->pszSQL != NULL )
    {
        OGRLayer* poLayer = poSrcDS->ExecuteSQL(psOptions->pszSQL,
                                                psOptions->poSpatialFilter, NULL );
        if( poLayer != NULL )
        {
            // Custom layer will be rasterized in the first band.
            eErr = ProcessLayer( (OGRLayerH)poLayer, hDstDS, psOptions->poSpatialFilter,
                          nXSize, nYSize, 1,
                          bIsXExtentSet, bIsYExtentSet,
                          dfXMin, dfXMax, dfYMin, dfYMax, psOptions->pszBurnAttribute,
                          psOptions->dfIncreaseBurnValue, psOptions->dfMultiplyBurnValue,
                          psOptions->eOutputType, psOptions->eAlgorithm, psOptions->pOptions,
                          psOptions->bQuiet, psOptions->pfnProgress, psOptions->pProgressData );

            poSrcDS->ReleaseResultSet(poLayer);
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each layer.                                             */
/* -------------------------------------------------------------------- */
    char* pszOutputSRS = ( psOptions->pszOutputSRS ) ? CPLStrdup(psOptions->pszOutputSRS) : NULL;
    for( int i = 0; i < nLayerCount; i++ )
    {
        OGRLayerH hLayer = ( psOptions->papszLayers == NULL ) ?
            GDALDatasetGetLayer(hSrcDataset, 0) :
            GDALDatasetGetLayerByName( hSrcDataset, psOptions->papszLayers[i]);
        if( hLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unable to find layer \"%s\", skipping.",
                     psOptions->papszLayers[i] ? psOptions->papszLayers[i] : "null" );
            continue;
        }

        if( psOptions->pszWHERE )
        {
            if( OGR_L_SetAttributeFilter( hLayer, psOptions->pszWHERE ) != OGRERR_NONE )
                break;
        }

        if ( psOptions->poSpatialFilter != NULL )
            OGR_L_SetSpatialFilter( hLayer, (OGRGeometryH)psOptions->poSpatialFilter );

        // Fetch the first meaningful SRS definition
        if ( !pszOutputSRS )
        {
            OGRSpatialReferenceH hSRS = OGR_L_GetSpatialRef( hLayer );
            if ( hSRS )
                OSRExportToWkt( hSRS, &pszOutputSRS );
        }

        eErr = ProcessLayer( hLayer, hDstDS, psOptions->poSpatialFilter, nXSize, nYSize,
                      i + 1 + nBands - nLayerCount,
                      bIsXExtentSet, bIsYExtentSet,
                      dfXMin, dfXMax, dfYMin, dfYMax, psOptions->pszBurnAttribute,
                      psOptions->dfIncreaseBurnValue, psOptions->dfMultiplyBurnValue,
                      psOptions->eOutputType, psOptions->eAlgorithm, psOptions->pOptions,
                      psOptions->bQuiet, psOptions->pfnProgress, psOptions->pProgressData );
        if( eErr != CE_None )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Apply geotransformation matrix.                                 */
/* -------------------------------------------------------------------- */
    double  adfGeoTransform[6];
    adfGeoTransform[0] = dfXMin;
    adfGeoTransform[1] = (dfXMax - dfXMin) / nXSize;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = dfYMin;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = (dfYMax - dfYMin) / nYSize;
    GDALSetGeoTransform( hDstDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Apply SRS definition if set.                                    */
/* -------------------------------------------------------------------- */
    if ( pszOutputSRS )
    {
        GDALSetProjection( hDstDS, pszOutputSRS );
        CPLFree(pszOutputSRS);
    }

/* -------------------------------------------------------------------- */
/*      End                                                             */
/* -------------------------------------------------------------------- */
    GDALGridOptionsFree(psOptionsToFree);

    if( eErr == CE_None )
        return hDstDS;
    else
    {
        GDALClose(hDstDS);
        return NULL;
    }
}

/************************************************************************/
/*                            IsNumber()                               */
/************************************************************************/

static int IsNumber(const char* pszStr)
{
    if (*pszStr == '-' || *pszStr == '+')
        pszStr ++;
    if (*pszStr == '.')
        pszStr ++;
    return (*pszStr >= '0' && *pszStr <= '9');
}

/************************************************************************/
/*                             GDALGridOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALGridOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdal_translate.html">gdal_translate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALGridOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALGridOptions struct. Must be freed with GDALGridOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALGridOptions *GDALGridOptionsNew(char** papszArgv, GDALGridOptionsForBinary* psOptionsForBinary)
{
    GDALGridOptions *psOptions =
        static_cast<GDALGridOptions *>(CPLCalloc(1, sizeof(GDALGridOptions)));

    psOptions->pszFormat = CPLStrdup("GTiff");
    psOptions->bQuiet = TRUE;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->papszLayers = NULL;
    psOptions->pszBurnAttribute = NULL;
    psOptions->dfIncreaseBurnValue = 0.0;
    psOptions->dfMultiplyBurnValue = 1.0;
    psOptions->pszWHERE = NULL;
    psOptions->pszSQL = NULL;
    psOptions->eOutputType = GDT_Float64;
    psOptions->papszCreateOptions = NULL;
    psOptions->nXSize = 0;
    psOptions->nYSize = 0;
    psOptions->dfXMin = 0.0;
    psOptions->dfXMax = 0.0;
    psOptions->dfYMin = 0.0;
    psOptions->dfYMax = 0.0;
    psOptions->bIsXExtentSet = FALSE;
    psOptions->bIsYExtentSet = FALSE;
    psOptions->eAlgorithm = GGA_InverseDistanceToAPower;
    psOptions->pOptions = NULL;
    psOptions->pszOutputSRS = NULL;
    psOptions->poSpatialFilter = NULL;
    psOptions->poClipSrc = NULL;
    psOptions->bClipSrc = FALSE;
    psOptions->pszClipSrcDS = NULL;
    psOptions->pszClipSrcSQL = NULL;
    psOptions->pszClipSrcLayer = NULL;
    psOptions->pszClipSrcWhere = NULL;
    psOptions->bNoDataSet = FALSE;
    psOptions->dfNoDataValue = 0;

    ParseAlgorithmAndOptions( szAlgNameInvDist, &psOptions->eAlgorithm, &psOptions->pOptions );

    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;
/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != NULL && i < argc; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
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

        else if( EQUAL(papszArgv[i],"-ot") && papszArgv[i+1] )
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
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", papszArgv[i+1] );
                GDALGridOptionsFree(psOptions);
                return NULL;
            }
            i++;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-txe") )
        {
            psOptions->dfXMin = CPLAtof(papszArgv[++i]);
            psOptions->dfXMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsXExtentSet = TRUE;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-tye") )
        {
            psOptions->dfYMin = CPLAtof(papszArgv[++i]);
            psOptions->dfYMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsYExtentSet = TRUE;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-outsize") )
        {
            psOptions->nXSize = atoi(papszArgv[++i]);
            psOptions->nYSize = atoi(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-co") )
        {
            psOptions->papszCreateOptions = CSLAddString( psOptions->papszCreateOptions, papszArgv[++i] );
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-zfield") )
        {
            CPLFree(psOptions->pszBurnAttribute);
            psOptions->pszBurnAttribute = CPLStrdup(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-z_increase") )
        {
            psOptions->dfIncreaseBurnValue = CPLAtof(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-z_multiply") )
        {
            psOptions->dfMultiplyBurnValue = CPLAtof(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-where") )
        {
            CPLFree(psOptions->pszWHERE);
            psOptions->pszWHERE = CPLStrdup(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-l") )
        {
            psOptions->papszLayers = CSLAddString( psOptions->papszLayers, papszArgv[++i] );
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-sql") )
        {
            CPLFree(psOptions->pszSQL);
            psOptions->pszSQL = CPLStrdup(papszArgv[++i]);
        }

        else if( i+4 < argc && EQUAL(papszArgv[i],"-spat") )
        {
            OGRLinearRing  oRing;

            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
            oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
            oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

            delete psOptions->poSpatialFilter;
            psOptions->poSpatialFilter = new OGRPolygon();
            ((OGRPolygon *) psOptions->poSpatialFilter)->addRing( &oRing );
            i += 4;
        }

        else if( EQUAL(papszArgv[i],"-clipsrc") )
        {
            if (i + 1 >= argc)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "%s option requires 1 or 4 arguments", papszArgv[i]);
                GDALGridOptionsFree(psOptions);
                return NULL;
            }

            VSIStatBufL  sStat;
            psOptions->bClipSrc = TRUE;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != NULL
                 && papszArgv[i+3] != NULL
                 && papszArgv[i+4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                delete psOptions->poClipSrc;
                psOptions->poClipSrc = OGRGeometryFactory::createGeometry(wkbPolygon);
                ((OGRPolygon *) psOptions->poClipSrc)->addRing( &oRing );
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                char* pszTmp = (char*) papszArgv[i+1];
                delete psOptions->poClipSrc;
                OGRGeometryFactory::createFromWkt(&pszTmp, NULL, &psOptions->poClipSrc);
                if (psOptions->poClipSrc == NULL)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALGridOptionsFree(psOptions);
                    return NULL;
                }
                i ++;
            }
            else if (EQUAL(papszArgv[i+1], "spat_extent") )
            {
                i ++;
            }
            else
            {
                CPLFree(psOptions->pszClipSrcDS);
                psOptions->pszClipSrcDS = CPLStrdup(papszArgv[i+1]);
                i ++;
            }
        }
        else if( i+1 < argc && EQUAL(papszArgv[i],"-clipsrcsql") )
        {
            CPLFree(psOptions->pszClipSrcSQL);
            psOptions->pszClipSrcSQL = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < argc && EQUAL(papszArgv[i],"-clipsrclayer") )
        {
            CPLFree(psOptions->pszClipSrcLayer);
            psOptions->pszClipSrcLayer = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < argc && EQUAL(papszArgv[i],"-clipsrcwhere") )
        {
            CPLFree(psOptions->pszClipSrcWhere);
            psOptions->pszClipSrcWhere = CPLStrdup(papszArgv[i+1]);
            i ++;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-a_srs") )
        {
            OGRSpatialReference oOutputSRS;

            if( oOutputSRS.SetFromUserInput( papszArgv[i+1] ) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process SRS definition: %s",
                         papszArgv[i+1] );
                GDALGridOptionsFree(psOptions);
                return NULL;
            }

            CPLFree(psOptions->pszOutputSRS);
            oOutputSRS.exportToWkt( &(psOptions->pszOutputSRS) );
            i++;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-a") )
        {
            const char* pszAlgorithm = papszArgv[++i];
            CPLFree(psOptions->pOptions);
            if ( ParseAlgorithmAndOptions( pszAlgorithm, &psOptions->eAlgorithm, &psOptions->pOptions )
                 != CE_None )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to process algorithm name and parameters" );
                GDALGridOptionsFree(psOptions);
                return NULL;
            }

            char **papszParms = CSLTokenizeString2( pszAlgorithm, ":", FALSE );
            const char* pszNoDataValue = CSLFetchNameValue( papszParms, "nodata" );
            if( pszNoDataValue != NULL )
            {
                psOptions->bNoDataSet = TRUE;
                psOptions->dfNoDataValue = CPLAtofM(pszNoDataValue);
            }
            CSLDestroy(papszParms);
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALGridOptionsFree(psOptions);
            return NULL;
        }
        else if( !bGotSourceFilename )
        {
            bGotSourceFilename = true;
            if( psOptionsForBinary )
                psOptionsForBinary->pszSource = CPLStrdup(papszArgv[i]);
        }
        else if( !bGotDestFilename )
        {
            bGotDestFilename = true;
            if( psOptionsForBinary )
                psOptionsForBinary->pszDest = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALGridOptionsFree(psOptions);
            return NULL;
        }
    }

    if ( psOptions->bClipSrc && psOptions->pszClipSrcDS != NULL )
    {
        psOptions->poClipSrc = LoadGeometry( psOptions->pszClipSrcDS, psOptions->pszClipSrcSQL,
                                  psOptions->pszClipSrcLayer, psOptions->pszClipSrcWhere );
        if ( psOptions->poClipSrc == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load source clip geometry.");
            GDALGridOptionsFree(psOptions);
            return NULL;
        }
    }
    else if ( psOptions->bClipSrc && psOptions->poClipSrc == NULL && !psOptions->poSpatialFilter )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "-clipsrc must be used with -spat option or \n"
                 "a bounding box, WKT string or datasource must be "
                 "specified.");
        GDALGridOptionsFree(psOptions);
        return NULL;
    }

    if ( psOptions->poSpatialFilter )
    {
        if ( psOptions->poClipSrc )
        {
            OGRGeometry *poTemp = psOptions->poSpatialFilter->Intersection( psOptions->poClipSrc );
            if ( poTemp )
            {
                delete psOptions->poSpatialFilter;
                psOptions->poSpatialFilter = poTemp;
            }

            delete psOptions->poClipSrc;
            psOptions->poClipSrc = NULL;
        }
    }
    else
    {
        if ( psOptions->poClipSrc )
        {
            psOptions->poSpatialFilter = psOptions->poClipSrc;
            psOptions->poClipSrc = NULL;
        }
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
    }

    return psOptions;
}

/************************************************************************/
/*                          GDALGridOptionsFree()                       */
/************************************************************************/

/**
 * Frees the GDALGridOptions struct.
 *
 * @param psOptions the options struct for GDALGrid().
 *
 * @since GDAL 2.1
 */

void GDALGridOptionsFree(GDALGridOptions *psOptions)
{
    if( psOptions )
    {
        CPLFree(psOptions->pszFormat);
        CSLDestroy(psOptions->papszLayers);
        CPLFree(psOptions->pszBurnAttribute);
        CPLFree(psOptions->pszWHERE);
        CPLFree(psOptions->pszSQL);
        CSLDestroy(psOptions->papszCreateOptions);
        CPLFree(psOptions->pOptions);
        CPLFree(psOptions->pszOutputSRS);
        delete psOptions->poSpatialFilter;
        delete psOptions->poClipSrc;
        CPLFree(psOptions->pszClipSrcDS);
        CPLFree(psOptions->pszClipSrcSQL);
        CPLFree(psOptions->pszClipSrcLayer);
        CPLFree(psOptions->pszClipSrcWhere);
        CPLFree(psOptions);
    }
}

/************************************************************************/
/*                     GDALGridOptionsSetProgress()                     */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALGrid().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALGridOptionsSetProgress( GDALGridOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = FALSE;
}
