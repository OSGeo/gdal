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
#include "gdalalg_raster_overview_refresh.h"

/************************************************************************/
/*                     GDALRasterOverviewAlgorithm                      */
/************************************************************************/

class GDALRasterOverviewAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "overview";
    static constexpr const char *DESCRIPTION =
        "Manage overviews of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview.html";

    explicit GDALRasterOverviewAlgorithm(bool standaloneStep = false)
        : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                          ConstructorOptions()
                                              .SetStandaloneStep(standaloneStep)
                                              .SetAddDefaultArguments(false))
    {
        if (standaloneStep)
        {
            RegisterSubAlgorithm<GDALRasterOverviewAlgorithmAddStandalone>();
            RegisterSubAlgorithm<GDALRasterOverviewAlgorithmDelete>();
            RegisterSubAlgorithm<GDALRasterOverviewAlgorithmRefresh>();
        }
        else
        {
            RegisterSubAlgorithm<GDALRasterOverviewAlgorithmAdd>();
        }
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

/************************************************************************/
/*                GDALRasterOverviewAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterOverviewAlgorithmStandalone final
    : public GDALRasterOverviewAlgorithm
{
  public:
    GDALRasterOverviewAlgorithmStandalone()
        : GDALRasterOverviewAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterOverviewAlgorithmStandalone() override;
};

//! @endcond

#endif
