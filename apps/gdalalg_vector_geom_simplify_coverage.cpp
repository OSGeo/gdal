/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector geom simplify-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_simplify_coverage.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"
#include "ogrsf_frmts.h"

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorGeomSimplifyCoverageAlgorithm::
    GDALVectorGeomSimplifyCoverageAlgorithm(bool standaloneStep)
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

/**
 * Dataset used to read all input features into memory and simplify their
 * geometries.
 */
class GDALVectorGeomSimplifyCoverageOutputDataset final : public GDALDataset
{
  public:
    GDALVectorGeomSimplifyCoverageOutputDataset();

    void AddLayer(std::unique_ptr<OGRLayer> poNewLayer)
    {
        m_layers.push_back(std::move(poNewLayer));
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return m_layers[idx].get();
    }

  private:
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};
};

class GDALVectorGeomSimplifyCoverageAlgorithmLayer final : public OGRLayer
{
  public:
    explicit GDALVectorGeomSimplifyCoverageAlgorithmLayer(
        OGRLayer &src,
        const GDALVectorGeomSimplifyCoverageAlgorithm::Options &opts)
        : m_srcLayer(src), m_opts(opts)
    {
    }

    ~GDALVectorGeomSimplifyCoverageAlgorithmLayer() override
    {
        if (m_GEOSContext != nullptr)
        {
            for (size_t i = 0; i < m_features.size(); i++)
            {
                GEOSGeom_destroy_r(m_GEOSContext, m_geos_result[i]);
            }
            GEOSFree_r(m_GEOSContext, m_geos_result);
            finishGEOS_r(m_GEOSContext);
        }
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCRandomRead))
        {
            return true;
        }
        return m_srcLayer.TestCapability(pszCap);
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_srcLayer.GetLayerDefn();
    }

    void ResetReading() override
    {
        m_srcLayer.ResetReading();
        m_pos = 0;
        m_features.clear();
    }

    OGRFeature *GetNextFeature() override
    {
        if (!m_materialized)
        {
            if (!Materialize())
            {
                return nullptr;
            }
        }

        if (m_pos >= m_features.size())
        {
            return nullptr;
        }

        GEOSGeometry *dstGeom = m_geos_result[m_pos];
        if (m_outputMultiPart &&
            GEOSGetNumGeometries_r(m_GEOSContext, dstGeom) == 1)
        {
            dstGeom = GEOSGeom_createCollection_r(
                m_GEOSContext, GEOS_MULTIPOLYGON, &dstGeom, 1);
        }

        std::unique_ptr<OGRGeometry> poSimplified(
            OGRGeometryFactory::createFromGEOS(m_GEOSContext, dstGeom));
        GEOSGeom_destroy_r(m_GEOSContext, m_geos_result[m_pos]);
        m_geos_result[m_pos] = nullptr;

        if (poSimplified == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to convert result from GEOS");
            return nullptr;
        }

        m_features[m_pos]->SetGeometry(std::move(poSimplified));

        auto &ret = m_features[m_pos];
        m_pos++;

        return ret.release();
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorGeomSimplifyCoverageAlgorithmLayer)

    bool Materialize()
    {
        std::vector<GEOSGeometry *> geoms;

        m_GEOSContext = OGRGeometry::createGEOSContext();

        for (auto &feature : m_srcLayer)
        {
            const OGRGeometry *fgeom = feature->GetGeometryRef();

            // Avoid segfault with non-polygonal inputs on GEOS < 3.12.2
            // Later versions produce an error instead
            if (fgeom == nullptr || fgeom->getDimension() != 2)
            {
                for (auto &geom : geoms)
                {
                    GEOSGeom_destroy_r(m_GEOSContext, geom);
                }
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Coverage simplification can only be performed on "
                         "polygonal geometries.");
                return false;
            }

            geoms.push_back(fgeom->exportToGEOS(m_GEOSContext, false));

            if (!m_outputMultiPart &&
                GEOSGetNumGeometries_r(m_GEOSContext, geoms.back()) > 1)
            {
                m_outputMultiPart = true;
            }

            feature->SetGeometry(nullptr);  // free some memory
            m_features.emplace_back(std::move(feature));
        }

        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_GEOSContext, GEOS_GEOMETRYCOLLECTION, geoms.data(),
            static_cast<unsigned int>(geoms.size()));

        if (coll == nullptr)
        {
            for (auto &geom : geoms)
            {
                GEOSGeom_destroy_r(m_GEOSContext, geom);
            }
            return false;
        }

        GEOSGeometry *geos_result = GEOSCoverageSimplifyVW_r(
            m_GEOSContext, coll, m_opts.tolerance, m_opts.preserveBoundary);
        GEOSGeom_destroy_r(m_GEOSContext, coll);

        if (geos_result == nullptr)
        {
            return false;
        }

        unsigned int nResultGeoms;
        m_geos_result = GEOSGeom_releaseCollection_r(m_GEOSContext, geos_result,
                                                     &nResultGeoms);

        CPLAssert(m_features.size() == nResultGeoms);

        m_materialized = true;

        return true;
    }

    OGRLayer &m_srcLayer;
    std::size_t m_pos{0};
    bool m_materialized{false};
    bool m_outputMultiPart{false};

    const GDALVectorGeomSimplifyCoverageAlgorithm::Options &m_opts;
    std::vector<std::unique_ptr<OGRFeature, OGRFeatureUniquePtrDeleter>>
        m_features{};
    GEOSContextHandle_t m_GEOSContext{nullptr};
    GEOSGeometry **m_geos_result{nullptr};
};

GDALVectorGeomSimplifyCoverageOutputDataset::
    GDALVectorGeomSimplifyCoverageOutputDataset()
{
}

bool GDALVectorGeomSimplifyCoverageAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS =
        std::make_unique<GDALVectorGeomSimplifyCoverageOutputDataset>();

    bool bFoundActiveLayer = false;

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            auto poLayer =
                std::make_unique<GDALVectorGeomSimplifyCoverageAlgorithmLayer>(
                    *poSrcLayer, m_opts);
            poDstDS->AddLayer(std::move(poLayer));
            bFoundActiveLayer = true;
        }
        else
        {
            poDstDS->AddLayer(
                std::make_unique<GDALVectorPipelinePassthroughLayer>(
                    *poSrcLayer));
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

bool GDALVectorGeomSimplifyCoverageAlgorithm::RunStep(GDALProgressFunc, void *)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.12 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

//! @endcond
