/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "nodata-to-alpha" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_NODATA_TO_ALPHA_INCLUDED
#define GDALALG_RASTER_NODATA_TO_ALPHA_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterNoDataToAlphaAlgorithm                   */
/************************************************************************/

class GDALRasterNoDataToAlphaAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "nodata-to-alpha";
    static constexpr const char *DESCRIPTION =
        "Replace nodata value(s) with an alpha band.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_nodata_to_alpha.html";

    explicit GDALRasterNoDataToAlphaAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<double> m_nodata{};

    // Work variables
    std::unique_ptr<GDALDataset> m_tempDS{};
};

/************************************************************************/
/*              GDALRasterNoDataToAlphaAlgorithmStandalone              */
/************************************************************************/

class GDALRasterNoDataToAlphaAlgorithmStandalone final
    : public GDALRasterNoDataToAlphaAlgorithm
{
  public:
    GDALRasterNoDataToAlphaAlgorithmStandalone()
        : GDALRasterNoDataToAlphaAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterNoDataToAlphaAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_NODATA_TO_ALPHA_INCLUDED */
