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
/*                          ~ZarrGroupBase()                            */
/************************************************************************/

ZarrGroupBase::~ZarrGroupBase()
{
    // We need to explicitly flush arrays so that the _ARRAY_DIMENSIONS
    // is properly written. As it relies on checking if the dimensions of the
    // array have an indexing variable, then still need to be all alive.
    for( auto& kv: m_oMapMDArrays )
    {
        kv.second->Flush();
    }
}

/************************************************************************/
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetMDArrayNames(CSLConstList) const
{
    if( !m_bDirectoryExplored )
        ExploreDirectory();

    return m_aosArrays;
}

/************************************************************************/
/*                            RegisterArray()                           */
/************************************************************************/

void ZarrGroupBase::RegisterArray(const std::shared_ptr<ZarrArray>& array) const
{
    m_oMapMDArrays[array->GetName()] = array;
    m_aosArrays.emplace_back(array->GetName());
    array->RegisterGroup(m_pSelf);
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetGroupNames(CSLConstList) const
{
    if( !m_bDirectoryExplored )
        ExploreDirectory();

    return m_aosGroups;
}

/************************************************************************/
/*                  ZarrGroupBase::CreateAttribute()                    */
/************************************************************************/

std::shared_ptr<GDALAttribute> ZarrGroupBase::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( anDimensions.size() >= 2 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create attributes of dimension >= 2");
        return nullptr;
    }
    LoadAttributes();
    return m_oAttrGroup.CreateAttribute(osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> ZarrGroupBase::GetDimensions(CSLConstList) const
{
    if( !m_bReadFromZMetadata && !m_bDimensionsInstantiated )
    {
        m_bDimensionsInstantiated = true;
        // We need to instantiate arrays to discover dimensions
        const auto aosArrays = GetMDArrayNames();
        for( const auto& osArray: aosArrays )
        {
            OpenMDArray(osArray);
        }
    }

    std::vector<std::shared_ptr<GDALDimension>> oRes;
    for( const auto& oIter: m_oMapDimensions )
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension> ZarrGroupBase::CreateDimension(const std::string& osName,
                                                         const std::string& osType,
                                                         const std::string& osDirection,
                                                         GUInt64 nSize,
                                                         CSLConstList)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty dimension name not supported");
        return nullptr;
    }
    GetDimensions(nullptr);

    if( m_oMapDimensions.find(osName) != m_oMapDimensions.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dimension with same name already exists");
        return nullptr;
    }
    auto newDim(std::make_shared<GDALDimensionWeakIndexingVar>(
                    GetFullName(), osName, osType, osDirection, nSize));
    m_oMapDimensions[osName] = newDim;
    return newDim;
}

/************************************************************************/
/*                      ZarrGroupV2::Create()                           */
/************************************************************************/

std::shared_ptr<ZarrGroupV2> ZarrGroupV2::Create(
                const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                const std::string& osParentName,
                const std::string& osName)
{
    auto poGroup = std::shared_ptr<ZarrGroupV2>(
        new ZarrGroupV2(poSharedResource, osParentName, osName));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                      ZarrGroupV2::~ZarrGroupV2()                     */
/************************************************************************/

ZarrGroupV2::~ZarrGroupV2()
{
    if( m_oAttrGroup.IsModified() )
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

void ZarrGroupV2::ExploreDirectory() const
{
    if( m_bDirectoryExplored || m_osDirectoryName.empty() )
        return;
    m_bDirectoryExplored = true;

    const CPLStringList aosFiles(VSIReadDir(m_osDirectoryName.c_str()));
    // If the directory contains a .zarray, no need to recurse.
    for( int i = 0; i < aosFiles.size(); ++i )
    {
        if( strcmp(aosFiles[i], ".zarray") == 0 )
            return;
    }

    for( int i = 0; i < aosFiles.size(); ++i )
    {
        if( strcmp(aosFiles[i], ".") != 0 &&
            strcmp(aosFiles[i], "..") != 0 &&
            strcmp(aosFiles[i], ".zgroup") != 0 &&
            strcmp(aosFiles[i], ".zattrs") != 0 )
        {
            const std::string osSubDir =
                CPLFormFilename(m_osDirectoryName.c_str(), aosFiles[i], nullptr);
            VSIStatBufL sStat;
            std::string osFilename =
                CPLFormFilename(osSubDir.c_str(), ".zarray", nullptr);
            if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
                m_aosArrays.emplace_back(aosFiles[i]);
            else
            {
                osFilename =
                    CPLFormFilename(osSubDir.c_str(), ".zgroup", nullptr);
                if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
                    m_aosGroups.emplace_back(aosFiles[i]);
            }
        }
    }
}

/************************************************************************/
/*                             OpenMDArray()                            */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrGroupV2::OpenMDArray(const std::string& osName,
                                                   CSLConstList) const
{
    auto oIter = m_oMapMDArrays.find(osName);
    if( oIter != m_oMapMDArrays.end() )
        return oIter->second;

    if( !m_bReadFromZMetadata && !m_osDirectoryName.empty() )
    {
        const std::string osSubDir =
            CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
        VSIStatBufL sStat;
        const std::string osZarrayFilename = CPLFormFilename(
            osSubDir.c_str(), ".zarray", nullptr);
        if( VSIStatL(osZarrayFilename.c_str(), &sStat) == 0 )
        {
            CPLJSONDocument oDoc;
            if( !oDoc.Load(osZarrayFilename) )
                return nullptr;
            const auto oRoot = oDoc.GetRoot();
            std::set<std::string> oSetFilenamesInLoading;
            return LoadArray(osName, osZarrayFilename, oRoot, false,
                             CPLJSONObject(), oSetFilenamesInLoading);
        }
    }

    return nullptr;
}

/************************************************************************/
/*                              OpenGroup()                             */
/************************************************************************/

std::shared_ptr<GDALGroup> ZarrGroupV2::OpenGroup(const std::string& osName,
                                               CSLConstList) const
{
    auto oIter = m_oMapGroups.find(osName);
    if( oIter != m_oMapGroups.end() )
        return oIter->second;

    if( !m_bReadFromZMetadata && !m_osDirectoryName.empty() )
    {
        const std::string osSubDir =
            CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
        VSIStatBufL sStat;
        const std::string osZgroupFilename = CPLFormFilename(
            osSubDir.c_str(), ".zgroup", nullptr);
        if( VSIStatL(osZgroupFilename.c_str(), &sStat) == 0 )
        {
            CPLJSONDocument oDoc;
            if( !oDoc.Load(osZgroupFilename) )
                return nullptr;

            auto poSubGroup = ZarrGroupV2::Create(m_poSharedResource,
                                                  GetFullName(), osName);
            poSubGroup->m_poParent = m_pSelf;
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
/*                   ZarrGroupV2::LoadAttributes()                      */
/************************************************************************/

void ZarrGroupV2::LoadAttributes() const
{
    if( m_bAttributesLoaded || m_osDirectoryName.empty() )
        return;
    m_bAttributesLoaded = true;

    CPLJSONDocument oDoc;
    const std::string osZattrsFilename(
        CPLFormFilename(m_osDirectoryName.c_str(), ".zattrs", nullptr));
    CPLErrorHandlerPusher quietError(CPLQuietErrorHandler);
    CPLErrorStateBackuper errorStateBackuper;
    if( !oDoc.Load(osZattrsFilename) )
        return;
    auto oRoot = oDoc.GetRoot();
    m_oAttrGroup.Init(oRoot, m_bUpdatable);
}

/************************************************************************/
/*                      ZarrGroupV3::Create()                           */
/************************************************************************/

std::shared_ptr<ZarrGroupV3> ZarrGroupV3::Create(
                const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                const std::string& osParentName,
                const std::string& osName,
                const std::string& osRootDirectoryName)
{
    auto poGroup = std::shared_ptr<ZarrGroupV3>(
        new ZarrGroupV3(poSharedResource, osParentName, osName, osRootDirectoryName));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                             OpenMDArray()                            */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrGroupV3::OpenMDArray(const std::string& osName,
                                                   CSLConstList) const
{
    auto oIter = m_oMapMDArrays.find(osName);
    if( oIter != m_oMapMDArrays.end() )
        return oIter->second;

    std::string osFilenamePrefix =
        m_osDirectoryName + "/meta/root";
    if( !(GetFullName() == "/" && osName == "/" ) )
    {
        osFilenamePrefix += GetFullName();
        if( GetFullName() != "/" )
            osFilenamePrefix += '/';
        osFilenamePrefix += osName;
    }

    std::string osFilename(osFilenamePrefix);
    osFilename += ".array.json";

    VSIStatBufL sStat;
    if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osFilename) )
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        std::set<std::string> oSetFilenamesInLoading;
        return LoadArray(osName, osFilename, oRoot, false,
                         CPLJSONObject(), oSetFilenamesInLoading);
    }

    return nullptr;
}

/************************************************************************/
/*                   ZarrGroupV3::LoadAttributes()                      */
/************************************************************************/

void ZarrGroupV3::LoadAttributes() const
{
    if( m_bAttributesLoaded )
        return;
    m_bAttributesLoaded = true;

    std::string osFilename = m_osDirectoryName + "/meta/root";
    if( GetFullName() != "/" )
        osFilename += GetFullName();
    osFilename += ".group.json";

    VSIStatBufL sStat;
    if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osFilename) )
            return;
        auto oRoot = oDoc.GetRoot();
        m_oAttrGroup.Init(oRoot["attributes"], m_bUpdatable);
    }
}

/************************************************************************/
/*                   ZarrGroupV2::GetOrCreateSubGroup()                 */
/************************************************************************/

std::shared_ptr<ZarrGroupV2> ZarrGroupV2::GetOrCreateSubGroup(
                                        const std::string& osSubGroupFullname)
{
    auto poSubGroup = std::dynamic_pointer_cast<ZarrGroupV2>(
                                OpenGroupFromFullname(osSubGroupFullname));
    if( poSubGroup )
    {
        return poSubGroup;
    }

    const auto nLastSlashPos = osSubGroupFullname.rfind('/');
    auto poBelongingGroup = (nLastSlashPos == 0) ?
        this :
        GetOrCreateSubGroup(osSubGroupFullname.substr(0, nLastSlashPos)).get();

    poSubGroup = ZarrGroupV2::Create(
            m_poSharedResource,
            poBelongingGroup->GetFullName(),
            osSubGroupFullname.substr(nLastSlashPos + 1));
    poSubGroup->m_poParent = poBelongingGroup->m_pSelf;
    poSubGroup->SetDirectoryName(CPLFormFilename(
        poBelongingGroup->m_osDirectoryName.c_str(),
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
/*                   ZarrGroupV2::InitFromZMetadata()                   */
/************************************************************************/

void ZarrGroupV2::InitFromZMetadata(const CPLJSONObject& obj)
{
    m_bDirectoryExplored = true;
    m_bAttributesLoaded = true;
    m_bReadFromZMetadata = true;

    const auto metadata = obj["metadata"];
    if( metadata.GetType() != CPLJSONObject::Type::Object )
        return;
    const auto children = metadata.GetChildren();
    std::map<std::string, const CPLJSONObject*> oMapArrays;

    // First pass to create groups and collect arrays
    for( const auto& child: children )
    {
        const std::string osName(child.GetName());
        if( std::count(osName.begin(), osName.end(), '/') > 32 )
        {
            // Avoid too deep recursion in GetOrCreateSubGroup()
            continue;
        }
        if( osName == ".zattrs" )
        {
            m_oAttrGroup.Init(child, m_bUpdatable);
        }
        else if( osName.size() > strlen("/.zgroup") &&
                 osName.substr(osName.size() - strlen("/.zgroup")) == "/.zgroup" )
        {
            GetOrCreateSubGroup("/" + osName.substr(0, osName.size() - strlen("/.zgroup")));
        }
        else if( osName.size() > strlen("/.zarray") &&
                 osName.substr(osName.size() - strlen("/.zarray")) == "/.zarray" )
        {
            auto osArrayFullname = osName.substr(0, osName.size() - strlen("/.zarray"));
            oMapArrays[osArrayFullname] = &child;
        }
    }

    const auto CreateArray = [this](const std::string& osArrayFullname,
                                    const CPLJSONObject& oArray,
                                    const CPLJSONObject& oAttributes)
    {
        const auto nLastSlashPos = osArrayFullname.rfind('/');
        auto poBelongingGroup =
            (nLastSlashPos == std::string::npos) ? this:
            GetOrCreateSubGroup("/" + osArrayFullname.substr(0, nLastSlashPos)).get();
        const auto osArrayName = nLastSlashPos == std::string::npos ?
            osArrayFullname :
            osArrayFullname.substr(nLastSlashPos + 1);
        const std::string osZarrayFilename =
            CPLFormFilename(
                CPLFormFilename(poBelongingGroup->m_osDirectoryName.c_str(),
                                osArrayName.c_str(), nullptr),
                ".zarray",
                nullptr);
        std::set<std::string> oSetFilenamesInLoading;
        poBelongingGroup->LoadArray(
                    osArrayName, osZarrayFilename, oArray, true,
                    oAttributes, oSetFilenamesInLoading);
    };

    struct ArrayDesc
    {
        std::string osArrayFullname{};
        const CPLJSONObject* poArray = nullptr;
        const CPLJSONObject* poAttrs = nullptr;
    };
    std::vector<ArrayDesc> aoRegularArrays;

    // Second pass to read attributes and create arrays that are indexing
    // variable
    for( const auto& child: children )
    {
        const std::string osName(child.GetName());
        if( osName.size() > strlen("/.zattrs") &&
            osName.substr(osName.size() - strlen("/.zattrs")) == "/.zattrs" )
        {
            const auto osObjectFullnameNoLeadingSlash =
                osName.substr(0, osName.size() - strlen("/.zattrs"));
            auto poSubGroup = std::dynamic_pointer_cast<ZarrGroupV2>(
                    OpenGroupFromFullname('/'  + osObjectFullnameNoLeadingSlash));
            if( poSubGroup )
            {
                poSubGroup->m_oAttrGroup.Init(child, m_bUpdatable);
            }
            else
            {
                auto oIter = oMapArrays.find(osObjectFullnameNoLeadingSlash);
                if( oIter != oMapArrays.end() )
                {
                    const auto nLastSlashPos = osObjectFullnameNoLeadingSlash.rfind('/');
                    const auto osArrayName = (nLastSlashPos == std::string::npos) ?
                        osObjectFullnameNoLeadingSlash :
                        osObjectFullnameNoLeadingSlash.substr(nLastSlashPos + 1);
                    const auto arrayDimensions = child["_ARRAY_DIMENSIONS"].ToArray();
                    if( arrayDimensions.IsValid() && arrayDimensions.Size() == 1 &&
                        arrayDimensions[0].ToString() == osArrayName )
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
    for( const auto& desc: aoRegularArrays )
    {
        CreateArray(desc.osArrayFullname, *(desc.poArray), *(desc.poAttrs));
        oMapArrays.erase(desc.osArrayFullname);
    }

    // Fourth pass to create arrays without attributes
    for( const auto& kv: oMapArrays )
    {
        CreateArray(kv.first, *(kv.second), CPLJSONObject());
    }
}

/************************************************************************/
/*                   ZarrGroupV2::InitFromZGroup()                      */
/************************************************************************/

bool ZarrGroupV2::InitFromZGroup(const CPLJSONObject& obj)
{
    // Parse potential NCZarr (V2) extensions:
    // https://www.unidata.ucar.edu/software/netcdf/documentation/NUG/nczarr_head.html
    const auto nczarrGroup = obj["_NCZARR_GROUP"];
    if( nczarrGroup.GetType() == CPLJSONObject::Type::Object )
    {
        if( m_bUpdatable )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update of NCZarr datasets is not supported");
            return false;
        }
        m_bDirectoryExplored = true;

        // If not opening from the root of the dataset, walk up to it
        if( !obj["_NCZARR_SUPERBLOCK"].IsValid() &&
            m_poParent.lock() == nullptr )
        {
            const std::string osParentGroupFilename(
                    CPLFormFilename(CPLGetPath(m_osDirectoryName.c_str()),
                                    ".zgroup", nullptr));
            VSIStatBufL sStat;
            if( VSIStatL( osParentGroupFilename.c_str(), &sStat ) == 0 )
            {
                CPLJSONDocument oDoc;
                if( oDoc.Load(osParentGroupFilename) )
                {
                    auto poParent = ZarrGroupV2::Create(
                        m_poSharedResource, std::string(), std::string());
                    poParent->m_bDirectoryExplored = true;
                    poParent->SetDirectoryName(CPLGetPath(m_osDirectoryName.c_str()));
                    poParent->InitFromZGroup(oDoc.GetRoot());
                    m_poParentStrongRef = poParent;
                    m_poParent = poParent;

                    // Patch our name and fullname
                    m_osName = CPLGetFilename(m_osDirectoryName.c_str());
                    m_osFullName = poParent->GetFullName() == "/" ? m_osName :
                        poParent->GetFullName() + "/" + m_osName;
                }
            }
        }

        const auto IsValidName = [](const std::string& s)
        {
            return !s.empty() &&
                   s != "." &&
                   s != ".." &&
                   s.find("/") == std::string::npos &&
                   s.find("\\") == std::string::npos;
        };

        // Create dimensions first, as they will be potentially patched
        // by the OpenMDArray() later
        const auto dims = nczarrGroup["dims"];
        for( const auto& jDim: dims.GetChildren() )
        {
            const auto osName = jDim.GetName();
            const GUInt64 nSize = jDim.ToLong();
            if( !IsValidName(osName) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid dimension name for %s",
                         osName.c_str());
            }
            else if( nSize == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid dimension size for %s",
                         osName.c_str());
            }
            else
            {
                CreateDimension(osName,
                                std::string(), // type
                                std::string(), // direction,
                                nSize, nullptr);
            }
        }

        const auto vars = nczarrGroup["vars"].ToArray();
        // open first indexing variables
        std::set<std::string> oSetIndexingArrayNames;
        for( const auto& var: vars )
        {
            const auto osVarName = var.ToString();
            if( IsValidName(osVarName) &&
                m_oMapDimensions.find(osVarName) != m_oMapDimensions.end() &&
                m_oMapMDArrays.find(osVarName) == m_oMapMDArrays.end() &&
                oSetIndexingArrayNames.find(osVarName) == oSetIndexingArrayNames.end() )
            {
                oSetIndexingArrayNames.insert(osVarName);
                OpenMDArray(osVarName);
            }
        }

        // add regular arrays
        std::set<std::string> oSetRegularArrayNames;
        for( const auto& var: vars )
        {
            const auto osVarName = var.ToString();
            if( IsValidName(osVarName) &&
                m_oMapDimensions.find(osVarName) == m_oMapDimensions.end() &&
                m_oMapMDArrays.find(osVarName) == m_oMapMDArrays.end() &&
                oSetRegularArrayNames.find(osVarName) == oSetRegularArrayNames.end() )
            {
                oSetRegularArrayNames.insert(osVarName);
                m_aosArrays.emplace_back(osVarName);
            }
        }

        // Finally list groups
        std::set<std::string> oSetGroups;
        const auto groups = nczarrGroup["groups"].ToArray();
        for( const auto& group: groups )
        {
            const auto osGroupName = group.ToString();
            if( IsValidName(osGroupName) &&
                oSetGroups.find(osGroupName) == oSetGroups.end() )
            {
                oSetGroups.insert(osGroupName);
                m_aosGroups.emplace_back(osGroupName);
            }
        }
    }
    return true;
}

/************************************************************************/
/*                   ZarrGroupV2::CreateOnDisk()                        */
/************************************************************************/

std::shared_ptr<ZarrGroupV2> ZarrGroupV2::CreateOnDisk(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                                       const std::string& osParentName,
                                                       const std::string& osName,
                                                       const std::string& osDirectoryName)
{
    if( VSIMkdir(osDirectoryName.c_str(), 0755) != 0 )
    {
        VSIStatBufL sStat;
        if( VSIStatL(osDirectoryName.c_str(), &sStat) == 0 )
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
    VSILFILE* fp = VSIFOpenL(osZgroupFilename.c_str(), "wb" );
    if( !fp )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                 osZgroupFilename.c_str());
        return nullptr;
    }
    VSIFPrintfL(fp, "{\n  \"zarr_format\": 2\n}\n");
    VSIFCloseL(fp);

    auto poGroup = ZarrGroupV2::Create(poSharedResource, osParentName, osName);
    poGroup->SetDirectoryName(osDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;

    CPLJSONObject oObj;
    oObj.Add("zarr_format", 2);
    poSharedResource->SetZMetadataItem(osZgroupFilename, oObj);

    return poGroup;
}

/************************************************************************/
/*                         IsValidObjectName()                          */
/************************************************************************/

static bool IsValidObjectName(const std::string& osName)
{
    return !( osName.empty() || osName == "." || osName == ".." ||
              osName.find('/') != std::string::npos ||
              osName.find('\\') != std::string::npos ||
              osName.find(':') != std::string::npos ||
              STARTS_WITH(osName.c_str(), ".z") );
}

/************************************************************************/
/*                      ZarrGroupV2::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup> ZarrGroupV2::CreateGroup(const std::string& osName,
                                                    CSLConstList /* papszOptions */)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( !IsValidObjectName(osName) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid group name");
        return nullptr;
    }

    GetGroupNames();

    if( m_oMapGroups.find(osName) != m_oMapGroups.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }
    const std::string osDirectoryName =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    auto poGroup = CreateOnDisk(m_poSharedResource,
                                GetFullName(), osName, osDirectoryName);
    if( !poGroup )
        return nullptr;
    m_oMapGroups[osName] = poGroup;
    m_aosGroups.emplace_back(osName);
    return poGroup;
}

/************************************************************************/
/*                          FillDTypeElts()                             */
/************************************************************************/

static CPLJSONObject FillDTypeElts(const GDALExtendedDataType& oDataType,
                                   size_t nGDALStartOffset,
                                   std::vector<DtypeElt>& aoDtypeElts,
                                   bool bZarrV2,
                                   bool bUseUnicode)
{
    CPLJSONObject dtype;
    const auto eClass = oDataType.GetClass();
    const size_t nNativeStartOffset = aoDtypeElts.empty() ? 0:
        aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize;
    const std::string dummy("dummy");

    switch( eClass )
    {
        case GEDTC_STRING:
        {
            if( oDataType.GetMaxStringLength() == 0 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "String arrays of unlimited size are not supported");
                dtype = CPLJSONObject();
                dtype.Deinit();
                return dtype;
            }
            DtypeElt elt;
            elt.nativeOffset = nNativeStartOffset;
            if( bUseUnicode )
            {
                elt.nativeType = DtypeElt::NativeType::STRING_UNICODE;
                elt.nativeSize = oDataType.GetMaxStringLength() * 4;
#ifdef CPL_MSB
                elt.needByteSwapping = true;
#endif
                dtype.Set(dummy, CPLSPrintf("<U%d", static_cast<int>(oDataType.GetMaxStringLength())));
            }
            else
            {
                elt.nativeType = DtypeElt::NativeType::STRING_ASCII;
                elt.nativeSize = oDataType.GetMaxStringLength();
                dtype.Set(dummy, CPLSPrintf("|S%d", static_cast<int>(oDataType.GetMaxStringLength())));
            }
            elt.gdalOffset = nGDALStartOffset;
            elt.gdalSize = sizeof(char*);
            aoDtypeElts.emplace_back(elt);
            break;
        }

        case GEDTC_NUMERIC:
        {
            const auto eDT = oDataType.GetNumericDataType();
            DtypeElt elt;
            bool bUnsupported = false;
            switch( eDT )
            {
                case GDT_Byte:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, bZarrV2 ? "|u1" : "u1");
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
                    if( !bZarrV2 )
                    {
                        bUnsupported = true;
                        break;
                    }
                    elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                    dtype.Set(dummy, "<c8");
                    break;
                }
                case GDT_CFloat64:
                {
                    if( !bZarrV2 )
                    {
                        bUnsupported = true;
                        break;
                    }
                    elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                    dtype.Set(dummy, "<c16");
                    break;
                }
                case GDT_TypeCount:
                {
                    static_assert(GDT_TypeCount == GDT_Int64 + 1, "GDT_TypeCount == GDT_Int64 + 1");
                    break;
                }
            }
            if( bUnsupported )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported data type: %s",
                         GDALGetDataTypeName(eDT));
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
            const auto& comps = oDataType.GetComponents();
            CPLJSONArray array;
            for( const auto& comp: comps )
            {
                CPLJSONArray subArray;
                subArray.Add(comp->GetName());
                const auto subdtype = FillDTypeElts(
                    comp->GetType(),
                    nGDALStartOffset + comp->GetOffset(),
                    aoDtypeElts,
                    bZarrV2,
                    bUseUnicode);
                if( !subdtype.IsValid() )
                {
                    dtype = CPLJSONObject();
                    dtype.Deinit();
                    return dtype;
                }
                if( subdtype.GetType() == CPLJSONObject::Type::Object )
                    subArray.Add(subdtype["dummy"]);
                else
                    subArray.Add(subdtype);
                array.Add(subArray);
            }
            dtype = array;
            break;
        }
    }
    return dtype;
}

