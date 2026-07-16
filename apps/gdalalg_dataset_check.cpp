/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset check" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_dataset_check.h"

#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal_dataset.h"
#include "gdal_multidim.h"
#include "gdal_rasterband.h"
#include "ogrsf_frmts.h"
#include "ogr_recordbatch.h"

#include <algorithm>
#include <limits>

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                     GDALDatasetCheckAlgorithm()                      */
/************************************************************************/

GDALDatasetCheckAlgorithm::GDALDatasetCheckAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddInputDatasetArg(&m_input, GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                     GDAL_OF_MULTIDIM_RASTER);
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats);

    AddArg("return-code", 0, _("Return code"), &m_retCode)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*                 GDALDatasetCheckAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALDatasetCheckAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    auto poDS = m_input.GetDatasetRef();
    CPLAssert(poDS);

    const CPLStringList aosOpenOptions(m_openOptions);
    const CPLStringList aosAllowedDrivers(m_inputFormats);

    const CPLStringList aosSubdatasets(
        CSLDuplicate(poDS->GetMetadata("SUBDATASETS")));
    const int nSubdatasets = aosSubdatasets.size() / 2;

    bool bRet = true;
    if (nSubdatasets)
    {
        int i = 0;
        for (auto [pszKey, pszValue] : cpl::IterateNameValue(aosSubdatasets))
        {
            if (cpl::ends_with(std::string_view(pszKey), "_NAME"))
            {
                auto poSubDS = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(pszValue, 0, aosAllowedDrivers.List(),
                                      aosOpenOptions.List()));
                if (poSubDS)
                {
                    std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                        pScaled(GDALCreateScaledProgress(
                                    static_cast<double>(i) / nSubdatasets,
                                    static_cast<double>(i + 1) / nSubdatasets,
                                    pfnProgress, pProgressData),
                                GDALDestroyScaledProgress);
                    ++i;
                    bRet = CheckDataset(poSubDS.get(), false,
                                        GDALScaledProgress, pScaled.get());
                    if (!bRet)
                        break;
                }
                else
                {
                    m_retCode = 1;
                }
            }
        }
    }
    else
    {
        bRet = CheckDataset(poDS, /* bRasterOnly=*/false, pfnProgress,
                            pProgressData);
    }

    return bRet;
}

/************************************************************************/
/*                         GetGroupPixelCount()                         */
/************************************************************************/

static GIntBig GetGroupPixelCount(const GDALGroup *poGroup)
{
    GIntBig nPixelCount = 0;
    for (const std::string &osArrayName : poGroup->GetMDArrayNames())
    {
        auto poArray = poGroup->OpenMDArray(osArrayName);
        if (poArray)
        {
            GIntBig nPixels = 1;
            for (auto &poDim : poArray->GetDimensions())
                nPixels *= poDim->GetSize();
            nPixelCount += nPixels;
        }
    }
    for (const std::string &osGroupName : poGroup->GetGroupNames())
    {
        auto poSubGroup = poGroup->OpenGroup(osGroupName);
        if (poSubGroup)
            nPixelCount += GetGroupPixelCount(poSubGroup.get());
    }
    return nPixelCount;
}

/************************************************************************/
/*                            ProgressStruct                            */
/************************************************************************/

namespace
{
struct ProgressStruct
{
    GIntBig nTotalContent = 0;
    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressData = nullptr;

    // In-out variable
    GIntBig nProgress = 0;

    // Work variable
    std::vector<GByte> *pabyData = nullptr;

    // Output variables
    bool bError = false;
    bool bInterrupted = false;
};
}  // namespace

/************************************************************************/
/*                         MDArrayProcessFunc()                         */
/************************************************************************/

