/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid invdist" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_invdist.h"

#include <limits>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*    GDALVectorGridInvdistAlgorithm::GDALVectorGridInvdistAlgorithm()  */
/************************************************************************/

GDALVectorGridInvdistAlgorithm::GDALVectorGridInvdistAlgorithm()
    : GDALVectorGridAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddArg("power", 0, _("Weighting power"), &m_power).SetDefault(m_power);
    AddArg("smoothing", 0, _("Smoothing parameter"), &m_smoothing)
        .SetDefault(m_smoothing);

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

            if (m_minPoints > 0 && m_radius == 0 && m_radius1 == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'radius' or 'radius1' and 'radius2' should be "
                            "defined when 'min-points' is.");
                ret = false;
            }

            if (m_maxPoints < std::numeric_limits<int>::max() &&
                m_radius == 0 && m_radius1 == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'radius' or 'radius1' and 'radius2' should be "
                            "defined when 'max-points' is.");
                ret = false;
            }
            return ret;
        });
}

/************************************************************************/
/*               GDALVectorGridInvdistAlgorithm::RunImpl()              */
/************************************************************************/

std::string GDALVectorGridInvdistAlgorithm::GetGridAlgorithm() const
{
    std::string ret = CPLSPrintf(
        "invdist:power=%.17g:smoothing=%.17g:angle=%.17g:nodata=%.17g", m_power,
        m_smoothing, m_angle, m_nodata);
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
