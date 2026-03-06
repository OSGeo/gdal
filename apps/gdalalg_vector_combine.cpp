/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector combine" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025-2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_combine.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"

#include <algorithm>
#include <cinttypes>
#include <optional>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorCombineAlgorithm::GDALVectorCombineAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("group-by", 0,
           _("Names of field(s) by which inputs should be grouped"), &m_groupBy)
        .AddValidationAction(
            [this]()
            {
                auto fields = m_groupBy;

                std::sort(fields.begin(), fields.end());
                if (std::adjacent_find(fields.begin(), fields.end()) !=
                    fields.end())
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "--group-by must be a list of unique field names.");
                    return false;
                }
                return true;
            });

    AddArg("keep-nested", 0,
           _("Avoid combining the components of multipart geometries"),
           &m_keepNested);
}

namespace
{
class GDALVectorCombineOutputLayer final
    : public GDALVectorNonStreamingAlgorithmLayer
{
  public:
    explicit GDALVectorCombineOutputLayer(
        OGRLayer &srcLayer, int geomFieldIndex,
        const std::vector<std::string> &groupBy, bool keepNested)
        : GDALVectorNonStreamingAlgorithmLayer(srcLayer, geomFieldIndex),
          m_groupBy(groupBy), m_defn(OGRFeatureDefn::CreateFeatureDefn(
                                  srcLayer.GetLayerDefn()->GetName())),
          m_keepNested(keepNested)
    {
        m_defn->Reference();

        const OGRFeatureDefn *srcDefn = m_srcLayer.GetLayerDefn();

        // Copy field definitions for attribute fields used in
        // --group-by. All other attributes are discarded.
        for (const auto &fieldName : m_groupBy)
        {
            // RunStep already checked that the field exists
            const auto iField = srcDefn->GetFieldIndex(fieldName.c_str());
            CPLAssert(iField >= 0);

            m_srcFieldIndices.push_back(iField);
            m_defn->AddFieldDefn(srcDefn->GetFieldDefn(iField));
        }

        // Create a new geometry field corresponding to each input geometry
        // field. An appropriate type is worked out below.
        m_defn->SetGeomType(wkbNone);  // Remove default geometry field
        for (const OGRGeomFieldDefn *srcGeomDefn : srcDefn->GetGeomFields())
        {
            const auto eSrcGeomType = srcGeomDefn->GetType();
            const bool bHasZ = OGR_GT_HasZ(eSrcGeomType);
            const bool bHasM = OGR_GT_HasM(eSrcGeomType);

            OGRwkbGeometryType eDstGeomType =
                OGR_GT_SetModifier(wkbGeometryCollection, bHasZ, bHasM);

            // If the layer claims to have single-part geometries, choose a more
            // specific output type like "MultiPoint" rather than "GeometryCollection"
            if (wkbFlatten(eSrcGeomType) != wkbUnknown &&
                !OGR_GT_IsSubClassOf(wkbFlatten(eSrcGeomType),
                                     wkbGeometryCollection))
            {
                eDstGeomType = OGR_GT_GetCollection(eSrcGeomType);
            }

            auto dstGeomDefn = std::make_unique<OGRGeomFieldDefn>(
                srcGeomDefn->GetNameRef(), eDstGeomType);
            dstGeomDefn->SetSpatialRef(srcGeomDefn->GetSpatialRef());
            m_defn->AddGeomFieldDefn(std::move(dstGeomDefn));
        }
    }

    ~GDALVectorCombineOutputLayer() override
    {
        m_defn->Release();
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
        {
            return static_cast<GIntBig>(m_features.size());
        }

        return OGRLayer::GetFeatureCount(bForce);
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_defn;
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                        bool bForce) override
    {
        return m_srcLayer.GetExtent3D(iGeomField, psExtent, bForce);
    }

    std::unique_ptr<OGRFeature> GetNextProcessedFeature() override
    {
        if (!m_itFeature)
        {
            m_itFeature = m_features.begin();
        }

        if (m_itFeature.value() == m_features.end())
        {
            return nullptr;
        }

        std::unique_ptr<OGRFeature> feature(
            m_itFeature.value()->second->Clone());
        feature->SetFID(m_nProcessedFeaturesRead++);
        ++m_itFeature.value();
        return feature;
    }

    bool Process(GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        const int nGeomFields = m_srcLayer.GetLayerDefn()->GetGeomFieldCount();

        const GIntBig nLayerFeatures =
            m_srcLayer.TestCapability(OLCFastFeatureCount)
                ? m_srcLayer.GetFeatureCount(false)
                : -1;
        const double dfInvLayerFeatures =
            1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));

