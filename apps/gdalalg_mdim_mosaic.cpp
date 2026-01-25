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
#include "cpl_vsi_virtual.h"
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
/*          GDALMdimMosaicAlgorithm::GDALMdimMosaicAlgorithm()          */
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
    AddArrayNameArg(&m_array, _("Name of array(s) to mosaic."));
}

/************************************************************************/
/*                          GetDimensionDesc()                          */
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
/*           GDALMdimMosaicAlgorithm::BuildArrayParameters()            */
/************************************************************************/

bool GDALMdimMosaicAlgorithm::BuildArrayParameters(
    const CPLStringList &aosInputDatasetNames,
    std::vector<ArrayParameters> &aoArrayParameters)
{
    for (const char *pszDatasetName : cpl::Iterate(aosInputDatasetNames))
    {
        auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            pszDatasetName, GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR,
            CPLStringList(m_inputFormats).List(),
            CPLStringList(m_openOptions).List(), nullptr));
        if (!poDS)
            return false;
        auto poRG = poDS->GetRootGroup();
        if (!poRG)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot get root group for dataset %s", pszDatasetName);
            return false;
        }
        std::vector<std::shared_ptr<GDALMDArray>> apoArrays;
        if (!m_array.empty())
        {
            for (const auto &array : m_array)
            {
                auto poArray = poRG->OpenMDArrayFromFullname(array);
                if (!poArray)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot find array %s in dataset %s",
                                array.c_str(), pszDatasetName);
                    return false;
                }
                apoArrays.push_back(std::move(poArray));
            }
        }
        else
        {
            for (const std::string &arrayName :
                 poRG->GetMDArrayFullNamesRecursive())
            {
                auto poArray = poRG->OpenMDArrayFromFullname(arrayName);
                if (!poArray)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot open array %s of dataset %s",
                                arrayName.c_str(), pszDatasetName);
                    return false;
                }
                if (poArray->GetDimensionCount() < 2)
                    continue;
                m_array.push_back(arrayName);
                apoArrays.push_back(std::move(poArray));
            }
            if (apoArrays.empty())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "No array of dimension count >= 2 found in dataset %s",
                    pszDatasetName);
                return false;
            }
        }

        if (aoArrayParameters.empty())
            aoArrayParameters.resize(apoArrays.size());

        for (size_t iArray = 0; iArray < apoArrays.size(); ++iArray)
        {
            auto &arrayParameters = aoArrayParameters[iArray];
            auto &poArray = apoArrays[iArray];
            if (poArray->GetDimensionCount() == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Array %s of dataset %s has no dimensions",
                            poArray->GetName().c_str(), pszDatasetName);
                return false;
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

            const auto anBlockSize = poArray->GetBlockSize();
            CPLAssert(anBlockSize.size() == poArray->GetDimensionCount());

            if (!arrayParameters.poFirstSourceArray)
            {
                arrayParameters.poFirstSourceArray = poArray;
                CPLAssert(arrayParameters.mosaicDimensions.empty());
                size_t iDim = 0;
                for (auto &poDim : poArray->GetDimensions())
                {
                    auto descOpt = GetDimensionDesc(pszDatasetName, poDim);
                    if (!descOpt.has_value())
                        return false;
                    auto &desc = descOpt.value();
                    AddToSourceShortDimDesc(desc, poDim->GetSize());
                    desc.nBlockSize = anBlockSize[iDim];
                    arrayParameters.mosaicDimensions.push_back(std::move(desc));
                    ++iDim;
                }
            }
            else
            {
                if (poArray->GetDimensionCount() !=
                    arrayParameters.mosaicDimensions.size())
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Array %s of dataset %s does not have the same "
                                "number of dimensions as in other datasets",
                                poArray->GetName().c_str(), pszDatasetName);
                    return false;
                }
                if (poArray->GetDataType() !=
                    arrayParameters.poFirstSourceArray->GetDataType())
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Array %s of dataset %s does not have the same "
                                "data type as in other datasets",
                                poArray->GetName().c_str(), pszDatasetName);
                    return false;
                }
                const void *poFirstRawNoData =
                    arrayParameters.poFirstSourceArray->GetRawNoDataValue();
                const void *poRawNoData = poArray->GetRawNoDataValue();
                if (!((!poFirstRawNoData && !poRawNoData) ||
                      (poFirstRawNoData && poRawNoData &&
                       memcmp(poFirstRawNoData, poRawNoData,
                              poArray->GetDataType().GetSize()) == 0)))
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Array %s of dataset %s does not have the same "
                                "nodata value as in other datasets",
                                poArray->GetName().c_str(), pszDatasetName);
                    return false;
                }
                const std::vector<std::shared_ptr<GDALDimension>> apoDims =
                    poArray->GetDimensions();
                for (size_t iDim = 0;
                     iDim < arrayParameters.mosaicDimensions.size(); ++iDim)
                {
                    DimensionDesc &desc =
                        arrayParameters.mosaicDimensions[iDim];
                    auto &poDim = apoDims[iDim];
                    if (poDim->GetName() != desc.osName)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Dimension %d of array %s of dataset %s does "
                            "not have the same name as in other datasets",
                            static_cast<int>(iDim), poArray->GetName().c_str(),
                            pszDatasetName);
                        return false;
                    }
                    if (desc.nBlockSize != anBlockSize[iDim])
                        desc.nBlockSize = 0;

                    auto descThisDatasetOpt =
                        GetDimensionDesc(pszDatasetName, poDim);
                    if (!descThisDatasetOpt.has_value())
                        return false;
                    auto &descThisDataset = descThisDatasetOpt.value();
                    AddToSourceShortDimDesc(descThisDataset, poDim->GetSize());
                    if (desc.aaValues.empty())
                    {
                        if (!descThisDataset.aaValues.empty())
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dimension %s of array %s of dataset %s "
                                "has irregularly-spaced values, contrary "
                                "to other datasets",
                                poDim->GetName().c_str(),
                                poArray->GetName().c_str(), pszDatasetName);
                            return false;
                        }
                        if (std::fabs(descThisDataset.dfIncrement -
                                      desc.dfIncrement) >
                            1e-10 * std::fabs(desc.dfIncrement))
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dimension %s of array %s of dataset %s is "
                                "indexed by a variable with spacing %g, "
                                "whereas it is %g in other datasets",
                                poDim->GetName().c_str(),
                                poArray->GetName().c_str(), pszDatasetName,
                                descThisDataset.dfIncrement, desc.dfIncrement);
                            return false;
                        }
                        const double dfPos =
                            (descThisDataset.dfStart - desc.dfStart) /
                            desc.dfIncrement;
                        if (std::fabs(std::round(dfPos) - dfPos) > 1e-3)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "Dimension %s of array %s of dataset "
                                        "%s is indexed "
                                        "by a variable whose start value is "
                                        "not aligned "
                                        "with the one of other datasets",
                                        poDim->GetName().c_str(),
                                        poArray->GetName().c_str(),
                                        pszDatasetName);
                            return false;
                        }
                        const double dfEnd = std::max(
                            desc.dfStart + static_cast<double>(desc.nSize) *
                                               desc.dfIncrement,
                            descThisDataset.dfStart +
                                static_cast<double>(descThisDataset.nSize) *
                                    descThisDataset.dfIncrement);
                        desc.dfStart =
                            std::min(desc.dfStart, descThisDataset.dfStart);
                        const double dfSize =
                            (dfEnd - desc.dfStart) / desc.dfIncrement;
                        constexpr double MAX_INTEGER_REPRESENTABLE =
                            static_cast<double>(1ULL << 53);
                        if (dfSize > MAX_INTEGER_REPRESENTABLE)
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dimension %s of array %s of dataset %s "
                                "would be too large if merged.",
                                poDim->GetName().c_str(),
                                poArray->GetName().c_str(), pszDatasetName);
                            return false;
                        }
                        desc.nSize = static_cast<uint64_t>(dfSize + 0.5);
                    }
                    else
                    {
                        if (descThisDataset.aaValues.empty())
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dimension %s of array %s of dataset %s "
                                "has regularly spaced labels, contrary to "
                                "other datasets",
                                poDim->GetName().c_str(),
                                poArray->GetName().c_str(), pszDatasetName);
                            return false;
                        }
                        if (descThisDataset.nProgressionSign !=
                            desc.nProgressionSign)
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Dataset %s: values in indexing variable %s of "
                                "dimension %s must be either increasing or "
                                "decreasing in all input datasets.",
                                pszDatasetName,
                                poDim->GetIndexingVariable()->GetName().c_str(),
                                desc.osName.c_str());
                            return false;
                        }
                        CPLAssert(descThisDataset.aaValues.size() == 1);
                        if (descThisDataset.aaValues[0][0] <
                            desc.aaValues[0][0])
                        {
                            if (descThisDataset.aaValues[0].back() >=
                                desc.aaValues[0][0])
                            {
                                ReportError(
                                    CE_Failure, CPLE_AppDefined,
                                    "Dataset %s: values in indexing variable "
                                    "%s of "
                                    "dimension %s are not the same as in other "
                                    "datasets",
                                    pszDatasetName,
                                    poDim->GetIndexingVariable()
                                        ->GetName()
                                        .c_str(),
                                    desc.osName.c_str());
                                return false;
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
                                            "variable %s of dimension %s are "
                                            "not "
                                            "the same as in other datasets",
                                            pszDatasetName,
                                            poDim->GetIndexingVariable()
                                                ->GetName()
                                                .c_str(),
                                            desc.osName.c_str());
                                        return false;
                                    }
                                }
                                else if (descThisDataset.aaValues[0][0] >
                                             desc.aaValues[i][0] &&
                                         (i + 1 == desc.aaValues.size() ||
                                          descThisDataset.aaValues[0][0] <
                                              desc.aaValues[i + 1][0]))
                                {
                                    if (descThisDataset.aaValues[0][0] <=
                                        desc.aaValues[i].back())
                                    {
                                        ReportError(
                                            CE_Failure, CPLE_AppDefined,
                                            "Dataset %s: values in indexing "
                                            "variable %s of dimension %s are "
                                            "overlapping with the ones of "
                                            "other "
                                            "datasets",
                                            pszDatasetName,
                                            poDim->GetIndexingVariable()
                                                ->GetName()
                                                .c_str(),
                                            desc.osName.c_str());
                                        return false;
                                    }
                                    if (i + 1 < desc.aaValues.size() &&
                                        descThisDataset.aaValues[0].back() >=
                                            desc.aaValues[i + 1][0])
                                    {
                                        ReportError(
                                            CE_Failure, CPLE_AppDefined,
                                            "Dataset %s: values in indexing "
                                            "variable %s of dimension %s are "
                                            "overlapping with the ones of "
                                            "other "
                                            "datasets",
                                            pszDatasetName,
                                            poDim->GetIndexingVariable()
                                                ->GetName()
                                                .c_str(),
                                            desc.osName.c_str());
                                        return false;
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

            arrayParameters.aaoSourceShortDimDesc.push_back(
                std::move(aoSourceShortDimDesc));
        }
    }

    return true;
}

