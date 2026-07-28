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

#include "gdalalg_vector_convex_hull.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

GDALVectorConvexHullAlgorithm::GDALVectorConvexHullAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
}

#ifdef HAVE_GEOS

namespace
{

class GDALVectorConvexHullAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorConvexHullAlgorithm>
{
  public:
    GDALVectorConvexHullAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorConvexHullAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorConvexHullAlgorithm>(
              oSrcLayer, opts),
          m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
    {
        // Convex hull output type can be Point/LineString/Polygon depending on input.
        // To avoid schema/type conflicts, advertise unknown geometry type for
        // processed geometry fields.
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            if (IsSelectedGeomField(i))
                m_poFeatureDefn->GetGeomFieldDefn(i)->SetType(wkbUnknown);
        }
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn.get();
    }

  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override
    {
        const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
        for (int i = 0; i < nGeomFieldCount; ++i)
        {
            if (!IsSelectedGeomField(i))
                continue;

            if (const OGRGeometry *poGeom = poSrcFeature->GetGeomFieldRef(i))
            {
                std::unique_ptr<OGRGeometry> poHull(poGeom->ConvexHull());
                if (!poHull)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Failed to compute convex hull of feature %" PRId64,
                        static_cast<int64_t>(poSrcFeature->GetFID()));
                    return nullptr;
                }

                poHull->assignSpatialReference(poGeom->getSpatialReference());
                poSrcFeature->SetGeomField(i, std::move(poHull));
            }
        }

        poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn.get());
        return poSrcFeature;
    }

  private:
    const OGRFeatureDefnRefCountedPtr m_poFeatureDefn;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorConvexHullAlgorithmLayer)
};

}  // namespace

#endif  // HAVE_GEOS

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorConvexHullAlgorithm::CreateAlgLayer(
    [[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorConvexHullAlgorithmLayer>(srcLayer,
                                                                m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

bool GDALVectorConvexHullAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
#ifdef HAVE_GEOS
    return GDALVectorGeomAbstractAlgorithm::RunStep(ctxt);
#else
    (void)ctxt;
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

GDALVectorConvexHullAlgorithmStandalone::
    ~GDALVectorConvexHullAlgorithmStandalone() = default;

//! @endcond
