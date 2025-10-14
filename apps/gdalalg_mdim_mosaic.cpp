/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim mosaic" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_mosaic.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "vrtdataset.h"

#include <algorithm>
#include <cmath>
#include <optional>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALMdimMosaicAlgorithm::GDALMdimMosaicAlgorithm()        */
/************************************************************************/

GDALMdimMosaicAlgorithm::GDALMdimMosaicAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_CREATE_MULTIDIMENSIONAL});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_ALG_DCAP_RASTER_OR_MULTIDIM_RASTER});
    AddInputDatasetArg(&m_inputDatasets, GDAL_OF_MULTIDIM_RASTER)
        .SetDatasetInputFlags(GADV_NAME)
        .SetDatasetOutputFlags(0)
        .SetAutoOpenDataset(false)
        .SetMinCount(1);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_MULTIDIM_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);
    AddArrayNameArg(&m_array, _("Name of array to mosaic."));
}

/************************************************************************/
/*                           GetDimensionDesc()                         */
/************************************************************************/

std::optional<GDALMdimMosaicAlgorithm::DimensionDesc>
GDALMdimMosaicAlgorithm::GetDimensionDesc(
    const std::string &osDSName,
    const std::shared_ptr<GDALDimension> &poDim) const
{
    auto poVar = poDim->GetIndexingVariable();
    if (!poVar)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset %s: dimension %s lacks an indexing variable",
                    osDSName.c_str(), poDim->GetName().c_str());
        return std::nullopt;
    }
    if (poVar->GetDimensionCount() != 1)
    {
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "Dataset %s: indexing variable %s of dimension %s is not 1D",
            osDSName.c_str(), poVar->GetName().c_str(),
            poDim->GetName().c_str());
        return std::nullopt;
    }
    if (poVar->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset %s: indexing variable %s of dimension %s has a "
                    "non-numeric data type",
                    osDSName.c_str(), poVar->GetName().c_str(),
                    poDim->GetName().c_str());
        return std::nullopt;
    }
    DimensionDesc desc;
    desc.osName = poDim->GetName();
    desc.osType = poDim->GetType();
    desc.osDirection = poDim->GetDirection();
    const auto nSize = poVar->GetDimensions()[0]->GetSize();
    desc.attributes = poVar->GetAttributes();
    CPLAssert(nSize > 0);
    desc.nSize = nSize;
    if (nSize <= 2 || !poVar->IsRegularlySpaced(desc.dfStart, desc.dfIncrement))
    {
        constexpr uint64_t LIMIT = 100 * 1000 * 1000;
        if (nSize > LIMIT)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset %s: indexing variable %s of dimension %s has "
                        "too large size",
                        osDSName.c_str(), poVar->GetName().c_str(),
                        desc.osName.c_str());
            return std::nullopt;
        }
        std::vector<double> adfValues(static_cast<size_t>(nSize));
        GUInt64 arrayStartIdx[] = {0};
        size_t anCount[] = {adfValues.size()};
        GInt64 arrayStep[] = {1};
        GPtrDiff_t bufferStride[] = {1};
        if (!poVar->Read(arrayStartIdx, anCount, arrayStep, bufferStride,
                         GDALExtendedDataType::Create(GDT_Float64),
                         adfValues.data()))
        {
            return std::nullopt;
        }
        if (std::isnan(adfValues[0]))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset %s: indexing variable %s of dimension %s has "
                        "NaN values",
                        osDSName.c_str(), poVar->GetName().c_str(),
                        desc.osName.c_str());
            return std::nullopt;
        }
        if (nSize > 1)
        {
            const int nSign = adfValues[1] > adfValues[0] ? 1 : -1;
            if (std::isnan(adfValues[1]))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Dataset %s: indexing variable %s of dimension %s "
                            "has NaN values",
                            osDSName.c_str(), poVar->GetName().c_str(),
                            desc.osName.c_str());
                return std::nullopt;
            }
            for (size_t i = 2; i < nSize; ++i)
            {
                if (std::isnan(adfValues[i]))
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Dataset %s: indexing variable %s of dimension "
                                "%s has NaN values",
                                osDSName.c_str(), poVar->GetName().c_str(),
                                desc.osName.c_str());
                    return std::nullopt;
                }
                const int nSign2 = adfValues[i] > adfValues[i - 1] ? 1 : -1;
                if (nSign != nSign2)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Dataset %s: indexing variable %s of dimension "
                                "%s is not strictly increasing or decreasing",
                                osDSName.c_str(), poVar->GetName().c_str(),
                                desc.osName.c_str());
                    return std::nullopt;
                }
            }
            desc.nProgressionSign = nSign;
        }
        std::sort(adfValues.begin(), adfValues.end());
        desc.aaValues.push_back(std::move(adfValues));
    }
    return desc;
}

