/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector geom simplify-coverage"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_SIMPLIFY_COVERAGE_INCLUDED
#define GDALALG_VECTOR_GEOM_SIMPLIFY_COVERAGE_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALVectorGeomSimplifyCoverageAlgorithm               */
/************************************************************************/

class GDALVectorGeomSimplifyCoverageAlgorithm
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "simplify-coverage";
    static constexpr const char *DESCRIPTION =
        "Simplify shared boundaries of a polygonal vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_simplify_coverage.html";

    explicit GDALVectorGeomSimplifyCoverageAlgorithm(
        bool standaloneStep = false);

    struct Options
    {
        double tolerance = 0;
        bool preserveBoundary = false;
    };

  private:
    bool RunStep(GDALVectorPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};

    Options m_opts{};
};

/************************************************************************/
/*                 GDALVectorGeomSimplifyCoverageStandalone                  */
/************************************************************************/

class GDALVectorGeomSimplifyCoverageStandalone final
    : public GDALVectorGeomSimplifyCoverageAlgorithm
{
  public:
    GDALVectorGeomSimplifyCoverageStandalone()
        : GDALVectorGeomSimplifyCoverageAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SIMPLIFY_COVERAGE_INCLUDED */
