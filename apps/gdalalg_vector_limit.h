/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "limit" step of "vector pipeline"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_LIMIT_INCLUDED
#define GDALALG_VECTOR_LIMIT_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorLimitAlgorithm                       */
/************************************************************************/

class GDALVectorLimitAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "limit";
    static constexpr const char *DESCRIPTION =
        "Truncate a vector dataset to no more than a specified number of "
        "features.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_pipeline.html";

    explicit GDALVectorLimitAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};
    int m_featureLimit{};
};

//! @endcond

#endif /* GDALALG_VECTOR_LIMIT_INCLUDED */
