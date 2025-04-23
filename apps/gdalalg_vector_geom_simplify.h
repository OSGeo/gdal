/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom simplify"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_SIMPLIFY_INCLUDED
#define GDALALG_VECTOR_GEOM_SIMPLIFY_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALVectorGeomSimplifyAlgorithm                     */
/************************************************************************/

class GDALVectorGeomSimplifyAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "simplify";
    static constexpr const char *DESCRIPTION =
        "Simplify geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_simplify.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        double m_tolerance = 0;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomSimplifyAlgorithm(bool standaloneStep);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SIMPLIFY_INCLUDED */
