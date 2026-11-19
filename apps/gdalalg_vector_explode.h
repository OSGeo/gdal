/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "explode" step of "vector pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_EXPLODE_INCLUDED
#define GDALALG_VECTOR_EXPLODE_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorExplodeAlgorithm                      */
/************************************************************************/

class GDALVectorExplodeAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "explode";
    static constexpr const char *DESCRIPTION =
        "Explode fields or geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_explode.html";

    explicit GDALVectorExplodeAlgorithm(bool standaloneStep = false);

  protected:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

  private:
    std::string m_activeLayer{};
    std::vector<std::string> m_fields{};
    std::vector<std::string> m_geomFields{};
    bool m_defaultGeom{false};
    std::string m_indexFieldName{};
};

/************************************************************************/
/*                 GDALVectorExplodeAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorExplodeAlgorithmStandalone final
    : public GDALVectorExplodeAlgorithm
{
  public:
    GDALVectorExplodeAlgorithmStandalone()
        : GDALVectorExplodeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorExplodeAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_EXPLODE_INCLUDED */
