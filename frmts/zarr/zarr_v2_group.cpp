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

#include "cpl_minixml.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                      ZarrV2Group::Create()                           */
/************************************************************************/

std::shared_ptr<ZarrV2Group>
ZarrV2Group::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName)
{
    auto poGroup = std::shared_ptr<ZarrV2Group>(
        new ZarrV2Group(poSharedResource, osParentName, osName));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                      ZarrV2Group::~ZarrV2Group()                     */
/************************************************************************/

ZarrV2Group::~ZarrV2Group()
{
    if (m_bValid && m_oAttrGroup.IsModified())
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(m_oAttrGroup.Serialize());
        const std::string osAttrFilename =
            CPLFormFilename(m_osDirectoryName.c_str(), ".zattrs", nullptr);
        oDoc.Save(osAttrFilename);
        m_poSharedResource->SetZMetadataItem(osAttrFilename, oDoc.GetRoot());
    }
}

/************************************************************************/
/*                        ExploreDirectory()                            */
/************************************************************************/

void ZarrV2Group::ExploreDirectory() const
{
    if (m_bDirectoryExplored || m_osDirectoryName.empty())
        return;
    m_bDirectoryExplored = true;

    const CPLStringList aosFiles(VSIReadDir(m_osDirectoryName.c_str()));
    // If the directory contains a .zarray, no need to recurse.
    for (int i = 0; i < aosFiles.size(); ++i)
    {
        if (strcmp(aosFiles[i], ".zarray") == 0)
            return;
    }

    for (int i = 0; i < aosFiles.size(); ++i)
    {
        if (aosFiles[i][0] != 0 && strcmp(aosFiles[i], ".") != 0 &&
            strcmp(aosFiles[i], "..") != 0 &&
            strcmp(aosFiles[i], ".zgroup") != 0 &&
            strcmp(aosFiles[i], ".zattrs") != 0 &&
            // Exclude filenames ending with '/'. This can happen on some
            // object storage like S3 where a "foo" file and a "foo/" directory
            // can coexist. The ending slash is only appended in that situation
            // where both a file and directory have the same name. So we can
            // safely ignore the one with an ending slash, as we will also
            // encounter its version without slash. Cf use case of
            // https://github.com/OSGeo/gdal/issues/8192
            aosFiles[i][strlen(aosFiles[i]) - 1] != '/')
        {
            const std::string osSubDir = CPLFormFilename(
                m_osDirectoryName.c_str(), aosFiles[i], nullptr);
            VSIStatBufL sStat;
            std::string osFilename =
                CPLFormFilename(osSubDir.c_str(), ".zarray", nullptr);
            if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            {
                if (std::find(m_aosArrays.begin(), m_aosArrays.end(),
                              aosFiles[i]) == m_aosArrays.end())
                {
                    m_aosArrays.emplace_back(aosFiles[i]);
                }
            }
            else
            {
                osFilename =
                    CPLFormFilename(osSubDir.c_str(), ".zgroup", nullptr);
                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    m_aosGroups.emplace_back(aosFiles[i]);
            }
        }
    }
}

/************************************************************************/
/*                             OpenZarrArray()                          */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrV2Group::OpenZarrArray(const std::string &osName,
                                                      CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
        return oIter->second;

    if (!m_bReadFromZMetadata && !m_osDirectoryName.empty())
    {
        const std::string osSubDir =
            CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
        VSIStatBufL sStat;
        const std::string osZarrayFilename =
            CPLFormFilename(osSubDir.c_str(), ".zarray", nullptr);
        if (VSIStatL(osZarrayFilename.c_str(), &sStat) == 0)
        {
            CPLJSONDocument oDoc;
            if (!oDoc.Load(osZarrayFilename))
                return nullptr;
            const auto oRoot = oDoc.GetRoot();
            return LoadArray(osName, osZarrayFilename, oRoot, false,
                             CPLJSONObject());
        }
    }

    return nullptr;
}

/************************************************************************/
/*                              OpenZarrGroup()                             */
/************************************************************************/

