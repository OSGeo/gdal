/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_frmts.h"
#include "common.h"
#include "include_basisu_sdk.h"

#include <algorithm>
#include <mutex>

/************************************************************************/
/*                        GDALInitBasisUTranscoder()                    */
/************************************************************************/

void GDALInitBasisUTranscoder()
{
    static std::once_flag flag;
    std::call_once(flag, basist::basisu_transcoder_init);
}

/************************************************************************/
/*                        GDALInitBasisUEncoder()                       */
/************************************************************************/

void GDALInitBasisUEncoder()
{
    static std::once_flag flag;
    std::call_once(flag, []() { basisu::basisu_encoder_init(); });
}

/************************************************************************/
/*                           GDALRegister_BASISU_KTX2()                 */
/*                                                                      */
/*      This function exists so that when built as a plugin, there      */
/*      is a function that will register both drivers.                  */
/************************************************************************/

void GDALRegister_BASISU_KTX2()
{
    GDALRegister_BASISU();
    GDALRegister_KTX2();
}

/************************************************************************/
/*                     GDAL_KTX2_BASISU_CreateCopy()                    */
/************************************************************************/

bool GDAL_KTX2_BASISU_CreateCopy(const char *pszFilename, GDALDataset *poSrcDS,
                                 bool bIsKTX2, CSLConstList papszOptions,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData)
{
    const int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0 || nBands > 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only band count >= 1 and <= 4 is supported");
        return false;
    }
    if (poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only Byte data type supported");
        return false;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    void *pSrcData = VSI_MALLOC3_VERBOSE(nXSize, nYSize, nBands);
    if (pSrcData == nullptr)
        return false;

    if (poSrcDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pSrcData, nXSize,
                          nYSize, GDT_Byte, nBands, nullptr, nBands,
                          static_cast<GSpacing>(nBands) * nXSize, 1,
                          nullptr) != CE_None)
    {
        VSIFree(pSrcData);
        return false;
    }

    basisu::image img;
    try
    {
        img.init(static_cast<const uint8_t *>(pSrcData), nXSize, nYSize,
                 nBands);
        VSIFree(pSrcData);
    }
    catch (const std::exception &e)
    {
        VSIFree(pSrcData);
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return false;
    }

    GDALInitBasisUEncoder();

    const bool bVerbose = CPLTestBool(CPLGetConfigOption("KTX2_VERBOSE", "NO"));

    basisu::basis_compressor_params params;
    params.m_create_ktx2_file = bIsKTX2;

    params.m_source_images.push_back(img);

    params.m_perceptual = EQUAL(
        CSLFetchNameValueDef(papszOptions, "COLORSPACE", "PERCEPTUAL_SRGB"),
        "PERCEPTUAL_SRGB");

    params.m_write_output_basis_files = true;

    std::string osTempFilename;
    bool bUseTempFilename = STARTS_WITH(pszFilename, "/vsi");
#ifdef _WIN32
    if (!bUseTempFilename)
    {
        bUseTempFilename = !CPLIsASCII(pszFilename, static_cast<size_t>(-1));
    }
