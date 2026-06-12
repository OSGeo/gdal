/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "explode" step of "vector pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_explode.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <cinttypes>
#include <list>
#include <memory>
#include <numeric>
#include <vector>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALVectorExplodeAlgorithm::GDALVectorExplodeAlgorithm()       */
/************************************************************************/

GDALVectorExplodeAlgorithm::GDALVectorExplodeAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);

    {
        auto &arg =
            AddArg("field", 0, _("Attribute fields(s) to explode"), &m_fields)
                .SetMetaVar("FIELD");

        SetAutoCompleteFunctionForFieldName(
            arg, nullptr, true, false, m_inputDataset, {"ALL"},
            [](const OGRFieldDefn *defn)
            { return OGR_GetFieldTypeIsList(defn->GetType()); });
    }

    AddArg("geometry", 0, _("Explode default geometry field"), &m_defaultGeom);

    {
        auto &arg = AddArg("geometry-field", 0,
                           _("Geometry field(s) to explode"), &m_geomFields)
                        .SetMetaVar("GEOMETRY-FIELD");
        SetAutoCompleteFunctionForFieldName(arg, nullptr, false, true,
                                            m_inputDataset, {"ALL"});
    }

    AddArg("index-field", 0, _("Name of the output index field"),
           &m_indexFieldName)
        .SetDefault(m_indexFieldName);
}

GDALVectorExplodeAlgorithmStandalone::~GDALVectorExplodeAlgorithmStandalone() =
    default;

namespace
{

class GDALVectorExplodeLayer final : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorExplodeLayer(OGRLayer &srcLayer,
                           const std::vector<std::string> &fieldsToExplode,
                           const std::vector<std::string> &geomFieldsToExplode,
                           const std::string &indexFieldName)
        : GDALVectorPipelineOutputLayer(srcLayer),
          m_fieldsToExplode(fieldsToExplode),
          m_geomFieldsToExplode(geomFieldsToExplode),
          m_indexFieldName(indexFieldName)
    {
        if (!PrepareFeatureDefn())
        {
            m_setupError = true;
        }
    }

    bool PrepareFeatureDefn()
    {
        m_poFeatureDefn.reset(
            OGRFeatureDefn::CreateFeatureDefn(m_srcLayer.GetName()));

        // Avoid creating geometry field with null SRS
        // We'll copy it in later from the source layer
        m_poFeatureDefn->DeleteGeomFieldDefn(0);

        const bool addIndexField = !m_indexFieldName.empty();

        if (addIndexField)
        {
            auto poIdxField = std::make_unique<OGRFieldDefn>(
                m_indexFieldName.c_str(), OFTInteger);
            m_poFeatureDefn->AddFieldDefn(std::move(poIdxField));
        }

        const OGRFeatureDefn *poSrcDefn = m_srcLayer.GetLayerDefn();

        // By default, all fields copied as-is.
        m_unnestedFieldSrcToDstMap.resize(poSrcDefn->GetFieldCount(), -1);
        m_passThroughFieldSrcToDstMap.resize(poSrcDefn->GetFieldCount());
        std::iota(m_passThroughFieldSrcToDstMap.begin(),
                  m_passThroughFieldSrcToDstMap.end(), addIndexField ? 1 : 0);

        m_geomFieldExploded.resize(poSrcDefn->GetGeomFieldCount(), false);

        for (const auto &fieldName : m_fieldsToExplode)
        {
            const int iSrcField = poSrcDefn->GetFieldIndex(fieldName.c_str());
            if (iSrcField < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field '%s' not found in source layer.",
                         fieldName.c_str());
                return false;
            }

            const OGRFieldDefn *poSrcFieldDefn =
                poSrcDefn->GetFieldDefn(iSrcField);
            const auto eSrcType = poSrcFieldDefn->GetType();
            if (OGR_GetFieldTypeIsList(eSrcType))
            {
                m_passThroughFieldSrcToDstMap[iSrcField] = -1;
                m_unnestedFieldSrcToDstMap[iSrcField] =
                    iSrcField + addIndexField;
            }
        }

