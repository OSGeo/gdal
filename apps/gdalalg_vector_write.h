/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_WRITE_INCLUDED
#define GDALALG_VECTOR_WRITE_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorWriteAlgorithm                       */
/************************************************************************/

class GDALVectorWriteAlgorithm final : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "write";
    static constexpr const char *DESCRIPTION = "Write a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_pipeline.html";

    GDALVectorWriteAlgorithm();

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

//! @endcond

#endif /* GDALALG_VECTOR_WRITE_INCLUDED */
