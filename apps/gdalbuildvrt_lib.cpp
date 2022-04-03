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

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <vector>
#include <set>

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
#include "ogr_spatialref.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

namespace {
typedef enum
{
    LOWEST_RESOLUTION,
    HIGHEST_RESOLUTION,
    AVERAGE_RESOLUTION,
    USER_RESOLUTION
} ResolutionStrategy;

struct DatasetProperty
{
    int    isFileOK = FALSE;
    int    nRasterXSize = 0;
    int    nRasterYSize = 0;
    double adfGeoTransform[6];
    int    nBlockXSize = 0;
    int    nBlockYSize = 0;
    GDALDataType firstBandType = GDT_Unknown;
    std::vector<bool>   abHasNoData{};
    std::vector<double> adfNoDataValues{};
    std::vector<bool>   abHasOffset{};
    std::vector<double> adfOffset{};
    std::vector<bool>   abHasScale{};
    std::vector<bool>   abHasMaskBand{};
    std::vector<double> adfScale{};
    int    bHasDatasetMask = 0;
    int    nMaskBlockXSize = 0;
    int    nMaskBlockYSize = 0;
    std::vector<int> anOverviewFactors{};

    DatasetProperty()
    {
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 0;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 0;
    }
};

struct BandProperty
{
    GDALColorInterp        colorInterpretation = GCI_Undefined;
    GDALDataType           dataType = GDT_Unknown;
    std::unique_ptr<GDALColorTable> colorTable{};
    bool                   bHasNoData = false;
    double                 noDataValue = 0;
    bool                   bHasOffset = false;
    double                 dfOffset = 0;
    bool                   bHasScale = false;
    double                 dfScale = 0;
};
} // namespace

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
                  int nTargetXSize, int nTargetYSize,
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

    *pdfSrcXSize = psDP->nRasterXSize;
    *pdfSrcYSize = psDP->nRasterYSize;
    if( *pdfSrcXOff > 0 )
        *pdfSrcXSize -= *pdfSrcXOff;
    if( *pdfSrcYOff > 0 )
        *pdfSrcYSize -= *pdfSrcYOff;

    const double dfSrcToDstXSize = psDP->adfGeoTransform[GEOTRSFRM_WE_RES] / we_res;
    *pdfDstXSize = *pdfSrcXSize * dfSrcToDstXSize;
    const double dfSrcToDstYSize = psDP->adfGeoTransform[GEOTRSFRM_NS_RES] / ns_res;
    *pdfDstYSize = *pdfSrcYSize * dfSrcToDstYSize;

    if( *pdfDstXOff + *pdfDstXSize > nTargetXSize )
    {
        *pdfDstXSize = nTargetXSize - *pdfDstXOff;
        *pdfSrcXSize = *pdfDstXSize / dfSrcToDstXSize;
    }

    if( *pdfDstYOff + *pdfDstYSize > nTargetYSize )
    {
        *pdfDstYSize = nTargetYSize - *pdfDstYOff;
        *pdfSrcYSize = *pdfDstYSize / dfSrcToDstYSize;
    }

    return TRUE;
}

/************************************************************************/
/*                            VRTBuilder                                */
/************************************************************************/

class VRTBuilder
{
    /* Input parameters */
    bool                bStrict = false;
    char               *pszOutputFilename = nullptr;
    int                 nInputFiles = 0;
    char              **ppszInputFilenames = nullptr;
    int                 nSrcDSCount = 0;
    GDALDatasetH       *pahSrcDS = nullptr;
    int                 nTotalBands = 0;
    bool                bExplicitBandList = false;
    int                 nMaxSelectedBandNo = 0;
    int                 nSelectedBands = 0;
    int                *panSelectedBandList = nullptr;
    ResolutionStrategy  resolutionStrategy = AVERAGE_RESOLUTION;
    double              we_res = 0;
    double              ns_res = 0;
    int                 bTargetAlignedPixels = 0;
    double              minX = 0;
    double              minY = 0;
    double              maxX = 0;
    double              maxY = 0;
    int                 bSeparate = 0;
    int                 bAllowProjectionDifference = 0;
    int                 bAddAlpha = 0;
    int                 bHideNoData = 0;
    int                 nSubdataset = 0;
    char               *pszSrcNoData = nullptr;
    char               *pszVRTNoData = nullptr;
    char               *pszOutputSRS = nullptr;
    char               *pszResampling = nullptr;
    char              **papszOpenOptions = nullptr;
    bool                bUseSrcMaskBand = true;

    /* Internal variables */
    char               *pszProjectionRef = nullptr;
    std::vector<BandProperty> asBandProperties{};
    int                 bFirst = TRUE;
    int                 bHasGeoTransform = 0;
    int                 nRasterXSize = 0;
    int                 nRasterYSize = 0;
    std::vector<DatasetProperty> asDatasetProperties{};
    int                 bUserExtent = 0;
    int                 bAllowSrcNoData = TRUE;
    double             *padfSrcNoData = nullptr;
    int                 nSrcNoDataCount = 0;
    int                 bAllowVRTNoData = TRUE;
    double             *padfVRTNoData = nullptr;
    int                 nVRTNoDataCount = 0;
    int                 bHasRunBuild = 0;
    int                 bHasDatasetMask = 0;

    std::string         AnalyseRaster(GDALDatasetH hDS,
                              DatasetProperty* psDatasetProperties);

    void        CreateVRTSeparate(VRTDatasetH hVRTDS);
    void        CreateVRTNonSeparate(VRTDatasetH hVRTDS);

    public:
                VRTBuilder(bool bStrictIn,
                           const char* pszOutputFilename,
                           int nInputFiles,
                           const char* const * ppszInputFilenames,
                           GDALDatasetH *pahSrcDSIn,
                           const int *panSelectedBandListIn, int nBandCount,
                           ResolutionStrategy resolutionStrategy,
                           double we_res, double ns_res,
                           int bTargetAlignedPixels,
                           double minX, double minY, double maxX, double maxY,
                           int bSeparate, int bAllowProjectionDifference,
                           int bAddAlpha, int bHideNoData, int nSubdataset,
                           const char* pszSrcNoData, const char* pszVRTNoData,
                           bool bUseSrcMaskBand,
                           const char* pszOutputSRS,
                           const char* pszResampling,
                           const char* const* papszOpenOptionsIn );

