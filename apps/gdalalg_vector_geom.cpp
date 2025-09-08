/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom" step of "vector pipeline", or "gdal vector geom" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom.h"
#include "gdalalg_vector_set_geom_type.h"
#include "gdalalg_vector_explode_collections.h"
#include "gdalalg_vector_make_valid.h"
#include "gdalalg_vector_segmentize.h"
#include "gdalalg_vector_simplify.h"
#include "gdalalg_vector_buffer.h"
#include "gdalalg_vector_swap_xy.h"

#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm()         */
/************************************************************************/

GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep = */ false)
{
    m_hidden = true;

    RegisterSubAlgorithm<GDALVectorSetGeomTypeAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorExplodeCollectionsAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorMakeValidAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSegmentizeAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSimplifyAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorBufferAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSwapXYAlgorithm>(standaloneStep);
}

/************************************************************************/
/*              GDALVectorGeomAlgorithm::WarnIfDeprecated()             */
/************************************************************************/

void GDALVectorGeomAlgorithm::WarnIfDeprecated()
{
    ReportError(CE_Warning, CPLE_AppDefined,
                "'gdal vector geom' is deprecated in GDAL 3.12, and will be "
                "removed in GDAL 3.13. Is subcommands are directly available "
                "under 'gdal vector'");
}

/************************************************************************/
/*                GDALVectorGeomAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALVectorGeomAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vector geom\" program.");
    return false;
}

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

GDALVectorGeomAlgorithmStandalone::~GDALVectorGeomAlgorithmStandalone() =
    default;

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
    if (m_poGeosContext != nullptr)
    {
        for (auto &poGeom : m_apoGeosInputs)
        {
            GEOSGeom_destroy_r(m_poGeosContext, poGeom);
        }

        GEOSGeom_destroy_r(m_poGeosContext, m_poGeosResultAsCollection);

        for (size_t i = 0; i < m_nGeosResultSize; i++)
        {
            GEOSGeom_destroy_r(m_poGeosContext, m_papoGeosResults[i]);
        }

        GEOSFree_r(m_poGeosContext, m_papoGeosResults);
        finishGEOS_r(m_poGeosContext);
    }
}

bool GDALGeosNonStreamingAlgorithmDataset::ConvertInputsToGeos(
    OGRLayer &srcLayer, OGRLayer &dstLayer, bool sameDefn)
{
    for (auto &feature : srcLayer)
    {
        const OGRGeometry *poSrcGeom =
            feature->GetGeomFieldRef(m_sourceGeometryField);

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
    }

    return true;
}

bool GDALGeosNonStreamingAlgorithmDataset::ConvertOutputsFromGeos(
    OGRLayer &dstLayer)
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
    for (size_t i = 0; i < m_apoFeatures.size(); i++)
    {
        GEOSGeometry *poGeosResult = m_papoGeosResults[i];
#else
    auto nGeoms =
        GEOSGetNumGeometries_r(m_poGeosContext, m_poGeosResultAsCollection);
    for (decltype(nGeoms) i = 0; i < nGeoms; i++)
    {
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
            m_apoFeatures[i]->SetGeometry(std::move(poResultGeom));

            if (dstLayer.CreateFeature(m_apoFeatures[i].get()) != CE_None)
            {
                return false;
            }
        }

        m_apoFeatures[i].reset();
    }

    return true;
}

bool GDALGeosNonStreamingAlgorithmDataset::Process(OGRLayer &srcLayer,
                                                   OGRLayer &dstLayer)
{
    bool sameDefn = dstLayer.GetLayerDefn()->IsSame(srcLayer.GetLayerDefn());

    if (!ConvertInputsToGeos(srcLayer, dstLayer, sameDefn))
    {
        return false;
    }

    if (!ProcessGeos() || m_poGeosResultAsCollection == nullptr)
    {
        return false;
    }

    return ConvertOutputsFromGeos(dstLayer);
}

#endif

//! @endcond
