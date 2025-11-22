/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector collect"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_COLLECT_INCLUDED
#define GDALALG_VECTOR_COLLECT_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALVectorCollectAlgorithm                    */
/************************************************************************/

class GDALVectorCollectAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "collect";
    static constexpr const char *DESCRIPTION =
        "Combine features into collections";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_collect.html";

    explicit GDALVectorCollectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_groupBy{};
};

/************************************************************************/
/*                    GDALVectorCollectAlgorithmStandalone              */
/************************************************************************/

class GDALVectorCollectAlgorithmStandalone final
    : public GDALVectorCollectAlgorithm
{
  public:
    GDALVectorCollectAlgorithmStandalone()
        : GDALVectorCollectAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCollectAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_COLLECT_INCLUDED */
