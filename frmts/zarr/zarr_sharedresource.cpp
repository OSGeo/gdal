/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "zarr.h"

#include "cpl_json.h"

/************************************************************************/
/*              ZarrSharedResource::ZarrSharedResource()                */
/************************************************************************/

ZarrSharedResource::ZarrSharedResource(const std::string &osRootDirectoryName,
                                       bool bUpdatable)
    : m_bUpdatable(bUpdatable)
{
    m_oObj.Add("zarr_consolidated_format", 1);
    m_oObj.Add("metadata", CPLJSONObject());

    m_osRootDirectoryName = osRootDirectoryName;
    if (!m_osRootDirectoryName.empty() && m_osRootDirectoryName.back() == '/')
    {
        m_osRootDirectoryName.resize(m_osRootDirectoryName.size() - 1);
    }
    m_poPAM = std::make_shared<GDALPamMultiDim>(
        CPLFormFilename(m_osRootDirectoryName.c_str(), "pam", nullptr));
}

/************************************************************************/
/*              ZarrSharedResource::Create()                            */
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
    if (m_bZMetadataModified)
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(m_oObj);
        oDoc.Save(CPLFormFilename(m_osRootDirectoryName.c_str(), ".zmetadata",
                                  nullptr));
    }
}

/************************************************************************/
/*             ZarrSharedResource::OpenRootGroup()                      */
/************************************************************************/

std::shared_ptr<ZarrGroupBase> ZarrSharedResource::OpenRootGroup()
{
    {
        auto poRG = ZarrV2Group::Create(shared_from_this(), std::string(), "/");
        poRG->SetUpdatable(m_bUpdatable);
        poRG->SetDirectoryName(m_osRootDirectoryName);

        const std::string osZarrayFilename(
            CPLFormFilename(m_osRootDirectoryName.c_str(), ".zarray", nullptr));
        VSIStatBufL sStat;
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
                const std::string osGroupFilename(CPLFormFilename(
                    CPLGetDirname(m_osRootDirectoryName.c_str()), ".zgroup",
                    nullptr));
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
                CPLGetBasename(m_osRootDirectoryName.c_str()));
            if (!poRG->LoadArray(osArrayName, osZarrayFilename, oRoot, false,
                                 CPLJSONObject()))
                return nullptr;

            return poRG;
        }

        const std::string osZmetadataFilename(CPLFormFilename(
            m_osRootDirectoryName.c_str(), ".zmetadata", nullptr));
        if (CPLTestBool(CSLFetchNameValueDef(GetOpenOptions(), "USE_ZMETADATA",
                                             "YES")) &&
            VSIStatL(osZmetadataFilename.c_str(), &sStat) == 0)
        {
            if (!m_bZMetadataEnabled)
            {
                CPLJSONDocument oDoc;
                if (!oDoc.Load(osZmetadataFilename))
                    return nullptr;

                m_bZMetadataEnabled = true;
                m_oObj = oDoc.GetRoot();
            }
            poRG->InitFromZMetadata(m_oObj);

            return poRG;
        }

        const std::string osGroupFilename(
            CPLFormFilename(m_osRootDirectoryName.c_str(), ".zgroup", nullptr));
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
    poRG_V3->SetUpdatable(m_bUpdatable);

    const std::string osZarrJsonFilename(
        CPLFormFilename(m_osRootDirectoryName.c_str(), "zarr.json", nullptr));
    VSIStatBufL sStat;
    if (VSIStatL(osZarrJsonFilename.c_str(), &sStat) == 0)
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
        const std::string osNodeType = oRoot.GetString("node_type");
        if (osNodeType == "array")
        {
            const std::string osArrayName(
                CPLGetBasename(m_osRootDirectoryName.c_str()));
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
/*             ZarrSharedResource::SetZMetadataItem()                   */
/************************************************************************/

void ZarrSharedResource::SetZMetadataItem(const std::string &osFilename,
                                          const CPLJSONObject &obj)
{
    if (m_bZMetadataEnabled)
    {
        CPLString osNormalizedFilename(osFilename);
        osNormalizedFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));
        m_bZMetadataModified = true;
        const char *pszKey =
            osNormalizedFilename.c_str() + m_osRootDirectoryName.size() + 1;
        auto oMetadata = m_oObj["metadata"];
        oMetadata.DeleteNoSplitName(pszKey);
        oMetadata.AddNoSplitName(pszKey, obj);
    }
}

/************************************************************************/
/*         ZarrSharedResource::DeleteZMetadataItemRecursive()           */
/************************************************************************/

void ZarrSharedResource::DeleteZMetadataItemRecursive(
    const std::string &osFilename)
{
    if (m_bZMetadataEnabled)
    {
        CPLString osNormalizedFilename(osFilename);
        osNormalizedFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));
        m_bZMetadataModified = true;
        const char *pszKey =
            osNormalizedFilename.c_str() + m_osRootDirectoryName.size() + 1;

        auto oMetadata = m_oObj["metadata"];
        for (auto &item : oMetadata.GetChildren())
        {
            if (STARTS_WITH(item.GetName().c_str(), pszKey))
            {
                oMetadata.DeleteNoSplitName(item.GetName());
            }
        }
    }
}

/************************************************************************/
/*             ZarrSharedResource::RenameZMetadataRecursive()           */
/************************************************************************/

void ZarrSharedResource::RenameZMetadataRecursive(
    const std::string &osOldFilename, const std::string &osNewFilename)
{
    if (m_bZMetadataEnabled)
    {
        CPLString osNormalizedOldFilename(osOldFilename);
        osNormalizedOldFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedOldFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        CPLString osNormalizedNewFilename(osNewFilename);
        osNormalizedNewFilename.replaceAll('\\', '/');
        CPLAssert(STARTS_WITH(osNormalizedNewFilename.c_str(),
                              (m_osRootDirectoryName + '/').c_str()));

        m_bZMetadataModified = true;

        const char *pszOldKeyRadix =
            osNormalizedOldFilename.c_str() + m_osRootDirectoryName.size() + 1;
        const char *pszNewKeyRadix =
            osNormalizedNewFilename.c_str() + m_osRootDirectoryName.size() + 1;

        auto oMetadata = m_oObj["metadata"];
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
/*             ZarrSharedResource::UpdateDimensionSize()                */
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
/*             ZarrSharedResource::AddArrayInLoading()                  */
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
/*             ZarrSharedResource::RemoveArrayInLoading()               */
/************************************************************************/

void ZarrSharedResource::RemoveArrayInLoading(
    const std::string &osZarrayFilename)
{
    m_oSetArrayInLoading.erase(osZarrayFilename);
}
