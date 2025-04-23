/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid average" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_average.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*    GDALVectorGridAverageAlgorithm::GDALVectorGridAverageAlgorithm()  */
/************************************************************************/

GDALVectorGridAverageAlgorithm::GDALVectorGridAverageAlgorithm()
    : GDALVectorGridAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddRadiusArg();
    AddRadius1AndRadius2Arg();
    AddAngleArg();
    AddMinPointsArg();
    AddMaxPointsArg();
    AddMinMaxPointsPerQuadrantArg();
    AddNodataArg();

    AddValidationAction(
        [this]()
        {
            bool ret = true;
            if (m_maxPoints < std::numeric_limits<int>::max() &&
                m_minPointsPerQuadrant == 0 &&
                m_maxPointsPerQuadrant == std::numeric_limits<int>::max())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'min-points-per-quadrant' and/or "
                            "'max-points-per-quadrant' should be defined when "
                            "'max-points' is.");
                ret = false;
            }
            return ret;
        });
}

/************************************************************************/
/*               GDALVectorGridAverageAlgorithm::RunImpl()              */
/************************************************************************/

std::string GDALVectorGridAverageAlgorithm::GetGridAlgorithm() const
{
    std::string ret =
        CPLSPrintf("average:angle=%.17g:nodata=%.17g", m_angle, m_nodata);
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
    if (m_maxPoints < std::numeric_limits<int>::max())
        ret += CPLSPrintf(":max_points=%d", m_maxPoints);
    if (m_minPointsPerQuadrant > 0)
        ret +=
            CPLSPrintf(":min_points_per_quadrant=%d", m_minPointsPerQuadrant);
    if (m_maxPointsPerQuadrant < std::numeric_limits<int>::max())
        ret +=
            CPLSPrintf(":max_points_per_quadrant=%d", m_maxPointsPerQuadrant);
    return ret;
}

//! @endcond
