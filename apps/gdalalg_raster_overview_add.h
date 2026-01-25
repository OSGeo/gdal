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

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterOverviewAlgorithmAdd                    */
/************************************************************************/

class GDALRasterOverviewAlgorithmAdd /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "add";
    static constexpr const char *DESCRIPTION = "Adding overviews.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_overview_add.html";

    explicit GDALRasterOverviewAlgorithmAdd(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc, void *) override;

    std::vector<GDALArgDatasetValue> m_overviewSources{};
    std::string m_resampling{};
    std::vector<int> m_levels{};
    int m_minSize = 256;
    bool m_readOnly = false;
};

/************************************************************************/
/*               GDALRasterOverviewAlgorithmAddStandalone               */
/************************************************************************/

class GDALRasterOverviewAlgorithmAddStandalone final
    : public GDALRasterOverviewAlgorithmAdd
{
  public:
    GDALRasterOverviewAlgorithmAddStandalone()
        : GDALRasterOverviewAlgorithmAdd(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterOverviewAlgorithmAddStandalone() override;
};

//! @endcond

#endif
