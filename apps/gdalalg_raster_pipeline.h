/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIPELINE_INCLUDED
#define GDALALG_RASTER_PIPELINE_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterPipelineStepAlgorithm                    */
/************************************************************************/

class GDALRasterAlgorithmStepRegistry;

class GDALRasterPipelineStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    ~GDALRasterPipelineStepAlgorithm() override;

  protected:
    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    friend class GDALRasterPipelineAlgorithm;
    friend class GDALRasterMosaicStackCommonAlgorithm;

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER;
    }

    void SetOutputVRTCompatible(bool b);
};

/************************************************************************/
/*           GDALRasterPipelineNonNativelyStreamingAlgorithm            */
/************************************************************************/

class GDALRasterPipelineNonNativelyStreamingAlgorithm /* non-final */
    : public GDALRasterPipelineStepAlgorithm
{
  protected:
    GDALRasterPipelineNonNativelyStreamingAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL, bool standaloneStep);

    bool IsNativelyStreamingCompatible() const override;

    static std::unique_ptr<GDALDataset>
    CreateTemporaryDataset(int nWidth, int nHeight, int nBands,
                           GDALDataType eDT, bool bTiledIfPossible,
                           GDALDataset *poSrcDSForMetadata,
                           bool bCopyMetadata = true);
    static std::unique_ptr<GDALDataset>
    CreateTemporaryCopy(GDALAlgorithm *poAlg, GDALDataset *poSrcDS,
                        int nSingleBand, bool bTiledIfPossible,
                        GDALProgressFunc pfnProgress, void *pProgressData);
};

/************************************************************************/
/*                   GDALRasterAlgorithmStepRegistry                    */
/************************************************************************/

class GDALRasterAlgorithmStepRegistry : public virtual GDALAlgorithmRegistry
{
  public:
    GDALRasterAlgorithmStepRegistry() = default;
    ~GDALRasterAlgorithmStepRegistry() override;

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm>
    bool Register(const std::string &name = std::string())
    {
        static_assert(
            std::is_base_of_v<GDALRasterPipelineStepAlgorithm, MyAlgorithm>,
            "Algorithm is not a GDALRasterPipelineStepAlgorithm");

        AlgInfo info;
        info.m_name = name.empty() ? MyAlgorithm::NAME : name;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return GDALAlgorithmRegistry::Register(info);
    }
};

/************************************************************************/
/*                     GDALRasterPipelineAlgorithm                      */
/************************************************************************/

class GDALRasterPipelineAlgorithm final : public GDALAbstractPipelineAlgorithm
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Process a raster dataset applying several steps.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

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

    explicit GDALRasterPipelineAlgorithm(bool openForMixedRasterVector = false);

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    static void RegisterAlgorithms(GDALRasterAlgorithmStepRegistry &registry,
                                   bool forMixedPipeline);

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER;
    }

  protected:
    GDALRasterAlgorithmStepRegistry m_stepRegistry{};

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
        auto pipeline = std::make_unique<GDALRasterPipelineAlgorithm>();
        pipeline->m_bInnerPipeline = true;
        return pipeline;
    }
};

//! @endcond

#endif
