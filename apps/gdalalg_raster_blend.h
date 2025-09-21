/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "blend" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_BLEND_INCLUDED
#define GDALALG_RASTER_BLEND_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterBlendAlgorithm                         */
/************************************************************************/

class GDALRasterBlendAlgorithm /* non final*/
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    explicit GDALRasterBlendAlgorithm(bool standaloneStep = false);

    static constexpr const char *NAME = "blend";
    static constexpr const char *DESCRIPTION =
        "Blend/compose two raster datasets";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_blend.html";

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool ValidateGlobal();

    GDALArgDatasetValue m_overlayDataset{};
    std::string m_operator{};
    static constexpr int OPACITY_INPUT_RANGE = 100;
    int m_opacity = OPACITY_INPUT_RANGE;
};

/************************************************************************/
/*                  GDALRasterBlendAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterBlendAlgorithmStandalone final : public GDALRasterBlendAlgorithm
{
  public:
    GDALRasterBlendAlgorithmStandalone()
        : GDALRasterBlendAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterBlendAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_COLOR_MERGE_INCLUDED */
