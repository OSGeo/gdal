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
/*                    GDALVectorGridNearestAlgorithm                    */
/************************************************************************/

class GDALVectorGridNearestAlgorithm /* non final */
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "nearest";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using nearest neighbor "
        "interpolation.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    explicit GDALVectorGridNearestAlgorithm(bool standaloneStep = false);

    std::string GetGridAlgorithm() const override;
};

/************************************************************************/
/*               GDALVectorGridNearestAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridNearestAlgorithmStandalone final
    : public GDALVectorGridNearestAlgorithm
{
  public:
    GDALVectorGridNearestAlgorithmStandalone()
        : GDALVectorGridNearestAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridNearestAlgorithmStandalone() override;
};

//! @endcond

#endif
