/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "tpi" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_TPI_INCLUDED
#define GDALALG_RASTER_TPI_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALRasterTPIAlgorithm                       */
/************************************************************************/

class GDALRasterTPIAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "tpi";
    static constexpr const char *DESCRIPTION =
        "Generate a Topographic Position Index (TPI) map";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_tpi.html";

    explicit GDALRasterTPIAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    bool m_noEdges = false;
};

/************************************************************************/
/*                    GDALRasterTPIAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterTPIAlgorithmStandalone final : public GDALRasterTPIAlgorithm
{
  public:
    GDALRasterTPIAlgorithmStandalone()
        : GDALRasterTPIAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_TPI_INCLUDED */
