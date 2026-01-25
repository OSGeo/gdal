/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector explode-collections"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_explode_collections.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <list>
#include <utility>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*               GDALVectorExplodeCollectionsAlgorithm()                */
/************************************************************************/

GDALVectorExplodeCollectionsAlgorithm::GDALVectorExplodeCollectionsAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("geometry-type", 0, _("Geometry type"), &m_opts.m_type)
        .SetAutoCompleteFunction(
            [](const std::string &currentValue)
            {
                std::vector<std::string> oRet;
                for (const char *type :
                     {"GEOMETRY", "POINT", "LINESTRING", "POLYGON",
                      "CIRCULARSTRING", "COMPOUNDCURVE", "CURVEPOLYGON",
                      "POLYHEDRALSURFACE", "TIN"})
                {
                    if (currentValue.empty() ||
                        STARTS_WITH(type, currentValue.c_str()))
                    {
                        oRet.push_back(type);
                        oRet.push_back(std::string(type).append("Z"));
                        oRet.push_back(std::string(type).append("M"));
                        oRet.push_back(std::string(type).append("ZM"));
                    }
                }
                return oRet;
            });

    AddArg("skip-on-type-mismatch", 0,
           _("Skip feature when change of feature geometry type failed"),
           &m_opts.m_skip);
}

namespace
{

/************************************************************************/
/*              GDALVectorExplodeCollectionsAlgorithmLayer              */
/************************************************************************/

class GDALVectorExplodeCollectionsAlgorithmLayer final
    : public GDALVectorPipelineOutputLayer
{
  private:
    const GDALVectorExplodeCollectionsAlgorithm::Options m_opts;
    int m_iGeomIdx = -1;
    OGRFeatureDefn *const m_poFeatureDefn = nullptr;
    GIntBig m_nextFID = 1;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorExplodeCollectionsAlgorithmLayer)

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override;

    bool IsSelectedGeomField(int idx) const
    {
        return m_iGeomIdx < 0 || idx == m_iGeomIdx;
    }

  public:
    GDALVectorExplodeCollectionsAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorExplodeCollectionsAlgorithm::Options &opts);

    ~GDALVectorExplodeCollectionsAlgorithmLayer() override
    {
        m_poFeatureDefn->Release();
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn;
    }

    void ResetReading() override
    {
        m_nextFID = 1;
        GDALVectorPipelineOutputLayer::ResetReading();
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries) || EQUAL(pszCap, OLCFastGetExtent) ||
            EQUAL(pszCap, OLCStringsAsUTF8))
        {
            return m_srcLayer.TestCapability(pszCap);
        }
        return false;
    }
};

/************************************************************************/
/*             GDALVectorExplodeCollectionsAlgorithmLayer()             */
/************************************************************************/

GDALVectorExplodeCollectionsAlgorithmLayer::
    GDALVectorExplodeCollectionsAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorExplodeCollectionsAlgorithm::Options &opts)
    : GDALVectorPipelineOutputLayer(oSrcLayer), m_opts(opts),
      m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
{
    SetDescription(oSrcLayer.GetDescription());
    SetMetadata(oSrcLayer.GetMetadata());
    m_poFeatureDefn->Reference();

    if (!m_opts.m_geomField.empty())
    {
        const int nIdx = oSrcLayer.GetLayerDefn()->GetGeomFieldIndex(
            m_opts.m_geomField.c_str());
        if (nIdx >= 0)
            m_iGeomIdx = nIdx;
        else
            m_iGeomIdx = INT_MAX;
    }

    for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
    {
        if (IsSelectedGeomField(i))
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            poGeomFieldDefn->SetType(
                !m_opts.m_type.empty()
                    ? m_opts.m_eType
                    : OGR_GT_GetSingle(poGeomFieldDefn->GetType()));
        }
    }
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

void GDALVectorExplodeCollectionsAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature,
    std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures)
{
    std::list<std::pair<std::unique_ptr<OGRFeature>, int>> apoTmpFeatures;
    apoTmpFeatures.emplace_back(std::move(poSrcFeature), 0);
    const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    while (!apoTmpFeatures.empty())
    {
        auto [poCurFeature, nextGeomIndex] = std::move(apoTmpFeatures.front());
        auto insertionPoint = apoTmpFeatures.erase(apoTmpFeatures.begin());
        bool bInsertionDone = false;
        for (int i = nextGeomIndex; i < nGeomFieldCount; ++i)
        {
            auto poGeom = poCurFeature->GetGeomFieldRef(i);
            if (poGeom && !poGeom->IsEmpty() &&
                OGR_GT_IsSubClassOf(poGeom->getGeometryType(),
                                    wkbGeometryCollection) &&
                IsSelectedGeomField(i))
            {
                const auto poGeomFieldDefn =
                    m_poFeatureDefn->GetGeomFieldDefn(i);
                bInsertionDone = true;
                const auto eTargetType =
                    !m_opts.m_type.empty()
                        ? m_opts.m_eType
                        : OGR_GT_GetSingle(poGeomFieldDefn->GetType());
                auto poColl = std::unique_ptr<OGRGeometryCollection>(
                    poCurFeature->StealGeometry(i)->toGeometryCollection());
                bool bTmpFeaturesInserted = false;
                for (const auto *poSubGeomRef : poColl.get())
                {
                    auto poNewFeature =
                        std::unique_ptr<OGRFeature>(poCurFeature->Clone());
                    auto poNewGeom =
                        std::unique_ptr<OGRGeometry>(poSubGeomRef->clone());
                    if (poNewGeom->getGeometryType() != eTargetType)
                        poNewGeom = OGRGeometryFactory::forceTo(
                            std::move(poNewGeom), eTargetType);
                    if (m_opts.m_skip && !m_opts.m_type.empty() &&
                        (!poNewGeom ||
                         (wkbFlatten(eTargetType) != wkbUnknown &&
                          poNewGeom->getGeometryType() != eTargetType)))
                    {
                        // skip
                    }
                    else
                    {
                        poNewGeom->assignSpatialReference(
                            poGeomFieldDefn->GetSpatialRef());
                        poNewFeature->SetGeomFieldDirectly(i,
                                                           poNewGeom.release());

                        if (!m_opts.m_geomField.empty() ||
                            i == nGeomFieldCount - 1)
                        {
                            poNewFeature->SetFDefnUnsafe(m_poFeatureDefn);
                            poNewFeature->SetFID(m_nextFID);
                            ++m_nextFID;
                            apoOutFeatures.push_back(std::move(poNewFeature));
                        }
                        else
                        {
                            bTmpFeaturesInserted = true;
                            apoTmpFeatures.insert(
                                insertionPoint,
                                std::pair<std::unique_ptr<OGRFeature>, int>(
                                    std::move(poNewFeature),
                                    nextGeomIndex + 1));
                        }
                    }
                }

                if (bTmpFeaturesInserted)
                    break;
            }
            else if (poGeom)
            {
                const auto poGeomFieldDefn =
                    m_poFeatureDefn->GetGeomFieldDefn(i);
                poGeom->assignSpatialReference(
                    poGeomFieldDefn->GetSpatialRef());
            }
        }
        if (!bInsertionDone)
        {
            poCurFeature->SetFDefnUnsafe(m_poFeatureDefn);
            poCurFeature->SetFID(m_nextFID);
            ++m_nextFID;
            apoOutFeatures.push_back(std::move(poCurFeature));
        }
    }
}

}  // namespace

/************************************************************************/
/*       GDALVectorExplodeCollectionsAlgorithm::CreateAlgLayer()        */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorExplodeCollectionsAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorExplodeCollectionsAlgorithmLayer>(
        srcLayer, m_opts);
}

/************************************************************************/
/*           GDALVectorExplodeCollectionsAlgorithm::RunStep()           */
/************************************************************************/

bool GDALVectorExplodeCollectionsAlgorithm::RunStep(
    GDALPipelineStepRunContext &ctxt)
{
    if (!m_opts.m_type.empty())
    {
        m_opts.m_eType = OGRFromOGCGeomType(m_opts.m_type.c_str());
        if (wkbFlatten(m_opts.m_eType) == wkbUnknown &&
            !STARTS_WITH_CI(m_opts.m_type.c_str(), "GEOMETRY"))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Invalid geometry type '%s'", m_opts.m_type.c_str());
            return false;
        }
    }

    return GDALVectorGeomAbstractAlgorithm::RunStep(ctxt);
}

GDALVectorExplodeCollectionsAlgorithmStandalone::
    ~GDALVectorExplodeCollectionsAlgorithmStandalone() = default;

//! @endcond
