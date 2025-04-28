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

#include "gdalalg_clip_common.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterClipAlgorithm                        */
/************************************************************************/

class GDALRasterClipAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm,
      public GDALClipCommon
{
  public:
    static constexpr const char *NAME = "clip";
    static constexpr const char *DESCRIPTION = "Clip a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_clip.html";

    explicit GDALRasterClipAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    bool m_onlyBBOX{false};
    bool m_allowExtentOutsideSource{false};
    bool m_addAlpha{false};
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
