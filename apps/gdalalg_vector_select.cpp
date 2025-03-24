/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "select" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_select.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALVectorSelectAlgorithm::GDALVectorSelectAlgorithm()       */
/************************************************************************/

GDALVectorSelectAlgorithm::GDALVectorSelectAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("fields", 0, _("Fields to select (or exclude if --exclude)"),
           &m_fields)
        .SetPositional()
        .SetRequired();
    AddArg("exclude", 0, _("Exclude specified fields"), &m_exclude)
        .SetMutualExclusionGroup("exclude-ignore");
    AddArg("ignore-missing-fields", 0, _("Ignore missing fields"),
           &m_ignoreMissingFields)
        .SetMutualExclusionGroup("exclude-ignore");
}

namespace
{

/************************************************************************/
/*                   GDALVectorSelectAlgorithmLayer                     */
/************************************************************************/

class GDALVectorSelectAlgorithmLayer final
    : public OGRLayerWithTranslateFeature,
      public OGRGetNextFeatureThroughRaw<GDALVectorSelectAlgorithmLayer>
{
  private:
    OGRLayer &m_oSrcLayer;
    OGRFeatureDefn *const m_poFeatureDefn = nullptr;
    std::vector<int> m_anMapSrcFieldsToDstFields{};
    std::vector<int> m_anMapDstGeomFieldsToSrcGeomFields{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSelectAlgorithmLayer)

    std::unique_ptr<OGRFeature> TranslateFeature(OGRFeature *poSrcFeature) const
    {
        auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        poFeature->SetFID(poSrcFeature->GetFID());
        const auto styleString = poSrcFeature->GetStyleString();
        if (styleString)
            poFeature->SetStyleString(styleString);
        poFeature->SetFieldsFrom(
            poSrcFeature, m_anMapSrcFieldsToDstFields.data(), false, false);
        int iDstGeomField = 0;
        for (int nSrcGeomField : m_anMapDstGeomFieldsToSrcGeomFields)
        {
            poFeature->SetGeomFieldDirectly(
                iDstGeomField, poSrcFeature->StealGeometry(nSrcGeomField));
            ++iDstGeomField;
        }
        return poFeature;
    }

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(GDALVectorSelectAlgorithmLayer)

  public:
    explicit GDALVectorSelectAlgorithmLayer(OGRLayer &oSrcLayer)
        : m_oSrcLayer(oSrcLayer),
          m_poFeatureDefn(new OGRFeatureDefn(oSrcLayer.GetName()))
    {
        SetDescription(oSrcLayer.GetDescription());
        m_poFeatureDefn->SetGeomType(wkbNone);
        m_poFeatureDefn->Reference();
    }

    ~GDALVectorSelectAlgorithmLayer() override
    {
        if (m_poFeatureDefn)
            m_poFeatureDefn->Dereference();
    }

    bool IncludeFields(const std::vector<std::string> &selectedFields,
                       bool bStrict)
    {
        std::set<std::string> oSetSelFields;
        std::set<std::string> oSetSelFieldsUC;
        for (const std::string &osFieldName : selectedFields)
        {
            oSetSelFields.insert(osFieldName);
            oSetSelFieldsUC.insert(CPLString(osFieldName).toupper());
        }

        std::set<std::string> oSetUsedSetFieldsUC;

        const auto poSrcLayerDefn = m_oSrcLayer.GetLayerDefn();
        for (int i = 0; i < poSrcLayerDefn->GetFieldCount(); ++i)
        {
            const auto poSrcFieldDefn = poSrcLayerDefn->GetFieldDefn(i);
            auto oIter = oSetSelFieldsUC.find(
                CPLString(poSrcFieldDefn->GetNameRef()).toupper());
            if (oIter != oSetSelFieldsUC.end())
            {
                m_anMapSrcFieldsToDstFields.push_back(
                    m_poFeatureDefn->GetFieldCount());
                OGRFieldDefn oDstFieldDefn(*poSrcFieldDefn);
                m_poFeatureDefn->AddFieldDefn(&oDstFieldDefn);
                oSetUsedSetFieldsUC.insert(*oIter);
            }
            else
            {
                m_anMapSrcFieldsToDstFields.push_back(-1);
            }
        }

        for (int i = 0; i < poSrcLayerDefn->GetGeomFieldCount(); ++i)
        {
            const auto poSrcFieldDefn = poSrcLayerDefn->GetGeomFieldDefn(i);
            auto oIter = oSetSelFieldsUC.find(
                CPLString(poSrcFieldDefn->GetNameRef()).toupper());
            if (oIter != oSetSelFieldsUC.end())
            {
                m_anMapDstGeomFieldsToSrcGeomFields.push_back(i);
                OGRGeomFieldDefn oDstFieldDefn(*poSrcFieldDefn);
                m_poFeatureDefn->AddGeomFieldDefn(&oDstFieldDefn);
                oSetUsedSetFieldsUC.insert(*oIter);
            }
        }

        auto oIter = oSetSelFieldsUC.find(
            CPLString(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME).toupper());
        if (m_poFeatureDefn->GetGeomFieldCount() == 0 &&
            oIter != oSetSelFieldsUC.end() &&
            poSrcLayerDefn->GetGeomFieldCount() == 1)
        {
            const auto poSrcFieldDefn = poSrcLayerDefn->GetGeomFieldDefn(0);
            m_anMapDstGeomFieldsToSrcGeomFields.push_back(0);
            OGRGeomFieldDefn oDstFieldDefn(*poSrcFieldDefn);
            m_poFeatureDefn->AddGeomFieldDefn(&oDstFieldDefn);
            oSetUsedSetFieldsUC.insert(*oIter);
        }

        if (oSetUsedSetFieldsUC.size() != oSetSelFields.size())
        {
            for (const std::string &osName : oSetSelFields)
            {
                if (!cpl::contains(oSetUsedSetFieldsUC,
                                   CPLString(osName).toupper()))
                {
                    CPLError(bStrict ? CE_Failure : CE_Warning, CPLE_AppDefined,
                             "Field '%s' does not exist in layer '%s'.%s",
                             osName.c_str(), m_oSrcLayer.GetDescription(),
                             bStrict ? " You may specify "
                                       "--ignore-missing-fields to skip it"
                                     : " It will be ignored");
                    if (bStrict)
                        return false;
                }
            }
        }

        return true;
    }

    void ExcludeFields(const std::vector<std::string> &fields)
    {
        std::set<std::string> oSetSelFields;
        std::set<std::string> oSetSelFieldsUC;
        for (const std::string &osFieldName : fields)
        {
            oSetSelFields.insert(osFieldName);
            oSetSelFieldsUC.insert(CPLString(osFieldName).toupper());
        }

        const auto poSrcLayerDefn = m_oSrcLayer.GetLayerDefn();
        for (int i = 0; i < poSrcLayerDefn->GetFieldCount(); ++i)
        {
            const auto poSrcFieldDefn = poSrcLayerDefn->GetFieldDefn(i);
            auto oIter = oSetSelFieldsUC.find(
                CPLString(poSrcFieldDefn->GetNameRef()).toupper());
            if (oIter != oSetSelFieldsUC.end())
            {
                m_anMapSrcFieldsToDstFields.push_back(-1);
            }
            else
            {
                m_anMapSrcFieldsToDstFields.push_back(
                    m_poFeatureDefn->GetFieldCount());
                OGRFieldDefn oDstFieldDefn(*poSrcFieldDefn);
                m_poFeatureDefn->AddFieldDefn(&oDstFieldDefn);
            }
        }

        if (oSetSelFieldsUC.find(
                CPLString(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME).toupper()) !=
                oSetSelFieldsUC.end() &&
            poSrcLayerDefn->GetGeomFieldCount() == 1)
        {
            // exclude default geometry field
        }
        else
        {
            for (int i = 0; i < poSrcLayerDefn->GetGeomFieldCount(); ++i)
            {
                const auto poSrcFieldDefn = poSrcLayerDefn->GetGeomFieldDefn(i);
                auto oIter = oSetSelFieldsUC.find(
                    CPLString(poSrcFieldDefn->GetNameRef()).toupper());
                if (oIter == oSetSelFieldsUC.end())
                {
                    m_anMapDstGeomFieldsToSrcGeomFields.push_back(i);
                    OGRGeomFieldDefn oDstFieldDefn(*poSrcFieldDefn);
                    m_poFeatureDefn->AddGeomFieldDefn(&oDstFieldDefn);
                }
            }
        }
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (!m_poAttrQuery && !m_poFilterGeom)
            return m_oSrcLayer.GetFeatureCount(bForce);
        return OGRLayer::GetFeatureCount(bForce);
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_oSrcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    void ResetReading() override
    {
        m_oSrcLayer.ResetReading();
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        apoOutFeatures.push_back(TranslateFeature(poSrcFeature.release()));
    }

    OGRFeature *GetNextRawFeature()
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_oSrcLayer.GetNextFeature());
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(poSrcFeature.get()).release();
    }

    OGRFeature *GetFeature(GIntBig nFID) override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_oSrcLayer.GetFeature(nFID));
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(poSrcFeature.get()).release();
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCRandomRead) || EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries) ||
            (EQUAL(pszCap, OLCFastFeatureCount) && !m_poAttrQuery &&
             !m_poFilterGeom) ||
            EQUAL(pszCap, OLCFastGetExtent) || EQUAL(pszCap, OLCStringsAsUTF8))
        {
            return m_oSrcLayer.TestCapability(pszCap);
        }
        return false;
    }
};

}  // namespace

/************************************************************************/
/*               GDALVectorSelectAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorSelectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    auto poSrcDS = m_inputDataset.GetDatasetRef();

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        auto poLayer =
            std::make_unique<GDALVectorSelectAlgorithmLayer>(*poSrcLayer);
        if (m_exclude)
        {
            poLayer->ExcludeFields(m_fields);
        }
        else
        {
            if (!poLayer->IncludeFields(m_fields, !m_ignoreMissingFields))
                return false;
        }
        outDS->AddLayer(*poSrcLayer, std::move(poLayer));
    }

    m_outputDataset.Set(std::move(outDS));

    return true;
}

//! @endcond