/************************************************************************/
/*           GDALMdimMosaicAlgorithm::GetInputDatasetNames()            */
/************************************************************************/

bool GDALMdimMosaicAlgorithm::GetInputDatasetNames(
    GDALProgressFunc pfnProgress, void *pProgressData,
    CPLStringList &aosInputDatasetNames) const
{
    for (auto &ds : m_inputDatasets)
    {
        if (ds.GetName()[0] == '@')
        {
            auto f = VSIVirtualHandleUniquePtr(
                VSIFOpenL(ds.GetName().c_str() + 1, "r"));
            if (!f)
            {
                ReportError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                            ds.GetName().c_str() + 1);
                return false;
            }
            while (const char *filename = CPLReadLineL(f.get()))
            {
                aosInputDatasetNames.push_back(filename);
            }
        }
        else if (ds.GetName().find_first_of("*?[") != std::string::npos)
        {
            CPLStringList aosMatches(VSIGlob(ds.GetName().c_str(), nullptr,
                                             pfnProgress, pProgressData));
            for (const char *pszStr : aosMatches)
            {
                aosInputDatasetNames.push_back(pszStr);
            }
        }
        else
        {
            std::string osDatasetName = ds.GetName();
            if (!GetReferencePathForRelativePaths().empty())
            {
                osDatasetName = GDALDataset::BuildFilename(
                    osDatasetName.c_str(),
                    GetReferencePathForRelativePaths().c_str(), true);
            }
            aosInputDatasetNames.push_back(osDatasetName.c_str());
        }
    }
    return true;
}

