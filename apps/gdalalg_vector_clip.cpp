/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "clip" step of "vector pipeline", or "gdal vector clip" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_clip.h"

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorClipAlgorithm::GDALVectorClipAlgorithm()          */
/************************************************************************/

GDALVectorClipAlgorithm::GDALVectorClipAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddBBOXArg(&m_bbox, _("Clipping bounding box as xmin,ymin,xmax,ymax"))
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("bbox-crs", 0, _("CRS of clipping bounding box"), &m_bboxCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("bbox_srs");
    AddArg("geometry", 0, _("Clipping geometry (WKT or GeoJSON)"), &m_geometry)
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("geometry-crs", 0, _("CRS of clipping geometry"), &m_geometryCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("geometry_srs");
    AddArg("like", 0, _("Dataset to use as a template for bounds"),
           &m_likeDataset, GDAL_OF_RASTER | GDAL_OF_VECTOR)
        .SetMetaVar("DATASET")
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("like-sql", 0, ("SELECT statement to run on the 'like' dataset"),
           &m_likeSQL)
        .SetMetaVar("SELECT-STATEMENT")
        .SetMutualExclusionGroup("sql-where");
    AddArg("like-layer", 0, ("Name of the layer of the 'like' dataset"),
           &m_likeLayer)
        .SetMetaVar("LAYER-NAME");
    AddArg("like-where", 0, ("WHERE SQL clause to run on the 'like' dataset"),
           &m_likeWhere)
        .SetMetaVar("WHERE-EXPRESSION")
        .SetMutualExclusionGroup("sql-where");
}

/************************************************************************/
/*                   GDALVectorClipAlgorithmLayer                       */
/************************************************************************/

namespace
{
class GDALVectorClipAlgorithmLayer final : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorClipAlgorithmLayer(OGRLayer &oSrcLayer,
                                 std::unique_ptr<OGRGeometry> poClipGeom)
        : GDALVectorPipelineOutputLayer(oSrcLayer),
          m_poClipGeom(std::move(poClipGeom)),
          m_eSrcLayerGeomType(oSrcLayer.GetGeomType()),
          m_eFlattenSrcLayerGeomType(wkbFlatten(m_eSrcLayerGeomType)),
          m_bSrcLayerGeomTypeIsCollection(OGR_GT_IsSubClassOf(
              m_eFlattenSrcLayerGeomType, wkbGeometryCollection)),
          m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
    {
        SetDescription(oSrcLayer.GetDescription());
        SetMetadata(oSrcLayer.GetMetadata());
        oSrcLayer.SetSpatialFilter(m_poClipGeom.get());
        m_poFeatureDefn->Reference();
    }

    ~GDALVectorClipAlgorithmLayer()
    {
        m_poFeatureDefn->Release();
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        std::unique_ptr<OGRGeometry> poIntersection;
        auto poGeom = poSrcFeature->GetGeometryRef();
        if (poGeom)
        {
            poIntersection.reset(poGeom->Intersection(m_poClipGeom.get()));
        }
        if (!poIntersection)
            return;
        poIntersection->assignSpatialReference(
            m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef());

        poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn);

        const auto eFeatGeomType =
            wkbFlatten(poIntersection->getGeometryType());
        if (m_eFlattenSrcLayerGeomType != wkbUnknown &&
            m_eFlattenSrcLayerGeomType != eFeatGeomType)
        {
            // If the intersection is a collection of geometry and the
            // layer geometry type is of non-collection type, create
            // one feature per element of the collection.
            if (!m_bSrcLayerGeomTypeIsCollection &&
                OGR_GT_IsSubClassOf(eFeatGeomType, wkbGeometryCollection))
            {
                auto poGeomColl = std::unique_ptr<OGRGeometryCollection>(
                    poIntersection.release()->toGeometryCollection());
                for (const auto *poSubGeom : poGeomColl.get())
                {
                    auto poDstFeature =
                        std::unique_ptr<OGRFeature>(poSrcFeature->Clone());
                    poDstFeature->SetGeometry(poSubGeom);
                    apoOutFeatures.push_back(std::move(poDstFeature));
                }
            }
            else if (OGR_GT_GetCollection(eFeatGeomType) ==
                     m_eFlattenSrcLayerGeomType)
            {
                poIntersection.reset(OGRGeometryFactory::forceTo(
                    poIntersection.release(), m_eSrcLayerGeomType));
                poSrcFeature->SetGeometryDirectly(poIntersection.release());
                apoOutFeatures.push_back(std::move(poSrcFeature));
            }
            else if (m_eFlattenSrcLayerGeomType == wkbGeometryCollection)
            {
                auto poGeomColl = std::make_unique<OGRGeometryCollection>();
                poGeomColl->addGeometry(std::move(poIntersection));
                poSrcFeature->SetGeometryDirectly(poGeomColl.release());
                apoOutFeatures.push_back(std::move(poSrcFeature));
            }
            // else discard geometries of incompatible type with the
            // layer geometry type
        }
        else
        {
            poSrcFeature->SetGeometryDirectly(poIntersection.release());
            apoOutFeatures.push_back(std::move(poSrcFeature));
        }
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCStringsAsUTF8) ||
            EQUAL(pszCap, OLCCurveGeometries) || EQUAL(pszCap, OLCZGeometries))
            return m_srcLayer.TestCapability(pszCap);
        return false;
    }

  private:
    std::unique_ptr<OGRGeometry> const m_poClipGeom{};
    const OGRwkbGeometryType m_eSrcLayerGeomType;
    const OGRwkbGeometryType m_eFlattenSrcLayerGeomType;
    const bool m_bSrcLayerGeomTypeIsCollection;
    OGRFeatureDefn *const m_poFeatureDefn;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorClipAlgorithmLayer)
};

}  // namespace

