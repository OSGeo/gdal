/* ****************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL scattered data gridding (interpolation) tool
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 * ****************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include <cstdlib>
#include <vector>
#include <algorithm>

#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdalgrid.h"
#include "commonutils.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( 
        "Usage: gdal_grid [--help-general] [--formats]\n"
        "    [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
        "          CInt16/CInt32/CFloat32/CFloat64}]\n"
        "    [-of format] [-co \"NAME=VALUE\"]\n"
        "    [-zfield field_name] [-z_increase increase_value] [-z_multiply multiply_value]\n"
        "    [-a_srs srs_def] [-spat xmin ymin xmax ymax]\n"
        "    [-clipsrc <xmin ymin xmax ymax>|WKT|datasource|spat_extent]\n"
        "    [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
        "    [-clipsrcwhere expression]\n"
        "    [-l layername]* [-where expression] [-sql select_statement]\n"
        "    [-txe xmin xmax] [-tye ymin ymax] [-outsize xsize ysize]\n"
        "    [-a algorithm[:parameter1=value1]*]"
        "    [-q]\n"
        "    <src_datasource> <dst_filename>\n"
        "\n"
        "Available algorithms and parameters with their's defaults:\n"
        "    Inverse distance to a power (default)\n"
        "        invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0\n"
        "    Moving average\n"
        "        average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "    Nearest neighbor\n"
        "        nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0\n"
        "    Various data metrics\n"
        "        <metric name>:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "        possible metrics are:\n"
        "            minimum\n"
        "            maximum\n"
        "            range\n"
        "            count\n"
        "            average_distance\n"
        "            average_distance_pts\n"
        "\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    GDALDestroyDriverManager();
    exit( 1 );
}

/************************************************************************/
/*                          GetAlgorithmName()                          */
/*                                                                      */
/*      Translates algortihm code into mnemonic name.                   */
/************************************************************************/

