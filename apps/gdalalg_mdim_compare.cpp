/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_compare.h"

#include "gdal_dataset.h"
#include "gdal_multidim.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <iterator>
#include <set>
#include <utility>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALMdimCompareAlgorithm::GDALMdimCompareAlgorithm()         */
/************************************************************************/

GDALMdimCompareAlgorithm::GDALMdimCompareAlgorithm(bool standaloneStep)
    : GDALMdimPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                    ConstructorOptions()
                                        .SetStandaloneStep(standaloneStep)
                                        .SetInputDatasetMaxCount(1)
                                        .SetAddDefaultArguments(false))
{
    if (standaloneStep)
    {
        AddProgressArg();
    }
    else
    {
        AddMdimHiddenInputDatasetArg();
    }

    auto &referenceDatasetArg =
        AddArg("reference", 0, _("Reference dataset"), &m_referenceDataset,
               GDAL_OF_MULTIDIM_RASTER)
            .SetPositional()
            .SetRequired();

    SetAutoCompleteFunctionForFilename(referenceDatasetArg,
                                       GDAL_OF_MULTIDIM_RASTER);

    if (standaloneStep)
    {
        AddMdimInputArgs(/* openForMixedMdimVector = */ false,
                         /* hiddenForCLI = */ false,
                         /* acceptRaster = */ false);
    }

    AddArg("metric", 0, _("Comparison metric(s)"), &m_metrics)
        .SetChoices(METRIC_ALL, METRIC_NONE, METRIC_DIFF, METRIC_RMSD,
                    METRIC_PSNR)
        .SetDefault(METRIC_DEFAULT);

    AddArrayNameArg(&m_array, _("Name of array(s) to compare"));

    AddOutputStringArg(&m_output);

    AddArg("skip-binary", 0, _("Skip binary file comparison"), &m_skipBinary);

    AddArg("return-code", 0, _("Return code"), &m_retCode)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*                           GetPixelCount()                            */
/************************************************************************/

static uint64_t GetPixelCount(const std::shared_ptr<GDALMDArray> &array)
{
    uint64_t nPixels = 1;
    for (const auto &poDim : array->GetDimensions())
    {
        nPixels *= poDim->GetSize();
    }
    return nPixels;
}

/************************************************************************/
/*                 GDALMdimCompareAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALMdimCompareAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poRefDS = m_referenceDataset.GetDatasetRef();
    CPLAssert(poRefDS);

    CPLAssert(m_inputDataset.size() == 1);
    auto poInputDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poInputDS);

    std::vector<std::string> aosReport;

    if (!m_skipBinary)
    {
        if (BinaryComparison(this, aosReport, poRefDS, poInputDS))
        {
            return true;
        }
    }

    auto poRefRootGroup = poRefDS->GetRootGroup();
    if (!poRefRootGroup)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot get root group on reference dataset");
        return false;
    }

    auto poInputRootGroup = poInputDS->GetRootGroup();
    if (!poInputRootGroup)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot get root group on input dataset");
        return false;
    }

    auto refArrays = poRefRootGroup->GetMDArrayFullNamesRecursive();
    auto inputArrays = poInputRootGroup->GetMDArrayFullNamesRecursive();
    if (m_array.empty())
    {
        std::sort(refArrays.begin(), refArrays.end());
        std::sort(inputArrays.begin(), inputArrays.end());

        {
            std::vector<std::string> missing;
            std::set_difference(refArrays.begin(), refArrays.end(),
                                inputArrays.begin(), inputArrays.end(),
                                std::back_inserter(missing));
            if (!missing.empty())
            {
                std::string line =
                    "The following arrays are found in the reference dataset, "
                    "but missing in the input one: ";
                bool first = true;
                for (const auto &name : missing)
                {
                    if (!first)
                        line += ", ";
                    first = false;
                    line += name;
                }
                aosReport.push_back(std::move(line));
            }
        }

        {
            std::vector<std::string> missing;
            std::set_difference(inputArrays.begin(), inputArrays.end(),
                                refArrays.begin(), refArrays.end(),
                                std::back_inserter(missing));
            if (!missing.empty())
            {
                std::string line =
                    "The following arrays are found in the input dataset, but "
                    "missing in the reference one: ";
                bool first = true;
                for (const auto &name : missing)
                {
                    if (!first)
                        line += ", ";
                    first = false;
                    line += name;
                }
                aosReport.push_back(std::move(line));
            }
        }

        std::set_intersection(refArrays.begin(), refArrays.end(),
                              inputArrays.begin(), inputArrays.end(),
                              std::back_inserter(m_array));
    }
    else
    {
        std::set<std::string> newArrays;
        for (const auto &name : m_array)
        {
            const auto ExistsIn =
                [&name,
                 &newArrays](const std::vector<std::string> &aosExitingArrays,
                             bool insert)
            {
                bool ret = false;
                for (const auto &exitingArray : aosExitingArrays)
                {
                    if (!name.empty() &&
                        ((name[0] == '/' && name == exitingArray) ||
                         (name[0] != '/' &&
                          name == CPLGetFilename(exitingArray.c_str()))))
                    {
                        if (insert)
                            newArrays.insert(exitingArray);
                        ret = true;
                    }
                }
                return ret;
            };

            if (!ExistsIn(refArrays, true))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array '%s' does not exist in reference dataset",
                            name.c_str());
                return false;
            }
            if (!ExistsIn(inputArrays, false))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array '%s' does not exist in input dataset",
                            name.c_str());
                return false;
            }
        }
        m_array.clear();
        m_array.insert(m_array.end(), newArrays.begin(), newArrays.end());
    }

    std::vector<
        std::pair<std::shared_ptr<GDALMDArray>, std::shared_ptr<GDALMDArray>>>
        arrayPairs;
    uint64_t nTotalPixels = 0;
    for (const auto &array : m_array)
    {
        auto poRefArray = poRefRootGroup->OpenMDArrayFromFullname(array);
        if (!poRefArray)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Array '%s' cannot be opened in reference dataset",
                        array.c_str());
            return false;
        }

        auto poInputArray = poInputRootGroup->OpenMDArrayFromFullname(array);
        if (!poInputArray)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Array '%s' cannot be opened in input dataset",
                        array.c_str());
            return false;
        }

        nTotalPixels += GetPixelCount(poRefArray);
        arrayPairs.emplace_back(std::move(poRefArray), std::move(poInputArray));
    }

    uint64_t nCurPixels = 0;
    for (const auto &[poRefArray, poInputArray] : arrayPairs)
    {
        const uint64_t nThisArrayPixels = GetPixelCount(poRefArray);
        const double dfMinPct =
            static_cast<double>(nCurPixels) / static_cast<double>(nTotalPixels);
        const double dfMaxPct =
            static_cast<double>(nCurPixels + nThisArrayPixels) /
            static_cast<double>(nTotalPixels);
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(GDALCreateScaledProgress(dfMinPct, dfMaxPct,
                                                     ctxt.m_pfnProgress,
                                                     ctxt.m_pProgressData),
                            GDALDestroyScaledProgress);
        CompareArray(aosReport, poRefArray, poInputArray,
                     pScaledProgress ? GDALScaledProgress : nullptr,
                     pScaledProgress.get());
        nCurPixels += nThisArrayPixels;
    }

    if (ctxt.m_pfnProgress)
        ctxt.m_pfnProgress(1.0, "", ctxt.m_pProgressData);

    for (const auto &s : aosReport)
    {
        m_output += s;
        m_output += '\n';
    }

    m_retCode = static_cast<int>(aosReport.size());

    return true;
}

/************************************************************************/
/*                         NonZeroValueIterator                         */
/************************************************************************/

namespace
{
struct NonZeroValueIterator
{
    std::vector<double> adfValues{};
    uint64_t nNonZero = 0;

    static bool func(GDALAbstractMDArray *array,
                     const GUInt64 *chunkArrayStartIdx,
                     const size_t *chunkCount, GUInt64 /* iCurChunk */,
                     GUInt64 /* nChunkCount */, void *pUserData)
    {
        auto self = static_cast<NonZeroValueIterator *>(pUserData);
        const size_t nDims = array->GetDimensionCount();
        size_t nElts = 1;
        for (size_t i = 0; i < nDims; ++i)
            nElts *= chunkCount[i];
        if (self->adfValues.size() < nElts)
        {
            try
            {
                self->adfValues.resize(nElts);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                return false;
            }
        }
        if (!array->Read(chunkArrayStartIdx, chunkCount,
                         /* step = */ nullptr, /* stride = */ nullptr,
                         GDALExtendedDataType::Create(GDT_Float64),
                         self->adfValues.data()))
        {
            return false;
        }
        for (size_t i = 0; i < nElts; ++i)
        {
            if (self->adfValues[i] != 0)
                self->nNonZero++;
        }

        return true;
    }
};
}  // namespace

/************************************************************************/
/*                          GetDataTypeName()                           */
/************************************************************************/

static std::string GetDataTypeName(const GDALExtendedDataType &dt)
{
    switch (dt.GetClass())
    {
        case GEDTC_NUMERIC:
            return GDALGetDataTypeName(dt.GetNumericDataType());
        case GEDTC_STRING:
            return "String";
        case GEDTC_COMPOUND:
            break;
    }
    return "Compound";
}

/************************************************************************/
/*               GDALMdimCompareAlgorithm::CompareArray()               */
/************************************************************************/

void GDALMdimCompareAlgorithm::CompareArray(
    std::vector<std::string> &aosReport,
    const std::shared_ptr<GDALMDArray> &poRefArray,
    const std::shared_ptr<GDALMDArray> &poInputArray,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    const auto &osName = poRefArray->GetFullName();
    const auto nDims = poRefArray->GetDimensionCount();
    if (nDims != poInputArray->GetDimensionCount())
    {
        aosReport.push_back(
            CPLSPrintf("Array %s: dimension count in reference is %d, whereas "
                       "it is %d in input",
                       osName.c_str(), static_cast<int>(nDims),
                       static_cast<int>(poInputArray->GetDimensionCount())));
        return;
    }

    if (!poRefArray->HasSameShapeAs(*poInputArray))
    {
        const auto ShapeToString = [](const std::shared_ptr<GDALMDArray> &array)
        {
            std::string s("[");
            for (const auto &poDim : array->GetDimensions())
            {
                if (s.size() > 1)
                    s += ',';
                s += std::to_string(poDim->GetSize());
            }
            s += ']';
            return s;
        };

        aosReport.push_back(CPLSPrintf(
            "Array %s: shape in reference is %s, whereas it is %s in input",
            osName.c_str(), ShapeToString(poRefArray).c_str(),
            ShapeToString(poInputArray).c_str()));
        return;
    }

    if (poRefArray->GetDataType() != poInputArray->GetDataType())
    {
        aosReport.push_back(CPLSPrintf(
            "Array %s: data type in reference is %s, whereas it is %s in input",
            osName.c_str(), GetDataTypeName(poRefArray->GetDataType()).c_str(),
            GetDataTypeName(poInputArray->GetDataType()).c_str()));
    }
    if (poRefArray->GetDataType().GetClass() != GEDTC_NUMERIC ||
        poInputArray->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Array %s: not compared, as comparison of non-numeric data "
                 "types is not currently supported",
                 osName.c_str());
        return;
    }

    auto diffArray = (*poRefArray) - poInputArray;
    if (!diffArray)
    {
        // Given above checks, this shouldn't happen.
        aosReport.push_back(CPLSPrintf(
            "Array %s: cannot compute array of differences", osName.c_str()));
        return;
    }

    int nCountMetrics = 0;
    if (HasMetric(METRIC_DIFF))
        ++nCountMetrics;
    if (HasMetric(METRIC_RMSD) || HasMetric(METRIC_PSNR))
        ++nCountMetrics;

    double dfLastPct = 0;
    if (HasMetric(METRIC_DIFF))
    {
        double dfMin = 0, dfMax = 0;

        const double dfNewLastPct = 1.0 / nCountMetrics;
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(GDALCreateScaledProgress(
                                0.0, dfNewLastPct, pfnProgress, pProgressData),
                            GDALDestroyScaledProgress);
        dfLastPct = dfNewLastPct;

        if (!diffArray->ComputeStatistics(
                /* bApproxOK =*/false, &dfMin, &dfMax, nullptr, nullptr,
                nullptr, pScaledProgress ? GDALScaledProgress : nullptr,
                pScaledProgress.get(), nullptr))
        {
            aosReport.push_back(CPLSPrintf(
                "Array %s: cannot compute statistics", osName.c_str()));
            return;
        }
        if (dfMin != 0 || dfMax != 0)
        {
            const double dfMaxDiff =
                std::max(std::fabs(dfMin), std::fabs(dfMax));
            aosReport.push_back(
                CPLSPrintf("Array %s: maximum pixel value difference: %g",
                           osName.c_str(), dfMaxDiff));

            NonZeroValueIterator it;
            std::vector<GUInt64> arrayStartIdx(nDims, 0);
            std::vector<GUInt64> count(nDims, 0);
            for (size_t i = 0; i < nDims; ++i)
                count[i] = diffArray->GetDimensions()[i]->GetSize();

            size_t nMaxSize = 100 * 1024 * 1024;
            const GIntBig nUsableRAM = CPLGetUsablePhysicalRAM() / 10;
            if (nUsableRAM > 0)
                nMaxSize = static_cast<size_t>(nUsableRAM);

            if (!diffArray->ProcessPerChunk(
                    arrayStartIdx.data(), count.data(),
                    diffArray->GetProcessingChunkSize(nMaxSize).data(),
                    NonZeroValueIterator::func, &it))
            {
                aosReport.push_back(
                    CPLSPrintf("Array %s: diffArray->ProcessPerChunk() failed",
                               osName.c_str()));
                return;
            }

            aosReport.push_back(
                CPLSPrintf("Array %s: pixels differing: %" PRIu64,
                           osName.c_str(), it.nNonZero));
        }
    }

    if (HasMetric(METRIC_RMSD) || HasMetric(METRIC_PSNR))
    {
        // For PSNR on floating point image, we need to compute min and max of
        // reference band
        const bool bIsInteger = CPL_TO_BOOL(GDALDataTypeIsInteger(
            poRefArray->GetDataType().GetNumericDataType()));
        const double dfScalingProgress =
            HasMetric(METRIC_PSNR) && !bIsInteger ? 0.5 : 1;
        const double dfNewLastPct =
            std::min(1.0, dfLastPct + dfScalingProgress * (1.0 - dfLastPct));
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(GDALCreateScaledProgress(dfLastPct, dfNewLastPct,
                                                     pfnProgress,
                                                     pProgressData),
                            GDALDestroyScaledProgress);
        dfLastPct = dfNewLastPct;

        auto squaredDiffArray = (*diffArray) * diffArray;
        double dfMeanSquareError = 0;
        if (squaredDiffArray->ComputeStatistics(
                /* bApproxOK = */ false,
                /* pdfMin = */ nullptr,
                /* pdfMax = */ nullptr, &dfMeanSquareError,
                /* pdfStdDev = */ nullptr, nullptr,
                pScaledProgress ? GDALScaledProgress : nullptr,
                pScaledProgress.get(), nullptr))
        {
            const double dfRMSD = std::sqrt(dfMeanSquareError);
            if (dfRMSD > 0)
            {
                if (HasMetric(METRIC_RMSD))
                {
                    aosReport.push_back(CPLSPrintf("Array %s: RMSD: %g",
                                                   osName.c_str(), dfRMSD));
                }

                if (HasMetric(METRIC_PSNR))
                {
                    if (bIsInteger)
                    {
                        double dfMaxAmplitude =
                            std::pow(2.0, GDALGetDataTypeSizeBits(
                                              poRefArray->GetDataType()
                                                  .GetNumericDataType())) -
                            1;

                        const double dfPSNR_dB =
                            20 * std::log10(dfMaxAmplitude / dfRMSD);
                        aosReport.push_back(
                            CPLSPrintf("Array %s: PSNR (dB): %g",
                                       osName.c_str(), dfPSNR_dB));
                    }
                    else
                    {
                        double dfMin = 0;
                        double dfMax = 0;
                        const char *const apszOptions[] = {
                            "SET_STATISTICS=FALSE", nullptr};

                        pScaledProgress.reset(GDALCreateScaledProgress(
                            dfLastPct, 1.0, pfnProgress, pProgressData));

                        if (poRefArray->ComputeStatistics(
                                /* bApproxOK = */ false, &dfMin, &dfMax,
                                nullptr, nullptr, nullptr,
                                pScaledProgress ? GDALScaledProgress : nullptr,
                                pScaledProgress.get(), apszOptions))
                        {
                            const double dfPSNR_dB =
                                20 * std::log10((dfMax - dfMin) / dfRMSD);
                            aosReport.push_back(
                                CPLSPrintf("Array %s: PSNR (dB): %g",
                                           osName.c_str(), dfPSNR_dB));
                        }
                        else
                        {
                            aosReport.push_back(
                                std::string("Error during PSNR computation: ")
                                    .append(CPLGetLastErrorMsg()));
                        }
                    }
                }
            }
        }
        else
        {
            aosReport.push_back(
                std::string("Error during RMSD/PSNR computation: ")
                    .append(CPLGetLastErrorMsg()));
        }
    }
}

/************************************************************************/
/*                ~GDALMdimCompareAlgorithmStandalone()                 */
/************************************************************************/

GDALMdimCompareAlgorithmStandalone::~GDALMdimCompareAlgorithmStandalone() =
    default;

//! @endcond
