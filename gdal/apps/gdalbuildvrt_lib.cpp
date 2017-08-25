/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build VRT datasets from raster products
 *           or content of SHP tile index
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2007-2016, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_api.h"
#include "ogr_srs_api.h"

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_vrt.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

typedef enum
{
    LOWEST_RESOLUTION,
    HIGHEST_RESOLUTION,
    AVERAGE_RESOLUTION,
    USER_RESOLUTION
} ResolutionStrategy;

typedef struct
{
    int    isFileOK;
    int    nRasterXSize;
    int    nRasterYSize;
    double adfGeoTransform[6];
    int    nBlockXSize;
    int    nBlockYSize;
    GDALDataType firstBandType;
    int*         panHasNoData;
    double*      padfNoDataValues;
    int    bHasDatasetMask;
    int    nMaskBlockXSize;
    int    nMaskBlockYSize;
} DatasetProperty;

typedef struct
{
    GDALColorInterp        colorInterpretation;
    GDALDataType           dataType;
    GDALColorTableH        colorTable;
    int                    bHasNoData;
    double                 noDataValue;
} BandProperty;

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static int ArgIsNumeric( const char *pszArg )

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}

/************************************************************************/
/*                         GetSrcDstWin()                               */
/************************************************************************/

static int  GetSrcDstWin(DatasetProperty* psDP,
                  double we_res, double ns_res,
                  double minX, double minY, double maxX, double maxY,
                  double* pdfSrcXOff, double* pdfSrcYOff, double* pdfSrcXSize, double* pdfSrcYSize,
                  double* pdfDstXOff, double* pdfDstYOff, double* pdfDstXSize, double* pdfDstYSize)
{
    /* Check that the destination bounding box intersects the source bounding box */
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
         psDP->nRasterXSize *
         psDP->adfGeoTransform[GEOTRSFRM_WE_RES] < minX )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] > maxX )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
         psDP->nRasterYSize *
         psDP->adfGeoTransform[GEOTRSFRM_NS_RES] > maxY )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] < minY )
         return FALSE;

    *pdfSrcXSize = psDP->nRasterXSize;
    *pdfSrcYSize = psDP->nRasterYSize;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] < minX )
    {
        *pdfSrcXOff = (minX - psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X]) /
            psDP->adfGeoTransform[GEOTRSFRM_WE_RES];
        *pdfDstXOff = 0.0;
    }
    else
    {
        *pdfSrcXOff = 0.0;
        *pdfDstXOff =
            ((psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] - minX) / we_res);
    }
    if ( maxY < psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y])
    {
        *pdfSrcYOff = (psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] - maxY) /
            -psDP->adfGeoTransform[GEOTRSFRM_NS_RES];
        *pdfDstYOff = 0.0;
    }
    else
    {
        *pdfSrcYOff = 0.0;
        *pdfDstYOff =
            ((maxY - psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y]) / -ns_res);
    }
    *pdfDstXSize = (psDP->nRasterXSize *
         psDP->adfGeoTransform[GEOTRSFRM_WE_RES] / we_res);
    *pdfDstYSize = (psDP->nRasterYSize *
         psDP->adfGeoTransform[GEOTRSFRM_NS_RES] / ns_res);

    return TRUE;
}

/************************************************************************/
/*                            VRTBuilder                                */
/************************************************************************/

class VRTBuilder
{
    /* Input parameters */
    char               *pszOutputFilename;
    int                 nInputFiles;
    char              **ppszInputFilenames;
    GDALDatasetH       *pahSrcDS;
    int                 nBands;
    int                *panBandList;
    int                 nMaxBandNo;
    ResolutionStrategy  resolutionStrategy;
    double              we_res;
    double              ns_res;
    int                 bTargetAlignedPixels;
    double              minX;
    double              minY;
    double              maxX;
    double              maxY;
    int                 bSeparate;
    int                 bAllowProjectionDifference;
    int                 bAddAlpha;
    int                 bHideNoData;
    int                 nSubdataset;
    char               *pszSrcNoData;
    char               *pszVRTNoData;
    char               *pszOutputSRS;
    char               *pszResampling;
    char              **papszOpenOptions;

    /* Internal variables */
    char               *pszProjectionRef;
    BandProperty       *pasBandProperties;
    int                 bFirst;
    int                 bHasGeoTransform;
    int                 nRasterXSize;
    int                 nRasterYSize;
    DatasetProperty    *pasDatasetProperties;
    int                 bUserExtent;
    int                 bAllowSrcNoData;
    double             *padfSrcNoData;
    int                 nSrcNoDataCount;
    int                 bAllowVRTNoData;
    double             *padfVRTNoData;
    int                 nVRTNoDataCount;
    int                 bHasRunBuild;
    int                 bHasDatasetMask;

    int         AnalyseRaster(GDALDatasetH hDS,
                              DatasetProperty* psDatasetProperties);

    void        CreateVRTSeparate(VRTDatasetH hVRTDS);
    void        CreateVRTNonSeparate(VRTDatasetH hVRTDS);

    public:
                VRTBuilder(const char* pszOutputFilename,
                           int nInputFiles,
                           const char* const * ppszInputFilenames,
                           GDALDatasetH *pahSrcDSIn,
                           const int *panBandListIn, int nBandCount, int nMaxBandNo,
                           ResolutionStrategy resolutionStrategy,
                           double we_res, double ns_res,
                           int bTargetAlignedPixels,
                           double minX, double minY, double maxX, double maxY,
                           int bSeparate, int bAllowProjectionDifference,
                           int bAddAlpha, int bHideNoData, int nSubdataset,
                           const char* pszSrcNoData, const char* pszVRTNoData,
                           const char* pszOutputSRS,
                           const char* pszResampling,
                           const char* const* papszOpenOptionsIn );

               ~VRTBuilder();

        GDALDataset*     Build(GDALProgressFunc pfnProgress, void * pProgressData);
};

/************************************************************************/
/*                          VRTBuilder()                                */
/************************************************************************/