std::shared_ptr<ZarrGroupBase>
ZarrV2Group::OpenZarrGroup(const std::string &osName, CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
        return oIter->second;

    if (!m_bReadFromZMetadata && !m_osDirectoryName.empty())
    {
        const std::string osSubDir =
            CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
        VSIStatBufL sStat;
        const std::string osZgroupFilename =
            CPLFormFilename(osSubDir.c_str(), ".zgroup", nullptr);
        if (VSIStatL(osZgroupFilename.c_str(), &sStat) == 0)
        {
            CPLJSONDocument oDoc;
            if (!oDoc.Load(osZgroupFilename))
                return nullptr;

            auto poSubGroup =
                ZarrV2Group::Create(m_poSharedResource, GetFullName(), osName);
            poSubGroup->m_poParent =
                std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
            poSubGroup->SetUpdatable(m_bUpdatable);
            poSubGroup->SetDirectoryName(osSubDir);
            m_oMapGroups[osName] = poSubGroup;

            // Must be done after setting m_oMapGroups, to avoid infinite
            // recursion when opening NCZarr datasets with indexing variables
            // of dimensions
            poSubGroup->InitFromZGroup(oDoc.GetRoot());

            return poSubGroup;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                   ZarrV2Group::LoadAttributes()                      */
/************************************************************************/

void ZarrV2Group::LoadAttributes() const
{
    if (m_bAttributesLoaded || m_osDirectoryName.empty())
        return;
    m_bAttributesLoaded = true;

    CPLJSONDocument oDoc;
    const std::string osZattrsFilename(
        CPLFormFilename(m_osDirectoryName.c_str(), ".zattrs", nullptr));
    CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
    if (!oDoc.Load(osZattrsFilename))
        return;
    auto oRoot = oDoc.GetRoot();
    m_oAttrGroup.Init(oRoot, m_bUpdatable);
}

/************************************************************************/
/*                   ZarrV2Group::GetOrCreateSubGroup()                 */
/************************************************************************/

std::shared_ptr<ZarrV2Group>
ZarrV2Group::GetOrCreateSubGroup(const std::string &osSubGroupFullname)
{
    auto poSubGroup = std::dynamic_pointer_cast<ZarrV2Group>(
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

    poSubGroup =
        ZarrV2Group::Create(m_poSharedResource, poBelongingGroup->GetFullName(),
                            osSubGroupFullname.substr(nLastSlashPos + 1));
    poSubGroup->m_poParent = std::dynamic_pointer_cast<ZarrGroupBase>(
        poBelongingGroup->m_pSelf.lock());
    poSubGroup->SetDirectoryName(
        CPLFormFilename(poBelongingGroup->m_osDirectoryName.c_str(),
                        poSubGroup->GetName().c_str(), nullptr));
    poSubGroup->m_bDirectoryExplored = true;
    poSubGroup->m_bAttributesLoaded = true;
    poSubGroup->m_bReadFromZMetadata = true;
    poSubGroup->SetUpdatable(m_bUpdatable);

    poBelongingGroup->m_oMapGroups[poSubGroup->GetName()] = poSubGroup;
    poBelongingGroup->m_aosGroups.emplace_back(poSubGroup->GetName());
    return poSubGroup;
}

/************************************************************************/
/*                   ZarrV2Group::InitFromZMetadata()                   */
/************************************************************************/

void ZarrV2Group::InitFromZMetadata(const CPLJSONObject &obj)
{
    m_bDirectoryExplored = true;
    m_bAttributesLoaded = true;
    m_bReadFromZMetadata = true;

    const auto metadata = obj["metadata"];
    if (metadata.GetType() != CPLJSONObject::Type::Object)
        return;
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
        if (osName == ".zattrs")
        {
            m_oAttrGroup.Init(child, m_bUpdatable);
        }
        else if (osName.size() > strlen("/.zgroup") &&
                 osName.substr(osName.size() - strlen("/.zgroup")) ==
                     "/.zgroup")
        {
            GetOrCreateSubGroup(
                "/" + osName.substr(0, osName.size() - strlen("/.zgroup")));
        }
        else if (osName.size() > strlen("/.zarray") &&
                 osName.substr(osName.size() - strlen("/.zarray")) ==
                     "/.zarray")
        {
            auto osArrayFullname =
                osName.substr(0, osName.size() - strlen("/.zarray"));
            oMapArrays[osArrayFullname] = &child;
        }
    }

    const auto CreateArray = [this](const std::string &osArrayFullname,
                                    const CPLJSONObject &oArray,
                                    const CPLJSONObject &oAttributes)
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
        const std::string osZarrayFilename = CPLFormFilename(
            CPLFormFilename(poBelongingGroup->m_osDirectoryName.c_str(),
                            osArrayName.c_str(), nullptr),
            ".zarray", nullptr);
        poBelongingGroup->LoadArray(osArrayName, osZarrayFilename, oArray, true,
                                    oAttributes);
    };

    struct ArrayDesc
    {
        std::string osArrayFullname{};
        const CPLJSONObject *poArray = nullptr;
        const CPLJSONObject *poAttrs = nullptr;
    };

    std::vector<ArrayDesc> aoRegularArrays;

    // Second pass to read attributes and create arrays that are indexing
    // variable
    for (const auto &child : children)
    {
        const std::string osName(child.GetName());
        if (osName.size() > strlen("/.zattrs") &&
            osName.substr(osName.size() - strlen("/.zattrs")) == "/.zattrs")
        {
            const auto osObjectFullnameNoLeadingSlash =
                osName.substr(0, osName.size() - strlen("/.zattrs"));
            auto poSubGroup = std::dynamic_pointer_cast<ZarrV2Group>(
                OpenGroupFromFullname('/' + osObjectFullnameNoLeadingSlash));
            if (poSubGroup)
            {
                poSubGroup->m_oAttrGroup.Init(child, m_bUpdatable);
            }
            else
            {
                auto oIter = oMapArrays.find(osObjectFullnameNoLeadingSlash);
                if (oIter != oMapArrays.end())
                {
                    const auto nLastSlashPos =
                        osObjectFullnameNoLeadingSlash.rfind('/');
                    const auto osArrayName =
                        (nLastSlashPos == std::string::npos)
                            ? osObjectFullnameNoLeadingSlash
                            : osObjectFullnameNoLeadingSlash.substr(
                                  nLastSlashPos + 1);
                    const auto arrayDimensions =
                        child["_ARRAY_DIMENSIONS"].ToArray();
                    if (arrayDimensions.IsValid() &&
                        arrayDimensions.Size() == 1 &&
                        arrayDimensions[0].ToString() == osArrayName)
                    {
                        CreateArray(osObjectFullnameNoLeadingSlash,
                                    *(oIter->second), child);
                        oMapArrays.erase(oIter);
                    }
                    else
                    {
                        ArrayDesc desc;
                        desc.osArrayFullname = osObjectFullnameNoLeadingSlash;
                        desc.poArray = oIter->second;
                        desc.poAttrs = &child;
                        aoRegularArrays.emplace_back(std::move(desc));
                    }
                }
            }
        }
    }

    // Third pass to create non-indexing arrays with attributes
    for (const auto &desc : aoRegularArrays)
    {
        CreateArray(desc.osArrayFullname, *(desc.poArray), *(desc.poAttrs));
        oMapArrays.erase(desc.osArrayFullname);
    }

    // Fourth pass to create arrays without attributes
    for (const auto &kv : oMapArrays)
    {
        CreateArray(kv.first, *(kv.second), CPLJSONObject());
    }
}

/************************************************************************/
/*                   ZarrV2Group::InitFromZGroup()                      */
/************************************************************************/

bool ZarrV2Group::InitFromZGroup(const CPLJSONObject &obj)
{
    // Parse potential NCZarr (V2) extensions:
    // https://www.unidata.ucar.edu/software/netcdf/documentation/NUG/nczarr_head.html
    const auto nczarrGroup = obj["_NCZARR_GROUP"];
    if (nczarrGroup.GetType() == CPLJSONObject::Type::Object)
    {
        if (m_bUpdatable)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update of NCZarr datasets is not supported");
            return false;
        }
        m_bDirectoryExplored = true;

        // If not opening from the root of the dataset, walk up to it
        if (!obj["_NCZARR_SUPERBLOCK"].IsValid() &&
            m_poParent.lock() == nullptr)
        {
            const std::string osParentGroupFilename(CPLFormFilename(
                CPLGetPath(m_osDirectoryName.c_str()), ".zgroup", nullptr));
            VSIStatBufL sStat;
            if (VSIStatL(osParentGroupFilename.c_str(), &sStat) == 0)
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(osParentGroupFilename))
                {
                    auto poParent = ZarrV2Group::Create(
                        m_poSharedResource, std::string(), std::string());
                    poParent->m_bDirectoryExplored = true;
                    poParent->SetDirectoryName(
                        CPLGetPath(m_osDirectoryName.c_str()));
                    poParent->InitFromZGroup(oDoc.GetRoot());
                    m_poParentStrongRef = poParent;
                    m_poParent = poParent;

                    // Patch our name and fullname
                    m_osName = CPLGetFilename(m_osDirectoryName.c_str());
                    m_osFullName =
                        poParent->GetFullName() == "/"
                            ? m_osName
                            : poParent->GetFullName() + "/" + m_osName;
                }
            }
        }

        const auto IsValidName = [](const std::string &s)
        {
            return !s.empty() && s != "." && s != ".." &&
                   s.find("/") == std::string::npos &&
                   s.find("\\") == std::string::npos;
        };

        // Create dimensions first, as they will be potentially patched
        // by the OpenMDArray() later
        const auto dims = nczarrGroup["dims"];
        for (const auto &jDim : dims.GetChildren())
        {
            const auto osName = jDim.GetName();
            const GUInt64 nSize = jDim.ToLong();
            if (!IsValidName(osName))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid dimension name for %s", osName.c_str());
            }
            else if (nSize == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid dimension size for %s", osName.c_str());
            }
            else
            {
                CreateDimension(osName,
                                std::string(),  // type
                                std::string(),  // direction,
                                nSize, nullptr);
            }
        }

        const auto vars = nczarrGroup["vars"].ToArray();
        // open first indexing variables
        std::set<std::string> oSetIndexingArrayNames;
        for (const auto &var : vars)
        {
            const auto osVarName = var.ToString();
            if (IsValidName(osVarName) &&
                m_oMapDimensions.find(osVarName) != m_oMapDimensions.end() &&
                m_oMapMDArrays.find(osVarName) == m_oMapMDArrays.end() &&
                oSetIndexingArrayNames.find(osVarName) ==
                    oSetIndexingArrayNames.end())
            {
                oSetIndexingArrayNames.insert(osVarName);
                OpenMDArray(osVarName);
            }
        }

        // add regular arrays
        std::set<std::string> oSetRegularArrayNames;
        for (const auto &var : vars)
        {
            const auto osVarName = var.ToString();
            if (IsValidName(osVarName) &&
                m_oMapDimensions.find(osVarName) == m_oMapDimensions.end() &&
                m_oMapMDArrays.find(osVarName) == m_oMapMDArrays.end() &&
                oSetRegularArrayNames.find(osVarName) ==
                    oSetRegularArrayNames.end())
            {
                oSetRegularArrayNames.insert(osVarName);
                m_aosArrays.emplace_back(osVarName);
            }
        }

        // Finally list groups
        std::set<std::string> oSetGroups;
        const auto groups = nczarrGroup["groups"].ToArray();
        for (const auto &group : groups)
        {
            const auto osGroupName = group.ToString();
            if (IsValidName(osGroupName) &&
                oSetGroups.find(osGroupName) == oSetGroups.end())
            {
                oSetGroups.insert(osGroupName);
                m_aosGroups.emplace_back(osGroupName);
            }
        }
    }
    return true;
}

