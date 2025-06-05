/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster stack" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_STACK_INCLUDED
#define GDALALG_RASTER_STACK_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterStackAlgorithm                        */
/************************************************************************/

class GDALRasterStackAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "stack";
    static constexpr const char *DESCRIPTION =
        "Combine together input bands into a multi-band output, either virtual "
        "(VRT) or materialized.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_stack.html";

    explicit GDALRasterStackAlgorithm(bool bStandalone = false);

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
    bool m_writeAbsolutePaths = false;
};

/************************************************************************/
/*                   GDALRasterStackAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterStackAlgorithmStandalone final : public GDALRasterStackAlgorithm
{
  public:
    GDALRasterStackAlgorithmStandalone()
        : GDALRasterStackAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterStackAlgorithmStandalone() override;
};

//! @endcond

#endif