               ~VRTBuilder();

        GDALDataset*     Build(GDALProgressFunc pfnProgress, void * pProgressData);
};

/************************************************************************/
/*                          VRTBuilder()                                */
/************************************************************************/

VRTBuilder::VRTBuilder(bool bStrictIn, const char* pszOutputFilenameIn,
                       int nInputFilesIn,
                       const char* const * ppszInputFilenamesIn,
                       GDALDatasetH *pahSrcDSIn,
                       const int *panSelectedBandListIn, int nBandCount,
                       ResolutionStrategy resolutionStrategyIn,
                       double we_resIn, double ns_resIn,
                       int bTargetAlignedPixelsIn,
                       double minXIn, double minYIn, double maxXIn, double maxYIn,
                       int bSeparateIn, int bAllowProjectionDifferenceIn,
                       int bAddAlphaIn, int bHideNoDataIn, int nSubdatasetIn,
                       const char* pszSrcNoDataIn, const char* pszVRTNoDataIn,
                       bool bUseSrcMaskBandIn,
                       const char* pszOutputSRSIn,
                       const char* pszResamplingIn,
                       const char* const * papszOpenOptionsIn ):
    bStrict(bStrictIn)
{
    pszOutputFilename = CPLStrdup(pszOutputFilenameIn);
    nInputFiles = nInputFilesIn;
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
        nSrcDSCount = nInputFiles;
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

    bExplicitBandList = nBandCount != 0;
    nSelectedBands = nBandCount;
    if( nBandCount )
    {
        panSelectedBandList = static_cast<int *>(CPLMalloc(nSelectedBands * sizeof(int)));
        memcpy(panSelectedBandList, panSelectedBandListIn, nSelectedBands * sizeof(int));
    }

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
    pszSrcNoData = (pszSrcNoDataIn) ? CPLStrdup(pszSrcNoDataIn) : nullptr;
    pszVRTNoData = (pszVRTNoDataIn) ? CPLStrdup(pszVRTNoDataIn) : nullptr;
    pszOutputSRS = (pszOutputSRSIn) ? CPLStrdup(pszOutputSRSIn) : nullptr;
    pszResampling = (pszResamplingIn) ? CPLStrdup(pszResamplingIn) : nullptr;
    bUseSrcMaskBand = bUseSrcMaskBandIn;
}

/************************************************************************/
/*                         ~VRTBuilder()                                */
/************************************************************************/

VRTBuilder::~VRTBuilder()
{
    CPLFree(pszOutputFilename);
    CPLFree(pszSrcNoData);
    CPLFree(pszVRTNoData);
    CPLFree(panSelectedBandList);

    if( ppszInputFilenames )
    {
        for(int i=0;i<nInputFiles;i++)
        {
            CPLFree(ppszInputFilenames[i]);
        }
    }
    CPLFree(ppszInputFilenames);
    CPLFree(pahSrcDS);

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
    int bRet = hSRS1 != nullptr && hSRS2 != nullptr && OSRIsSame(hSRS1,hSRS2);
    if (hSRS1)
        OSRDestroySpatialReference(hSRS1);
    if (hSRS2)
        OSRDestroySpatialReference(hSRS2);
    return bRet;
}

/************************************************************************/
/*                         GetProjectionName()                          */
/************************************************************************/

static CPLString GetProjectionName(const char* pszProjection)
{
    if( !pszProjection )
        return "(null)";

    OGRSpatialReference oSRS;
    oSRS.SetFromUserInput(pszProjection);
    const char* pszRet = nullptr;
    if( oSRS.IsProjected() )
        pszRet = oSRS.GetAttrValue("PROJCS");
    else if( oSRS.IsGeographic() )
        pszRet = oSRS.GetAttrValue("GEOGCS");
    return pszRet ? pszRet : "(null)";
}

/************************************************************************/
/*                           AnalyseRaster()                            */
/************************************************************************/

