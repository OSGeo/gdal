/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "scale" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SCALE_INCLUDED
#define GDALALG_RASTER_SCALE_INCLUDED

#include <limits>

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterScaleAlgorithm                          */
/************************************************************************/

class GDALRasterScaleAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "scale";
    static constexpr const char *DESCRIPTION =
        "Scale the values of the bands of a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_scale.html";

    explicit GDALRasterScaleAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_type{};
    int m_band = 0;
    double m_srcMin = std::numeric_limits<double>::quiet_NaN();
    double m_srcMax = std::numeric_limits<double>::quiet_NaN();
    double m_dstMin = std::numeric_limits<double>::quiet_NaN();
    double m_dstMax = std::numeric_limits<double>::quiet_NaN();
    double m_exponent = std::numeric_limits<double>::quiet_NaN();
    bool m_noClip = false;
};

/************************************************************************/
/*                 GDALRasterScaleAlgorithmStandalone                   */
/************************************************************************/

class GDALRasterScaleAlgorithmStandalone final : public GDALRasterScaleAlgorithm
{
  public:
    GDALRasterScaleAlgorithmStandalone()
        : GDALRasterScaleAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_SCALE_INCLUDED */
