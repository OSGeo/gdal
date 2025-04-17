/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "clip" step of "raster pipeline", or "gdal raster clip" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CLIP_INCLUDED
#define GDALALG_RASTER_CLIP_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterClipAlgorithm                        */
/************************************************************************/

class GDALRasterClipAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "clip";
    static constexpr const char *DESCRIPTION = "Clip a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_clip.html";

    explicit GDALRasterClipAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<double> m_bbox{};
    std::string m_bboxCrs{};
    bool m_allowExtentOutsideSource{false};
    GDALArgDatasetValue m_likeDataset{};
};

/************************************************************************/
/*                   GDALRasterClipAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterClipAlgorithmStandalone final : public GDALRasterClipAlgorithm
{
  public:
    GDALRasterClipAlgorithmStandalone()
        : GDALRasterClipAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_CLIP_INCLUDED */