/************************************************************************/
/*                   ZarrV2Group::CreateOnDisk()                        */
/************************************************************************/

std::shared_ptr<ZarrV2Group> ZarrV2Group::CreateOnDisk(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
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

    const std::string osZgroupFilename(
        CPLFormFilename(osDirectoryName.c_str(), ".zgroup", nullptr));
    VSILFILE *fp = VSIFOpenL(osZgroupFilename.c_str(), "wb");
    if (!fp)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                 osZgroupFilename.c_str());
        return nullptr;
    }
    VSIFPrintfL(fp, "{\n  \"zarr_format\": 2\n}\n");
    VSIFCloseL(fp);

    auto poGroup = ZarrV2Group::Create(poSharedResource, osParentName, osName);
    poGroup->SetDirectoryName(osDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;

    CPLJSONObject oObj;
    oObj.Add("zarr_format", 2);
    poSharedResource->SetZMetadataItem(osZgroupFilename, oObj);

    return poGroup;
}

/************************************************************************/
/*                      ZarrV2Group::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup>
ZarrV2Group::CreateGroup(const std::string &osName,
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

    if (std::find(m_aosGroups.begin(), m_aosGroups.end(), osName) !=
        m_aosGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }

    const std::string osDirectoryName =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
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
/*                          FillDTypeElts()                             */
/************************************************************************/

