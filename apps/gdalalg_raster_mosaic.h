/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster mosaic" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_MOSAIC_INCLUDED
#define GDALALG_RASTER_MOSAIC_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterMosaicAlgorithm                        */
/************************************************************************/

class GDALRasterMosaicAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "mosaic";
    static constexpr const char *DESCRIPTION =
        "Build a mosaic, either virtual (VRT) or materialized.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_mosaic.html";

    explicit GDALRasterMosaicAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<GDALArgDatasetValue> m_inputDatasets{};
    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    std::string m_resolution{};
    std::vector<double> m_bbox{};
    bool m_targetAlignedPixels = false;
    std::vector<double> m_srcNoData{};
    std::vector<double> m_dstNoData{};
    std::vector<int> m_bands{};
    bool m_hideNoData = false;
    bool m_addAlpha = false;
};

//! @endcond

#endif
