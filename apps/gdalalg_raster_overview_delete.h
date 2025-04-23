/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_OVERVIEW_DELETE_INCLUDED
#define GDALALG_RASTER_OVERVIEW_DELETE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALRasterOverviewAlgorithmDelete                    */
/************************************************************************/

class GDALRasterOverviewAlgorithmDelete final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "delete";
    static constexpr const char *DESCRIPTION = "Deleting overviews.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview_delete.html";

    GDALRasterOverviewAlgorithmDelete();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;

    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    bool m_readOnly = false;
};

//! @endcond

#endif
