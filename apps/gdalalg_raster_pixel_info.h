/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pixelinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIXEL_INFO_INCLUDED
#define GDALALG_RASTER_PIXEL_INFO_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterPixelInfoAlgorithm                     */
/************************************************************************/

class GDALRasterPixelInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "pixel-info";
    static constexpr const char *DESCRIPTION =
        "Return information on a pixel of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pixel_info.html";

    GDALRasterPixelInfoAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_format = "json";
    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_output{};
    std::vector<int> m_band{};
    int m_overview = -1;
    std::vector<double> m_pos{};
    std::string m_posCrs{};
    std::string m_resampling = "nearest";

    void PrintLine(const std::string &str);
};

//! @endcond

#endif
