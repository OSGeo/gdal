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

#ifndef GDALALG_VECTOR_GRID_INVDIST_INCLUDED
#define GDALALG_VECTOR_GRID_INVDIST_INCLUDED

#include "gdalalg_vector_grid.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGridInvdistAlgorithm                    */
/************************************************************************/

class GDALVectorGridInvdistAlgorithm /* non final */
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "invdist";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using weighted inverse "
        "distance interpolation.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    explicit GDALVectorGridInvdistAlgorithm(bool standaloneStep = false);

    std::string GetGridAlgorithm() const override;

  private:
    double m_power = 2.0;
    double m_smoothing = 0.0;
};

/************************************************************************/
/*               GDALVectorGridInvdistAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridInvdistAlgorithmStandalone final
    : public GDALVectorGridInvdistAlgorithm
{
  public:
    GDALVectorGridInvdistAlgorithmStandalone()
        : GDALVectorGridInvdistAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridInvdistAlgorithmStandalone() override;
};

//! @endcond

#endif
