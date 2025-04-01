/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "color-map" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_COLOR_MAP_INCLUDED
#define GDALALG_RASTER_COLOR_MAP_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterColorMapAlgorithm                    */
/************************************************************************/

class GDALRasterColorMapAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "color-map";
    static constexpr const char *DESCRIPTION =
        "Generate a RGB or RGBA dataset from a single band, using a color "
        "map";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_color_map.html";

    explicit GDALRasterColorMapAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    std::string m_colorMap{};
    bool m_addAlpha = false;
    std::string m_colorSelection = "interpolate";
};

/************************************************************************/
/*                  GDALRasterColorMapAlgorithmStandalone               */
/************************************************************************/

class GDALRasterColorMapAlgorithmStandalone final
    : public GDALRasterColorMapAlgorithm
{
  public:
    GDALRasterColorMapAlgorithmStandalone()
        : GDALRasterColorMapAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_COLOR_MAP_INCLUDED */
