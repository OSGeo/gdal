/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "select" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SELECT_INCLUDED
#define GDALALG_RASTER_SELECT_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterSelectAlgorithm                         */
/************************************************************************/

class GDALRasterSelectAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "select";
    static constexpr const char *DESCRIPTION =
        "Select a subset of bands from a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_select.html";

    explicit GDALRasterSelectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<std::string> m_bands{};
    std::string m_mask{};
};

/************************************************************************/
/*                 GDALRasterSelectAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterSelectAlgorithmStandalone final
    : public GDALRasterSelectAlgorithm
{
  public:
    GDALRasterSelectAlgorithmStandalone()
        : GDALRasterSelectAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_SELECT_INCLUDED */
