/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector check-geometry"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CHECK_GEOMETRY_INCLUDED
#define GDALALG_VECTOR_CHECK_GEOMETRY_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALVectorCheckGeometryAlgorithm                   */
/************************************************************************/

class GDALVectorCheckGeometryAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "check-geometry";
    static constexpr const char *DESCRIPTION =
        "Check a dataset for invalid geometries";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_check_geometry.html";

    explicit GDALVectorCheckGeometryAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_includeFields{};
    std::string m_geomField{};
    bool m_includeValid{false};
};

/************************************************************************/
/*              GDALVectorCheckGeometryAlgorithmStandalone              */
/************************************************************************/

class GDALVectorCheckGeometryAlgorithmStandalone final
    : public GDALVectorCheckGeometryAlgorithm
{
  public:
    GDALVectorCheckGeometryAlgorithmStandalone()
        : GDALVectorCheckGeometryAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCheckGeometryAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CHECK_GEOMETRY_INCLUDED */
