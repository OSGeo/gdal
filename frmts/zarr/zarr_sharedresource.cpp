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
#include "vsikerchunk.h"

#include "cpl_json.h"

/************************************************************************/
/*               ZarrSharedResource::ZarrSharedResource()               */
/************************************************************************/

ZarrSharedResource::ZarrSharedResource(const std::string &osRootDirectoryName,
                                       bool bUpdatable)
    : m_bUpdatable(bUpdatable)
{
    m_oObjConsolidatedMetadata.Deinit();

    m_osRootDirectoryName = osRootDirectoryName;
    if (!m_osRootDirectoryName.empty() && m_osRootDirectoryName.back() == '/')
    {
        m_osRootDirectoryName.pop_back();
    }
    m_poPAM = std::make_shared<GDALPamMultiDim>(
        CPLFormFilenameSafe(m_osRootDirectoryName.c_str(), "pam", nullptr));
}

/************************************************************************/
/*                     ZarrSharedResource::Create()                     */
/************************************************************************/

std::shared_ptr<ZarrSharedResource>
ZarrSharedResource::Create(const std::string &osRootDirectoryName,
                           bool bUpdatable)
{
    return std::shared_ptr<ZarrSharedResource>(
        new ZarrSharedResource(osRootDirectoryName, bUpdatable));
}

/************************************************************************/
/*              ZarrSharedResource::~ZarrSharedResource()               */
/************************************************************************/

ZarrSharedResource::~ZarrSharedResource()
{
    // We try to clean caches at dataset closing, especially for Parquet
    // references, since closing Parquet datasets when the virtual file
    // systems are destroyed can be too late and cause crashes.
    VSIKerchunkFileSystemsCleanCache();

    if (m_bConsolidatedMetadataModified)
    {
        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::EXTERNAL)
        {
            CPLJSONDocument oDoc;
            oDoc.SetRoot(m_oObjConsolidatedMetadata);
            oDoc.Save(CPLFormFilenameSafe(m_osRootDirectoryName.c_str(),
                                          ".zmetadata", nullptr));
        }
        else if (m_eConsolidatedMetadataKind ==
                     ConsolidatedMetadataKind::INTERNAL &&
                 !cpl::starts_with(m_osRootDirectoryName, "/vsizip/"))
        {
            CPLJSONDocument oDoc;
            const std::string osFilename = CPLFormFilenameSafe(
                m_osRootDirectoryName.c_str(), "zarr.json", nullptr);
            if (oDoc.Load(osFilename))
            {
                oDoc.GetRoot().Set("consolidated_metadata",
                                   m_oObjConsolidatedMetadata);
                oDoc.Save(osFilename);
            }
        }
    }
}

/************************************************************************/
/*                 ZarrSharedResource::OpenRootGroup()                  */
/************************************************************************/

