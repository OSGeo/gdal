/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_WRITE_INCLUDED
#define GDALALG_MDIM_WRITE_INCLUDED

#include "gdalmdimpipelinestepalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALMdimWriteAlgorithm                        */
/************************************************************************/

class GDALMdimWriteAlgorithm final : public GDALMdimPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "write";
    static constexpr const char *DESCRIPTION =
        "Write a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_pipeline.html";

    GDALMdimWriteAlgorithm();

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool GeneratesFilesFromUserInput() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    friend class GDALMdimPipelineStepAlgorithm;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif /* GDALALG_MDIM_WRITE_INCLUDED */