        GIntBig nFeaturesRead = 0;

        std::vector<std::string> fieldValues(m_srcFieldIndices.size());
        const auto srcDstFieldMap =
            m_defn->ComputeMapForSetFrom(m_srcLayer.GetLayerDefn(), true);
        for (const auto &srcFeature : m_srcLayer)
        {
            for (size_t iDstField = 0; iDstField < m_srcFieldIndices.size();
                 iDstField++)
            {
                const int iSrcField = m_srcFieldIndices[iDstField];
                fieldValues[iDstField] =
                    srcFeature->GetFieldAsString(iSrcField);
            }

            OGRFeature *dstFeature;

            if (auto it = m_features.find(fieldValues); it == m_features.end())
            {
                it = m_features
                         .insert(std::pair(
                             fieldValues, std::make_unique<OGRFeature>(m_defn)))
                         .first;
                dstFeature = it->second.get();

                dstFeature->SetFrom(srcFeature.get(), srcDstFieldMap.data(),
                                    false);

                for (int iGeomField = 0; iGeomField < nGeomFields; iGeomField++)
                {
                    OGRGeomFieldDefn *poGeomDefn =
                        m_defn->GetGeomFieldDefn(iGeomField);
                    const auto eGeomType = poGeomDefn->GetType();

                    std::unique_ptr<OGRGeometry> poGeom(
                        OGRGeometryFactory::createGeometry(eGeomType));
                    poGeom->assignSpatialReference(poGeomDefn->GetSpatialRef());

                    dstFeature->SetGeomField(iGeomField, std::move(poGeom));
                }
            }
            else
            {
                dstFeature = it->second.get();
            }

            for (int iGeomField = 0; iGeomField < nGeomFields; iGeomField++)
            {
                OGRGeomFieldDefn *poGeomFieldDefn =
                    m_defn->GetGeomFieldDefn(iGeomField);

                std::unique_ptr<OGRGeometry> poSrcGeom(
                    srcFeature->StealGeometry(iGeomField));
                if (poSrcGeom != nullptr && !poSrcGeom->IsEmpty())
                {
                    const auto eSrcType = poSrcGeom->getGeometryType();
                    const auto bSrcIsCollection = OGR_GT_IsSubClassOf(
                        wkbFlatten(eSrcType), wkbGeometryCollection);
                    const auto bDstIsUntypedCollection =
                        wkbFlatten(poGeomFieldDefn->GetType()) ==
                        wkbGeometryCollection;

                    // Did this geometry unexpectedly have Z?
                    if (OGR_GT_HasZ(eSrcType) !=
                        OGR_GT_HasZ(poGeomFieldDefn->GetType()))
                    {
                        AddZ(iGeomField);
                    }

                    // Did this geometry unexpectedly have M?
                    if (OGR_GT_HasM(eSrcType) !=
                        OGR_GT_HasM(poGeomFieldDefn->GetType()))
                    {
                        AddM(iGeomField);
                    }

                    // Do we need to change the output from a typed collection
                    // like MultiPolygon to a generic GeometryCollection?
                    if (m_keepNested && bSrcIsCollection &&
                        !bDstIsUntypedCollection)
                    {
                        SetTypeGeometryCollection(iGeomField);
                    }

                    OGRGeometryCollection *poDstGeom =
                        cpl::down_cast<OGRGeometryCollection *>(
                            dstFeature->GetGeomFieldRef(iGeomField));

                    if (m_keepNested || !bSrcIsCollection)
                    {
                        if (poDstGeom->addGeometry(std::move(poSrcGeom)) !=
                            OGRERR_NONE)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Failed to add geometry of type %s to output "
                                "feature of type %s",
                                OGRGeometryTypeToName(eSrcType),
                                OGRGeometryTypeToName(
                                    poDstGeom->getGeometryType()));
                            return false;
                        }
                    }
                    else
                    {
                        std::unique_ptr<OGRGeometryCollection>
                            poSrcGeomCollection(
                                poSrcGeom.release()->toGeometryCollection());
                        if (poDstGeom->addGeometryComponents(
                                std::move(poSrcGeomCollection)) != OGRERR_NONE)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Failed to add components from geometry "
                                     "of type %s to output "
                                     "feature of type %s",
                                     OGRGeometryTypeToName(eSrcType),
                                     OGRGeometryTypeToName(
                                         poDstGeom->getGeometryType()));
                            return false;
                        }
                    }
                }
            }

            if (pfnProgress && nLayerFeatures > 0 &&
                !pfnProgress(static_cast<double>(++nFeaturesRead) *
                                 dfInvLayerFeatures,
                             "", pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
                return false;
            }
        }

        if (pfnProgress)
        {
            pfnProgress(1.0, "", pProgressData);
        }

        return true;
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastFeatureCount))
        {
            return true;
        }

        if (EQUAL(pszCap, OLCStringsAsUTF8) ||
            EQUAL(pszCap, OLCFastGetExtent) ||
            EQUAL(pszCap, OLCFastGetExtent3D) ||
            EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries))
        {
            return m_srcLayer.TestCapability(pszCap);
        }

        return false;
    }

    void ResetReading() override
    {
        m_itFeature.reset();
        m_nProcessedFeaturesRead = 0;
    }

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorCombineOutputLayer)

  private:
    void AddM(int iGeomField)
    {
        OGRGeomFieldDefn *poGeomFieldDefn =
            m_defn->GetGeomFieldDefn(iGeomField);
        whileUnsealing(poGeomFieldDefn)
            ->SetType(OGR_GT_SetM(poGeomFieldDefn->GetType()));

        for (auto &[_, poFeature] : m_features)
        {
            poFeature->GetGeomFieldRef(iGeomField)->setMeasured(true);
        }
    }

    void AddZ(int iGeomField)
    {
        OGRGeomFieldDefn *poGeomFieldDefn =
            m_defn->GetGeomFieldDefn(iGeomField);
        whileUnsealing(poGeomFieldDefn)
            ->SetType(OGR_GT_SetZ(poGeomFieldDefn->GetType()));

        for (auto &[_, poFeature] : m_features)
        {
            poFeature->GetGeomFieldRef(iGeomField)->set3D(true);
        }
    }

    void SetTypeGeometryCollection(int iGeomField)
    {
        OGRGeomFieldDefn *poGeomFieldDefn =
            m_defn->GetGeomFieldDefn(iGeomField);
        const bool hasZ = OGR_GT_HasZ(poGeomFieldDefn->GetType());
        const bool hasM = OGR_GT_HasM(poGeomFieldDefn->GetType());

        whileUnsealing(poGeomFieldDefn)
            ->SetType(OGR_GT_SetModifier(wkbGeometryCollection, hasZ, hasM));

        for (auto &[_, poFeature] : m_features)
        {
            std::unique_ptr<OGRGeometry> poTmpGeom(
                poFeature->StealGeometry(iGeomField));
            poTmpGeom = OGRGeometryFactory::forceTo(std::move(poTmpGeom),
                                                    poGeomFieldDefn->GetType());
            CPLAssert(poTmpGeom);
            poFeature->SetGeomField(iGeomField, std::move(poTmpGeom));
        }
    }

    const std::vector<std::string> m_groupBy{};
    std::vector<int> m_srcFieldIndices{};
    std::map<std::vector<std::string>, std::unique_ptr<OGRFeature>>
        m_features{};
    std::optional<decltype(m_features)::const_iterator> m_itFeature{};
    OGRFeatureDefn *const m_defn;
    GIntBig m_nProcessedFeaturesRead = 0;
    const bool m_keepNested;
};
}  // namespace

