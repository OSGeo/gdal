/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid linear" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid_linear.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*    GDALVectorGridLinearAlgorithm::GDALVectorGridLinearAlgorithm()    */
/************************************************************************/

GDALVectorGridLinearAlgorithm::GDALVectorGridLinearAlgorithm()
    : GDALVectorGridAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    m_radius = std::numeric_limits<double>::infinity();
    AddRadiusArg().SetDefault(m_radius);
    AddNodataArg();
}

/************************************************************************/
/*               GDALVectorGridLinearAlgorithm::RunImpl()              */
/************************************************************************/

std::string GDALVectorGridLinearAlgorithm::GetGridAlgorithm() const
{
    return CPLSPrintf("linear:radius=%.17g:nodata=%.17g", m_radius, m_nodata);
}

//! @endcond
