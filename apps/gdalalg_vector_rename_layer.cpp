/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "rename-layer" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_rename_layer.h"

//! @cond Doxygen_Suppress

#include <map>

#include "cpl_string.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*   GDALVectorRenameLayerAlgorithm::GDALVectorRenameLayerAlgorithm()   */
/************************************************************************/

GDALVectorRenameLayerAlgorithm::GDALVectorRenameLayerAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetAddInputLayerNameArgument(false))
{
    AddLayerNameArg(&m_inputLayerName);
    if (!standaloneStep)
    {
        AddOutputLayerNameArg(/* hiddenForCLI = */ false,
                              /* shortNameOutputLayerAllowed = */ false);
    }
    AddArg("ascii", 0, _("Force names to ASCII character"), &m_ascii);
    AddArg("lower-case", 0,
           _("Force names to lower case (only on ASCII characters)"),
           &m_lowerCase);
    AddArg("filename-compatible", 0, _("Force names to be usable as filenames"),
           &m_filenameCompatible);
    AddArg("reserved-characters", 0, _("Reserved character(s) to be removed"),
           &m_reservedChars);
    AddArg("replacement-character", 0,
           _("Replacement character when ASCII conversion not possible"),
           &m_replacementChar)
        .SetMaxCharCount(1);
    AddArg("max-length", 0, _("Maximum length of layer names"), &m_maxLength)
        .SetMinValueIncluded(1);

    AddValidationAction(
        [this]()
        {
            if (!m_inputLayerName.empty() && m_outputLayerName.empty())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Argument output-layer must be specified when "
                            "input-layer is specified");
                return false;
            }

            if (!m_inputDataset.empty() && m_inputDataset[0].GetDatasetRef())
            {
                auto poSrcDS = m_inputDataset[0].GetDatasetRef();
                if (!m_inputLayerName.empty() &&
                    poSrcDS->GetLayerByName(m_inputLayerName.c_str()) ==
                        nullptr)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Input layer '%s' does not exist",
                                m_inputLayerName.c_str());
                    return false;
                }

                if (!m_outputLayerName.empty() && m_inputLayerName.empty() &&
                    poSrcDS->GetLayerCount() >= 2)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Argument input-layer must be specified when "
                                "output-layer is specified and there is more "
                                "than one layer");
                    return false;
                }
            }

            return true;
        });
}

namespace
{

/************************************************************************/
/*                 GDALVectorRenameLayerAlgorithmLayer                  */
/************************************************************************/

class GDALVectorRenameLayerAlgorithmLayer final
    : public GDALVectorPipelineOutputLayer
{
  private:
    const OGRFeatureDefnRefCountedPtr m_poFeatureDefn;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorRenameLayerAlgorithmLayer)

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override
    {
        poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn.get());
        apoOutFeatures.push_back(std::move(poSrcFeature));
    }

  public:
    explicit GDALVectorRenameLayerAlgorithmLayer(
        OGRLayer &oSrcLayer, const std::string &osOutputLayerName)
        : GDALVectorPipelineOutputLayer(oSrcLayer),
          m_poFeatureDefn(oSrcLayer.GetLayerDefn()->Clone())
    {
        m_poFeatureDefn->SetName(osOutputLayerName.c_str());
        const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        const auto poSrcLayerDefn = oSrcLayer.GetLayerDefn();
        for (int i = 0; i < nGeomFieldCount; ++i)
        {
            m_poFeatureDefn->GetGeomFieldDefn(i)->SetSpatialRef(
                poSrcLayerDefn->GetGeomFieldDefn(i)->GetSpatialRef());
        }
        SetDescription(m_poFeatureDefn->GetName());
        SetMetadata(oSrcLayer.GetMetadata());
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn.get();
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        return m_srcLayer.GetFeatureCount(bForce);
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        return m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr SetIgnoredFields(CSLConstList papszFields) override
    {
        return m_srcLayer.SetIgnoredFields(papszFields);
    }

    OGRErr SetAttributeFilter(const char *pszAttributeFilter) override
    {
        OGRLayer::SetAttributeFilter(pszAttributeFilter);
        return m_srcLayer.SetAttributeFilter(pszAttributeFilter);
    }

    OGRGeometry *GetSpatialFilter() override
    {
        return m_srcLayer.GetSpatialFilter();
    }

    OGRErr ISetSpatialFilter(int iGeomField, const OGRGeometry *poGeom) override
    {
        return m_srcLayer.SetSpatialFilter(iGeomField, poGeom);
    }

    OGRFeature *GetFeature(GIntBig nFID) override
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_srcLayer.GetFeature(nFID));
        if (!poSrcFeature)
            return nullptr;
        poSrcFeature->SetFDefnUnsafe(m_poFeatureDefn.get());
        return poSrcFeature.release();
    }

    int TestCapability(const char *pszCap) const override
    {
        return m_srcLayer.TestCapability(pszCap);
    }
};

