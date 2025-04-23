/******************************************************************************
 *
 * Project:  GDAL
* Purpose:  gdal "vector grid minimum/maximum/range/count/average-distance/average-distance-pts" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_data_metrics.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*            GDALVectorGridDataMetricsAbstractAlgorithm()              */
/************************************************************************/

GDALVectorGridDataMetricsAbstractAlgorithm::
    GDALVectorGridDataMetricsAbstractAlgorithm(const std::string &name,
                                               const std::string &description,
                                               const std::string &helpURL,
                                               const std::string &method)
    : GDALVectorGridAbstractAlgorithm(name, description, helpURL),
      m_method(method)
{
    AddRadiusArg();
    AddRadius1AndRadius2Arg();
    AddAngleArg();
    AddMinPointsArg();
    AddMinMaxPointsPerQuadrantArg();
    AddNodataArg();
}

/************************************************************************/
/*         GDALVectorGridDataMetricsAbstractAlgorithm::RunImpl()        */
/************************************************************************/

std::string GDALVectorGridDataMetricsAbstractAlgorithm::GetGridAlgorithm() const
{
    std::string ret = CPLSPrintf("%s:angle=%.17g:nodata=%.17g",
                                 m_method.c_str(), m_angle, m_nodata);
    if (m_radius > 0)
    {
        ret += CPLSPrintf(":radius=%.17g", m_radius);
    }
    else
    {
        if (m_radius1 > 0)
            ret += CPLSPrintf(":radius1=%.17g", m_radius1);
        if (m_radius2 > 0)
            ret += CPLSPrintf(":radius2=%.17g", m_radius2);
    }
    if (m_minPoints > 0)
        ret += CPLSPrintf(":min_points=%d", m_minPoints);
    if (m_minPointsPerQuadrant > 0)
        ret +=
            CPLSPrintf(":min_points_per_quadrant=%d", m_minPointsPerQuadrant);
    if (m_maxPointsPerQuadrant < std::numeric_limits<int>::max())
        ret +=
            CPLSPrintf(":max_points_per_quadrant=%d", m_maxPointsPerQuadrant);
    return ret;
}

//! @endcond
