/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_reproject.h"
#include "gdalalg_raster_reproject.h"

#include "gdal_priv.h"

#include <map>
#include <set>
#include <utility>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALMdimReprojectAlgorithm::GDALMdimReprojectAlgorithm()       */
/************************************************************************/

GDALMdimReprojectAlgorithm::GDALMdimReprojectAlgorithm(bool standaloneStep)
    : GDALMdimPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetStandaloneStep(standaloneStep))
{
    AddArg(GDAL_ARG_NAME_OUTPUT_CRS, 'd', _("Output CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs")
        .AddHiddenAlias("dst-crs");

    GDALRasterReprojectUtils::AddResamplingArg(this, m_resampling);
}

namespace
{

/************************************************************************/
/*                       GDALMdimReprojectParams                        */
/************************************************************************/

struct GDALMdimReprojectParams
{
    GDALRIOResampleAlg eResampleAlg = GRIORA_NearestNeighbour;
    OGRSpatialReferenceRefCountedPtr poTargetSRS{};
};

/************************************************************************/
/*                        GDALMdimReprojectGroup                        */
/************************************************************************/

/** Wrapper around a source group object, that is essentially passthrough,
 * except with 2D+ arrays.
 */
class GDALMdimReprojectGroup final : public GDALGroup
{
    std::shared_ptr<GDALGroup> m_poSrcGroup{};
    const GDALMdimReprojectParams m_sParams;
    std::vector<std::string> m_aosArrayNames{};
    std::map<std::string, std::shared_ptr<GDALMDArray>> m_oMapArrays{};
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims{};

  public:
    GDALMdimReprojectGroup(const std::string &osParentName,
                           const std::shared_ptr<GDALGroup> &poSrcGroup,
                           const GDALMdimReprojectParams &sParams);

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        return m_poSrcGroup->GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override
    {
        return m_poSrcGroup->GetAttributes(papszOptions);
    }

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions = nullptr) const override
    {
        return m_poSrcGroup->GetGroupNames(papszOptions);
    }

    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override
    {
        auto poSrcChildGroup = m_poSrcGroup->OpenGroup(osName, papszOptions);
        if (!poSrcChildGroup)
            return nullptr;
        return std::make_shared<GDALMdimReprojectGroup>(
            GetFullName(), std::move(poSrcChildGroup), m_sParams);
    }

    std::vector<std::string> GetMDArrayNames(CSLConstList) const override
    {
        return m_aosArrayNames;
    }

    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string &osName,
                                             CSLConstList) const override
    {
        const auto oIter = m_oMapArrays.find(osName);
        if (oIter != m_oMapArrays.end())
            return oIter->second;
        return nullptr;
    }

    std::vector<std::shared_ptr<GDALDimension>>
        GetDimensions(CSLConstList) const override;
};

/************************************************************************/
/*           GDALMdimReprojectGroup::GDALMdimReprojectGroup()           */
/************************************************************************/