/************************************************************************/
/*                          FillBlockSize()                             */
/************************************************************************/

static bool FillBlockSize(const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                          const GDALExtendedDataType& oDataType,
                          std::vector<GUInt64>& anBlockSize,
                          CSLConstList papszOptions)
{
    const auto nDims = aoDimensions.size();
    anBlockSize.resize(nDims);
    for( size_t i = 0; i < nDims; ++i )
        anBlockSize[i] = 1;
    if( nDims >= 2 )
    {
        anBlockSize[nDims-2] = std::min(std::max<GUInt64>(1, aoDimensions[nDims-2]->GetSize()),
                                        static_cast<GUInt64>(256));
        anBlockSize[nDims-1] = std::min(std::max<GUInt64>(1, aoDimensions[nDims-1]->GetSize()),
                                        static_cast<GUInt64>(256));
    }
    else if( nDims == 1 )
    {
        anBlockSize[0] = std::max<GUInt64>(1, aoDimensions[0]->GetSize());
    }

    const char* pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if( pszBlockSize )
    {
        const auto aszTokens(CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if( static_cast<size_t>(aszTokens.size()) != nDims )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return false;
        }
        size_t nBlockSize = oDataType.GetSize();
        for( size_t i = 0; i < nDims; ++i )
        {
            anBlockSize[i] = static_cast<GUInt64>(CPLAtoGIntBig(aszTokens[i]));
            if( anBlockSize[i] == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Values in BLOCKSIZE should be > 0");
                return false;
            }
            if( anBlockSize[i] > std::numeric_limits<size_t>::max() / nBlockSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large values in BLOCKSIZE");
                return false;
            }
            nBlockSize *= static_cast<size_t>(anBlockSize[i]);
        }
    }
    return true;
}

