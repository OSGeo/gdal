/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "hillshade" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_HILLSHADE_INCLUDED
#define GDALALG_RASTER_HILLSHADE_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterHillshadeAlgorithm                     */
/************************************************************************/

class GDALRasterHillshadeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "hillshade";
    static constexpr const char *DESCRIPTION = "Generate a shaded relief map";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_hillshade.html";

    explicit GDALRasterHillshadeAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    double m_zfactor = 1;
    double m_xscale = std::numeric_limits<double>::quiet_NaN();
    double m_yscale = std::numeric_limits<double>::quiet_NaN();
    double m_azimuth = 315;
    double m_altitude = 45;
    std::string m_gradientAlg = "Horn";
    std::string m_variant = "regular";
    bool m_noEdges = false;
};

/************************************************************************/
/*               GDALRasterHillshadeAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterHillshadeAlgorithmStandalone final
    : public GDALRasterHillshadeAlgorithm
{
  public:
    GDALRasterHillshadeAlgorithmStandalone()
        : GDALRasterHillshadeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_HILLSHADE_INCLUDED */
