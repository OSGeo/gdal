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

#include "gdalalg_vector_concave_hull.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

GDALVectorConcaveHullAlgorithm::GDALVectorConcaveHullAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("ratio", 0, _("Ratio controlling the concavity"), &m_opts.m_ratio)
        .SetRequired()
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(1);

    AddArg("allow-holes", 0, _("Allow holes in the output polygon"),
           &m_opts.m_allowHoles)
        .SetDefault(false);

    AddArg("tight", 0,
           _("Whether the hull must follow the outer boundaries of the input "
             "polygons"),
           &m_opts.m_tight)
        .SetDefault(false);
}

#ifdef HAVE_GEOS

namespace
{

class GDALVectorConcaveHullAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<
          GDALVectorConcaveHullAlgorithm>
{
  public:
    GDALVectorConcaveHullAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorConcaveHullAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorConcaveHullAlgorithm>(
              oSrcLayer, opts),
          m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
    {
        // Concave hull output type can vary; advertise unknown to avoid schema conflicts.
        // In polygon/multipolygoon mode, preserve input type
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            if (IsSelectedGeomField(i))
            {
                const auto eType =
                    m_poFeatureDefn->GetGeomFieldDefn(i)->GetType();
                if (eType != wkbPolygon && eType != wkbMultiPolygon)
                    m_poFeatureDefn->GetGeomFieldDefn(i)->SetType(wkbUnknown);
            }
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
                const auto eType = wkbFlatten(poGeom->getGeometryType());
                std::unique_ptr<OGRGeometry> poHull(
                    (eType == wkbPolygon || eType == wkbMultiPolygon)
                        ? poGeom->ConcaveHullOfPolygons(m_opts.m_ratio,
                                                        m_opts.m_tight,
                                                        m_opts.m_allowHoles)
                        : poGeom->ConcaveHull(m_opts.m_ratio,
                                              m_opts.m_allowHoles));
                if (!poHull)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Failed to compute concave hull of feature %" PRId64,
                        static_cast<int64_t>(poSrcFeature->GetFID()));
                    return nullptr;
                }
                const auto eLayerGeomType =
                    m_poFeatureDefn->GetGeomFieldDefn(i)->GetType();
                if (eLayerGeomType == wkbPolygon)
                {
                    poHull.reset(
                        OGRGeometryFactory::forceToPolygon(poHull.release()));
                }
                else if (eLayerGeomType == wkbMultiPolygon)
                {
                    poHull.reset(OGRGeometryFactory::forceToMultiPolygon(
                        poHull.release()));
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

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorConcaveHullAlgorithmLayer)
};

}  // namespace

#endif  // HAVE_GEOS

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorConcaveHullAlgorithm::CreateAlgLayer(
    [[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorConcaveHullAlgorithmLayer>(srcLayer,
                                                                 m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

bool GDALVectorConcaveHullAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
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

GDALVectorConcaveHullAlgorithmStandalone::
    ~GDALVectorConcaveHullAlgorithmStandalone() = default;

//! @endcond
