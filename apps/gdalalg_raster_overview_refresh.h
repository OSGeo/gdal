/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview refresh" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_OVERVIEW_REFRESH_INCLUDED
#define GDALALG_RASTER_OVERVIEW_REFRESH_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALRasterOverviewAlgorithmRefresh                  */
/************************************************************************/

class GDALRasterOverviewAlgorithmRefresh final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "refresh";
    static constexpr const char *DESCRIPTION = "Refresh overviews.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview_refresh.html";

    GDALRasterOverviewAlgorithmRefresh();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;

    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    bool m_readOnly = false;

    std::string m_resampling{};
    std::vector<int> m_levels{};

    bool m_refreshFromSourceTimestamp = false;
    std::vector<double> m_refreshBbox{};
    std::vector<std::string> m_like{};
};

//! @endcond

#endif
