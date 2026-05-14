/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector make-point"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_MAKE_POINT_INCLUDED
#define GDALALG_VECTOR_MAKE_POINT_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorMakePointAlgorithm                     */
/************************************************************************/

class GDALVectorMakePointAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "make-point";
    static constexpr const char *DESCRIPTION =
        "Create point geometries from attribute fields";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_make_point.html";

    explicit GDALVectorMakePointAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_xField{};
    std::string m_yField{};
    std::string m_zField{};
    std::string m_mField{};
    std::string m_dstCrs{};
};

/************************************************************************/
/*                GDALVectorMakePointAlgorithmStandalone                */
/************************************************************************/

class GDALVectorMakePointAlgorithmStandalone final
    : public GDALVectorMakePointAlgorithm
{
  public:
    GDALVectorMakePointAlgorithmStandalone()
        : GDALVectorMakePointAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorMakePointAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_MAKE_POINT_INCLUDED */
