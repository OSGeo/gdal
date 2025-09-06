/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "tee" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_tee.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALTeeStepAlgorithm::GDALTeeStepAlgorithm()            */
/************************************************************************/

GDALTeeStepAlgorithm::GDALTeeStepAlgorithm(int nDatasetType)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetAddDefaultArguments(false)),
      m_nDatasetType(nDatasetType)
{
    AddArg("tee-pipeline", 0, _("Nested pipeline"), &m_pipelines,
           m_nDatasetType)
        .SetPositional()
        .SetMinCount(1)
        .SetMaxCount(INT_MAX)
        .SetMetaVar("PIPELINE")
        .SetPackedValuesAllowed(false)
        .SetDatasetInputFlags(GADV_NAME)
        .SetDatasetOutputFlags(GADV_NAME)
        .SetAutoOpenDataset(false);
}

/************************************************************************/
/*                    GDALTeeStepAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALTeeStepAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    // Backup filters
    std::vector<std::string> aosAttributeFilters;
    std::vector<std::unique_ptr<OGRGeometry>> apoSpatialFilters;
    for (auto *poLayer : poSrcDS->GetLayers())
    {
        const char *pszQueryString = poLayer->GetAttrQueryString();
        aosAttributeFilters.push_back(pszQueryString ? pszQueryString : "");
        const auto poSpatFilter = poLayer->GetSpatialFilter();
        apoSpatialFilters.push_back(std::unique_ptr<OGRGeometry>(
            poSpatFilter ? poSpatFilter->clone() : nullptr));
    }

    int iTeeDS = 0;
    for (const auto &dataset : m_pipelines)
    {
        const auto oIter = m_oMapNameToAlg.find(dataset.GetName());
        if (oIter == m_oMapNameToAlg.end())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "'%s' is not a valid nested pipeline",
                        dataset.GetName().c_str());
            return false;
        }
        auto subAlg = oIter->second.first;
        const auto &subAlgArgs = oIter->second.second;

        auto &subAlgInputDatasets = subAlg->GetInputDatasets();
        CPLAssert(subAlgInputDatasets.empty());
        subAlgInputDatasets.resize(1);
        subAlgInputDatasets[0].Set(poSrcDS);

        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(
                GDALCreateScaledProgress(
                    double(iTeeDS) / static_cast<int>(m_pipelines.size()),
                    double(iTeeDS + 1) / static_cast<int>(m_pipelines.size()),
                    pfnProgress, pProgressData),
                GDALDestroyScaledProgress);

        if (!(subAlg->ParseCommandLineArguments(subAlgArgs) &&
              subAlg->Run(pScaledProgress ? GDALScaledProgress : nullptr,
                          pScaledProgress.get()) &&
              subAlg->Finalize()))
        {
            // Error message emitted by above methods
            return false;
        }

        // Restore filters
        for (int i = 0; i < static_cast<int>(aosAttributeFilters.size()); ++i)
        {
            auto poLayer = poSrcDS->GetLayer(i);
            poLayer->SetAttributeFilter(aosAttributeFilters[i].empty()
                                            ? aosAttributeFilters[i].c_str()
                                            : nullptr);
            poLayer->SetSpatialFilter(apoSpatialFilters[i].get());
            poLayer->ResetReading();
        }

        ++iTeeDS;
    }

    m_outputDataset.Set(poSrcDS);
    return true;
}

GDALTeeRasterAlgorithm::~GDALTeeRasterAlgorithm() = default;

GDALTeeVectorAlgorithm::~GDALTeeVectorAlgorithm() = default;

//! @endcond
