/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Base classes for some geometry-related vector algorithms
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom.h"
#include "cpl_enumerate.h"

#include <algorithm>
#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALVectorGeomAbstractAlgorithm()                    */
/************************************************************************/

GDALVectorGeomAbstractAlgorithm::GDALVectorGeomAbstractAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool standaloneStep, OptionsBase &opts)
    : GDALVectorPipelineStepAlgorithm(name, description, helpURL,
                                      standaloneStep),
      m_activeLayer(opts.m_activeLayer)
{
    AddActiveLayerArg(&opts.m_activeLayer);
    AddArg("active-geometry", 0,
           _("Geometry field name to which to restrict the processing (if not "
             "specified, all)"),
           &opts.m_geomField);
}

/************************************************************************/
/*               GDALVectorGeomAbstractAlgorithm::RunStep()             */
/************************************************************************/

bool GDALVectorGeomAbstractAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            outDS->AddLayer(*poSrcLayer, CreateAlgLayer(*poSrcLayer));
        }
        else
        {
            outDS->AddLayer(
                *poSrcLayer,
                std::make_unique<GDALVectorPipelinePassthroughLayer>(
                    *poSrcLayer));
        }
    }

    m_outputDataset.Set(std::move(outDS));

    return true;
}

#ifdef HAVE_GEOS

/************************************************************************/
/*                    GDALGeosNonStreamingAlgorithmDataset              */
/************************************************************************/

GDALGeosNonStreamingAlgorithmDataset::GDALGeosNonStreamingAlgorithmDataset()
    : m_poGeosContext{OGRGeometry::createGEOSContext()}
{
}

GDALGeosNonStreamingAlgorithmDataset::~GDALGeosNonStreamingAlgorithmDataset()
{
    Cleanup();
    if (m_poGeosContext != nullptr)
    {
        finishGEOS_r(m_poGeosContext);
    }
}

void GDALGeosNonStreamingAlgorithmDataset::Cleanup()
{
    m_apoFeatures.clear();

    if (m_poGeosContext != nullptr)
    {
        for (auto &poGeom : m_apoGeosInputs)
        {
            GEOSGeom_destroy_r(m_poGeosContext, poGeom);
        }
        m_apoGeosInputs.clear();

        if (m_poGeosContext != nullptr)
        {
            GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);
            m_poGeosResultAsCollection = nullptr;
        }

        for (size_t i = 0; i < m_nGeosResultSize; i++)
        {
            GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
        }
        m_nGeosResultSize = 0;

        if (m_papoGeosResults != nullptr)
        {
            GEOSFree_r(m_poGeosContext, m_papoGeosResults);
            m_papoGeosResults = nullptr;
        }
    }
}

bool GDALGeosNonStreamingAlgorithmDataset::ConvertInputsToGeos(
    OGRLayer &srcLayer, OGRLayer &dstLayer, int geomFieldIndex, bool sameDefn,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    const GIntBig nLayerFeatures = srcLayer.TestCapability(OLCFastFeatureCount)
                                       ? srcLayer.GetFeatureCount(false)
                                       : -1;
    const double dfInvLayerFeatures =
        1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
    const double dfProgressRatio = dfInvLayerFeatures * 0.5;

    for (auto &feature : srcLayer)
    {
        const OGRGeometry *poSrcGeom = feature->GetGeomFieldRef(geomFieldIndex);

        if (PolygonsOnly())
        {
            const auto eFGType = poSrcGeom
                                     ? wkbFlatten(poSrcGeom->getGeometryType())
                                     : wkbUnknown;
            if (eFGType != wkbPolygon && eFGType != wkbMultiPolygon &&
                eFGType != wkbCurvePolygon && eFGType != wkbMultiSurface)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Coverage checking can only be performed on "
                         "polygonal geometries. Feature %" PRId64
                         " does not have one",
                         static_cast<int64_t>(feature->GetFID()));
                return false;
            }
        }

        if (poSrcGeom)
        {
            GEOSGeometry *geosGeom =
                poSrcGeom->exportToGEOS(m_poGeosContext, false);
            if (!geosGeom)
            {
                // should not happen normally
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry of feature %" PRId64
                         " failed to convert to GEOS",
                         static_cast<int64_t>(feature->GetFID()));
                return false;
            }

            m_apoGeosInputs.push_back(geosGeom);
        }
        else
        {
            m_apoGeosInputs.push_back(GEOSGeom_createEmptyCollection_r(
                m_poGeosContext, GEOS_GEOMETRYCOLLECTION));
        }

        feature->SetGeometry(nullptr);  // free some memory

        if (sameDefn)
        {
            feature->SetFDefnUnsafe(dstLayer.GetLayerDefn());
            m_apoFeatures.push_back(
                std::unique_ptr<OGRFeature>(feature.release()));
        }
        else
        {
            auto newFeature =
                std::make_unique<OGRFeature>(dstLayer.GetLayerDefn());
            newFeature->SetFrom(feature.get(), true);
            newFeature->SetFID(feature->GetFID());
            m_apoFeatures.push_back(std::move(newFeature));
        }

        if (pfnProgress && nLayerFeatures > 0 &&
            !pfnProgress(static_cast<double>(m_apoFeatures.size()) *
                             dfProgressRatio,
                         "", pProgressData))
        {
            ReportError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            return false;
        }
    }

    return true;
}

