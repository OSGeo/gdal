/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_compare.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"
#include "gdal_alg.h"
#include "gdal_priv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#if defined(__x86_64__) || defined(_M_X64)
#define USE_SSE2
#include <emmintrin.h>
#elif defined(USE_NEON_OPTIMIZATIONS)
#define USE_SSE2
#include "include_sse2neon.h"
#endif

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALRasterCompareAlgorithm::GDALRasterCompareAlgorithm()       */
/************************************************************************/

GDALRasterCompareAlgorithm::GDALRasterCompareAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetInputDatasetMaxCount(1)
              .SetInputDatasetHelpMsg(_("Input raster dataset"))
              .SetAddDefaultArguments(false))
{
    AddProgressArg();

    if (!standaloneStep)
    {
        AddRasterHiddenInputDatasetArg();
    }

    auto &referenceDatasetArg = AddArg("reference", 0, _("Reference dataset"),
                                       &m_referenceDataset, GDAL_OF_RASTER)
                                    .SetPositional()
                                    .SetRequired();

    SetAutoCompleteFunctionForFilename(referenceDatasetArg, GDAL_OF_RASTER);

    if (standaloneStep)
    {
        AddRasterInputArgs(/* openForMixedRasterVector = */ false,
                           /* hiddenForCLI = */ false);
    }

    AddArg("skip-all-optional", 0, _("Skip all optional comparisons"),
           &m_skipAllOptional);
    AddArg("skip-binary", 0, _("Skip binary file comparison"), &m_skipBinary);
    AddArg("skip-crs", 0, _("Skip CRS comparison"), &m_skipCRS);
    AddArg("skip-geotransform", 0, _("Skip geotransform comparison"),
           &m_skipGeotransform);
    AddArg("skip-overview", 0, _("Skip overview comparison"), &m_skipOverview);
    AddArg("skip-metadata", 0, _("Skip metadata comparison"), &m_skipMetadata);
    AddArg("skip-rpc", 0, _("Skip RPC metadata comparison"), &m_skipRPC);
    AddArg("skip-geolocation", 0, _("Skip Geolocation metadata comparison"),
           &m_skipGeolocation);
    AddArg("skip-subdataset", 0, _("Skip subdataset comparison"),
           &m_skipSubdataset);

    AddOutputStringArg(&m_output);

    AddArg("return-code", 0, _("Return code"), &m_retCode)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*            GDALRasterCompareAlgorithm::BinaryComparison()            */
/************************************************************************/

bool GDALRasterCompareAlgorithm::BinaryComparison(
    std::vector<std::string> &aosReport, GDALDataset *poRefDS,
    GDALDataset *poInputDS)
{
    if (poRefDS->GetDescription()[0] == 0)
    {
        ReportError(
            CE_Warning, CPLE_AppDefined,
            "Reference dataset has no name. Skipping binary file comparison");
        return false;
    }

    auto poRefDrv = poRefDS->GetDriver();
    if (poRefDrv && EQUAL(poRefDrv->GetDescription(), "MEM"))
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Reference dataset is a in-memory dataset. Skipping binary "
                    "file comparison");
        return false;
    }

    if (poInputDS->GetDescription()[0] == 0)
    {
        ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset has no name. Skipping binary file comparison");
        return false;
    }

    auto poInputDrv = poInputDS->GetDriver();
    if (poInputDrv && EQUAL(poInputDrv->GetDescription(), "MEM"))
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Input dataset is a in-memory dataset. Skipping binary "
                    "file comparison");
        return false;
    }

    VSIVirtualHandleUniquePtr fpRef(VSIFOpenL(poRefDS->GetDescription(), "rb"));
    VSIVirtualHandleUniquePtr fpInput(
        VSIFOpenL(poInputDS->GetDescription(), "rb"));
    if (!fpRef)
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Reference dataset '%s' is not a file. Skipping binary "
                    "file comparison",
                    poRefDS->GetDescription());
        return false;
    }

    if (!fpInput)
    {
        ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset '%s' is not a file. Skipping binary file comparison",
            poInputDS->GetDescription());
        return false;
    }

    fpRef->Seek(0, SEEK_END);
    fpInput->Seek(0, SEEK_END);
    const auto nRefSize = fpRef->Tell();
    const auto nInputSize = fpInput->Tell();
    if (nRefSize != nInputSize)
    {
        aosReport.push_back("Reference file has size " +
                            std::to_string(nRefSize) +
                            " bytes, whereas input file has size " +
                            std::to_string(nInputSize) + " bytes.");

        return false;
    }

    constexpr size_t BUF_SIZE = 1024 * 1024;
    std::vector<GByte> abyRef(BUF_SIZE);
    std::vector<GByte> abyInput(BUF_SIZE);

    fpRef->Seek(0, SEEK_SET);
    fpInput->Seek(0, SEEK_SET);

    do
    {
        const size_t nRefRead = fpRef->Read(abyRef.data(), 1, BUF_SIZE);
        const size_t nInputRead = fpInput->Read(abyInput.data(), 1, BUF_SIZE);

        if (nRefRead != BUF_SIZE && fpRef->Tell() != nRefSize)
        {
            aosReport.push_back("Failed to fully read reference file");
            return false;
        }

        if (nInputRead != BUF_SIZE && fpInput->Tell() != nRefSize)
        {
            aosReport.push_back("Failed to fully read input file");
            return false;
        }

        if (abyRef != abyInput)
        {
            aosReport.push_back(
                "Reference file and input file differ at the binary level.");
            return false;
        }
    } while (fpRef->Tell() < nRefSize);

    return true;
}

/************************************************************************/
/*             GDALRasterCompareAlgorithm::CRSComparison()              */
/************************************************************************/