/************************************************************************/
/*                     ZarrGroupV2::CreateMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrGroupV2::CreateMDArray(
            const std::string& osName,
            const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
            const GDALExtendedDataType& oDataType,
            CSLConstList papszOptions )
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( !IsValidObjectName(osName) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid array name");
        return nullptr;
    }

    std::vector<DtypeElt> aoDtypeElts;
    constexpr bool bZarrV2 = true;
    const bool bUseUnicode = EQUAL(
        CSLFetchNameValueDef(papszOptions, "STRING_FORMAT", "ASCII"), "UNICODE");
    const auto dtype = FillDTypeElts(oDataType, 0, aoDtypeElts, bZarrV2, bUseUnicode);
    if( !dtype.IsValid() || aoDtypeElts.empty() )
        return nullptr;

    GetMDArrayNames();

    if( m_oMapMDArrays.find(osName) != m_oMapMDArrays.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    CPLJSONObject oCompressor;
    oCompressor.Deinit();
    const char* pszCompressor = CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    const CPLCompressor* psCompressor = nullptr;
    const CPLCompressor* psDecompressor = nullptr;
    if( !EQUAL(pszCompressor, "NONE") )
    {
        psCompressor = CPLGetCompressor(pszCompressor);
        psDecompressor = CPLGetCompressor(pszCompressor);
        if( psCompressor == nullptr || psDecompressor == nullptr )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for %s not available", pszCompressor);
            return nullptr;
        }
        const char* pszOptions = CSLFetchNameValue(psCompressor->papszMetadata, "OPTIONS");
        if( pszOptions )
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot = oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if( psRoot )
            {
                for( const CPLXMLNode* psNode = psRoot->psChild;
                            psNode != nullptr; psNode = psNode->psNext )
                {
                    if( psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0 )
                    {
                        const char* pszName = CPLGetXMLValue(psNode, "name", nullptr);
                        const char* pszType = CPLGetXMLValue(psNode, "type", nullptr);
                        if( pszName && pszType )
                        {
                            const char* pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszCompressor) + '_' + pszName).c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if( pszVal )
                            {
                                if( EQUAL(pszName, "SHUFFLE") && EQUAL(pszVal, "BYTE") )
                                {
                                    pszVal = "1";
                                    pszType = "integer";
                                }

                                if( !oCompressor.IsValid() )
                                {
                                    oCompressor = CPLJSONObject();
                                    oCompressor.Add("id",
                                        CPLString(pszCompressor).tolower());
                                }

                                std::string osOptName(CPLString(pszName).tolower());
                                if( STARTS_WITH(pszType, "int") )
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
    const char* pszFilter = CSLFetchNameValueDef(papszOptions, "FILTER", "NONE");
    if( !EQUAL(pszFilter, "NONE") )
    {
        const auto psFilterCompressor = CPLGetCompressor(pszFilter);
        const auto psFilterDecompressor = CPLGetCompressor(pszFilter);
        if( psFilterCompressor == nullptr || psFilterDecompressor == nullptr )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for filter %s not available", pszFilter);
            return nullptr;
        }

        CPLJSONObject oFilter;
        oFilter.Add("id", CPLString(pszFilter).tolower());
        oFilters.Add(oFilter);

        const char* pszOptions = CSLFetchNameValue(psFilterCompressor->papszMetadata, "OPTIONS");
        if( pszOptions )
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot = oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if( psRoot )
            {
                for( const CPLXMLNode* psNode = psRoot->psChild;
                            psNode != nullptr; psNode = psNode->psNext )
                {
                    if( psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0 )
                    {
                        const char* pszName = CPLGetXMLValue(psNode, "name", nullptr);
                        const char* pszType = CPLGetXMLValue(psNode, "type", nullptr);
                        if( pszName && pszType )
                        {
                            const char* pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszFilter) + '_' + pszName).c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if( pszVal )
                            {
                                std::string osOptName(CPLString(pszName).tolower());
                                if( STARTS_WITH(pszType, "int") )
                                    oFilter.Add(osOptName, atoi(pszVal));
                                else
                                    oFilter.Add(osOptName, pszVal);
                            }
                        }
                    }
                }
            }
        }

        if( EQUAL(pszFilter, "delta") &&
            CSLFetchNameValue(papszOptions, "DELTA_DTYPE") == nullptr )
        {
            if( oDataType.GetClass() != GEDTC_NUMERIC )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "DELTA_DTYPE option must be specified");
                return nullptr;
            }
            switch( oDataType.GetNumericDataType() )
            {
                case GDT_Unknown: break;
                case GDT_Byte: oFilter.Add("dtype", "u1"); break;
                case GDT_UInt16: oFilter.Add("dtype", "<u2"); break;
                case GDT_Int16: oFilter.Add("dtype", "<i2"); break;
                case GDT_UInt32: oFilter.Add("dtype", "<u4"); break;
                case GDT_Int32: oFilter.Add("dtype", "<i4"); break;
                case GDT_UInt64: oFilter.Add("dtype", "<u8"); break;
                case GDT_Int64: oFilter.Add("dtype", "<i8"); break;
                case GDT_Float32: oFilter.Add("dtype", "<f4"); break;
                case GDT_Float64: oFilter.Add("dtype", "<f8"); break;
                case GDT_CInt16: oFilter.Add("dtype", "<i2"); break;
                case GDT_CInt32: oFilter.Add("dtype", "<i4"); break;
                case GDT_CFloat32: oFilter.Add("dtype", "<f4"); break;
                case GDT_CFloat64: oFilter.Add("dtype", "<f8"); break;
                case GDT_TypeCount: break;
            }
        }
    }

    const std::string osZarrayDirectory =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if( VSIMkdir(osZarrayDirectory.c_str(), 0755) != 0 )
    {
        VSIStatBufL sStat;
        if( VSIStatL(osZarrayDirectory.c_str(), &sStat) == 0 )
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
    if( !FillBlockSize(aoDimensions, oDataType, anBlockSize, papszOptions) )
        return nullptr;

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");

    const char* pszDimSeparator = CSLFetchNameValueDef(papszOptions,
                                                       "DIM_SEPARATOR",
                                                       ".");

    auto poArray = ZarrArray::Create(m_poSharedResource,
                                     GetFullName(), osName,
                                     aoDimensions, oDataType,
                                     aoDtypeElts, anBlockSize, bFortranOrder);

    if( !poArray )
        return nullptr;
    const std::string osZarrayFilename =
        CPLFormFilename(osZarrayDirectory.c_str(), ".zarray", nullptr);
    poArray->SetNew(true);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetVersion(2);
    poArray->SetDtype(dtype);
    poArray->SetCompressorDecompressor(pszCompressor, psCompressor, psDecompressor);
    if( oCompressor.IsValid() )
        poArray->SetCompressorJsonV2(oCompressor);
    poArray->SetFilters(oFilters);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    RegisterArray(poArray);

    return poArray;
}

/************************************************************************/
/*                        ExploreDirectory()                            */
/************************************************************************/

