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

#include <algorithm>
#include <cinttypes>
#include <limits>

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

class GDALVectorSimplifyCoverageOutputDataset final
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

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer,
                 std::function<bool(double)> *progressFunc) override
    {
        std::vector<OGRFeatureUniquePtr> features;
        std::vector<GEOSGeometry *> geoms;
        m_poGeosContext = OGRGeometry::createGEOSContext();

        const int64_t nFeatureCount =
            progressFunc != nullptr &&
                    srcLayer.TestCapability(OLCFastFeatureCount)
                ? srcLayer.GetFeatureCount()
                : 0;

        // Copy features from srcLayer into dstLayer, converting
        // their geometries to GEOS
        int64_t iCurFeature = 0;
        bool bRet = true;

        // Somewhat arbitrary progress ratios, but vast majority of time
        // is spent in GEOS
        constexpr double PCT_FIRST_PASS = 0.05;
        constexpr double PCT_GEOS = 0.95;
        constexpr double PCT_LAST_PASS = 1.0 - PCT_GEOS;
        constexpr int REPORT_EVERY_N_FEATURES = 100;

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
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Coverage simplification can only be performed on "
                         "polygonal geometries. Feature %" PRId64
                         " does not have one",
                         static_cast<int64_t>(feature->GetFID()));
                bRet = false;
                break;
            }

            GEOSGeometry *geosGeom =
                fgeom->exportToGEOS(m_poGeosContext, false);
            if (!geosGeom)
            {
                // should not happen normally
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry of feature %" PRId64
                         " failed to convert to GEOS",
                         static_cast<int64_t>(feature->GetFID()));
                bRet = false;
                break;
            }
            geoms.push_back(geosGeom);

            feature->SetGeometry(nullptr);  // free some memory
            feature->SetFDefnUnsafe(dstLayer.GetLayerDefn());

            features.push_back(std::move(feature));

            if (progressFunc && nFeatureCount > 0 &&
                ((iCurFeature) % REPORT_EVERY_N_FEATURES) == 0)
            {
                if (!(*progressFunc)(PCT_FIRST_PASS *
                                     static_cast<double>(iCurFeature) /
                                     static_cast<double>(
                                         std::max<int64_t>(1, nFeatureCount))))
                {
                    bRet = false;
                    break;
                }
            }
            ++iCurFeature;
        }

        if (bRet)
        {
            bRet = (!progressFunc || (*progressFunc)(PCT_FIRST_PASS));
        }
        if (!bRet)
        {
            for (auto &geom : geoms)
            {
                GEOSGeom_destroy_r(m_poGeosContext, geom);
            }
            return false;
        }

        // Perform coverage simplification
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

        bRet = progressFunc == nullptr || (*progressFunc)(PCT_GEOS);

        // Create features with the modified geometries
        for (size_t i = 0; bRet && i < features.size(); i++)
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

            bRet = dstLayer.CreateFeature(features[i].get()) == CE_None;
            features[i].reset();

            if (bRet && progressFunc && (i % REPORT_EVERY_N_FEATURES) == 0)
            {
                bRet = (*progressFunc)(
                    PCT_GEOS + PCT_LAST_PASS * static_cast<double>(i) /
                                   static_cast<double>(
                                       std::max<size_t>(1, features.size())));
            }
        }

        if (bRet && progressFunc)
            bRet = (*progressFunc)(1.0);

        return bRet;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSimplifyCoverageOutputDataset)

    const GDALVectorSimplifyCoverageAlgorithm::Options &m_opts;
    GEOSContextHandle_t m_poGeosContext{nullptr};
    GEOSGeometry **m_papoGeosResults{nullptr};
    unsigned int m_nGeosResultSize{0};
};

bool GDALVectorSimplifyCoverageAlgorithm::RunStep(
    GDALVectorPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS =
        std::make_unique<GDALVectorSimplifyCoverageOutputDataset>(m_opts);

    int nCountProcessedLayers = 0;
    bool bAllProcessedLayersFastHaveFeatureCount =
        ctxt.m_pfnProgress != nullptr;
    int64_t nCountAllFeatures = 0;
    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            ++nCountProcessedLayers;
            if (bAllProcessedLayersFastHaveFeatureCount)
            {
                bAllProcessedLayersFastHaveFeatureCount =
                    poSrcLayer->TestCapability(OLCFastFeatureCount);
                if (bAllProcessedLayersFastHaveFeatureCount)
                {
                    const int64_t nThisLayerFeatureCount =
                        poSrcLayer->GetFeatureCount();
                    if (nThisLayerFeatureCount >= 0 &&
                        nThisLayerFeatureCount <=
                            std::numeric_limits<int64_t>::max() -
                                nCountAllFeatures)
                        nCountAllFeatures += nThisLayerFeatureCount;
                    else
                        bAllProcessedLayersFastHaveFeatureCount = false;
                }
            }
        }
    }

    if (!m_activeLayer.empty() && nCountProcessedLayers == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Specified layer '%s' was not found",
                    m_activeLayer.c_str());
        return false;
    }

    int iCurProcessedLayer = 0;
    int64_t nCurFeatureCount = 0;
    const double dfProgressRatio =
        bAllProcessedLayersFastHaveFeatureCount
            ? 1.0 / static_cast<double>(std::max<int64_t>(1, nCountAllFeatures))
            : 1.0 / std::max(1, nCountProcessedLayers);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledProgressData(nullptr, GDALDestroyScaledProgress);
            if (ctxt.m_pfnProgress)
            {
                int64_t nThisLayerFeatureCount = 0;
                if (bAllProcessedLayersFastHaveFeatureCount)
                    nThisLayerFeatureCount = poSrcLayer->GetFeatureCount();
                if (bAllProcessedLayersFastHaveFeatureCount)
                {
                    pScaledProgressData.reset(GDALCreateScaledProgress(
                        static_cast<double>(nCurFeatureCount) * dfProgressRatio,
                        static_cast<double>(nCurFeatureCount +
                                            nThisLayerFeatureCount) *
                            dfProgressRatio,
                        ctxt.m_pfnProgress, ctxt.m_pProgressData));
                }
                else
                {
                    pScaledProgressData.reset(GDALCreateScaledProgress(
                        static_cast<double>(iCurProcessedLayer) *
                            dfProgressRatio,
                        static_cast<double>(iCurProcessedLayer + 1) *
                            dfProgressRatio,
                        ctxt.m_pfnProgress, ctxt.m_pProgressData));
                }
                ++iCurProcessedLayer;
                nCurFeatureCount += nThisLayerFeatureCount;
            }

            std::function<bool(double)> ProgressFunc = [&pScaledProgressData](
                                                           double dfPct) {
                return GDALScaledProgress(dfPct, "", pScaledProgressData.get());
            };

            if (!poDstDS->AddProcessedLayer(
                    *poSrcLayer, pScaledProgressData ? &ProgressFunc : nullptr))
            {
                return false;
            }
        }
        else
        {
            poDstDS->AddPassThroughLayer(*poSrcLayer);
        }
    }

    if (ctxt.m_pfnProgress)
        ctxt.m_pfnProgress(1.0, "", ctxt.m_pProgressData);

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