std::shared_ptr<ZarrGroupBase> ZarrSharedResource::OpenRootGroup()
{
    // Probe zarr.json first so v3 datasets skip the v2 stat cascade.
    const std::string osZarrJsonFilename(CPLFormFilenameSafe(
        m_osRootDirectoryName.c_str(), "zarr.json", nullptr));
    VSIStatBufL sStat;
    const bool bHasZarrJson =
        (VSIStatL(osZarrJsonFilename.c_str(), &sStat) == 0);

    if (!bHasZarrJson)
    {
        auto poRG = ZarrV2Group::Create(shared_from_this(), std::string(), "/");
        // Prevents potential recursion
        m_poWeakRootGroup = poRG;
        poRG->SetUpdatable(m_bUpdatable);
        poRG->SetDirectoryName(m_osRootDirectoryName);

        const std::string osZarrayFilename(CPLFormFilenameSafe(
            m_osRootDirectoryName.c_str(), ".zarray", nullptr));
        const auto nErrorCount = CPLGetErrorCounter();
        if (VSIStatL(osZarrayFilename.c_str(), &sStat) == 0)
        {
            CPLJSONDocument oDoc;
            if (!oDoc.Load(osZarrayFilename))
                return nullptr;
            const auto oRoot = oDoc.GetRoot();
            if (oRoot["_NCZARR_ARRAY"].IsValid())
            {
                // If opening a NCZarr array, initialize its group from NCZarr
                // metadata.
                const std::string osGroupFilename(CPLFormFilenameSafe(
                    CPLGetDirnameSafe(m_osRootDirectoryName.c_str()).c_str(),
                    ".zgroup", nullptr));
                if (VSIStatL(osGroupFilename.c_str(), &sStat) == 0)
                {
                    CPLJSONDocument oDocGroup;
                    if (oDocGroup.Load(osGroupFilename))
                    {
                        if (!poRG->InitFromZGroup(oDocGroup.GetRoot()))
                            return nullptr;
                    }
                }
            }
            const std::string osArrayName(
                CPLGetBasenameSafe(m_osRootDirectoryName.c_str()));

            if (!poRG->LoadArray(osArrayName, osZarrayFilename, oRoot, false,
                                 CPLJSONObject()))
                return nullptr;

            return poRG;
        }
        else if (CPLGetErrorCounter() > nErrorCount &&
                 strstr(CPLGetLastErrorMsg(),
                        "Generation of Kerchunk Parquet cache"))
        {
            return nullptr;
        }

        const std::string osZmetadataFilename(CPLFormFilenameSafe(
            m_osRootDirectoryName.c_str(), ".zmetadata", nullptr));
        if (CPLTestBool(CSLFetchNameValueDef(
                GetOpenOptions(), "USE_CONSOLIDATED_METADATA",
                CSLFetchNameValueDef(GetOpenOptions(), "USE_ZMETADATA",
                                     "YES"))) &&
            VSIStatL(osZmetadataFilename.c_str(), &sStat) == 0)
        {
            if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::NONE)
            {
                CPLJSONDocument oDoc;
                if (!oDoc.Load(osZmetadataFilename))
                    return nullptr;

                m_eConsolidatedMetadataKind =
                    ConsolidatedMetadataKind::EXTERNAL;
                m_oObjConsolidatedMetadata = oDoc.GetRoot();
            }

            poRG->InitFromConsolidatedMetadata(m_oObjConsolidatedMetadata);

            return poRG;
        }

        const std::string osGroupFilename(CPLFormFilenameSafe(
            m_osRootDirectoryName.c_str(), ".zgroup", nullptr));
        if (VSIStatL(osGroupFilename.c_str(), &sStat) == 0)
        {
            CPLJSONDocument oDoc;
            if (!oDoc.Load(osGroupFilename))
                return nullptr;

            if (!poRG->InitFromZGroup(oDoc.GetRoot()))
                return nullptr;
            return poRG;
        }
    }

    // Zarr v3
    auto poRG_V3 = ZarrV3Group::Create(shared_from_this(), std::string(), "/",
                                       m_osRootDirectoryName);
    // Prevents potential recursion
    m_poWeakRootGroup = poRG_V3;
    poRG_V3->SetUpdatable(m_bUpdatable);

    if (bHasZarrJson)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osZarrJsonFilename))
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        if (oRoot.GetInteger("zarr_format") != 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unhandled zarr_format value");
            return nullptr;
        }

        // Not yet adopted, but described at https://github.com/zarr-developers/zarr-specs/pull/309/files
        // and used for example by
        // https://s3.explorer.eopf.copernicus.eu/esa-zarr-sentinel-explorer-fra/tests-output/sentinel-2-l2a/S2B_MSIL2A_20251218T110359_N0511_R094_T32VLK_20251218T115223.zarr/measurements/reflectance/zarr.json
        const auto oConsolidatedMetadata =
            oRoot.GetObj("consolidated_metadata");
        if (oConsolidatedMetadata.GetType() == CPLJSONObject::Type::Object &&
            oConsolidatedMetadata.GetString("kind") == "inline" &&
            CPLTestBool(CSLFetchNameValueDef(
                GetOpenOptions(), "USE_CONSOLIDATED_METADATA",
                CSLFetchNameValueDef(GetOpenOptions(), "USE_ZMETADATA",
                                     "YES"))))
        {
            if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::NONE)
            {
                CPLDebug("JSON", "Using consolidated_metadata");
                m_eConsolidatedMetadataKind =
                    ConsolidatedMetadataKind::INTERNAL;
                m_oObjConsolidatedMetadata = oConsolidatedMetadata;
                m_oRootAttributes = oRoot.GetObj("attributes");
            }

            poRG_V3->InitFromConsolidatedMetadata(m_oObjConsolidatedMetadata,
                                                  m_oRootAttributes);
        }

        const std::string osNodeType = oRoot.GetString("node_type");
        if (osNodeType == "array")
        {
            const std::string osArrayName(
                CPLGetBasenameSafe(m_osRootDirectoryName.c_str()));
            poRG_V3->SetExplored();
            if (!poRG_V3->LoadArray(osArrayName, osZarrJsonFilename, oRoot))
                return nullptr;

            return poRG_V3;
        }
        else if (osNodeType == "group")
        {
            return poRG_V3;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unhandled node_type value");
            return nullptr;
        }
    }

    // No explicit zarr.json in root directory ? Then recurse until we find
    // one.
    auto psDir = VSIOpenDir(m_osRootDirectoryName.c_str(), -1, nullptr);
    if (!psDir)
        return nullptr;
    bool bZarrJsonFound = false;
    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
    {
        if (!VSI_ISDIR(psEntry->nMode) &&
            strcmp(CPLGetFilename(psEntry->pszName), "zarr.json") == 0)
        {
            bZarrJsonFound = true;
            break;
        }
    }
    VSICloseDir(psDir);
    if (bZarrJsonFound)
        return poRG_V3;

    return nullptr;
}