VRTBuilder::VRTBuilder(const char* pszOutputFilenameIn,
                       int nInputFilesIn,
                       const char* const * ppszInputFilenamesIn,
                       GDALDatasetH *pahSrcDSIn,
                       const int *panBandListIn, int nBandCount, int nMaxBandNoIn,
                       ResolutionStrategy resolutionStrategyIn,
                       double we_resIn, double ns_resIn,
                       int bTargetAlignedPixelsIn,
                       double minXIn, double minYIn, double maxXIn, double maxYIn,
                       int bSeparateIn, int bAllowProjectionDifferenceIn,
                       int bAddAlphaIn, int bHideNoDataIn, int nSubdatasetIn,
                       const char* pszSrcNoDataIn, const char* pszVRTNoDataIn,
                       const char* pszOutputSRSIn,
                       const char* pszResamplingIn,
                       const char* const * papszOpenOptionsIn )
{
    pszOutputFilename = CPLStrdup(pszOutputFilenameIn);
    nInputFiles = nInputFilesIn;
    pahSrcDS = NULL;
    ppszInputFilenames = NULL;
    papszOpenOptions = CSLDuplicate(const_cast<char**>(papszOpenOptionsIn));

    if( ppszInputFilenamesIn )
    {
        ppszInputFilenames =
           static_cast<char **>(CPLMalloc(nInputFiles * sizeof(char*)));
        for(int i=0;i<nInputFiles;i++)
        {
            ppszInputFilenames[i] = CPLStrdup(ppszInputFilenamesIn[i]);
        }
    }
    else if( pahSrcDSIn )
    {
        pahSrcDS = static_cast<GDALDatasetH *>(
            CPLMalloc(nInputFiles * sizeof(GDALDatasetH)));
        memcpy(pahSrcDS, pahSrcDSIn, nInputFiles * sizeof(GDALDatasetH));
        ppszInputFilenames =
            static_cast<char **>(CPLMalloc(nInputFiles * sizeof(char*)));
        for(int i=0;i<nInputFiles;i++)
        {
            ppszInputFilenames[i] = CPLStrdup(GDALGetDescription(pahSrcDSIn[i]));
        }
    }

    nBands = nBandCount;
    panBandList = NULL;
    if( nBandCount )
    {
        panBandList = static_cast<int *>(CPLMalloc(nBands * sizeof(int)));
        memcpy(panBandList, panBandListIn, nBands * sizeof(int));
    }
    nMaxBandNo = nMaxBandNoIn;

    resolutionStrategy = resolutionStrategyIn;
    we_res = we_resIn;
    ns_res = ns_resIn;
    bTargetAlignedPixels = bTargetAlignedPixelsIn;
    minX = minXIn;
    minY = minYIn;
    maxX = maxXIn;
    maxY = maxYIn;
    bSeparate = bSeparateIn;
    bAllowProjectionDifference = bAllowProjectionDifferenceIn;
    bAddAlpha = bAddAlphaIn;
    bHideNoData = bHideNoDataIn;
    nSubdataset = nSubdatasetIn;
    pszSrcNoData = (pszSrcNoDataIn) ? CPLStrdup(pszSrcNoDataIn) : NULL;
    pszVRTNoData = (pszVRTNoDataIn) ? CPLStrdup(pszVRTNoDataIn) : NULL;
    pszOutputSRS = (pszOutputSRSIn) ? CPLStrdup(pszOutputSRSIn) : NULL;
    pszResampling = (pszResamplingIn) ? CPLStrdup(pszResamplingIn) : NULL;

    bUserExtent = FALSE;
    pszProjectionRef = NULL;
    pasBandProperties = NULL;
    bFirst = TRUE;
    bHasGeoTransform = FALSE;
    nRasterXSize = 0;
    nRasterYSize = 0;
    pasDatasetProperties = NULL;
    bAllowSrcNoData = TRUE;
    padfSrcNoData = NULL;
    nSrcNoDataCount = 0;
    bAllowVRTNoData = TRUE;
    padfVRTNoData = NULL;
    nVRTNoDataCount = 0;
    bHasRunBuild = FALSE;
    bHasDatasetMask = FALSE;
}

/************************************************************************/
/*                         ~VRTBuilder()                                */
/************************************************************************/

VRTBuilder::~VRTBuilder()
{
    CPLFree(pszOutputFilename);
    CPLFree(pszSrcNoData);
    CPLFree(pszVRTNoData);
    CPLFree(panBandList);

    for(int i=0;i<nInputFiles;i++)
    {
        CPLFree(ppszInputFilenames[i]);
    }
    CPLFree(ppszInputFilenames);
    CPLFree(pahSrcDS);

    if (pasDatasetProperties != NULL)
    {
        for(int i=0;i<nInputFiles;i++)
        {
            CPLFree(pasDatasetProperties[i].padfNoDataValues);
            CPLFree(pasDatasetProperties[i].panHasNoData);
        }
    }
    CPLFree(pasDatasetProperties);

    if (!bSeparate && pasBandProperties != NULL)
    {
        for(int j=0;j<nBands;j++)
        {
            GDALDestroyColorTable(pasBandProperties[j].colorTable);
        }
    }
    CPLFree(pasBandProperties);

    CPLFree(pszProjectionRef);
    CPLFree(padfSrcNoData);
    CPLFree(padfVRTNoData);
    CPLFree(pszOutputSRS);
    CPLFree(pszResampling);
    CSLDestroy(papszOpenOptions);
}

/************************************************************************/
/*                           ProjAreEqual()                             */
/************************************************************************/

static int ProjAreEqual(const char* pszWKT1, const char* pszWKT2)
{
    if (EQUAL(pszWKT1, pszWKT2))
        return TRUE;

    OGRSpatialReferenceH hSRS1 = OSRNewSpatialReference(pszWKT1);
    OGRSpatialReferenceH hSRS2 = OSRNewSpatialReference(pszWKT2);
    int bRet = hSRS1 != NULL && hSRS2 != NULL && OSRIsSame(hSRS1,hSRS2);
    if (hSRS1)
        OSRDestroySpatialReference(hSRS1);
    if (hSRS2)
        OSRDestroySpatialReference(hSRS2);
    return bRet;
}

/************************************************************************/
/*                           AnalyseRaster()                            */
/************************************************************************/