void ZarrGroupV3::ExploreDirectory() const
{
    if( m_bDirectoryExplored )
        return;
    m_bDirectoryExplored = true;

    const std::string osDirname =
        m_osDirectoryName + "/meta/root" + GetFullName();

    if( GetFullName() == "/" )
    {
        VSIStatBufL sStat;
        if( VSIStatL((m_osDirectoryName + "/meta/root.array.json").c_str(),
                     &sStat) == 0 )
        {
            m_aosArrays.emplace_back("/");
        }
    }

    const CPLStringList aosFiles(VSIReadDir(osDirname.c_str()));
    std::set<std::string> oSetGroups;
    for( int i = 0; i < aosFiles.size(); ++i )
    {
        const std::string osFilename(aosFiles[i]);
        if( osFilename.size() > strlen(".group.json") &&
                     osFilename.substr(osFilename.size() - strlen(".group.json")) == ".group.json" )
        {
            const auto osGroupName =
                osFilename.substr(0, osFilename.size() - strlen(".group.json"));
            if( oSetGroups.find(osGroupName) == oSetGroups.end() )
            {
                oSetGroups.insert(osGroupName);
                m_aosGroups.emplace_back(osGroupName);
            }
        }
        else if( osFilename.size() > strlen(".array.json") &&
                 osFilename.substr(osFilename.size() - strlen(".array.json")) == ".array.json" )
        {
            const auto osArrayName =
                osFilename.substr(0, osFilename.size() - strlen(".array.json"));
            m_aosArrays.emplace_back(osArrayName);
        }
        else if( osFilename != "." && osFilename != ".." )
        {
            VSIStatBufL sStat;
            if( VSIStatL(CPLFormFilename(osDirname.c_str(), osFilename.c_str(), nullptr),
                         &sStat) == 0 &&
                VSI_ISDIR(sStat.st_mode) )
            {
                const auto& osGroupName = osFilename;
                if( oSetGroups.find(osGroupName) == oSetGroups.end() )
                {
                    oSetGroups.insert(osGroupName);
                    m_aosGroups.emplace_back(osGroupName);
                }
            }
        }
    }
}

