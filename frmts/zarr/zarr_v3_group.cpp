/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr.h"
#include "zarr_v3_codec.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                        ZarrV3Group::Create()                         */
/************************************************************************/

std::shared_ptr<ZarrV3Group>
ZarrV3Group::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName,
                    const std::string &osRootDirectoryName)
{
    auto poGroup = std::shared_ptr<ZarrV3Group>(new ZarrV3Group(
        poSharedResource, osParentName, osName, osRootDirectoryName));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                           OpenZarrArray()                            */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrV3Group::OpenZarrArray(const std::string &osName,
                                                      CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
        return oIter->second;

    if (m_bReadFromConsolidatedMetadata)
        return nullptr;

    const std::string osSubDir =
        CPLFormFilenameSafe(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    const std::string osZarrayFilename =
        CPLFormFilenameSafe(osSubDir.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osZarrayFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osZarrayFilename))
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        return LoadArray(osName, osZarrayFilename, oRoot);
    }

    return nullptr;
}

/************************************************************************/
/*                    ZarrV3Group::LoadAttributes()                     */
/************************************************************************/

void ZarrV3Group::LoadAttributes() const
{
    if (m_bAttributesLoaded)
        return;
    m_bAttributesLoaded = true;

    const std::string osFilename =
        CPLFormFilenameSafe(m_osDirectoryName.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osFilename))
            return;
        auto oRoot = oDoc.GetRoot();
        m_oAttrGroup.Init(oRoot["attributes"], m_bUpdatable);
    }
}

/************************************************************************/
/*                          ExploreDirectory()                          */
/************************************************************************/

void ZarrV3Group::ExploreDirectory() const
{
    if (m_bDirectoryExplored)
        return;
    m_bDirectoryExplored = true;

    auto psDir = VSIOpenDir(m_osDirectoryName.c_str(), 0, nullptr);
    if (!psDir)
        return;
    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
    {
        if (VSI_ISDIR(psEntry->nMode))
        {
            std::string osName(psEntry->pszName);
            while (!osName.empty() &&
                   (osName.back() == '/' || osName.back() == '\\'))
                osName.pop_back();
            if (osName.empty())
                continue;
            const std::string osSubDir = CPLFormFilenameSafe(
                m_osDirectoryName.c_str(), osName.c_str(), nullptr);
            VSIStatBufL sStat;
            const std::string osZarrJsonFilename =
                CPLFormFilenameSafe(osSubDir.c_str(), "zarr.json", nullptr);
            if (VSIStatL(osZarrJsonFilename.c_str(), &sStat) == 0)
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(osZarrJsonFilename.c_str()))
                {
                    const auto oRoot = oDoc.GetRoot();
                    if (oRoot.GetInteger("zarr_format") != 3)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled zarr_format value");
                        continue;
                    }
                    const std::string osNodeType = oRoot.GetString("node_type");
                    if (osNodeType == "array")
                    {
                        if (!cpl::contains(m_oSetArrayNames, osName))
                        {
                            m_oSetArrayNames.insert(osName);
                            m_aosArrays.emplace_back(std::move(osName));
                        }
                    }
                    else if (osNodeType == "group")
                    {
                        if (!cpl::contains(m_oSetGroupNames, osName))
                        {
                            m_oSetGroupNames.insert(osName);
                            m_aosGroups.emplace_back(std::move(osName));
                        }
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled node_type value");
                        continue;
                    }
                }
            }
            else
            {
                // Implicit group (deprecated)
                if (!cpl::contains(m_oSetGroupNames, osName))
                {
                    m_oSetGroupNames.insert(osName);
                    m_aosGroups.emplace_back(std::move(osName));
                }
            }
        }
    }
    VSICloseDir(psDir);
}

/************************************************************************/
/*                      ZarrV3Group::ZarrV3Group()                      */
/************************************************************************/

ZarrV3Group::ZarrV3Group(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::string &osDirectoryName)
    : ZarrGroupBase(poSharedResource, osParentName, osName)
{
    m_osDirectoryName = osDirectoryName;
}

/************************************************************************/
/*                     ZarrV3Group::~ZarrV3Group()                      */
/************************************************************************/

ZarrV3Group::~ZarrV3Group()
{
    ZarrV3Group::Close();
}

/************************************************************************/
/*                    GenerateMultiscalesMetadata()                     */
/************************************************************************/

