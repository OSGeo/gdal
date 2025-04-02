/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid invdistnn" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_invdistnn.h"

#include <limits>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/* GDALVectorGridInvdistNNAlgorithm::GDALVectorGridInvdistNNAlgorithm() */
/************************************************************************/

GDALVectorGridInvdistNNAlgorithm::GDALVectorGridInvdistNNAlgorithm()
    : GDALVectorGridAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddArg("power", 0, _("Weighting power"), &m_power).SetDefault(m_power);
    AddArg("smoothing", 0, _("Smoothing parameter"), &m_smoothing)
        .SetDefault(m_smoothing);

    AddRadiusArg();
    AddMinPointsArg();
    m_maxPoints = 12;
    AddMaxPointsArg();
    AddMinMaxPointsPerQuadrantArg();
    AddNodataArg();
}

/************************************************************************/
/*             GDALVectorGridInvdistNNAlgorithm::RunImpl()              */
/************************************************************************/

std::string GDALVectorGridInvdistNNAlgorithm::GetGridAlgorithm() const
{
    std::string ret =
        CPLSPrintf("invdistnn:power=%.17g:smoothing=%.17g:nodata=%.17g",
                   m_power, m_smoothing, m_nodata);
    ret += CPLSPrintf(":radius=%.17g", m_radius);
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