int VRTBuilder::AnalyseRaster( GDALDatasetH hDS, DatasetProperty* psDatasetProperties)
{
    const char* dsFileName = GDALGetDescription(hDS);
    char** papszMetadata = GDALGetMetadata( hDS, "SUBDATASETS" );
    if( CSLCount(papszMetadata) > 0 && GDALGetRasterCount(hDS) == 0 )
    {
        pasDatasetProperties = static_cast<DatasetProperty *>(
            CPLRealloc(pasDatasetProperties,
                       (nInputFiles + CSLCount(papszMetadata)) *
                       sizeof(DatasetProperty)));

        ppszInputFilenames = static_cast<char **>(
            CPLRealloc(ppszInputFilenames,
                       sizeof(char*) * (nInputFiles + CSLCount(papszMetadata))));
        if ( nSubdataset < 0 )
        {
            int count = 1;
            char subdatasetNameKey[80];
            snprintf(subdatasetNameKey, sizeof(subdatasetNameKey), "SUBDATASET_%d_NAME", count);
            while(*papszMetadata != NULL)
            {
                if (EQUALN(*papszMetadata, subdatasetNameKey, strlen(subdatasetNameKey)))
                {
                    memset(&pasDatasetProperties[nInputFiles], 0, sizeof(DatasetProperty));
                    ppszInputFilenames[nInputFiles++] =
                            CPLStrdup(*papszMetadata+strlen(subdatasetNameKey)+1);
                    count++;
                    snprintf(subdatasetNameKey, sizeof(subdatasetNameKey), "SUBDATASET_%d_NAME", count);
                }
                papszMetadata++;
            }
        }
        else
        {
            char        subdatasetNameKey[80];
            const char  *pszSubdatasetName;

            snprintf( subdatasetNameKey, sizeof(subdatasetNameKey),
                      "SUBDATASET_%d_NAME", nSubdataset );
            pszSubdatasetName = CSLFetchNameValue( papszMetadata, subdatasetNameKey );
            if ( pszSubdatasetName )
            {
                memset( &pasDatasetProperties[nInputFiles], 0, sizeof(DatasetProperty) );
                ppszInputFilenames[nInputFiles++] = CPLStrdup( pszSubdatasetName );
            }
        }
        return FALSE;
    }

    const char* proj = GDALGetProjectionRef(hDS);
    double* padfGeoTransform = psDatasetProperties->adfGeoTransform;
    int bGotGeoTransform = GDALGetGeoTransform(hDS, padfGeoTransform) == CE_None;
    if (bSeparate)
    {
        if (bFirst)
        {
            bHasGeoTransform = bGotGeoTransform;
            if (!bHasGeoTransform)
            {
                if (bUserExtent)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                        "User extent ignored by gdalbuildvrt -separate with ungeoreferenced images.");
                }
                if (resolutionStrategy == USER_RESOLUTION)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                        "User resolution ignored by gdalbuildvrt -separate with ungeoreferenced images.");
                }
            }
        }
        else if (bHasGeoTransform != bGotGeoTransform)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "gdalbuildvrt -separate cannot stack ungeoreferenced and georeferenced images. Skipping %s",
                    dsFileName);
            return FALSE;
        }
        else if (!bHasGeoTransform &&
                    (nRasterXSize != GDALGetRasterXSize(hDS) ||
                    nRasterYSize != GDALGetRasterYSize(hDS)))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "gdalbuildvrt -separate cannot stack ungeoreferenced images that have not the same dimensions. Skipping %s",
                    dsFileName);
            return FALSE;
        }
    }
    else
    {
        if (!bGotGeoTransform)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "gdalbuildvrt does not support ungeoreferenced image. Skipping %s",
                    dsFileName);
            return FALSE;
        }
        bHasGeoTransform = TRUE;
    }

    if (bGotGeoTransform)
    {
        if (padfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] != 0 ||
            padfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] != 0)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "gdalbuildvrt does not support rotated geo transforms. Skipping %s",
                    dsFileName);
            return FALSE;
        }
        if (padfGeoTransform[GEOTRSFRM_NS_RES] >= 0)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "gdalbuildvrt does not support positive NS resolution. Skipping %s",
                    dsFileName);
            return FALSE;
        }
    }

    psDatasetProperties->nRasterXSize = GDALGetRasterXSize(hDS);
    psDatasetProperties->nRasterYSize = GDALGetRasterYSize(hDS);
    if (bFirst && bSeparate && !bGotGeoTransform)
    {
        nRasterXSize = GDALGetRasterXSize(hDS);
        nRasterYSize = GDALGetRasterYSize(hDS);
    }

    double ds_minX = padfGeoTransform[GEOTRSFRM_TOPLEFT_X];
    double ds_maxY = padfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
    double ds_maxX = ds_minX +
                GDALGetRasterXSize(hDS) *
                padfGeoTransform[GEOTRSFRM_WE_RES];
    double ds_minY = ds_maxY +
                GDALGetRasterYSize(hDS) *
                padfGeoTransform[GEOTRSFRM_NS_RES];

    GDALGetBlockSize(GDALGetRasterBand( hDS, 1 ),
                        &psDatasetProperties->nBlockXSize,
                        &psDatasetProperties->nBlockYSize);

    int _nBands = GDALGetRasterCount(hDS);

    //if provided band list
    if(nBands != 0 && _nBands != 0 && nMaxBandNo != 0 && _nBands >= nMaxBandNo)
    {
        if(_nBands < nMaxBandNo)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Skipping %s as it has no such bands", dsFileName);
            return FALSE;
        }
        else
        {
            _nBands = nMaxBandNo;
        }
    }

    if (_nBands == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "Skipping %s as it has no bands", dsFileName);
        return FALSE;
    }
    else if (_nBands > 1 && bSeparate)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s has %d bands. Only the first one will "
                    "be taken into account in the -separate case",
                    dsFileName, _nBands);
        _nBands = 1;
    }

    /* For the -separate case */
    psDatasetProperties->firstBandType = GDALGetRasterDataType(GDALGetRasterBand(hDS, 1));

    psDatasetProperties->padfNoDataValues =
        static_cast<double *>(CPLCalloc(sizeof(double), _nBands));
    psDatasetProperties->panHasNoData =
        static_cast<int *>(CPLCalloc(sizeof(int), _nBands));

    psDatasetProperties->bHasDatasetMask = GDALGetMaskFlags(GDALGetRasterBand(hDS, 1)) == GMF_PER_DATASET;
    if (psDatasetProperties->bHasDatasetMask)
        bHasDatasetMask = TRUE;
    GDALGetBlockSize(GDALGetMaskBand(GDALGetRasterBand( hDS, 1 )),
                        &psDatasetProperties->nMaskBlockXSize,
                        &psDatasetProperties->nMaskBlockYSize);

    int j;
    for(j=0;j<_nBands;j++)
    {
        if (nSrcNoDataCount > 0)
        {
            psDatasetProperties->panHasNoData[j] = TRUE;
            if (j < nSrcNoDataCount)
                psDatasetProperties->padfNoDataValues[j] = padfSrcNoData[j];
            else
                psDatasetProperties->padfNoDataValues[j] = padfSrcNoData[nSrcNoDataCount - 1];
        }
        else
        {
            psDatasetProperties->padfNoDataValues[j]  =
                GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, j+1),
                                        &psDatasetProperties->panHasNoData[j]);
        }
    }

    if (bFirst)
    {
        if (proj)
            pszProjectionRef = CPLStrdup(proj);
        if (!bUserExtent)
        {
            minX = ds_minX;
            minY = ds_minY;
            maxX = ds_maxX;
            maxY = ds_maxY;
        }

        //if not provided an explicit band list, take the one of the first dataset
        if(nBands == 0)
        {
            nBands = _nBands;
            CPLFree(panBandList);
            panBandList = static_cast<int *>(CPLMalloc(nBands * sizeof(int)));
            for(j=0;j<nBands;j++)
            {
                panBandList[j] = j + 1;
                if(nMaxBandNo < j + 1)
                    nMaxBandNo = j + 1;
            }
        }
        if (!bSeparate)
        {
            pasBandProperties = static_cast<BandProperty *>(
                CPLMalloc(nMaxBandNo * sizeof(BandProperty)));
            for(j=0;j<nMaxBandNo;j++)
            {
                GDALRasterBandH hRasterBand = GDALGetRasterBand( hDS, j+1 );
                pasBandProperties[j].colorInterpretation =
                        GDALGetRasterColorInterpretation(hRasterBand);
                pasBandProperties[j].dataType = GDALGetRasterDataType(hRasterBand);
                if (pasBandProperties[j].colorInterpretation == GCI_PaletteIndex)
                {
                    pasBandProperties[j].colorTable =
                            GDALGetRasterColorTable( hRasterBand );
                    if (pasBandProperties[j].colorTable)
                    {
                        pasBandProperties[j].colorTable =
                                GDALCloneColorTable(pasBandProperties[j].colorTable);
                    }
                }
                else
                    pasBandProperties[j].colorTable = NULL;

                if (nVRTNoDataCount > 0)
                {
                    pasBandProperties[j].bHasNoData = TRUE;
                    if (j < nVRTNoDataCount)
                        pasBandProperties[j].noDataValue = padfVRTNoData[j];
                    else
                        pasBandProperties[j].noDataValue = padfVRTNoData[nVRTNoDataCount - 1];
                }
                else
                {
                    pasBandProperties[j].noDataValue =
                            GDALGetRasterNoDataValue(hRasterBand, &pasBandProperties[j].bHasNoData);
                }
            }
        }
    }
    else
    {
        if ((proj != NULL && pszProjectionRef == NULL) ||
            (proj == NULL && pszProjectionRef != NULL) ||
            (proj != NULL && pszProjectionRef != NULL && ProjAreEqual(proj, pszProjectionRef) == FALSE))
        {
            if (!bAllowProjectionDifference)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt does not support heterogeneous projection. Skipping %s",
                            dsFileName);
                return FALSE;
            }
        }
        if (!bSeparate)
        {
            if (nMaxBandNo > _nBands)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt does not support heterogeneous band numbers. Skipping %s",
                        dsFileName);
                return FALSE;
            }
            for(j=0;j<nMaxBandNo;j++)
            {
                GDALRasterBandH hRasterBand = GDALGetRasterBand( hDS, j+1 );
                if (pasBandProperties[j].colorInterpretation != GDALGetRasterColorInterpretation(hRasterBand) ||
                    pasBandProperties[j].dataType != GDALGetRasterDataType(hRasterBand))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                                "gdalbuildvrt does not support heterogeneous band characteristics. Skipping %s",
                                dsFileName);
                    return FALSE;
                }
                if (pasBandProperties[j].colorTable)
                {
                    GDALColorTableH colorTable = GDALGetRasterColorTable( hRasterBand );
                    int nRefColorEntryCount = GDALGetColorEntryCount(pasBandProperties[j].colorTable);
                    if (colorTable == NULL ||
                        GDALGetColorEntryCount(colorTable) != nRefColorEntryCount)
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                    "gdalbuildvrt does not support rasters with different color tables (different number of color table entries). Skipping %s",
                                dsFileName);
                        return FALSE;
                    }

                    /* Check that the palette are the same too */
                    /* We just warn and still process the file. It is not a technical no-go, but the user */
                    /* should check that the end result is OK for him. */
                    for(int i=0;i<nRefColorEntryCount;i++)
                    {
                        const GDALColorEntry* psEntry = GDALGetColorEntry(colorTable, i);
                        const GDALColorEntry* psEntryRef = GDALGetColorEntry(pasBandProperties[j].colorTable, i);
                        if (psEntry->c1 != psEntryRef->c1 || psEntry->c2 != psEntryRef->c2 ||
                            psEntry->c3 != psEntryRef->c3 || psEntry->c4 != psEntryRef->c4)
                        {
                            static int bFirstWarningPCT = TRUE;
                            if (bFirstWarningPCT)
                                CPLError(CE_Warning, CPLE_NotSupported,
                                        "%s has different values than the first raster for some entries in the color table.\n"
                                        "The end result might produce weird colors.\n"
                                        "You're advised to pre-process your rasters with other tools, such as pct2rgb.py or gdal_translate -expand RGB\n"
                                        "to operate gdalbuildvrt on RGB rasters instead", dsFileName);
                            else
                                CPLError(CE_Warning, CPLE_NotSupported,
                                            "%s has different values than the first raster for some entries in the color table.",
                                            dsFileName);
                            bFirstWarningPCT = FALSE;
                            break;
                        }
                    }
                }
            }
        }
        if (!bUserExtent)
        {
            if (ds_minX < minX) minX = ds_minX;
            if (ds_minY < minY) minY = ds_minY;
            if (ds_maxX > maxX) maxX = ds_maxX;
            if (ds_maxY > maxY) maxY = ds_maxY;
        }
    }

    if (resolutionStrategy == AVERAGE_RESOLUTION)
    {
        we_res += padfGeoTransform[GEOTRSFRM_WE_RES];
        ns_res += padfGeoTransform[GEOTRSFRM_NS_RES];
    }
    else if (resolutionStrategy != USER_RESOLUTION)
    {
        if (bFirst)
        {
            we_res = padfGeoTransform[GEOTRSFRM_WE_RES];
            ns_res = padfGeoTransform[GEOTRSFRM_NS_RES];
        }
        else if (resolutionStrategy == HIGHEST_RESOLUTION)
        {
            we_res = std::min(we_res, padfGeoTransform[GEOTRSFRM_WE_RES]);
            //ns_res is negative, the highest resolution is the max value.
            ns_res = std::max(ns_res, padfGeoTransform[GEOTRSFRM_NS_RES]);
        }
        else
        {
            we_res = std::max(we_res, padfGeoTransform[GEOTRSFRM_WE_RES]);
            // ns_res is negative, the lowest resolution is the min value.
            ns_res = std::min(ns_res, padfGeoTransform[GEOTRSFRM_NS_RES]);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                         CreateVRTSeparate()                          */
/************************************************************************/

void VRTBuilder::CreateVRTSeparate(VRTDatasetH hVRTDS)
{
    int iBand = 1;
    for(int i=0;i<nInputFiles;i++)
    {
        DatasetProperty* psDatasetProperties = &pasDatasetProperties[i];

        if (psDatasetProperties->isFileOK == FALSE)
            continue;

        double dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize,
               dfDstXOff, dfDstYOff, dfDstXSize, dfDstYSize;
        if (bHasGeoTransform)
        {
            if ( ! GetSrcDstWin(psDatasetProperties,
                        we_res, ns_res, minX, minY, maxX, maxY,
                        &dfSrcXOff, &dfSrcYOff, &dfSrcXSize, &dfSrcYSize,
                        &dfDstXOff, &dfDstYOff, &dfDstXSize, &dfDstYSize) )
                continue;
        }
        else
        {
            dfSrcXOff = dfSrcYOff = dfDstXOff = dfDstYOff = 0;
            dfSrcXSize = dfDstXSize = nRasterXSize;
            dfSrcYSize = dfDstYSize = nRasterYSize;
        }

        const char* dsFileName = ppszInputFilenames[i];

        GDALAddBand(hVRTDS, psDatasetProperties->firstBandType, NULL);

        GDALProxyPoolDatasetH hProxyDS =
            GDALProxyPoolDatasetCreate(dsFileName,
                                        psDatasetProperties->nRasterXSize,
                                        psDatasetProperties->nRasterYSize,
                                        GA_ReadOnly, TRUE, pszProjectionRef,
                                        psDatasetProperties->adfGeoTransform);
        reinterpret_cast<GDALProxyPoolDataset*>(hProxyDS)->
                                        SetOpenOptions( papszOpenOptions );

        GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                            psDatasetProperties->firstBandType,
                                            psDatasetProperties->nBlockXSize,
                                            psDatasetProperties->nBlockYSize);

        VRTSourcedRasterBandH hVRTBand =
                (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, iBand);

        if (bHideNoData)
            GDALSetMetadataItem(hVRTBand,"HideNoDataValue","1",NULL);

        VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand*)hVRTBand;

        VRTSimpleSource* poSimpleSource;
        if (bAllowSrcNoData && psDatasetProperties->panHasNoData[0])
        {
            GDALSetRasterNoDataValue(hVRTBand, psDatasetProperties->padfNoDataValues[0]);
            poSimpleSource = new VRTComplexSource();
            poSimpleSource->SetNoDataValue( psDatasetProperties->padfNoDataValues[0] );
        }
        else
            poSimpleSource = new VRTSimpleSource();
        if( pszResampling )
            poSimpleSource->SetResampling(pszResampling);
        poVRTBand->ConfigureSource( poSimpleSource,
                                    (GDALRasterBand*)GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                    FALSE,
                                    dfSrcXOff, dfSrcYOff,
                                    dfSrcXSize, dfSrcYSize,
                                    dfDstXOff, dfDstYOff,
                                    dfDstXSize, dfDstYSize );

        poVRTBand->AddSource( poSimpleSource );

        GDALDereferenceDataset(hProxyDS);

        iBand ++;
    }
}