/************************************************************************/
/*                      ZarrGroupV3GetFilename()                        */
/************************************************************************/

static std::string ZarrGroupV3GetFilename(const std::string& osParentFullName,
                                          const std::string& osName,
                                          const std::string& osRootDirectoryName)
{
    const std::string osMetaDir(
        CPLFormFilename(osRootDirectoryName.c_str(), "meta", nullptr));

    std::string osGroupFilename(osMetaDir);
    if( osName == "/" )
    {
        osGroupFilename += "/root.group.json";
    }
    else
    {
        osGroupFilename += "/root";
        osGroupFilename += (osParentFullName == "/" ? std::string() : osParentFullName);
        osGroupFilename += '/';
        osGroupFilename += osName;
        osGroupFilename += ".group.json";
    }
    return osGroupFilename;
}

/************************************************************************/
/*                      ZarrGroupV3::ZarrGroupV3()                      */
/************************************************************************/

ZarrGroupV3::ZarrGroupV3(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                         const std::string& osParentName,
                         const std::string& osName,
                         const std::string& osRootDirectoryName):
        ZarrGroupBase(poSharedResource, osParentName, osName),
        m_osGroupFilename(ZarrGroupV3GetFilename(osParentName,
                                                 osName,
                                                 osRootDirectoryName))
{
    m_osDirectoryName = osRootDirectoryName;
}

