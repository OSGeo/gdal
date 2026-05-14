/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster viewshed" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_VIEWSHED_INCLUDED
#define GDALALG_RASTER_VIEWSHED_INCLUDED

#include "gdalalg_raster_pipeline.h"
#include "viewshed/viewshed_types.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterViewshedAlgorithm                      */
/************************************************************************/

class GDALRasterViewshedAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "viewshed";
    static constexpr const char *DESCRIPTION =
        "Compute the viewshed of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_viewshed.html";

    explicit GDALRasterViewshedAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<double> m_observerPos{};
    gdal::viewshed::Options m_opts{};

    std::string m_outputMode = "normal";
    int m_band = 1;
    int m_numThreads = 3;
    GDALArgDatasetValue m_sdFilename{};

    // Work variables
    std::string m_numThreadsStr{};
};

/************************************************************************/
/*                GDALRasterViewshedAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterViewshedAlgorithmStandalone final
    : public GDALRasterViewshedAlgorithm
{
  public:
    GDALRasterViewshedAlgorithmStandalone()
        : GDALRasterViewshedAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterViewshedAlgorithmStandalone() override;
};

//! @endcond

#endif
