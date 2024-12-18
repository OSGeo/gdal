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

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALRasterPipelineStepAlgorithm                       */
/************************************************************************/

class GDALRasterPipelineStepAlgorithm /* non final */ : public GDALAlgorithm
{
  protected:
    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    friend class GDALRasterPipelineAlgorithm;

    virtual bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) = 0;

    void AddInputArgs(bool openForMixedRasterVector, bool hiddenForCLI);
    void AddOutputArgs(bool hiddenForCLI);

    bool m_standaloneStep = false;

    // Input arguments
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::vector<std::string> m_inputLayerNames{};

    // Output arguments
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    std::string m_outputLayerName{};

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*                     GDALRasterPipelineAlgorithm                      */
/************************************************************************/

// This is an easter egg to pay tribute to PROJ pipeline syntax
// We accept "gdal vector +gdal=pipeline +step +gdal=read +input=in.tif +step +gdal=reproject +dst-crs=EPSG:32632 +step +gdal=write +output=out.tif +overwrite"
// as an alternative to (recommended):
// "gdal vector pipeline ! read in.tif ! reproject--dst-crs=EPSG:32632 ! write out.tif --overwrite"
#ifndef GDAL_PIPELINE_PROJ_NOSTALGIA
#define GDAL_PIPELINE_PROJ_NOSTALGIA
#endif

class GDALRasterPipelineAlgorithm final : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION = "Process a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

    static std::vector<std::string> GetAliases()
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

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    bool Finalize() override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    std::string GetUsageAsJSON() const override;

    GDALDataset *GetDatasetRef()
    {
        return m_inputDataset.GetDatasetRef();
    }

    /* cppcheck-suppress functionStatic */
    void SetDataset(GDALDataset *)
    {
    }

  private:
    std::string m_pipeline{};

    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::unique_ptr<GDALRasterPipelineStepAlgorithm>
    GetStepAlg(const std::string &name) const;

    GDALAlgorithmRegistry m_stepRegistry{};
    std::vector<std::unique_ptr<GDALRasterPipelineStepAlgorithm>> m_steps{};
};

//! @endcond

#endif