        for (const auto &fieldName : m_geomFieldsToExplode)
        {
            // Is it a geometry field?
            int iSrcGeomField = poSrcDefn->GetGeomFieldIndex(fieldName.c_str());

            // Interpret --geometry-field _OGR_GEOMETRY_ as the first geometry
            // field, regardless of what it is actually named
            if (iSrcGeomField < 0)
            {
                if (poSrcDefn->GetGeomFieldCount() > 0 &&
                    EQUAL(fieldName.c_str(),
                          OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME))
                {
                    iSrcGeomField = 0;
                }
            }

            // Didn't find anything by name. Check by index.
            if (iSrcGeomField < 0 &&
                std::all_of(
                    fieldName.begin(), fieldName.end(), [](char c)
                    { return std::isdigit(static_cast<unsigned char>(c)); }))
            {
                const int iGeomField = std::atoi(fieldName.c_str());

                if (iGeomField < poSrcDefn->GetGeomFieldCount())
                {
                    iSrcGeomField = iGeomField;
                }
            }

            if (iSrcGeomField < 0)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Could not find geometry field '%s' in source layer '%s'",
                    fieldName.c_str(), m_srcLayer.GetName());
                return false;
            }

            m_geomFieldExploded[iSrcGeomField] = true;
        }

        // Create attribute fields
        for (int iSrcField = 0; iSrcField < poSrcDefn->GetFieldCount();
             iSrcField++)
        {
            const auto *poSrcFieldDefn = poSrcDefn->GetFieldDefn(iSrcField);
            std::unique_ptr<OGRFieldDefn> poDstFieldDefn;

            if (m_passThroughFieldSrcToDstMap[iSrcField] != -1)
            {
                poDstFieldDefn =
                    std::make_unique<OGRFieldDefn>(*poSrcFieldDefn);
            }
            else
            {
                const auto eScalarType =
                    OGR_GetFieldTypeAsScalar(poSrcFieldDefn->GetType());
                poDstFieldDefn = std::make_unique<OGRFieldDefn>(
                    poSrcFieldDefn->GetNameRef(), eScalarType);
            }

            m_poFeatureDefn->AddFieldDefn(std::move(poDstFieldDefn));
        }

        // Create geometry fields
        for (int iSrcGeomField = 0;
             iSrcGeomField < poSrcDefn->GetGeomFieldCount(); iSrcGeomField++)
        {
            const OGRGeomFieldDefn *poSrcGeomFieldDefn =
                poSrcDefn->GetGeomFieldDefn(iSrcGeomField);
            std::unique_ptr<OGRGeomFieldDefn> poDstGeomFieldDefn;

            if (m_geomFieldExploded[iSrcGeomField])
            {
                const auto eDstType =
                    OGR_GT_GetSingle(poSrcGeomFieldDefn->GetType());
                poDstGeomFieldDefn = std::make_unique<OGRGeomFieldDefn>(
                    poSrcGeomFieldDefn->GetNameRef(), eDstType);
                poDstGeomFieldDefn->SetSpatialRef(
                    poSrcGeomFieldDefn->GetSpatialRef());
            }
            else
            {
                poDstGeomFieldDefn =
                    std::make_unique<OGRGeomFieldDefn>(*poSrcGeomFieldDefn);
            }

            m_poFeatureDefn->AddGeomFieldDefn(std::move(poDstGeomFieldDefn));
        }

        return true;
    }

    const char *GetDescription() const override
    {
        return m_poFeatureDefn->GetName();
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn.get();
    }

    void ResetReading() override
    {
        m_nextFID = 1;
        GDALVectorPipelineOutputLayer::ResetReading();
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastGetExtent) ||
            EQUAL(pszCap, OLCFastGetExtent3D) ||
            EQUAL(pszCap, OLCStringsAsUTF8) ||
            EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries))
        {
            return m_srcLayer.TestCapability(pszCap);
        }

        return false;
    }

    bool TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        if (m_setupError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to prepare output layer.");
            return false;
        }

        int nDstFeatures = 1;

        for (int iDstFeature = 0; iDstFeature < nDstFeatures; iDstFeature++)
        {
            auto poDstFeature =
                std::make_unique<OGRFeature>(m_poFeatureDefn.get());
            if (!m_indexFieldName.empty())
            {
                poDstFeature->SetField(0, iDstFeature);
            }

            if (poDstFeature->SetFieldsFrom(
                    poSrcFeature.get(), m_passThroughFieldSrcToDstMap.data(),
                    true) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to set fields of output feature");
                return false;
            }

            for (int iSrcArrayField = 0;
                 iSrcArrayField <
                 static_cast<int>(m_unnestedFieldSrcToDstMap.size());
                 iSrcArrayField++)
            {
                const int iDstField =
                    m_unnestedFieldSrcToDstMap[iSrcArrayField];
                if (iDstField < 0)
                {
                    continue;
                }

                const auto poSrcFieldDefn =
                    poSrcFeature->GetFieldDefnRef(iSrcArrayField);
                const auto eSrcType = poSrcFieldDefn->GetType();
                int nArrayLength = -1;
                if (eSrcType == OFTIntegerList)
                {
                    const int *pnArray = poSrcFeature->GetFieldAsIntegerList(
                        iSrcArrayField, &nArrayLength);
                    if (iDstFeature >= nArrayLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Field '%s' of source feature %" PRId64
                                 " does not have enough elements.",
                                 poSrcFieldDefn->GetNameRef(),
                                 static_cast<int64_t>(poSrcFeature->GetFID()));
                        return false;
                    }
                    poDstFeature->SetField(iDstField, pnArray[iDstFeature]);
                }
                else if (eSrcType == OFTInteger64List)
                {
                    const GIntBig *pnArray =
                        poSrcFeature->GetFieldAsInteger64List(iSrcArrayField,
                                                              &nArrayLength);
                    if (iDstFeature >= nArrayLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Field '%s' of source feature %" PRId64
                                 " does not have enough elements.",
                                 poSrcFieldDefn->GetNameRef(),
                                 static_cast<int64_t>(poSrcFeature->GetFID()));
                        return false;
                    }
                    poDstFeature->SetField(iDstField, pnArray[iDstFeature]);
                }
                else if (eSrcType == OFTRealList)
                {
                    const double *padfArray =
                        poSrcFeature->GetFieldAsDoubleList(iSrcArrayField,
                                                           &nArrayLength);
                    if (iDstFeature >= nArrayLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Field '%s' of source feature %" PRId64
                                 " does not have enough elements.",
                                 poSrcFieldDefn->GetNameRef(),
                                 static_cast<int64_t>(poSrcFeature->GetFID()));
                        return false;
                    }
                    poDstFeature->SetField(iDstField, padfArray[iDstFeature]);
                }
                else if (eSrcType == OFTStringList)
                {
                    CSLConstList papszArray =
                        poSrcFeature->GetFieldAsStringList(iSrcArrayField);
                    nArrayLength = CSLCount(papszArray);
                    if (iDstFeature >= nArrayLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Field '%s' of source feature %" PRId64
                                 " does not have enough elements.",
                                 poSrcFieldDefn->GetNameRef(),
                                 static_cast<int64_t>(poSrcFeature->GetFID()));
                        return false;
                    }
                    poDstFeature->SetField(iDstField, papszArray[iDstFeature]);
                }
                nDstFeatures = std::max(nDstFeatures, nArrayLength);
            }

            for (int iGeomField = 0;
                 iGeomField < poSrcFeature->GetGeomFieldCount(); iGeomField++)
            {
                if (m_geomFieldExploded[iGeomField])
                {
                    std::unique_ptr<OGRGeometry> poDstGeom;

                    OGRGeometry *poSrcGeom(
                        poSrcFeature->GetGeomFieldRef(iGeomField));

                    const bool bSrcIsCollection =
                        poSrcGeom != nullptr &&
                        OGR_GT_IsSubClassOf(
                            wkbFlatten(poSrcGeom->getGeometryType()),
                            wkbGeometryCollection);

                    if (bSrcIsCollection)
                    {
                        OGRGeometryCollection *poColl =
                            poSrcGeom->toGeometryCollection();

                        auto nGeoms = poColl->getNumGeometries();
                        nDstFeatures = std::max(nDstFeatures, nGeoms);

                        if (nGeoms == 0)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Geometry field '%s' of source feature %" PRId64
                                " has %d elements (expected %d)",
                                poSrcFeature->GetDefnRef()
                                    ->GetGeomFieldDefn(iGeomField)
                                    ->GetNameRef(),
                                static_cast<int64_t>(poSrcFeature->GetFID()),
                                nGeoms + iDstFeature, nDstFeatures);
                            return false;
                        }

                        poDstGeom = poColl->stealGeometry(0);
                    }
                    else
                    {
                        if (iDstFeature > 1 &&
                            apoOutFeatures.front()->GetGeomFieldRef(
                                iGeomField) != nullptr)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Geometry field '%s' of source feature %" PRId64
                                " is not a collection.",
                                poSrcFeature->GetDefnRef()
                                    ->GetGeomFieldDefn(iGeomField)
                                    ->GetNameRef(),
                                static_cast<int64_t>(poSrcFeature->GetFID()));
                            return false;
                        }

                        poDstGeom.reset(
                            poSrcFeature->StealGeometry(iGeomField));
                    }

                    poDstFeature->SetGeomField(iGeomField,
                                               std::move(poDstGeom));
                }
                else
                {
                    std::unique_ptr<OGRGeometry> poSrcGeom;

                    if (iDstFeature == 0)
                    {
                        poSrcGeom.reset(
                            poSrcFeature->StealGeometry(iGeomField));
                    }
                    else
                    {
                        poSrcGeom.reset(apoOutFeatures.front()
                                            ->GetGeomFieldRef(iGeomField)
                                            ->clone());
                    }

                    poDstFeature->SetGeomField(iGeomField,
                                               std::move(poSrcGeom));
                }
            }

            poDstFeature->SetFID(m_nextFID++);
            apoOutFeatures.push_back(std::move(poDstFeature));
        }

        return true;
    }

  protected:
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                        bool bForce) override
    {
        return m_srcLayer.GetExtent3D(iGeomField, psExtent3D, bForce);
    }

  private:
    std::vector<int> m_passThroughFieldSrcToDstMap{};
    std::vector<int> m_unnestedFieldSrcToDstMap{};
    std::vector<bool> m_geomFieldExploded{};
    std::vector<std::string> m_fieldsToExplode{};
    std::vector<std::string> m_geomFieldsToExplode{};
    std::string m_indexFieldName{};
    bool m_setupError{false};
    OGRFeatureDefnRefCountedPtr m_poFeatureDefn{nullptr};
    GIntBig m_nextFID{1};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorExplodeLayer)
};

}  // namespace

