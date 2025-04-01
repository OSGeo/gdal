/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "slope" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SLOPE_INCLUDED
#define GDALALG_RASTER_SLOPE_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterSlopeAlgorithm                       */
/************************************************************************/

class GDALRasterSlopeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "slope";
    static constexpr const char *DESCRIPTION = "Generate a slope map";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_slope.html";

    explicit GDALRasterSlopeAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    std::string m_unit = "degree";
    double m_xscale = std::numeric_limits<double>::quiet_NaN();
    double m_yscale = std::numeric_limits<double>::quiet_NaN();
    std::string m_gradientAlg = "Horn";
    bool m_noEdges = false;
};

/************************************************************************/
/*                 GDALRasterSlopeAlgorithmStandalone                   */
/************************************************************************/

class GDALRasterSlopeAlgorithmStandalone final : public GDALRasterSlopeAlgorithm
{
  public:
    GDALRasterSlopeAlgorithmStandalone()
        : GDALRasterSlopeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_SLOPE_INCLUDED */