/************************************************************************/
/*                  ZarrSharedResource::GetRootGroup()                  */
/************************************************************************/

std::shared_ptr<ZarrGroupBase> ZarrSharedResource::GetRootGroup()
{
    auto poRootGroup = m_poWeakRootGroup.lock();
    if (poRootGroup)
        return poRootGroup;
    poRootGroup = OpenRootGroup();
    m_poWeakRootGroup = poRootGroup;
    return poRootGroup;
}

/************************************************************************/
/*        ZarrSharedResource::InitConsolidatedMetadataIfNeeded()        */
/************************************************************************/

void ZarrSharedResource::InitConsolidatedMetadataIfNeeded()
{
    if (!m_oObjConsolidatedMetadata.IsValid())
    {
        m_oObjConsolidatedMetadata = CPLJSONObject();
        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::EXTERNAL)
        {
            m_oObjConsolidatedMetadata.Add("zarr_consolidated_format", 1);
            m_oObjConsolidatedMetadata.Add("metadata", CPLJSONObject());
        }
        else
        {
            m_oObjConsolidatedMetadata.Add("kind", "inline");
            m_oObjConsolidatedMetadata.Add("must_understand", false);
            m_oObjConsolidatedMetadata.Add("metadata", CPLJSONObject());
        }
    }
}

/************************************************************************/
/*                ZarrSharedResource::SetZMetadataItem()                */
/************************************************************************/

void ZarrSharedResource::SetZMetadataItem(const std::string &osFilename,
                                          const CPLJSONObject &obj)
{
    if (m_eConsolidatedMetadataKind != ConsolidatedMetadataKind::NONE)
    {
        InitConsolidatedMetadataIfNeeded();

        CPLString osNormalizedFilename(osFilename);
        osNormalizedFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::INTERNAL)
        {
            const auto nPos = osNormalizedFilename.rfind('/');
            if (nPos == std::string::npos)
                return;
            osNormalizedFilename.resize(nPos);
        }

        const char *pszKey =
            osNormalizedFilename.c_str() + m_osRootDirectoryName.size() + 1;
        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::EXTERNAL ||
            strcmp(pszKey, "zarr.json") != 0)
        {
            m_bConsolidatedMetadataModified = true;
            auto oMetadata = m_oObjConsolidatedMetadata["metadata"];
            oMetadata.DeleteNoSplitName(pszKey);
            oMetadata.AddNoSplitName(pszKey, obj);
        }
    }
}

/************************************************************************/
/*          ZarrSharedResource::DeleteZMetadataItemRecursive()          */
/************************************************************************/