bool GDALGeosNonStreamingAlgorithmDataset::ConvertOutputsFromGeos(
    OGRLayer &dstLayer, GDALProgressFunc pfnProgress, void *pProgressData,
    double dfProgressStart, double dfProgressRatio)
{
    const OGRSpatialReference *poResultSRS =
        dstLayer.GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef();

// GEOSGeom_releaseCollection allows us to take ownership of the contents of
// a GeometryCollection. We can then incrementally free the geometries as
// we write them to features. It requires GEOS >= 3.12.
#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12)
#define GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
#endif

    const auto eLayerGeomType = dstLayer.GetLayerDefn()->GetGeomType();

#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
    m_nGeosResultSize =
        GEOSGetNumGeometries_r(m_poGeosContext, m_poGeosResultAsCollection);
    m_papoGeosResults = GEOSGeom_releaseCollection_r(
        m_poGeosContext, m_poGeosResultAsCollection, &m_nGeosResultSize);
    GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);
    m_poGeosResultAsCollection = nullptr;
    CPLAssert(m_apoFeatures.size() == m_nGeosResultSize);

    // Create features with the modified geometries
    for (const auto &[i, poFeature] : cpl::enumerate(m_apoFeatures))
    {
        GEOSGeometry *poGeosResult = m_papoGeosResults[i];
#else
    auto nGeoms =
        GEOSGetNumGeometries_r(m_poGeosContext, m_poGeosResultAsCollection);
    for (decltype(nGeoms) i = 0; i < nGeoms; i++)
    {
        auto &poFeature = m_apoFeatures[i];
        GEOSGeometry *poGeosResult = const_cast<GEOSGeometry *>(
            GEOSGetGeometryN_r(m_poGeosContext, m_poGeosResultAsCollection, i));
#endif
        std::unique_ptr<OGRGeometry> poResultGeom;

        bool skipFeature =
            SkipEmpty() && GEOSisEmpty_r(m_poGeosContext, poGeosResult);

        if (!skipFeature)
        {
            poResultGeom.reset(OGRGeometryFactory::createFromGEOS(
                m_poGeosContext, poGeosResult));

            if (poResultGeom && eLayerGeomType != wkbUnknown &&
                wkbFlatten(poResultGeom->getGeometryType()) !=
                    wkbFlatten(eLayerGeomType))
            {
                poResultGeom.reset(OGRGeometryFactory::forceTo(
                    poResultGeom.release(), eLayerGeomType));
            }

            if (poResultGeom == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to convert result from GEOS");
                return false;
            }
        }

#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
        GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
        m_papoGeosResults[i] = nullptr;
#undef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
#endif

        if (!skipFeature)
        {
            poResultGeom->assignSpatialReference(poResultSRS);
            poFeature->SetGeometry(std::move(poResultGeom));

            if (dstLayer.CreateFeature(std::move(poFeature)) != CE_None)
            {
                return false;
            }
        }
        else
        {
            poFeature.reset();
        }

        if (pfnProgress &&
            !pfnProgress(dfProgressStart +
                             static_cast<double>(i) * dfProgressRatio,
                         "", pProgressData))
        {
            ReportError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            return false;
        }
    }

    return true;
}

bool GDALGeosNonStreamingAlgorithmDataset::Process(OGRLayer &srcLayer,
                                                   OGRLayer &dstLayer,
                                                   int geomFieldIndex,
                                                   GDALProgressFunc pfnProgress,
                                                   void *pProgressData)
{
    Cleanup();

    const bool sameDefn =
        dstLayer.GetLayerDefn()->IsSame(srcLayer.GetLayerDefn());

    if (!ConvertInputsToGeos(srcLayer, dstLayer, geomFieldIndex, sameDefn,
                             pfnProgress, pProgressData))
    {
        return false;
    }

    if (!ProcessGeos() || m_poGeosResultAsCollection == nullptr)
    {
        return false;
    }

    const GIntBig nLayerFeatures = srcLayer.TestCapability(OLCFastFeatureCount)
                                       ? srcLayer.GetFeatureCount(false)
                                       : -1;
    const double dfProgressStart = nLayerFeatures > 0 ? 0.5 : 0.0;
    const double dfProgressRatio =
        (nLayerFeatures > 0 ? 0.5 : 1.0) /
        std::max(1.0, static_cast<double>(m_apoFeatures.size()));
    return ConvertOutputsFromGeos(dstLayer, pfnProgress, pProgressData,
                                  dfProgressStart, dfProgressRatio);
}

#endif

//! @endcond
