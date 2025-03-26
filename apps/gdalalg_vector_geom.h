/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom" step of "vector pipeline", or "gdal vector geom" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_INCLUDED
#define GDALALG_VECTOR_GEOM_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorGeomAlgorithm                        */
/************************************************************************/

class GDALVectorGeomAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "geom";
    static constexpr const char *DESCRIPTION =
        "Geometry operations on a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_geom.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALVectorGeomAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*                    GDALVectorGeomAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorGeomAlgorithmStandalone final : public GDALVectorGeomAlgorithm
{
  public:
    GDALVectorGeomAlgorithmStandalone()
        : GDALVectorGeomAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_INCLUDED */