/************************************************************************/
/*                GDALVectorRenameLayerAlgorithmDataset                 */
/************************************************************************/

class GDALVectorRenameLayerAlgorithmDataset final
    : public GDALVectorPipelineOutputDataset
{
  public:
    GDALVectorRenameLayerAlgorithmDataset(
        GDALDataset &oSrcDS, const std::vector<std::string> &aosNewLayerNames)
        : GDALVectorPipelineOutputDataset(oSrcDS)
    {
        const int nLayerCount = oSrcDS.GetLayerCount();
        CPLAssert(aosNewLayerNames.size() == static_cast<size_t>(nLayerCount));
        for (int i = 0; i < nLayerCount; ++i)
        {
            m_mapOldLayerNameToNew[oSrcDS.GetLayer(i)->GetName()] =
                aosNewLayerNames[i];
        }
    }

    const GDALRelationship *
    GetRelationship(const std::string &name) const override;

  private:
    std::map<std::string, std::string> m_mapOldLayerNameToNew{};
    mutable std::map<std::string, std::unique_ptr<GDALRelationship>>
        m_relationships{};
};

/************************************************************************/
/*                          GetRelationship()                           */
/************************************************************************/

const GDALRelationship *GDALVectorRenameLayerAlgorithmDataset::GetRelationship(
    const std::string &name) const
{
    const auto oIterRelationships = m_relationships.find(name);
    if (oIterRelationships != m_relationships.end())
        return oIterRelationships->second.get();

    const GDALRelationship *poSrcRelationShip = m_srcDS.GetRelationship(name);
    if (!poSrcRelationShip)
        return nullptr;
    const auto oIterLeftTableName =
        m_mapOldLayerNameToNew.find(poSrcRelationShip->GetLeftTableName());
    const auto oIterRightTableName =
        m_mapOldLayerNameToNew.find(poSrcRelationShip->GetRightTableName());
    const auto oIterMappingTableName =
        m_mapOldLayerNameToNew.find(poSrcRelationShip->GetMappingTableName());
    if (oIterLeftTableName == m_mapOldLayerNameToNew.end() &&
        oIterRightTableName == m_mapOldLayerNameToNew.end() &&
        oIterMappingTableName == m_mapOldLayerNameToNew.end())
    {
        return poSrcRelationShip;
    }

    auto poNewRelationship =
        std::make_unique<GDALRelationship>(*poSrcRelationShip);
    if (oIterLeftTableName != m_mapOldLayerNameToNew.end())
        poNewRelationship->SetLeftTableName(oIterLeftTableName->second);
    if (oIterRightTableName != m_mapOldLayerNameToNew.end())
        poNewRelationship->SetRightTableName(oIterRightTableName->second);
    if (oIterMappingTableName != m_mapOldLayerNameToNew.end())
        poNewRelationship->SetMappingTableName(oIterMappingTableName->second);

    return m_relationships.insert({name, std::move(poNewRelationship)})
        .first->second.get();
}

}  // namespace

/************************************************************************/
/*                       TruncateUTF8ToMaxChar()                        */
/************************************************************************/

static void TruncateUTF8ToMaxChar(std::string &osStr, size_t maxCharCount)
{
    size_t nCharacterCount = 0;
    for (size_t i = 0; i < osStr.size(); ++i)
    {
        // Is it first byte of a UTF-8 character?
        if ((osStr[i] & 0xc0) != 0x80)
        {
            ++nCharacterCount;
            if (nCharacterCount == maxCharCount)
            {
                osStr.resize(i + 1);
                break;
            }
        }
    }
}

