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

#include "gdalalg_raster_mosaic_stack_common.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterStackAlgorithm                       */
/************************************************************************/

class GDALRasterStackAlgorithm /* non final */
    : public GDALRasterMosaicStackCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "stack";
    static constexpr const char *DESCRIPTION =
        "Combine together input bands into a multi-band output, either virtual "
        "(VRT) or materialized.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_stack.html";

    explicit GDALRasterStackAlgorithm(bool bStandalone = false);

    bool CanBeFirstStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

/************************************************************************/
/*                  GDALRasterStackAlgorithmStandalone                  */
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
