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
    // We need to explictly flush arrays so that the _ARRAY_DIMENSIONS
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
/*                      ZarrGroupV2::~ZarrGroupV2()                     */
/************************************************************************/

ZarrGroupV2::~ZarrGroupV2()
{
    if( m_oAttrGroup.IsModified() )
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(m_oAttrGroup.Serialize());
        oDoc.Save(CPLFormFilename(m_osDirectoryName.c_str(), ".zattrs", nullptr));
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
    for( int i = 0; i < aosFiles.size(); ++i )
    {
        if( strcmp(aosFiles[i], ".") != 0 &&
            strcmp(aosFiles[i], "..") != 0 &&
            strcmp(aosFiles[i], ".zarray") != 0 &&
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
            return LoadArray(osName, osZarrayFilename, oRoot, false, CPLJSONObject());
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

            auto poSubGroup = std::make_shared<ZarrGroupV2>(GetFullName(), osName);
            poSubGroup->SetUpdatable(m_bUpdatable);
            poSubGroup->SetDirectoryName(osSubDir);
            m_oMapGroups[osName] = poSubGroup;
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
        return LoadArray(osName, osFilename, oRoot, false, CPLJSONObject());
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

    poSubGroup = std::make_shared<ZarrGroupV2>(
            poBelongingGroup->GetFullName(),
            osSubGroupFullname.substr(nLastSlashPos + 1));
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
        const auto osArrayName = osArrayFullname.substr(nLastSlashPos + 1);
        const std::string osZarrayFilename =
            CPLFormFilename(
                CPLFormFilename(poBelongingGroup->m_osDirectoryName.c_str(),
                                osArrayName.c_str(), nullptr),
                ".zarray",
                nullptr);
        poBelongingGroup->LoadArray(
                    osArrayName, osZarrayFilename, oArray, true, oAttributes);
    };

    // Second pass to read attributes and create arrays that have attributes
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
                    CreateArray(osObjectFullnameNoLeadingSlash,
                                *(oIter->second), child);
                    oMapArrays.erase(oIter);
                }
            }
        }
    }

    // Third pass to create arrays without attributes
    for( const auto& kv: oMapArrays )
    {
        CreateArray(kv.first, *(kv.second), CPLJSONObject());
    }
}

/************************************************************************/
/*                   ZarrGroupV2::CreateOnDisk()                        */
/************************************************************************/

std::shared_ptr<ZarrGroupV2> ZarrGroupV2::CreateOnDisk(const std::string& osParentName,
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

    auto poGroup = std::make_shared<ZarrGroupV2>(osParentName, osName);
    poGroup->SetDirectoryName(osDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;
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
    auto poGroup = CreateOnDisk(GetFullName(), osName, osDirectoryName);
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
                                   std::vector<DtypeElt>& aoDtypeElts)
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
            elt.nativeType = DtypeElt::NativeType::STRING;
            elt.nativeOffset = nNativeStartOffset;
            elt.nativeSize = oDataType.GetMaxStringLength();
            elt.gdalOffset = nGDALStartOffset;
            elt.gdalSize = sizeof(char*);
            aoDtypeElts.emplace_back(elt);
            dtype.Set(dummy, CPLSPrintf("|S%d", static_cast<int>(elt.nativeSize)));
            break;
        }

        case GEDTC_NUMERIC:
        {
            const auto eDT = oDataType.GetNumericDataType();
            DtypeElt elt;
            switch( eDT )
            {
                case GDT_Byte:
                {
                    elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                    dtype.Set(dummy, "|u1");
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
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unsupported data type: %s",
                             GDALGetDataTypeName(eDT));
                    dtype = CPLJSONObject();
                    dtype.Deinit();
                    return dtype;
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
                    static_assert(GDT_TypeCount == GDT_CFloat64 + 1, "GDT_TypeCount == GDT_CFloat64 + 1");
                    break;
                }
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
                    aoDtypeElts);
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
    const auto dtype = FillDTypeElts(oDataType, 0, aoDtypeElts);
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

    const auto nDims = aoDimensions.size();
    std::vector<GUInt64> anBlockSize(nDims, 1);
    if( nDims >= 2 )
    {
        anBlockSize[nDims-2] = std::min(aoDimensions[nDims-2]->GetSize(),
                                        static_cast<GUInt64>(256));
        anBlockSize[nDims-1] = std::min(aoDimensions[nDims-1]->GetSize(),
                                        static_cast<GUInt64>(256));
    }
    else if( nDims == 1 )
    {
        anBlockSize[0] = aoDimensions[0]->GetSize();
    }

    const char* pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if( pszBlockSize )
    {
        const auto aszTokens(CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if( static_cast<size_t>(aszTokens.size()) != nDims )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return nullptr;
        }
        for( size_t i = 0; i < nDims; ++i )
        {
            anBlockSize[i] = static_cast<uint32_t>(CPLAtoGIntBig(aszTokens[i]));
        }
    }

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");

    auto poArray = ZarrArray::Create(GetFullName(), osName,
                                     aoDimensions, oDataType,
                                     aoDtypeElts, anBlockSize, bFortranOrder);

    if( !poArray )
        return nullptr;
    const std::string osZarrayFilename =
        CPLFormFilename(osZarrayDirectory.c_str(), ".zarray", nullptr);
    poArray->SetNew(true);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetVersion(2);
    poArray->SetDtype(dtype);
    poArray->SetCompressorDecompressor(psCompressor, psDecompressor);
    if( oCompressor.IsValid() )
        poArray->SetCompressorJsonV2(oCompressor);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    m_oMapMDArrays[osName] = poArray;
    m_aosArrays.emplace_back(osName);
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
        auto poSubGroup = std::make_shared<ZarrGroupV3>(GetFullName(), osName);
        poSubGroup->SetDirectoryName(m_osDirectoryName);
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    // Implicit group
    if( VSIStatL(osFilenamePrefix.c_str(), &sStat) == 0 &&
        VSI_ISDIR(sStat.st_mode) )
    {
        auto poSubGroup = std::make_shared<ZarrGroupV3>(GetFullName(), osName);
        poSubGroup->SetDirectoryName(m_osDirectoryName);
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    return nullptr;
}
