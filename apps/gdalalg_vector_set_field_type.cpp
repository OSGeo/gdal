/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "set-field-type" step of "vector pipeline"
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_set_field_type.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                  GDALVectorSetFieldTypeAlgorithm()                   */
/************************************************************************/

GDALVectorSetFieldTypeAlgorithm::GDALVectorSetFieldTypeAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    auto &layerArg = AddActiveLayerArg(&m_activeLayer);
    auto &fieldNameArg = AddFieldNameArg(&m_fieldName)
                             .SetRequired()
                             .SetMutualExclusionGroup("name-or-type");
    SetAutoCompleteFunctionForFieldName(fieldNameArg, layerArg, m_inputDataset);
    AddFieldTypeSubtypeArg(&m_srcFieldType, &m_srcFieldSubType,
                           &m_srcFieldTypeSubTypeStr, "src-field-type",
                           _("Source field type or subtype"))
        .SetRequired()
        .SetMutualExclusionGroup("name-or-type");
    AddFieldTypeSubtypeArg(&m_newFieldType, &m_newFieldSubType,
                           &m_newFieldTypeSubTypeStr, std::string(),
                           _("Target field type or subtype"))
        .AddAlias("dst-field-type")
        .SetRequired();
    AddValidationAction(
        [this] { return m_inputDataset.empty() || GlobalValidation(); });
}

/************************************************************************/
/*                  Get_OGR_SCHEMA_OpenOption_Layer()                   */
/************************************************************************/

CPLJSONObject
GDALVectorSetFieldTypeAlgorithm::Get_OGR_SCHEMA_OpenOption_Layer() const
{
    CPLJSONObject oLayer;
    oLayer.Set("name", m_activeLayer.empty() ? "*" : m_activeLayer);
    oLayer.Set("schemaType", "Patch");
    CPLJSONArray oFields;
    CPLJSONObject oField;
    if (m_fieldName.empty())
    {
        oField.Set("srcType", OGRFieldDefn::GetFieldTypeName(m_srcFieldType));
        oField.Set("srcSubType",
                   OGRFieldDefn::GetFieldSubTypeName(m_srcFieldSubType));
    }
    else
    {
        oField.Set("name", m_fieldName);
    }
    if (!m_newFieldTypeSubTypeStr.empty())
    {
        oField.Set("type", OGRFieldDefn::GetFieldTypeName(m_newFieldType));
        oField.Set("subType",
                   OGRFieldDefn::GetFieldSubTypeName(m_newFieldSubType));
    }
    oFields.Add(oField);
    oLayer.Set("fields", oFields);
    return oLayer;
}

/************************************************************************/
/*                          GlobalValidation()                          */
/************************************************************************/

bool GDALVectorSetFieldTypeAlgorithm::GlobalValidation() const
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto layer = m_activeLayer.empty()
                     ? poSrcDS->GetLayer(0)
                     : const_cast<GDALDataset *>(poSrcDS)->GetLayerByName(
                           m_activeLayer.c_str());
    if (!layer)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find layer '%s'",
                 m_activeLayer.c_str());
        return false;
    }
    if (!m_fieldName.empty() &&
        layer->GetLayerDefn()->GetFieldIndex(m_fieldName.c_str()) < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find field '%s' in layer '%s'", m_fieldName.c_str(),
                 layer->GetName());
        return false;
    }
    return true;
}

/************************************************************************/
/*                 GDALVectorSetFieldTypeAlgorithmLayer                 */
/************************************************************************/

namespace
{
class GDALVectorSetFieldTypeAlgorithmLayer final
    : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorSetFieldTypeAlgorithmLayer(OGRLayer &oSrcLayer,
                                         const std::string &activeLayer,
                                         const std::string &fieldName,
                                         const OGRFieldType srcFieldType,
                                         const OGRFieldSubType srcFieldSubType,
                                         const OGRFieldType newFieldType,
                                         const OGRFieldSubType newFieldSubType)
        : GDALVectorPipelineOutputLayer(oSrcLayer)
    {

        m_poFeatureDefn = oSrcLayer.GetLayerDefn()->Clone();
        m_poFeatureDefn->Reference();

        if (activeLayer.empty() || activeLayer == GetDescription())
        {
            if (!fieldName.empty())
            {
                m_fieldIndex =
                    m_poFeatureDefn->GetFieldIndex(fieldName.c_str());
                if (m_fieldIndex >= 0)
                {
                    auto poFieldDefn =
                        m_poFeatureDefn->GetFieldDefn(m_fieldIndex);
                    if (poFieldDefn->GetType() != newFieldType)
                    {
                        m_passThrough = false;
                    }

                    // Set to OFSTNone to bypass the check that prevents changing the type
                    poFieldDefn->SetSubType(OFSTNone);
                    poFieldDefn->SetType(newFieldType);
                    poFieldDefn->SetSubType(newFieldSubType);
                }
            }
            else
            {
                for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
                {
                    auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
                    if (poFieldDefn->GetType() == srcFieldType &&
                        poFieldDefn->GetSubType() == srcFieldSubType)
                    {
                        m_passThrough = false;

                        // Set to OFSTNone to bypass the check that prevents changing the type
                        poFieldDefn->SetSubType(OFSTNone);
                        poFieldDefn->SetType(newFieldType);
                        poFieldDefn->SetSubType(newFieldSubType);
                    }
                }
            }

            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
            {
                m_identityMap.push_back(i);
            }
        }
    }

    ~GDALVectorSetFieldTypeAlgorithmLayer() override
    {
        m_poFeatureDefn->Release();
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn;
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        if (m_passThrough)
        {
            apoOutFeatures.push_back(std::move(poSrcFeature));
        }
        else
        {
            auto poDstFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
            const auto result{poDstFeature->SetFrom(
                poSrcFeature.get(), m_identityMap.data(), false, true)};
            if (result != OGRERR_NONE)
            {
                if (m_fieldIndex >= 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot convert field '%s' to new type, setting "
                             "it to NULL",
                             m_poFeatureDefn->GetFieldDefn(m_fieldIndex)
                                 ->GetNameRef());
                }
            }
            else
            {
                poDstFeature->SetFID(poSrcFeature->GetFID());
                apoOutFeatures.push_back(std::move(poDstFeature));
            }
        }
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCStringsAsUTF8) ||
            EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCZGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries))
            return m_srcLayer.TestCapability(pszCap);
        return false;
    }

  private:
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    int m_fieldIndex{-1};
    bool m_passThrough = true;
    std::vector<int> m_identityMap{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSetFieldTypeAlgorithmLayer)
};

}  // namespace

/************************************************************************/
/*              GDALVectorSetFieldTypeAlgorithm::RunStep()              */
/************************************************************************/

bool GDALVectorSetFieldTypeAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    if (!GlobalValidation())
        return false;

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    bool ret = true;
    for (int i = 0; ret && i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        ret = (poSrcLayer != nullptr);
        if (ret)
        {
            outDS->AddLayer(
                *poSrcLayer,
                std::make_unique<GDALVectorSetFieldTypeAlgorithmLayer>(
                    *poSrcLayer, m_activeLayer, m_fieldName, m_srcFieldType,
                    m_srcFieldSubType, m_newFieldType, m_newFieldSubType));
        }
    }

    if (ret)
        m_outputDataset.Set(std::move(outDS));

    return ret;
}

GDALVectorSetFieldTypeAlgorithmStandalone::
    ~GDALVectorSetFieldTypeAlgorithmStandalone() = default;

//! @endcond