void ZarrV3Group::GenerateMultiscalesMetadata(const char *pszResampling)
{
    const auto aosGroupNames = GetGroupNames();
    if (aosGroupNames.empty())
    {
        // No child groups - remove stale multiscales metadata if present.
        if (!m_bAttributesLoaded)
            LoadAttributes();
        if (m_oAttrGroup.GetAttribute("multiscales"))
            m_oAttrGroup.DeleteAttribute("multiscales");
        auto poExistingConv = m_oAttrGroup.GetAttribute("zarr_conventions");
        if (poExistingConv)
        {
            // Preserve non-multiscales entries.
            const char *pszExisting = poExistingConv->ReadAsString();
            CPLJSONArray oFiltered;
            if (pszExisting)
            {
                CPLJSONDocument oDoc;
                if (oDoc.LoadMemory(pszExisting))
                {
                    for (const auto &oEntry : oDoc.GetRoot().ToArray())
                    {
                        if (oEntry.GetString("uuid") != ZARR_MULTISCALES_UUID)
                            oFiltered.Add(oEntry);
                    }
                }
            }
            m_oAttrGroup.DeleteAttribute("zarr_conventions");
            if (oFiltered.Size() > 0)
            {
                const auto oJsonDT =
                    GDALExtendedDataType::CreateString(0, GEDTST_JSON);
                auto poAttr = m_oAttrGroup.CreateAttribute("zarr_conventions",
                                                           {}, oJsonDT);
                if (poAttr)
                    poAttr->Write(
                        oFiltered.Format(CPLJSONObject::PrettyFormat::Plain)
                            .c_str());
            }
        }
        return;
    }

    // Collect {arrayName -> [(groupName, array)]} across child groups.
    struct LevelInfo
    {
        std::string osGroupName;  // empty for base (this group)
        std::shared_ptr<GDALMDArray> poArray;
    };

    std::map<std::string, std::vector<LevelInfo>> oMapArrayToLevels;

    for (const auto &osGroupName : aosGroupNames)
    {
        auto poChildGroup = OpenZarrGroup(osGroupName);
        if (!poChildGroup)
            continue;
        for (const auto &osArrayName : poChildGroup->GetMDArrayNames())
        {
            auto poArray = poChildGroup->OpenMDArray(osArrayName);
            if (poArray)
            {
                oMapArrayToLevels[osArrayName].push_back(
                    {osGroupName, std::move(poArray)});
            }
        }
    }

    if (oMapArrayToLevels.empty())
        return;

    // For each array found in child groups, check if the base (this group)
    // also has an array with the same name. If so, prepend it as the base
    // level with an empty group name (meaning "this group").
    for (auto &[osArrayName, aoLevels] : oMapArrayToLevels)
    {
        auto poBaseArray = OpenMDArray(osArrayName);
        if (poBaseArray)
        {
            aoLevels.insert(aoLevels.begin(),
                            LevelInfo{"", std::move(poBaseArray)});
        }
    }

    // Pick the first array name (alphabetical) with >= 2 levels
    // (base + at least one overview) and >= 2 dimensions (skip 1D
    // coordinate arrays).
    //
    // Expected hierarchy from BuildOverviews():
    //   /group/
    //     data        <- base array (e.g. 10980 x 10980)
    //     y, x        <- 1D coordinate arrays (skipped)
    //     ovr_2x/
    //       data      <- 2x overview  (5490 x 5490)
    //       y, x
    //     ovr_4x/
    //       data      <- 4x overview  (2745 x 2745)
    //       y, x
    //
    // Multiple >=2D arrays sharing the same name across levels is
    // possible but unusual; we use the first alphabetically.
    std::string osCanonicalArrayName;
    for (const auto &[osArrayName, aoLevels] : oMapArrayToLevels)
    {
        if (aoLevels.size() >= 2 &&
            aoLevels[0].poArray->GetDimensionCount() >= 2)
        {
            osCanonicalArrayName = osArrayName;
            break;
        }
    }

    if (osCanonicalArrayName.empty())
    {
        CPLDebug("ZARR", "GenerateMultiscalesMetadata: no array with "
                         ">=2 levels and >=2 dimensions found");
        return;
    }

    auto &aoLevels = oMapArrayToLevels[osCanonicalArrayName];

    // Sort by total element count, largest first (= full resolution).
    std::stable_sort(aoLevels.begin(), aoLevels.end(),
                     [](const LevelInfo &a, const LevelInfo &b)
                     {
                         const auto &dimsA = a.poArray->GetDimensions();
                         const auto &dimsB = b.poArray->GetDimensions();
                         GUInt64 sizeA = 1, sizeB = 1;
                         for (const auto &d : dimsA)
                             sizeA *= d->GetSize();
                         for (const auto &d : dimsB)
                             sizeB *= d->GetSize();
                         return sizeA > sizeB;
                     });

    const auto &poBaseArray = aoLevels[0].poArray;
    const size_t nBaseDimCount = poBaseArray->GetDimensionCount();
    const auto &oBaseType = poBaseArray->GetDataType();

    // Asset path for a level. Empty group name means the base array lives
    // in this group - use the array name directly (LoadOverviews resolves
    // single-component paths as array names in the parent group).
    const auto assetPath =
        [&osCanonicalArrayName](const std::string &osGroupName) -> std::string
    { return osGroupName.empty() ? osCanonicalArrayName : osGroupName; };

    // Base level: identity scale, no translation, no derived_from.
    CPLJSONArray oLayout;
    {
        CPLJSONObject oBaseItem;
        oBaseItem.Add("asset", assetPath(aoLevels[0].osGroupName));

        CPLJSONArray oScale;
        for (size_t iDim = 0; iDim < nBaseDimCount; ++iDim)
            oScale.Add(1.0);
        CPLJSONObject oTransform;
        oTransform.Add("scale", oScale);
        oBaseItem.Add("transform", oTransform);

        oLayout.Add(oBaseItem);
    }

    // Overview levels: sequential derived_from chain.
    for (size_t iLevel = 1; iLevel < aoLevels.size(); ++iLevel)
    {
        const auto &info = aoLevels[iLevel];
        const auto &poArray = info.poArray;

        if (poArray->GetDimensionCount() != nBaseDimCount ||
            poArray->GetDataType() != oBaseType)
        {
            CPLDebug("ZARR",
                     "GenerateMultiscalesMetadata: skipping level '%s' "
                     "(dim count or data type mismatch with base)",
                     info.osGroupName.c_str());
            continue;
        }

        const auto &apoDims = poArray->GetDimensions();
        // Previous valid level for sequential derived_from.
        const auto &oPrevDims = aoLevels[iLevel - 1].poArray->GetDimensions();

        CPLJSONObject oItem;
        oItem.Add("asset", assetPath(info.osGroupName));
        oItem.Add("derived_from", assetPath(aoLevels[iLevel - 1].osGroupName));

        CPLJSONArray oScale;
        CPLJSONArray oTranslation;
        for (size_t iDim = 0; iDim < nBaseDimCount; ++iDim)
        {
            const auto nOvSize = apoDims[iDim]->GetSize();
            const auto nPrevSize = oPrevDims[iDim]->GetSize();
            const double dfScale = nOvSize > 0
                                       ? static_cast<double>(nPrevSize) /
                                             static_cast<double>(nOvSize)
                                       : 0.0;
            oScale.Add(dfScale);
            oTranslation.Add(0.0);
        }

        CPLJSONObject oTransform;
        oTransform.Add("scale", oScale);
        oTransform.Add("translation", oTranslation);
        oItem.Add("transform", oTransform);

        if (pszResampling)
            oItem.Add("resampling_method", pszResampling);

        oLayout.Add(oItem);
    }

    if (oLayout.Size() < 2)
        return;

    CPLJSONObject oMultiscales;
    oMultiscales.Add("layout", oLayout);

    // Preserve existing zarr_conventions entries.
    if (!m_bAttributesLoaded)
        LoadAttributes();

    CPLJSONArray oZarrConventions;
    auto poExistingConv = m_oAttrGroup.GetAttribute("zarr_conventions");
    if (poExistingConv)
    {
        const char *pszExisting = poExistingConv->ReadAsString();
        if (pszExisting)
        {
            CPLJSONDocument oDoc;
            if (oDoc.LoadMemory(pszExisting))
            {
                for (const auto &oEntry : oDoc.GetRoot().ToArray())
                {
                    if (oEntry.GetString("uuid") != ZARR_MULTISCALES_UUID)
                        oZarrConventions.Add(oEntry);
                }
            }
        }
        m_oAttrGroup.DeleteAttribute("zarr_conventions");
    }

    {
        CPLJSONObject oConv;
        oConv.Set("uuid", ZARR_MULTISCALES_UUID);
        oConv.Set("schema_url",
                  "https://raw.githubusercontent.com/zarr-conventions/"
                  "multiscales/refs/tags/v1/schema.json");
        oConv.Set("spec_url", "https://github.com/zarr-conventions/"
                              "multiscales/blob/v1/README.md");
        oConv.Set("name", "multiscales");
        oConv.Set("description", "Multiscale layout of zarr datasets");
        oZarrConventions.Add(oConv);
    }

    if (m_oAttrGroup.GetAttribute("multiscales"))
        m_oAttrGroup.DeleteAttribute("multiscales");

    const auto oJsonDT = GDALExtendedDataType::CreateString(0, GEDTST_JSON);
    {
        auto poAttr =
            m_oAttrGroup.CreateAttribute("zarr_conventions", {}, oJsonDT);
        if (poAttr)
            poAttr->Write(
                oZarrConventions.Format(CPLJSONObject::PrettyFormat::Plain)
                    .c_str());
    }
    {
        auto poAttr = m_oAttrGroup.CreateAttribute("multiscales", {}, oJsonDT);
        if (poAttr)
            poAttr->Write(
                oMultiscales.Format(CPLJSONObject::PrettyFormat::Plain)
                    .c_str());
    }
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

bool ZarrV3Group::Close()
{
    bool bRet = ZarrGroupBase::Close();

    if (m_bValid && (m_oAttrGroup.IsModified() ||
                     (m_bUpdatable && !m_bFileHasBeenWritten &&
                      m_poSharedResource->IsConsolidatedMetadataEnabled())))
    {
        CPLJSONDocument oDoc;
        auto oRoot = oDoc.GetRoot();
        oRoot.Add("zarr_format", 3);
        oRoot.Add("node_type", "group");
        oRoot.Add("attributes", m_oAttrGroup.Serialize());
        const std::string osZarrJsonFilename = CPLFormFilenameSafe(
            m_osDirectoryName.c_str(), "zarr.json", nullptr);
        if (!m_bFileHasBeenWritten)
        {
            oRoot.Add("consolidated_metadata",
                      m_poSharedResource->GetConsolidatedMetadataObj());
            bRet = oDoc.Save(osZarrJsonFilename) && bRet;
        }
        else
        {
            bRet = oDoc.Save(osZarrJsonFilename) && bRet;
            if (bRet)
                m_poSharedResource->SetZMetadataItem(osZarrJsonFilename, oRoot);
        }
        m_bFileHasBeenWritten = bRet;
    }

    return bRet;
}

/************************************************************************/
/*                  ZarrV3Group::GetOrCreateSubGroup()                  */
/************************************************************************/

std::shared_ptr<ZarrV3Group>
ZarrV3Group::GetOrCreateSubGroup(const std::string &osSubGroupFullname)
{
    auto poSubGroup = std::dynamic_pointer_cast<ZarrV3Group>(
        OpenGroupFromFullname(osSubGroupFullname));
    if (poSubGroup)
    {
        return poSubGroup;
    }

    const auto nLastSlashPos = osSubGroupFullname.rfind('/');
    auto poBelongingGroup =
        (nLastSlashPos == 0)
            ? this
            : GetOrCreateSubGroup(osSubGroupFullname.substr(0, nLastSlashPos))
                  .get();

    poSubGroup = ZarrV3Group::Create(
        m_poSharedResource, poBelongingGroup->GetFullName(),
        osSubGroupFullname.substr(nLastSlashPos + 1), m_osDirectoryName);
    poSubGroup->m_poParent = std::dynamic_pointer_cast<ZarrGroupBase>(
        poBelongingGroup->m_pSelf.lock());
    poSubGroup->SetDirectoryName(
        CPLFormFilenameSafe(poBelongingGroup->m_osDirectoryName.c_str(),
                            poSubGroup->GetName().c_str(), nullptr));
    poSubGroup->m_bDirectoryExplored = true;
    poSubGroup->m_bAttributesLoaded = true;
    poSubGroup->m_bReadFromConsolidatedMetadata = true;
    poSubGroup->m_bFileHasBeenWritten = true;
    poSubGroup->SetUpdatable(m_bUpdatable);

    poBelongingGroup->m_oMapGroups[poSubGroup->GetName()] = poSubGroup;
    poBelongingGroup->m_oSetGroupNames.insert(poSubGroup->GetName());
    poBelongingGroup->m_aosGroups.emplace_back(poSubGroup->GetName());
    return poSubGroup;
}

/************************************************************************/
/*             ZarrV3Group::InitFromConsolidatedMetadata()              */
/************************************************************************/

void ZarrV3Group::InitFromConsolidatedMetadata(
    const CPLJSONObject &oConsolidatedMetadata,
    const CPLJSONObject &oRootAttributes)
{
    const auto metadata = oConsolidatedMetadata["metadata"];
    if (metadata.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "consolidated_metadata lacks 'metadata' object");
        return;
    }
    m_bDirectoryExplored = true;
    m_bAttributesLoaded = true;
    m_bReadFromConsolidatedMetadata = true;

    if (oRootAttributes.IsValid())
    {
        m_oAttrGroup.Init(oRootAttributes, m_bUpdatable);
    }

    const auto children = metadata.GetChildren();
    std::map<std::string, const CPLJSONObject *> oMapArrays;

    // First pass to create groups and collect arrays
    for (const auto &child : children)
    {
        const std::string osName(child.GetName());
        if (std::count(osName.begin(), osName.end(), '/') > 32)
        {
            // Avoid too deep recursion in GetOrCreateSubGroup()
            continue;
        }

        const std::string osNodeType = child.GetString("node_type");
        if (osNodeType == "group")
        {
            auto poGroup = GetOrCreateSubGroup("/" + osName);
            auto oAttributes = child["attributes"];
            if (oAttributes.IsValid())
            {
                poGroup->m_oAttrGroup.Init(oAttributes, m_bUpdatable);
            }
        }
        else if (osNodeType == "array")
        {
            oMapArrays[osName] = &child;
        }
    }

    const auto CreateArray =
        [this](const std::string &osArrayFullname, const CPLJSONObject &oArray)
    {
        const auto nLastSlashPos = osArrayFullname.rfind('/');
        auto poBelongingGroup =
            (nLastSlashPos == std::string::npos)
                ? this
                : GetOrCreateSubGroup("/" +
                                      osArrayFullname.substr(0, nLastSlashPos))
                      .get();
        const auto osArrayName =
            nLastSlashPos == std::string::npos
                ? osArrayFullname
                : osArrayFullname.substr(nLastSlashPos + 1);
        const std::string osZarrayFilename = CPLFormFilenameSafe(
            CPLFormFilenameSafe(poBelongingGroup->m_osDirectoryName.c_str(),
                                osArrayName.c_str(), nullptr)
                .c_str(),
            "zarr.json", nullptr);
        poBelongingGroup->LoadArray(osArrayName, osZarrayFilename, oArray);
    };

    struct ArrayDesc
    {
        std::string osArrayFullname{};
        const CPLJSONObject *poArray = nullptr;
    };

    std::vector<ArrayDesc> aoRegularArrays;

    // Second pass to read attributes and create arrays that are indexing
    // variable
    for (const auto &child : children)
    {
        const std::string osName(child.GetName());
        const std::string osNodeType = child.GetString("node_type");
        if (osNodeType == "array")
        {
            auto oIter = oMapArrays.find(osName);
            if (oIter != oMapArrays.end())
            {
                const auto nLastSlashPos = osName.rfind('/');
                const std::string osArrayName =
                    (nLastSlashPos == std::string::npos)
                        ? osName
                        : osName.substr(nLastSlashPos + 1);
                const auto arrayDimensions = child["dimension_names"].ToArray();
                if (arrayDimensions.IsValid() && arrayDimensions.Size() == 1 &&
                    arrayDimensions[0].ToString() == osArrayName)
                {
                    CreateArray(osName, child);
                    oMapArrays.erase(oIter);
                }
                else
                {
                    ArrayDesc desc;
                    desc.osArrayFullname = std::move(osName);
                    desc.poArray = oIter->second;
                    aoRegularArrays.emplace_back(std::move(desc));
                }
            }
        }
    }

    // Third pass to create non-indexing arrays with attributes
    for (const auto &desc : aoRegularArrays)
    {
        CreateArray(desc.osArrayFullname, *(desc.poArray));
        oMapArrays.erase(desc.osArrayFullname);
    }

    // Fourth pass to create arrays without attributes
    for (const auto &kv : oMapArrays)
    {
        CreateArray(kv.first, *(kv.second));
    }
}