/** Read a chunk of a multidimensional array */
static bool MDArrayProcessFunc(GDALAbstractMDArray *array,
                               const GUInt64 *startIdx,
                               const size_t *chunkCount,
                               GUInt64 /* iCurChunk */,
                               GUInt64 /* nChunkCount */, void *pUserData)
{
    ProgressStruct *psProgress = static_cast<ProgressStruct *>(pUserData);
    size_t nPixels = 1;
    const auto nDimCount = array->GetDimensionCount();
    for (size_t i = 0; i < nDimCount; ++i)
        nPixels *= chunkCount[i];
    auto &dt = array->GetDataType();
    const size_t nDTSize = dt.GetSize();
    const size_t nReqSize = nPixels * nDTSize;
    if (psProgress->pabyData->size() < nReqSize)
    {
        try
        {
            psProgress->pabyData->resize(nReqSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Out of memory while allocating memory chunk");
            psProgress->bError = true;
            return false;
        }
    }
    if (!array->Read(startIdx, chunkCount, /* arrayStep = */ nullptr,
                     /* bufferStride = */ nullptr, dt,
                     psProgress->pabyData->data()))
    {
        psProgress->bError = true;
        return false;
    }
    if (dt.NeedsFreeDynamicMemory())
    {
        for (size_t i = 0; i < nPixels; ++i)
        {
            dt.FreeDynamicMemory(psProgress->pabyData->data() + i * nDTSize);
        }
    }
    psProgress->nProgress += nPixels;
    if (psProgress->pfnProgress &&
        !psProgress->pfnProgress(
            static_cast<double>(psProgress->nProgress) /
                static_cast<double>(psProgress->nTotalContent),
            "", psProgress->pProgressData))
    {
        psProgress->bInterrupted = true;
        return false;
    }
    return true;
}

/************************************************************************/
/*               GDALDatasetCheckAlgorithm::CheckGroup()                */
/************************************************************************/

bool GDALDatasetCheckAlgorithm::CheckGroup(GDALGroup *poGroup,
                                           GIntBig &nProgress,
                                           GIntBig nTotalContent,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    CPLDebug("GDALDatasetCheckAlgorithm", "Checking group %s",
             poGroup->GetFullName().c_str());
    for (const std::string &osArrayName : poGroup->GetMDArrayNames())
    {
        auto poArray = poGroup->OpenMDArray(osArrayName);
        if (poArray)
        {
            CPLDebug("GDALDatasetCheckAlgorithm", "Checking array %s",
                     poArray->GetFullName().c_str());
            std::vector<GUInt64> anStartIdx(poArray->GetDimensionCount());
            std::vector<GUInt64> anCount;
            for (auto &poDim : poArray->GetDimensions())
                anCount.push_back(poDim->GetSize());
            constexpr size_t BUFFER_SIZE = 10 * 1024 * 1024;

            std::vector<GByte> abyData;

            ProgressStruct sProgress;
            sProgress.pabyData = &abyData;
            sProgress.nProgress = nProgress;
            sProgress.nTotalContent = nTotalContent;
            sProgress.pfnProgress = pfnProgress;
            sProgress.pProgressData = pProgressData;
            if (!poArray->ProcessPerChunk(
                    anStartIdx.data(), anCount.data(),
                    poArray->GetProcessingChunkSize(BUFFER_SIZE).data(),
                    MDArrayProcessFunc, &sProgress) ||
                sProgress.bError)
            {
                if (sProgress.bInterrupted)
                {
                    ReportError(CE_Failure, CPLE_UserInterrupt,
                                "Interrupted by user");
                }
                m_retCode = 1;
                return false;
            }
            nProgress = sProgress.nProgress;
        }
    }
    for (const std::string &osGroupName : poGroup->GetGroupNames())
    {
        auto poSubGroup = poGroup->OpenGroup(osGroupName);
        if (poSubGroup &&
            !CheckGroup(poSubGroup.get(), nProgress, nTotalContent, pfnProgress,
                        pProgressData))
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*              GDALDatasetCheckAlgorithm::CheckDataset()               */
/************************************************************************/

bool GDALDatasetCheckAlgorithm::CheckDataset(GDALDataset *poDS,
                                             bool bRasterOnly,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    const int nBands = poDS->GetRasterCount();
    auto poRootGroup = poDS->GetRootGroup();
    const GIntBig nTotalPixelsMD =
        poRootGroup ? GetGroupPixelCount(poRootGroup.get()) : 0;
    const GIntBig nTotalPixelsRegularRaster =
        nTotalPixelsMD ? 0
                       : static_cast<GIntBig>(nBands) * poDS->GetRasterXSize() *
                             poDS->GetRasterYSize();
    GIntBig nTotalFeatures = 0;
    bool bFastArrow = true;
    if (!bRasterOnly)
    {
        for (auto *poLayer : poDS->GetLayers())
        {
            bFastArrow =
                bFastArrow && poLayer->TestCapability(OLCFastGetArrowStream);
            const auto nFeatures = poLayer->GetFeatureCount(false);
            if (nFeatures >= 0)
                nTotalFeatures += nFeatures;
        }
    }

    // Totally arbitrary "equivalence" between a vector feature and a pixel
    // in terms of computation / I/O effort.
    constexpr int RATIO_FEATURE_TO_PIXEL = 100;
    const GIntBig nTotalContent = nTotalPixelsMD + nTotalPixelsRegularRaster +
                                  nTotalFeatures * RATIO_FEATURE_TO_PIXEL;

    if (!bRasterOnly)
    {
        const double dfRatioFeatures =
            (nTotalFeatures == nTotalContent)
                ? 1.0
                : static_cast<double>(nTotalFeatures * RATIO_FEATURE_TO_PIXEL) /
                      nTotalContent;

        if (bFastArrow)
        {
            GIntBig nCountFeatures = 0;
            for (auto *poLayer : poDS->GetLayers())
            {
                struct ArrowArrayStream stream;
                if (!poLayer->GetArrowStream(&stream))
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "GetArrowStream() failed");
                    m_retCode = 1;
                    return false;
                }
                while (true)
                {
                    struct ArrowArray array;
                    int ret = stream.get_next(&stream, &array);
                    if (ret != 0 || CPLGetLastErrorType() == CE_Failure)
                    {
                        if (array.release)
                            array.release(&array);
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "ArrowArrayStream::get_next() failed");
                        m_retCode = 1;
                        stream.release(&stream);
                        return false;
                    }
                    if (array.release == nullptr)
                        break;
                    nCountFeatures += array.length;
                    array.release(&array);
                    const double dfPct = static_cast<double>(nCountFeatures) /
                                         (static_cast<double>(nTotalFeatures) +
                                          std::numeric_limits<double>::min()) *
                                         dfRatioFeatures;
                    if (pfnProgress && !pfnProgress(dfPct, "", pProgressData))
                    {
                        ReportError(CE_Failure, CPLE_UserInterrupt,
                                    "Interrupted by user");
                        m_retCode = 1;
                        stream.release(&stream);
                        return false;
                    }
                }
                stream.release(&stream);
            }
        }
        else
        {
            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaled(
                GDALCreateScaledProgress(0, dfRatioFeatures, pfnProgress,
                                         pProgressData),
                GDALDestroyScaledProgress);
            GIntBig nCurFeatures = 0;
            while (true)
            {
                const bool bGotFeature =
                    std::unique_ptr<OGRFeature>(poDS->GetNextFeature(
                        nullptr, nullptr, GDALScaledProgress, pScaled.get())) !=
                    nullptr;
                if (CPLGetLastErrorType() == CE_Failure)
                {
                    m_retCode = 1;
                    return false;
                }
                if (!bGotFeature)
                    break;
                ++nCurFeatures;
                if (pfnProgress && nTotalFeatures > 0 &&
                    !pfnProgress(
                        std::min(1.0, static_cast<double>(nCurFeatures) /
                                          static_cast<double>(nTotalFeatures)) *
                            dfRatioFeatures,
                        "", pProgressData))
                {
                    ReportError(CE_Failure, CPLE_UserInterrupt,
                                "Interrupted by user");
                    m_retCode = 1;
                    return false;
                }
            }
            if (pfnProgress && nTotalContent == 0)
                pfnProgress(1.0, "", pProgressData);
        }
    }

    GIntBig nProgress = nTotalFeatures * RATIO_FEATURE_TO_PIXEL;
    if (poRootGroup && nTotalPixelsMD)
    {
        return CheckGroup(poRootGroup.get(), nProgress, nTotalContent,
                          pfnProgress, pProgressData);
    }
    else if (nBands)
    {
        std::vector<GByte> abyBuffer;
        const auto eDT = poDS->GetRasterBand(1)->GetRasterDataType();
        const auto nDTSize = GDALGetDataTypeSizeBytes(eDT);
        constexpr size_t BUFFER_SIZE = 10 * 1024 * 1024;
        const char *pszInterleaving =
            poDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
        if (pszInterleaving && EQUAL(pszInterleaving, "PIXEL"))
        {
            for (const auto &oWindow :
                 poDS->GetRasterBand(1)->IterateWindows(BUFFER_SIZE))
            {
                const size_t nPixels = static_cast<size_t>(oWindow.nXSize) *
                                       oWindow.nYSize * nBands;
                const size_t nReqSize = nPixels * nDTSize;
                if (abyBuffer.size() < nReqSize)
                {
                    try
                    {
                        abyBuffer.resize(nReqSize);
                    }
                    catch (const std::exception &)
                    {
                        ReportError(
                            CE_Failure, CPLE_OutOfMemory,
                            "Out of memory while allocating memory chunk");
                        m_retCode = 1;
                        return false;
                    }
                }
                if (poDS->RasterIO(GF_Read, oWindow.nXOff, oWindow.nYOff,
                                   oWindow.nXSize, oWindow.nYSize,
                                   abyBuffer.data(), oWindow.nXSize,
                                   oWindow.nYSize, eDT, nBands, nullptr, 0, 0,
                                   0, nullptr) != CE_None ||
                    CPLGetLastErrorType() == CE_Failure)
                {
                    m_retCode = 1;
                    return false;
                }
                nProgress += nPixels;
                if (pfnProgress &&
                    !pfnProgress(static_cast<double>(nProgress) /
                                     static_cast<double>(
                                         std::max<GIntBig>(1, nTotalContent)),
                                 "", pProgressData))
                {
                    ReportError(CE_Failure, CPLE_UserInterrupt,
                                "Interrupted by user");
                    m_retCode = 1;
                    return false;
                }
            }
        }
        else
        {
            for (int iBand = 1; iBand <= nBands; ++iBand)
            {
                auto poBand = poDS->GetRasterBand(iBand);
                for (const auto &oWindow : poBand->IterateWindows(BUFFER_SIZE))
                {
                    const size_t nPixels =
                        static_cast<size_t>(oWindow.nXSize) * oWindow.nYSize;
                    const size_t nReqSize = nPixels * nDTSize;
                    if (abyBuffer.size() < nReqSize)
                    {
                        try
                        {
                            abyBuffer.resize(nReqSize);
                        }
                        catch (const std::exception &)
                        {
                            ReportError(
                                CE_Failure, CPLE_OutOfMemory,
                                "Out of memory while allocating memory chunk");
                            m_retCode = 1;
                            return false;
                        }
                    }
                    if (poBand->RasterIO(GF_Read, oWindow.nXOff, oWindow.nYOff,
                                         oWindow.nXSize, oWindow.nYSize,
                                         abyBuffer.data(), oWindow.nXSize,
                                         oWindow.nYSize, eDT, 0, 0,
                                         nullptr) != CE_None ||
                        CPLGetLastErrorType() == CE_Failure)
                    {
                        m_retCode = 1;
                        return false;
                    }
                    nProgress +=
                        static_cast<GIntBig>(oWindow.nXSize) * oWindow.nYSize;
                    if (pfnProgress &&
                        !pfnProgress(static_cast<double>(nProgress) /
                                         static_cast<double>(std::max<GIntBig>(
                                             1, nTotalContent)),
                                     "", pProgressData))
                    {
                        ReportError(CE_Failure, CPLE_UserInterrupt,
                                    "Interrupted by user");
                        m_retCode = 1;
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

//! @endcond
