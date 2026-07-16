/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster update" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_UPDATE_INCLUDED
#define GDALALG_RASTER_UPDATE_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include "gdalalg_clip_common.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALRasterUpdateAlgorithm                       */
/************************************************************************/

class GDALRasterUpdateAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm,
      public GDALClipCommon
{
  public:
    static constexpr const char *NAME = "update";
    static constexpr const char *DESCRIPTION =
        "Update the destination raster with the content of the input one.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_update.html";

    explicit GDALRasterUpdateAlgorithm(bool standaloneStep = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool CanBeMiddleStep() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool OutputDatasetAllowedBeforeRunningStep() const override
    {
        return true;
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_resampling{};
    std::vector<std::string> m_warpOptions{};
    std::vector<std::string> m_transformOptions{};
    double m_errorThreshold = std::numeric_limits<double>::quiet_NaN();
    bool m_noUpdateOverviews = false;
};

/************************************************************************/
/*                 GDALRasterUpdateAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterUpdateAlgorithmStandalone final
    : public GDALRasterUpdateAlgorithm
{
  public:
    GDALRasterUpdateAlgorithmStandalone()
        : GDALRasterUpdateAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterUpdateAlgorithmStandalone() override;
};

//! @endcond

#endif