/************************************************************************/
/*                           OpenZarrGroup()                            */
/************************************************************************/

std::shared_ptr<ZarrGroupBase>
ZarrV3Group::OpenZarrGroup(const std::string &osName, CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
        return oIter->second;

    if (m_bReadFromConsolidatedMetadata)
        return nullptr;

    const std::string osSubDir =
        CPLFormFilenameSafe(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    const std::string osSubDirZarrJsonFilename =
        CPLFormFilenameSafe(osSubDir.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    // Explicit group
    if (VSIStatL(osSubDirZarrJsonFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (oDoc.Load(osSubDirZarrJsonFilename.c_str()))
        {
            const auto oRoot = oDoc.GetRoot();
            if (oRoot.GetInteger("zarr_format") != 3)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unhandled zarr_format value");
                return nullptr;
            }
            const std::string osNodeType = oRoot.GetString("node_type");
            if (osNodeType != "group")
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s is a %s, not a group",
                         osName.c_str(), osNodeType.c_str());
                return nullptr;
            }
            auto poSubGroup = ZarrV3Group::Create(
                m_poSharedResource, GetFullName(), osName, osSubDir);
            poSubGroup->m_bFileHasBeenWritten = true;
            poSubGroup->m_poParent =
                std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
            poSubGroup->SetUpdatable(m_bUpdatable);
            m_oMapGroups[osName] = poSubGroup;
            return poSubGroup;
        }
        return nullptr;
    }

    // Implicit group
    if (VSIStatL(osSubDir.c_str(), &sStat) == 0 && VSI_ISDIR(sStat.st_mode))
    {
        // Note: Python zarr v3.0.2 still generates implicit groups
        // See https://github.com/zarr-developers/zarr-python/issues/2794
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Support for Zarr V3 implicit group is now deprecated, and "
                 "may be removed in a future version");
        auto poSubGroup = ZarrV3Group::Create(m_poSharedResource, GetFullName(),
                                              osName, osSubDir);
        poSubGroup->m_bFileHasBeenWritten = true;
        poSubGroup->m_poParent =
            std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    return nullptr;
}

