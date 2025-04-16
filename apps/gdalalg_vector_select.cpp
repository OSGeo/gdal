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
    AddActiveLayerArg(&m_activeLayer);
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
    : public GDALVectorPipelineOutputLayer
{
  private:
    OGRFeatureDefn *const m_poFeatureDefn = nullptr;
    std::vector<int> m_anMapSrcFieldsToDstFields{};
    std::vector<int> m_anMapDstGeomFieldsToSrcGeomFields{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSelectAlgorithmLayer)

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const
    {
        auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        poFeature->SetFID(poSrcFeature->GetFID());
        const auto styleString = poSrcFeature->GetStyleString();
        if (styleString)
            poFeature->SetStyleString(styleString);
        poFeature->SetFieldsFrom(poSrcFeature.get(),
                                 m_anMapSrcFieldsToDstFields.data(), false,
                                 false);
        int iDstGeomField = 0;
        for (int nSrcGeomField : m_anMapDstGeomFieldsToSrcGeomFields)
        {
            poFeature->SetGeomFieldDirectly(
                iDstGeomField, poSrcFeature->StealGeometry(nSrcGeomField));
            ++iDstGeomField;
        }
        return poFeature;
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        apoOutFeatures.push_back(TranslateFeature(std::move(poSrcFeature)));
    }

  public:
    explicit GDALVectorSelectAlgorithmLayer(OGRLayer &oSrcLayer)
        : GDALVectorPipelineOutputLayer(oSrcLayer),
          m_poFeatureDefn(new OGRFeatureDefn(oSrcLayer.GetName()))
    {
        SetDescription(oSrcLayer.GetDescription());
        SetMetadata(oSrcLayer.GetMetadata());
        m_poFeatureDefn->SetGeomType(wkbNone);
        m_poFeatureDefn->Reference();
    }

    ~GDALVectorSelectAlgorithmLayer() override
    {
        m_poFeatureDefn->Release();
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

        const auto poSrcLayerDefn = m_srcLayer.GetLayerDefn();
        for (const auto poSrcFieldDefn : poSrcLayerDefn->GetFields())
        {
            const auto oIter = oSetSelFieldsUC.find(
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

        for (const auto poSrcFieldDefn : poSrcLayerDefn->GetGeomFields())
        {
            const auto oIter = oSetSelFieldsUC.find(
                CPLString(poSrcFieldDefn->GetNameRef()).toupper());
            if (oIter != oSetSelFieldsUC.end())
            {
                m_anMapDstGeomFieldsToSrcGeomFields.push_back(
                    m_poFeatureDefn->GetGeomFieldCount());
                OGRGeomFieldDefn oDstFieldDefn(*poSrcFieldDefn);
                m_poFeatureDefn->AddGeomFieldDefn(&oDstFieldDefn);
                oSetUsedSetFieldsUC.insert(*oIter);
            }
        }

        const auto oIter = oSetSelFieldsUC.find(
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
                             osName.c_str(), m_srcLayer.GetDescription(),
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

        const auto poSrcLayerDefn = m_srcLayer.GetLayerDefn();
        for (const auto poSrcFieldDefn : poSrcLayerDefn->GetFields())
        {
            const auto oIter = oSetSelFieldsUC.find(
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
            for (const auto poSrcFieldDefn : poSrcLayerDefn->GetGeomFields())
            {
                const auto oIter = oSetSelFieldsUC.find(
                    CPLString(poSrcFieldDefn->GetNameRef()).toupper());
                if (oIter == oSetSelFieldsUC.end())
                {
                    m_anMapDstGeomFieldsToSrcGeomFields.push_back(
                        m_poFeatureDefn->GetGeomFieldCount());
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
            return m_srcLayer.GetFeatureCount(bForce);
        return OGRLayer::GetFeatureCount(bForce);
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRFeature *GetFeature(GIntBig nFID) override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_srcLayer.GetFeature(nFID));
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(std::move(poSrcFeature)).release();
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
            return m_srcLayer.TestCapability(pszCap);
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

//! @endcond
