/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector concave-hull"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CONCAVE_HULL_INCLUDED
#define GDALALG_VECTOR_CONCAVE_HULL_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

class GDALVectorConcaveHullAlgorithm /* non final */
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "concave-hull";
    static constexpr const char *DESCRIPTION =
        "Compute the concave hull of geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_concave_hull.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        double m_ratio = 0.0;
        bool m_allowHoles = false;
        bool m_tight = false;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorConcaveHullAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

class GDALVectorConcaveHullAlgorithmStandalone final
    : public GDALVectorConcaveHullAlgorithm
{
  public:
    GDALVectorConcaveHullAlgorithmStandalone()
        : GDALVectorConcaveHullAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorConcaveHullAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_CONCAVE_HULL_INCLUDED */