/************************************************************************/
/*                      ZarrGroupV3::~ZarrGroupV3()                     */
/************************************************************************/

ZarrGroupV3::~ZarrGroupV3()
{
    if( m_bNew || m_oAttrGroup.IsModified() )
    {
        CPLJSONDocument oDoc;
        auto oRoot = oDoc.GetRoot();
        oRoot.Add("extensions", CPLJSONArray());
        oRoot.Add("attributes", m_oAttrGroup.Serialize());
        oDoc.Save(m_osGroupFilename);
    }
}

/************************************************************************/
/*                              OpenGroup()                             */
/************************************************************************/

std::shared_ptr<GDALGroup> ZarrGroupV3::OpenGroup(const std::string& osName,
                                               CSLConstList) const
{
    auto oIter = m_oMapGroups.find(osName);
    if( oIter != m_oMapGroups.end() )
        return oIter->second;

    std::string osFilenamePrefix =
        m_osDirectoryName + "/meta/root" + GetFullName();
    if( GetFullName() != "/" )
        osFilenamePrefix += '/';
    osFilenamePrefix += osName;

    std::string osFilename(osFilenamePrefix);
    osFilename += ".group.json";

    VSIStatBufL sStat;
    // Explicit group
    if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
    {
        auto poSubGroup = ZarrGroupV3::Create(
            m_poSharedResource, GetFullName(), osName, m_osDirectoryName);
        poSubGroup->m_poParent = m_pSelf;
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    // Implicit group
    if( VSIStatL(osFilenamePrefix.c_str(), &sStat) == 0 &&
        VSI_ISDIR(sStat.st_mode) )
    {
        auto poSubGroup = ZarrGroupV3::Create(
            m_poSharedResource, GetFullName(), osName, m_osDirectoryName);
        poSubGroup->m_poParent = m_pSelf;
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    return nullptr;
}

/************************************************************************/
/*                   ZarrGroupV3::CreateOnDisk()                        */
/************************************************************************/

std::shared_ptr<ZarrGroupV3> ZarrGroupV3::CreateOnDisk(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                                       const std::string& osParentFullName,
                                                       const std::string& osName,
                                                       const std::string& osRootDirectoryName)
{
    const std::string osMetaDir(
        CPLFormFilename(osRootDirectoryName.c_str(), "meta", nullptr));
    std::string osGroupDir(osMetaDir);
    osGroupDir += "/root";

    if( osParentFullName.empty() )
    {
        if( VSIMkdir(osRootDirectoryName.c_str(), 0755) != 0 )
        {
            VSIStatBufL sStat;
            if( VSIStatL(osRootDirectoryName.c_str(), &sStat) == 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                         osRootDirectoryName.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                         osRootDirectoryName.c_str());
            }
            return nullptr;
        }

        const std::string osZarrJsonFilename(
            CPLFormFilename(osRootDirectoryName.c_str(), "zarr.json", nullptr));
        VSILFILE* fp = VSIFOpenL(osZarrJsonFilename.c_str(), "wb" );
        if( !fp )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                     osZarrJsonFilename.c_str());
            return nullptr;
        }
        VSIFPrintfL(fp,
            "{\n"
            "    \"zarr_format\": \"https://purl.org/zarr/spec/protocol/core/3.0\",\n"
            "    \"metadata_encoding\": \"https://purl.org/zarr/spec/protocol/core/3.0\",\n"
            "    \"metadata_key_suffix\": \".json\",\n"
            "    \"extensions\": []\n"
            "}\n");
        VSIFCloseL(fp);

        if( VSIMkdir(osMetaDir.c_str(), 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osMetaDir.c_str());
            return nullptr;
        }
    }
    else
    {
        osGroupDir += (osParentFullName == "/" ? std::string() : osParentFullName);
        osGroupDir += '/';
        osGroupDir += osName;
    }

    if( VSIMkdir(osGroupDir.c_str(), 0755) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                 osGroupDir.c_str());
        return nullptr;
    }

    auto poGroup = ZarrGroupV3::Create(poSharedResource,
                                       osParentFullName, osName,
                                       osRootDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;
    poGroup->m_bNew = true;
    return poGroup;
}