void ZarrSharedResource::DeleteZMetadataItemRecursive(
    const std::string &osFilename)
{
    if (m_eConsolidatedMetadataKind != ConsolidatedMetadataKind::NONE)
    {
        InitConsolidatedMetadataIfNeeded();

        CPLString osNormalizedFilename(osFilename);
        osNormalizedFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::INTERNAL)
        {
            const auto nPos = osNormalizedFilename.rfind('/');
            if (nPos == std::string::npos)
                return;
            osNormalizedFilename.resize(nPos);
        }

        const char *pszKey =
            osNormalizedFilename.c_str() + m_osRootDirectoryName.size() + 1;
        if (m_eConsolidatedMetadataKind == ConsolidatedMetadataKind::EXTERNAL ||
            strcmp(pszKey, "zarr.json") != 0)
        {
            m_bConsolidatedMetadataModified = true;
            auto oMetadata = m_oObjConsolidatedMetadata["metadata"];
            for (auto &item : oMetadata.GetChildren())
            {
                if (STARTS_WITH(item.GetName().c_str(), pszKey))
                {
                    oMetadata.DeleteNoSplitName(item.GetName());
                }
            }
        }
    }
}

/************************************************************************/
/*            ZarrSharedResource::RenameZMetadataRecursive()            */
/************************************************************************/

void ZarrSharedResource::RenameZMetadataRecursive(
    const std::string &osOldFilename, const std::string &osNewFilename)
{
    if (m_eConsolidatedMetadataKind != ConsolidatedMetadataKind::NONE)
    {
        InitConsolidatedMetadataIfNeeded();

        CPLString osNormalizedOldFilename(osOldFilename);
        osNormalizedOldFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedOldFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        CPLString osNormalizedNewFilename(osNewFilename);
        osNormalizedNewFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedNewFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        m_bConsolidatedMetadataModified = true;

        const char *pszOldKeyRadix =
            osNormalizedOldFilename.c_str() + m_osRootDirectoryName.size() + 1;
        const char *pszNewKeyRadix =
            osNormalizedNewFilename.c_str() + m_osRootDirectoryName.size() + 1;

        auto oMetadata = m_oObjConsolidatedMetadata["metadata"];
        for (auto &item : oMetadata.GetChildren())
        {
            if (STARTS_WITH(item.GetName().c_str(), pszOldKeyRadix))
            {
                oMetadata.DeleteNoSplitName(item.GetName());
                std::string osNewKey(pszNewKeyRadix);
                osNewKey += (item.GetName().c_str() + strlen(pszOldKeyRadix));
                oMetadata.AddNoSplitName(osNewKey, item);
            }
        }
    }
}

/************************************************************************/
/*              ZarrSharedResource::UpdateDimensionSize()               */
/************************************************************************/

void ZarrSharedResource::UpdateDimensionSize(
    const std::shared_ptr<GDALDimension> &poDim)
{
    auto poRG = m_poWeakRootGroup.lock();
    if (!poRG)
        poRG = OpenRootGroup();
    if (poRG)
    {
        poRG->UpdateDimensionSize(poDim);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "UpdateDimensionSize() failed");
    }
    poRG.reset();
}

/************************************************************************/
/*               ZarrSharedResource::AddArrayInLoading()                */
/************************************************************************/

bool ZarrSharedResource::AddArrayInLoading(const std::string &osZarrayFilename)
{
    // Prevent too deep or recursive array loading
    if (m_oSetArrayInLoading.find(osZarrayFilename) !=
        m_oSetArrayInLoading.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt at recursively loading %s", osZarrayFilename.c_str());
        return false;
    }
    if (m_oSetArrayInLoading.size() == 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too deep call stack in LoadArray()");
        return false;
    }
    m_oSetArrayInLoading.insert(osZarrayFilename);
    return true;
}

/************************************************************************/
/*              ZarrSharedResource::RemoveArrayInLoading()              */
/************************************************************************/

void ZarrSharedResource::RemoveArrayInLoading(
    const std::string &osZarrayFilename)
{
    m_oSetArrayInLoading.erase(osZarrayFilename);
}