static CPLJSONObject FillDTypeElts(const GDALExtendedDataType &oDataType,
                                   size_t nGDALStartOffset,
                                   std::vector<DtypeElt> &aoDtypeElts,
                                   bool bUseUnicode)
{
    CPLJSONObject dtype;
    const auto eClass = oDataType.GetClass();
    const size_t nNativeStartOffset =
        aoDtypeElts.empty()
            ? 0
            : aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize;
    const std::string dummy("dummy");

    switch (eClass)
    {
        case GEDTC_STRING:
        {
            if (oDataType.GetMaxStringLength() == 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "String arrays of unlimited size are not supported");
                dtype = CPLJSONObject();
                dtype.Deinit();
                return dtype;
            }
            DtypeElt elt;
            elt.nativeOffset = nNativeStartOffset;
            if (bUseUnicode)
            {
                elt.nativeType = DtypeElt::NativeType::STRING_UNICODE;
                elt.nativeSize = oDataType.GetMaxStringLength() * 4;
#ifdef CPL_MSB
                elt.needByteSwapping = true;
#endif
                dtype.Set(
                    dummy,
                    CPLSPrintf("<U%d", static_cast<int>(
                                           oDataType.GetMaxStringLength())));
            }
            else
            {
                elt.nativeType = DtypeElt::NativeType::STRING_ASCII;
                elt.nativeSize = oDataType.GetMaxStringLength();
                dtype.Set(
                    dummy,
                    CPLSPrintf("|S%d", static_cast<int>(
                                           oDataType.GetMaxStringLength())));
            }
            elt.gdalOffset = nGDALStartOffset;
            elt.gdalSize = sizeof(char *);
            aoDtypeElts.emplace_back(elt);
            break;
        }

        case GEDTC_NUMERIC:
        {
            const auto eDT = oDataType.GetNumericDataType();
            DtypeElt elt;
            bool bUnsupported = false;
            switch (eDT)
            {
                case GDT_Byte:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, "|u1");
                    break;
                }
                case GDT_Int8:
                {
                    elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                    dtype.Set(dummy, "|i1");
                    break;
                }
                case GDT_UInt16:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, "<u2");
                    break;
                }
                case GDT_Int16:
                {
                    elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                    dtype.Set(dummy, "<i2");
                    break;
                }
                case GDT_UInt32:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, "<u4");
                    break;
                }
                case GDT_Int32:
                {
                    elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                    dtype.Set(dummy, "<i4");
                    break;
                }
                case GDT_UInt64:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, "<u8");
                    break;
                }
                case GDT_Int64:
                {
                    elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                    dtype.Set(dummy, "<i8");
                    break;
                }
                case GDT_Float32:
                {
                    elt.nativeType = DtypeElt::NativeType::IEEEFP;
                    dtype.Set(dummy, "<f4");
                    break;
                }
                case GDT_Float64:
                {
                    elt.nativeType = DtypeElt::NativeType::IEEEFP;
                    dtype.Set(dummy, "<f8");
                    break;
                }
                case GDT_Unknown:
                case GDT_CInt16:
                case GDT_CInt32:
                {
                    bUnsupported = true;
                    break;
                }
                case GDT_CFloat32:
                {
                    elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                    dtype.Set(dummy, "<c8");
                    break;
                }
                case GDT_CFloat64:
                {
                    elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                    dtype.Set(dummy, "<c16");
                    break;
                }
                case GDT_TypeCount:
                {
                    static_assert(GDT_TypeCount == GDT_Int8 + 1,
                                  "GDT_TypeCount == GDT_Int8 + 1");
                    break;
                }
            }
            if (bUnsupported)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported data type: %s", GDALGetDataTypeName(eDT));
                dtype = CPLJSONObject();
                dtype.Deinit();
                return dtype;
            }
            elt.nativeOffset = nNativeStartOffset;
            elt.nativeSize = GDALGetDataTypeSizeBytes(eDT);
            elt.gdalOffset = nGDALStartOffset;
            elt.gdalSize = elt.nativeSize;
