/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid nearest" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_nearest.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*    GDALVectorGridNearestAlgorithm::GDALVectorGridNearestAlgorithm()  */
/************************************************************************/

GDALVectorGridNearestAlgorithm::GDALVectorGridNearestAlgorithm()
    : GDALVectorGridAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddRadiusArg();
    AddRadius1AndRadius2Arg();
    AddAngleArg();
    AddNodataArg();
}

/************************************************************************/
/*               GDALVectorGridNearestAlgorithm::RunImpl()              */
/************************************************************************/

std::string GDALVectorGridNearestAlgorithm::GetGridAlgorithm() const
{
    std::string ret =
        CPLSPrintf("nearest:angle=%.17g:nodata=%.17g", m_angle, m_nodata);
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
    return ret;
}

//! @endcond
