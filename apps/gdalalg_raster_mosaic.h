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

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterMosaicAlgorithm                        */
/************************************************************************/

class GDALRasterMosaicAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "mosaic";
    static constexpr const char *DESCRIPTION =
        "Build a mosaic, either virtual (VRT) or materialized.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_mosaic.html";

    explicit GDALRasterMosaicAlgorithm(bool bStandalone = false);

    static ConstructorOptions GetConstructorOptions(bool standaloneStep);

  private:
    bool RunStep(GDALRasterPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_resolution{};
    std::vector<double> m_bbox{};
    bool m_targetAlignedPixels = false;
    std::vector<double> m_srcNoData{};
    std::vector<double> m_dstNoData{};
    std::vector<int> m_bands{};
    bool m_hideNoData = false;
    bool m_addAlpha = false;
    bool m_writeAbsolutePaths = false;
    std::string m_pixelFunction{};
    std::vector<std::string> m_pixelFunctionArgs{};
};

/************************************************************************/
/*                   GDALRasterMosaicAlgorithmStandalone                */
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