void GDALRasterCompareAlgorithm::CRSComparison(
    std::vector<std::string> &aosReport, GDALDataset *poRefDS,
    GDALDataset *poInputDS)
{
    const auto poRefCRS = poRefDS->GetSpatialRef();
    const auto poInputCRS = poInputDS->GetSpatialRef();

    if (poRefCRS == nullptr)
    {
        if (poInputCRS)
        {
            aosReport.push_back(
                "Reference dataset has no CRS, but input dataset has one.");
        }
        return;
    }

    if (poInputCRS == nullptr)
    {
        aosReport.push_back(
            "Reference dataset has a CRS, but input dataset has none.");
        return;
    }

    if (poRefCRS->IsSame(poInputCRS))
        return;

    const char *apszOptions[] = {"FORMAT=WKT2_2019", nullptr};
    const auto poRefWKT = poRefCRS->exportToWkt(apszOptions);
    const auto poInputWKT = poInputCRS->exportToWkt(apszOptions);
    aosReport.push_back(
        "Reference and input CRS are not equivalent. Reference one is '" +
        poRefWKT + "'. Input one is '" + poInputWKT + "'");
}

/************************************************************************/
/*         GDALRasterCompareAlgorithm::GeotransformComparison()         */
/************************************************************************/

void GDALRasterCompareAlgorithm::GeoTransformComparison(
    std::vector<std::string> &aosReport, GDALDataset *poRefDS,
    GDALDataset *poInputDS)
{
    GDALGeoTransform refGT;
    CPLErr eErr1 = poRefDS->GetGeoTransform(refGT);
    GDALGeoTransform inputGT;
    CPLErr eErr2 = poInputDS->GetGeoTransform(inputGT);
    if (eErr1 == CE_Failure && eErr2 == CE_Failure)
        return;

    if (eErr1 == CE_Failure && eErr2 == CE_None)
    {
        aosReport.push_back(
            "Reference dataset has no geotransform, but input one has one.");
        return;
    }

    if (eErr1 == CE_None && eErr2 == CE_Failure)
    {
        aosReport.push_back(
            "Reference dataset has a geotransform, but input one has none.");
        return;
    }

    for (int i = 0; i < 6; ++i)
    {
        if ((refGT[i] != 0 &&
             std::fabs(refGT[i] - inputGT[i]) > 1e-10 * std::fabs(refGT[i])) ||
            (refGT[i] == 0 && std::fabs(refGT[i] - inputGT[i]) > 1e-10))
        {
            std::string s = "Geotransform of reference and input dataset are "
                            "not equivalent. Reference geotransform is (";
            for (int j = 0; j < 6; ++j)
            {
                if (j > 0)
                    s += ',';
                s += std::to_string(refGT[j]);
            }
            s += "). Input geotransform is (";
            for (int j = 0; j < 6; ++j)
            {
                if (j > 0)
                    s += ',';
                s += std::to_string(inputGT[j]);
            }
            s += ')';
            aosReport.push_back(std::move(s));
            return;
        }
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3")
#endif

/************************************************************************/
/*                                Diff()                                */
/************************************************************************/

template <class T> CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW static T Diff(T a, T b)
{
    return a - b;
}

/************************************************************************/
/*                           CompareVectors()                           */
/************************************************************************/

template <class T, class Tdiff, bool bIsComplex>
static void CompareVectors(size_t nValCount, const T *refValues,
                           const T *inputValues, uint64_t &countDiffPixels,
                           Tdiff &maxDiffValue)
{
    constexpr bool bIsFloatingPoint = std::is_floating_point_v<T>;
    if constexpr (bIsComplex)
    {
        for (size_t i = 0; i < nValCount; ++i)
        {
            if constexpr (bIsFloatingPoint)
            {
                static_assert(std::is_same_v<T, Tdiff>);
                if (std::isnan(refValues[2 * i]) &&
                    std::isnan(inputValues[2 * i]) &&
                    std::isnan(refValues[2 * i + 1]) &&
                    std::isnan(inputValues[2 * i + 1]))
                {
                    continue;
                }
            }

            if (refValues[2 * i] != inputValues[2 * i] ||
                refValues[2 * i + 1] != inputValues[2 * i + 1])
            {
                const Tdiff diff =
                    std::hypot(static_cast<Tdiff>(refValues[2 * i]) -
                                   static_cast<Tdiff>(inputValues[2 * i]),
                               static_cast<Tdiff>(refValues[2 * i + 1]) -
                                   static_cast<Tdiff>(inputValues[2 * i + 1]));
                ++countDiffPixels;
                if (diff > maxDiffValue)
                    maxDiffValue = diff;
            }
        }
    }
    else
    {
        static_assert(sizeof(Tdiff) == sizeof(T));
        size_t i = 0;
#ifdef USE_SSE2
        if constexpr (std::is_same_v<T, float>)
        {
            static_assert(std::is_same_v<T, Tdiff>);

            auto vMaxDiff = _mm_setzero_ps();

            // Mask for absolute value (clears the sign bit)
            const auto absMask = _mm_castsi128_ps(
                _mm_set1_epi32(std::numeric_limits<int32_t>::max()));

            constexpr size_t VALS_PER_REG = sizeof(vMaxDiff) / sizeof(T);
            while (i + VALS_PER_REG <= nValCount)
            {
                auto vCountDiff = _mm_setzero_si128();

                // We can do a maximum of std::numeric_limits<uint32_t>::max()
                // accumulations into vCountDiff
                const size_t nInnerLimit = [i, nValCount](size_t valsPerReg)
                {
                    if constexpr (sizeof(size_t) > sizeof(uint32_t))
                    {
                        return std::min(
                            nValCount - valsPerReg,
                            i + std::numeric_limits<uint32_t>::max() *
                                    valsPerReg);
                    }
                    else
                    {
                        return nValCount - valsPerReg;
                    }
                }(VALS_PER_REG);

                for (; i <= nInnerLimit; i += VALS_PER_REG)
                {
                    const auto a = _mm_loadu_ps(refValues + i);
                    const auto b = _mm_loadu_ps(inputValues + i);

                    // Compute absolute value of difference
                    const auto absDiff = _mm_and_ps(_mm_sub_ps(a, b), absMask);

                    // Update vMaxDiff
                    const auto aIsNan = _mm_cmpunord_ps(a, a);
                    const auto bIsNan = _mm_cmpunord_ps(b, b);
                    const auto valNotEqual = _mm_andnot_ps(
                        _mm_or_ps(aIsNan, bIsNan), _mm_cmpneq_ps(a, b));
                    vMaxDiff =
                        _mm_max_ps(vMaxDiff, _mm_and_ps(absDiff, valNotEqual));

                    // Update vCountDiff
                    const auto nanMisMatch = _mm_xor_ps(aIsNan, bIsNan);
                    // if nanMisMatch OR (both values not NaN and a != b)
                    const auto maskIsDiff = _mm_or_ps(nanMisMatch, valNotEqual);
                    const auto shiftedMaskDiff =
                        _mm_srli_epi32(_mm_castps_si128(maskIsDiff), 31);
                    vCountDiff = _mm_add_epi32(vCountDiff, shiftedMaskDiff);
                }

                // Horizontal add into countDiffPixels
                uint32_t anCountDiff[VALS_PER_REG];
                _mm_storeu_si128(reinterpret_cast<__m128i *>(anCountDiff),
                                 vCountDiff);
                for (size_t j = 0; j < VALS_PER_REG; ++j)
                {
                    countDiffPixels += anCountDiff[j];
                }
            }

            // Horizontal max into maxDiffValue
            float afMaxDiffValue[VALS_PER_REG];
            _mm_storeu_ps(afMaxDiffValue, vMaxDiff);
            for (size_t j = 0; j < VALS_PER_REG; ++j)
            {
                CPLAssert(!std::isnan(afMaxDiffValue[j]));
                maxDiffValue = std::max(maxDiffValue, afMaxDiffValue[j]);
            }
        }
#endif
        if constexpr (bIsFloatingPoint)
        {
            static_assert(std::is_same_v<T, Tdiff>);
            for (; i < nValCount; ++i)
            {
                if (std::isnan(refValues[i]))
                {
                    if (!std::isnan(inputValues[i]))
                    {
                        ++countDiffPixels;
                    }
                    continue;
                }
                else if (std::isnan(inputValues[i]))
                {
                    ++countDiffPixels;
                    continue;
                }
                else if (refValues[i] == inputValues[i])
                {
                    continue;
                }

                const Tdiff diff =
                    refValues[i] >= inputValues[i]
                        ? Diff(static_cast<Tdiff>(refValues[i]),
                               static_cast<Tdiff>(inputValues[i]))
                        : Diff(static_cast<Tdiff>(inputValues[i]),
                               static_cast<Tdiff>(refValues[i]));
                if (diff > 0)
                {
                    ++countDiffPixels;
                    if (diff > maxDiffValue)
                        maxDiffValue = diff;
                }
            }
        }
        else
        {
            static_assert(std::is_unsigned_v<Tdiff>);
            while (i < nValCount)
            {
                // Autovectorizer friendly inner loop (GCC, clang, ICX),
                // by making sure it increases countDiffLocal on the same size
                // as Tdiff.

                Tdiff countDiffLocal = 0;
                const size_t innerLimit = [i, nValCount]()
                {
                    if constexpr (sizeof(Tdiff) < sizeof(size_t))
                    {
                        return std::min(nValCount - 1,
                                        i + std::numeric_limits<Tdiff>::max());
                    }
                    else
                    {
                        (void)i;
                        return nValCount - 1;
                    }
                }();
                for (; i <= innerLimit; ++i)
                {
                    const Tdiff diff =
                        refValues[i] >= inputValues[i]
                            ? Diff(static_cast<Tdiff>(refValues[i]),
                                   static_cast<Tdiff>(inputValues[i]))
                            : Diff(static_cast<Tdiff>(inputValues[i]),
                                   static_cast<Tdiff>(refValues[i]));
                    countDiffLocal += (diff > 0);
                    maxDiffValue = std::max(maxDiffValue, diff);
                }
                countDiffPixels += countDiffLocal;
            }
        }
    }
}

/************************************************************************/
/*                       DatasetPixelComparison()                       */
/************************************************************************/

template <class T, class Tdiff, bool bIsComplex>
static void DatasetPixelComparison(std::vector<std::string> &aosReport,
                                   GDALDataset *poRefDS, GDALDataset *poInputDS,
                                   GDALDataType eReqDT,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    std::vector<T> refValues;
    std::vector<T> inputValues;

    CPLAssert(GDALDataTypeIsComplex(eReqDT) == bIsComplex);

    const uint64_t nTotalPixels =
        static_cast<uint64_t>(poRefDS->GetRasterXSize()) *
        poRefDS->GetRasterYSize();
    uint64_t nIterPixels = 0;

    constexpr int nValPerPixel = bIsComplex ? 2 : 1;
    const int nBands = poRefDS->GetRasterCount();

    std::vector<Tdiff> maxDiffValue(nBands, 0);
    std::vector<uint64_t> countDiffPixels(nBands, 0);

    size_t nMaxSize = 0;
    const GIntBig nUsableRAM = CPLGetUsablePhysicalRAM() / 10;
    if (nUsableRAM > 0)
        nMaxSize = static_cast<size_t>(nUsableRAM);

    for (const auto &window : GDALRasterBand::WindowIteratorWrapper(
             *(poRefDS->GetRasterBand(1)), *(poInputDS->GetRasterBand(1)),
             nMaxSize))
    {
        const size_t nValCount =
            static_cast<size_t>(window.nXSize) * window.nYSize;
        const size_t nArraySize = nValCount * nValPerPixel * nBands;
        try
        {
            if (refValues.size() < nArraySize)
            {
                refValues.resize(nArraySize);
                inputValues.resize(nArraySize);
            }
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary arrays");
            aosReport.push_back("Out of memory allocating temporary arrays");
            return;
        }

        if (poRefDS->RasterIO(GF_Read, window.nXOff, window.nYOff,
                              window.nXSize, window.nYSize, refValues.data(),
                              window.nXSize, window.nYSize, eReqDT, nBands,
                              nullptr, 0, 0, 0, nullptr) == CE_None &&
            poInputDS->RasterIO(
                GF_Read, window.nXOff, window.nYOff, window.nXSize,
                window.nYSize, inputValues.data(), window.nXSize, window.nYSize,
                eReqDT, nBands, nullptr, 0, 0, 0, nullptr) == CE_None)
        {
            for (int i = 0; i < nBands; ++i)
            {
                CompareVectors<T, Tdiff, bIsComplex>(
                    nValCount, refValues.data() + i * nValCount * nValPerPixel,
                    inputValues.data() + i * nValCount * nValPerPixel,
                    countDiffPixels[i], maxDiffValue[i]);
            }
        }
        else
        {
            aosReport.push_back("I/O error when comparing pixel values");
        }

        if (pfnProgress)
        {
            nIterPixels += nValCount;
            if (!pfnProgress(static_cast<double>(nIterPixels) /
                                 static_cast<double>(nTotalPixels),
                             "", pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
                break;
            }
        }
    }
    for (int i = 0; i < nBands; ++i)
    {
        if (countDiffPixels[i])
        {
            aosReport.push_back(
                "Band " + std::to_string(i + 1) +
                ": pixels differing: " + std::to_string(countDiffPixels[i]));
            aosReport.push_back("Band " + std::to_string(i + 1) +
                                ": maximum pixel value difference: " +
                                std::to_string(maxDiffValue[i]));
        }
    }
}

/************************************************************************/
/*           GDALRasterCompareAlgorithm::DatasetComparison()            */
/************************************************************************/

void GDALRasterCompareAlgorithm::DatasetComparison(
    std::vector<std::string> &aosReport, GDALDataset *poRefDS,
    GDALDataset *poInputDS, GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (!m_skipCRS)
    {
        CRSComparison(aosReport, poRefDS, poInputDS);
    }

    if (!m_skipGeotransform)
    {
        GeoTransformComparison(aosReport, poRefDS, poInputDS);
    }

    bool ret = true;
    if (poRefDS->GetRasterCount() != poInputDS->GetRasterCount())
    {
        aosReport.push_back("Reference dataset has " +
                            std::to_string(poRefDS->GetRasterCount()) +
                            " band(s), but input dataset has " +
                            std::to_string(poInputDS->GetRasterCount()));
        ret = false;
    }

    if (poRefDS->GetRasterXSize() != poInputDS->GetRasterXSize())
    {
        aosReport.push_back("Reference dataset width is " +
                            std::to_string(poRefDS->GetRasterXSize()) +
                            ", but input dataset width is " +
                            std::to_string(poInputDS->GetRasterXSize()));
        ret = false;
    }

    if (poRefDS->GetRasterYSize() != poInputDS->GetRasterYSize())
    {
        aosReport.push_back("Reference dataset height is " +
                            std::to_string(poRefDS->GetRasterYSize()) +
                            ", but input dataset height is " +
                            std::to_string(poInputDS->GetRasterYSize()));
        ret = false;
    }

    if (!m_skipMetadata)
    {
        MetadataComparison(aosReport, "(dataset default metadata domain)",
                           poRefDS->GetMetadata(), poInputDS->GetMetadata());
    }

    if (!m_skipRPC)
    {
        MetadataComparison(aosReport, "RPC", poRefDS->GetMetadata("RPC"),
                           poInputDS->GetMetadata("RPC"));
    }

    if (!m_skipGeolocation)
    {
        MetadataComparison(aosReport, "GEOLOCATION",
                           poRefDS->GetMetadata("GEOLOCATION"),
                           poInputDS->GetMetadata("GEOLOCATION"));
    }

    if (!ret)
        return;

    const int nBands = poRefDS->GetRasterCount();

    bool doBandBasedPixelComparison = true;
    // Do not do band-by-band pixel difference if there are too many interleaved
    // bands as this could be extremely slow
    if (nBands > 10)
    {
        const char *pszRefInterleave =
            poRefDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
        const char *pszInputInterleave =
            poInputDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
        if ((pszRefInterleave && EQUAL(pszRefInterleave, "PIXEL")) ||
            (pszInputInterleave && EQUAL(pszInputInterleave, "PIXEL")))
        {
            doBandBasedPixelComparison = false;
        }
    }

    for (int i = 0; i < nBands; ++i)
    {
        void *pScaledProgress = GDALCreateScaledProgress(
            static_cast<double>(i) / nBands,
            static_cast<double>(i + 1) / nBands, pfnProgress, pProgressData);
        BandComparison(
            aosReport, std::to_string(i + 1), doBandBasedPixelComparison,
            poRefDS->GetRasterBand(i + 1), poInputDS->GetRasterBand(i + 1),
            pScaledProgress ? GDALScaledProgress : nullptr, pScaledProgress);
        GDALDestroyScaledProgress(pScaledProgress);
    }

    if (!doBandBasedPixelComparison)
    {
        const auto eReqDT =
            GDALDataTypeUnion(poRefDS->GetRasterBand(1)->GetRasterDataType(),
                              poInputDS->GetRasterBand(1)->GetRasterDataType());
        switch (eReqDT)
        {
            case GDT_UInt8:
                DatasetPixelComparison<uint8_t, uint8_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Int8:
                DatasetPixelComparison<int8_t, uint8_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_UInt16:
                DatasetPixelComparison<uint16_t, uint16_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Int16:
                DatasetPixelComparison<int16_t, uint16_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_UInt32:
                DatasetPixelComparison<uint32_t, uint32_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Int32:
                DatasetPixelComparison<int32_t, uint32_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_UInt64:
                DatasetPixelComparison<uint64_t, uint64_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Int64:
                DatasetPixelComparison<int64_t, uint64_t, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Float16:
            case GDT_Float32:
                DatasetPixelComparison<float, float, false>(
                    aosReport, poRefDS, poInputDS, GDT_Float32, pfnProgress,
                    pProgressData);
                break;
            case GDT_Float64:
                DatasetPixelComparison<double, double, false>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_CInt16:
                DatasetPixelComparison<int16_t, float, true>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_CInt32:
                DatasetPixelComparison<int32_t, double, true>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_CFloat16:
            case GDT_CFloat32:
                DatasetPixelComparison<float, float, true>(
                    aosReport, poRefDS, poInputDS, GDT_CFloat32, pfnProgress,
                    pProgressData);
                break;
            case GDT_CFloat64:
                DatasetPixelComparison<double, double, true>(
                    aosReport, poRefDS, poInputDS, eReqDT, pfnProgress,
                    pProgressData);
                break;
            case GDT_Unknown:
            case GDT_TypeCount:
                break;
        }
    }
}

/************************************************************************/
/*                           ComparePixels()                            */
/************************************************************************/

template <class T, class Tdiff, bool bIsComplex>
static void ComparePixels(std::vector<std::string> &aosReport,
                          const std::string &bandId, GDALRasterBand *poRefBand,
                          GDALRasterBand *poInputBand, GDALDataType eReqDT,
                          GDALProgressFunc pfnProgress, void *pProgressData)
{
    std::vector<T> refValues;
    std::vector<T> inputValues;
    Tdiff maxDiffValue = 0;
    uint64_t countDiffPixels = 0;

    CPLAssert(GDALDataTypeIsComplex(eReqDT) == bIsComplex);
    const uint64_t nTotalPixels =
        static_cast<uint64_t>(poRefBand->GetXSize()) * poRefBand->GetYSize();
    uint64_t nIterPixels = 0;

    constexpr int nValPerPixel = bIsComplex ? 2 : 1;

    size_t nMaxSize = 0;
    const GIntBig nUsableRAM = CPLGetUsablePhysicalRAM() / 10;
    if (nUsableRAM > 0)
        nMaxSize = static_cast<size_t>(nUsableRAM);

    for (const auto &window : GDALRasterBand::WindowIteratorWrapper(
             *poRefBand, *poInputBand, nMaxSize))
    {
        const size_t nValCount =
            static_cast<size_t>(window.nXSize) * window.nYSize;
        const size_t nArraySize = nValCount * nValPerPixel;
        try
        {
            if (refValues.size() < nArraySize)
            {
                refValues.resize(nArraySize);
                inputValues.resize(nArraySize);
            }
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary arrays");
            aosReport.push_back("Out of memory allocating temporary arrays");
            return;
        }

        if (poRefBand->RasterIO(GF_Read, window.nXOff, window.nYOff,
                                window.nXSize, window.nYSize, refValues.data(),
                                window.nXSize, window.nYSize, eReqDT, 0, 0,
                                nullptr) == CE_None &&
            poInputBand->RasterIO(
                GF_Read, window.nXOff, window.nYOff, window.nXSize,
                window.nYSize, inputValues.data(), window.nXSize, window.nYSize,
                eReqDT, 0, 0, nullptr) == CE_None)
        {
            CompareVectors<T, Tdiff, bIsComplex>(nValCount, refValues.data(),
                                                 inputValues.data(),
                                                 countDiffPixels, maxDiffValue);
        }
        else
        {
            aosReport.push_back("I/O error when comparing pixel values");
        }

        if (pfnProgress)
        {
            nIterPixels += nValCount;
            if (!pfnProgress(static_cast<double>(nIterPixels) /
                                 static_cast<double>(nTotalPixels),
                             "", pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
                break;
            }
        }
    }
    if (countDiffPixels)
    {
        aosReport.push_back(
            bandId + ": pixels differing: " + std::to_string(countDiffPixels));

        std::string reportMessage(bandId);
        reportMessage += ": maximum pixel value difference: ";
        if constexpr (std::is_floating_point_v<T>)
        {
            if (std::isinf(maxDiffValue))
                reportMessage += "inf";
            else if (std::isnan(maxDiffValue))
                reportMessage += "nan";
            else
                reportMessage += std::to_string(maxDiffValue);
        }
        else
        {
            reportMessage += std::to_string(maxDiffValue);
        }
        aosReport.push_back(std::move(reportMessage));
    }
}

/************************************************************************/
/*                           ComparePixels()                            */
/************************************************************************/

static void ComparePixels(std::vector<std::string> &aosReport,
                          const std::string &bandId, GDALRasterBand *poRefBand,
                          GDALRasterBand *poInputBand,
                          GDALProgressFunc pfnProgress, void *pProgressData)
{
    const auto eReqDT = GDALDataTypeUnion(poRefBand->GetRasterDataType(),
                                          poInputBand->GetRasterDataType());
    switch (eReqDT)
    {
        case GDT_UInt8:
            ComparePixels<uint8_t, uint8_t, false>(aosReport, bandId, poRefBand,
                                                   poInputBand, eReqDT,
                                                   pfnProgress, pProgressData);
            break;
        case GDT_Int8:
            ComparePixels<int8_t, uint8_t, false>(aosReport, bandId, poRefBand,
                                                  poInputBand, eReqDT,
                                                  pfnProgress, pProgressData);
            break;
        case GDT_UInt16:
            ComparePixels<uint16_t, uint16_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_Int16:
            ComparePixels<int16_t, uint16_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_UInt32:
            ComparePixels<uint32_t, uint32_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_Int32:
            ComparePixels<int32_t, uint32_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_UInt64:
            ComparePixels<uint64_t, uint64_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_Int64:
            ComparePixels<int64_t, uint64_t, false>(
                aosReport, bandId, poRefBand, poInputBand, eReqDT, pfnProgress,
                pProgressData);
            break;
        case GDT_Float16:
        case GDT_Float32:
            ComparePixels<float, float, false>(aosReport, bandId, poRefBand,
                                               poInputBand, GDT_Float32,
                                               pfnProgress, pProgressData);
            break;
        case GDT_Float64:
            ComparePixels<double, double, false>(aosReport, bandId, poRefBand,
                                                 poInputBand, eReqDT,
                                                 pfnProgress, pProgressData);
            break;
        case GDT_CInt16:
            ComparePixels<int16_t, float, true>(aosReport, bandId, poRefBand,
                                                poInputBand, eReqDT,
                                                pfnProgress, pProgressData);
            break;
        case GDT_CInt32:
            ComparePixels<int32_t, double, true>(aosReport, bandId, poRefBand,
                                                 poInputBand, eReqDT,
                                                 pfnProgress, pProgressData);
            break;
        case GDT_CFloat16:
        case GDT_CFloat32:
            ComparePixels<float, float, true>(aosReport, bandId, poRefBand,
                                              poInputBand, GDT_CFloat32,
                                              pfnProgress, pProgressData);
            break;
        case GDT_CFloat64:
            ComparePixels<double, double, true>(aosReport, bandId, poRefBand,
                                                poInputBand, eReqDT,
                                                pfnProgress, pProgressData);
            break;
        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

/************************************************************************/
/*             GDALRasterCompareAlgorithm::BandComparison()             */
/************************************************************************/

void GDALRasterCompareAlgorithm::BandComparison(
    std::vector<std::string> &aosReport, const std::string &bandId,
    bool doBandBasedPixelComparison, GDALRasterBand *poRefBand,
    GDALRasterBand *poInputBand, GDALProgressFunc pfnProgress,
    void *pProgressData)
{
    bool ret = true;

    if (poRefBand->GetXSize() != poInputBand->GetXSize())
    {
        aosReport.push_back("Reference band width is " +
                            std::to_string(poRefBand->GetXSize()) +
                            ", but input band width is " +
                            std::to_string(poInputBand->GetXSize()));
        ret = false;
    }

    if (poRefBand->GetYSize() != poInputBand->GetYSize())
    {
        aosReport.push_back("Reference band height is " +
                            std::to_string(poRefBand->GetYSize()) +
                            ", but input band height is " +
                            std::to_string(poInputBand->GetYSize()));
        ret = false;
    }

    if (strcmp(poRefBand->GetDescription(), poInputBand->GetDescription()) != 0)
    {
        aosReport.push_back("Reference band " + bandId + " has description " +
                            std::string(poRefBand->GetDescription()) +
                            ", but input band has description " +
                            std::string(poInputBand->GetDescription()));
    }

    if (poRefBand->GetRasterDataType() != poInputBand->GetRasterDataType())
    {
        aosReport.push_back(
            "Reference band " + bandId + " has data type " +
            std::string(GDALGetDataTypeName(poRefBand->GetRasterDataType())) +
            ", but input band has data type " +
            std::string(GDALGetDataTypeName(poInputBand->GetRasterDataType())));
    }

    int bRefHasNoData = false;
    const double dfRefNoData = poRefBand->GetNoDataValue(&bRefHasNoData);
    int bInputHasNoData = false;
    const double dfInputNoData = poInputBand->GetNoDataValue(&bInputHasNoData);
    if (!bRefHasNoData && !bInputHasNoData)
    {
        // ok
    }
    else if (bRefHasNoData && !bInputHasNoData)
    {
        aosReport.push_back("Reference band " + bandId + " has nodata value " +
                            std::to_string(dfRefNoData) +
                            ", but input band has none.");
    }
    else if (!bRefHasNoData && bInputHasNoData)
    {
        aosReport.push_back("Reference band " + bandId +
                            " has no nodata value, " +
                            "but input band has no data value " +
                            std::to_string(dfInputNoData) + ".");
    }
    else if ((std::isnan(dfRefNoData) && std::isnan(dfInputNoData)) ||
             dfRefNoData == dfInputNoData)
    {
        // ok
    }
    else
    {
        aosReport.push_back("Reference band " + bandId + " has nodata value " +
                            std::to_string(dfRefNoData) +
                            ", but input band has no data value " +
                            std::to_string(dfInputNoData) + ".");
    }

    if (poRefBand->GetColorInterpretation() !=
        poInputBand->GetColorInterpretation())
    {
        aosReport.push_back("Reference band " + bandId +
                            " has color interpretation " +
                            std::string(GDALGetColorInterpretationName(
                                poRefBand->GetColorInterpretation())) +
                            ", but input band has color interpretation " +
                            std::string(GDALGetColorInterpretationName(
                                poInputBand->GetColorInterpretation())));
    }

    if (!ret)
        return;

    const uint64_t nBasePixels =
        static_cast<uint64_t>(poRefBand->GetXSize()) * poRefBand->GetYSize();
    uint64_t nTotalPixels = nBasePixels;
    const int nOvrCount = poRefBand->GetOverviewCount();
    for (int i = 0; i < nOvrCount; ++i)
    {
        auto poOvrBand = poRefBand->GetOverview(i);
        const uint64_t nOvrPixels =
            static_cast<uint64_t>(poOvrBand->GetXSize()) *
            poOvrBand->GetYSize();
        nTotalPixels += nOvrPixels;
    }

    if (doBandBasedPixelComparison)
    {
        void *pScaledProgress =
            GDALCreateScaledProgress(0.0,
                                     static_cast<double>(nBasePixels) /
                                         static_cast<double>(nTotalPixels),
                                     pfnProgress, pProgressData);
        ComparePixels(aosReport, bandId, poRefBand, poInputBand,
                      pScaledProgress ? GDALScaledProgress : nullptr,
                      pScaledProgress);
        GDALDestroyScaledProgress(pScaledProgress);
    }

    if (!m_skipOverview)
    {
        if (nOvrCount != poInputBand->GetOverviewCount())
        {
            aosReport.push_back(
                "Reference band " + bandId + " has " +
                std::to_string(nOvrCount) +
                " overview band(s), but input band has " +
                std::to_string(poInputBand->GetOverviewCount()));
        }
        else
        {
            uint64_t nIterPixels = nBasePixels;

            for (int i = 0; i < nOvrCount; ++i)
            {
                GDALRasterBand *poOvrBand = poRefBand->GetOverview(i);
                const uint64_t nOvrPixels =
                    static_cast<uint64_t>(poOvrBand->GetXSize()) *
                    poOvrBand->GetYSize();
                void *pScaledProgress = GDALCreateScaledProgress(
                    static_cast<double>(nIterPixels) /
                        static_cast<double>(nTotalPixels),
                    static_cast<double>(nIterPixels + nOvrPixels) /
                        static_cast<double>(nTotalPixels),
                    pfnProgress, pProgressData);
                BandComparison(aosReport, "overview of band " + bandId,
                               doBandBasedPixelComparison, poOvrBand,
                               poInputBand->GetOverview(i),
                               pScaledProgress ? GDALScaledProgress : nullptr,
                               pScaledProgress);
                GDALDestroyScaledProgress(pScaledProgress);
                nIterPixels += nOvrPixels;
            }
        }
    }

    if (poRefBand->GetMaskFlags() != poInputBand->GetMaskFlags())
    {
        aosReport.push_back("Reference band " + bandId + " has mask flags = " +
                            std::to_string(poRefBand->GetMaskFlags()) +
                            " , but input band has mask flags = " +
                            std::to_string(poInputBand->GetMaskFlags()));
    }
    else if (poRefBand->GetMaskFlags() == GMF_PER_DATASET)
    {
        BandComparison(aosReport, "mask of band " + bandId, true,
                       poRefBand->GetMaskBand(), poInputBand->GetMaskBand(),
                       nullptr, nullptr);
    }

    if (!m_skipMetadata)
    {
        MetadataComparison(aosReport, "(band default metadata domain)",
                           poRefBand->GetMetadata(),
                           poInputBand->GetMetadata());
    }
}

/************************************************************************/
/*           GDALRasterCompareAlgorithm::MetadataComparison()           */
/************************************************************************/

void GDALRasterCompareAlgorithm::MetadataComparison(
    std::vector<std::string> &aosReport, const std::string &metadataDomain,
    CSLConstList aosRef, CSLConstList aosInput)
{
    std::map<std::string, std::string> oMapRef;
    std::map<std::string, std::string> oMapInput;

    std::array<const char *, 3> ignoredKeys = {
        "backend",   // from gdalcompare.py. Not sure why
        "ERR_BIAS",  // RPC optional key
        "ERR_RAND",  // RPC optional key
    };

    for (const auto &[key, value] : cpl::IterateNameValue(aosRef))
    {
        const char *pszKey = key;
        const auto eq = [pszKey](const char *s)
        { return strcmp(pszKey, s) == 0; };
        auto it = std::find_if(ignoredKeys.begin(), ignoredKeys.end(), eq);
        if (it == ignoredKeys.end())
        {
            oMapRef[key] = value;
        }
    }

    for (const auto &[key, value] : cpl::IterateNameValue(aosInput))
    {
        const char *pszKey = key;
        const auto eq = [pszKey](const char *s)
        { return strcmp(pszKey, s) == 0; };
        auto it = std::find_if(ignoredKeys.begin(), ignoredKeys.end(), eq);
        if (it == ignoredKeys.end())
        {
            oMapInput[key] = value;
        }
    }

    const auto strip = [](const std::string &s)
    {
        const auto posBegin = s.find_first_not_of(' ');
        if (posBegin == std::string::npos)
            return std::string();
        const auto posEnd = s.find_last_not_of(' ');
        return s.substr(posBegin, posEnd - posBegin + 1);
    };

    for (const auto &sKeyValuePair : oMapRef)
    {
        const auto oIter = oMapInput.find(sKeyValuePair.first);
        if (oIter == oMapInput.end())
        {
            aosReport.push_back("Reference metadata " + metadataDomain +
                                " contains key '" + sKeyValuePair.first +
                                "' but input metadata does not.");
        }
        else
        {
            // this will always have the current date set
            if (sKeyValuePair.first == "NITF_FDT")
                continue;

            std::string ref = oIter->second;
            std::string input = sKeyValuePair.second;
            if (metadataDomain == "RPC")
            {
                // _RPC.TXT files and in-file have a difference
                // in white space that is not otherwise meaningful.
                ref = strip(ref);
                input = strip(input);
            }
            if (ref != input)
            {
                aosReport.push_back(
                    "Reference metadata " + metadataDomain + " has value '" +
                    ref + "' for key '" + sKeyValuePair.first +
                    "' but input metadata has value '" + input + "'.");
            }
        }
    }

    for (const auto &sKeyValuePair : oMapInput)
    {
        if (!cpl::contains(oMapRef, sKeyValuePair.first))
        {
            aosReport.push_back("Input metadata " + metadataDomain +
                                " contains key '" + sKeyValuePair.first +
                                "' but reference metadata does not.");
        }
    }
}

/************************************************************************/
/*                GDALRasterCompareAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterCompareAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poRefDS = m_referenceDataset.GetDatasetRef();
    CPLAssert(poRefDS);

    CPLAssert(m_inputDataset.size() == 1);
    auto poInputDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poInputDS);

    if (m_skipAllOptional)
    {
        m_skipBinary = true;
        m_skipCRS = true;
        m_skipGeotransform = true;
        m_skipOverview = true;
        m_skipMetadata = true;
        m_skipRPC = true;
        m_skipGeolocation = true;
        m_skipSubdataset = true;
    }

    std::vector<std::string> aosReport;

    if (!m_skipBinary)
    {
        if (BinaryComparison(aosReport, poRefDS, poInputDS))
        {
            return true;
        }
    }

    CSLConstList papszSubDSRef =
        m_skipSubdataset ? nullptr : poRefDS->GetMetadata("SUBDATASETS");
    const int nCountRef = CSLCount(papszSubDSRef) / 2;
    CSLConstList papszSubDSInput =
        m_skipSubdataset ? nullptr : poInputDS->GetMetadata("SUBDATASETS");
    const int nCountInput = CSLCount(papszSubDSInput) / 2;

    if (!m_skipSubdataset)
    {
        if (nCountRef != nCountInput)
        {
            aosReport.push_back("Reference dataset has " +
                                std::to_string(nCountRef) +
                                " subdataset(s) whereas input dataset has " +
                                std::to_string(nCountInput) + " one(s).");
            m_skipSubdataset = true;
        }
    }

    // Compute total number of pixels, including in subdatasets
    const uint64_t nBasePixels =
        static_cast<uint64_t>(poRefDS->GetRasterXSize()) *
        poRefDS->GetRasterYSize() * poRefDS->GetRasterCount();
    uint64_t nTotalPixels = nBasePixels;
    if (ctxt.m_pfnProgress && !m_skipSubdataset)
    {
        for (int i = 0; i < nCountRef; ++i)
        {
            const char *pszRef = CSLFetchNameValue(
                papszSubDSRef, CPLSPrintf("SUBDATASET_%d_NAME", i + 1));
            const char *pszInput = CSLFetchNameValue(
                papszSubDSInput, CPLSPrintf("SUBDATASET_%d_NAME", i + 1));
            if (pszRef && pszInput)
            {
                auto poSubRef = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(pszRef, GDAL_OF_RASTER));
                auto poSubInput = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(pszInput, GDAL_OF_RASTER));
                if (poSubRef && poSubInput)
                {
                    const uint64_t nSubDSPixels =
                        static_cast<uint64_t>(poSubRef->GetRasterXSize()) *
                        poSubRef->GetRasterYSize() * poSubRef->GetRasterCount();
                    nTotalPixels += nSubDSPixels;
                }
            }
        }
    }

    {
        void *pScaledProgress =
            GDALCreateScaledProgress(0.0,
                                     static_cast<double>(nBasePixels) /
                                         static_cast<double>(nTotalPixels),
                                     ctxt.m_pfnProgress, ctxt.m_pProgressData);
        DatasetComparison(aosReport, poRefDS, poInputDS,
                          pScaledProgress ? GDALScaledProgress : nullptr,
                          pScaledProgress);
        GDALDestroyScaledProgress(pScaledProgress);
    }

    if (!m_skipSubdataset)
    {
        uint64_t nIterPixels = nBasePixels;
        for (int i = 0; i < nCountRef; ++i)
        {
            const char *pszRef = CSLFetchNameValue(
                papszSubDSRef, CPLSPrintf("SUBDATASET_%d_NAME", i + 1));
            const char *pszInput = CSLFetchNameValue(
                papszSubDSInput, CPLSPrintf("SUBDATASET_%d_NAME", i + 1));
            if (pszRef && pszInput)
            {
                auto poSubRef = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    pszRef, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
                auto poSubInput =
                    std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        pszInput, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
                if (poSubRef && poSubInput)
                {
                    const uint64_t nSubDSPixels =
                        static_cast<uint64_t>(poSubRef->GetRasterXSize()) *
                        poSubRef->GetRasterYSize() * poSubRef->GetRasterCount();
                    void *pScaledProgress = GDALCreateScaledProgress(
                        static_cast<double>(nIterPixels) /
                            static_cast<double>(nTotalPixels),
                        static_cast<double>(nIterPixels + nSubDSPixels) /
                            static_cast<double>(nTotalPixels),
                        ctxt.m_pfnProgress, ctxt.m_pProgressData);
                    DatasetComparison(
                        aosReport, poSubRef.get(), poSubInput.get(),
                        pScaledProgress ? GDALScaledProgress : nullptr,
                        pScaledProgress);
                    GDALDestroyScaledProgress(pScaledProgress);
                    nIterPixels += nSubDSPixels;
                }
            }
        }
    }

    for (const auto &s : aosReport)
    {
        m_output += s;
        m_output += '\n';
    }

    m_retCode = static_cast<int>(aosReport.size());

    return true;
}

/************************************************************************/
/*               ~GDALRasterCompareAlgorithmStandalone()                */
/************************************************************************/

GDALRasterCompareAlgorithmStandalone::~GDALRasterCompareAlgorithmStandalone() =
    default;

//! @endcond
