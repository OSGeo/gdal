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

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterRGBToPaletteAlgorithm                   */
/************************************************************************/

class GDALRasterRGBToPaletteAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "rgb-to-palette";
    static constexpr const char *DESCRIPTION =
        "Convert a RGB image into a pseudo-color / paletted image.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_rgb_to_palette.html";

    GDALRasterRGBToPaletteAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;

    int m_colorCount = 256;
};

//! @endcond

#endif
