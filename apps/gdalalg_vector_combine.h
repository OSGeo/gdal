/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector combine"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025-2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_COMBINE_INCLUDED
#define GDALALG_VECTOR_COMBINE_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorCombineAlgorithm                      */
/************************************************************************/

class GDALVectorCombineAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "combine";
    static constexpr const char *DESCRIPTION =
        "Combine features into collections";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_combine.html";

    explicit GDALVectorCombineAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_groupBy{};
    bool m_keepNested{false};
};

/************************************************************************/
/*                 GDALVectorCombineAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorCombineAlgorithmStandalone final
    : public GDALVectorCombineAlgorithm
{
  public:
    GDALVectorCombineAlgorithmStandalone()
        : GDALVectorCombineAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCombineAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_COMBINE_INCLUDED */
