/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "roughness" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_ROUGHNESS_INCLUDED
#define GDALALG_RASTER_ROUGHNESS_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterRoughnessAlgorithm                     */
/************************************************************************/

class GDALRasterRoughnessAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "roughness";
    static constexpr const char *DESCRIPTION = "Generate a roughness map";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_roughness.html";

    explicit GDALRasterRoughnessAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    bool m_noEdges = false;
};

/************************************************************************/
/*                 GDALRasterRoughnessAlgorithmStandalone               */
/************************************************************************/

class GDALRasterRoughnessAlgorithmStandalone final
    : public GDALRasterRoughnessAlgorithm
{
  public:
    GDALRasterRoughnessAlgorithmStandalone()
        : GDALRasterRoughnessAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_ROUGHNESS_INCLUDED */
