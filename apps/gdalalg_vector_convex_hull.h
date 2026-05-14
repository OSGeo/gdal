/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector convex-hull"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CONVEX_HULL_INCLUDED
#define GDALALG_VECTOR_CONVEX_HULL_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

class GDALVectorConvexHullAlgorithm /* non final */
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "convex-hull";
    static constexpr const char *DESCRIPTION =
        "Compute the convex hull of geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_convex_hull.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorConvexHullAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

class GDALVectorConvexHullAlgorithmStandalone final
    : public GDALVectorConvexHullAlgorithm
{
  public:
    GDALVectorConvexHullAlgorithmStandalone()
        : GDALVectorConvexHullAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorConvexHullAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CONVEX_HULL_INCLUDED */