/************************************************************************/
/*                      ZarrGroupV3::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup> ZarrGroupV3::CreateGroup(const std::string& osName,
                                                    CSLConstList /* papszOptions */)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( !IsValidObjectName(osName) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid group name");
        return nullptr;
    }

    GetGroupNames();

    if( m_oMapGroups.find(osName) != m_oMapGroups.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }

    auto poGroup = CreateOnDisk(m_poSharedResource,
                                GetFullName(), osName, m_osDirectoryName);
    if( !poGroup )
        return nullptr;
    m_oMapGroups[osName] = poGroup;
    m_aosGroups.emplace_back(osName);
    return poGroup;
}

/************************************************************************/
/*                     ZarrGroupV3::CreateMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrGroupV3::CreateMDArray(
            const std::string& osName,
            const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
            const GDALExtendedDataType& oDataType,
            CSLConstList papszOptions )
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( !IsValidObjectName(osName) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid array name");
        return nullptr;
    }

    if( oDataType.GetClass() != GEDTC_NUMERIC )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported data type with Zarr V3");
        return nullptr;
    }

    if( !EQUAL(CSLFetchNameValueDef(papszOptions, "FILTER", "NONE"), "NONE") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FILTER option not supported with Zarr V3");
        return nullptr;
    }

    std::vector<DtypeElt> aoDtypeElts;
    constexpr bool bZarrV2 = false;
    const auto dtype = FillDTypeElts(oDataType, 0, aoDtypeElts, bZarrV2, false)["dummy"];
    if( !dtype.IsValid() || aoDtypeElts.empty() )
        return nullptr;

    GetMDArrayNames();

    if( m_oMapMDArrays.find(osName) != m_oMapMDArrays.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    CPLJSONObject oCompressor;
    oCompressor.Deinit();
    const char* pszCompressor = CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    const CPLCompressor* psCompressor = nullptr;
    const CPLCompressor* psDecompressor = nullptr;
    if( !EQUAL(pszCompressor, "NONE") )
    {
        psCompressor = CPLGetCompressor(pszCompressor);
        psDecompressor = CPLGetCompressor(pszCompressor);
        if( psCompressor == nullptr || psDecompressor == nullptr )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for %s not available", pszCompressor);
            return nullptr;
        }
        const char* pszOptions = CSLFetchNameValue(psCompressor->papszMetadata, "OPTIONS");
        if( pszOptions )
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot = oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if( psRoot )
            {
                CPLJSONObject configuration;
                for( const CPLXMLNode* psNode = psRoot->psChild;
                            psNode != nullptr; psNode = psNode->psNext )
                {
                    if( psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0 )
                    {
                        const char* pszName = CPLGetXMLValue(psNode, "name", nullptr);
                        const char* pszType = CPLGetXMLValue(psNode, "type", nullptr);
                        if( pszName && pszType )
                        {
                            const char* pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszCompressor) + '_' + pszName).c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if( pszVal )
                            {
                                if( EQUAL(pszName, "SHUFFLE") && EQUAL(pszVal, "BYTE") )
                                {
                                    pszVal = "1";
                                    pszType = "integer";
                                }

                                if( !oCompressor.IsValid() )
                                {
                                    oCompressor = CPLJSONObject();
                                    oCompressor.Add("codec",
                                        "https://purl.org/zarr/spec/codec/" +
                                        CPLString(pszCompressor).tolower() + "/1.0");
                                    oCompressor.Add("configuration", configuration);
                                }

                                std::string osOptName(CPLString(pszName).tolower());
                                if( STARTS_WITH(pszType, "int") )
                                    configuration.Add(osOptName, atoi(pszVal));
                                else
                                    configuration.Add(osOptName, pszVal);
                            }
                        }
                    }
                }
            }
        }
    }

    std::string osFilenamePrefix =
        m_osDirectoryName + "/meta/root";
    if( !(GetFullName() == "/" && osName == "/" ) )
    {
        osFilenamePrefix += GetFullName();
        if( GetFullName() != "/" )
            osFilenamePrefix += '/';
        osFilenamePrefix += osName;
    }

    std::string osFilename(osFilenamePrefix);
    osFilename += ".array.json";

    std::vector<GUInt64> anBlockSize;
    if( !FillBlockSize(aoDimensions, oDataType, anBlockSize, papszOptions) )
        return nullptr;

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");

    const char* pszDimSeparator = CSLFetchNameValueDef(papszOptions,
                                                       "DIM_SEPARATOR",
                                                       "/");

    auto poArray = ZarrArray::Create(m_poSharedResource,
                                     GetFullName(), osName,
                                     aoDimensions, oDataType,
                                     aoDtypeElts, anBlockSize, bFortranOrder);

    if( !poArray )
        return nullptr;
    poArray->SetNew(true);
    poArray->SetFilename(osFilename);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetVersion(3);
    poArray->SetDtype(dtype);
    poArray->SetCompressorDecompressor(pszCompressor, psCompressor, psDecompressor);
    if( oCompressor.IsValid() )
        poArray->SetCompressorJsonV3(oCompressor);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    RegisterArray(poArray);

    return poArray;
}

/************************************************************************/
/*              ZarrSharedResource::ZarrSharedResource()                */
/************************************************************************/

