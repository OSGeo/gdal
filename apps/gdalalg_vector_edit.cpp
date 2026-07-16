/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "edit" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_edit.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorEditAlgorithm::GDALVectorEditAlgorithm()          */
/************************************************************************/

GDALVectorEditAlgorithm::GDALVectorEditAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddGeometryTypeArg(&m_geometryType, _("Layer geometry type"));

    AddArg("crs", 0, _("Override CRS (without reprojection)"), &m_overrideCrs)
        .AddHiddenAlias("a_srs")
        .SetIsCRSArg(/*noneAllowed=*/true);

    {
        auto &arg = AddArg("metadata", 0, _("Add/update dataset metadata item"),
                           &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }

    AddArg("unset-metadata", 0, _("Remove dataset metadata item"),
           &m_unsetMetadata)
        .SetMetaVar("<KEY>");

    {
        auto &arg =
            AddArg("layer-metadata", 0, _("Add/update layer metadata item"),
                   &m_layerMetadata)
                .SetMetaVar("<KEY>=<VALUE>")
                .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
    }

    AddArg("unset-layer-metadata", 0, _("Remove layer metadata item"),
           &m_unsetLayerMetadata)
        .SetMetaVar("<KEY>");

    AddArg("unset-fid", 0,
           _("Unset the identifier of each feature and the FID column name"),
           &m_unsetFID);
}

/************************************************************************/
/*                     GDALVectorEditAlgorithmLayer                     */
/************************************************************************/

namespace
{
class GDALVectorEditAlgorithmLayer final : public GDALVectorPipelineOutputLayer
{
  public:
    GDALVectorEditAlgorithmLayer(
        OGRLayer &oSrcLayer, const std::string &activeLayer,
        bool bChangeGeomType, OGRwkbGeometryType eType,
        const std::string &overrideCrs,
        const std::vector<std::string> &layerMetadata,
        const std::vector<std::string> &unsetLayerMetadata, bool unsetFID)
        : GDALVectorPipelineOutputLayer(oSrcLayer),
          m_bOverrideCrs(!overrideCrs.empty()), m_unsetFID(unsetFID)
    {
        SetDescription(oSrcLayer.GetDescription());
        SetMetadata(oSrcLayer.GetMetadata());

        m_poFeatureDefn = oSrcLayer.GetLayerDefn()->Clone();
        m_poFeatureDefn->Reference();

        if (activeLayer.empty() || activeLayer == GetDescription())
        {
            const CPLStringList aosMD(layerMetadata);
            for (const auto &[key, value] : cpl::IterateNameValue(aosMD))
            {
                if (SetMetadataItem(key, value) != CE_None)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "SetMetadataItem('%s', '%s') failed", key, value);
                }
            }

            for (const std::string &key : unsetLayerMetadata)
            {
                if (SetMetadataItem(key.c_str(), nullptr) != CE_None)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "SetMetadataItem('%s', NULL) failed", key.c_str());
                }
            }

            if (bChangeGeomType)
            {
                for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
                {
                    m_poFeatureDefn->GetGeomFieldDefn(i)->SetType(eType);
                }
            }

            if (!overrideCrs.empty())
            {
                if (!EQUAL(overrideCrs.c_str(), "null") &&
                    !EQUAL(overrideCrs.c_str(), "none"))
                {
                    m_poSRS = new OGRSpatialReference();
                    m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    m_poSRS->SetFromUserInput(overrideCrs.c_str());
                }
                for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
                {
                    m_poFeatureDefn->GetGeomFieldDefn(i)->SetSpatialRef(
                        m_poSRS);
                }
            }
        }
    }

    ~GDALVectorEditAlgorithmLayer() override
    {
        m_poFeatureDefn->Release();
        if (m_poSRS)
            m_poSRS->Release();
    }

    const char *GetFIDColumn() const override
    {
        if (m_unsetFID)
            return "";
        return m_srcLayer.GetFIDColumn();
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn;
    }

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn);
        if (m_bOverrideCrs)
        {
            for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
            {
                auto poGeom = poSrcFeature->GetGeomFieldRef(i);
                if (poGeom)
                    poGeom->assignSpatialReference(m_poSRS);
            }
        }
        if (m_unsetFID)
            poSrcFeature->SetFID(OGRNullFID);
        apoOutFeatures.push_back(std::move(poSrcFeature));
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCStringsAsUTF8) ||
            EQUAL(pszCap, OLCCurveGeometries) || EQUAL(pszCap, OLCZGeometries))
            return m_srcLayer.TestCapability(pszCap);
        return false;
    }

  private:
    const bool m_bOverrideCrs;
    const bool m_unsetFID;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    OGRSpatialReference *m_poSRS = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorEditAlgorithmLayer)
};

}  // namespace

/************************************************************************/
/*                  GDALVectorEditAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorEditAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();

    bool bChangeGeomType = false;
    OGRwkbGeometryType eType = wkbUnknown;
    if (!m_geometryType.empty())
    {
        eType = OGRFromOGCGeomType(m_geometryType.c_str());
        bChangeGeomType = true;
    }

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    const CPLStringList aosMD(m_metadata);
    for (const auto &[key, value] : cpl::IterateNameValue(aosMD))
    {
        if (outDS->SetMetadataItem(key, value) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "SetMetadataItem('%s', '%s') failed", key, value);
            return false;
        }
    }

    for (const std::string &key : m_unsetMetadata)
    {
        if (outDS->SetMetadataItem(key.c_str(), nullptr) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "SetMetadataItem('%s', NULL) failed", key.c_str());
            return false;
        }
    }

    bool ret = true;
    for (int i = 0; ret && i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        ret = (poSrcLayer != nullptr);
        if (ret)
        {
            outDS->AddLayer(*poSrcLayer,
                            std::make_unique<GDALVectorEditAlgorithmLayer>(
                                *poSrcLayer, m_activeLayer, bChangeGeomType,
                                eType, m_overrideCrs, m_layerMetadata,
                                m_unsetLayerMetadata, m_unsetFID));
        }
    }

    if (ret)
        m_outputDataset.Set(std::move(outDS));

    return ret;
}

GDALVectorEditAlgorithmStandalone::~GDALVectorEditAlgorithmStandalone() =
    default;

//! @endcond
