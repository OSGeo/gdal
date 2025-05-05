/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "resize" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_RESIZE_INCLUDED
#define GDALALG_RASTER_RESIZE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterResizeAlgorithm                        */
/************************************************************************/

class GDALRasterResizeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "resize";
    static constexpr const char *DESCRIPTION =
        "Resize a raster dataset without changing the georeferenced extents.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_resize.html";

    explicit GDALRasterResizeAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<std::string> m_size{};
    std::string m_resampling{};
};

/************************************************************************/
/*                  GDALRasterResizeAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterResizeAlgorithmStandalone final
    : public GDALRasterResizeAlgorithm
{
  public:
    GDALRasterResizeAlgorithmStandalone()
        : GDALRasterResizeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_RESIZE_INCLUDED */
