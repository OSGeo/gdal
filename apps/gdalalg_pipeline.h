/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_PIPELINE_INCLUDED
#define GDALALG_PIPELINE_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalg_abstract_pipeline.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"

/************************************************************************/
/*                      GDALAlgorithmStepRegistry                       */
/************************************************************************/

class GDALAlgorithmStepRegistry final : public GDALRasterAlgorithmStepRegistry,
                                        public GDALVectorAlgorithmStepRegistry
{
  public:
    GDALAlgorithmStepRegistry() = default;
    ~GDALAlgorithmStepRegistry() override;

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm>
    bool Register(const std::string &name = std::string())
    {
        static_assert(std::is_base_of_v<GDALPipelineStepAlgorithm, MyAlgorithm>,
                      "Algorithm is not a GDALPipelineStepAlgorithm");

        AlgInfo info;
        info.m_name = name.empty() ? MyAlgorithm::NAME : name;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return GDALAlgorithmRegistry::Register(info);
    }
};

/************************************************************************/
/*                        GDALPipelineAlgorithm                         */
/************************************************************************/

class GDALPipelineAlgorithm final : public GDALAbstractPipelineAlgorithm

{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a dataset applying several steps.";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
            "+pipeline",
            "+gdal=pipeline",
#endif
        };
    }

    GDALPipelineAlgorithm();

    int GetInputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER | GDAL_OF_VECTOR;
    }

  protected:
    GDALAlgorithmStepRegistry m_stepRegistry{};

    GDALAlgorithmRegistry &GetStepRegistry() override
    {
        return m_stepRegistry;
    }

    const GDALAlgorithmRegistry &GetStepRegistry() const override
    {
        return m_stepRegistry;
    }

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  private:
    std::unique_ptr<GDALAbstractPipelineAlgorithm>
    CreateNestedPipeline() const override
    {
        auto pipeline = std::make_unique<GDALPipelineAlgorithm>();
        pipeline->m_bInnerPipeline = true;
        return pipeline;
    }
};

//! @endcond

#endif
