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

#ifndef GDALALG_VECTOR_GRID_INVDISTNN_INCLUDED
#define GDALALG_VECTOR_GRID_INVDISTNN_INCLUDED

#include "gdalalg_vector_grid.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGridInvdistNNAlgorithm                  */
/************************************************************************/

class GDALVectorGridInvdistNNAlgorithm final
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "invdistnn";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using weighted inverse "
        "distance interpolation nearest neighbour.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    GDALVectorGridInvdistNNAlgorithm();

    std::string GetGridAlgorithm() const override;

  private:
    double m_power = 2.0;
    double m_smoothing = 0.0;
};

//! @endcond

#endif