/************************************************************************/
/*            GDALMdimMosaicAlgorithm::BuildDimensionDesc()             */
/************************************************************************/

std::optional<std::vector<GDALMdimMosaicAlgorithm::DimensionDesc>>
GDALMdimMosaicAlgorithm::BuildDimensionDesc(
    std::shared_ptr<GDALMDArray> &poFirstSourceArray,
    std::vector<std::vector<SourceShortDimDesc>> &aaoSourceShortDimDesc)
{
    std::vector<DimensionDesc> aDimensions;

    for (const auto &dataset : m_inputDatasets)
    {
        auto poDS = std::unique_ptr<GDALDataset>(
            GDALDataset::Open(dataset.GetName().c_str(),
                              GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR,
                              CPLStringList(m_inputFormats).List(),
                              CPLStringList(m_openOptions).List(), nullptr));
        if (!poDS)
            return std::nullopt;
        auto poRG = poDS->GetRootGroup();
        if (!poRG)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot get root group for dataset %s",
                        dataset.GetName().c_str());
            return std::nullopt;
        }
        std::shared_ptr<GDALMDArray> poArray;
        if (!m_array.empty())
        {
            poArray = poRG->OpenMDArrayFromFullname(m_array);
            if (!poArray)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot find array %s in dataset %s",
                            m_array.c_str(), dataset.GetName().c_str());
                return std::nullopt;
            }
        }
        else
        {
            std::vector<std::string> arrayNames;
            for (const std::string &arrayName :
                 poRG->GetMDArrayFullNamesRecursive())
            {
                auto poIterArray = poRG->OpenMDArrayFromFullname(arrayName);
                if (!poIterArray)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot open array %s of dataset %s",
                                arrayName.c_str(), dataset.GetName().c_str());
                    return std::nullopt;
                }
                if (poIterArray->GetDimensionCount() < 2)
                    continue;
                poArray = poIterArray;
                arrayNames.push_back(arrayName);
            }
            if (arrayNames.empty())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "No array of dimension count >= 2 found in dataset %s",
                    dataset.GetName().c_str());
                return std::nullopt;
            }
            if (arrayNames.size() >= 2)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Several arrays of dimension count >= 2 found in "
                            "dataset %s. Specify one with the 'array' argument",
                            dataset.GetName().c_str());
                return std::nullopt;
            }
            m_array = arrayNames[0];
        }
        CPLAssert(poArray);

        if (poArray->GetDimensionCount() == 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Array %s of dataset %s has no dimensions",
                        m_array.c_str(), dataset.GetName().c_str());
            return std::nullopt;
        }

        std::vector<SourceShortDimDesc> aoSourceShortDimDesc;
        const auto AddToSourceShortDimDesc =
            [&aoSourceShortDimDesc](const DimensionDesc &dimDesc,
                                    uint64_t nSize)
        {
            SourceShortDimDesc sourceDesc;
            sourceDesc.nSize = nSize;
            sourceDesc.bIsRegularlySpaced = dimDesc.aaValues.empty();
            if (sourceDesc.bIsRegularlySpaced)
                sourceDesc.dfStart = dimDesc.dfStart;
            else
                sourceDesc.dfStart = dimDesc.aaValues[0][0];
            aoSourceShortDimDesc.push_back(std::move(sourceDesc));
        };

        if (!poFirstSourceArray)
        {
            poFirstSourceArray = poArray;
            CPLAssert(aDimensions.empty());
            for (auto &poDim : poArray->GetDimensions())
            {
                auto descOpt = GetDimensionDesc(dataset.GetName(), poDim);
                if (!descOpt.has_value())
                    return std::nullopt;
                const auto &desc = descOpt.value();
                AddToSourceShortDimDesc(desc, poDim->GetSize());
                aDimensions.push_back(std::move(desc));
            }
        }
        else
        {
            if (poArray->GetDimensionCount() != aDimensions.size())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array %s of dataset %s does not have the same "
                            "number of dimensions as in other datasets",
                            m_array.c_str(), dataset.GetName().c_str());
                return std::nullopt;
            }
            if (poArray->GetDataType() != poFirstSourceArray->GetDataType())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array %s of dataset %s does not have the same "
                            "data type as in other datasets",
                            m_array.c_str(), dataset.GetName().c_str());
                return std::nullopt;
            }
            const void *poFirstRawNoData =
                poFirstSourceArray->GetRawNoDataValue();
            const void *poRawNoData = poArray->GetRawNoDataValue();
            if (!((!poFirstRawNoData && !poRawNoData) ||
                  (poFirstRawNoData && poRawNoData &&
                   memcmp(poFirstRawNoData, poRawNoData,
                          poArray->GetDataType().GetSize()) == 0)))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array %s of dataset %s does not have the same "
                            "nodata value as in other datasets",
                            m_array.c_str(), dataset.GetName().c_str());
                return std::nullopt;
            }
            const auto apoDims = poArray->GetDimensions();
            for (size_t iDim = 0; iDim < aDimensions.size(); ++iDim)
            {
                DimensionDesc &desc = aDimensions[iDim];
                auto &poDim = apoDims[iDim];
                if (poDim->GetName() != desc.osName)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Dimension %d of array %s of dataset %s does "
                                "not have the same name as in other datasets",
                                static_cast<int>(iDim), m_array.c_str(),
                                dataset.GetName().c_str());
                    return std::nullopt;
                }

                auto descThisDatasetOpt =
                    GetDimensionDesc(dataset.GetName(), poDim);
                if (!descThisDatasetOpt.has_value())
                    return std::nullopt;
                auto &descThisDataset = descThisDatasetOpt.value();
                AddToSourceShortDimDesc(descThisDataset, poDim->GetSize());
                if (desc.aaValues.empty())
                {
                    if (!descThisDataset.aaValues.empty())
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Dimension %s of array %s of dataset %s "
                                    "has irregularly-spaced values, contrary "
                                    "to other datasets",
                                    poDim->GetName().c_str(), m_array.c_str(),
                                    dataset.GetName().c_str());
                        return std::nullopt;
                    }
                    if (std::fabs(descThisDataset.dfIncrement -
                                  desc.dfIncrement) >
                        1e-10 * std::fabs(desc.dfIncrement))
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Dimension %s of array %s of dataset %s is "
                                    "indexed by a variable with spacing %g, "
                                    "whereas it is %g in other datasets",
                                    poDim->GetName().c_str(), m_array.c_str(),
                                    dataset.GetName().c_str(),
                                    descThisDataset.dfIncrement,
                                    desc.dfIncrement);
                        return std::nullopt;
                    }
                    const double dfPos =
                        (descThisDataset.dfStart - desc.dfStart) /
                        desc.dfIncrement;
                    if (std::fabs(std::round(dfPos) - dfPos) > 1e-3)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Dimension %s of array %s of dataset %s is indexed "
                            "by a variable whose start value is not aligned "
                            "with the one of other datasets",
                            poDim->GetName().c_str(), m_array.c_str(),
                            dataset.GetName().c_str());
                        return std::nullopt;
                    }
                    desc.dfStart =
                        std::min(desc.dfStart, descThisDataset.dfStart);
                    const double dfEnd = std::max(
                        desc.dfStart +
                            static_cast<double>(desc.nSize) * desc.dfIncrement,
                        descThisDataset.dfStart +
                            static_cast<double>(descThisDataset.nSize) *
                                descThisDataset.dfIncrement);
                    const double dfSize =
                        (dfEnd - desc.dfStart) / desc.dfIncrement;
                    constexpr double MAX_INTEGER_REPRESENTABLE =
                        static_cast<double>(1ULL << 53);
                    if (dfSize > MAX_INTEGER_REPRESENTABLE)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Dimension %s of array %s of dataset %s "
                                    "would be too large if merged.",
                                    poDim->GetName().c_str(), m_array.c_str(),
                                    dataset.GetName().c_str());
                        return std::nullopt;
                    }
                    desc.nSize = static_cast<uint64_t>(dfSize + 0.5);
                }
                else
                {
                    if (descThisDataset.aaValues.empty())
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Dimension %s of array %s of dataset %s "
                                    "has regularly spaced labels, contrary to "
                                    "other datasets",
                                    poDim->GetName().c_str(), m_array.c_str(),
                                    dataset.GetName().c_str());
                        return std::nullopt;
                    }
                    if (descThisDataset.nProgressionSign !=
                        desc.nProgressionSign)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Dataset %s: values in indexing variable %s of "
                            "dimension %s must be either increasing or "
                            "decreasing in all input datasets.",
                            dataset.GetName().c_str(),
                            poDim->GetIndexingVariable()->GetName().c_str(),
                            desc.osName.c_str());
                        return std::nullopt;
                    }
                    CPLAssert(descThisDataset.aaValues.size() == 1);
                    if (descThisDataset.aaValues[0][0] < desc.aaValues[0][0])
                    {
                        if (descThisDataset.aaValues[0].back() >=
                            desc.aaValues[0][0])
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dataset %s: values in indexing variable %s of "
                                "dimension %s are not the same as in other "
                                "datasets",
                                dataset.GetName().c_str(),
                                poDim->GetIndexingVariable()->GetName().c_str(),
                                desc.osName.c_str());
                            return std::nullopt;
                        }
                        desc.aaValues.insert(
                            desc.aaValues.begin(),
                            std::move(descThisDataset.aaValues[0]));
                    }
                    else
                    {
                        for (size_t i = 0; i < desc.aaValues.size(); ++i)
                        {
                            if (descThisDataset.aaValues[0][0] ==
                                desc.aaValues[i][0])
                            {
                                if (descThisDataset.aaValues[0] !=
                                    desc.aaValues[i])
                                {
                                    ReportError(
                                        CE_Failure, CPLE_AppDefined,
                                        "Dataset %s: values in indexing "
                                        "variable %s of dimension %s are not "
                                        "the same as in other datasets",
                                        dataset.GetName().c_str(),
                                        poDim->GetIndexingVariable()
                                            ->GetName()
                                            .c_str(),
                                        desc.osName.c_str());
                                    return std::nullopt;
                                }
                            }
                            else if (descThisDataset.aaValues[0][0] >
                                     desc.aaValues[i][0])
                            {
                                if (descThisDataset.aaValues[0][0] <=
                                    desc.aaValues[i].back())
                                {
                                    ReportError(
                                        CE_Failure, CPLE_AppDefined,
                                        "Dataset %s: values in indexing "
                                        "variable %s of dimension %s are "
                                        "overlapping with the ones of other "
                                        "datasets",
                                        dataset.GetName().c_str(),
                                        poDim->GetIndexingVariable()
                                            ->GetName()
                                            .c_str(),
                                        desc.osName.c_str());
                                    return std::nullopt;
                                }
                                if (i + 1 < desc.aaValues.size() &&
                                    descThisDataset.aaValues[0][0] <
                                        desc.aaValues[i + 1][0] &&
                                    descThisDataset.aaValues[0].back() >=
                                        desc.aaValues[i + 1][0])
                                {
                                    ReportError(
                                        CE_Failure, CPLE_AppDefined,
                                        "Dataset %s: values in indexing "
                                        "variable %s of dimension %s are "
                                        "overlapping with the ones of other "
                                        "datasets",
                                        dataset.GetName().c_str(),
                                        poDim->GetIndexingVariable()
                                            ->GetName()
                                            .c_str(),
                                        desc.osName.c_str());
                                    return std::nullopt;
                                }
                                desc.aaValues.insert(
                                    desc.aaValues.begin() + i + 1,
                                    std::move(descThisDataset.aaValues[0]));
                                break;
                            }
                        }
                    }
                }
            }
        }

        aaoSourceShortDimDesc.push_back(std::move(aoSourceShortDimDesc));
    }

    return aDimensions;
}