GDALMdimReprojectGroup::GDALMdimReprojectGroup(
    const std::string &osParentName,
    const std::shared_ptr<GDALGroup> &poSrcGroup,
    const GDALMdimReprojectParams &sParams)
    : GDALGroup(osParentName, poSrcGroup->GetName()), m_poSrcGroup(poSrcGroup),
      m_sParams(sParams)
{
    // We collect source arrays and dimensions, and remove dimensions that
    // are only referenced by the spatial dimensions of reprojected arrays.

    // First bool in pair: referenced by 1d variables (other than its own indexing variable)
    // Second bool in pair: referenced by >= 2d variables
    std::map<std::string, std::pair<bool, bool>> oMapArrayDims;
    std::map<std::string, std::shared_ptr<GDALDimension>> oMapNewDims;
    std::vector<std::shared_ptr<GDALMDArray>> apoIndexingVariables;

    for (const std::string &osName : m_poSrcGroup->GetMDArrayNames())
    {
        auto poSrcArray = m_poSrcGroup->OpenMDArray(osName);
        if (!poSrcArray)
            continue;
        if (poSrcArray->GetDimensionCount() == 0)
        {
            m_aosArrayNames.push_back(osName);
            m_oMapArrays[osName] = std::move(poSrcArray);
        }
        else if (poSrcArray->GetDimensionCount() == 1)
        {
            m_aosArrayNames.push_back(osName);
            const auto &poDim = poSrcArray->GetDimensions()[0];
            if (poDim->GetName() == osName)
            {
                apoIndexingVariables.push_back(poSrcArray);
            }
            else
            {
                oMapArrayDims[poDim->GetName()].first = true;
                m_oMapArrays[osName] = std::move(poSrcArray);
            }
        }
        else
        {
            std::vector<std::shared_ptr<GDALDimension>> apoNewDims(
                poSrcArray->GetDimensionCount());
            CPLStringList aosOptions;
            aosOptions.SetNameValue("PARENT_PATH", GetFullName().c_str());
            aosOptions.SetNameValue("NAME", osName.c_str());
            auto poDstArray = poSrcArray->GetResampled(
                apoNewDims, m_sParams.eResampleAlg, m_sParams.poTargetSRS.get(),
                aosOptions.List());
            if (poDstArray)
            {
                m_aosArrayNames.push_back(osName);
                CPLAssert(poDstArray->GetDimensionCount() ==
                          poSrcArray->GetDimensionCount());
                for (size_t i = 0; i < poDstArray->GetDimensionCount(); ++i)
                {
                    const auto &poSrcDim = poSrcArray->GetDimensions()[i];
                    const auto &poDstDim = poDstArray->GetDimensions()[i];
                    if (poSrcDim.get() != poDstDim.get())
                    {
                        oMapArrayDims[poSrcDim->GetName()].second = true;
                        oMapNewDims[poDstDim->GetName()] = poDstDim;

                        auto poVar = poDstDim->GetIndexingVariable();
                        if (poVar)
                        {
                            m_aosArrayNames.push_back(poVar->GetName());
                            m_oMapArrays[poVar->GetName()] = poVar;
                        }
                    }
                }
                m_oMapArrays[osName] = std::move(poDstArray);
            }
        }
    }

    for (auto &poArray : apoIndexingVariables)
    {
        auto oIter = oMapArrayDims.find(poArray->GetName());
        if (oIter == oMapArrayDims.end() || !oIter->second.second)
        {
            m_aosArrayNames.push_back(poArray->GetName());
            m_oMapArrays[poArray->GetName()] = poArray;
        }
    }

    for (const auto &poDim : m_poSrcGroup->GetDimensions())
    {
        auto oIter = oMapArrayDims.find(poDim->GetName());
        if (oIter == oMapArrayDims.end() || !oIter->second.second)
        {
            m_apoDims.push_back(poDim);
        }
    }
    for (const auto &[_, poDim] : oMapNewDims)
    {
        m_apoDims.push_back(poDim);
    }
}

/************************************************************************/
/*               GDALMdimReprojectGroup::GetDimensions()                */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
GDALMdimReprojectGroup::GetDimensions(CSLConstList) const
{
    return m_apoDims;
}

/************************************************************************/
/*                       GDALMdimReprojectDataset                       */
/************************************************************************/

class GDALMdimReprojectDataset final : public GDALDataset
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};

  public:
    GDALMdimReprojectDataset(GDALDataset *poSrcDS,
                             const GDALMdimReprojectParams &params)
    {
        auto poSrcRootGroup = poSrcDS->GetRootGroup();
        if (poSrcRootGroup)
        {
            m_poRootGroup = std::make_shared<GDALMdimReprojectGroup>(
                std::string(), std::move(poSrcRootGroup), params);
        }
    }

    std::shared_ptr<GDALGroup> GetRootGroup() const override;
};

std::shared_ptr<GDALGroup> GDALMdimReprojectDataset::GetRootGroup() const
{
    return m_poRootGroup;
}

}  // namespace

/************************************************************************/
/*                GDALMdimReprojectAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALMdimReprojectAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    GDALMdimReprojectParams sParams;
    sParams.eResampleAlg = GDALRasterIOGetResampleAlg(m_resampling.c_str());
    if (!m_dstCrs.empty())
    {
        sParams.poTargetSRS = OGRSpatialReferenceRefCountedPtr::makeInstance();
        sParams.poTargetSRS->SetFromUserInput(m_dstCrs.c_str());
    }
    m_outputDataset.Set(
        std::make_unique<GDALMdimReprojectDataset>(poSrcDS, sParams));

    return true;
}

GDALMdimReprojectAlgorithmStandalone::~GDALMdimReprojectAlgorithmStandalone() =
    default;

//! @endcond