#ifdef CPL_MSB
            elt.needByteSwapping = elt.nativeSize > 1;
#endif
            aoDtypeElts.emplace_back(elt);
            break;
        }

        case GEDTC_COMPOUND:
        {
            const auto &comps = oDataType.GetComponents();
            CPLJSONArray array;
            for (const auto &comp : comps)
            {
                CPLJSONArray subArray;
                subArray.Add(comp->GetName());
                const auto subdtype = FillDTypeElts(
                    comp->GetType(), nGDALStartOffset + comp->GetOffset(),
                    aoDtypeElts, bUseUnicode);
                if (!subdtype.IsValid())
                {
                    dtype = CPLJSONObject();
                    dtype.Deinit();
                    return dtype;
                }
                if (subdtype.GetType() == CPLJSONObject::Type::Object)
                    subArray.Add(subdtype["dummy"]);
                else
                    subArray.Add(subdtype);
                array.Add(subArray);
            }
            dtype = std::move(array);
            break;
        }
    }
    return dtype;
}

/************************************************************************/
/*                     ZarrV2Group::CreateMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrV2Group::CreateMDArray(
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

    std::vector<DtypeElt> aoDtypeElts;
    const bool bUseUnicode =
        EQUAL(CSLFetchNameValueDef(papszOptions, "STRING_FORMAT", "ASCII"),
              "UNICODE");
    const auto dtype = FillDTypeElts(oDataType, 0, aoDtypeElts, bUseUnicode);
    if (!dtype.IsValid() || aoDtypeElts.empty())
        return nullptr;

    GetMDArrayNames();

    if (std::find(m_aosArrays.begin(), m_aosArrays.end(), osName) !=
        m_aosArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    CPLJSONObject oCompressor;
    oCompressor.Deinit();
    const char *pszCompressor =
        CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    const CPLCompressor *psCompressor = nullptr;
    const CPLCompressor *psDecompressor = nullptr;
    if (!EQUAL(pszCompressor, "NONE"))
    {
        psCompressor = CPLGetCompressor(pszCompressor);
        psDecompressor = CPLGetCompressor(pszCompressor);
        if (psCompressor == nullptr || psDecompressor == nullptr)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for %s not available",
                     pszCompressor);
            return nullptr;
        }
        const char *pszOptions =
            CSLFetchNameValue(psCompressor->papszMetadata, "OPTIONS");
        if (pszOptions)
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot =
                oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if (psRoot)
            {
                for (const CPLXMLNode *psNode = psRoot->psChild;
                     psNode != nullptr; psNode = psNode->psNext)
                {
                    if (psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0)
                    {
                        const char *pszName =
                            CPLGetXMLValue(psNode, "name", nullptr);
                        const char *pszType =
                            CPLGetXMLValue(psNode, "type", nullptr);
                        if (pszName && pszType)
                        {
                            const char *pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszCompressor) + '_' + pszName)
                                    .c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if (pszVal)
                            {
                                if (EQUAL(pszName, "SHUFFLE") &&
                                    EQUAL(pszVal, "BYTE"))
                                {
                                    pszVal = "1";
                                    pszType = "integer";
                                }

                                if (!oCompressor.IsValid())
                                {
                                    oCompressor = CPLJSONObject();
                                    oCompressor.Add(
                                        "id",
                                        CPLString(pszCompressor).tolower());
                                }

                                std::string osOptName(
                                    CPLString(pszName).tolower());
                                if (STARTS_WITH(pszType, "int"))
                                    oCompressor.Add(osOptName, atoi(pszVal));
                                else
                                    oCompressor.Add(osOptName, pszVal);
                            }
                        }
                    }
                }
            }
        }
    }

    CPLJSONArray oFilters;
    const char *pszFilter =
        CSLFetchNameValueDef(papszOptions, "FILTER", "NONE");
    if (!EQUAL(pszFilter, "NONE"))
    {
        const auto psFilterCompressor = CPLGetCompressor(pszFilter);
        const auto psFilterDecompressor = CPLGetCompressor(pszFilter);
        if (psFilterCompressor == nullptr || psFilterDecompressor == nullptr)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for filter %s not available",
                     pszFilter);
            return nullptr;
        }

        CPLJSONObject oFilter;
        oFilter.Add("id", CPLString(pszFilter).tolower());
        oFilters.Add(oFilter);

        const char *pszOptions =
            CSLFetchNameValue(psFilterCompressor->papszMetadata, "OPTIONS");
        if (pszOptions)
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot =
                oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if (psRoot)
            {
                for (const CPLXMLNode *psNode = psRoot->psChild;
                     psNode != nullptr; psNode = psNode->psNext)
                {
                    if (psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0)
                    {
                        const char *pszName =
                            CPLGetXMLValue(psNode, "name", nullptr);
                        const char *pszType =
                            CPLGetXMLValue(psNode, "type", nullptr);
                        if (pszName && pszType)
                        {
                            const char *pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszFilter) + '_' + pszName)
                                    .c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if (pszVal)
                            {
                                std::string osOptName(
                                    CPLString(pszName).tolower());
                                if (STARTS_WITH(pszType, "int"))
                                    oFilter.Add(osOptName, atoi(pszVal));
                                else
                                    oFilter.Add(osOptName, pszVal);
                            }
                        }
                    }
                }
            }
        }

        if (EQUAL(pszFilter, "delta") &&
            CSLFetchNameValue(papszOptions, "DELTA_DTYPE") == nullptr)
        {
            if (oDataType.GetClass() != GEDTC_NUMERIC)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "DELTA_DTYPE option must be specified");
                return nullptr;
            }
            switch (oDataType.GetNumericDataType())
            {
                case GDT_Unknown:
                    break;
                case GDT_Byte:
                    oFilter.Add("dtype", "u1");
                    break;
                case GDT_Int8:
                    oFilter.Add("dtype", "i1");
                    break;
                case GDT_UInt16:
                    oFilter.Add("dtype", "<u2");
                    break;
                case GDT_Int16:
                    oFilter.Add("dtype", "<i2");
                    break;
                case GDT_UInt32:
                    oFilter.Add("dtype", "<u4");
                    break;
                case GDT_Int32:
                    oFilter.Add("dtype", "<i4");
                    break;
                case GDT_UInt64:
                    oFilter.Add("dtype", "<u8");
                    break;
                case GDT_Int64:
                    oFilter.Add("dtype", "<i8");
                    break;
                case GDT_Float32:
                    oFilter.Add("dtype", "<f4");
                    break;
                case GDT_Float64:
                    oFilter.Add("dtype", "<f8");
                    break;
                case GDT_CInt16:
                    oFilter.Add("dtype", "<i2");
                    break;
                case GDT_CInt32:
                    oFilter.Add("dtype", "<i4");
                    break;
                case GDT_CFloat32:
                    oFilter.Add("dtype", "<f4");
                    break;
                case GDT_CFloat64:
                    oFilter.Add("dtype", "<f8");
                    break;
                case GDT_TypeCount:
                    break;
            }
        }
    }

    const std::string osZarrayDirectory =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if (VSIMkdir(osZarrayDirectory.c_str(), 0755) != 0)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osZarrayDirectory.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                     osZarrayDirectory.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osZarrayDirectory.c_str());
        }
        return nullptr;
    }

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::FillBlockSize(aoDimensions, oDataType, anBlockSize,
                                  papszOptions))
        return nullptr;

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");

    const char *pszDimSeparator =
        CSLFetchNameValueDef(papszOptions, "DIM_SEPARATOR", ".");

    auto poArray = ZarrV2Array::Create(m_poSharedResource, GetFullName(),
                                       osName, aoDimensions, oDataType,
                                       aoDtypeElts, anBlockSize, bFortranOrder);

    if (!poArray)
        return nullptr;
    const std::string osZarrayFilename =
        CPLFormFilename(osZarrayDirectory.c_str(), ".zarray", nullptr);
    poArray->SetNew(true);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetDtype(dtype);
    poArray->SetCompressorDecompressor(pszCompressor, psCompressor,
                                       psDecompressor);
    if (oCompressor.IsValid())
        poArray->SetCompressorJson(oCompressor);
    poArray->SetFilters(oFilters);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    poArray->Flush();
    RegisterArray(poArray);

    return poArray;
}