/************************************************************************/
/*                       CreateVRTNonSeparate()                         */
/************************************************************************/

void VRTBuilder::CreateVRTNonSeparate(VRTDatasetH hVRTDS)
{
    for(int j=0;j<nBands;j++)
    {
        GDALRasterBandH hBand;
        int nSelBand = panBandList[j]-1;
        GDALAddBand(hVRTDS, pasBandProperties[nSelBand].dataType, NULL);
        hBand = GDALGetRasterBand(hVRTDS, j+1);
        GDALSetRasterColorInterpretation(hBand, pasBandProperties[nSelBand].colorInterpretation);
        if (pasBandProperties[nSelBand].colorInterpretation == GCI_PaletteIndex)
        {
            GDALSetRasterColorTable(hBand, pasBandProperties[nSelBand].colorTable);
        }
        if (bAllowVRTNoData && pasBandProperties[nSelBand].bHasNoData)
            GDALSetRasterNoDataValue(hBand, pasBandProperties[nSelBand].noDataValue);
        if ( bHideNoData )
            GDALSetMetadataItem(hBand,"HideNoDataValue","1",NULL);
    }

    VRTSourcedRasterBand* poMaskVRTBand = NULL;
    if (bAddAlpha)
    {
        GDALRasterBandH hBand;
        GDALAddBand(hVRTDS, GDT_Byte, NULL);
        hBand = GDALGetRasterBand(hVRTDS, nBands + 1);
        GDALSetRasterColorInterpretation(hBand, GCI_AlphaBand);
    }
    else if (bHasDatasetMask)
    {
        GDALCreateDatasetMaskBand(hVRTDS, GMF_PER_DATASET);
        poMaskVRTBand = (VRTSourcedRasterBand*)GDALGetMaskBand(GDALGetRasterBand(hVRTDS, 1));
    }

    for( int i = 0; i < nInputFiles; i++ )
    {
        DatasetProperty* psDatasetProperties = &pasDatasetProperties[i];

        if (psDatasetProperties->isFileOK == FALSE)
            continue;

        double dfSrcXOff;
        double dfSrcYOff;
        double dfSrcXSize;
        double dfSrcYSize;
        double dfDstXOff;
        double dfDstYOff;
        double dfDstXSize;
        double dfDstYSize;
        if ( ! GetSrcDstWin(psDatasetProperties,
                        we_res, ns_res, minX, minY, maxX, maxY,
                        &dfSrcXOff, &dfSrcYOff, &dfSrcXSize, &dfSrcYSize,
                        &dfDstXOff, &dfDstYOff, &dfDstXSize, &dfDstYSize) )
            continue;

        const char* dsFileName = ppszInputFilenames[i];

        GDALProxyPoolDatasetH hProxyDS =
            GDALProxyPoolDatasetCreate(dsFileName,
                                        psDatasetProperties->nRasterXSize,
                                        psDatasetProperties->nRasterYSize,
                                        GA_ReadOnly, TRUE, pszProjectionRef,
                                        psDatasetProperties->adfGeoTransform);
        reinterpret_cast<GDALProxyPoolDataset*>(hProxyDS)->
                                        SetOpenOptions( papszOpenOptions );

        for(int j=0;j<nMaxBandNo;j++)
        {
            GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                            pasBandProperties[j].dataType,
                                            psDatasetProperties->nBlockXSize,
                                            psDatasetProperties->nBlockYSize);
        }
        if (bHasDatasetMask && !bAddAlpha)
        {
            ((GDALProxyPoolRasterBand*)((GDALProxyPoolDataset*)hProxyDS)->GetRasterBand(1))->
                    AddSrcMaskBandDescription  (GDT_Byte,
                                                psDatasetProperties->nMaskBlockXSize,
                                                psDatasetProperties->nMaskBlockYSize);
        }

        for(int j=0;j<nBands;j++)
        {
            VRTSourcedRasterBandH hVRTBand =
                    (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, j + 1);

            /* Place the raster band at the right position in the VRT */
            int nSelBand = panBandList[j] - 1;
            VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand*)hVRTBand;

            VRTSimpleSource* poSimpleSource;
            if (bAllowSrcNoData && psDatasetProperties->panHasNoData[nSelBand])
            {
                poSimpleSource = new VRTComplexSource();
                poSimpleSource->SetNoDataValue( psDatasetProperties->padfNoDataValues[nSelBand] );
            }
            else
                poSimpleSource = new VRTSimpleSource();
            if( pszResampling )
                poSimpleSource->SetResampling(pszResampling);
            poVRTBand->ConfigureSource( poSimpleSource,
                                        (GDALRasterBand*)GDALGetRasterBand((GDALDatasetH)hProxyDS, nSelBand + 1),
                                        FALSE,
                                        dfSrcXOff, dfSrcYOff,
                                        dfSrcXSize, dfSrcYSize,
                                        dfDstXOff, dfDstYOff,
                                        dfDstXSize, dfDstYSize );

            poVRTBand->AddSource( poSimpleSource );
        }

        if (bAddAlpha)
        {
            VRTSourcedRasterBandH hVRTBand =
                    (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, nBands + 1);
            /* Little trick : we use an offset of 255 and a scaling of 0, so that in areas covered */
            /* by the source, the value of the alpha band will be 255, otherwise it will be 0 */
            ((VRTSourcedRasterBand *) hVRTBand)->AddComplexSource(
                                            (GDALRasterBand*)GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                            dfSrcXOff, dfSrcYOff,
                                            dfSrcXSize, dfSrcYSize,
                                            dfDstXOff, dfDstYOff,
                                            dfDstXSize, dfDstYSize,
                                            255, 0, VRT_NODATA_UNSET);
        }
        else if (bHasDatasetMask)
        {
            VRTSimpleSource* poSimpleSource = new VRTSimpleSource();
            if( pszResampling )
                poSimpleSource->SetResampling(pszResampling);
            poMaskVRTBand->ConfigureSource( poSimpleSource,
                                            (GDALRasterBand*)GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                            TRUE,
                                            dfSrcXOff, dfSrcYOff,
                                            dfSrcXSize, dfSrcYSize,
                                            dfDstXOff, dfDstYOff,
                                            dfDstXSize, dfDstYSize );

            poMaskVRTBand->AddSource( poSimpleSource );
        }

        GDALDereferenceDataset(hProxyDS);
    }
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALDataset* VRTBuilder::Build(GDALProgressFunc pfnProgress, void * pProgressData)
{
    if (bHasRunBuild)
        return NULL;
    bHasRunBuild = TRUE;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    bUserExtent = (minX != 0 || minY != 0 || maxX != 0 || maxY != 0);
    if (bUserExtent)
    {
        if (minX >= maxX || minY >= maxY )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user extent");
            return NULL;
        }
    }

    if (resolutionStrategy == USER_RESOLUTION)
    {
        if (we_res <= 0 || ns_res <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user resolution");
            return NULL;
        }

        /* We work with negative north-south resolution in all the following code */
        ns_res = -ns_res;
    }
    else
    {
        we_res = ns_res = 0;
    }

    pasDatasetProperties = static_cast<DatasetProperty *>(
        CPLCalloc(nInputFiles, sizeof(DatasetProperty)));

    if (pszSrcNoData != NULL)
    {
        if (EQUAL(pszSrcNoData, "none"))
        {
            bAllowSrcNoData = FALSE;
        }
        else
        {
            char **papszTokens = CSLTokenizeString( pszSrcNoData );
            nSrcNoDataCount = CSLCount(papszTokens);
            padfSrcNoData = static_cast<double *>(
                CPLMalloc(sizeof(double) * nSrcNoDataCount));
            for(int i=0;i<nSrcNoDataCount;i++)
            {
                if( !ArgIsNumeric(papszTokens[i]) &&
                    !EQUAL(papszTokens[i], "nan") &&
                    !EQUAL(papszTokens[i], "-inf") &&
                    !EQUAL(papszTokens[i], "inf") )
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "Invalid -srcnodata value");
                    CSLDestroy(papszTokens);
                    return NULL;
                }
                padfSrcNoData[i] = CPLAtofM(papszTokens[i]);
            }
            CSLDestroy(papszTokens);
        }
    }

    if (pszVRTNoData != NULL)
    {
        if (EQUAL(pszVRTNoData, "none"))
        {
            bAllowVRTNoData = FALSE;
        }
        else
        {
            char **papszTokens = CSLTokenizeString( pszVRTNoData );
            nVRTNoDataCount = CSLCount(papszTokens);
            padfVRTNoData = static_cast<double *>(
                CPLMalloc(sizeof(double) * nVRTNoDataCount));
            for(int i=0;i<nVRTNoDataCount;i++)
            {
                if( !ArgIsNumeric(papszTokens[i]) &&
                    !EQUAL(papszTokens[i], "nan") &&
                    !EQUAL(papszTokens[i], "-inf") &&
                    !EQUAL(papszTokens[i], "inf") )
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "Invalid -vrtnodata value");
                    CSLDestroy(papszTokens);
                    return NULL;
                }
                padfVRTNoData[i] = CPLAtofM(papszTokens[i]);
            }
            CSLDestroy(papszTokens);
        }
    }

    int nCountValid = 0;
    for(int i=0;i<nInputFiles;i++)
    {
        const char* dsFileName = ppszInputFilenames[i];

        if (!pfnProgress( 1.0 * (i+1) / nInputFiles, NULL, pProgressData))
        {
            return NULL;
        }

        GDALDatasetH hDS =
            (pahSrcDS) ? pahSrcDS[i] :
                GDALOpenEx( ppszInputFilenames[i],
                            GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, NULL,
                            papszOpenOptions, NULL );
        pasDatasetProperties[i].isFileOK = FALSE;

        if (hDS)
        {
            if (AnalyseRaster( hDS, &pasDatasetProperties[i] ))
            {
                pasDatasetProperties[i].isFileOK = TRUE;
                nCountValid ++;
                bFirst = FALSE;
            }
            if( pahSrcDS == NULL )
                GDALClose(hDS);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Can't open %s. Skipping it", dsFileName);
        }
    }

    if (nCountValid == 0)
        return NULL;

    if (bHasGeoTransform)
    {
        if (resolutionStrategy == AVERAGE_RESOLUTION)
        {
            we_res /= nCountValid;
            ns_res /= nCountValid;
        }

        if ( bTargetAlignedPixels )
        {
            minX = floor(minX / we_res) * we_res;
            maxX = ceil(maxX / we_res) * we_res;
            minY = floor(minY / -ns_res) * -ns_res;
            maxY = ceil(maxY / -ns_res) * -ns_res;
        }

        nRasterXSize = static_cast<int>(0.5 + (maxX - minX) / we_res);
        nRasterYSize = static_cast<int>(0.5 + (maxY - minY) / -ns_res);
    }

    if (nRasterXSize == 0 || nRasterYSize == 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Computed VRT dimension is invalid. You've probably "
                  "specified inappropriate resolution.");
        return NULL;
    }

    VRTDatasetH hVRTDS = VRTCreate(nRasterXSize, nRasterYSize);
    GDALSetDescription(hVRTDS, pszOutputFilename);

    if( pszOutputSRS )
    {
        GDALSetProjection(hVRTDS, pszOutputSRS);
    }
    else if (pszProjectionRef)
    {
        GDALSetProjection(hVRTDS, pszProjectionRef);
    }

    if (bHasGeoTransform)
    {
        double adfGeoTransform[6];
        adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = minX;
        adfGeoTransform[GEOTRSFRM_WE_RES] = we_res;
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0;
        adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = maxY;
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0;
        adfGeoTransform[GEOTRSFRM_NS_RES] = ns_res;
        GDALSetGeoTransform(hVRTDS, adfGeoTransform);
    }

    if (bSeparate)
    {
        CreateVRTSeparate(hVRTDS);
    }
    else
    {
        CreateVRTNonSeparate(hVRTDS);
    }

    return (GDALDataset*)hVRTDS;
}

