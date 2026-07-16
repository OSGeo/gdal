/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "pansharpen" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PANSHARPEN_INCLUDED
#define GDALALG_RASTER_PANSHARPEN_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterPansharpenAlgorithm                     */
/************************************************************************/

class GDALRasterPansharpenAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "pansharpen";
    static constexpr const char *DESCRIPTION =
        "Perform a pansharpen operation.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pansharpen.html";

    explicit GDALRasterPansharpenAlgorithm(bool standaloneStep = false);

  private:
    static ConstructorOptions GetConstructorOptions(bool standaloneStep);
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<GDALArgDatasetValue> m_spectralDatasets{};
    std::string m_resampling = "cubic";
    std::vector<double> m_weights{};
    double m_nodata = 0;
    std::string m_spatialExtentAdjustment = "union";
    int m_bitDepth = 0;
    int m_numThreads = 0;

    // Work variables
    std::string m_numThreadsStr{"ALL_CPUS"};
};

/************************************************************************/
/*               GDALRasterPansharpenAlgorithmStandalone                */
/************************************************************************/

class GDALRasterPansharpenAlgorithmStandalone final
    : public GDALRasterPansharpenAlgorithm
{
  public:
    GDALRasterPansharpenAlgorithmStandalone()
        : GDALRasterPansharpenAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterPansharpenAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_PANSHARPEN_INCLUDED */
