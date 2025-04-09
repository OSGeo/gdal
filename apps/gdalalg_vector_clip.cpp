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
              m_eFlattenSrcLayerGeomType, wkbGeometryCollection))
    {
        SetDescription(oSrcLayer.GetDescription());
        SetMetadata(oSrcLayer.GetMetadata());
        oSrcLayer.SetSpatialFilter(m_poClipGeom.get());
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_srcLayer.GetLayerDefn();
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        auto poGeom = poSrcFeature->GetGeometryRef();
        if (!poGeom)
            return;

        auto poIntersection = std::unique_ptr<OGRGeometry>(
            poGeom->Intersection(m_poClipGeom.get()));
        if (!poIntersection)
            return;

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
    std::unique_ptr<OGRGeometry> m_poClipGeom{};
    const OGRwkbGeometryType m_eSrcLayerGeomType;
    const OGRwkbGeometryType m_eFlattenSrcLayerGeomType;
    const bool m_bSrcLayerGeomTypeIsCollection;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorClipAlgorithmLayer)
};

}  // namespace

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

static std::unique_ptr<OGRGeometry> LoadGeometry(GDALDataset *poDS,
                                                 const std::string &osSQL,
                                                 const std::string &osLyr,
                                                 const std::string &osWhere)
{
    OGRLayer *poLyr = nullptr;
    if (!osSQL.empty())
        poLyr = poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
    else if (!osLyr.empty())
        poLyr = poDS->GetLayerByName(osLyr.c_str());
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from clipping dataset.");
        return nullptr;
    }

    if (!osWhere.empty())
        poLyr->SetAttributeFilter(osWhere.c_str());

    OGRGeometryCollection oGC;

    const auto poSRSSrc = poLyr->GetSpatialRef();
    if (poSRSSrc)
    {
        auto poSRSClone = poSRSSrc->Clone();
        oGC.assignSpatialReference(poSRSClone);
        poSRSClone->Release();
    }

    for (auto &poFeat : poLyr)
    {
        auto poSrcGeom = std::unique_ptr<OGRGeometry>(poFeat->StealGeometry());
        if (poSrcGeom)
        {
            // Only take into account areal geometries.
            if (poSrcGeom->getDimension() == 2)
            {
                if (!poSrcGeom->IsValid())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Geometry of feature " CPL_FRMT_GIB " of %s "
                             "is invalid.",
                             poFeat->GetFID(), poDS->GetDescription());
                    return nullptr;
                }
                else
                {
                    oGC.addGeometry(std::move(poSrcGeom));
                }
            }
        }
    }

    if (!osSQL.empty())
        poDS->ReleaseResultSet(poLyr);

    if (oGC.IsEmpty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No clipping geometry found");
        return nullptr;
    }

    return std::unique_ptr<OGRGeometry>(oGC.UnaryUnion());
}

/************************************************************************/
/*                 GDALVectorClipAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorClipAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::unique_ptr<OGRGeometry> poClipGeom;

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

    if (!m_bbox.empty())
    {
        poClipGeom = std::make_unique<OGRPolygon>(m_bbox[0], m_bbox[1],
                                                  m_bbox[2], m_bbox[3]);

        if (!m_bboxCrs.empty())
        {
            auto poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput(m_bboxCrs.c_str()));
            poClipGeom->assignSpatialReference(poSRS);
            poSRS->Release();
        }
    }
    else if (!m_geometry.empty())
    {
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            auto [poGeom, eErr] =
                OGRGeometryFactory::createFromWkt(m_geometry.c_str());
            if (eErr == OGRERR_NONE)
            {
                poClipGeom = std::move(poGeom);
            }
            else
            {
                poClipGeom.reset(
                    OGRGeometryFactory::createFromGeoJson(m_geometry.c_str()));
                if (poClipGeom)
                {
                    auto poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput("WGS84"));
                    poClipGeom->assignSpatialReference(poSRS);
                    poSRS->Release();
                }
            }
        }
        if (!poClipGeom)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Clipping geometry is neither a valid WKT or GeoJSON geometry");
            return false;
        }

        if (!m_geometryCrs.empty())
        {
            auto poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput(m_geometryCrs.c_str()));
            poClipGeom->assignSpatialReference(poSRS);
            poSRS->Release();
        }
    }
    else if (auto poLikeDS = m_likeDataset.GetDatasetRef())
    {
        if (poLikeDS->GetLayerCount() > 1 && m_likeLayer.empty() &&
            m_likeSQL.empty())
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Only single layer dataset can be specified with --like when "
                "neither --like-layer or --like-sql have been specified");
            return false;
        }
        else if (poLikeDS->GetLayerCount() > 0)
        {
            poClipGeom =
                LoadGeometry(poLikeDS, m_likeSQL, m_likeLayer, m_likeWhere);
            if (!poClipGeom)
                return false;
        }
        else if (poLikeDS->GetRasterCount() > 0)
        {
            double adfGT[6];
            if (poLikeDS->GetGeoTransform(adfGT) != CE_None)
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Dataset '%s' has no geotransform matrix. Its bounds "
                    "cannot be established.",
                    poLikeDS->GetDescription());
                return false;
            }
            auto poLikeSRS = poLikeDS->GetSpatialRef();
            if (bSrcLayerHasSRS && !poLikeSRS)
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "Dataset '%s' has no SRS. Assuming its SRS is the "
                            "same as the input vector.",
                            poLikeDS->GetDescription());
            }
            const double dfTLX = adfGT[0];
            const double dfTLY = adfGT[3];
            const double dfTRX =
                adfGT[0] + poLikeDS->GetRasterXSize() * adfGT[1];
            const double dfTRY =
                adfGT[3] + poLikeDS->GetRasterXSize() * adfGT[4];
            const double dfBLX =
                adfGT[0] + poLikeDS->GetRasterYSize() * adfGT[2];
            const double dfBLY =
                adfGT[3] + poLikeDS->GetRasterYSize() * adfGT[5];
            const double dfBRX = adfGT[0] +
                                 poLikeDS->GetRasterXSize() * adfGT[1] +
                                 poLikeDS->GetRasterYSize() * adfGT[2];
            const double dfBRY = adfGT[3] +
                                 poLikeDS->GetRasterXSize() * adfGT[4] +
                                 poLikeDS->GetRasterYSize() * adfGT[5];

            auto poPoly = std::make_unique<OGRPolygon>();
            auto poLR = std::make_unique<OGRLinearRing>();
            poLR->addPoint(dfTLX, dfTLY);
            poLR->addPoint(dfTRX, dfTRY);
            poLR->addPoint(dfBRX, dfBRY);
            poLR->addPoint(dfBLX, dfBLY);
            poLR->addPoint(dfTLX, dfTLY);
            poPoly->addRingDirectly(poLR.release());
            poPoly->assignSpatialReference(poLikeSRS);
            poClipGeom = std::move(poPoly);
        }
        else
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot get extent from clip dataset");
            return false;
        }
    }
    else
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "--bbox, --geometry or --like must be specified");
        return false;
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
