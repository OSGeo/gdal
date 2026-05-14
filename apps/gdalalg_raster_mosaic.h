/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster mosaic" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_MOSAIC_INCLUDED
#define GDALALG_RASTER_MOSAIC_INCLUDED

#include "gdalalg_raster_mosaic_stack_common.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALRasterMosaicAlgorithm                       */
/************************************************************************/

class GDALRasterMosaicAlgorithm /* non final */
    : public GDALRasterMosaicStackCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "mosaic";
    static constexpr const char *DESCRIPTION =
        "Build a mosaic, either virtual (VRT) or materialized.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_mosaic.html";

    explicit GDALRasterMosaicAlgorithm(bool bStandalone = false);

    bool CanBeFirstStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    bool m_addAlpha = false;
    std::string m_pixelFunction{};
    std::vector<std::string> m_pixelFunctionArgs{};
};

/************************************************************************/
/*                 GDALRasterMosaicAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterMosaicAlgorithmStandalone final
    : public GDALRasterMosaicAlgorithm
{
  public:
    GDALRasterMosaicAlgorithmStandalone()
        : GDALRasterMosaicAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterMosaicAlgorithmStandalone() override;
};

//! @endcond

#endif