/************************************************************************/
/*                GDALVectorExplodeAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorExplodeAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poOutDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    if (m_defaultGeom)
    {
        m_geomFields.emplace_back(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME);
    }

    if (m_fields.empty() && m_geomFields.empty())
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "At least one field or geometry field must be specified");
        return false;
    }

    for (OGRLayer *poSrcLayer : poSrcDS->GetLayers())
    {
        if (!poSrcLayer)
            continue;

        if (!m_activeLayer.empty() &&
            poSrcLayer->GetDescription() != m_activeLayer)
        {
            poOutDS->AddLayer(
                *poSrcLayer,
                std::make_unique<GDALVectorPipelinePassthroughLayer>(
                    *poSrcLayer));
        }

        const auto *poLayerDefn = poSrcLayer->GetLayerDefn();

        auto fieldsForLayer = m_fields;
        auto geomFieldsForLayer = m_geomFields;

        if (geomFieldsForLayer.size() == 1 && geomFieldsForLayer[0] == "ALL")
        {
            geomFieldsForLayer.clear();
            for (int iGeomField = 0;
                 iGeomField < poLayerDefn->GetGeomFieldCount(); iGeomField++)
            {
                geomFieldsForLayer.emplace_back(
                    poLayerDefn->GetGeomFieldDefn(iGeomField)->GetNameRef());
            }
        }

        if (fieldsForLayer.size() == 1 && fieldsForLayer[0] == "ALL")
        {
            fieldsForLayer.clear();
            for (int iField = 0; iField < poLayerDefn->GetFieldCount();
                 iField++)
            {
                fieldsForLayer.emplace_back(
                    poLayerDefn->GetFieldDefn(iField)->GetNameRef());
            }
        }

        auto poOutLayer = std::make_unique<GDALVectorExplodeLayer>(
            *poSrcLayer, fieldsForLayer, geomFieldsForLayer, m_indexFieldName);
        poOutDS->AddLayer(*poSrcLayer, std::move(poOutLayer));
    }

    m_outputDataset.Set(std::move(poOutDS));
    return true;
}

//! @endcond