/************************************************************************/
/*                     ZarrV3Group::CreateOnDisk()                      */
/************************************************************************/

std::shared_ptr<ZarrV3Group> ZarrV3Group::CreateOnDisk(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentFullName, const std::string &osName,
    const std::string &osDirectoryName)
{
    if (VSIMkdir(osDirectoryName.c_str(), 0755) != 0)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osDirectoryName.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                     osDirectoryName.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osDirectoryName.c_str());
        }
        return nullptr;
    }

    const std::string osZarrJsonFilename(
        CPLFormFilenameSafe(osDirectoryName.c_str(), "zarr.json", nullptr));
    VSILFILE *fp = nullptr;
    if (!(poSharedResource->IsConsolidatedMetadataEnabled() &&
          cpl::starts_with(osZarrJsonFilename, "/vsizip/") &&
          osParentFullName.empty() && osName == "/"))
    {
        fp = VSIFOpenL(osZarrJsonFilename.c_str(), "wb");
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                     osZarrJsonFilename.c_str());
            return nullptr;
        }
        VSIFPrintfL(fp, "{\n"
                        "    \"zarr_format\": 3,\n"
                        "    \"node_type\": \"group\",\n"
                        "    \"attributes\": {}\n"
                        "}\n");
        VSIFCloseL(fp);
    }

    auto poGroup = ZarrV3Group::Create(poSharedResource, osParentFullName,
                                       osName, osDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;
    poGroup->m_bFileHasBeenWritten = fp != nullptr;

    CPLJSONObject oObj;
    oObj.Add("zarr_format", 3);
    oObj.Add("node_type", "group");
    oObj.Add("attributes", CPLJSONObject());
    poSharedResource->SetZMetadataItem(osZarrJsonFilename, oObj);

    return poGroup;
}