/************************************************************************/
/*                        add_file_to_list()                            */
/************************************************************************/

static bool add_file_to_list(const char* filename, const char* tile_index,
                             int* pnInputFiles, char*** pppszInputFilenames)
{

    int nInputFiles = *pnInputFiles;
    char** ppszInputFilenames = *pppszInputFilenames;

    if (EQUAL(CPLGetExtension(filename), "SHP"))
    {
        OGRRegisterAll();

        /* Handle gdaltindex Shapefile as a special case */
        OGRDataSourceH hDS = OGROpen( filename, FALSE, NULL );
        if( hDS  == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to open shapefile `%s'.",
                      filename );
            return false;
        }

        OGRLayerH hLayer = OGR_DS_GetLayer(hDS, 0);

        OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);

        int ti_field;
        for( ti_field = 0; ti_field < OGR_FD_GetFieldCount(hFDefn); ti_field++ )
        {
            OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, ti_field );
            const char* pszName = OGR_Fld_GetNameRef(hFieldDefn);

            if (strcmp(pszName, "LOCATION") == 0 && strcmp("LOCATION", tile_index) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "This shapefile seems to be a tile index of "
                        "OGR features and not GDAL products.");
            }
            if( strcmp(pszName, tile_index) == 0 )
                break;
        }

        if( ti_field == OGR_FD_GetFieldCount(hFDefn) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to find field `%s' in DBF file `%s'.",
                     tile_index, filename );
            return false;
        }

        /* Load in memory existing file names in SHP */
        const int nTileIndexFiles =
            static_cast<int>(OGR_L_GetFeatureCount(hLayer, TRUE));
        if (nTileIndexFiles == 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Tile index %s is empty. Skipping it.\n", filename);
            return true;
        }

        ppszInputFilenames = static_cast<char **>(
            CPLRealloc(ppszInputFilenames,
                       sizeof(char*) * (nInputFiles+nTileIndexFiles + 1)));
        for(int j=0;j<nTileIndexFiles;j++)
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hLayer);
            ppszInputFilenames[nInputFiles++] =
                    CPLStrdup(OGR_F_GetFieldAsString(hFeat, ti_field ));
            OGR_F_Destroy(hFeat);
        }
        ppszInputFilenames[nInputFiles] = NULL;

        OGR_DS_Destroy( hDS );
    }
    else
    {
        ppszInputFilenames = static_cast<char **>(
            CPLRealloc(ppszInputFilenames,
                       sizeof(char*) * (nInputFiles + 1 + 1)));
        ppszInputFilenames[nInputFiles++] = CPLStrdup(filename);
        ppszInputFilenames[nInputFiles] = NULL;
    }

    *pnInputFiles = nInputFiles;
    *pppszInputFilenames = ppszInputFilenames;
    return true;
}

