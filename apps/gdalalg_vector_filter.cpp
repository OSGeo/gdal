/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "filter" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_filter.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm()       */
/************************************************************************/

GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddBBOXArg(&m_bbox);
    AddArg("where", 0,
           _("Attribute query in a restricted form of the queries used in the "
             "SQL WHERE statement"),
           &m_where)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<WHERE>|@<filename>")
        .SetRemoveSQLCommentsEnabled();
    AddArg("fields", 0, _("Selected fields"), &m_selectedFields);
}

namespace
{

/************************************************************************/
/*                 GDALVectorFilterAlgorithmDataset                     */
/************************************************************************/

class GDALVectorFilterAlgorithmDataset final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};

  public:
    GDALVectorFilterAlgorithmDataset() = default;

    void AddLayer(std::unique_ptr<OGRLayer> poLayer)
    {
        m_layers.push_back(std::move(poLayer));
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return idx >= 0 && idx < GetLayerCount() ? m_layers[idx].get()
                                                 : nullptr;
    }
};

/************************************************************************/
/*                   GDALVectorFilterAlgorithmLayer                     */
/************************************************************************/

class GDALVectorFilterAlgorithmLayer final : public OGRLayer
{
  private:
    bool m_bIsOK = true;
    OGRLayer *const m_poSrcLayer;
    OGRFeatureDefn *const m_poFeatureDefn = nullptr;
    std::vector<int> m_anMapSrcFieldsToDstFields{};
    std::vector<int> m_anMapDstGeomFieldsToSrcGeomFields{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorFilterAlgorithmLayer)

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

  public:
    GDALVectorFilterAlgorithmLayer(
        OGRLayer *poSrcLayer, const std::vector<std::string> &selectedFields,
        bool bStrict)
        : m_poSrcLayer(poSrcLayer),
          m_poFeatureDefn(new OGRFeatureDefn(poSrcLayer->GetName()))
    {
        SetDescription(poSrcLayer->GetDescription());
        m_poFeatureDefn->SetGeomType(wkbNone);
        m_poFeatureDefn->Reference();

        std::set<std::string> oSetSelFields;
        std::set<std::string> oSetSelFieldsUC;
        for (const std::string &osFieldName : selectedFields)
        {
            oSetSelFields.insert(osFieldName);
            oSetSelFieldsUC.insert(CPLString(osFieldName).toupper());
        }

        std::set<std::string> oSetUsedSetFieldsUC;

        const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
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
                             osName.c_str(), poSrcLayer->GetDescription(),
                             bStrict ? "" : " It will be ignored");
                    if (bStrict)
                        m_bIsOK = false;
                }
            }
        }
    }

    ~GDALVectorFilterAlgorithmLayer() override
    {
        if (m_poFeatureDefn)
            m_poFeatureDefn->Dereference();
    }

    bool IsOK() const
    {
        return m_bIsOK;
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        return m_poSrcLayer->GetFeatureCount(bForce);
    }

    OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override
    {
        return m_poSrcLayer->GetExtent(psExtent, bForce);
    }

    OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
    {
        return m_poSrcLayer->GetExtent(iGeomField, psExtent, bForce);
    }

    void ResetReading() override
    {
        m_poSrcLayer->ResetReading();
    }

    OGRFeature *GetNextFeature() override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_poSrcLayer->GetNextFeature());
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(poSrcFeature.get()).release();
    }

    OGRFeature *GetFeature(GIntBig nFID) override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_poSrcLayer->GetFeature(nFID));
        if (!poSrcFeature)
            return nullptr;
        return TranslateFeature(poSrcFeature.get()).release();
    }

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, OLCRandomRead) || EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries) ||
            EQUAL(pszCap, OLCFastFeatureCount) ||
            EQUAL(pszCap, OLCFastGetExtent) || EQUAL(pszCap, OLCStringsAsUTF8))
        {
            return m_poSrcLayer->TestCapability(pszCap);
        }
        return false;
    }
};

}  // namespace

/************************************************************************/
/*               GDALVectorFilterAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorFilterAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    auto poSrcDS = m_inputDataset.GetDatasetRef();
    const int nLayerCount = poSrcDS->GetLayerCount();

    bool ret = true;
    if (m_bbox.size() == 4)
    {
        const double xmin = m_bbox[0];
        const double ymin = m_bbox[1];
        const double xmax = m_bbox[2];
        const double ymax = m_bbox[3];
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (poSrcLayer)
                poSrcLayer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
        }
    }

    if (ret && !m_where.empty())
    {
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (ret)
                ret = poSrcLayer->SetAttributeFilter(m_where.c_str()) ==
                      OGRERR_NONE;
        }
    }

    if (ret && !m_selectedFields.empty())
    {
        auto outDS = std::make_unique<GDALVectorFilterAlgorithmDataset>();
        outDS->SetDescription(poSrcDS->GetDescription());

        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (ret)
            {
                auto poLayer = std::make_unique<GDALVectorFilterAlgorithmLayer>(
                    poSrcLayer, m_selectedFields, /* bStrict = */ true);
                ret = poLayer->IsOK();
                if (ret)
                {
                    outDS->AddLayer(std::move(poLayer));
                }
            }
        }

        m_outputDataset.Set(std::move(outDS));
    }
    else if (ret)
    {
        m_outputDataset.Set(m_inputDataset.GetDatasetRef());
    }

    return ret;
}

//! @endcond