bool GDALVectorCombineAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS = std::make_unique<GDALVectorNonStreamingAlgorithmDataset>();

    GDALVectorAlgorithmLayerProgressHelper progressHelper(ctxt);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() == 0)
            {
                if (m_inputLayerNames.empty())
                    continue;
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified layer '%s' has no geometry field",
                            poSrcLayer->GetDescription());
                return false;
            }

            // Check that all attributes exist
            for (const auto &fieldName : m_groupBy)
            {
                const int iSrcFieldIndex =
                    poSrcLayerDefn->GetFieldIndex(fieldName.c_str());
                if (iSrcFieldIndex == -1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Specified attribute field '%s' does not exist "
                                "in layer '%s'",
                                fieldName.c_str(),
                                poSrcLayer->GetDescription());
                    return false;
                }
            }

            progressHelper.AddProcessedLayer(*poSrcLayer);
        }
    }

    for ([[maybe_unused]] auto [poSrcLayer, bProcessed, layerProgressFunc,
                                layerProgressData] : progressHelper)
    {
        auto poLayer = std::make_unique<GDALVectorCombineOutputLayer>(
            *poSrcLayer, -1, m_groupBy, m_keepNested);

        if (!poDstDS->AddProcessedLayer(std::move(poLayer), layerProgressFunc,
                                        layerProgressData.get()))
        {
            return false;
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

GDALVectorCombineAlgorithmStandalone::~GDALVectorCombineAlgorithmStandalone() =
    default;

//! @endcond