/************************************************************************/
/*                        GDALBuildVRTOptions                           */
/************************************************************************/

/** Options for use with GDALBuildVRT(). GDALBuildVRTOptions* must be allocated and
 * freed with GDALBuildVRTOptionsNew() and GDALBuildVRTOptionsFree() respectively.
 */
struct GDALBuildVRTOptions
{
    char *pszResolution;
    int bSeparate;
    int bAllowProjectionDifference;
    double we_res;
    double ns_res;
    int bTargetAlignedPixels;
    double xmin;
    double ymin;
    double xmax;
    double ymax;
    int bAddAlpha;
    int bHideNoData;
    int nSubdataset;
    char* pszSrcNoData;
    char* pszVRTNoData;
    char* pszOutputSRS;
    int *panBandList;
    int nBandCount;
    int nMaxBandNo;
    char* pszResampling;
    char** papszOpenOptions;

    /*! allow or suppress progress monitor and other non-error output */
    int bQuiet;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;
};

/************************************************************************/
/*                        GDALBuildVRTOptionsClone()                   */
/************************************************************************/

static
GDALBuildVRTOptions* GDALBuildVRTOptionsClone(const GDALBuildVRTOptions *psOptionsIn)
{
    GDALBuildVRTOptions* psOptions = static_cast<GDALBuildVRTOptions*>(
        CPLMalloc(sizeof(GDALBuildVRTOptions)) );
    memcpy(psOptions, psOptionsIn, sizeof(GDALBuildVRTOptions));
    if( psOptionsIn->pszResolution ) psOptions->pszResolution = CPLStrdup(psOptionsIn->pszResolution);
    if( psOptionsIn->pszSrcNoData ) psOptions->pszSrcNoData = CPLStrdup(psOptionsIn->pszSrcNoData);
    if( psOptionsIn->pszVRTNoData ) psOptions->pszVRTNoData = CPLStrdup(psOptionsIn->pszVRTNoData);
    if( psOptionsIn->pszOutputSRS ) psOptions->pszOutputSRS = CPLStrdup(psOptionsIn->pszOutputSRS);
    if( psOptionsIn->pszResampling ) psOptions->pszResampling = CPLStrdup(psOptionsIn->pszResampling);
    if( psOptionsIn->panBandList )
    {
        psOptions->panBandList = static_cast<int*>(CPLMalloc(sizeof(int) * psOptionsIn->nBandCount));
        memcpy(psOptions->panBandList, psOptionsIn->panBandList, sizeof(int) * psOptionsIn->nBandCount);
    }
    if( psOptionsIn->papszOpenOptions ) psOptions->papszOpenOptions = CSLDuplicate(psOptionsIn->papszOpenOptions);
    return psOptions;
}

