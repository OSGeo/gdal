/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster rgb-to-palette" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_RGB_TO_PALETTE_INCLUDED
#define GDALALG_RASTER_RGB_TO_PALETTE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterRGBToPaletteAlgorithm                    */
/************************************************************************/

class GDALRasterRGBToPaletteAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "rgb-to-palette";
    static constexpr const char *DESCRIPTION =
        "Convert a RGB image into a pseudo-color / paletted image.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_rgb_to_palette.html";

    explicit GDALRasterRGBToPaletteAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    int m_colorCount = 256;
    std::string m_colorMap{};
    int m_dstNoData = -1;
    bool m_noDither = false;
    int m_bitDepth = 5;
};

/************************************************************************/
/*              GDALRasterRGBToPaletteAlgorithmStandalone               */
/************************************************************************/

class GDALRasterRGBToPaletteAlgorithmStandalone final
    : public GDALRasterRGBToPaletteAlgorithm
{
  public:
    GDALRasterRGBToPaletteAlgorithmStandalone()
        : GDALRasterRGBToPaletteAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterRGBToPaletteAlgorithmStandalone() override;
};

//! @endcond

#endif
