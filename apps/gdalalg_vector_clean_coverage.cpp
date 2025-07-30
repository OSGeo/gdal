/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector clean-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_clean_coverage.h"

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

GDALVectorCleanCoverageAlgorithm::GDALVectorCleanCoverageAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddArg("snapping-distance", 0, _("Distance tolerance for snapping nodes"),
           &m_opts.snappingTolerance)
        .SetMinValueIncluded(0);

    AddArg("merge-strategy", 0,
           _("Algorithm to assign overlaps to neighboring polygons"),
           &m_opts.mergeStrategy)
        .SetChoices({"longest-border", "max-area", "min-area", "min-index"});

    AddArg("maximum-gap-width", 0, _("Maximum width of a gap to be closed"),
           &m_opts.maximumGapWidth)
        .SetMinValueIncluded(0);
}

#if defined HAVE_GEOS &&                                                       \
    (GEOS_VERSION_MAJOR > 3 ||                                                 \
     (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 14))

class GDALVectorCleanCoverageOutputDataset
    : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    GDALVectorCleanCoverageOutputDataset(
        const GDALVectorCleanCoverageAlgorithm::Options &opts)
        : m_opts(opts)
    {
        m_poGeosContext = OGRGeometry::createGEOSContext();
    }

    ~GDALVectorCleanCoverageOutputDataset() override;

    GEOSCoverageCleanParams *GetCoverageCleanParams() const
    {
        GEOSCoverageCleanParams *params =
            GEOSCoverageCleanParams_create_r(m_poGeosContext);

        if (!params)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create coverage clean parameters");
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setSnappingDistance_r(
                m_poGeosContext, params, m_opts.snappingTolerance))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set snapping tolerance");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setGapMaximumWidth_r(
                m_poGeosContext, params, m_opts.maximumGapWidth))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set maximum gap width");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        int mergeStrategy;
        if (m_opts.mergeStrategy == "longest-border")
        {
            mergeStrategy = GEOS_MERGE_LONGEST_BORDER;
        }
        else if (m_opts.mergeStrategy == "max-area")
        {
            mergeStrategy = GEOS_MERGE_MAX_AREA;
        }
        else if (m_opts.mergeStrategy == "min-area")
        {
            mergeStrategy = GEOS_MERGE_MIN_AREA;
        }
        else if (m_opts.mergeStrategy == "min-index")
        {
            mergeStrategy = GEOS_MERGE_MIN_INDEX;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown overlap merge strategy: %s",
                     m_opts.mergeStrategy.c_str());
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setOverlapMergeStrategy_r(
                m_poGeosContext, params, mergeStrategy))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set overlap merge strategy");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        return params;
    }

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer) override
    {
        std::vector<OGRFeatureUniquePtr> features;
        std::vector<GEOSGeometry *> geoms;

        GEOSCoverageCleanParams *params = GetCoverageCleanParams();

        // Copy features from srcLayer into dstLayer, converting
        // their geometries to GEOS
        for (auto &feature : srcLayer)
        {
            const OGRGeometry *fgeom = feature->GetGeometryRef();

            const auto eFGType =
                fgeom ? wkbFlatten(fgeom->getGeometryType()) : wkbUnknown;
            if (eFGType != wkbPolygon && eFGType != wkbMultiPolygon &&
                eFGType != wkbCurvePolygon && eFGType != wkbMultiSurface)
            {
                for (auto &geom : geoms)
                {
                    GEOSGeom_destroy_r(m_poGeosContext, geom);
                }
                GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Coverage cleaning can only be performed on "
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
                GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
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

        // Perform coverage cleaning
        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_poGeosContext, GEOS_GEOMETRYCOLLECTION, geoms.data(),
            static_cast<unsigned int>(geoms.size()));

        if (coll == nullptr)
        {
            for (auto &geom : geoms)
            {
                GEOSGeom_destroy_r(m_poGeosContext, geom);
            }
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return false;
        }

        GEOSGeometry *geos_result =
            GEOSCoverageCleanWithParams_r(m_poGeosContext, coll, params);
        GEOSGeom_destroy_r(m_poGeosContext, coll);
        GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);

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
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorCleanCoverageOutputDataset)

    const GDALVectorCleanCoverageAlgorithm::Options &m_opts;
    GEOSContextHandle_t m_poGeosContext{nullptr};
    GEOSGeometry **m_papoGeosResults{nullptr};
    unsigned int m_nGeosResultSize{0};
};

GDALVectorCleanCoverageOutputDataset::~GDALVectorCleanCoverageOutputDataset()
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

bool GDALVectorCleanCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS =
        std::make_unique<GDALVectorCleanCoverageOutputDataset>(m_opts);

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

bool GDALVectorCleanCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.14 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

GDALVectorCleanCoverageAlgorithmStandalone::
    ~GDALVectorCleanCoverageAlgorithmStandalone() = default;

//! @endcond