static void PrintAlgorithmAndOptions( GDALGridAlgorithm eAlgorithm,
                                      void *pOptions )
{
    switch ( eAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
            printf( "Algorithm name: \"%s\".\n", szAlgNameInvDist );
            printf( "Options are "
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
        case GGA_MovingAverage:
            printf( "Algorithm name: \"%s\".\n", szAlgNameAverage );
            printf( "Options are "
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
            printf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:nodata=%f\"\n",
                ((GDALGridNearestNeighborOptions *)pOptions)->dfRadius1,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfRadius2,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfAngle,
                ((GDALGridNearestNeighborOptions *)pOptions)->dfNoDataValue);
            break;
        case GGA_MetricMinimum:
            printf( "Algorithm name: \"%s\".\n", szAlgNameMinimum );
            printf( "Options are "
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
            printf( "Options are "
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
            printf( "Options are "
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
            printf( "Options are "
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
            printf( "Options are "
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
            printf( "Options are "
                    "\"radius1=%f:radius2=%f:angle=%f:min_points=%lu"
                    ":nodata=%f\"\n",
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius1,
                ((GDALGridDataMetricsOptions *)pOptions)->dfRadius2,
                ((GDALGridDataMetricsOptions *)pOptions)->dfAngle,
                (unsigned long)((GDALGridDataMetricsOptions *)pOptions)->nMinPoints,
                ((GDALGridDataMetricsOptions *)pOptions)->dfNoDataValue);
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
/*  Z value as requested. Test whther we are in the clipped region      */
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
/*  Process recursivelly geometry and extract points                    */
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
                    OGRLinearRing* poRing = poPoly->getInteriorRing(ir);
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
                          GUInt32 nXSize, GUInt32 nYSize, int nBand,
                          int& bIsXExtentSet, int& bIsYExtentSet,
                          double& dfXMin, double& dfXMax,
                          double& dfYMin, double& dfYMax,
                          const char *pszBurnAttribute,
                          const double dfIncreaseBurnValue,
                          const double dfMultiplyBurnValue,
                          GDALDataType eType,
                          GDALGridAlgorithm eAlgorithm, void *pOptions,
                          int bQuiet, GDALProgressFunc pfnProgress )

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

    if ( adfX.size() == 0 )
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
        printf( "Corner coordinates = (%f %f)-(%f %f).\n",
                dfXMin - dfDeltaX / 2, dfYMax + dfDeltaY / 2,
                dfXMax + dfDeltaX / 2, dfYMin - dfDeltaY / 2 );
        printf( "Grid cell size = (%f %f).\n", dfDeltaX, dfDeltaY );
        printf( "Source point count = %lu.\n", (unsigned long)adfX.size() );
        PrintAlgorithmAndOptions( eAlgorithm, pOptions );
        printf("\n");
    }

    GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, nBand );

    if (adfX.size() == 0)
    {
        // FIXME: Shoulda' set to nodata value instead
        GDALFillRaster( hBand, 0.0 , 0.0 );
        return CE_None;
    }

    GUInt32 nXOffset, nYOffset;
    int     nBlockXSize, nBlockYSize;
    int     nDataTypeSize = GDALGetDataTypeSize(eType) / 8;

    // Try to grow the work buffer up to 16 MB if it is smaller
    GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
    const GUInt32 nDesiredBufferSize = 16*1024*1024;
    if( (GUInt32)nBlockXSize < nXSize && (GUInt32)nBlockYSize < nYSize &&
        (GUInt32)nBlockXSize < nDesiredBufferSize / (nBlockYSize * nDataTypeSize) )
    {
        int nNewBlockXSize  = nDesiredBufferSize / (nBlockYSize * nDataTypeSize);
        nBlockXSize = (nNewBlockXSize / nBlockXSize) * nBlockXSize;
        if( (GUInt32)nBlockXSize > nXSize )
            nBlockXSize = nXSize;
    }
    else if( (GUInt32)nBlockXSize == nXSize && (GUInt32)nBlockYSize < nYSize &&
             (GUInt32)nBlockYSize < nDesiredBufferSize / (nXSize * nDataTypeSize) )
    {
        int nNewBlockYSize = nDesiredBufferSize / (nXSize * nDataTypeSize);
        nBlockYSize = (nNewBlockYSize / nBlockYSize) * nBlockYSize;
        if( (GUInt32)nBlockYSize > nYSize )
            nBlockYSize = nYSize;
    }
    CPLDebug("GDAL_GRID", "Work buffer: %d * %d", nBlockXSize, nBlockYSize);

    void    *pData =
        VSIMalloc3( nBlockXSize, nBlockYSize, nDataTypeSize );
    if( pData == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate work buffer");
        return CE_Failure;
    }

    GUInt32 nBlock = 0;
    GUInt32 nBlockCount = ((nXSize + nBlockXSize - 1) / nBlockXSize)
        * ((nYSize + nBlockYSize - 1) / nBlockYSize);

    CPLErr eErr = CE_None;
    for ( nYOffset = 0; nYOffset < nYSize && eErr == CE_None; nYOffset += nBlockYSize )
    {
        for ( nXOffset = 0; nXOffset < nXSize && eErr == CE_None; nXOffset += nBlockXSize )
        {
            void *pScaledProgress;
            pScaledProgress =
                GDALCreateScaledProgress( (double)nBlock / nBlockCount,
                                          (double)(nBlock + 1) / nBlockCount,
                                          pfnProgress, NULL );
            nBlock ++;

            int nXRequest = nBlockXSize;
            if (nXOffset + nXRequest > nXSize)
                nXRequest = nXSize - nXOffset;

            int nYRequest = nBlockYSize;
            if (nYOffset + nYRequest > nYSize)
                nYRequest = nYSize - nYOffset;

            eErr = GDALGridCreate( eAlgorithm, pOptions,
                            adfX.size(), &(adfX[0]), &(adfY[0]), &(adfZ[0]),
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
        fprintf( stderr,
            "FAILURE: Failed to identify source layer from datasource.\n" );
        GDALClose( (GDALDatasetH) poDS );
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
                fprintf( stderr, "FAILURE: Geometry not of polygon type.\n" );
                OGRGeometryFactory::destroyGeometry( poGeom );
                OGRFeature::DestroyFeature( poFeat );
                if ( pszSQL != NULL )
                    poDS->ReleaseResultSet( poLyr );
                GDALClose( (GDALDatasetH) poDS );
                return NULL;
            }
        }
    
        OGRFeature::DestroyFeature( poFeat );
    }
    
    if( pszSQL != NULL )
        poDS->ReleaseResultSet( poLyr );
    GDALClose( (GDALDatasetH) poDS );
    
    return poGeom;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", argv[i], nExtraArg)); } while(0)

int main( int argc, char ** argv )
{
    GDALDriverH     hDriver;
    const char      *pszSource=NULL, *pszDest=NULL, *pszFormat = "GTiff";
    int             bFormatExplicitelySet = FALSE;
    char            **papszLayers = NULL;
    const char      *pszBurnAttribute = NULL;
    double          dfIncreaseBurnValue = 0.0;
    double          dfMultiplyBurnValue = 1.0;
    const char      *pszWHERE = NULL, *pszSQL = NULL;
    GDALDataType    eOutputType = GDT_Float64;
    char            **papszCreateOptions = NULL;
    GUInt32         nXSize = 0, nYSize = 0;
    double          dfXMin = 0.0, dfXMax = 0.0, dfYMin = 0.0, dfYMax = 0.0;
    int             bIsXExtentSet = FALSE, bIsYExtentSet = FALSE;
    GDALGridAlgorithm eAlgorithm = GGA_InverseDistanceToAPower;
    void            *pOptions = NULL;
    char            *pszOutputSRS = NULL;
    int             bQuiet = FALSE;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    int             i;
    OGRGeometry     *poSpatialFilter = NULL;
    int             bClipSrc = FALSE;
    OGRGeometry     *poClipSrc = NULL;
    const char      *pszClipSrcDS = NULL;
    const char      *pszClipSrcSQL = NULL;
    const char      *pszClipSrcLayer = NULL;
    const char      *pszClipSrcWhere = NULL;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    GDALAllRegister();
    OGRRegisterAll();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
            Usage();
        else if( EQUAL(argv[i],"-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszFormat = argv[++i];
            bFormatExplicitelySet = TRUE;
        }

        else if( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet") )
        {
            bQuiet = TRUE;
            pfnProgress = GDALDummyProgress;
        }

        else if( EQUAL(argv[i],"-ot") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eOutputType = (GDALDataType) iType;
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                Usage(CPLSPrintf("Unknown output pixel type: %s.",
                                 argv[i + 1] ));
            }
            i++;
        }

        else if( EQUAL(argv[i],"-txe") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            dfXMin = atof(argv[++i]);
            dfXMax = atof(argv[++i]);
            bIsXExtentSet = TRUE;
        }   

        else if( EQUAL(argv[i],"-tye") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            dfYMin = atof(argv[++i]);
            dfYMax = atof(argv[++i]);
            bIsYExtentSet = TRUE;
        }   

        else if( EQUAL(argv[i],"-outsize") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            nXSize = atoi(argv[++i]);
            nYSize = atoi(argv[++i]);
        }   

        else if( EQUAL(argv[i],"-co") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }   

        else if( EQUAL(argv[i],"-zfield") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszBurnAttribute = argv[++i];
        }

        else if( EQUAL(argv[i],"-z_increase") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfIncreaseBurnValue = atof(argv[++i]);
        }

        else if( EQUAL(argv[i],"-z_multiply") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfMultiplyBurnValue = atof(argv[++i]);
        }

        else if( EQUAL(argv[i],"-where") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszWHERE = argv[++i];
        }

        else if( EQUAL(argv[i],"-l") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszLayers = CSLAddString( papszLayers, argv[++i] );
        }

        else if( EQUAL(argv[i],"-sql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSQL = argv[++i];
        }

        else if( EQUAL(argv[i],"-spat") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            OGRLinearRing  oRing;

            oRing.addPoint( atof(argv[i+1]), atof(argv[i+2]) );
            oRing.addPoint( atof(argv[i+1]), atof(argv[i+4]) );
            oRing.addPoint( atof(argv[i+3]), atof(argv[i+4]) );
            oRing.addPoint( atof(argv[i+3]), atof(argv[i+2]) );
            oRing.addPoint( atof(argv[i+1]), atof(argv[i+2]) );

            poSpatialFilter = new OGRPolygon();
            ((OGRPolygon *) poSpatialFilter)->addRing( &oRing );
            i += 4;
        }

        else if ( EQUAL(argv[i],"-clipsrc") )
        {
            if (i + 1 >= argc)
                Usage(CPLSPrintf("%s option requires 1 or 4 arguments", argv[i]));

            bClipSrc = TRUE;
            errno = 0;
            const double unused = strtod( argv[i + 1], NULL );    // XXX: is it a number or not?
            if ( errno != 0
                 && argv[i + 2] != NULL
                 && argv[i + 3] != NULL
                 && argv[i + 4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( atof(argv[i + 1]), atof(argv[i + 2]) );
                oRing.addPoint( atof(argv[i + 1]), atof(argv[i + 4]) );
                oRing.addPoint( atof(argv[i + 3]), atof(argv[i + 4]) );
                oRing.addPoint( atof(argv[i + 3]), atof(argv[i + 2]) );
                oRing.addPoint( atof(argv[i + 1]), atof(argv[i + 2]) );

                poClipSrc = new OGRPolygon();
                ((OGRPolygon *) poClipSrc)->addRing( &oRing );
                i += 4;

                (void)unused;
            }
            else if (EQUALN(argv[i + 1], "POLYGON", 7)
                     || EQUALN(argv[i + 1], "MULTIPOLYGON", 12))
            {
                OGRGeometryFactory::createFromWkt(&argv[i + 1], NULL, &poClipSrc);
                if ( poClipSrc == NULL )
                {
                    Usage("Invalid geometry. "
                             "Must be a valid POLYGON or MULTIPOLYGON WKT.");
                }
                i++;
            }
            else if (EQUAL(argv[i + 1], "spat_extent") )
            {
                i++;
            }
            else
            {
                pszClipSrcDS = argv[i + 1];
                i++;
            }
        }

        else if ( EQUAL(argv[i], "-clipsrcsql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcSQL = argv[i + 1];
            i++;
        }

        else if ( EQUAL(argv[i], "-clipsrclayer") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcLayer = argv[i + 1];
            i++;
        }

        else if ( EQUAL(argv[i], "-clipsrcwhere") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcWhere = argv[i + 1];
            i++;
        }

        else if( EQUAL(argv[i],"-a_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            OGRSpatialReference oOutputSRS;

            if( oOutputSRS.SetFromUserInput( argv[i+1] ) != OGRERR_NONE )
            {
                fprintf( stderr, "Failed to process SRS definition: %s\n", 
                         argv[i+1] );
                GDALDestroyDriverManager();
                exit( 1 );
            }

            oOutputSRS.exportToWkt( &pszOutputSRS );
            i++;
        }   

        else if( EQUAL(argv[i],"-a") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if ( ParseAlgorithmAndOptions( argv[++i], &eAlgorithm, &pOptions )
                 != CE_None )
            {
                fprintf( stderr,
                         "Failed to process algorithm name and parameters.\n" );
                exit( 1 );
            }
        }

        else if( argv[i][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        }

        else if( pszSource == NULL )
        {
            pszSource = argv[i];
        }

        else if( pszDest == NULL )
        {
            pszDest = argv[i];
        }

        else
        {
            Usage("Too many command options.");
        }
    }

    if( pszSource == NULL )
    {
        Usage("Source datasource is not specified.");
    }
    if( pszDest == NULL )
    {
        Usage("Target dataset is not specified.");
    }
    if( pszSQL == NULL && papszLayers == NULL )
    {
        Usage("Neither -sql nor -l are specified.");
    }
    
    if ( bClipSrc && pszClipSrcDS != NULL )
    {
        poClipSrc = LoadGeometry( pszClipSrcDS, pszClipSrcSQL,
                                  pszClipSrcLayer, pszClipSrcWhere );
        if ( poClipSrc == NULL )
        {
            Usage("Cannot load source clip geometry.");
        }
    }
    else if ( bClipSrc && poClipSrc == NULL && !poSpatialFilter )
    {
        Usage("-clipsrc must be used with -spat option or \n"
                 "a bounding box, WKT string or datasource must be "
                 "specified.");
    }

    if ( poSpatialFilter )
    {
        if ( poClipSrc )
        {
            OGRGeometry *poTemp = poSpatialFilter->Intersection( poClipSrc );

            if ( poTemp )
            {
                OGRGeometryFactory::destroyGeometry( poSpatialFilter );
                poSpatialFilter = poTemp;
            }

            OGRGeometryFactory::destroyGeometry( poClipSrc );
            poClipSrc = NULL;
        }
    }
    else
    {
        if ( poClipSrc )
        {
            poSpatialFilter = poClipSrc;
            poClipSrc = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL )
    {
        int	iDr;
        
        fprintf( stderr,
                 "FAILURE: Output driver `%s' not recognised.\n", pszFormat );
        fprintf( stderr,
        "The following format drivers are configured and support output:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
                || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY,
                                        NULL ) != NULL )
            {
                fprintf( stderr, "  %s: %s\n",
                         GDALGetDriverShortName( hDriver  ),
                         GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Open input datasource.                                          */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszSource, FALSE, NULL );
    if( hSrcDS == NULL )
    {
        fprintf( stderr, "Unable to open input datasource \"%s\".\n",
                 pszSource );
        fprintf( stderr, "%s\n", CPLGetLastErrorMsg() );
        exit( 3 );
    }

/* -------------------------------------------------------------------- */
/*      Create target raster file.                                      */
/* -------------------------------------------------------------------- */
    GDALDatasetH    hDstDS;
    int             nLayerCount = CSLCount(papszLayers);
    int             nBands = nLayerCount;

    if ( pszSQL )
        nBands++;

    // FIXME
    if ( nXSize == 0 )
        nXSize = 256;
    if ( nYSize == 0 )
        nYSize = 256;

    if (!bQuiet && !bFormatExplicitelySet)
        CheckExtensionConsistency(pszDest, pszFormat);

    hDstDS = GDALCreate( hDriver, pszDest, nXSize, nYSize, nBands,
                         eOutputType, papszCreateOptions );
    if ( hDstDS == NULL )
    {
        fprintf( stderr, "Unable to create target dataset \"%s\".\n",
                 pszDest );
        fprintf( stderr, "%s\n", CPLGetLastErrorMsg() );
        exit( 3 );
    }

/* -------------------------------------------------------------------- */
/*      If algorithm was not specified assigh default one.              */
/* -------------------------------------------------------------------- */
    if ( !pOptions )
        ParseAlgorithmAndOptions( szAlgNameInvDist, &eAlgorithm, &pOptions );

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */
    if( pszSQL != NULL )
    {
        OGRLayerH hLayer;

        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszSQL,
                                    (OGRGeometryH)poSpatialFilter, NULL ); 
        if( hLayer != NULL )
        {
            // Custom layer will be rasterized in the first band.
            ProcessLayer( hLayer, hDstDS, poSpatialFilter, nXSize, nYSize, 1,
                          bIsXExtentSet, bIsYExtentSet,
                          dfXMin, dfXMax, dfYMin, dfYMax, pszBurnAttribute,
                          dfIncreaseBurnValue, dfMultiplyBurnValue, eOutputType, eAlgorithm, pOptions,
                          bQuiet, pfnProgress );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each layer.                                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nLayerCount; i++ )
    {
        OGRLayerH hLayer = OGR_DS_GetLayerByName( hSrcDS, papszLayers[i]);
        if( hLayer == NULL )
        {
            fprintf( stderr, "Unable to find layer \"%s\", skipping.\n", 
                     papszLayers[i] );
            continue;
        }

        if( pszWHERE )
        {
            if( OGR_L_SetAttributeFilter( hLayer, pszWHERE ) != OGRERR_NONE )
                break;
        }

        if ( poSpatialFilter != NULL )
            OGR_L_SetSpatialFilter( hLayer, (OGRGeometryH)poSpatialFilter );

        // Fetch the first meaningful SRS definition
        if ( !pszOutputSRS )
        {
            OGRSpatialReferenceH hSRS = OGR_L_GetSpatialRef( hLayer );
            if ( hSRS )
                OSRExportToWkt( hSRS, &pszOutputSRS );
        }

        ProcessLayer( hLayer, hDstDS, poSpatialFilter, nXSize, nYSize,
                      i + 1 + nBands - nLayerCount,
                      bIsXExtentSet, bIsYExtentSet,
                      dfXMin, dfXMax, dfYMin, dfYMax, pszBurnAttribute,
                      dfIncreaseBurnValue, dfMultiplyBurnValue, eOutputType, eAlgorithm, pOptions,
                      bQuiet, pfnProgress );
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
        CPLFree( pszOutputSRS );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    OGR_DS_Destroy( hSrcDS );
    GDALClose( hDstDS );
    OGRGeometryFactory::destroyGeometry( poSpatialFilter );

    CPLFree( pOptions );
    CSLDestroy( papszCreateOptions );
    CSLDestroy( argv );
    CSLDestroy( papszLayers );

    OGRCleanupAll();

    GDALDestroyDriverManager();
 
    return 0;
}

