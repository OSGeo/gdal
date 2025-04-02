/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "unscale" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_UNSCALE_INCLUDED
#define GDALALG_RASTER_UNSCALE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterUnscaleAlgorithm                        */
/************************************************************************/

class GDALRasterUnscaleAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "unscale";
    static constexpr const char *DESCRIPTION =
        "Convert scaled values of a raster dataset into unscaled values.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_unscale.html";

    explicit GDALRasterUnscaleAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_type{};
};

/************************************************************************/
/*                 GDALRasterUnscaleAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterUnscaleAlgorithmStandalone final
    : public GDALRasterUnscaleAlgorithm
{
  public:
    GDALRasterUnscaleAlgorithmStandalone()
        : GDALRasterUnscaleAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_UNSCALE_INCLUDED */
