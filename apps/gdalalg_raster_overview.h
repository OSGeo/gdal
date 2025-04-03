/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster overview" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_OVERVIEW_INCLUDED
#define GDALALG_RASTER_OVERVIEW_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

#include "gdalalg_raster_overview_add.h"
#include "gdalalg_raster_overview_delete.h"

/************************************************************************/
/*                      GDALRasterOverviewAlgorithm                     */
/************************************************************************/

class GDALRasterOverviewAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "overview";
    static constexpr const char *DESCRIPTION =
        "Manage overviews of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview.html";

    GDALRasterOverviewAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALRasterOverviewAlgorithmAdd>();
        RegisterSubAlgorithm<GDALRasterOverviewAlgorithmDelete>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "raster overview\" program.");
        return false;
    }
};

//! @endcond

#endif
