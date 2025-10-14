/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector create-point"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CREATE_POINT_INCLUDED
#define GDALALG_VECTOR_CREATE_POINT_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALVectorCreatePointAlgorithm                       */
/************************************************************************/

class GDALVectorCreatePointAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "create-point";
    static constexpr const char *DESCRIPTION =
        "Create a point geometries from attribute fields";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_create_point.html";

    explicit GDALVectorCreatePointAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_xField{};
    std::string m_yField{};
    std::string m_zField{};
    std::string m_mField{};
    std::string m_dstCrs{};
};

/************************************************************************/
/*                 GDALVectorCreatePointAlgorithmStandalone             */
/************************************************************************/

class GDALVectorCreatePointAlgorithmStandalone final
    : public GDALVectorCreatePointAlgorithm
{
  public:
    GDALVectorCreatePointAlgorithmStandalone()
        : GDALVectorCreatePointAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCreatePointAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CREATE_POINT_INCLUDED */