std::string VRTBuilder::AnalyseRaster( GDALDatasetH hDS, DatasetProperty* psDatasetProperties)
{
    GDALDataset* poDS = GDALDataset::FromHandle(hDS);
    const char* dsFileName = poDS->GetDescription();
    char** papszMetadata = poDS->GetMetadata( "SUBDATASETS" );
    if( CSLCount(papszMetadata) > 0 && poDS->GetRasterCount() == 0 )
    {
        ppszInputFilenames = static_cast<char **>(
            CPLRealloc(ppszInputFilenames,
                       sizeof(char*) * (nInputFiles + CSLCount(papszMetadata))));
        if ( nSubdataset < 0 )
        {
            int count = 1;
            char subdatasetNameKey[80];
            snprintf(subdatasetNameKey, sizeof(subdatasetNameKey), "SUBDATASET_%d_NAME", count);
            while(*papszMetadata != nullptr)
            {
                if (EQUALN(*papszMetadata, subdatasetNameKey, strlen(subdatasetNameKey)))
                {
                    asDatasetProperties.resize(nInputFiles+1);
                    ppszInputFilenames[nInputFiles] =
                            CPLStrdup(*papszMetadata+strlen(subdatasetNameKey)+1);
                    nInputFiles++;
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
                asDatasetProperties.resize(nInputFiles+1);
                ppszInputFilenames[nInputFiles] = CPLStrdup( pszSubdatasetName );
                nInputFiles++;
            }
        }
        return "SILENTLY_IGNORE";
    }

    const char* proj = poDS->GetProjectionRef();
    double* padfGeoTransform = psDatasetProperties->adfGeoTransform;
    int bGotGeoTransform = poDS->GetGeoTransform(padfGeoTransform) == CE_None;
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
            return "gdalbuildvrt -separate cannot stack ungeoreferenced and georeferenced images.";
        }
        else if (!bHasGeoTransform &&
                    (nRasterXSize != poDS->GetRasterXSize() ||
                    nRasterYSize != poDS->GetRasterYSize()))
        {
            return "gdalbuildvrt -separate cannot stack ungeoreferenced images that have not the same dimensions.";
        }
    }
    else
    {
        if (!bGotGeoTransform)
        {
            return "gdalbuildvrt does not support ungeoreferenced image.";
        }
        bHasGeoTransform = TRUE;
    }

    if (bGotGeoTransform)
    {
        if (padfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] != 0 ||
            padfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] != 0)
        {
            return "gdalbuildvrt does not support rotated geo transforms.";
        }
        if (padfGeoTransform[GEOTRSFRM_NS_RES] >= 0)
        {
            return "gdalbuildvrt does not support positive NS resolution.";
        }
    }

    psDatasetProperties->nRasterXSize = poDS->GetRasterXSize();
    psDatasetProperties->nRasterYSize = poDS->GetRasterYSize();
    if (bFirst && bSeparate && !bGotGeoTransform)
    {
        nRasterXSize = poDS->GetRasterXSize();
        nRasterYSize = poDS->GetRasterYSize();
    }

    double ds_minX = padfGeoTransform[GEOTRSFRM_TOPLEFT_X];
    double ds_maxY = padfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
    double ds_maxX = ds_minX +
                GDALGetRasterXSize(hDS) *
                padfGeoTransform[GEOTRSFRM_WE_RES];
    double ds_minY = ds_maxY +
                GDALGetRasterYSize(hDS) *
                padfGeoTransform[GEOTRSFRM_NS_RES];

    int _nBands = GDALGetRasterCount(hDS);
    if (_nBands == 0)
    {
        return "Dataset has no bands";
    }
    else if (_nBands > 1 && bSeparate)
    {
        if( bStrict )
            return CPLSPrintf("%s has %d bands. Only one expected.", dsFileName, _nBands);
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                 "%s has %d bands. Only the first one will "
                 "be taken into account in the -separate case",
                 dsFileName, _nBands);
            _nBands = 1;
        }
    }

    GDALRasterBand* poFirstBand = poDS->GetRasterBand(1);
    poFirstBand->GetBlockSize(
                        &psDatasetProperties->nBlockXSize,
                        &psDatasetProperties->nBlockYSize);

    /* For the -separate case */
    psDatasetProperties->firstBandType = poFirstBand->GetRasterDataType();

    psDatasetProperties->adfNoDataValues.resize(_nBands);
    psDatasetProperties->abHasNoData.resize(_nBands);

    psDatasetProperties->adfOffset.resize(_nBands);
    psDatasetProperties->abHasOffset.resize(_nBands);

    psDatasetProperties->adfScale.resize(_nBands);
    psDatasetProperties->abHasScale.resize(_nBands);

    psDatasetProperties->abHasMaskBand.resize(_nBands);

    psDatasetProperties->bHasDatasetMask = poFirstBand->GetMaskFlags() == GMF_PER_DATASET;
    if (psDatasetProperties->bHasDatasetMask)
        bHasDatasetMask = TRUE;
    poFirstBand->GetMaskBand()->GetBlockSize(
                        &psDatasetProperties->nMaskBlockXSize,
                        &psDatasetProperties->nMaskBlockYSize);

    // Collect overview factors. We only handle power-of-two situations for now
    const int nOverviews = poFirstBand->GetOverviewCount();
    int nExpectedOvFactor = 2;
    for(int j=0; j < nOverviews; j++)
    {
        GDALRasterBand* poOverview = poFirstBand->GetOverview(j);
        if( !poOverview )
            continue;
        if( poOverview->GetXSize() < 128 &&
            poOverview->GetYSize() < 128 )
        {
            break;
        }

        const int nOvFactor =
            GDALComputeOvFactor(poOverview->GetXSize(),
                                poFirstBand->GetXSize(),
                                poOverview->GetYSize(),
                                poFirstBand->GetYSize());

        if( nOvFactor != nExpectedOvFactor )
            break;

        psDatasetProperties->anOverviewFactors.push_back(nOvFactor);
        nExpectedOvFactor *= 2;
    }

    for(int j=0;j<_nBands;j++)
    {
        GDALRasterBand* poBand = poDS->GetRasterBand(j+1);
        if (!bSeparate && nSrcNoDataCount > 0)
        {
            psDatasetProperties->abHasNoData[j] = true;
            if (j < nSrcNoDataCount)
                psDatasetProperties->adfNoDataValues[j] = padfSrcNoData[j];
            else
                psDatasetProperties->adfNoDataValues[j] = padfSrcNoData[nSrcNoDataCount - 1];
        }
        else
        {
            int bHasNoData = false;
            psDatasetProperties->adfNoDataValues[j]  =
                poBand->GetNoDataValue(&bHasNoData);
            psDatasetProperties->abHasNoData[j] = bHasNoData != 0;
        }

        int bHasOffset = false;
        psDatasetProperties->adfOffset[j] = poBand->GetOffset(&bHasOffset);
        psDatasetProperties->abHasOffset[j] = bHasOffset != 0 &&
                            psDatasetProperties->adfOffset[j] != 0.0;

        int bHasScale = false;
        psDatasetProperties->adfScale[j] = poBand->GetScale(&bHasScale);
        psDatasetProperties->abHasScale[j] = bHasScale != 0 &&
                            psDatasetProperties->adfScale[j] != 1.0;

        const int nMaskFlags = poBand->GetMaskFlags();
        psDatasetProperties->abHasMaskBand[j] =
                    (nMaskFlags != GMF_ALL_VALID && nMaskFlags != GMF_NODATA) ||
                    poBand->GetColorInterpretation() == GCI_AlphaBand;
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
        nTotalBands = _nBands;
        if(nSelectedBands == 0)
        {
            nSelectedBands = _nBands;
            CPLFree(panSelectedBandList);
            panSelectedBandList = static_cast<int *>(CPLMalloc(nSelectedBands * sizeof(int)));
            for(int j=0;j<nSelectedBands;j++)
            {
                panSelectedBandList[j] = j + 1;
            }
        }
        for(int j=0;j<nSelectedBands;j++)
        {
            nMaxSelectedBandNo = std::max(nMaxSelectedBandNo,
                                          panSelectedBandList[j]);
        }

        if (!bSeparate)
        {
            asBandProperties.resize(nSelectedBands);
            for(int j=0;j<nSelectedBands;j++)
            {
                const int nSelBand = panSelectedBandList[j];
                if( nSelBand <= 0 || nSelBand > _nBands )
                {
                    return CPLSPrintf("Invalid band number: %d", nSelBand);
                }
                GDALRasterBand* poBand = poDS->GetRasterBand(nSelBand);
                asBandProperties[j].colorInterpretation =
                        poBand->GetColorInterpretation();
                asBandProperties[j].dataType = poBand->GetRasterDataType();
                if (asBandProperties[j].colorInterpretation == GCI_PaletteIndex)
                {
                    auto colorTable = poBand->GetColorTable();
                    if (colorTable)
                    {
                        asBandProperties[j].colorTable.reset(colorTable->Clone());
                    }
                }
                else
                    asBandProperties[j].colorTable = nullptr;

                if (nVRTNoDataCount > 0)
                {
                    asBandProperties[j].bHasNoData = true;
                    if (j < nVRTNoDataCount)
                        asBandProperties[j].noDataValue = padfVRTNoData[j];
                    else
                        asBandProperties[j].noDataValue = padfVRTNoData[nVRTNoDataCount - 1];
                }
                else
                {
                    int bHasNoData = false;
                    asBandProperties[j].noDataValue =
                            poBand->GetNoDataValue(&bHasNoData);
                    asBandProperties[j].bHasNoData = bHasNoData != 0;
                }

                int bHasOffset = false;
                asBandProperties[j].dfOffset = poBand->GetOffset(&bHasOffset);
                asBandProperties[j].bHasOffset = bHasOffset != 0 &&
                                asBandProperties[j].dfOffset != 0.0;

                int bHasScale = false;
                asBandProperties[j].dfScale = poBand->GetScale(&bHasScale);
                asBandProperties[j].bHasScale = bHasScale != 0 &&
                                asBandProperties[j].dfScale != 1.0;
            }
        }
    }
    else
    {
        if ((proj != nullptr && pszProjectionRef == nullptr) ||
            (proj == nullptr && pszProjectionRef != nullptr) ||
            (proj != nullptr && pszProjectionRef != nullptr &&
                        ProjAreEqual(proj, pszProjectionRef) == FALSE))
        {
            if (!bAllowProjectionDifference)
            {
                CPLString osExpected = GetProjectionName(pszProjectionRef);
                CPLString osGot = GetProjectionName(proj);
                return CPLSPrintf("gdalbuildvrt does not support heterogeneous "
                         "projection: expected %s, got %s.",
                         osExpected.c_str(),
                         osGot.c_str());
            }
        }
        if (!bSeparate)
        {
            if (!bExplicitBandList && _nBands != nTotalBands)
            {
                return CPLSPrintf("gdalbuildvrt does not support heterogeneous band "
                                 "numbers: expected %d, got %d.",
                                 nTotalBands, _nBands);
            }
            else if( bExplicitBandList && _nBands < nMaxSelectedBandNo )
            {
                return CPLSPrintf("gdalbuildvrt does not support heterogeneous band "
                                  "numbers: expected at least %d, got %d.",
                                  nMaxSelectedBandNo, _nBands);
            }

            for(int j=0;j<nSelectedBands;j++)
            {
                const int nSelBand = panSelectedBandList[j];
                CPLAssert(nSelBand >= 1 && nSelBand <= _nBands);
                GDALRasterBand* poBand = poDS->GetRasterBand(nSelBand);
                if (asBandProperties[j].colorInterpretation !=
                            poBand->GetColorInterpretation())
                {
                    return CPLSPrintf("gdalbuildvrt does not support heterogeneous "
                             "band color interpretation: expected %s, got %s.",
                             GDALGetColorInterpretationName(
                                 asBandProperties[j].colorInterpretation),
                             GDALGetColorInterpretationName(
                                 poBand->GetColorInterpretation()));
                }
                if (asBandProperties[j].dataType != poBand->GetRasterDataType())
                {
                    return CPLSPrintf(
                             "gdalbuildvrt does not support heterogeneous "
                             "band data type: expected %s, got %s.",
                             GDALGetDataTypeName(
                                 asBandProperties[j].dataType),
                             GDALGetDataTypeName(
                                 poBand->GetRasterDataType()));
                }
                if (asBandProperties[j].colorTable)
                {
                    const GDALColorTable* colorTable = poBand->GetColorTable();
                    int nRefColorEntryCount = asBandProperties[j].colorTable->GetColorEntryCount();
                    if (colorTable == nullptr ||
                        colorTable->GetColorEntryCount() != nRefColorEntryCount)
                    {
                        return "gdalbuildvrt does not support rasters with different color tables (different number of color table entries)";
                    }

                    /* Check that the palette are the same too */
                    /* We just warn and still process the file. It is not a technical no-go, but the user */
                    /* should check that the end result is OK for him. */
                    for(int i=0;i<nRefColorEntryCount;i++)
                    {
                        const GDALColorEntry* psEntry = colorTable->GetColorEntry(i);
                        const GDALColorEntry* psEntryRef = asBandProperties[j].colorTable->GetColorEntry(i);
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

                if( psDatasetProperties->abHasOffset[j] != asBandProperties[j].bHasOffset ||
                    (asBandProperties[j].bHasOffset &&
                     psDatasetProperties->adfOffset[j] != asBandProperties[j].dfOffset) )
                {
                    return CPLSPrintf(
                             "gdalbuildvrt does not support heterogeneous "
                             "band offset: expected (%d,%f), got (%d,%f).",
                             static_cast<int>(asBandProperties[j].bHasOffset),
                             asBandProperties[j].dfOffset,
                             static_cast<int>(psDatasetProperties->abHasOffset[j]),
                             psDatasetProperties->adfOffset[j]);
                }

                if( psDatasetProperties->abHasScale[j] != asBandProperties[j].bHasScale ||
                    (asBandProperties[j].bHasScale &&
                     psDatasetProperties->adfScale[j] != asBandProperties[j].dfScale) )
                {
                    return CPLSPrintf(
                             "gdalbuildvrt does not support heterogeneous "
                             "band scale: expected (%d,%f), got (%d,%f).",
                             static_cast<int>(asBandProperties[j].bHasScale),
                             asBandProperties[j].dfScale,
                             static_cast<int>(psDatasetProperties->abHasScale[j]),
                             psDatasetProperties->adfScale[j]);
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

    return "";
}

/************************************************************************/
/*                         CreateVRTSeparate()                          */
/************************************************************************/

void VRTBuilder::CreateVRTSeparate(VRTDatasetH hVRTDS)
{
    int iBand = 1;
    for(int i=0; ppszInputFilenames != nullptr && i<nInputFiles;i++)
    {
        DatasetProperty* psDatasetProperties = &asDatasetProperties[i];

        if (psDatasetProperties->isFileOK == FALSE)
            continue;

        double dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize,
               dfDstXOff, dfDstYOff, dfDstXSize, dfDstYSize;
        if (bHasGeoTransform)
        {
            if ( ! GetSrcDstWin(psDatasetProperties,
                        we_res, ns_res, minX, minY, maxX, maxY,
                        nRasterXSize, nRasterYSize,
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

        GDALAddBand(hVRTDS, psDatasetProperties->firstBandType, nullptr);


        GDALDatasetH hSourceDS;
        bool bDropRef = false;
        if( nSrcDSCount == nInputFiles &&
            GDALGetDatasetDriver(pahSrcDS[i]) != nullptr &&
            ( dsFileName[0] == '\0' || // could be a unnamed VRT file
              EQUAL(GDALGetDescription(GDALGetDatasetDriver(pahSrcDS[i])), "MEM")) )
        {
            hSourceDS = pahSrcDS[i];
        }
        else
        {
            bDropRef = true;
            GDALProxyPoolDatasetH hProxyDS =
                GDALProxyPoolDatasetCreate(dsFileName,
                                            psDatasetProperties->nRasterXSize,
                                            psDatasetProperties->nRasterYSize,
                                            GA_ReadOnly, TRUE, pszProjectionRef,
                                            psDatasetProperties->adfGeoTransform);
            hSourceDS = static_cast<GDALDatasetH>(hProxyDS);
            reinterpret_cast<GDALProxyPoolDataset*>(hProxyDS)->
                                            SetOpenOptions( papszOpenOptions );

            GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                                psDatasetProperties->firstBandType,
                                                psDatasetProperties->nBlockXSize,
                                                psDatasetProperties->nBlockYSize);
        }

        VRTSourcedRasterBandH hVRTBand =
                static_cast<VRTSourcedRasterBandH>(GDALGetRasterBand(hVRTDS, iBand));

        if (bHideNoData)
            GDALSetMetadataItem(hVRTBand,"HideNoDataValue","1",nullptr);

        VRTSourcedRasterBand* poVRTBand = static_cast<VRTSourcedRasterBand*>(hVRTBand);

        if( bAllowVRTNoData )
        {
            if (nVRTNoDataCount > 0)
            {
                if (iBand-1 < nVRTNoDataCount)
                    GDALSetRasterNoDataValue(hVRTBand, padfVRTNoData[iBand-1]);
                else
                    GDALSetRasterNoDataValue(hVRTBand, padfVRTNoData[nVRTNoDataCount - 1]);
            }
            else if( psDatasetProperties->abHasNoData[0] )
            {
                GDALSetRasterNoDataValue(hVRTBand, psDatasetProperties->adfNoDataValues[0]);
            }
        }

        VRTSimpleSource* poSimpleSource;
        if (bAllowSrcNoData)
        {
            poSimpleSource = new VRTComplexSource();
            if (nSrcNoDataCount > 0)
            {
                if (iBand-1 < nSrcNoDataCount)
                    poSimpleSource->SetNoDataValue( padfSrcNoData[iBand-1] );
                else
                    poSimpleSource->SetNoDataValue( padfSrcNoData[nSrcNoDataCount - 1] );
            }
            else if( psDatasetProperties->abHasNoData[0] )
            {
                poSimpleSource->SetNoDataValue( psDatasetProperties->adfNoDataValues[0] );
            }
        }
        else if( bUseSrcMaskBand && psDatasetProperties->abHasMaskBand[0] )
        {
            auto poSource = new VRTComplexSource();
            poSource->SetUseMaskBand(true);
            poSimpleSource = poSource;
        }
        else
            poSimpleSource = new VRTSimpleSource();

        if( pszResampling )
            poSimpleSource->SetResampling(pszResampling);
        poVRTBand->ConfigureSource( poSimpleSource,
                                    static_cast<GDALRasterBand*>(GDALGetRasterBand(hSourceDS, 1)),
                                    FALSE,
                                    dfSrcXOff, dfSrcYOff,
                                    dfSrcXSize, dfSrcYSize,
                                    dfDstXOff, dfDstYOff,
                                    dfDstXSize, dfDstYSize );

        if( psDatasetProperties->abHasOffset[0] )
            poVRTBand->SetOffset( psDatasetProperties->adfOffset[0] );

        if( psDatasetProperties->abHasScale[0] )
            poVRTBand->SetScale( psDatasetProperties->adfScale[0] );

        poVRTBand->AddSource( poSimpleSource );

        if( bDropRef )
        {
            GDALDereferenceDataset(hSourceDS);
        }

        iBand ++;
    }
}

/************************************************************************/
/*                       CreateVRTNonSeparate()                         */
/************************************************************************/

void VRTBuilder::CreateVRTNonSeparate(VRTDatasetH hVRTDS)
{
    VRTDataset* poVRTDS = reinterpret_cast<VRTDataset*>(hVRTDS);
    for(int j=0;j<nSelectedBands;j++)
    {
        poVRTDS->AddBand(asBandProperties[j].dataType);
        GDALRasterBand *poBand = poVRTDS->GetRasterBand(j+1);
        poBand->SetColorInterpretation(asBandProperties[j].colorInterpretation);
        if (asBandProperties[j].colorInterpretation == GCI_PaletteIndex)
        {
            poBand->SetColorTable(asBandProperties[j].colorTable.get());
        }
        if (bAllowVRTNoData && asBandProperties[j].bHasNoData)
            poBand->SetNoDataValue(asBandProperties[j].noDataValue);
        if ( bHideNoData )
            poBand->SetMetadataItem("HideNoDataValue","1");

        if( asBandProperties[j].bHasOffset )
            poBand->SetOffset( asBandProperties[j].dfOffset );

        if( asBandProperties[j].bHasScale )
            poBand->SetScale( asBandProperties[j].dfScale );
    }

    VRTSourcedRasterBand* poMaskVRTBand = nullptr;
    if (bAddAlpha)
    {
        poVRTDS->AddBand(GDT_Byte);
        GDALRasterBand *poBand = poVRTDS->GetRasterBand(nSelectedBands + 1);
        poBand->SetColorInterpretation(GCI_AlphaBand);
    }
    else if (bHasDatasetMask)
    {
        poVRTDS->CreateMaskBand(GMF_PER_DATASET);
        poMaskVRTBand = static_cast<VRTSourcedRasterBand*>(poVRTDS->GetRasterBand(1)->GetMaskBand());
    }

    bool bCanCollectOverviewFactors = true;
    std::set<int> anOverviewFactorsSet;
    std::vector<int> anIdxValidDatasets;

    for( int i = 0; ppszInputFilenames != nullptr && i < nInputFiles; i++ )
    {
        DatasetProperty* psDatasetProperties = &asDatasetProperties[i];

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
                        nRasterXSize, nRasterYSize,
                        &dfSrcXOff, &dfSrcYOff, &dfSrcXSize, &dfSrcYSize,
                        &dfDstXOff, &dfDstYOff, &dfDstXSize, &dfDstYSize) )
            continue;

        anIdxValidDatasets.push_back(i);

        if( bCanCollectOverviewFactors )
        {
            if( std::abs(psDatasetProperties->adfGeoTransform[1] - we_res) > 1e-8 * std::abs(we_res) ||
                std::abs(psDatasetProperties->adfGeoTransform[5] - ns_res) > 1e-8 * std::abs(ns_res) )
            {
                bCanCollectOverviewFactors = false;
                anOverviewFactorsSet.clear();
            }
        }
        if( bCanCollectOverviewFactors )
        {
            for( int nOvFactor: psDatasetProperties->anOverviewFactors )
                anOverviewFactorsSet.insert(nOvFactor);
        }

        const char* dsFileName = ppszInputFilenames[i];

        GDALDatasetH hSourceDS;
        bool bDropRef = false;

        if( nSrcDSCount == nInputFiles &&
            GDALGetDatasetDriver(pahSrcDS[i]) != nullptr &&
            ( dsFileName[0] == '\0' || // could be a unnamed VRT file
              EQUAL(GDALGetDescription(GDALGetDatasetDriver(pahSrcDS[i])), "MEM")) )
        {
            hSourceDS = pahSrcDS[i];
        }
        else
        {
            bDropRef = true;
            GDALProxyPoolDatasetH hProxyDS =
                GDALProxyPoolDatasetCreate(dsFileName,
                                            psDatasetProperties->nRasterXSize,
                                            psDatasetProperties->nRasterYSize,
                                            GA_ReadOnly, TRUE, pszProjectionRef,
                                            psDatasetProperties->adfGeoTransform);
            reinterpret_cast<GDALProxyPoolDataset*>(hProxyDS)->
                                            SetOpenOptions( papszOpenOptions );

            for(int j=0;j<nMaxSelectedBandNo;j++)
            {
                GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                                asBandProperties[j].dataType,
                                                psDatasetProperties->nBlockXSize,
                                                psDatasetProperties->nBlockYSize);
            }
            if (bHasDatasetMask && !bAddAlpha)
            {
                static_cast<GDALProxyPoolRasterBand*>(reinterpret_cast<GDALProxyPoolDataset*>(hProxyDS)->GetRasterBand(1))->
                        AddSrcMaskBandDescription  (GDT_Byte,
                                                    psDatasetProperties->nMaskBlockXSize,
                                                    psDatasetProperties->nMaskBlockYSize);
            }

            hSourceDS = static_cast<GDALDatasetH>(hProxyDS);
        }

        for(int j=0;j<nSelectedBands;j++)
        {
            VRTSourcedRasterBandH hVRTBand =
                    static_cast<VRTSourcedRasterBandH>(poVRTDS->GetRasterBand(j + 1));
            const int nSelBand = panSelectedBandList[j];

            /* Place the raster band at the right position in the VRT */
            VRTSourcedRasterBand* poVRTBand = static_cast<VRTSourcedRasterBand*>(hVRTBand);

            VRTSimpleSource* poSimpleSource;
            if (bAllowSrcNoData && psDatasetProperties->abHasNoData[nSelBand - 1])
            {
                poSimpleSource = new VRTComplexSource();
                poSimpleSource->SetNoDataValue( psDatasetProperties->adfNoDataValues[nSelBand - 1] );
            }
            else if( bUseSrcMaskBand && psDatasetProperties->abHasMaskBand[nSelBand - 1] )
            {
                auto poSource = new VRTComplexSource();
                poSource->SetUseMaskBand(true);
                poSimpleSource = poSource;
            }
            else
                poSimpleSource = new VRTSimpleSource();
            if( pszResampling )
                poSimpleSource->SetResampling(pszResampling);
            auto poSrcBand = GDALRasterBand::FromHandle(
                GDALGetRasterBand(hSourceDS, nSelBand));
            poVRTBand->ConfigureSource( poSimpleSource,
                                        poSrcBand,
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
                    static_cast<VRTSourcedRasterBandH>(GDALGetRasterBand(hVRTDS, nSelectedBands + 1));
            /* Little trick : we use an offset of 255 and a scaling of 0, so that in areas covered */
            /* by the source, the value of the alpha band will be 255, otherwise it will be 0 */
            static_cast<VRTSourcedRasterBand *>(hVRTBand)->AddComplexSource(
                                            static_cast<GDALRasterBand*>(GDALGetRasterBand(hSourceDS, 1)),
                                            dfSrcXOff, dfSrcYOff,
                                            dfSrcXSize, dfSrcYSize,
                                            dfDstXOff, dfDstYOff,
                                            dfDstXSize, dfDstYSize,
                                            255, 0, VRT_NODATA_UNSET);
        }
        else if (bHasDatasetMask)
        {
            VRTSimpleSource* poSource;
            if( bUseSrcMaskBand )
            {
                auto poComplexSource = new VRTComplexSource();
                poComplexSource->SetUseMaskBand(true);
                poSource = poComplexSource;
            }
            else
            {
                poSource =  new VRTSimpleSource();
            }
            if( pszResampling )
                poSource->SetResampling(pszResampling);
            assert( poMaskVRTBand );
            poMaskVRTBand->ConfigureSource( poSource,
                                            static_cast<GDALRasterBand*>(GDALGetRasterBand(hSourceDS, 1)),
                                            TRUE,
                                            dfSrcXOff, dfSrcYOff,
                                            dfSrcXSize, dfSrcYSize,
                                            dfDstXOff, dfDstYOff,
                                            dfDstXSize, dfDstYSize );

            poMaskVRTBand->AddSource( poSource );
        }

        if( bDropRef )
        {
            GDALDereferenceDataset(hSourceDS);
        }
    }

    for( int i: anIdxValidDatasets )
    {
        const DatasetProperty* psDatasetProperties = &asDatasetProperties[i];
        for( auto oIter = anOverviewFactorsSet.begin(); oIter != anOverviewFactorsSet.end(); )
        {
            const int nGlobalOvrFactor = *oIter;
            auto oIterNext = oIter;
            ++oIterNext;

            if( psDatasetProperties->nRasterXSize / nGlobalOvrFactor < 128 &&
                psDatasetProperties->nRasterYSize / nGlobalOvrFactor < 128 )
            {
                break;
            }
            if( std::find(psDatasetProperties->anOverviewFactors.begin(),
                          psDatasetProperties->anOverviewFactors.end(),
                          nGlobalOvrFactor) == psDatasetProperties->anOverviewFactors.end() )
            {
                anOverviewFactorsSet.erase(oIter);
            }

            oIter = oIterNext;
        }
    }
    if( !anOverviewFactorsSet.empty() )
    {
        std::vector<int> anOverviewFactors;
        anOverviewFactors.insert(anOverviewFactors.end(),
                                 anOverviewFactorsSet.begin(),
                                 anOverviewFactorsSet.end());
        CPLConfigOptionSetter oSetter("VRT_VIRTUAL_OVERVIEWS", "YES", false);
        poVRTDS->BuildOverviews(pszResampling ? pszResampling : "nearest",
                                static_cast<int>(anOverviewFactors.size()),
                                &anOverviewFactors[0],
                                0, nullptr, nullptr, nullptr);
    }
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALDataset* VRTBuilder::Build(GDALProgressFunc pfnProgress, void * pProgressData)
{
    if (bHasRunBuild)
        return nullptr;
    bHasRunBuild = TRUE;

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    bUserExtent = (minX != 0 || minY != 0 || maxX != 0 || maxY != 0);
    if (bUserExtent)
    {
        if (minX >= maxX || minY >= maxY )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user extent");
            return nullptr;
        }
    }

    if (resolutionStrategy == USER_RESOLUTION)
    {
        if (we_res <= 0 || ns_res <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user resolution");
            return nullptr;
        }

        /* We work with negative north-south resolution in all the following code */
        ns_res = -ns_res;
    }
    else
    {
        we_res = ns_res = 0;
    }

    asDatasetProperties.resize(nInputFiles);

    if (pszSrcNoData != nullptr)
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
                    return nullptr;
                }
                padfSrcNoData[i] = CPLAtofM(papszTokens[i]);
            }
            CSLDestroy(papszTokens);
        }
    }

    if (pszVRTNoData != nullptr)
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
                    return nullptr;
                }
                padfVRTNoData[i] = CPLAtofM(papszTokens[i]);
            }
            CSLDestroy(papszTokens);
        }
    }

    int nCountValid = 0;
    for(int i=0; ppszInputFilenames != nullptr && i<nInputFiles;i++)
    {
        const char* dsFileName = ppszInputFilenames[i];

        if (!pfnProgress( 1.0 * (i+1) / nInputFiles, nullptr, pProgressData))
        {
            return nullptr;
        }

        GDALDatasetH hDS =
            (pahSrcDS) ? pahSrcDS[i] :
                GDALOpenEx( dsFileName,
                            GDAL_OF_RASTER, nullptr,
                            papszOpenOptions, nullptr );
        asDatasetProperties[i].isFileOK = FALSE;

        if (hDS)
        {
            const auto osErrorMsg = AnalyseRaster( hDS, &asDatasetProperties[i] );
            if (osErrorMsg.empty())
            {
                asDatasetProperties[i].isFileOK = TRUE;
                nCountValid ++;
                bFirst = FALSE;
            }
            if( pahSrcDS == nullptr )
                GDALClose(hDS);
            if( !osErrorMsg.empty() && osErrorMsg != "SILENTLY_IGNORE" )
            {
                if( bStrict )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMsg.c_str());
                    return nullptr;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "%s Skipping %s",
                             osErrorMsg.c_str(), dsFileName);
                }
            }
        }
        else
        {
            if( bStrict )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Can't open %s.", dsFileName);
                return nullptr;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Can't open %s. Skipping it", dsFileName);
            }
        }
    }

    if (nCountValid == 0)
        return nullptr;

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
        return nullptr;
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

    return static_cast<GDALDataset*>(hVRTDS);
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
        OGRDataSourceH hDS = OGROpen( filename, FALSE, nullptr );
        if( hDS  == nullptr )
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
        ppszInputFilenames[nInputFiles] = nullptr;

        OGR_DS_Destroy( hDS );
    }
    else
    {
        ppszInputFilenames = static_cast<char **>(
            CPLRealloc(ppszInputFilenames,
                       sizeof(char*) * (nInputFiles + 1 + 1)));
        ppszInputFilenames[nInputFiles++] = CPLStrdup(filename);
        ppszInputFilenames[nInputFiles] = nullptr;
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
    bool bStrict;
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
    int *panSelectedBandList;
    int nBandCount;
    char* pszResampling;
    char** papszOpenOptions;
    bool bUseSrcMaskBand;

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
    if( psOptionsIn->panSelectedBandList )
    {
        psOptions->panSelectedBandList = static_cast<int*>(CPLMalloc(sizeof(int) * psOptionsIn->nBandCount));
        memcpy(psOptions->panSelectedBandList, psOptionsIn->panSelectedBandList, sizeof(int) * psOptionsIn->nBandCount);
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
 * This is the equivalent of the <a href="/programs/gdalbuildvrt.html">gdalbuildvrt</a> utility.
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
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALBuildVRT( const char *pszDest,
                           int nSrcCount, GDALDatasetH *pahSrcDS, const char* const* papszSrcDSNames,
                           const GDALBuildVRTOptions *psOptionsIn, int *pbUsageError )
{
    if( pszDest == nullptr )
        pszDest = "";

    if( nSrcCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No input dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALBuildVRTOptions* psOptions =
        (psOptionsIn) ? GDALBuildVRTOptionsClone(psOptionsIn) :
                        GDALBuildVRTOptionsNew(nullptr, nullptr);

    if (psOptions->we_res != 0 && psOptions->ns_res != 0 &&
        psOptions->pszResolution != nullptr && !EQUAL(psOptions->pszResolution, "user"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-tr option is not compatible with -resolution %s", psOptions->pszResolution);
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return nullptr;
    }

    if (psOptions->bTargetAlignedPixels && psOptions->we_res == 0 && psOptions->ns_res == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-tap option cannot be used without using -tr");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return nullptr;
    }

    if (psOptions->bAddAlpha && psOptions->bSeparate)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-addalpha option is not compatible with -separate.");
        if( pbUsageError )
            *pbUsageError = TRUE;
        GDALBuildVRTOptionsFree(psOptions);
        return nullptr;
    }

    ResolutionStrategy eStrategy = AVERAGE_RESOLUTION;
    if ( psOptions->pszResolution == nullptr || EQUAL(psOptions->pszResolution, "user") )
    {
        if ( psOptions->we_res != 0 || psOptions->ns_res != 0)
            eStrategy = USER_RESOLUTION;
        else if ( psOptions->pszResolution != nullptr && EQUAL(psOptions->pszResolution, "user") )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                      "-tr option must be used with -resolution user.");
            if( pbUsageError )
                *pbUsageError = TRUE;
            GDALBuildVRTOptionsFree(psOptions);
            return nullptr;
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
    if (psOptions->pszSrcNoData != nullptr && psOptions->pszVRTNoData == nullptr)
        psOptions->pszVRTNoData = CPLStrdup(psOptions->pszSrcNoData);

    VRTBuilder oBuilder(psOptions->bStrict,
                        pszDest, nSrcCount, papszSrcDSNames, pahSrcDS,
                        psOptions->panSelectedBandList, psOptions->nBandCount,
                        eStrategy, psOptions->we_res, psOptions->ns_res, psOptions->bTargetAlignedPixels,
                        psOptions->xmin, psOptions->ymin, psOptions->xmax, psOptions->ymax,
                        psOptions->bSeparate, psOptions->bAllowProjectionDifference,
                        psOptions->bAddAlpha, psOptions->bHideNoData, psOptions->nSubdataset,
                        psOptions->pszSrcNoData, psOptions->pszVRTNoData,
                        psOptions->bUseSrcMaskBand,
                        psOptions->pszOutputSRS, psOptions->pszResampling,
                        psOptions->papszOpenOptions);

    GDALDatasetH hDstDS =
        static_cast<GDALDatasetH>(oBuilder.Build(psOptions->pfnProgress, psOptions->pProgressData));

    GDALBuildVRTOptionsFree(psOptions);

    return hDstDS;
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = nullptr;

    CPLErrorReset();

    hSRS = OSRNewSpatialReference( nullptr );
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
 *                  The accepted options are the ones of the <a href="/programs/gdalbuildvrt.html">gdalbuildvrt</a> utility.
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
    psOptions->pProgressData = nullptr;
    psOptions->bUseSrcMaskBand = true;
    psOptions->bStrict = false;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int iArg = 0; papszArgv != nullptr && iArg < argc; iArg++ )
    {
        if( strcmp(papszArgv[iArg],"-strict") == 0 )
        {
            psOptions->bStrict = true;
        }
        else if( strcmp(papszArgv[iArg],"-non_strict") == 0 )
        {
            psOptions->bStrict = false;
        }
        else if( EQUAL(papszArgv[iArg],"-tileindex") && iArg + 1 < argc )
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
                return nullptr;
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
                        if (filename == nullptr)
                            break;
                        if( !add_file_to_list(filename, tile_index,
                                         &psOptionsForBinary->nSrcFiles,
                                         &psOptionsForBinary->papszSrcFiles) )
                        {
                            VSIFCloseL(f);
                            GDALBuildVRTOptionsFree(psOptions);
                            return nullptr;
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
                return nullptr;
            }

            psOptions->nBandCount++;
            psOptions->panSelectedBandList = static_cast<int *>(
                CPLRealloc(psOptions->panSelectedBandList,
                           sizeof(int) * psOptions->nBandCount));
            psOptions->panSelectedBandList[psOptions->nBandCount-1] = nBand;
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
            if(pszSRS == nullptr)
            {
                GDALBuildVRTOptionsFree(psOptions);
                return nullptr;
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
        else if( EQUAL(papszArgv[iArg],"-ignore_srcmaskband") )
        {
            psOptions->bUseSrcMaskBand = false;
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[iArg]);
            GDALBuildVRTOptionsFree(psOptions);
            return nullptr;
        }
        else
        {
            if( psOptionsForBinary )
            {
                if( psOptionsForBinary->pszDstFilename == nullptr )
                    psOptionsForBinary->pszDstFilename = CPLStrdup(papszArgv[iArg]);
                else
                {
                    if( !add_file_to_list(papszArgv[iArg], tile_index,
                                          &psOptionsForBinary->nSrcFiles,
                                          &psOptionsForBinary->papszSrcFiles) )
                    {
                        GDALBuildVRTOptionsFree(psOptions);
                        return nullptr;
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
        CPLFree( psOptions->panSelectedBandList );
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
