/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster zonal-stats" subcommand
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_ZONAL_STATS_INCLUDED
#define GDALALG_RASTER_ZONAL_STATS_INCLUDED

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterZonalStatsAlgorithm                      */
/************************************************************************/

class GDALRasterZonalStatsAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "zonal-stats";
    static constexpr const char *DESCRIPTION =
        "Calculate raster zonal statistics";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_zonal_stats.html";

    explicit GDALRasterZonalStatsAlgorithm(bool bStandalone = false);

    bool CanBeFirstStep() const override
    {
        return true;
    }

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_VECTOR;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_weights{};
    GDALArgDatasetValue m_zones{};
    std::string m_zonesLayer{};
    int m_zonesBand{0};
    std::vector<int> m_bands{};
    std::vector<std::string> m_stats{};
    std::vector<std::string> m_includeFields{};
    std::string m_strategy{};
    std::string m_memory{};
    std::string m_pixels{};
    int m_weightsBand{0};
};

/************************************************************************/
/*                 GDALRasterZonalStatsAlgorithmStandalone              */
/************************************************************************/

class GDALRasterZonalStatsAlgorithmStandalone final
    : public GDALRasterZonalStatsAlgorithm
{
  public:
    GDALRasterZonalStatsAlgorithmStandalone()
        : GDALRasterZonalStatsAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterZonalStatsAlgorithmStandalone() override;
};

//! @endcond

#endif