/************************************************************************/
/*              GDALVectorRenameLayerAlgorithm::RunStep()               */
/************************************************************************/

bool GDALVectorRenameLayerAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    // First pass over layer names to create new layer names matching specified
    // constraints
    std::vector<std::string> aosNames;
    std::map<std::string, int> oMapCountNames;
    bool bNonUniqueNames = false;
    const int nLayerCount = poSrcDS->GetLayerCount();
    for (int i = 0; i < nLayerCount; ++i)
    {
        const OGRLayer *poSrcLayer = poSrcDS->GetLayer(i);
        if ((m_inputLayerName == poSrcLayer->GetDescription() ||
             nLayerCount == 1) &&
            !m_outputLayerName.empty())
        {
            aosNames.push_back(m_outputLayerName);
        }
        else
        {
            std::string osName(poSrcLayer->GetDescription());
            if (!m_reservedChars.empty())
            {
                std::string osNewName;
                for (char c : osName)
                {
                    if (m_reservedChars.find(c) != std::string::npos)
                    {
                        if (!m_replacementChar.empty())
                            osNewName += m_replacementChar;
                    }
                    else
                    {
                        osNewName += c;
                    }
                }
                osName = std::move(osNewName);
            }
            if (m_filenameCompatible)
            {
                osName = CPLLaunderForFilenameSafe(
                    osName, m_replacementChar.c_str()[0]);
            }
            if (m_ascii)
            {
                char *pszStr = CPLUTF8ForceToASCII(
                    osName.c_str(), m_replacementChar.c_str()[0]);
                osName = pszStr;
                CPLFree(pszStr);
            }
            if (m_lowerCase)
            {
                for (char &c : osName)
                {
                    if (c >= 'A' && c <= 'Z')
                        c = c - 'A' + 'a';
                }
            }
            if (m_maxLength > 0)
            {
                TruncateUTF8ToMaxChar(osName, m_maxLength);
            }
            if (++oMapCountNames[osName] > 1)
                bNonUniqueNames = true;
            aosNames.push_back(std::move(osName));
        }
    }

    // Extra optional pass if some names are not unique
    if (bNonUniqueNames)
    {
        std::map<std::string, int> oMapCurCounter;
        bool bUniquenessPossible = true;
        for (auto &osName : aosNames)
        {
            const int nCountForName = oMapCountNames[osName];
            if (nCountForName > 1)
            {
                const int nCounter = ++oMapCurCounter[osName];
                std::string osSuffix("_");
                if (nCountForName <= 9)
                    osSuffix += CPLSPrintf("%d", nCounter);
                else if (nCountForName <= 99)
                    osSuffix += CPLSPrintf("%02d", nCounter);
                else
                    osSuffix += CPLSPrintf("%03d", nCounter);
                const size_t nNameLen = CPLStrlenUTF8Ex(osName.c_str());
                if (m_maxLength > 0 && nNameLen + osSuffix.size() >
                                           static_cast<size_t>(m_maxLength))
                {
                    if (nNameLen > osSuffix.size())
                    {
                        TruncateUTF8ToMaxChar(osName,
                                              nNameLen - osSuffix.size());
                        osName += osSuffix;
                    }
                    else if (bUniquenessPossible)
                    {
                        ReportError(CE_Warning, CPLE_AppDefined,
                                    "Cannot create unique name for '%s' while "
                                    "respecting %d maximum length",
                                    osName.c_str(), m_maxLength);
                        bUniquenessPossible = false;
                    }
                }
                else
                {
                    osName += osSuffix;
                }
            }
        }
    }

    auto outDS = std::make_unique<GDALVectorRenameLayerAlgorithmDataset>(
        *poSrcDS, aosNames);

    // Final pass to create output layers
    for (int i = 0; i < nLayerCount; ++i)
    {
        OGRLayer *poSrcLayer = poSrcDS->GetLayer(i);
        if (poSrcLayer->GetDescription() != aosNames[i])
        {
            auto poLayer =
                std::make_unique<GDALVectorRenameLayerAlgorithmLayer>(
                    *poSrcLayer, aosNames[i]);
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

GDALVectorRenameLayerAlgorithmStandalone::
    ~GDALVectorRenameLayerAlgorithmStandalone() = default;

//! @endcond
