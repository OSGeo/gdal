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

#ifndef GDALALG_VECTOR_GRID_AVERAGE_INCLUDED
#define GDALALG_VECTOR_GRID_AVERAGE_INCLUDED

#include "gdalalg_vector_grid.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGridAverageAlgorithm                    */
/************************************************************************/

class GDALVectorGridAverageAlgorithm /* non final */
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "average";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using moving average "
        "interpolation.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    explicit GDALVectorGridAverageAlgorithm(bool standaloneStep = false);

    std::string GetGridAlgorithm() const override;
};

/************************************************************************/
/*               GDALVectorGridAverageAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridAverageAlgorithmStandalone final
    : public GDALVectorGridAverageAlgorithm
{
  public:
    GDALVectorGridAverageAlgorithmStandalone()
        : GDALVectorGridAverageAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridAverageAlgorithmStandalone() override;
};

//! @endcond

#endif
