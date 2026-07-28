/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_READ_INCLUDED
#define GDALALG_RASTER_READ_INCLUDED

#include "gdalmdimpipelinestepalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALMdimReadAlgorithm                         */
/************************************************************************/

class GDALMdimReadAlgorithm final : public GDALMdimPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "read";
    static constexpr const char *DESCRIPTION =
        "Read a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_pipeline.html";

    explicit GDALMdimReadAlgorithm(bool openForMixedMdimVector = false);

    bool CanBeFirstStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif
