/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_WRITE_INCLUDED
#define GDALALG_RASTER_WRITE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterWriteAlgorithm                       */
/************************************************************************/

class GDALRasterWriteAlgorithm final : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "write";
    static constexpr const char *DESCRIPTION = "Write a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

    GDALRasterWriteAlgorithm();

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    friend class GDALRasterPipelineStepAlgorithm;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif /* GDALALG_RASTER_WRITE_INCLUDED */
