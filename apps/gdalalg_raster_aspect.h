/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "aspect" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_ASPECT_INCLUDED
#define GDALALG_RASTER_ASPECT_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterAspectAlgorithm                      */
/************************************************************************/

class GDALRasterAspectAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "aspect";
    static constexpr const char *DESCRIPTION = "Generate an aspect map";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_aspect.html";

    explicit GDALRasterAspectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    std::string m_convention = "azimuth";
    std::string m_gradientAlg = "Horn";
    bool m_zeroForFlat = false;
    bool m_noEdges = false;
};

/************************************************************************/
/*                 GDALRasterAspectAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterAspectAlgorithmStandalone final
    : public GDALRasterAspectAlgorithm
{
  public:
    GDALRasterAspectAlgorithmStandalone()
        : GDALRasterAspectAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_ASPECT_INCLUDED */
