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

// GEOSGeom_releaseCollection allows us to take ownership of the contents of
// a GeometryCollection. We can then incrementally free the geometries as
// we write them to features. It requires GEOS >= 3.12.
#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12)
#define GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
#endif

/************************************************************************/
/*                    GDALGeosNonStreamingAlgorithmLayer              */
/************************************************************************/

GDALGeosNonStreamingAlgorithmLayer::GDALGeosNonStreamingAlgorithmLayer(
    OGRLayer &srcLayer, int geomFieldIndex)
    : GDALVectorNonStreamingAlgorithmLayer(srcLayer, geomFieldIndex),
      m_poGeosContext{OGRGeometry::createGEOSContext()}
{
}

GDALGeosNonStreamingAlgorithmLayer::~GDALGeosNonStreamingAlgorithmLayer()
{
    Cleanup();
    if (m_poGeosContext != nullptr)
    {
        finishGEOS_r(m_poGeosContext);
    }
}

void GDALGeosNonStreamingAlgorithmLayer::Cleanup()
{
    m_readPos = 0;
    m_apoFeatures.clear();

    if (m_poGeosContext != nullptr)
    {
        for (auto &poGeom : m_apoGeosInputs)
        {
            GEOSGeom_destroy_r(m_poGeosContext, poGeom);
        }
        m_apoGeosInputs.clear();

        if (m_poGeosResultAsCollection != nullptr)
        {
            GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);
            m_poGeosResultAsCollection = nullptr;
        }

        if (m_papoGeosResults != nullptr)
        {
            for (size_t i = 0; i < m_nGeosResultSize; i++)
            {
                if (m_papoGeosResults[i] != nullptr)
                {
                    GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
                }
            }

            GEOSFree_r(m_poGeosContext, m_papoGeosResults);
            m_nGeosResultSize = 0;
            m_papoGeosResults = nullptr;
        }
    }
}

bool GDALGeosNonStreamingAlgorithmLayer::ConvertInputsToGeos(
    OGRLayer &srcLayer, int geomFieldIndex, GDALProgressFunc pfnProgress,
    void *pProgressData)
{
    const GIntBig nLayerFeatures = srcLayer.TestCapability(OLCFastFeatureCount)
                                       ? srcLayer.GetFeatureCount(false)
                                       : -1;
    const double dfInvLayerFeatures =
        1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
    const double dfProgressRatio = dfInvLayerFeatures * 0.5;

    const bool sameDefn = GetLayerDefn()->IsSame(srcLayer.GetLayerDefn());

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
            feature->SetFDefnUnsafe(GetLayerDefn());
            m_apoFeatures.push_back(
                std::unique_ptr<OGRFeature>(feature.release()));
        }
        else
        {
            auto newFeature = std::make_unique<OGRFeature>(GetLayerDefn());
            newFeature->SetFrom(feature.get(), true);
            newFeature->SetFID(feature->GetFID());
            m_apoFeatures.push_back(std::move(newFeature));
        }

        if (pfnProgress && nLayerFeatures > 0 &&
            !pfnProgress(static_cast<double>(m_apoFeatures.size()) *
                             dfProgressRatio,
                         "", pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            return false;
        }
    }

    return true;
}

bool GDALGeosNonStreamingAlgorithmLayer::Process(GDALProgressFunc pfnProgress,
                                                 void *pProgressData)
{
    Cleanup();

    if (!ConvertInputsToGeos(m_srcLayer, m_geomFieldIndex, pfnProgress,
                             pProgressData))
    {
        return false;
    }

    if (!ProcessGeos() || m_poGeosResultAsCollection == nullptr)
    {
        return false;
    }

#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
    m_nGeosResultSize =
        GEOSGetNumGeometries_r(m_poGeosContext, m_poGeosResultAsCollection);
    m_papoGeosResults = GEOSGeom_releaseCollection_r(
        m_poGeosContext, m_poGeosResultAsCollection, &m_nGeosResultSize);
    GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);
    m_poGeosResultAsCollection = nullptr;
    CPLAssert(m_apoFeatures.size() == m_nGeosResultSize);
#endif

    return true;
}

std::unique_ptr<OGRFeature>
GDALGeosNonStreamingAlgorithmLayer::GetNextProcessedFeature()
{
    GEOSGeometry *poGeosResult = nullptr;

    while (poGeosResult == nullptr && m_readPos < m_apoFeatures.size())
    {
        // Have we already constructed a result OGRGeometry when previously
        // accessing this feature?
        if (m_apoFeatures[m_readPos]->GetGeometryRef() != nullptr)
        {
            return std::unique_ptr<OGRFeature>(
                m_apoFeatures[m_readPos++]->Clone());
        }

#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
        poGeosResult = m_papoGeosResults[m_readPos];
#else
        poGeosResult = const_cast<GEOSGeometry *>(GEOSGetGeometryN_r(
            m_poGeosContext, m_poGeosResultAsCollection, m_readPos));
#endif

        if (poGeosResult != nullptr)
        {
            const bool skipFeature =
                SkipEmpty() && GEOSisEmpty_r(m_poGeosContext, poGeosResult);

            if (skipFeature)
            {
#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
                GEOSGeom_destroy_r(m_poGeosContext, poGeosResult);
                m_papoGeosResults[m_readPos] = nullptr;
#endif
                poGeosResult = nullptr;
            }
        }

        m_readPos++;
    }

    if (poGeosResult == nullptr)
    {
#ifndef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
        GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);
        m_poGeosResultAsCollection = nullptr;
#endif
        return nullptr;
    }

    const auto eLayerGeomType = GetLayerDefn()->GetGeomType();

    std::unique_ptr<OGRGeometry> poResultGeom(
        OGRGeometryFactory::createFromGEOS(m_poGeosContext, poGeosResult));

#ifdef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL
    GEOSGeom_destroy_r(m_poGeosContext, poGeosResult);
    m_papoGeosResults[m_readPos - 1] = nullptr;
#endif

    if (poResultGeom && eLayerGeomType != wkbUnknown &&
        wkbFlatten(poResultGeom->getGeometryType()) !=
            wkbFlatten(eLayerGeomType))
    {
        poResultGeom.reset(OGRGeometryFactory::forceTo(poResultGeom.release(),
                                                       eLayerGeomType));
    }

    if (poResultGeom == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to convert result from GEOS");
        return nullptr;
    }

    poResultGeom->assignSpatialReference(
        GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef());

    auto poFeature = m_apoFeatures[m_readPos - 1].get();
    poFeature->SetGeometry(std::move(poResultGeom));

    return std::unique_ptr<OGRFeature>(poFeature->Clone());
}

#undef GDAL_GEOS_NON_STREAMING_ALGORITHM_DATASET_INCREMENTAL

void GDALGeosNonStreamingAlgorithmLayer::ResetReading()
{
    m_readPos = 0;
}

#endif

//! @endcond
