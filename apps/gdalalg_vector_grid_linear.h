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

#ifndef GDALALG_VECTOR_GRID_LINEAR_INCLUDED
#define GDALALG_VECTOR_GRID_LINEAR_INCLUDED

#include "gdalalg_vector_grid.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorGridLinearAlgorithm                   */
/************************************************************************/

class GDALVectorGridLinearAlgorithm /* non final */
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "linear";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using linear/barycentric "
        "interpolation.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    explicit GDALVectorGridLinearAlgorithm(bool standaloneStep = false);

    std::string GetGridAlgorithm() const override;
};

/************************************************************************/
/*                GDALVectorGridLinearAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridLinearAlgorithmStandalone final
    : public GDALVectorGridLinearAlgorithm
{
  public:
    GDALVectorGridLinearAlgorithmStandalone()
        : GDALVectorGridLinearAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridLinearAlgorithmStandalone() override;
};

//! @endcond

#endif
