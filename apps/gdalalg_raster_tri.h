/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "tri" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_TRI_INCLUDED
#define GDALALG_RASTER_TRI_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALRasterTRIAlgorithm                       */
/************************************************************************/

class GDALRasterTRIAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "tri";
    static constexpr const char *DESCRIPTION =
        "Generate a Terrain Ruggedness Index (TRI) map";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_tri.html";

    explicit GDALRasterTRIAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int m_band = 1;
    std::string m_algorithm = "Riley";
    bool m_noEdges = false;
};

/************************************************************************/
/*                    GDALRasterTRIAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterTRIAlgorithmStandalone final : public GDALRasterTRIAlgorithm
{
  public:
    GDALRasterTRIAlgorithmStandalone()
        : GDALRasterTRIAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_TRI_INCLUDED */
