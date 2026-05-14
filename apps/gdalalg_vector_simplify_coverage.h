/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector simplify-coverage"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SIMPLIFY_COVERAGE_INCLUDED
#define GDALALG_VECTOR_SIMPLIFY_COVERAGE_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALVectorSimplifyCoverageAlgorithm                  */
/************************************************************************/

class GDALVectorSimplifyCoverageAlgorithm
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "simplify-coverage";
    static constexpr const char *DESCRIPTION =
        "Simplify shared boundaries of a polygonal vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_simplify_coverage.html";

    explicit GDALVectorSimplifyCoverageAlgorithm(bool standaloneStep = false);

    struct Options
    {
        double tolerance = 0;
        bool preserveBoundary = false;
    };

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};

    Options m_opts{};
};

/************************************************************************/
/*            GDALVectorSimplifyCoverageAlgorithmStandalone             */
/************************************************************************/

class GDALVectorSimplifyCoverageAlgorithmStandalone final
    : public GDALVectorSimplifyCoverageAlgorithm
{
  public:
    GDALVectorSimplifyCoverageAlgorithmStandalone()
        : GDALVectorSimplifyCoverageAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorSimplifyCoverageAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_SIMPLIFY_COVERAGE_INCLUDED */
