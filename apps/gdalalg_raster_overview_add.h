/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview add" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_OVERVIEW_ADD_INCLUDED
#define GDALALG_RASTER_OVERVIEW_ADD_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterOverviewAlgorithmAdd                    */
/************************************************************************/

class GDALRasterOverviewAlgorithmAdd final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "add";
    static constexpr const char *DESCRIPTION = "Adding overviews.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview_add.html";

    GDALRasterOverviewAlgorithmAdd();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;

    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_resampling{};
    std::vector<int> m_levels{};
    int m_minSize = 256;
    bool m_readOnly = false;
};

//! @endcond

#endif
