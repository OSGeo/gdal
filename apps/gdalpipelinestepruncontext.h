/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster/vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALPIPELINESTEPRUNCONTEXT_INCLUDED
#define GDALPIPELINESTEPRUNCONTEXT_INCLUDED

//! @cond Doxygen_Suppress

#include "cpl_progress.h"

/************************************************************************/
/*                      GDALPipelineStepRunContext                      */
/************************************************************************/

class GDALPipelineStepAlgorithm;

class GDALPipelineStepRunContext
{
  public:
    GDALPipelineStepRunContext() = default;

    // Progress callback to use during execution of the step
    GDALProgressFunc m_pfnProgress = nullptr;
    void *m_pProgressData = nullptr;

    // If there is a step in the pipeline immediately following step to which
    // this instance of GDALRasterPipelineStepRunContext is passed, and that
    // this next step is usable by the current step (as determined by
    // CanHandleNextStep()), then this member will point to this next step.
    GDALPipelineStepAlgorithm *m_poNextUsableStep = nullptr;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALPipelineStepRunContext)
};

//! @endcond

#endif