/************************************************************************/
/*                           GDALBuildVRT()                             */
/************************************************************************/

/**
 * Build a VRT from a list of datasets.
 *
 * This is the equivalent of the <a href="gdalbuildvrt.html">gdalbuildvrt</a> utility.
 *
 * GDALBuildVRTOptions* must be allocated and freed with GDALBuildVRTOptionsNew()
 * and GDALBuildVRTOptionsFree() respectively.
 * pahSrcDS and papszSrcDSNames cannot be used at the same time.
 *
 * @param pszDest the destination dataset path.
 * @param nSrcCount the number of input datasets.
 * @param pahSrcDS the list of input datasets (or NULL, exclusive with papszSrcDSNames)
 * @param papszSrcDSNames the list of input dataset names (or NULL, exclusive with pahSrcDS)
 * @param psOptionsIn the options struct returned by GDALBuildVRTOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALBuildVRT( const char *pszDest,
                           int nSrcCount, GDALDatasetH *pahSrcDS, const char* const* papszSrcDSNames,
                           const GDALBuildVRTOptions *psOptionsIn, int *pbUsageError )
{
    if( pszDest == NULL )
        pszDest = "";

    if( nSrcCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No input dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALBuildVRTOptions* psOptions =
        (psOptionsIn) ? GDALBuildVRTOptionsClone(psOptionsIn) :
                        GDALBuildVRTOptionsNew(NULL, NULL);

    if (psOptions->we_res != 0 && psOptions->ns_res != 0 &&
        psOptions->pszResolution != NULL && !EQUAL(psOptions->pszResolution, "user"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-tr option is not compatible with -resolution %s", psOptions->pszResolution);
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return NULL;
    }

    if (psOptions->bTargetAlignedPixels && psOptions->we_res == 0 && psOptions->ns_res == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-tap option cannot be used without using -tr");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return NULL;
    }

    if (psOptions->bAddAlpha && psOptions->bSeparate)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-addalpha option is not compatible with -separate.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return NULL;
    }

    ResolutionStrategy eStrategy = AVERAGE_RESOLUTION;
    if ( psOptions->pszResolution == NULL || EQUAL(psOptions->pszResolution, "user") )
    {
        if ( psOptions->we_res != 0 || psOptions->ns_res != 0)
            eStrategy = USER_RESOLUTION;
        else if ( psOptions->pszResolution != NULL && EQUAL(psOptions->pszResolution, "user") )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                      "-tr option must be used with -resolution user.");
            if( pbUsageError )
                *pbUsageError = TRUE;
            GDALBuildVRTOptionsFree(psOptions);
            return NULL;
        }
    }
    else if ( EQUAL(psOptions->pszResolution, "average") )
        eStrategy = AVERAGE_RESOLUTION;
    else if ( EQUAL(psOptions->pszResolution, "highest") )
        eStrategy = HIGHEST_RESOLUTION;
    else if ( EQUAL(psOptions->pszResolution, "lowest") )
        eStrategy = LOWEST_RESOLUTION;

    /* If -srcnodata is specified, use it as the -vrtnodata if the latter is not */
    /* specified */
    if (psOptions->pszSrcNoData != NULL && psOptions->pszVRTNoData == NULL)
        psOptions->pszVRTNoData = CPLStrdup(psOptions->pszSrcNoData);

    VRTBuilder oBuilder(pszDest, nSrcCount, papszSrcDSNames, pahSrcDS,
                        psOptions->panBandList, psOptions->nBandCount, psOptions->nMaxBandNo,
                        eStrategy, psOptions->we_res, psOptions->ns_res, psOptions->bTargetAlignedPixels,
                        psOptions->xmin, psOptions->ymin, psOptions->xmax, psOptions->ymax,
                        psOptions->bSeparate, psOptions->bAllowProjectionDifference,
                        psOptions->bAddAlpha, psOptions->bHideNoData, psOptions->nSubdataset,
                        psOptions->pszSrcNoData, psOptions->pszVRTNoData,
                        psOptions->pszOutputSRS, psOptions->pszResampling,
                        psOptions->papszOpenOptions);

    GDALDatasetH hDstDS =
        (GDALDatasetH)oBuilder.Build(psOptions->pfnProgress, psOptions->pProgressData);

    GDALBuildVRTOptionsFree(psOptions);

    return hDstDS;
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = NULL;

    CPLErrorReset();

    hSRS = OSRNewSpatialReference( NULL );
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
        OSRExportToWkt( hSRS, &pszResult );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Translating SRS failed:\n%s",
                  pszUserInput );
    }

    OSRDestroySpatialReference( hSRS );

    return pszResult;
}