/************************************************************************/
/*                      ZarrV3Group::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup>
ZarrV3Group::CreateGroup(const std::string &osName,
                         CSLConstList /* papszOptions */)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (!IsValidObjectName(osName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid group name");
        return nullptr;
    }

    GetGroupNames();

    if (cpl::contains(m_oSetGroupNames, osName))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }

    const std::string osDirectoryName =
        CPLFormFilenameSafe(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    auto poGroup = CreateOnDisk(m_poSharedResource, GetFullName(), osName,
                                osDirectoryName);
    if (!poGroup)
        return nullptr;
    poGroup->m_poParent =
        std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
    m_oMapGroups[osName] = poGroup;
    m_aosGroups.emplace_back(osName);
    return poGroup;
}

/************************************************************************/
/*                           FillDTypeElts()                            */
/************************************************************************/

static CPLJSONObject FillDTypeElts(const GDALExtendedDataType &oDataType,
                                   std::vector<DtypeElt> &aoDtypeElts)
{
    CPLJSONObject dtype;
    const std::string dummy("dummy");

    if (oDataType.GetClass() == GEDTC_STRING)
    {
        const int nMaxLen = std::max(
            2, atoi(CPLGetConfigOption("ZARR_VLEN_STRING_MAX_LENGTH", "256")));
        DtypeElt elt;
        elt.nativeType = DtypeElt::NativeType::STRING_ASCII;
        elt.nativeOffset = 0;
        elt.nativeSize = static_cast<size_t>(nMaxLen);
        elt.gdalOffset = 0;
        elt.gdalSize = oDataType.GetSize();
        aoDtypeElts.emplace_back(elt);
        dtype.Set(dummy, "string");
        return dtype;
    }

    const auto eDT = oDataType.GetNumericDataType();
    DtypeElt elt;
    bool bUnsupported = false;
    switch (eDT)
    {
        case GDT_UInt8:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint8");
            break;
        }
        case GDT_Int8:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int8");
            break;
        }
        case GDT_UInt16:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint16");
            break;
        }
        case GDT_Int16:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int16");
            break;
        }
        case GDT_UInt32:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint32");
            break;
        }
        case GDT_Int32:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int32");
            break;
        }
        case GDT_UInt64:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint64");
            break;
        }
        case GDT_Int64:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int64");
            break;
        }
        case GDT_Float16:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "float16");
            break;
        }
        case GDT_Float32:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "float32");
            break;
        }
        case GDT_Float64:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "float64");
            break;
        }
        case GDT_Unknown:
        case GDT_CInt16:
        case GDT_CInt32:
        {
            bUnsupported = true;
            break;
        }
        case GDT_CFloat16:
        {
            elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
            dtype.Set(dummy, "complex32");
            break;
        }
        case GDT_CFloat32:
        {
            elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
            dtype.Set(dummy, "complex64");
            break;
        }
        case GDT_CFloat64:
        {
            elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
            dtype.Set(dummy, "complex128");
            break;
        }
        case GDT_TypeCount:
        {
            static_assert(GDT_TypeCount == GDT_CFloat16 + 1,
                          "GDT_TypeCount == GDT_CFloat16 + 1");
            break;
        }
    }
    if (bUnsupported)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type: %s",
                 GDALGetDataTypeName(eDT));
        dtype = CPLJSONObject();
        dtype.Deinit();
        return dtype;
    }
    elt.nativeOffset = 0;
    elt.nativeSize = GDALGetDataTypeSizeBytes(eDT);
    elt.gdalOffset = 0;
    elt.gdalSize = elt.nativeSize;