/************************************************************************/
/*                  GDALMdimMosaicAlgorithm::RunImpl()                  */
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

    const bool bIsVRT = EQUAL(m_outputFormat.c_str(), "VRT");

    CPLStringList aosInputDatasetNames;
    const double dfIntermediatePercentage = bIsVRT ? 1.0 : 0.1;
    std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
        GDALCreateScaledProgress(0.0, dfIntermediatePercentage, pfnProgress,
                                 pProgressData),
        GDALDestroyScaledProgress);
    if (!GetInputDatasetNames(GDALScaledProgress, pScaledData.get(),
                              aosInputDatasetNames))
        return false;

    for (std::string &name : m_array)
    {
        if (!name.empty() && name[0] != '/')
            name = "/" + name;
    }

    std::vector<ArrayParameters> aoArrayParameters;
    if (!BuildArrayParameters(aosInputDatasetNames, aoArrayParameters))
    {
        return false;
    }

    auto poVRTDS = VRTDataset::CreateVRTMultiDimensional("", nullptr, nullptr);
    CPLAssert(poVRTDS);

    auto poDstGroup = poVRTDS->GetRootVRTGroup();
    CPLAssert(poDstGroup);

    std::map<std::string, std::shared_ptr<GDALDimension>>
        oMapAlreadyCreatedDims;

    // Iterate over arrays
    for (auto &arrayParameters : aoArrayParameters)
    {
        auto &poFirstSourceArray = arrayParameters.poFirstSourceArray;
        CPLAssert(poFirstSourceArray);
        auto &aaoSourceShortDimDesc = arrayParameters.aaoSourceShortDimDesc;
        CPLAssert(aaoSourceShortDimDesc.size() ==
                  static_cast<size_t>(aosInputDatasetNames.size()));

        // Create mosaic array dimensions
        std::vector<std::shared_ptr<GDALDimension>> apoDstDims;
        for (auto &desc : arrayParameters.mosaicDimensions)
        {
            auto oIterCreatedDims = oMapAlreadyCreatedDims.find(desc.osName);
            if (oIterCreatedDims != oMapAlreadyCreatedDims.end())
            {
                apoDstDims.push_back(oIterCreatedDims->second);
            }
            else
            {
                uint64_t nDimSize64 = desc.nSize;
                if (!desc.aaValues.empty())
                {
                    nDimSize64 = 0;
                    for (const auto &aValues : desc.aaValues)
                        nDimSize64 += aValues.size();
                }
                auto dstDim = poDstGroup->CreateDimension(
                    desc.osName, desc.osType, desc.osDirection, nDimSize64);
                if (!dstDim)
                    return false;

                auto var = poDstGroup->CreateVRTMDArray(
                    desc.osName, {dstDim},
                    GDALExtendedDataType::Create(GDT_Float64));
                if (!var)
                    return false;

                for (const auto &attr : desc.attributes)
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    auto dstAttr = var->CreateAttribute(
                        attr->GetName(), attr->GetDimensionsSize(),
                        attr->GetDataType());
                    if (dstAttr)
                    {
                        auto raw(attr->ReadAsRaw());
                        CPL_IGNORE_RET_VAL(
                            dstAttr->Write(raw.data(), raw.size()));
                    }
                }

                if (desc.aaValues.empty())
                {
                    auto poSource =
                        std::make_unique<VRTMDArraySourceRegularlySpaced>(
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
                    auto poSource =
                        std::make_unique<VRTMDArraySourceInlinedValues>(
                            var.get(),
                            /* bIsConstantValue = */ false, std::move(anOffset),
                            std::move(anCount), std::move(abyValues));
                    var->AddSource(std::move(poSource));
                }
                dstDim->SetIndexingVariable(std::move(var));
                oMapAlreadyCreatedDims[dstDim->GetName()] = dstDim;
                apoDstDims.push_back(std::move(dstDim));
            }
        }

        // Create mosaic array
        CPLStringList aosArrayCO;
        std::string osBlockSize;
        for (size_t i = 0; i < apoDstDims.size(); ++i)
        {
            if (i > 0)
                osBlockSize += ',';
            osBlockSize +=
                std::to_string(arrayParameters.mosaicDimensions[i].nBlockSize);
        }
        if (!osBlockSize.empty())
            aosArrayCO.SetNameValue("BLOCKSIZE", osBlockSize.c_str());

        auto poDstArray = poDstGroup->CreateVRTMDArray(
            CPLGetFilename(poFirstSourceArray->GetName().c_str()), apoDstDims,
            poFirstSourceArray->GetDataType(), aosArrayCO);
        if (!poDstArray)
            return false;

        GUInt64 nCurCost = 0;
        poDstArray->CopyFromAllExceptValues(poFirstSourceArray.get(), false,
                                            nCurCost, 0, nullptr, nullptr);

        // Add sources to mosaic array
        for (int iDS = 0; iDS < aosInputDatasetNames.size(); ++iDS)
        {
            const auto &aoSourceShortDimDesc = aaoSourceShortDimDesc[iDS];

            const auto dimCount = arrayParameters.mosaicDimensions.size();
            std::vector<GUInt64> anSrcOffset(dimCount);
            std::vector<GUInt64> anCount(dimCount);
            std::vector<GUInt64> anDstOffset;
            CPLAssert(aoSourceShortDimDesc.size() == dimCount);

            for (size_t iDim = 0; iDim < dimCount; ++iDim)
            {
                const DimensionDesc &desc =
                    arrayParameters.mosaicDimensions[iDim];
                const SourceShortDimDesc &sourceDesc =
                    aoSourceShortDimDesc[iDim];
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
                poDstArray.get(), false, false, aosInputDatasetNames[iDS],
                poFirstSourceArray->GetFullName(), std::string(),
                std::move(anTransposedAxis),
                std::string(),  // viewExpr
                std::move(anSrcOffset), std::move(anCount), std::move(anStep),
                std::move(anDstOffset));
            poDstArray->AddSource(std::move(poSource));
        }
    }

    pScaledData.reset(GDALCreateScaledProgress(dfIntermediatePercentage, 1.0,
                                               pfnProgress, pProgressData));
    auto poOutDS = std::unique_ptr<GDALDataset>(
        poOutDrv->CreateCopy(m_outputDataset.GetName().c_str(), poVRTDS.get(),
                             false, CPLStringList(m_creationOptions).List(),
                             GDALScaledProgress, pScaledData.get()));

    if (poOutDS)
        m_outputDataset.Set(std::move(poOutDS));

    return m_outputDataset.GetDatasetRef() != nullptr;
}

//! @endcond
