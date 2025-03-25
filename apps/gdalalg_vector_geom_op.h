/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom-op" step of "vector pipeline", or "gdal vector geom-op" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_OP_INCLUDED
#define GDALALG_VECTOR_GEOM_OP_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorGeomOpAlgorithm                      */
/************************************************************************/

class GDALVectorGeomOpAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "geom-op";
    static constexpr const char *DESCRIPTION =
        "Geometry operations on a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_op.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALVectorGeomOpAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*                   GDALVectorGeomOpAlgorithmStandalone                */
/************************************************************************/

class GDALVectorGeomOpAlgorithmStandalone final
    : public GDALVectorGeomOpAlgorithm
{
  public:
    GDALVectorGeomOpAlgorithmStandalone()
        : GDALVectorGeomOpAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_OP_INCLUDED */