#ifdef CPL_MSB
    elt.needByteSwapping = elt.nativeSize > 1;
#endif
    aoDtypeElts.emplace_back(elt);

    return dtype;
}

/************************************************************************/
/*                     ZarrV3Group::CreateMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrV3Group::CreateMDArray(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (!IsValidObjectName(osName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid array name");
        return nullptr;
    }

    if (oDataType.GetClass() != GEDTC_NUMERIC &&
        oDataType.GetClass() != GEDTC_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported data type with Zarr V3");
        return nullptr;
    }

    if (!EQUAL(CSLFetchNameValueDef(papszOptions, "FILTER", "NONE"), "NONE"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FILTER option not supported with Zarr V3");
        return nullptr;
    }

    std::vector<DtypeElt> aoDtypeElts;
    const auto dtype = FillDTypeElts(oDataType, aoDtypeElts)["dummy"];
    if (!dtype.IsValid() || aoDtypeElts.empty())
        return nullptr;

    GetMDArrayNames();

    if (cpl::contains(m_oSetArrayNames, osName))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    std::vector<GUInt64> anOuterBlockSize;
    if (!ZarrArray::FillBlockSize(aoDimensions, oDataType, anOuterBlockSize,
                                  papszOptions))
        return nullptr;

    const char *pszDimSeparator =
        CSLFetchNameValueDef(papszOptions, "DIM_SEPARATOR", "/");

    const std::string osArrayDirectory =
        CPLFormFilenameSafe(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if (VSIMkdir(osArrayDirectory.c_str(), 0755) != 0)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osArrayDirectory.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                     osArrayDirectory.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osArrayDirectory.c_str());
        }
        return nullptr;
    }

    std::unique_ptr<ZarrV3CodecSequence> poCodecs;
    CPLJSONArray oCodecs;

    const bool bIsString = (oDataType.GetClass() == GEDTC_STRING);

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");
    if (!bIsString && bFortranOrder && aoDimensions.size() > 1)
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "transpose");
        std::vector<int> anOrder;
        const int nDims = static_cast<int>(aoDimensions.size());
        for (int i = 0; i < nDims; ++i)
        {
            anOrder.push_back(nDims - 1 - i);
        }
        oCodec.Add("configuration",
                   ZarrV3CodecTranspose::GetConfiguration(anOrder));
        oCodecs.Add(oCodec);
    }

    // Array-to-bytes codec: vlen-utf8 for strings, bytes for numeric
    if (bIsString)
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "vlen-utf8");
        oCodecs.Add(oCodec);
    }
    else
    {
        // Not documented option, but 'bytes' codec is required
        const char *pszEndian =
            CSLFetchNameValueDef(papszOptions, "@ENDIAN", "little");
        CPLJSONObject oCodec;
        oCodec.Add("name", "bytes");
        oCodec.Add("configuration", ZarrV3CodecBytes::GetConfiguration(
                                        EQUAL(pszEndian, "little")));
        oCodecs.Add(oCodec);
    }

    const char *pszCompressor =
        CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    if (EQUAL(pszCompressor, "GZIP"))
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "gzip");
        const char *pszLevel =
            CSLFetchNameValueDef(papszOptions, "GZIP_LEVEL", "6");
        oCodec.Add("configuration",
                   ZarrV3CodecGZip::GetConfiguration(atoi(pszLevel)));
        oCodecs.Add(oCodec);
    }
    else if (EQUAL(pszCompressor, "BLOSC"))
    {
        const auto psCompressor = CPLGetCompressor("blosc");
        if (!psCompressor)
            return nullptr;
        const char *pszOptions =
            CSLFetchNameValueDef(psCompressor->papszMetadata, "OPTIONS", "");
        CPLXMLTreeCloser oTreeCompressor(CPLParseXMLString(pszOptions));
        const auto psRoot =
            oTreeCompressor.get()
                ? CPLGetXMLNode(oTreeCompressor.get(), "=Options")
                : nullptr;
        if (!psRoot)
            return nullptr;

        const char *cname = "zlib";
        for (const CPLXMLNode *psNode = psRoot->psChild; psNode != nullptr;
             psNode = psNode->psNext)
        {
            if (psNode->eType == CXT_Element)
            {
                const char *pszName = CPLGetXMLValue(psNode, "name", "");
                if (EQUAL(pszName, "CNAME"))
                {
                    cname = CPLGetXMLValue(psNode, "default", cname);
                }
            }
        }

        CPLJSONObject oCodec;
        oCodec.Add("name", "blosc");
        cname = CSLFetchNameValueDef(papszOptions, "BLOSC_CNAME", cname);
        const int clevel =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOSC_CLEVEL", "5"));
        const char *shuffle =
            CSLFetchNameValueDef(papszOptions, "BLOSC_SHUFFLE", "BYTE");
        shuffle = (EQUAL(shuffle, "0") || EQUAL(shuffle, "NONE")) ? "noshuffle"
                  : (EQUAL(shuffle, "1") || EQUAL(shuffle, "BYTE")) ? "shuffle"
                  : (EQUAL(shuffle, "2") || EQUAL(shuffle, "BIT"))
                      ? "bitshuffle"
                      : "invalid";
        const int nDefaultTypeSize =
            bIsString ? 1
                      : GDALGetDataTypeSizeBytes(GDALGetNonComplexDataType(
                            oDataType.GetNumericDataType()));
        const int typesize =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOSC_TYPESIZE",
                                      CPLSPrintf("%d", nDefaultTypeSize)));
        const int blocksize =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOSC_BLOCKSIZE", "0"));
        oCodec.Add("configuration",
                   ZarrV3CodecBlosc::GetConfiguration(cname, clevel, shuffle,
                                                      typesize, blocksize));
        oCodecs.Add(oCodec);
    }
    else if (EQUAL(pszCompressor, "ZSTD"))
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "zstd");
        const char *pszLevel =
            CSLFetchNameValueDef(papszOptions, "ZSTD_LEVEL", "13");
        const bool bChecksum = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "ZSTD_CHECKSUM", "FALSE"));
        oCodec.Add("configuration", ZarrV3CodecZstd::GetConfiguration(
                                        atoi(pszLevel), bChecksum));
        oCodecs.Add(oCodec);
    }
    else if (!EQUAL(pszCompressor, "NONE"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "COMPRESS = %s not implemented with Zarr V3", pszCompressor);
        return nullptr;
    }

    // Sharding: wrap inner codecs into a sharding_indexed codec
    const char *pszShardChunkShape =
        CSLFetchNameValue(papszOptions, "SHARD_CHUNK_SHAPE");
    if (pszShardChunkShape != nullptr)
    {

        const CPLStringList aosChunkShape(
            CSLTokenizeString2(pszShardChunkShape, ",", 0));
        if (static_cast<size_t>(aosChunkShape.size()) != aoDimensions.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SHARD_CHUNK_SHAPE has %d values, expected %d",
                     aosChunkShape.size(),
                     static_cast<int>(aoDimensions.size()));
            return nullptr;
        }

        CPLJSONArray oChunkShapeArray;
        for (int i = 0; i < aosChunkShape.size(); ++i)
        {
            const auto nInner = static_cast<GUInt64>(atoll(aosChunkShape[i]));
            if (nInner == 0 || anOuterBlockSize[i] % nInner != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "SHARD_CHUNK_SHAPE[%d]=%s must divide "
                         "BLOCKSIZE[%d]=" CPL_FRMT_GUIB " evenly",
                         i, aosChunkShape[i], i, anOuterBlockSize[i]);
                return nullptr;
            }
            oChunkShapeArray.Add(static_cast<uint64_t>(nInner));
        }

        // Index codecs: always bytes(little) + crc32c
        CPLJSONArray oIndexCodecs;
        {
            CPLJSONObject oBytesCodec;
            oBytesCodec.Add("name", "bytes");
            oBytesCodec.Add("configuration",
                            ZarrV3CodecBytes::GetConfiguration(true));
            oIndexCodecs.Add(oBytesCodec);
        }
        {
            CPLJSONObject oCRC32CCodec;
            oCRC32CCodec.Add("name", "crc32c");
            oIndexCodecs.Add(oCRC32CCodec);
        }

        CPLJSONObject oShardingConfig;
        oShardingConfig.Add("chunk_shape", oChunkShapeArray);
        oShardingConfig.Add("codecs", oCodecs);
        oShardingConfig.Add("index_codecs", oIndexCodecs);
        oShardingConfig.Add("index_location", "end");

        CPLJSONObject oShardingCodec;
        oShardingCodec.Add("name", "sharding_indexed");
        oShardingCodec.Add("configuration", oShardingConfig);

        // Replace top-level codecs with just the sharding codec
        oCodecs = CPLJSONArray();
        oCodecs.Add(oShardingCodec);
    }

    std::vector<GUInt64> anInnerBlockSize = anOuterBlockSize;
    if (oCodecs.Size() > 0)
    {
        std::vector<GByte> abyNoData;
        poCodecs = ZarrV3Array::SetupCodecs(oCodecs, anOuterBlockSize,
                                            anInnerBlockSize,
                                            aoDtypeElts.back(), abyNoData);
        if (!poCodecs)
        {
            return nullptr;
        }
    }

    auto poArray = ZarrV3Array::Create(m_poSharedResource, Self(), osName,
                                       aoDimensions, oDataType, aoDtypeElts,
                                       anOuterBlockSize, anInnerBlockSize);

    if (!poArray)
        return nullptr;
    poArray->SetNew(true);
    const std::string osFilename =
        CPLFormFilenameSafe(osArrayDirectory.c_str(), "zarr.json", nullptr);
    poArray->SetFilename(osFilename);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetDtype(dtype);
    const std::string osLastCodecName =
        oCodecs.Size() > 0 ? oCodecs[oCodecs.Size() - 1].GetString("name")
                           : std::string();
    if (!osLastCodecName.empty() && osLastCodecName != "bytes" &&
        osLastCodecName != "vlen-utf8")
    {
        poArray->SetStructuralInfo(
            "COMPRESSOR", oCodecs[oCodecs.Size() - 1].ToString().c_str());
    }
    if (poCodecs)
        poArray->SetCodecs(oCodecs, std::move(poCodecs));

    poArray->SetCreationOptions(papszOptions);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    if (!cpl::starts_with(osFilename, "/vsi") && !poArray->Flush())
        return nullptr;
    RegisterArray(poArray);

    return poArray;
}