/************************************************************************/
/*                   GDALMdimMosaicAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALMdimMosaicAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_outputFormat.empty())
    {
        const auto aosFormats =
            CPLStringList(GDALGetOutputDriversForDatasetName(
                m_outputDataset.GetName().c_str(), GDAL_OF_MULTIDIM_RASTER,
                /* bSingleMatch = */ true,
                /* bWarn = */ true));
        if (aosFormats.size() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot guess driver for %s",
                        m_outputDataset.GetName().c_str());
            return false;
        }
        m_outputFormat = aosFormats[0];
    }
    auto poOutDrv =
        GetGDALDriverManager()->GetDriverByName(m_outputFormat.c_str());
    if (!poOutDrv)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Driver %s does not exist",
                    m_outputFormat.c_str());
        return false;
    }

    if (!m_array.empty() && m_array[0] != '/')
        m_array = "/" + m_array;

    std::shared_ptr<GDALMDArray> poFirstSourceArray;
    std::vector<std::vector<SourceShortDimDesc>> aaoSourceShortDimDesc;

    auto aDimensionsOpt =
        BuildDimensionDesc(poFirstSourceArray, aaoSourceShortDimDesc);
    if (!aDimensionsOpt.has_value())
        return false;
    const auto &aDimensions = aDimensionsOpt.value();

    CPLAssert(poFirstSourceArray);
    CPLAssert(aaoSourceShortDimDesc.size() == m_inputDatasets.size());

    auto poVRTDS = VRTDataset::CreateVRTMultiDimensional("", nullptr, nullptr);
    CPLAssert(poVRTDS);

    auto poDstGroup = poVRTDS->GetRootVRTGroup();
    CPLAssert(poDstGroup);

    // Create mosaicenated array dimensions
    std::vector<std::shared_ptr<GDALDimension>> apoDstDims;
    for (auto &desc : aDimensions)
    {
        uint64_t nDimSize64 = desc.nSize;
        if (!desc.aaValues.empty())
        {
            nDimSize64 = 0;
            for (const auto &aValues : desc.aaValues)
                nDimSize64 += aValues.size();
        }
        auto dstDim = poDstGroup->CreateDimension(desc.osName, desc.osType,
                                                  desc.osDirection, nDimSize64);
        if (!dstDim)
            return false;

        auto var = poDstGroup->CreateVRTMDArray(
            desc.osName, {dstDim}, GDALExtendedDataType::Create(GDT_Float64));
        if (!var)
            return false;

        for (const auto &attr : desc.attributes)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            auto dstAttr =
                var->CreateAttribute(attr->GetName(), attr->GetDimensionsSize(),
                                     attr->GetDataType());
            if (dstAttr)
            {
                auto raw(attr->ReadAsRaw());
                CPL_IGNORE_RET_VAL(dstAttr->Write(raw.data(), raw.size()));
            }
        }

        if (desc.aaValues.empty())
        {
            auto poSource = std::make_unique<VRTMDArraySourceRegularlySpaced>(
                desc.dfStart, desc.dfIncrement);
            var->AddSource(std::move(poSource));
        }
        else
        {
            const size_t nDimSize = static_cast<size_t>(nDimSize64);
            std::vector<GUInt64> anOffset = {0};
            std::vector<size_t> anCount = {nDimSize};
            std::vector<double> adfValues;
            adfValues.reserve(nDimSize);
            if (desc.nProgressionSign >= 0)
            {
                for (const auto &aValues : desc.aaValues)
                    adfValues.insert(adfValues.end(), aValues.begin(),
                                     aValues.end());
            }
            else
            {
                for (auto oIter = desc.aaValues.rbegin();
                     oIter != desc.aaValues.rend(); ++oIter)
                {
                    adfValues.insert(adfValues.end(), oIter->rbegin(),
                                     oIter->rend());
                }
            }
            std::vector<GByte> abyValues(nDimSize * sizeof(double));
            memcpy(abyValues.data(), adfValues.data(),
                   nDimSize * sizeof(double));
            auto poSource = std::make_unique<VRTMDArraySourceInlinedValues>(
                var.get(),
                /* bIsConstantValue = */ false, std::move(anOffset),
                std::move(anCount), std::move(abyValues));
            var->AddSource(std::move(poSource));
        }
        dstDim->SetIndexingVariable(std::move(var));
        apoDstDims.push_back(std::move(dstDim));
    }

    // Create mosaicenated array
    auto poDstArray = poDstGroup->CreateVRTMDArray(
        CPLGetFilename(m_array.c_str()), apoDstDims,
        poFirstSourceArray->GetDataType());

    GUInt64 nCurCost = 0;
    poDstArray->CopyFromAllExceptValues(poFirstSourceArray.get(), false,
                                        nCurCost, 0, nullptr, nullptr);

    // Add sources to mosaicenated array
    for (size_t iDS = 0; iDS < m_inputDatasets.size(); ++iDS)
    {
        const auto &dataset = m_inputDatasets[iDS];
        const auto &aoSourceShortDimDesc = aaoSourceShortDimDesc[iDS];

        const auto dimCount = aDimensions.size();
        std::vector<GUInt64> anSrcOffset(dimCount);
        std::vector<GUInt64> anCount(dimCount);
        std::vector<GUInt64> anDstOffset;
        CPLAssert(aoSourceShortDimDesc.size() == dimCount);

        for (size_t iDim = 0; iDim < dimCount; ++iDim)
        {
            const DimensionDesc &desc = aDimensions[iDim];
            const SourceShortDimDesc &sourceDesc = aoSourceShortDimDesc[iDim];
            if (sourceDesc.bIsRegularlySpaced)
            {
                const double dfPos =
                    (sourceDesc.dfStart - desc.dfStart) / desc.dfIncrement;
                anDstOffset.push_back(static_cast<uint64_t>(dfPos + 0.5));
            }
            else
            {
                uint64_t nPos = 0;
                bool bFound = false;
                for (size_t i = 0; i < desc.aaValues.size(); ++i)
                {
                    if (sourceDesc.dfStart == desc.aaValues[i][0])
                    {
                        bFound = true;
                        break;
                    }
                    else
                    {
                        nPos += desc.aaValues[i].size();
                    }
                }
                CPLAssert(bFound);
                CPL_IGNORE_RET_VAL(bFound);

                anDstOffset.push_back(nPos);
            }

            anCount[iDim] = sourceDesc.nSize;
        }

        std::vector<GUInt64> anStep(dimCount, 1);
        std::vector<int> anTransposedAxis;
        auto poSource = std::make_unique<VRTMDArraySourceFromArray>(
            poDstArray.get(), false, false, dataset.GetName().c_str(), m_array,
            std::string(), std::move(anTransposedAxis),
            std::string(),  // viewExpr
            std::move(anSrcOffset), std::move(anCount), std::move(anStep),
            std::move(anDstOffset));
        poDstArray->AddSource(std::move(poSource));
    }

    auto poOutDS = std::unique_ptr<GDALDataset>(poOutDrv->CreateCopy(
        m_outputDataset.GetName().c_str(), poVRTDS.get(), false,
        CPLStringList(m_creationOptions).List(), pfnProgress, pProgressData));

    if (poOutDS)
        m_outputDataset.Set(std::move(poOutDS));

    return m_outputDataset.GetDatasetRef() != nullptr;
}

//! @endcond