/************************************************************************/
/*                 GDALVectorClipAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorClipAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();
    bool bSrcLayerHasSRS = false;
    for (int i = 0; i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        if (poSrcLayer &&
            (m_activeLayer.empty() ||
             m_activeLayer == poSrcLayer->GetDescription()) &&
            poSrcLayer->GetSpatialRef())
        {
            bSrcLayerHasSRS = true;
            break;
        }
    }

    auto [poClipGeom, errMsg] = GetClipGeometry();
    if (!poClipGeom)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "%s", errMsg.c_str());
        return false;
    }

    auto poLikeDS = m_likeDataset.GetDatasetRef();
    if (bSrcLayerHasSRS && !poClipGeom->getSpatialReference() && poLikeDS &&
        poLikeDS->GetLayerCount() == 0)
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Dataset '%s' has no CRS. Assuming its CRS is the "
                    "same as the input vector.",
                    poLikeDS->GetDescription());
    }

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    bool ret = true;
    for (int i = 0; ret && i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        ret = (poSrcLayer != nullptr);
        if (ret)
        {
            if (m_activeLayer.empty() ||
                m_activeLayer == poSrcLayer->GetDescription())
            {
                auto poClipGeomForLayer =
                    std::unique_ptr<OGRGeometry>(poClipGeom->clone());
                if (poClipGeomForLayer->getSpatialReference() &&
                    poSrcLayer->GetSpatialRef())
                {
                    ret = poClipGeomForLayer->transformTo(
                              poSrcLayer->GetSpatialRef()) == OGRERR_NONE;
                }
                if (ret)
                {
                    outDS->AddLayer(
                        *poSrcLayer,
                        std::make_unique<GDALVectorClipAlgorithmLayer>(
                            *poSrcLayer, std::move(poClipGeomForLayer)));
                }
            }
            else
            {
                outDS->AddLayer(
                    *poSrcLayer,
                    std::make_unique<GDALVectorPipelinePassthroughLayer>(
                        *poSrcLayer));
            }
        }
    }

    if (ret)
        m_outputDataset.Set(std::move(outDS));

    return ret;
}

//! @endcond
