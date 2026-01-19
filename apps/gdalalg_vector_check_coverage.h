/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector check-coverage"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CHECK_COVERAGE_INCLUDED
#define GDALALG_VECTOR_CHECK_COVERAGE_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorCheckCoverageAlgorithm                 */
/************************************************************************/

class GDALVectorCheckCoverageAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "check-coverage";
    static constexpr const char *DESCRIPTION =
        "Check a polygon coverage for validity";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_check_coverage.html";

    explicit GDALVectorCheckCoverageAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_geomField{};
    bool m_includeValid{false};
    double m_maximumGapWidth{};
};

/************************************************************************/
/*                 GDALVectorCheckCoverageAlgorithmStandalone           */
/************************************************************************/

class GDALVectorCheckCoverageAlgorithmStandalone final
    : public GDALVectorCheckCoverageAlgorithm
{
  public:
    GDALVectorCheckCoverageAlgorithmStandalone()
        : GDALVectorCheckCoverageAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCheckCoverageAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CHECK_COVERAGE_INCLUDED */
