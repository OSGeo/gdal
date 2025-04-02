/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_READ_INCLUDED
#define GDALALG_VECTOR_READ_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorReadAlgorithm                        */
/************************************************************************/

class GDALVectorReadAlgorithm final : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "read";
    static constexpr const char *DESCRIPTION = "Read a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_pipeline.html";

    GDALVectorReadAlgorithm();

  private:
    bool RunStep(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