#endif
    if (bUseTempFilename)
    {
        osTempFilename = CPLGenerateTempFilename(nullptr);
        CPLDebug("KTX2", "Using temporary file %s", osTempFilename.c_str());
        params.m_out_filename = osTempFilename;
    }
    else
    {
        params.m_out_filename = pszFilename;
    }

    params.m_uastc = EQUAL(
        CSLFetchNameValueDef(papszOptions, "COMPRESSION", "ETC1S"), "UASTC");
    if (params.m_uastc)
    {
        if (bIsKTX2)
        {
            const char *pszSuperCompression = CSLFetchNameValueDef(
                papszOptions, "UASTC_SUPER_COMPRESSION", "ZSTD");
            params.m_ktx2_uastc_supercompression =
                EQUAL(pszSuperCompression, "ZSTD") ? basist::KTX2_SS_ZSTANDARD
                                                   : basist::KTX2_SS_NONE;
        }

        const int nLevel =
            std::min(std::max(0, atoi(CSLFetchNameValueDef(
                                     papszOptions, "UASTC_LEVEL", "2"))),
                     static_cast<int>(basisu::TOTAL_PACK_UASTC_LEVELS - 1));
        static const uint32_t anLevelFlags[] = {
            basisu::cPackUASTCLevelFastest, basisu::cPackUASTCLevelFaster,
            basisu::cPackUASTCLevelDefault, basisu::cPackUASTCLevelSlower,
            basisu::cPackUASTCLevelVerySlow};
        CPL_STATIC_ASSERT(CPL_ARRAYSIZE(anLevelFlags) ==
                          basisu::TOTAL_PACK_UASTC_LEVELS);
        params.m_pack_uastc_flags &= ~basisu::cPackUASTCLevelMask;
        params.m_pack_uastc_flags |= anLevelFlags[nLevel];

        const char *pszUASTC_RDO_LEVEL =
            CSLFetchNameValue(papszOptions, "UASTC_RDO_LEVEL");
        if (pszUASTC_RDO_LEVEL)
        {
            params.m_rdo_uastc_quality_scalar =
                static_cast<float>(CPLAtof(pszUASTC_RDO_LEVEL));
            params.m_rdo_uastc = true;
        }

        for (const char *pszOption :
             {"ETC1S_LEVEL", "ETC1S_QUALITY_LEVEL",
              "ETC1S_MAX_SELECTOR_CLUSTERS", "ETC1S_MAX_SELECTOR_CLUSTERS"})
        {
            if (CSLFetchNameValue(papszOptions, pszOption) != nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s ignored for COMPRESSION=UASTC", pszOption);
            }
        }
    }
    else
    {
        // CPL_STATIC_ASSERT(basisu::BASISU_MIN_COMPRESSION_LEVEL == 0);
        CPL_STATIC_ASSERT(basisu::BASISU_MAX_COMPRESSION_LEVEL == 6);
        params.m_compression_level =
            std::min(std::max(0, atoi(CSLFetchNameValueDef(
                                     papszOptions, "ETC1S_LEVEL", "1"))),
                     static_cast<int>(basisu::BASISU_MAX_COMPRESSION_LEVEL));
        CPL_STATIC_ASSERT(basisu::BASISU_QUALITY_MIN == 1);
        CPL_STATIC_ASSERT(basisu::BASISU_QUALITY_MAX == 255);
        const char *pszQualityLevel =
            CSLFetchNameValue(papszOptions, "ETC1S_QUALITY_LEVEL");
        params.m_quality_level =
            std::min(std::max(static_cast<int>(basisu::BASISU_QUALITY_MIN),
                              atoi(pszQualityLevel ? pszQualityLevel : "128")),
                     static_cast<int>(basisu::BASISU_QUALITY_MAX));
        params.m_max_endpoint_clusters = 0;
        params.m_max_selector_clusters = 0;

        const char *pszMaxEndpointClusters =
            CSLFetchNameValue(papszOptions, "ETC1S_MAX_ENDPOINTS_CLUSTERS");
        const char *pszMaxSelectorClusters =
            CSLFetchNameValue(papszOptions, "ETC1S_MAX_SELECTOR_CLUSTERS");
        if (pszQualityLevel == nullptr && (pszMaxEndpointClusters != nullptr ||
                                           pszMaxSelectorClusters != nullptr))
        {
            params.m_quality_level = -1;
            if (pszMaxEndpointClusters != nullptr)
            {
                params.m_max_endpoint_clusters = atoi(pszMaxEndpointClusters);
                if (pszMaxSelectorClusters == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ETC1S_MAX_SELECTOR_CLUSTERS must be set when "
                             "ETC1S_MAX_ENDPOINTS_CLUSTERS is set");
                    return false;
                }
            }

            if (pszMaxSelectorClusters != nullptr)
            {
                params.m_max_selector_clusters = atoi(pszMaxSelectorClusters);
                if (pszMaxEndpointClusters == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ETC1S_MAX_ENDPOINTS_CLUSTERS must be set when "
                             "ETC1S_MAX_SELECTOR_CLUSTERS is set");
                    return false;
                }
            }
        }
        else
        {
            for (const char *pszOption : {"ETC1S_MAX_ENDPOINTS_CLUSTERS",
                                          "ETC1S_MAX_SELECTOR_CLUSTERS"})
            {
                if (CSLFetchNameValue(papszOptions, pszOption) != nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s ignored when ETC1S_QUALITY_LEVEL is specified",
                             pszOption);
                }
            }
        }

        for (const char *pszOption : {"UASTC_LEVEL", "UASTC_RDO_LEVEL"})
        {
            if (CSLFetchNameValue(papszOptions, pszOption) != nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s ignored for COMPRESSION=ETC1S", pszOption);
            }
        }
    }

    if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "MIPMAP", "NO")))
    {
        params.m_mip_gen = true;
        params.m_mip_srgb = params.m_perceptual;
    }

    const int nNumThreads = std::max(
        1, atoi(CSLFetchNameValueDef(
               papszOptions, "NUM_THREADS",
               CPLGetConfigOption("GDAL_NUM_THREADS",
                                  CPLSPrintf("%d", CPLGetNumCPUs())))));
    CPLDebug("KTX2", "Using %d threads", nNumThreads);
    if (params.m_uastc)
    {
        params.m_rdo_uastc_multithreading = nNumThreads > 1;
    }
    params.m_multithreading = nNumThreads > 1;
    params.m_debug = bVerbose;
    params.m_status_output = bVerbose;
    params.m_compute_stats = bVerbose;

    basisu::job_pool jpool(nNumThreads);
    params.m_pJob_pool = &jpool;

    basisu::basis_compressor comp;
    basisu::enable_debug_printf(bVerbose);

    if (!comp.init(params))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basis_compressor::init() failed");
        return false;
    }

    const basisu::basis_compressor::error_code result = comp.process();
    if (result != basisu::basis_compressor::cECSuccess)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basis_compressor::process() failed");
        return false;
    }

    if (!osTempFilename.empty())
    {
        if (CPLCopyFile(pszFilename, osTempFilename.c_str()) != 0)
        {
            VSIUnlink(osTempFilename.c_str());
            return false;
        }
        VSIUnlink(osTempFilename.c_str());
    }

    if (pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    return true;
}
