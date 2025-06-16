/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "color-merge" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_COLOR_MERGE_INCLUDED
#define GDALALG_RASTER_COLOR_MERGE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALRasterColorMergeAlgorithm                       */
/************************************************************************/

class GDALRasterColorMergeAlgorithm /* non final*/
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    explicit GDALRasterColorMergeAlgorithm(bool standaloneStep = false);

    static constexpr const char *NAME = "color-merge";
    static constexpr const char *DESCRIPTION =
        "Use a grayscale raster to replace the intensity of a RGB/RGBA dataset";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_color_merge.html";

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    GDALArgDatasetValue m_grayScaleDataset{};
};

/************************************************************************/
/*                GDALRasterColorMergeAlgorithmStandalone                */
/************************************************************************/

class GDALRasterColorMergeAlgorithmStandalone final
    : public GDALRasterColorMergeAlgorithm
{
  public:
    GDALRasterColorMergeAlgorithmStandalone()
        : GDALRasterColorMergeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterColorMergeAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_COLOR_MERGE_INCLUDED */
