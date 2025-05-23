/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector simplify-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_simplify_coverage.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"
#include "ogrsf_frmts.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorSimplifyCoverageAlgorithm::GDALVectorSimplifyCoverageAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddArg("tolerance", 0, _("Distance tolerance for simplification."),
           &m_opts.tolerance)
        .SetRequired()
        .SetMinValueIncluded(0);
    AddArg("preserve-boundary", 0,
           _("Whether the exterior boundary should be preserved."),
           &m_opts.preserveBoundary);
}

#if defined HAVE_GEOS &&                                                       \
    (GEOS_VERSION_MAJOR > 3 ||                                                 \
     (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12))

class GDALVectorSimplifyCoverageOutputDataset
    : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    GDALVectorSimplifyCoverageOutputDataset(
        const GDALVectorSimplifyCoverageAlgorithm::Options &opts)
        : m_opts(opts)
    {
    }

    ~GDALVectorSimplifyCoverageOutputDataset()
    {
        if (m_poGeosContext != nullptr)
        {
            for (size_t i = 0; i < m_nGeosResultSize; i++)
            {
                GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
            }
            GEOSFree_r(m_poGeosContext, m_papoGeosResults);
            finishGEOS_r(m_poGeosContext);
        }
    }

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer) override
    {
        std::vector<OGRFeatureUniquePtr> features;
        std::vector<GEOSGeometry *> geoms;
        m_poGeosContext = OGRGeometry::createGEOSContext();

        // Copy features from srcLayer into dstLayer, converting
        // their geometries to GEOS
        for (auto &feature : srcLayer)
        {
            const OGRGeometry *fgeom = feature->GetGeometryRef();

            // Avoid segfault with non-polygonal inputs on GEOS < 3.12.2
            // Later versions produce an error instead
            const auto eFGType =
                fgeom ? wkbFlatten(fgeom->getGeometryType()) : wkbUnknown;
            if (eFGType != wkbPolygon && eFGType != wkbMultiPolygon &&
                eFGType != wkbCurvePolygon && eFGType != wkbMultiSurface)
            {
                for (auto &geom : geoms)
                {
                    GEOSGeom_destroy_r(m_poGeosContext, geom);
                }
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Coverage simplification can only be performed on "
                         "polygonal geometries. Feature %" PRId64
                         " does not have one",
                         static_cast<int64_t>(feature->GetFID()));
                return false;
            }

            GEOSGeometry *geosGeom =
                fgeom->exportToGEOS(m_poGeosContext, false);
            if (!geosGeom)
            {
                // should not happen normally
                for (auto &geom : geoms)
                {
                    GEOSGeom_destroy_r(m_poGeosContext, geom);
                }
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry of feature %" PRId64
                         " failed to convert to GEOS",
                         static_cast<int64_t>(feature->GetFID()));
                return false;
            }
            geoms.push_back(geosGeom);

            feature->SetGeometry(nullptr);  // free some memory
            feature->SetFDefnUnsafe(dstLayer.GetLayerDefn());

            features.push_back(std::move(feature));
        }

        // Perform coverage simplifciation
        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_poGeosContext, GEOS_GEOMETRYCOLLECTION, geoms.data(),
            static_cast<unsigned int>(geoms.size()));

        if (coll == nullptr)
        {
            for (auto &geom : geoms)
            {
                GEOSGeom_destroy_r(m_poGeosContext, geom);
            }
            return false;
        }

        GEOSGeometry *geos_result = GEOSCoverageSimplifyVW_r(
            m_poGeosContext, coll, m_opts.tolerance, m_opts.preserveBoundary);
        GEOSGeom_destroy_r(m_poGeosContext, coll);

        if (geos_result == nullptr)
        {
            return false;
        }

        m_papoGeosResults = GEOSGeom_releaseCollection_r(
            m_poGeosContext, geos_result, &m_nGeosResultSize);
        GEOSGeom_destroy_r(m_poGeosContext, geos_result);
        CPLAssert(features.size() == m_nGeosResultSize);

        // Create features with the modified geometries
        for (size_t i = 0; i < features.size(); i++)
        {
            GEOSGeometry *dstGeom = m_papoGeosResults[i];

            std::unique_ptr<OGRGeometry> poSimplified(
                OGRGeometryFactory::createFromGEOS(m_poGeosContext, dstGeom));
            GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
            m_papoGeosResults[i] = nullptr;

            if (poSimplified == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to convert result from GEOS");
                return false;
            }
            poSimplified->assignSpatialReference(
                dstLayer.GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef());
            features[i]->SetGeometry(std::move(poSimplified));

            if (dstLayer.CreateFeature(features[i].get()) != CE_None)
            {
                return false;
            }
            features[i].reset();
        }

        return true;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSimplifyCoverageOutputDataset)

    const GDALVectorSimplifyCoverageAlgorithm::Options &m_opts;
    GEOSContextHandle_t m_poGeosContext{nullptr};
    GEOSGeometry **m_papoGeosResults{nullptr};
    unsigned int m_nGeosResultSize{0};
};

bool GDALVectorSimplifyCoverageAlgorithm::RunStep(
    GDALVectorPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS =
        std::make_unique<GDALVectorSimplifyCoverageOutputDataset>(m_opts);

    bool bFoundActiveLayer = false;

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            if (!poDstDS->AddProcessedLayer(*poSrcLayer))
            {
                return false;
            }
            bFoundActiveLayer = true;
        }
        else
        {
            poDstDS->AddPassThroughLayer(*poSrcLayer);
        }
    }

    if (!bFoundActiveLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Specified layer '%s' was not found",
                    m_activeLayer.c_str());
        return false;
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

#else

bool GDALVectorSimplifyCoverageAlgorithm::RunStep(
    GDALVectorPipelineStepRunContext &)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.12 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

//! @endcond