/************************************************************************/
/*                             GDALBuildVRTOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALBuildVRTOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdalbuildvrt.html">gdalbuildvrt</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALBuildVRTOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALBuildVRTOptions struct. Must be freed with GDALBuildVRTOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALBuildVRTOptions *GDALBuildVRTOptionsNew(char** papszArgv,
                                            GDALBuildVRTOptionsForBinary* psOptionsForBinary)
{
    GDALBuildVRTOptions *psOptions = static_cast<GDALBuildVRTOptions *>(
        CPLCalloc(1, sizeof(GDALBuildVRTOptions)) );

    const char *tile_index = "location";

    psOptions->nSubdataset = -1;
    psOptions->bQuiet = TRUE;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int iArg = 0; papszArgv != NULL && iArg < argc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-tileindex") && iArg + 1 < argc )
        {
            tile_index = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-resolution") && iArg + 1 < argc )
        {
            CPLFree(psOptions->pszResolution);
            psOptions->pszResolution = CPLStrdup(papszArgv[++iArg]);
            if( !EQUAL(psOptions->pszResolution, "user") &&
                !EQUAL(psOptions->pszResolution, "average") &&
                !EQUAL(psOptions->pszResolution, "highest") &&
                !EQUAL(psOptions->pszResolution, "lowest") )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Illegal resolution value (%s).", psOptions->pszResolution );
                GDALBuildVRTOptionsFree(psOptions);
                return NULL;
            }
        }
        else if( EQUAL(papszArgv[iArg],"-input_file_list") && iArg + 1 < argc )
        {
            ++iArg;
            if( psOptionsForBinary )
            {
                const char* input_file_list = papszArgv[iArg];
                VSILFILE* f = VSIFOpenL(input_file_list, "r");
                if (f)
                {
                    while(1)
                    {
                        const char* filename = CPLReadLineL(f);
                        if (filename == NULL)
                            break;
                        if( !add_file_to_list(filename, tile_index,
                                         &psOptionsForBinary->nSrcFiles,
                                         &psOptionsForBinary->papszSrcFiles) )
                        {
                            VSIFCloseL(f);
                            GDALBuildVRTOptionsFree(psOptions);
                            return NULL;
                        }
                    }
                    VSIFCloseL(f);
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-input_file_list not supported in non binary mode");
            }
        }
        else if ( EQUAL(papszArgv[iArg],"-separate") )
        {
            psOptions->bSeparate = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-allow_projection_difference") )
        {
            psOptions->bAllowProjectionDifference = TRUE;
        }
        else if( EQUAL(papszArgv[iArg], "-sd") && iArg + 1 < argc )
        {
            psOptions->nSubdataset = atoi(papszArgv[++iArg]);
        }
        /* Alternate syntax for output file */
        else if( EQUAL(papszArgv[iArg],"-o") && iArg + 1 < argc )
        {
            ++iArg;
            if( psOptionsForBinary )
            {
                CPLFree(psOptionsForBinary->pszDstFilename);
                psOptionsForBinary->pszDstFilename = CPLStrdup(papszArgv[iArg]);
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-o not supported in non binary mode");
            }
        }
        else if ( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") )
        {
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bQuiet = TRUE;
            }
        }
        else if ( EQUAL(papszArgv[iArg],"-tr") && iArg + 2 < argc )
        {
            psOptions->we_res = CPLAtofM(papszArgv[++iArg]);
            psOptions->ns_res = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-tap") )
        {
            psOptions->bTargetAlignedPixels = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-te") && iArg + 4 < argc )
        {
            psOptions->xmin = CPLAtofM(papszArgv[++iArg]);
            psOptions->ymin = CPLAtofM(papszArgv[++iArg]);
            psOptions->xmax = CPLAtofM(papszArgv[++iArg]);
            psOptions->ymax = CPLAtofM(papszArgv[++iArg]);
        }
        else if ( EQUAL(papszArgv[iArg],"-addalpha") )
        {
            psOptions->bAddAlpha = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-b") && iArg + 1 < argc )
        {
            const char* pszBand = papszArgv[++iArg];
            int nBand = atoi(pszBand);
            if( nBand < 1 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Illegal band number (%s).", papszArgv[iArg] );
                GDALBuildVRTOptionsFree(psOptions);
                return NULL;
            }

            if(nBand > psOptions->nMaxBandNo)
            {
                psOptions->nMaxBandNo = nBand;
            }

            psOptions->nBandCount++;
            psOptions->panBandList = static_cast<int *>(
                CPLRealloc(psOptions->panBandList,
                           sizeof(int) * psOptions->nBandCount));
            psOptions->panBandList[psOptions->nBandCount-1] = nBand;
        }
        else if ( EQUAL(papszArgv[iArg],"-hidenodata") )
        {
            psOptions->bHideNoData = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-overwrite") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bOverwrite = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-srcnodata") && iArg + 1 < argc )
        {
            CPLFree(psOptions->pszSrcNoData);
            psOptions->pszSrcNoData = CPLStrdup(papszArgv[++iArg]);
        }
        else if ( EQUAL(papszArgv[iArg],"-vrtnodata") && iArg + 1 < argc )
        {
            CPLFree(psOptions->pszVRTNoData);
            psOptions->pszVRTNoData = CPLStrdup(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-a_srs") && iArg + 1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++iArg]);
            if(pszSRS == NULL)
            {
                GDALBuildVRTOptionsFree(psOptions);
                return NULL;
            }
            CPLFree(psOptions->pszOutputSRS);
            psOptions->pszOutputSRS = pszSRS;
        }
        else if( EQUAL(papszArgv[iArg],"-r") && iArg + 1 < argc )
        {
            CPLFree(psOptions->pszResampling);
            psOptions->pszResampling = CPLStrdup(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-oo") && iArg+1 < argc )
        {
            psOptions->papszOpenOptions =
                    CSLAddString( psOptions->papszOpenOptions,
                                  papszArgv[++iArg] );
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[iArg]);
            GDALBuildVRTOptionsFree(psOptions);
            return NULL;
        }
        else
        {
            if( psOptionsForBinary )
            {
                if( psOptionsForBinary->pszDstFilename == NULL )
                    psOptionsForBinary->pszDstFilename = CPLStrdup(papszArgv[iArg]);
                else
                {
                    if( !add_file_to_list(papszArgv[iArg], tile_index,
                                          &psOptionsForBinary->nSrcFiles,
                                          &psOptionsForBinary->papszSrcFiles) )
                    {
                        GDALBuildVRTOptionsFree(psOptions);
                        return NULL;
                    }
                }
            }
        }
    }

    return psOptions;
}

/************************************************************************/
/*                        GDALBuildVRTOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALBuildVRTOptions struct.
 *
 * @param psOptions the options struct for GDALBuildVRT().
 *
 * @since GDAL 2.1
 */

void GDALBuildVRTOptionsFree( GDALBuildVRTOptions *psOptions )
{
    if( psOptions )
    {
        CPLFree( psOptions->pszResolution );
        CPLFree( psOptions->pszSrcNoData );
        CPLFree( psOptions->pszVRTNoData );
        CPLFree( psOptions->pszOutputSRS );
        CPLFree( psOptions->panBandList );
        CPLFree( psOptions->pszResampling );
        CSLDestroy( psOptions->papszOpenOptions );
    }

    CPLFree(psOptions);
}

/************************************************************************/
/*                 GDALBuildVRTOptionsSetProgress()                    */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALBuildVRT().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALBuildVRTOptionsSetProgress( GDALBuildVRTOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = FALSE;
}
