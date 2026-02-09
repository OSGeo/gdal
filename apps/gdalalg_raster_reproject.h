/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_REPROJECT_INCLUDED
#define GDALALG_RASTER_REPROJECT_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterReprojectAlgorithm                     */
/************************************************************************/

class GDALRasterReprojectAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reproject";
    static constexpr const char *DESCRIPTION = "Reproject a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_reproject.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "warp"};
    }

    explicit GDALRasterReprojectAlgorithm(bool standaloneStep = false);

    bool CanHandleNextStep(GDALPipelineStepAlgorithm *) const override;

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_srsCrs{};
    std::string m_dstCrs{};
    std::string m_resampling{};
    std::vector<double> m_resolution{};
    std::vector<double> m_bbox{};
    std::string m_bboxCrs{};
    std::vector<int> m_size{};
    bool m_targetAlignedPixels = false;
    std::vector<std::string> m_srcNoData{};
    std::vector<std::string> m_dstNoData{};
    bool m_addAlpha = false;
    std::vector<std::string> m_warpOptions{};
    std::vector<std::string> m_transformOptions{};
    double m_errorThreshold = std::numeric_limits<double>::quiet_NaN();
    int m_numThreads = 0;
    GDALArgDatasetValue m_likeDataset{};

    // Work variables
    std::string m_numThreadsStr{"ALL_CPUS"};
};

/************************************************************************/
/*                GDALRasterReprojectAlgorithmStandalone                */
/************************************************************************/

class GDALRasterReprojectAlgorithmStandalone final
    : public GDALRasterReprojectAlgorithm
{
  public:
    GDALRasterReprojectAlgorithmStandalone()
        : GDALRasterReprojectAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterReprojectAlgorithmStandalone() override;
};

/************************************************************************/
/*                       GDALRasterReprojectUtils                       */
/************************************************************************/

class GDALRasterReprojectUtils final
{
  public:
    static void AddResamplingArg(GDALAlgorithm *alg, std::string &resampling);

    static void AddWarpOptTransformOptErrorThresholdArg(
        GDALAlgorithm *alg, std::vector<std::string> &warpOptions,
        std::vector<std::string> &transformOptions, double &errorThreshold);
};

//! @endcond

#endif /* GDALALG_RASTER_REPROJECT_INCLUDED */
