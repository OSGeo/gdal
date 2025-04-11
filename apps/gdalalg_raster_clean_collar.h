/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster clean-collar" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CLEAN_COLLAR_INCLUDED
#define GDALALG_RASTER_CLEAN_COLLAR_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALRasterCleanCollarAlgorithm                      */
/************************************************************************/

class GDALRasterCleanCollarAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "clean-collar";
    static constexpr const char *DESCRIPTION =
        "Clean the collar of a raster dataset, removing noise.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_clean_collar.html";

    explicit GDALRasterCleanCollarAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_update = false;
    bool m_overwrite = false;
    std::vector<std::string> m_color{};
    int m_colorThreshold = 15;
    int m_pixelDistance = 2;
    bool m_addAlpha = false;
    bool m_addMask = false;
    std::string m_algorithm = "floodfill";
};

//! @endcond

#endif
