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
#include "commonutils.h"

#include <cmath>
#include <cstdint>
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
    /*! output format. Use the short format name. */
    char *pszFormat;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet;

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
    double          dfXRes;
    double          dfYRes;
    double          dfXMin;
    double          dfXMax;
    double          dfYMin;
    double          dfYMax;
    bool            bIsXExtentSet;
    bool            bIsYExtentSet;
    GDALGridAlgorithm eAlgorithm;
    void            *pOptions;
    char            *pszOutputSRS;
    OGRGeometry     *poSpatialFilter;
    bool            bClipSrc;
    OGRGeometry     *poClipSrc;
    char            *pszClipSrcDS;
    char            *pszClipSrcSQL;
    char            *pszClipSrcLayer;
    char            *pszClipSrcWhere;
    bool             bNoDataSet;
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
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameInvDist);
        GDALGridInverseDistanceToAPowerOptions *pOptions2 =
            static_cast<GDALGridInverseDistanceToAPowerOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"power=%f:smoothing=%f:radius1=%f:radius2=%f:angle=%f"
                  ":max_points=%lu:min_points=%lu:nodata=%f\"\n",
                  pOptions2->dfPower,
                  pOptions2->dfSmoothing,
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMaxPoints),
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_InverseDistanceToAPowerNearestNeighbor:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameInvDistNearestNeighbor);
        GDALGridInverseDistanceToAPowerNearestNeighborOptions *pOptions2 =
            static_cast<GDALGridInverseDistanceToAPowerNearestNeighborOptions *>(
                pOptions);
        CPLprintf("Options are "
                  "\"power=%f:smoothing=%f:radius=%f"
                  ":max_points=%lu:min_points=%lu:nodata=%f\"\n",
                  pOptions2->dfPower,
                  pOptions2->dfSmoothing,
                  pOptions2->dfRadius,
                  static_cast<unsigned long>(pOptions2->nMaxPoints),
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MovingAverage:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameAverage);
        GDALGridMovingAverageOptions *pOptions2 =
            static_cast<GDALGridMovingAverageOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_NearestNeighbor:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameNearest);
        GDALGridNearestNeighborOptions *pOptions2 =
            static_cast<GDALGridNearestNeighborOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MetricMinimum:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameMinimum);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
            break;
    }
    case GGA_MetricMaximum:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameMaximum);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MetricRange:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameRange);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MetricCount:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameCount);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MetricAverageDistance:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameAverageDistance);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_MetricAverageDistancePts:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameAverageDistancePts);
        GDALGridDataMetricsOptions *pOptions2 =
            static_cast<GDALGridDataMetricsOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                  ":nodata=%f\"\n",
                  pOptions2->dfRadius1,
                  pOptions2->dfRadius2,
                  pOptions2->dfAngle,
                  static_cast<unsigned long>(pOptions2->nMinPoints),
                  pOptions2->dfNoDataValue);
        break;
    }
    case GGA_Linear:
    {
        printf("Algorithm name: \"%s\".\n", szAlgNameLinear );
        GDALGridLinearOptions *pOptions2 =
            static_cast<GDALGridLinearOptions *>(pOptions);
        CPLprintf("Options are "
                  "\"radius=%f:nodata=%f\"\n",
                  pOptions2->dfRadius,
                  pOptions2->dfNoDataValue);
        break;
    }
    default:
    {
        printf("Algorithm is unknown.\n");
        break;
    }
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
    if (nullptr == poGeom)
        return;

    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch (eType)
    {
    case wkbPoint:
        return ProcessGeometry(poGeom->toPoint(), poClipSrc,
            iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
    case wkbLinearRing:
    case wkbLineString:
        {
            OGRLineString *poLS = poGeom->toLineString();
            OGRPoint point;
            for (int pointIndex = 0; pointIndex < poLS->getNumPoints(); pointIndex++)
            {
                poLS->getPoint(pointIndex, &point);
                ProcessCommonGeometry(&point, poClipSrc,
                    iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);
            }
        }
        break;
    case wkbPolygon:
        {
            OGRPolygon* poPoly = poGeom->toPolygon();
            OGRLinearRing* poRing = poPoly->getExteriorRing();
            ProcessCommonGeometry(poRing, poClipSrc,
                iBurnField, dfBurnValue, dfIncreaseBurnValue, dfMultiplyBurnValue, adfX, adfY, adfZ);

            const int nRings = poPoly->getNumInteriorRings();
            if (nRings > 0)
            {
                for (int ir = 0; ir < nRings; ++ir)
                {
                    poRing = poPoly->getInteriorRing(ir);
                    ProcessCommonGeometry(poRing, poClipSrc,
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
            OGRGeometryCollection* pOGRGeometryCollection = poGeom->toGeometryCollection();
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
                          bool& bIsXExtentSet, bool& bIsYExtentSet,
                          double& dfXMin, double& dfXMax,
                          double& dfYMin, double& dfYMax,
                          const char *pszBurnAttribute,
                          const double dfIncreaseBurnValue,
                          const double dfMultiplyBurnValue,
                          GDALDataType eType,
                          GDALGridAlgorithm eAlgorithm, void *pOptions,
                            bool bQuiet, GDALProgressFunc pfnProgress,
                            void* pProgressData )

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

    while( (poFeat = reinterpret_cast<OGRFeature*>(OGR_L_GetNextFeature( hSrcLayer ))) != nullptr )
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
            bIsXExtentSet = true;
        }

        if ( !bIsYExtentSet )
        {
            dfYMin = sEnvelope.MinY;
            dfYMax = sEnvelope.MaxY;
            bIsYExtentSet = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform gridding.                                               */
/* -------------------------------------------------------------------- */

    const double dfDeltaX = (dfXMax - dfXMin) / nXSize;
    const double dfDeltaY = (dfYMax - dfYMin) / nYSize;

    if ( !bQuiet )
    {
        printf( "Grid data type is \"%s\"\n", GDALGetDataTypeName(eType) );
        printf("Grid size = (%lu %lu).\n",
               static_cast<unsigned long>(nXSize),
               static_cast<unsigned long>(nYSize));
        CPLprintf( "Corner coordinates = (%f %f)-(%f %f).\n",
                dfXMin - dfDeltaX / 2, dfYMax + dfDeltaY / 2,
                dfXMax + dfDeltaX / 2, dfYMin - dfDeltaY / 2 );
        CPLprintf( "Grid cell size = (%f %f).\n", dfDeltaX, dfDeltaY );
        printf("Source point count = %lu.\n",
               static_cast<unsigned long>(adfX.size()));
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

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);

    // Try to grow the work buffer up to 16 MB if it is smaller
    GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
    if( nXSize == 0 || nYSize == 0 || nBlockXSize == 0 || nBlockYSize == 0 )
        return CE_Failure;

    const int nDesiredBufferSize = 16*1024*1024;
    if( nBlockXSize < nXSize && nBlockYSize < nYSize &&
        nBlockXSize < nDesiredBufferSize / (nBlockYSize * nDataTypeSize) )
    {
        const int nNewBlockXSize =
            nDesiredBufferSize / (nBlockYSize * nDataTypeSize);
        nBlockXSize = (nNewBlockXSize / nBlockXSize) * nBlockXSize;
        if( nBlockXSize > nXSize )
            nBlockXSize = nXSize;
    }
    else if( nBlockXSize == nXSize && nBlockYSize < nYSize &&
             nBlockYSize < nDesiredBufferSize / (nXSize * nDataTypeSize) )
    {
        const int nNewBlockYSize =
            nDesiredBufferSize / (nXSize * nDataTypeSize);
        nBlockYSize = (nNewBlockYSize / nBlockYSize) * nBlockYSize;
        if( nBlockYSize > nYSize )
            nBlockYSize = nYSize;
    }
    CPLDebug("GDAL_GRID", "Work buffer: %d * %d", nBlockXSize, nBlockYSize);

    void *pData = VSIMalloc3(nBlockXSize, nBlockYSize, nDataTypeSize);
    if( pData == nullptr )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate work buffer");
        return CE_Failure;
    }

    int nBlock = 0;
    const int nBlockCount =
        ((nXSize + nBlockXSize - 1) / nBlockXSize) *
        ((nYSize + nBlockYSize - 1) / nBlockYSize);

    GDALGridContext* psContext = GDALGridContextCreate( eAlgorithm, pOptions,
                                                        static_cast<int>(adfX.size()),
                                                        &(adfX[0]), &(adfY[0]), &(adfZ[0]),
                                                        TRUE );
    if( psContext == nullptr )
    {
        CPLFree( pData );
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    for ( int nYOffset = 0;
          nYOffset < nYSize && eErr == CE_None;
          nYOffset += nBlockYSize )
    {
        for ( int nXOffset = 0;
              nXOffset < nXSize && eErr == CE_None;
              nXOffset += nBlockXSize )
        {
            void *pScaledProgress = GDALCreateScaledProgress(
                static_cast<double>(nBlock) / nBlockCount,
                static_cast<double>(nBlock + 1) / nBlockCount,
                pfnProgress, pProgressData);
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
    GDALDataset *poDS = static_cast<GDALDataset*>(
        GDALOpenEx(pszDS, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if ( poDS == nullptr )
        return nullptr;

    OGRLayer *poLyr = nullptr;
    if ( pszSQL != nullptr )
        poLyr = poDS->ExecuteSQL( pszSQL, nullptr, nullptr );
    else if ( pszLyr != nullptr )
        poLyr = poDS->GetLayerByName( pszLyr );
    else
        poLyr = poDS->GetLayer(0);

    if ( poLyr == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource." );
        GDALClose(poDS);
        return nullptr;
    }

    if ( pszWhere )
        poLyr->SetAttributeFilter( pszWhere );

    OGRGeometryCollection *poGeom = nullptr;
    OGRFeature *poFeat = nullptr;
    while ( (poFeat = poLyr->GetNextFeature()) != nullptr )
    {
        OGRGeometry* poSrcGeom = poFeat->GetGeometryRef();
        if ( poSrcGeom )
        {
            const OGRwkbGeometryType eType =
                wkbFlatten(poSrcGeom->getGeometryType());

            if ( poGeom == nullptr )
                poGeom = new OGRMultiPolygon();

            if ( eType == wkbPolygon )
            {
                poGeom->addGeometry( poSrcGeom );
            }
            else if ( eType == wkbMultiPolygon )
            {
                const int nGeomCount =
                    static_cast<OGRMultiPolygon *>(poSrcGeom)->
                        getNumGeometries();

                for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
                {
                    poGeom->addGeometry(
                        static_cast<OGRMultiPolygon *>(poSrcGeom)->
                            getGeometryRef(iGeom) );
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry not of polygon type.");
                OGRGeometryFactory::destroyGeometry( poGeom );
                OGRFeature::DestroyFeature( poFeat );
                if ( pszSQL != nullptr )
                    poDS->ReleaseResultSet( poLyr );
                GDALClose(poDS);
                return nullptr;
            }
        }

        OGRFeature::DestroyFeature( poFeat );
    }

    if( pszSQL != nullptr )
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
 * This is the equivalent of the <a href="/programs/gdal_grid.html">gdal_grid</a> utility.
 *
 * GDALGridOptions* must be allocated and freed with GDALGridOptionsNew()
 * and GDALGridOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALGridOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALGrid( const char *pszDest, GDALDatasetH hSrcDataset,
                       const GDALGridOptions *psOptionsIn, int *pbUsageError )

{
    if( hSrcDataset == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( pszDest == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALGridOptions* psOptionsToFree = nullptr;
    const GDALGridOptions* psOptions = psOptionsIn;
    if( psOptions == nullptr )
    {
        psOptionsToFree = GDALGridOptionsNew(nullptr, nullptr);
        psOptions = psOptionsToFree;
    }

    GDALDataset* poSrcDS = static_cast<GDALDataset *>(hSrcDataset);

    if( psOptions->pszSQL == nullptr && psOptions->papszLayers == nullptr &&
        poSrcDS->GetLayerCount() != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset has not one single layer.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALGridOptionsFree(psOptionsToFree);
        return nullptr;
    }

    if ( (psOptions->nXSize != 0 || psOptions->nYSize != 0) && (psOptions->dfXRes != 0 || psOptions->dfYRes != 0) ) {
        CPLError(CE_Failure, CPLE_IllegalArg, "-outsize and -tr options cannot be used at the same time.");
        GDALGridOptionsFree(psOptionsToFree);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    CPLString osFormat;
    if( psOptions->pszFormat == nullptr )
    {
        osFormat = GetOutputDriverForRaster(pszDest);
        if( osFormat.empty() )
        {
            GDALGridOptionsFree(psOptionsToFree);
            return nullptr;
        }
    }
    else
    {
        osFormat = psOptions->pszFormat;
    }

    GDALDriverH hDriver = GDALGetDriverByName( osFormat );
    if( hDriver == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output driver `%s' not recognised.", osFormat.c_str() );
        fprintf( stderr,
        "The following format drivers are configured and support output:\n" );
        for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, nullptr) != nullptr &&
                ( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, nullptr ) != nullptr
                || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, nullptr ) != nullptr) )
            {
                fprintf( stderr, "  %s: %s\n",
                         GDALGetDriverShortName( hDriver  ),
                         GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        GDALGridOptionsFree(psOptionsToFree);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create target raster file.                                      */
/* -------------------------------------------------------------------- */
    int nLayerCount = CSLCount(psOptions->papszLayers);
    if( nLayerCount == 0 && psOptions->pszSQL == nullptr )
        nLayerCount = 1; /* due to above check */

    int nBands = nLayerCount;

    if ( psOptions->pszSQL )
        nBands++;

    int nXSize;
    int nYSize;
    if ( psOptions->dfXRes != 0 && psOptions->dfYRes != 0 )
    {
        if ((psOptions->dfXMax == psOptions->dfXMin) || (psOptions->dfYMax == psOptions->dfYMin)) {
            CPLError( CE_Failure, CPLE_IllegalArg,
                    "Invalid txe or tye parameters detected. Please check your -txe or -tye argument.");

            if(pbUsageError)
                *pbUsageError = TRUE;
            GDALGridOptionsFree(psOptionsToFree);
            return nullptr;
        }

        double dfXSize = (std::fabs(psOptions->dfXMax - psOptions->dfXMin) + (psOptions->dfXRes/2.0)) /
            psOptions->dfXRes;
        double dfYSize = (std::fabs(psOptions->dfYMax - psOptions->dfYMin) + (psOptions->dfYRes/2.0)) /
            psOptions->dfYRes;

        if (dfXSize >= 1 && dfXSize <= INT_MAX && dfYSize >= 1 && dfYSize <= INT_MAX) {
            nXSize = static_cast<int>(dfXSize);
            nYSize = static_cast<int>(dfYSize);
        } else {
            CPLError( CE_Failure, CPLE_IllegalArg, "Invalid output size detected. Please check your -tr argument");

            if(pbUsageError)
                *pbUsageError = TRUE;
            GDALGridOptionsFree(psOptionsToFree);
            return nullptr;
        }
    }
    else
    {
        // FIXME
        nXSize = psOptions->nXSize;
        if ( nXSize == 0 )
            nXSize = 256;
        nYSize = psOptions->nYSize;
        if ( nYSize == 0 )
            nYSize = 256;
    }

    GDALDatasetH hDstDS =
        GDALCreate(hDriver, pszDest, nXSize, nYSize, nBands,
                   psOptions->eOutputType, psOptions->papszCreateOptions);
    if ( hDstDS == nullptr )
    {
        GDALGridOptionsFree(psOptionsToFree);
        return nullptr;
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
    bool bIsXExtentSet = psOptions->bIsXExtentSet;
    bool bIsYExtentSet = psOptions->bIsYExtentSet;
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */

    if( psOptions->pszSQL != nullptr )
    {
        OGRLayer* poLayer = poSrcDS->ExecuteSQL(psOptions->pszSQL,
                                                psOptions->poSpatialFilter, nullptr );
        if( poLayer != nullptr )
        {
            // Custom layer will be rasterized in the first band.
            eErr = ProcessLayer( reinterpret_cast<OGRLayerH>(poLayer), hDstDS, psOptions->poSpatialFilter,
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
    char* pszOutputSRS =
        psOptions->pszOutputSRS ? CPLStrdup(psOptions->pszOutputSRS) : nullptr;
    for( int i = 0; i < nLayerCount; i++ )
    {
        OGRLayerH hLayer =
            psOptions->papszLayers == nullptr
            ? GDALDatasetGetLayer(hSrcDataset, 0)
            : GDALDatasetGetLayerByName(hSrcDataset, psOptions->papszLayers[i]);
        if( hLayer == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unable to find layer \"%s\", skipping.",
                     psOptions->papszLayers && psOptions->papszLayers[i] ?
                            psOptions->papszLayers[i] : "null" );
            continue;
        }

        if( psOptions->pszWHERE )
        {
            if( OGR_L_SetAttributeFilter( hLayer, psOptions->pszWHERE ) != OGRERR_NONE )
                break;
        }

        if ( psOptions->poSpatialFilter != nullptr )
            OGR_L_SetSpatialFilter( hLayer, reinterpret_cast<OGRGeometryH>(psOptions->poSpatialFilter) );

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
    double adfGeoTransform[6] = {
        dfXMin,
        (dfXMax - dfXMin) / nXSize,
        0.0,
        dfYMin,
        0.0,
        (dfYMax - dfYMin) / nYSize };
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

    if( eErr != CE_None )
    {
        GDALClose(hDstDS);
        return nullptr;
    }

    return hDstDS;
}

/************************************************************************/
/*                            IsNumber()                               */
/************************************************************************/

static bool IsNumber(const char* pszStr)
{
    if (*pszStr == '-' || *pszStr == '+')
        pszStr ++;
    if (*pszStr == '.')
        pszStr ++;
    return *pszStr >= '0' && *pszStr <= '9';
}

/************************************************************************/
/*                             GDALGridOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALGridOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
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

    psOptions->pszFormat = nullptr;
    psOptions->bQuiet = true;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = nullptr;
    psOptions->papszLayers = nullptr;
    psOptions->pszBurnAttribute = nullptr;
    psOptions->dfIncreaseBurnValue = 0.0;
    psOptions->dfMultiplyBurnValue = 1.0;
    psOptions->pszWHERE = nullptr;
    psOptions->pszSQL = nullptr;
    psOptions->eOutputType = GDT_Float64;
    psOptions->papszCreateOptions = nullptr;
    psOptions->nXSize = 0;
    psOptions->nYSize = 0;
    psOptions->dfXRes = 0;
    psOptions->dfYRes = 0;
    psOptions->dfXMin = 0.0;
    psOptions->dfXMax = 0.0;
    psOptions->dfYMin = 0.0;
    psOptions->dfYMax = 0.0;
    psOptions->bIsXExtentSet = false;
    psOptions->bIsYExtentSet = false;
    psOptions->eAlgorithm = GGA_InverseDistanceToAPower;
    psOptions->pOptions = nullptr;
    psOptions->pszOutputSRS = nullptr;
    psOptions->poSpatialFilter = nullptr;
    psOptions->poClipSrc = nullptr;
    psOptions->bClipSrc = false;
    psOptions->pszClipSrcDS = nullptr;
    psOptions->pszClipSrcSQL = nullptr;
    psOptions->pszClipSrcLayer = nullptr;
    psOptions->pszClipSrcWhere = nullptr;
    psOptions->bNoDataSet = false;
    psOptions->dfNoDataValue = 0;

    ParseAlgorithmAndOptions( szAlgNameInvDist, &psOptions->eAlgorithm, &psOptions->pOptions );

    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;
/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for( int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = true;
        }

        else if( EQUAL(papszArgv[i],"-ot") && papszArgv[i+1] )
        {
            for( int iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", papszArgv[i+1] );
                GDALGridOptionsFree(psOptions);
                return nullptr;
            }
            i++;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-txe") )
        {
            psOptions->dfXMin = CPLAtof(papszArgv[++i]);
            psOptions->dfXMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsXExtentSet = true;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-tye") )
        {
            psOptions->dfYMin = CPLAtof(papszArgv[++i]);
            psOptions->dfYMax = CPLAtof(papszArgv[++i]);
            psOptions->bIsYExtentSet = true;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-outsize") )
        {
            CPLAssert(papszArgv[i+1]);
            CPLAssert(papszArgv[i+2]);
            psOptions->nXSize = atoi(papszArgv[i+1]);
            psOptions->nYSize = atoi(papszArgv[i+2]);
            i += 2;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-tr") )
        {
            psOptions->dfXRes = CPLAtofM(papszArgv[++i]);
            psOptions->dfYRes = CPLAtofM(papszArgv[++i]);
            if( psOptions->dfXRes <= 0 || psOptions->dfYRes <= 0 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Wrong value for -tr parameters.");
                GDALGridOptionsFree(psOptions);
                return nullptr;
            }
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
            OGRPolygon *poPoly = new OGRPolygon();
            poPoly->addRing(&oRing);
            psOptions->poSpatialFilter = poPoly;
            i += 4;
        }

        else if( EQUAL(papszArgv[i],"-clipsrc") )
        {
            if (i + 1 >= argc || papszArgv[i+1] == nullptr)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "%s option requires 1 or 4 arguments", papszArgv[i]);
                GDALGridOptionsFree(psOptions);
                return nullptr;
            }

            VSIStatBufL  sStat;
            psOptions->bClipSrc = true;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != nullptr
                 && papszArgv[i+3] != nullptr
                 && papszArgv[i+4] != nullptr)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                delete psOptions->poClipSrc;
                OGRPolygon *poPoly = static_cast<OGRPolygon *>(
                    OGRGeometryFactory::createGeometry(wkbPolygon));
                poPoly->addRing(&oRing);
                psOptions->poClipSrc = poPoly;
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                delete psOptions->poClipSrc;
                OGRGeometryFactory::createFromWkt(papszArgv[i+1],
                                            nullptr, &psOptions->poClipSrc);
                if (psOptions->poClipSrc == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALGridOptionsFree(psOptions);
                    return nullptr;
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
                return nullptr;
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
                return nullptr;
            }

            char **papszParams = CSLTokenizeString2( pszAlgorithm, ":", FALSE );
            const char* pszNoDataValue = CSLFetchNameValue( papszParams, "nodata" );
            if( pszNoDataValue != nullptr )
            {
                psOptions->bNoDataSet = true;
                psOptions->dfNoDataValue = CPLAtofM(pszNoDataValue);
            }
            CSLDestroy(papszParams);
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALGridOptionsFree(psOptions);
            return nullptr;
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
            return nullptr;
        }
    }

    if ( psOptions->bClipSrc && psOptions->pszClipSrcDS != nullptr )
    {
        psOptions->poClipSrc = LoadGeometry( psOptions->pszClipSrcDS, psOptions->pszClipSrcSQL,
                                  psOptions->pszClipSrcLayer, psOptions->pszClipSrcWhere );
        if ( psOptions->poClipSrc == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load source clip geometry.");
            GDALGridOptionsFree(psOptions);
            return nullptr;
        }
    }
    else if ( psOptions->bClipSrc && psOptions->poClipSrc == nullptr && !psOptions->poSpatialFilter )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "-clipsrc must be used with -spat option or \n"
                 "a bounding box, WKT string or datasource must be "
                 "specified.");
        GDALGridOptionsFree(psOptions);
        return nullptr;
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
            psOptions->poClipSrc = nullptr;
        }
    }
    else
    {
        if ( psOptions->poClipSrc )
        {
            psOptions->poSpatialFilter = psOptions->poClipSrc;
            psOptions->poClipSrc = nullptr;
        }
    }

    if( psOptionsForBinary )
    {
        if( psOptions->pszFormat )
        {
            psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
        }
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
    if( psOptions == nullptr )
        return;

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
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = false;
}
