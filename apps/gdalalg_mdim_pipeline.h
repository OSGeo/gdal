/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_PIPELINE_INCLUDED
#define GDALALG_MDIM_PIPELINE_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_abstract_pipeline.h"
#include "gdalmdimpipelinestepalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALMdimAlgorithmStepRegistry                     */
/************************************************************************/

class GDALMdimAlgorithmStepRegistry : public virtual GDALAlgorithmRegistry
{
  public:
    GDALMdimAlgorithmStepRegistry() = default;
    ~GDALMdimAlgorithmStepRegistry() override;

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm>
    bool Register(const std::string &name = std::string())
    {
        static_assert(
            std::is_base_of_v<GDALMdimPipelineStepAlgorithm, MyAlgorithm>,
            "Algorithm is not a GDALMdimPipelineStepAlgorithm");

        AlgInfo info;
        info.m_name = name.empty() ? MyAlgorithm::NAME : name;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return GDALAlgorithmRegistry::Register(info);
    }
};

/************************************************************************/
/*                      GDALMdimPipelineAlgorithm                       */
/************************************************************************/

class GDALMdimPipelineAlgorithm final : public GDALAbstractPipelineAlgorithm
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a multidimensional dataset applying several steps.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_pipeline.html";

    explicit GDALMdimPipelineAlgorithm(bool openForMixedMdimVector = false);

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    static void RegisterAlgorithms(GDALMdimAlgorithmStepRegistry &registry,
                                   bool forMixedPipeline);

    int GetInputType() const override
    {
        return GDAL_OF_MULTIDIM_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_MULTIDIM_RASTER;
    }

  protected:
    GDALMdimAlgorithmStepRegistry m_stepRegistry{};

    GDALAlgorithmRegistry &GetStepRegistry() override
    {
        return m_stepRegistry;
    }

    const GDALAlgorithmRegistry &GetStepRegistry() const override
    {
        return m_stepRegistry;
    }

  private:
    std::unique_ptr<GDALAbstractPipelineAlgorithm>
    CreateNestedPipeline() const override
    {
        auto pipeline = std::make_unique<GDALMdimPipelineAlgorithm>();
        pipeline->m_bInnerPipeline = true;
        return pipeline;
    }
};

//! @endcond

#endif
