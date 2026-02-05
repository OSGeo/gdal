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

#ifndef GDALALG_TEE_INCLUDED
#define GDALALG_TEE_INCLUDED

#include "gdalalg_abstract_pipeline.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"
#include "ogrsf_frmts.h"

#include <utility>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                     GDALTeeStepAlgorithmAbstract                     */
/************************************************************************/

class GDALTeeStepAlgorithmAbstract /* non final */
{
  public:
    static constexpr const char *NAME = "tee";
    static constexpr const char *DESCRIPTION =
        "Pipes the input into the output stream and side nested pipelines.";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    virtual ~GDALTeeStepAlgorithmAbstract();

    void CopyFilenameBindingsFrom(const GDALTeeStepAlgorithmAbstract *other);

    bool BindFilename(const std::string &filename,
                      GDALAbstractPipelineAlgorithm *alg,
                      const std::vector<std::string> &args);

    bool HasOutputString() const;

  protected:
    GDALTeeStepAlgorithmAbstract() = default;

    std::vector<GDALArgDatasetValue> m_pipelines{};
    std::map<std::string, std::pair<GDALAbstractPipelineAlgorithm *,
                                    std::vector<std::string>>>
        m_oMapNameToAlg{};
};

/************************************************************************/
/*                       GDALTeeStepAlgorithmBase                       */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
class GDALTeeStepAlgorithmBase /* non final */
    : public BaseStepAlgorithm,
      public GDALTeeStepAlgorithmAbstract
{
  public:
    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool CanBeMiddleStep() const override
    {
        return true;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool GeneratesFilesFromUserInput() const override
    {
        return true;
    }

    bool HasOutputString() const override
    {
        return GDALTeeStepAlgorithmAbstract::HasOutputString();
    }

  protected:
    explicit GDALTeeStepAlgorithmBase();

    int GetInputType() const override
    {
        return nDatasetType;
    }

    int GetOutputType() const override
    {
        return nDatasetType;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

/************************************************************************/
/*         GDALTeeStepAlgorithmBase::GDALTeeStepAlgorithmBase()         */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
GDALTeeStepAlgorithmBase<BaseStepAlgorithm,
                         nDatasetType>::GDALTeeStepAlgorithmBase()
    : BaseStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                        GDALPipelineStepAlgorithm::ConstructorOptions()
                            .SetAddDefaultArguments(false))
{
    this->AddInputDatasetArg(&this->m_inputDataset, 0, true).SetHidden();

    this->AddArg("tee-pipeline", 0, _("Nested pipeline"), &m_pipelines,
                 nDatasetType)
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
/*                 GDALTeeStepAlgorithmBase::RunStep()                  */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
bool GDALTeeStepAlgorithmBase<BaseStepAlgorithm, nDatasetType>::RunStep(
    GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = this->m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(this->m_outputDataset.GetName().empty());
    CPLAssert(!this->m_outputDataset.GetDatasetRef());

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
            this->ReportError(CE_Failure, CPLE_AppDefined,
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

        if (this->IsCalledFromCommandLine())
            subAlg->SetCalledFromCommandLine();

        bool ret = (subAlg->ParseCommandLineArguments(subAlgArgs) &&
                    subAlg->Run(pScaledProgress ? GDALScaledProgress : nullptr,
                                pScaledProgress.get()) &&
                    subAlg->Finalize());

        this->m_output += subAlg->GetOutputString();

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

        if (!ret)
            return false;

        ++iTeeDS;
    }

    this->m_outputDataset.Set(poSrcDS);
    return true;
}

/************************************************************************/
/*                        GDALTeeRasterAlgorithm                        */
/************************************************************************/

class GDALTeeRasterAlgorithm final
    : public GDALTeeStepAlgorithmBase<GDALRasterPipelineStepAlgorithm,
                                      GDAL_OF_RASTER>
{
  public:
    GDALTeeRasterAlgorithm() = default;

    ~GDALTeeRasterAlgorithm() override;
};

/************************************************************************/
/*                        GDALTeeVectorAlgorithm                        */
/************************************************************************/

class GDALTeeVectorAlgorithm final
    : public GDALTeeStepAlgorithmBase<GDALVectorPipelineStepAlgorithm,
                                      GDAL_OF_VECTOR>
{
  public:
    GDALTeeVectorAlgorithm() = default;

    ~GDALTeeVectorAlgorithm() override;
};

//! @endcond

#endif
