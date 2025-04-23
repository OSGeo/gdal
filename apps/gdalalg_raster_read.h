/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_READ_INCLUDED
#define GDALALG_RASTER_READ_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterReadAlgorithm                        */
/************************************************************************/

class GDALRasterReadAlgorithm final : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "read";
    static constexpr const char *DESCRIPTION = "Read a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

    GDALRasterReadAlgorithm();

  private:
    bool RunStep(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
