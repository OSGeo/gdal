/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector clean-coverage"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CLEAN_COVERAGE_INCLUDED
#define GDALALG_VECTOR_CLEAN_COVERAGE_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorCleanCoverageAlgorithm                 */
/************************************************************************/

class GDALVectorCleanCoverageAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "clean-coverage";
    static constexpr const char *DESCRIPTION =
        "Alter polygon boundaries to make shared edges identical, removing "
        "gaps and overlaps";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_clean_coverage.html";

    explicit GDALVectorCleanCoverageAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    struct Options
    {
        double snappingTolerance = -1;
        double maximumGapWidth = 0;
        std::string mergeStrategy = "longest-border";
    };

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};

    Options m_opts{};
};

/************************************************************************/
/*                 GDALVectorCleanCoverageAlgorithmStandalone           */
/************************************************************************/

class GDALVectorCleanCoverageAlgorithmStandalone final
    : public GDALVectorCleanCoverageAlgorithm
{
  public:
    GDALVectorCleanCoverageAlgorithmStandalone()
        : GDALVectorCleanCoverageAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCleanCoverageAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CLEAN_COVERAGE_INCLUDED */