ZarrSharedResource::ZarrSharedResource(const std::string& osRootDirectoryName)
{
    m_oObj.Add("zarr_consolidated_format", 1);
    m_oObj.Add("metadata", CPLJSONObject());

    m_osRootDirectoryName = osRootDirectoryName;
    if( !m_osRootDirectoryName.empty() && m_osRootDirectoryName.back() == '/' )
    {
        m_osRootDirectoryName.resize(m_osRootDirectoryName.size() - 1);
    }
    m_poPAM = std::make_shared<GDALPamMultiDim>(
        CPLFormFilename(m_osRootDirectoryName.c_str(), "pam", nullptr));
}

/************************************************************************/
/*              ZarrSharedResource::~ZarrSharedResource()               */
/************************************************************************/

ZarrSharedResource::~ZarrSharedResource()
{
    if( m_bZMetadataModified )
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(m_oObj);
        oDoc.Save(
            CPLFormFilename(m_osRootDirectoryName.c_str(), ".zmetadata", nullptr));
    }
}

/************************************************************************/
/*             ZarrSharedResource::SetZMetadataItem()                   */
/************************************************************************/

void ZarrSharedResource::SetZMetadataItem(const std::string& osFilename,
                                              const CPLJSONObject& obj)
{
    if( m_bZMetadataEnabled )
    {
        CPLString osNormalizedFilename(osFilename);
        osNormalizedFilename.replaceAll('\\', '/');
        CPLAssert( STARTS_WITH(osNormalizedFilename.c_str(),
                               (m_osRootDirectoryName + '/').c_str()) );
        m_bZMetadataModified = true;
        const char* pszKey = osNormalizedFilename.c_str() + m_osRootDirectoryName.size() + 1;
        m_oObj["metadata"].DeleteNoSplitName(pszKey);
        m_oObj["metadata"].AddNoSplitName(pszKey, obj);
    }
}
