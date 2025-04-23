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

#ifndef GDALALG_VECTOR_GRID_NEAREST_INCLUDED
#define GDALALG_VECTOR_GRID_NEAREST_INCLUDED

#include "gdalalg_vector_grid.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorGridNearestAlgorithm                  */
/************************************************************************/

class GDALVectorGridNearestAlgorithm final
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "nearest";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using nearest neighbor "
        "interpolation.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    GDALVectorGridNearestAlgorithm();

    std::string GetGridAlgorithm() const override;
};

//! @endcond

#endif
