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

#include "cpl_compressor.h"
#include "cpl_json.h"
#include "gdal_priv.h"
#include "memmultidim.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                            ZarrDataset                               */
/************************************************************************/

class ZarrDataset final: public GDALDataset
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};

public:
    static int Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);

    std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }
};

/************************************************************************/
/*                        ZarrAttributeGroup()                          */
/************************************************************************/

class ZarrAttributeGroup
{
    // Use a MEMGroup as a convenient container for attributes.
    MEMGroup m_oGroup;

public:
    explicit ZarrAttributeGroup(const std::string& osParentName);

    void Init(const CPLJSONObject& obj);

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const { return m_oGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const { return m_oGroup.GetAttributes(papszOptions); }
};

/************************************************************************/
/*                             ZarrGroup                                */
/************************************************************************/

class ZarrArray;

class ZarrGroupBase CPL_NON_FINAL: public GDALGroup
{
protected:
    std::string m_osDirectoryName{};
    mutable std::map<CPLString, std::shared_ptr<GDALGroup>> m_oMapGroups{};
    mutable std::map<CPLString, std::shared_ptr<GDALMDArray>> m_oMapMDArrays{};
    mutable std::map<CPLString, std::shared_ptr<GDALDimensionWeakIndexingVar>> m_oMapDimensions{};
    mutable bool m_bDirectoryExplored = false;
    mutable std::vector<std::string> m_aosGroups{};
    mutable std::vector<std::string> m_aosArrays{};
    mutable ZarrAttributeGroup                        m_oAttrGroup;
    mutable bool                                      m_bAttributesLoaded = false;
    bool                                              m_bReadFromZMetadata = false;
    mutable bool                                      m_bDimensionsInstantiated = false;

    virtual void ExploreDirectory() const = 0;
    virtual void LoadAttributes() const = 0;

public:
    ZarrGroupBase(const std::string& osParentName, const std::string& osName):
        GDALGroup(osParentName, osName),
        m_oAttrGroup(osParentName) {}

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override
        { LoadAttributes(); return m_oAttrGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override
        { LoadAttributes(); return m_oAttrGroup.GetAttributes(papszOptions); }

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;

    void SetDirectoryName(const std::string& osDirectoryName) { m_osDirectoryName = osDirectoryName; }

    std::shared_ptr<ZarrArray> LoadArray(const std::string& osArrayName,
                                         const std::string& osZarrayFilename,
                                         const CPLJSONObject& oRoot,
                                         bool bLoadedFromZMetadata,
                                         const CPLJSONObject& oAttributes) const;
    void RegisterArray(const std::shared_ptr<GDALMDArray>& array) const;
};

class ZarrGroupV2 final: public ZarrGroupBase
{
    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    std::shared_ptr<ZarrGroupV2> GetOrCreateSubGroup(
                                        const std::string& osSubGroupFullname);

public:
    ZarrGroupV2(const std::string& osParentName, const std::string& osName):
        ZarrGroupBase(osParentName, osName) {}

    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    void InitFromZMetadata(const CPLJSONObject& oRoot);
};

class ZarrGroupV3 final: public ZarrGroupBase
{
    void ExploreDirectory() const override;
    void LoadAttributes() const override;

public:
    ZarrGroupV3(const std::string& osParentName, const std::string& osName):
        ZarrGroupBase(osParentName, osName) {}

    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                              DtypeElt()                              */
/************************************************************************/

struct DtypeElt
{
    enum class NativeType
    {
        BOOLEAN,
        UNSIGNED_INT,
        SIGNED_INT,
        IEEEFP,
        COMPLEX_IEEEFP,
        STRING,
    };

    NativeType           nativeType = NativeType::BOOLEAN;
    size_t               nativeOffset = 0;
    size_t               nativeSize = 0;
    bool                 needByteSwapping = false;
    bool                 gdalTypeIsApproxOfNative = false;
    GDALExtendedDataType gdalType = GDALExtendedDataType::Create(GDT_Unknown);
    size_t               gdalOffset = 0;
    size_t               gdalSize = 0;
};

/************************************************************************/
/*                             ZarrArray                                */
/************************************************************************/

class ZarrArray final: public GDALMDArray
{
    const std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    const GDALExtendedDataType                        m_oType;
    const std::vector<DtypeElt>                       m_aoDtypeElts;
    const std::vector<GUInt64>                        m_anBlockSize;
    GByte                                            *m_pabyNoData = nullptr;
    std::string                                       m_osDimSeparator { "." };
    std::string                                       m_osFilename{};
    mutable std::vector<GByte>                        m_abyRawTileData{};
    mutable std::vector<GByte>                        m_abyDecodedTileData{};
    mutable std::vector<uint64_t>                     m_anCachedTiledIndices{};
    mutable bool                                      m_bCachedTiledValid = false;
    mutable bool                                      m_bCachedTiledEmpty = false;
    bool                                              m_bUseOptimizedCodePaths = true;
    bool                                              m_bFortranOrder = false;
    const CPLCompressor                              *m_psDecompressor = nullptr;
    mutable std::vector<GByte>                        m_abyTmpRawTileData{}; // used for Fortran order
    mutable ZarrAttributeGroup                        m_oAttrGroup;
    mutable std::shared_ptr<OGRSpatialReference>      m_poSRS{};
    mutable bool                                      m_bAllocateWorkingBuffersDone = false;
    mutable bool                                      m_bWorkingBuffersOK = false;
    std::string                                       m_osRootDirectoryName{};
    int                                               m_nVersion = 0;

    ZarrArray(const std::string& osParentName,
              const std::string& osName,
              const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
              const GDALExtendedDataType& oType,
              const std::vector<DtypeElt>& aoDtypeElts,
              const std::vector<GUInt64>& anBlockSize,
              bool bFortranOrder);

    bool LoadTileData(const std::vector<uint64_t>& tileIndices,
                      bool& bMissingTileOut) const;
    void BlockTranspose(const std::vector<GByte>& abySrc,
                        std::vector<GByte>& abyDst) const;

    bool AllocateWorkingBuffers() const;

protected:
    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IsCacheable() const override { return false; }

public:
    ~ZarrArray() override;

    static std::shared_ptr<ZarrArray> Create(const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder);

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_osFilename; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_aoDims; }

    const GDALExtendedDataType& GetDataType() const override { return m_oType; }

    std::vector<GUInt64> GetBlockSize() const override { return m_anBlockSize; }

    const void* GetRawNoDataValue() const override { return m_pabyNoData; }

    void RegisterNoDataValue(const void*);

    void SetFilename(const std::string& osFilename ) { m_osFilename = osFilename; }

    void SetDimSeparator(const std::string& osDimSeparator) { m_osDimSeparator = osDimSeparator; }

    void SetDecompressor(const CPLCompressor* psComp) { m_psDecompressor = psComp; }

    void SetAttributes(const CPLJSONObject& attrs) { m_oAttrGroup.Init(attrs); }

    void SetSRS(const std::shared_ptr<OGRSpatialReference>& srs) { m_poSRS = srs; }

    void SetRootDirectoryName(const std::string& osRootDirectoryName) { m_osRootDirectoryName = osRootDirectoryName; }

    void SetVersion(int nVersion) { m_nVersion = nVersion; }

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override
        { return m_oAttrGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override
        { return m_oAttrGroup.GetAttributes(papszOptions); }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;
};

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
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetMDArrayNames(CSLConstList) const
{
    if( !m_bDirectoryExplored )
        ExploreDirectory();

    return m_aosArrays;
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
/*                            RegisterArray()                           */
/************************************************************************/

void ZarrGroupBase::RegisterArray(const std::shared_ptr<GDALMDArray>& array) const
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
    m_oAttrGroup.Init(oRoot);
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
        m_oAttrGroup.Init(oRoot["attributes"]);
    }
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
            m_oAttrGroup.Init(child);
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
                poSubGroup->m_oAttrGroup.Init(child);
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
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    // Implicit group
    if( VSIStatL(osFilenamePrefix.c_str(), &sStat) == 0 &&
        VSI_ISDIR(sStat.st_mode) )
    {
        auto poSubGroup = std::make_shared<ZarrGroupV3>(GetFullName(), osName);
        poSubGroup->SetDirectoryName(m_osDirectoryName);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    return nullptr;
}

/************************************************************************/
/*             ZarrAttributeGroup::ZarrAttributeGroup()                 */
/************************************************************************/

ZarrAttributeGroup::ZarrAttributeGroup(const std::string& osParentName):
                                            m_oGroup(osParentName, nullptr)
{
}

/************************************************************************/
/*                   ZarrAttributeGroup::Init()                         */
/************************************************************************/

void ZarrAttributeGroup::Init(const CPLJSONObject& obj)
{
    if( obj.GetType() != CPLJSONObject::Type::Object )
        return;
    const auto children = obj.GetChildren();
    for( const auto& item: children )
    {
        const auto itemType = item.GetType();
        bool bDone = false;
        if( itemType == CPLJSONObject::Type::String )
        {
            bDone = true;
            auto poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {},
                GDALExtendedDataType::CreateString(), nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const std::string str = item.ToString();
                const char* c_str = str.c_str();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              poAttr->GetDataType(),
                              &c_str);
            }
        }
        else if( itemType == CPLJSONObject::Type::Integer ||
                 itemType == CPLJSONObject::Type::Long ||
                 itemType == CPLJSONObject::Type::Double )
        {
            bDone = true;
            auto poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {},
                GDALExtendedDataType::Create(
                    itemType == CPLJSONObject::Type::Integer ?
                        GDT_Int32 : GDT_Float64),
                nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const double val = item.ToDouble();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              GDALExtendedDataType::Create(GDT_Float64),
                              &val);
            }
        }
        else if( itemType == CPLJSONObject::Type::Array )
        {
            const auto array = item.ToArray();
            bool isFirst = true;
            bool isString = false;
            bool isNumeric = false;
            bool foundInt64 = false;
            bool foundDouble = false;
            bool mixedType = false;
            size_t countItems = 0;
            for( const auto& subItem: array )
            {
                const auto subItemType = subItem.GetType();
                if( subItemType == CPLJSONObject::Type::String )
                {
                    if( isFirst )
                    {
                        isString = true;
                    }
                    else if( !isString )
                    {
                        mixedType = true;
                        break;
                    }
                    countItems ++;
                }
                else if( subItemType == CPLJSONObject::Type::Integer ||
                         subItemType == CPLJSONObject::Type::Long ||
                         subItemType == CPLJSONObject::Type::Double )
                {
                    if( isFirst )
                    {
                        isNumeric = true;
                    }
                    else if( !isNumeric )
                    {
                        mixedType = true;
                        break;
                    }
                    if( subItemType == CPLJSONObject::Type::Double )
                        foundDouble = true;
                    else if( subItemType == CPLJSONObject::Type::Long )
                        foundInt64 = true;
                    countItems ++;
                }
                else
                {
                    mixedType = true;
                    break;
                }
                isFirst = false;
            }

            if( !mixedType && !isFirst )
            {
                bDone = true;
                auto poAttr = m_oGroup.CreateAttribute(
                    item.GetName(), { countItems },
                    isString ?
                        GDALExtendedDataType::CreateString():
                        GDALExtendedDataType::Create(
                            (foundDouble || foundInt64) ? GDT_Float64 : GDT_Int32),
                    nullptr);
                if( poAttr )
                {
                    size_t idx = 0;
                    for( const auto& subItem: array )
                    {
                        const GUInt64 arrayStartIdx = idx;
                        const size_t count = 1;
                        const GInt64 arrayStep = 0;
                        const GPtrDiff_t bufferStride = 0;
                        const auto subItemType = subItem.GetType();
                        if( subItemType == CPLJSONObject::Type::String )
                        {
                            const std::string str = subItem.ToString();
                            const char* c_str = str.c_str();
                            poAttr->Write(&arrayStartIdx,
                                          &count,
                                          &arrayStep,
                                          &bufferStride,
                                          poAttr->GetDataType(),
                                          &c_str);
                        }
                        else if( subItemType == CPLJSONObject::Type::Integer ||
                                 subItemType == CPLJSONObject::Type::Long ||
                                 subItemType == CPLJSONObject::Type::Double )
                        {
                            const double val = subItem.ToDouble();
                            poAttr->Write(&arrayStartIdx,
                                          &count,
                                          &arrayStep,
                                          &bufferStride,
                                          GDALExtendedDataType::Create(GDT_Float64),
                                          &val);
                        }
                        ++idx;
                    }
                }
            }
        }

        if( !bDone )
        {
            auto poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {}, GDALExtendedDataType::CreateString(), nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const std::string str = item.ToString();
                const char* c_str = str.c_str();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              poAttr->GetDataType(),
                              &c_str);
            }
        }
    }
}

/************************************************************************/
/*                         ZarrArray::ZarrArray()                       */
/************************************************************************/

ZarrArray::ZarrArray(const std::string& osParentName,
                     const std::string& osName,
                     const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                     const GDALExtendedDataType& oType,
                     const std::vector<DtypeElt>& aoDtypeElts,
                     const std::vector<GUInt64>& anBlockSize,
                     bool bFortranOrder):
    GDALAbstractMDArray(osParentName, osName),
    GDALMDArray(osParentName, osName),
    m_aoDims(aoDims),
    m_oType(oType),
    m_aoDtypeElts(aoDtypeElts),
    m_anBlockSize(anBlockSize),
    m_bFortranOrder(bFortranOrder),
    m_oAttrGroup(osParentName)
{
}

/************************************************************************/
/*                          ZarrArray::Create()                         */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrArray::Create(const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder)
{
    auto arr = std::shared_ptr<ZarrArray>(
        new ZarrArray(osParentName, osName, aoDims, oType, aoDtypeElts,
                      anBlockSize, bFortranOrder));
    arr->SetSelf(arr);

    arr->m_bUseOptimizedCodePaths = CPLTestBool(
        CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES"));

    return arr;
}

/************************************************************************/
/*                              ~ZarrArray()                            */
/************************************************************************/

ZarrArray::~ZarrArray()
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }
    if( !m_abyDecodedTileData.empty() )
    {
        const size_t nDTSize = m_oType.GetSize();
        GByte* pDst = &m_abyDecodedTileData[0];
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        for( size_t i = 0; i < nValues; i++, pDst += nDTSize )
        {
            for( auto& elt: m_aoDtypeElts )
            {
                if( elt.nativeType == DtypeElt::NativeType::STRING )
                {
                    char* ptr;
                    char** pptr = reinterpret_cast<char**>(pDst + elt.gdalOffset);
                    memcpy(&ptr, pptr, sizeof(ptr));
                    VSIFree(ptr);
                }
            }
        }
    }
}

/************************************************************************/
/*               ZarrArray::AllocateWorkingBuffers()                    */
/************************************************************************/

bool ZarrArray::AllocateWorkingBuffers() const
{
    if( m_bAllocateWorkingBuffersDone )
        return m_bWorkingBuffersOK;

    m_bAllocateWorkingBuffersDone = true;

    // Reserve a buffer for tile content
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;
    size_t nTileSize = m_oType.GetClass() == GEDTC_STRING ?
                                m_oType.GetMaxStringLength() : nSourceSize;
    for( const auto& nBlockSize: m_anBlockSize )
    {
        nTileSize *= static_cast<size_t>(nBlockSize);
    }
    try
    {
        m_abyRawTileData.resize( nTileSize );
        if( m_bFortranOrder )
            m_abyTmpRawTileData.resize( nTileSize );
    }
    catch( const std::bad_alloc& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    bool bNeedDecodedBuffer = false;
    if( m_oType.GetClass() == GEDTC_COMPOUND && nSourceSize != m_oType.GetSize() )
    {
        bNeedDecodedBuffer = true;
    }
    else if( m_oType.GetClass() != GEDTC_STRING )
    {
        for( const auto& elt: m_aoDtypeElts )
        {
            if( elt.needByteSwapping || elt.gdalTypeIsApproxOfNative ||
                elt.nativeType == DtypeElt::NativeType::STRING )
            {
                bNeedDecodedBuffer = true;
                break;
            }
        }
    }
    if( bNeedDecodedBuffer )
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for( const auto& nBlockSize: m_anBlockSize )
        {
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        try
        {
            m_abyDecodedTileData.resize( nDecodedBufferSize );
        }
        catch( const std::bad_alloc& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
    }

    m_bWorkingBuffersOK = true;
    return true;
}

/************************************************************************/
/*                    ZarrArray::GetSpatialRef()                        */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> ZarrArray::GetSpatialRef() const
{
    return m_poSRS;
}

/************************************************************************/
/*                        RegisterNoDataValue()                         */
/************************************************************************/

void ZarrArray::RegisterNoDataValue(const void* pNoData)
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    if( pNoData == nullptr )
    {
        CPLFree(m_pabyNoData);
        m_pabyNoData = nullptr;
    }
    else
    {
        const auto nSize = m_oType.GetSize();
        if( m_pabyNoData == nullptr )
        {
            m_pabyNoData = static_cast<GByte*>(CPLMalloc(nSize));
        }
        memset(m_pabyNoData, 0, nSize);
        GDALExtendedDataType::CopyValue( pNoData, m_oType, m_pabyNoData, m_oType );
    }
}

/************************************************************************/
/*                      ZarrArray::BlockTranspose()                     */
/************************************************************************/

void ZarrArray::BlockTranspose(const std::vector<GByte>& abySrc,
                               std::vector<GByte>& abyDst) const
{
    // Perform transposition
    const size_t nDims = m_anBlockSize.size();
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;

    struct Stack
    {
        size_t       nIters = 0;
        const GByte* src_ptr = nullptr;
        GByte*       dst_ptr = nullptr;
        size_t       src_inc_offset = 0;
        size_t       dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims + 1);
    assert(!stack.empty()); // to make gcc 9.3 -O2 -Wnull-dereference happy

    stack[0].src_inc_offset = nSourceSize;
    for( size_t i = 1; i < nDims; ++i )
    {
        stack[i].src_inc_offset = stack[i-1].src_inc_offset *
                                static_cast<size_t>(m_anBlockSize[i-1]);
    }

    stack[nDims-1].dst_inc_offset = nSourceSize;
    for( size_t i = nDims - 1; i > 0; )
    {
        --i;
        stack[i].dst_inc_offset = stack[i+1].dst_inc_offset *
                                static_cast<size_t>(m_anBlockSize[i+1]);
    }

    stack[0].src_ptr = abySrc.data();
    stack[0].dst_ptr = &abyDst[0];

    size_t dimIdx = 0;
lbl_next_depth:
    if( dimIdx == nDims )
    {
        void* dst_ptr = stack[nDims].dst_ptr;
        const void* src_ptr = stack[nDims].src_ptr;
        if( nSourceSize == 1 )
            *stack[nDims].dst_ptr = *stack[nDims].src_ptr;
        else if( nSourceSize == 2 )
            *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr);
        else if( nSourceSize == 4 )
            *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr);
        else if( nSourceSize == 8 )
            *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr);
        else
            memcpy(dst_ptr, src_ptr, nSourceSize);
    }
    else
    {
        stack[dimIdx].nIters = static_cast<size_t>(m_anBlockSize[dimIdx]);
        while(true)
        {
            dimIdx ++;
            stack[dimIdx].src_ptr = stack[dimIdx-1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx-1].dst_ptr;
            goto lbl_next_depth;
lbl_return_to_caller:
            dimIdx --;
            if( (--stack[dimIdx].nIters) == 0 )
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;
}

/************************************************************************/
/*                        DecodeSourceElt()                             */
/************************************************************************/

static void DecodeSourceElt(const std::vector<DtypeElt>& elts,
                            const GByte* pSrc,
                            GByte* pDst)
{
    for( auto& elt: elts )
    {
        if( elt.needByteSwapping )
        {
            if( elt.nativeSize == 2 )
            {
                uint16_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint16_t*>(pDst + elt.gdalOffset) =
                    CPL_SWAP16(val);
            }
            else if( elt.nativeSize == 4 )
            {
                uint32_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                    CPL_SWAP32(val);
            }
            else if( elt.nativeSize == 8 )
            {
                if( elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP )
                {
                    uint32_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                        CPL_SWAP32(val);
                    memcpy(&val, pSrc + elt.nativeOffset + 4, sizeof(val));
                    *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset + 4) =
                        CPL_SWAP32(val);
                }
                else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    int64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                        CPL_SWAP64(val));
                }
                else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    uint64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                        CPL_SWAP64(val));
                }
                else
                {
                    uint64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                        CPL_SWAP64(val);
                }
            }
            else if( elt.nativeSize == 16 )
            {
                uint64_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                    CPL_SWAP64(val);
                memcpy(&val, pSrc + elt.nativeOffset + 8, sizeof(val));
                *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset + 8) =
                    CPL_SWAP64(val);
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if( elt.gdalTypeIsApproxOfNative )
        {
            if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                elt.nativeSize == 1 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Int16 );
                int16_t intVal = *reinterpret_cast<const int8_t*>(pSrc + elt.nativeOffset);
                memcpy(pDst + elt.gdalOffset, &intVal, sizeof(intVal));
            }
            else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                     elt.nativeSize == 8 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                int64_t intVal;
                memcpy(&intVal, pSrc + elt.nativeOffset, sizeof(intVal));
                double dblVal = static_cast<double>(intVal);
                memcpy(pDst + elt.gdalOffset, &dblVal, sizeof(dblVal));
            }
            else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT &&
                     elt.nativeSize == 8 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                uint64_t intVal;
                memcpy(&intVal, pSrc + elt.nativeOffset, sizeof(intVal));
                double dblVal = static_cast<double>(intVal);
                memcpy(pDst + elt.gdalOffset, &dblVal, sizeof(dblVal));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if( elt.nativeType == DtypeElt::NativeType::STRING )
        {
            char* ptr;
            char** pDstPtr = reinterpret_cast<char**>(pDst + elt.gdalOffset);
            memcpy(&ptr, pDstPtr, sizeof(ptr));
            VSIFree(ptr);

            char* pDstStr = static_cast<char*>(CPLMalloc(elt.nativeSize + 1));
            memcpy(pDstStr, pSrc + elt.nativeOffset, elt.nativeSize);
            pDstStr[elt.nativeSize] = 0;
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else
        {
            CPLAssert( elt.nativeSize == elt.gdalSize );
            memcpy(pDst + elt.gdalOffset,
                   pSrc + elt.nativeOffset,
                   elt.nativeSize);
        }
    }
}

/************************************************************************/
/*                        ZarrArray::LoadTileData()                     */
/************************************************************************/

bool ZarrArray::LoadTileData(const std::vector<uint64_t>& tileIndices,
                             bool& bMissingTileOut) const
{
    std::string osFilename;
    for( const auto index: tileIndices )
    {
        if( !osFilename.empty() )
            osFilename += m_osDimSeparator;
        osFilename += std::to_string(index);
    }

    if( m_nVersion == 2 )
    {
        osFilename = CPLFormFilename(
            CPLGetDirname(m_osFilename.c_str()), osFilename.c_str(), nullptr);
    }
    else
    {
        std::string osTmp = m_osRootDirectoryName + "/data/root";
        if( GetFullName() != "/" )
            osTmp += GetFullName();
        osFilename = osTmp + "/c" + osFilename;
    }

    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if( fp == nullptr )
    {
        // Missing files are OK and indicate nodata_value
        CPLDebugOnly("Zarr", "Tile %s missing (=nodata)", osFilename.c_str());
        bMissingTileOut = true;
        return true;
    }

    bMissingTileOut = false;
    bool bRet = true;
    if( m_psDecompressor == nullptr )
    {
        if( VSIFReadL(&m_abyRawTileData[0], 1, m_abyRawTileData.size(), fp) != m_abyRawTileData.size() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not read tile %s correctly",
                     osFilename.c_str());
            bRet = false;
        }
    }
    else
    {
        VSIFSeekL(fp, 0, SEEK_END);
        const auto nSize = VSIFTellL(fp);
        VSIFSeekL(fp, 0, SEEK_SET);
        if( nSize > static_cast<vsi_l_offset>(std::numeric_limits<int>::max()) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large tile %s",
                     osFilename.c_str());
            bRet = false;
        }
        else
        {
            std::vector<GByte> abyCompressedData;
            try
            {
                abyCompressedData.resize(static_cast<size_t>(nSize));

            }
            catch( const std::exception& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory for tile %s",
                         osFilename.c_str());
                bRet = false;
            }

            if( bRet &&
                VSIFReadL(&abyCompressedData[0], 1, abyCompressedData.size(),
                          fp) != abyCompressedData.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not read tile %s correctly",
                         osFilename.c_str());
                bRet = false;
            }
            else
            {
                void* out_buffer = &m_abyRawTileData[0];
                size_t out_size = m_abyRawTileData.size();
                if( !m_psDecompressor->pfnFunc(abyCompressedData.data(),
                                               abyCompressedData.size(),
                                               &out_buffer, &out_size,
                                               nullptr,
                                               m_psDecompressor->user_data ) ||
                    out_size != m_abyRawTileData.size() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression of tile %s failed",
                             osFilename.c_str());
                    bRet = false;
                }
            }
        }
    }
    VSIFCloseL(fp);

    if( bRet && !bMissingTileOut && m_bFortranOrder )
    {
        BlockTranspose(m_abyRawTileData, m_abyTmpRawTileData);
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    if( bRet && !bMissingTileOut && !m_abyDecodedTileData.empty() )
    {
        const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                                   m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        const GByte* pSrc = m_abyRawTileData.data();
        GByte* pDst = &m_abyDecodedTileData[0];
        for( size_t i = 0; i < nValues; i++, pSrc += nSourceSize, pDst += nDTSize )
        {
            DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    return bRet;
}

/************************************************************************/
/*                           ZarrArray::IRead()                         */
/************************************************************************/

bool ZarrArray::IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const
{
    if( !AllocateWorkingBuffers() )
        return false;

    // Need to be kept in top-level scope
    std::vector<GUInt64> arrayStartIdxMod;
    std::vector<GInt64> arrayStepMod;
    std::vector<GPtrDiff_t> bufferStrideMod;

    const size_t nDims = m_aoDims.size();
    bool negativeStep = false;
    for( size_t i = 0; i < nDims; ++i )
    {
        if( arrayStep[i] < 0 )
        {
            negativeStep = true;
            break;
        }
    }

    //const auto eBufferDT = bufferDataType.GetNumericDataType();
    const auto nBufferDTSize = static_cast<int>(bufferDataType.GetSize());

    // Make sure that arrayStep[i] are positive for sake of simplicity
    if( negativeStep )
    {
        arrayStartIdxMod.resize(nDims);
        arrayStepMod.resize(nDims);
        bufferStrideMod.resize(nDims);
        for( size_t i = 0; i < nDims; ++i )
        {
            if( arrayStep[i] < 0 )
            {
                arrayStartIdxMod[i] = arrayStartIdx[i] - (count[i] - 1) * (-arrayStep[i]);
                arrayStepMod[i] = -arrayStep[i];
                bufferStrideMod[i] = -bufferStride[i];
                pDstBuffer = static_cast<GByte*>(pDstBuffer) +
                    bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize * (count[i] - 1));
            }
            else
            {
                arrayStartIdxMod[i] = arrayStartIdx[i];
                arrayStepMod[i] = arrayStep[i];
                bufferStrideMod[i] = bufferStride[i];
            }
        }
        arrayStartIdx = arrayStartIdxMod.data();
        arrayStep = arrayStepMod.data();
        bufferStride = bufferStrideMod.data();
    }

    std::vector<uint64_t> indicesOuterLoop(nDims);
    std::vector<GByte*> dstPtrStackOuterLoop(nDims + 1);

    std::vector<uint64_t> indicesInnerLoop(nDims);
    std::vector<GByte*> dstPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> dstBufferStrideBytes;
    for( size_t i = 0; i < nDims; ++i )
    {
        dstBufferStrideBytes.push_back(
            bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize));
    }

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT =
        m_oType.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    const bool bSameCompoundAndNoDynamicMem =
        m_oType.GetClass() == GEDTC_COMPOUND &&
        m_oType == bufferDataType &&
        !m_oType.NeedsFreeDynamicMemory();
    std::vector<GByte> abyTargetNoData;
    bool bNoDataIsZero = false;

    size_t dimIdx = 0;
    dstPtrStackOuterLoop[0] = static_cast<GByte*>(pDstBuffer);
lbl_next_depth:
    if( dimIdx == nDims )
    {
        size_t dimIdxSubLoop = 0;
        dstPtrStackInnerLoop[0] = dstPtrStackOuterLoop[nDims];
        bool bEmptyTile = false;
        if( tileIndices == m_anCachedTiledIndices )
        {
            if( !m_bCachedTiledValid )
                return false;
            bEmptyTile = m_bCachedTiledEmpty;
        }
        else
        {
            m_anCachedTiledIndices = tileIndices;
            m_bCachedTiledValid = LoadTileData(tileIndices, bEmptyTile);
            if( !m_bCachedTiledValid )
            {
                return false;
            }
            m_bCachedTiledEmpty = bEmptyTile;
        }

        const GByte* pabySrcTile = m_abyDecodedTileData.empty() ?
                        m_abyRawTileData.data(): m_abyDecodedTileData.data();
        const size_t nSrcDTSize = m_abyDecodedTileData.empty() ? nSourceSize : nDTSize;

        for( size_t i = 0; i < nDims; ++i )
        {
            countInnerLoopInit[i] = 1;
            if( arrayStep[i] != 0 )
            {
                const auto nextBlockIdx = std::min(
                    (1 + indicesOuterLoop[i] / m_anBlockSize[i]) * m_anBlockSize[i],
                    arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(
                        (nextBlockIdx - indicesOuterLoop[i] + arrayStep[i] - 1) / arrayStep[i]);
            }
        }

        if( bEmptyTile && bBothAreNumericDT && abyTargetNoData.empty() )
        {
            abyTargetNoData.resize(nBufferDTSize);
            if( m_pabyNoData )
            {
                GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                &abyTargetNoData[0], bufferDataType);
                bNoDataIsZero = true;
                for( size_t i = 0; i < abyTargetNoData.size(); ++i )
                {
                    if( abyTargetNoData[i] != 0 )
                        bNoDataIsZero = false;
                }
            }
            else
            {
                bNoDataIsZero = true;
                GByte zero = 0;
                GDALCopyWords(&zero, GDT_Byte, 0,
                              &abyTargetNoData[0],
                              bufferDataType.GetNumericDataType(), 0,
                              1);
            }
        }

lbl_next_depth_inner_loop:
        if( dimIdxSubLoop == nDims - 1 )
        {
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            void* dst_ptr = dstPtrStackInnerLoop[dimIdxSubLoop];

            if( m_bUseOptimizedCodePaths &&
                bEmptyTile && bBothAreNumericDT && bNoDataIsZero &&
                nBufferDTSize == dstBufferStrideBytes[dimIdxSubLoop] )
            {
                memset(dst_ptr, 0, nBufferDTSize * countInnerLoopInit[dimIdxSubLoop]);
                goto end_inner_loop;
            }
            else if( m_bUseOptimizedCodePaths &&
                bEmptyTile && !abyTargetNoData.empty() && bBothAreNumericDT &&
                     dstBufferStrideBytes[dimIdxSubLoop] < std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( abyTargetNoData.data(),
                                 bufferDataType.GetNumericDataType(),
                                 0,
                                 dst_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );
                goto end_inner_loop;
            }
            else if( bEmptyTile )
            {
                for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                        ++i,
                        dst_ptr = static_cast<uint8_t*>(dst_ptr) + dstBufferStrideBytes[dimIdxSubLoop] )
                {
                    if( bNoDataIsZero )
                    {
                        if( nBufferDTSize == 1 )
                        {
                            *static_cast<uint8_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 2 )
                        {
                            *static_cast<uint16_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 4 )
                        {
                            *static_cast<uint32_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 8 )
                        {
                            *static_cast<uint64_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 16 )
                        {
                            static_cast<uint64_t*>(dst_ptr)[0] = 0;
                            static_cast<uint64_t*>(dst_ptr)[1] = 0;
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                    }
                    else if( m_pabyNoData )
                    {
                        if( bBothAreNumericDT )
                        {
                            const void* src_ptr_v = abyTargetNoData.data();
                            if( nBufferDTSize == 1 )
                                *static_cast<uint8_t*>(dst_ptr) = *static_cast<const uint8_t*>(src_ptr_v);
                            else if( nBufferDTSize == 2 )
                                *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr_v);
                            else if( nBufferDTSize == 4 )
                                *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr_v);
                            else if( nBufferDTSize == 8 )
                                *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr_v);
                            else if( nBufferDTSize == 16 )
                            {
                                static_cast<uint64_t*>(dst_ptr)[0] = static_cast<const uint64_t*>(src_ptr_v)[0];
                                static_cast<uint64_t*>(dst_ptr)[1] = static_cast<const uint64_t*>(src_ptr_v)[1];
                            }
                            else
                            {
                                CPLAssert(false);
                            }
                        }
                        else
                        {
                            GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                            dst_ptr, bufferDataType);
                        }
                    }
                    else
                    {
                        memset(dst_ptr, 0, nBufferDTSize);
                    }
                }

                goto end_inner_loop;
            }

            size_t nOffset = 0;
            for( size_t i = 0; i < nDims; i++ )
            {
                nOffset = static_cast<size_t>(nOffset * m_anBlockSize[i] +
                    (indicesInnerLoop[i] - tileIndices[i] * m_anBlockSize[i]));
            }
            const GByte* src_ptr = pabySrcTile + nOffset * nSrcDTSize;

            if( m_bUseOptimizedCodePaths && bBothAreNumericDT &&
                arrayStep[dimIdxSubLoop] <= static_cast<GIntBig>(std::numeric_limits<int>::max() / nDTSize) &&
                dstBufferStrideBytes[dimIdxSubLoop] <= std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( src_ptr,
                                 m_oType.GetNumericDataType(),
                                 static_cast<int>(arrayStep[dimIdxSubLoop] * nDTSize),
                                 dst_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );

                goto end_inner_loop;
            }

            for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                    ++i,
                    src_ptr += arrayStep[dimIdxSubLoop] * nSrcDTSize,
                    dst_ptr = static_cast<uint8_t*>(dst_ptr) + dstBufferStrideBytes[dimIdxSubLoop] )
            {
                if( bSameNumericDT )
                {
                    const void* src_ptr_v = src_ptr;
                    if( nSameDTSize == 1 )
                        *static_cast<uint8_t*>(dst_ptr) = *static_cast<const uint8_t*>(src_ptr_v);
                    else if( nSameDTSize == 2 )
                    {
                        *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 4 )
                    {
                        *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 8 )
                    {
                        *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 16 )
                    {
                        static_cast<uint64_t*>(dst_ptr)[0] = static_cast<const uint64_t*>(src_ptr_v)[0];
                        static_cast<uint64_t*>(dst_ptr)[1] = static_cast<const uint64_t*>(src_ptr_v)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if( bSameCompoundAndNoDynamicMem )
                {
                    memcpy(dst_ptr, src_ptr, nDTSize);
                }
                else if( m_oType.GetClass() == GEDTC_STRING )
                {
                    char* pDstStr = static_cast<char*>(CPLMalloc(nSourceSize + 1));
                    memcpy(pDstStr, src_ptr, nSourceSize);
                    pDstStr[nSourceSize] = 0;
                    char** pDstPtr = static_cast<char**>(dst_ptr);
                    memcpy(pDstPtr, &pDstStr, sizeof(char*));
                }
                else
                {
                    GDALExtendedDataType::CopyValue(src_ptr, m_oType,
                                                    dst_ptr, bufferDataType);
                }
            }
        }
        else
        {
            // This level of loop loops over individual samples, within a
            // block
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            countInnerLoop[dimIdxSubLoop] = countInnerLoopInit[dimIdxSubLoop];
            while(true)
            {
                dimIdxSubLoop ++;
                dstPtrStackInnerLoop[dimIdxSubLoop] = dstPtrStackInnerLoop[dimIdxSubLoop-1];
                goto lbl_next_depth_inner_loop;
lbl_return_to_caller_inner_loop:
                dimIdxSubLoop --;
                -- countInnerLoop[dimIdxSubLoop];
                if( countInnerLoop[dimIdxSubLoop] == 0 )
                {
                    break;
                }
                indicesInnerLoop[dimIdxSubLoop] += arrayStep[dimIdxSubLoop];
                dstPtrStackInnerLoop[dimIdxSubLoop] += dstBufferStrideBytes[dimIdxSubLoop];
            }
        }
end_inner_loop:
        if( dimIdxSubLoop > 0 )
            goto lbl_return_to_caller_inner_loop;
    }
    else
    {
        // This level of loop loops over blocks
        indicesOuterLoop[dimIdx] = arrayStartIdx[dimIdx];
        tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        while(true)
        {
            dimIdx ++;
            dstPtrStackOuterLoop[dimIdx] = dstPtrStackOuterLoop[dimIdx - 1];
            goto lbl_next_depth;
lbl_return_to_caller:
            dimIdx --;
            if( count[dimIdx] == 1 || arrayStep[dimIdx] == 0 )
                break;

            size_t nIncr;
            if( static_cast<GUInt64>(arrayStep[dimIdx]) < m_anBlockSize[dimIdx] )
            {
                // Compute index at next block boundary
                auto newIdx = indicesOuterLoop[dimIdx] +
                    (m_anBlockSize[dimIdx] - (indicesOuterLoop[dimIdx] % m_anBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>(
                    (newIdx - indicesOuterLoop[dimIdx] + arrayStep[dimIdx] - 1) / arrayStep[dimIdx]);
            }
            else
            {
                nIncr = 1;
            }
            indicesOuterLoop[dimIdx] += nIncr * arrayStep[dimIdx];
            if( indicesOuterLoop[dimIdx] > arrayStartIdx[dimIdx] + (count[dimIdx]-1) * arrayStep[dimIdx] )
                break;
            dstPtrStackOuterLoop[dimIdx] += bufferStride[dimIdx] * static_cast<GPtrDiff_t>(nIncr * nBufferDTSize);
            tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ZarrDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( !poOpenInfo->bIsDirectory )
    {
        return FALSE;
    }

    CPLString osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                              ".zarray", nullptr );

    VSIStatBufL sStat;
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                    ".zgroup", nullptr );
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    // Zarr V3
    osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                    "zarr.json", nullptr );
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                             ParseDtype()                             */
/************************************************************************/

static size_t GetAlignment(const CPLJSONObject& obj)
{
    if( obj.GetType() == CPLJSONObject::Type::String )
    {
        const auto str = obj.ToString();
        if( str.size() < 3 )
            return 1;
        const char chType = str[1];
        const int nBytes = atoi(str.c_str() + 2);
        if( chType == 'S' )
            return sizeof(char*);
        if( chType == 'c' && nBytes == 8 )
            return sizeof(float);
        if( chType == 'c' && nBytes == 16 )
            return sizeof(double);
       return nBytes;
    }
    else if( obj.GetType() == CPLJSONObject::Type::Array )
    {
        const auto oArray = obj.ToArray();
        size_t nAlignment = 1;
        for( const auto& oElt: oArray )
        {
            const auto oEltArray = oElt.ToArray();
            if( !oEltArray.IsValid() || oEltArray.Size() != 2 ||
                oEltArray[0].GetType() != CPLJSONObject::Type::String )
            {
                return 1;
            }
            nAlignment = std::max(nAlignment, GetAlignment(oEltArray[1]));
            if( nAlignment == sizeof(void*) )
                break;
        }
        return nAlignment;
    }
    return 1;
}

static GDALExtendedDataType ParseDtype(bool isZarrV2,
                                       const CPLJSONObject& obj,
                                       std::vector<DtypeElt>& elts)
{
    const auto AlignOffsetOn = [](size_t offset, size_t alignment)
    {
        return offset + (alignment - (offset % alignment)) % alignment;
    };

    do
    {
        if( obj.GetType() == CPLJSONObject::Type::String )
        {
            const auto str = obj.ToString();
            char chEndianness = 0;
            char chType;
            int nBytes;
            DtypeElt elt;
            if( isZarrV2 )
            {
                if( str.size() < 3 )
                    break;
                chEndianness = str[0];
                chType = str[1];
                nBytes = atoi(str.c_str() + 2);
            }
            else
            {
                if( str.size() < 2 )
                    break;
                if( str == "bool" )
                {
                    chType = 'b';
                    nBytes = 1;
                }
                else if( str == "u1" || str == "i1" )
                {
                    chType = str[0];
                    nBytes = 1;
                }
                else
                {
                    if( str.size() < 3 )
                        break;
                    chEndianness = str[0];
                    chType = str[1];
                    nBytes = atoi(str.c_str() + 2);
                }
            }

            if( chEndianness == '<' )
                elt.needByteSwapping = (CPL_IS_LSB == 0);
            else if( chEndianness == '>' )
                elt.needByteSwapping = (CPL_IS_LSB != 0);

            GDALDataType eDT;
            if( !elts.empty() )
            {
                elt.nativeOffset = elts.back().nativeOffset + elts.back().nativeSize;
            }
            elt.nativeSize = nBytes;
            if( chType == 'b' && nBytes == 1 ) // boolean
            {
                elt.nativeType = DtypeElt::NativeType::BOOLEAN;
                eDT = GDT_Byte;
            }
            else if( chType == 'u' && nBytes == 1 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_Byte;
            }
            else if( chType == 'i' && nBytes == 1 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Int16;
            }
            else if( chType == 'i' && nBytes == 2 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int16;
            }
            else if( chType == 'i' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int32;
            }
            else if( chType == 'i' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float64;
            }
            else if( chType == 'u' && nBytes == 2 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt16;
            }
            else if( chType == 'u' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt32;
            }
            else if( chType == 'u' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float64;
            }
            else if( chType == 'f' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float32;
            }
            else if( chType == 'f' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float64;
            }
            else if( chType == 'c' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat32;
            }
            else if( chType == 'c' && nBytes == 16 )
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat64;
            }
            else if( chType == 'S' )
            {
                elt.nativeType = DtypeElt::NativeType::STRING;
                elt.gdalType = GDALExtendedDataType::CreateString(nBytes);
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString(nBytes);
            }
            else
                break;
            elt.gdalType = GDALExtendedDataType::Create(eDT);
            elt.gdalSize = elt.gdalType.GetSize();
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
        else if( isZarrV2 && obj.GetType() == CPLJSONObject::Type::Array )
        {
            bool error = false;
            const auto oArray = obj.ToArray();
            std::vector<std::unique_ptr<GDALEDTComponent>> comps;
            size_t offset = 0;
            size_t alignmentMax = 1;
            for( const auto& oElt: oArray )
            {
                const auto oEltArray = oElt.ToArray();
                if( !oEltArray.IsValid() || oEltArray.Size() != 2 ||
                    oEltArray[0].GetType() != CPLJSONObject::Type::String )
                {
                    error = true;
                    break;
                }
                GDALExtendedDataType subDT = ParseDtype(isZarrV2, oEltArray[1], elts);
                if( subDT.GetClass() == GEDTC_NUMERIC &&
                    subDT.GetNumericDataType() == GDT_Unknown )
                {
                    error = true;
                    break;
                }

                const std::string osName = oEltArray[0].ToString();
                // Add padding for alignment
                const size_t alignmentSub = GetAlignment(oEltArray[1]);
                alignmentMax = std::max(alignmentMax, alignmentSub);
                offset = AlignOffsetOn(offset, alignmentSub);
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osName, offset, subDT)));
                offset += subDT.GetSize();
            }
            if( error )
                break;
            size_t nTotalSize = offset;
            nTotalSize = AlignOffsetOn(nTotalSize, alignmentMax);
            return GDALExtendedDataType::Create(obj.ToString(),
                                                nTotalSize,
                                                std::move(comps));
        }
    }
    while(false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for dtype");
    return GDALExtendedDataType::Create(GDT_Unknown);
}

static void SetGDALOffset(const GDALExtendedDataType& dt,
                          const size_t nBaseOffset,
                          std::vector<DtypeElt>& elts,
                          size_t& iCurElt)
{
    if( dt.GetClass() == GEDTC_COMPOUND )
    {
        const auto& comps = dt.GetComponents();
        for( const auto& comp: comps )
        {
            const size_t nBaseOffsetSub = nBaseOffset + comp->GetOffset();
            SetGDALOffset(comp->GetType(), nBaseOffsetSub, elts, iCurElt);
        }
    }
    else
    {
        elts[iCurElt].gdalOffset = nBaseOffset;
        iCurElt++;
    }
}

/************************************************************************/
/*                     ZarrGroupBase::LoadArray()                       */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrGroupBase::LoadArray(const std::string& osArrayName,
                                                const std::string& osZarrayFilename,
                                                const CPLJSONObject& oRoot,
                                                bool bLoadedFromZMetadata,
                                                const CPLJSONObject& oAttributesIn) const
{
    const bool isZarrV2 = dynamic_cast<const ZarrGroupV2*>(this) != nullptr;

    if( isZarrV2 )
    {
        const auto osFormat = oRoot["zarr_format"].ToString();
        if( osFormat != "2" )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for zarr_format");
            return nullptr;
        }
    }

    bool bFortranOrder = false;
    const char* orderKey = isZarrV2 ? "order": "chunk_memory_layout";
    const auto osOrder = oRoot[orderKey].ToString();
    if( osOrder == "C" )
    {
        // ok
    }
    else if( osOrder == "F" )
    {
        bFortranOrder = true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for %s", orderKey);
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if( !oShape.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    const char* chunksKey = isZarrV2 ? "chunks": "chunk_grid/chunk_shape";
    const auto oChunks = oRoot[chunksKey].ToArray();
    if( !oChunks.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing or not an array",
                 chunksKey);
        return nullptr;
    }

    if( oShape.Size() != oChunks.Size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }
    if( oShape.Size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "empty shape array not supported");
        return nullptr;
    }

    CPLJSONObject oAttributes(oAttributesIn);
    if( !bLoadedFromZMetadata && isZarrV2 )
    {
        CPLJSONDocument oDoc;
        const std::string osZattrsFilename(
            CPLFormFilename(CPLGetDirname(osZarrayFilename.c_str()), ".zattrs", nullptr));
        CPLErrorHandlerPusher quietError(CPLQuietErrorHandler);
        CPLErrorStateBackuper errorStateBackuper;
        if( oDoc.Load(osZattrsFilename) )
        {
            oAttributes = oDoc.GetRoot();
        }
    }
    else if( !isZarrV2 )
    {
        oAttributes = oRoot["attributes"];
    }

    const auto crs = oAttributes["crs"];
    std::shared_ptr<OGRSpatialReference> poSRS;
    if( crs.GetType() == CPLJSONObject::Type::Object )
    {
        for( const char* key: { "url", "wkt", "projjson" } )
        {
            const auto item = crs[key];
            if( item.IsValid() )
            {
                poSRS = std::make_shared<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if( poSRS->SetFromUserInput(item.ToString().c_str()) == OGRERR_NONE )
                {
                    oAttributes.Delete("crs");
                    break;
                }
                poSRS.reset();
            }
        }
    }

    std::vector<std::shared_ptr<GDALDimension>> aoDims;
    for( int i = 0; i < oShape.Size(); ++i )
    {
        const auto nSize = static_cast<GUInt64>(oShape[i].ToLong());
        if( nSize == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for shape");
            return nullptr;
        }
        aoDims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), CPLSPrintf("dim%d", i),
            std::string(), std::string(), nSize));
    }

    // XArray extension
    const auto arrayDimensionsObj = oAttributes["_ARRAY_DIMENSIONS"];
    if( arrayDimensionsObj.GetType() == CPLJSONObject::Type::Array )
    {
        const auto arrayDims = arrayDimensionsObj.ToArray();
        if( arrayDims.Size() == oShape.Size() )
        {
            bool ok = true;
            for( int i = 0; i < oShape.Size(); ++i )
            {
                if( arrayDims[i].GetType() == CPLJSONObject::Type::String )
                {
                    auto oIter = m_oMapDimensions.find(arrayDims[i].ToString());
                    if( oIter != m_oMapDimensions.end() )
                    {
                        if( oIter->second->GetSize() == aoDims[i]->GetSize() )
                        {
                            aoDims[i] = oIter->second;
                        }
                        else
                        {
                            ok = false;
                            CPLError(CE_Warning, CPLE_AppDefined,
                                 "Size of _ARRAY_DIMENSIONS[%d] different "
                                 "from the one of shape", i);
                        }
                    }
                    else
                    {
                        auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                            GetFullName(), arrayDims[i].ToString(),
                            std::string(), std::string(), aoDims[i]->GetSize());
                        m_oMapDimensions[poDim->GetName()] = poDim;
                        aoDims[i] = poDim;

                        // Try to load the indexing variable
                        // If loading from zmetadata, we will eventually instantiate
                        // the indexing array, so no need to be proactive.
                        if( !bLoadedFromZMetadata && osArrayName != poDim->GetName() )
                        {
                            const std::string osArrayFilenameDim =
                                isZarrV2 ?
                                    CPLFormFilename(
                                        CPLFormFilename(m_osDirectoryName.c_str(),
                                                        poDim->GetName().c_str(),
                                                        nullptr),
                                        ".zarray", nullptr) :
                                    CPLFormFilename(
                                        CPLGetDirname(osZarrayFilename.c_str()),
                                        (poDim->GetName() + ".array.json").c_str(),
                                        nullptr);
                            VSIStatBufL sStat;
                            if( VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0 )
                            {
                                CPLJSONDocument oDoc;
                                if( oDoc.Load(osArrayFilenameDim) )
                                {
                                    auto poDimArray = LoadArray(
                                        poDim->GetName(),
                                        osArrayFilenameDim,
                                        oDoc.GetRoot(),
                                        false,
                                        CPLJSONObject());
                                    if( poDimArray )
                                        poDim->SetIndexingVariable(poDimArray);
                                }
                            }
                        }
                    }
                }
            }
            if( ok )
            {
                oAttributes.Delete("_ARRAY_DIMENSIONS");
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Size of _ARRAY_DIMENSIONS different from the one of shape");
        }
    }

    const char* dtypeKey = isZarrV2 ? "dtype" : "data_type";
    auto oDtype = oRoot[dtypeKey];
    if( !oDtype.IsValid() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s missing", dtypeKey);
        return nullptr;
    }
    if( !isZarrV2 && oDtype["fallback"].IsValid() )
        oDtype = oDtype["fallback"];
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtype(isZarrV2, oDtype, aoDtypeElts);
    if( oType.GetClass() == GEDTC_NUMERIC && oType.GetNumericDataType() == GDT_Unknown )
        return nullptr;
    size_t iCurElt = 0;
    SetGDALOffset(oType, 0, aoDtypeElts, iCurElt);

    std::vector<GUInt64> anBlockSize;
    size_t nBlockSize = oType.GetSize();
    for( const auto& item: oChunks )
    {
        const auto nSize = static_cast<GUInt64>(item.ToLong());
        if( nSize == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for chunks");
            return nullptr;
        }
        if( nBlockSize > std::numeric_limits<size_t>::max() / nSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large chunks");
            return nullptr;
        }
        nBlockSize *= static_cast<size_t>(nSize);
        anBlockSize.emplace_back(nSize);
    }

    std::string osDimSeparator;
    if( isZarrV2 )
    {
        osDimSeparator = oRoot["dimension_separator"].ToString();
        if( osDimSeparator.empty() )
            osDimSeparator = ".";
    }
    else
    {
        osDimSeparator = oRoot["chunk_grid/separator"].ToString();
        if( osDimSeparator.empty() )
            osDimSeparator = "/";
    }

    const auto oFillValue = oRoot["fill_value"];
    if( !oFillValue.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "fill_value missing");
        return nullptr;
    }
    const auto eFillValueType = oFillValue.GetType();
    std::vector<GByte> abyNoData;

    struct NoDataFreer
    {
        std::vector<GByte>& m_abyNodata;
        const GDALExtendedDataType& m_oType;

        NoDataFreer(std::vector<GByte>& abyNoDataIn,
                    const GDALExtendedDataType& oTypeIn) :
                            m_abyNodata(abyNoDataIn), m_oType(oTypeIn) {}

        ~NoDataFreer()
        {
            if( !m_abyNodata.empty() )
                m_oType.FreeDynamicMemory(&m_abyNodata[0]);
        }
    };
    NoDataFreer NoDataFreer(abyNoData, oType);

    if( eFillValueType == CPLJSONObject::Type::Null )
    {
        // Nothing to do
    }
    else if( eFillValueType == CPLJSONObject::Type::String )
    {
        const auto osFillValue = oFillValue.ToString();
        if( oType.GetClass() == GEDTC_NUMERIC )
        {
            double dfNoDataValue;
            if( osFillValue == "NaN" )
            {
                dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
            }
            else if( osFillValue == "Infinity" )
            {
                dfNoDataValue = std::numeric_limits<double>::infinity();
            }
            else if( osFillValue == "-Infinity" )
            {
                dfNoDataValue = -std::numeric_limits<double>::infinity();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            if( oType.GetNumericDataType() == GDT_Float32 )
            {
                const float fNoDataValue = static_cast<float>(dfNoDataValue);
                abyNoData.resize(sizeof(fNoDataValue));
                memcpy( &abyNoData[0], &fNoDataValue, sizeof(fNoDataValue) );
            }
            else if( oType.GetNumericDataType() == GDT_Float64 )
            {
                abyNoData.resize(sizeof(dfNoDataValue));
                memcpy( &abyNoData[0], &dfNoDataValue, sizeof(dfNoDataValue) );
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
        }
        else if( oType.GetClass() == GEDTC_STRING )
        {
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(), osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes + 1);
            abyNativeFillValue[nBytes] = 0;
            abyNoData.resize( oType.GetSize() );
            char* pDstStr = CPLStrdup( reinterpret_cast<const char*>(&abyNativeFillValue[0]) );
            char** pDstPtr = reinterpret_cast<char**>(&abyNoData[0]);
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else
        {
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(), osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes);
            if( abyNativeFillValue.size() != aoDtypeElts.back().nativeOffset +
                                             aoDtypeElts.back().nativeSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            abyNoData.resize( oType.GetSize() );
            DecodeSourceElt( aoDtypeElts,
                             abyNativeFillValue.data(),
                             &abyNoData[0] );
        }
    }
    else if( eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double )
    {
        if( oType.GetClass() == GEDTC_NUMERIC )
        {
            const double dfNoDataValue = oFillValue.ToDouble();
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                          &abyNoData[0], oType.GetNumericDataType(), 0,
                          1);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
        return nullptr;
    }

    const CPLCompressor* psDecompressor = nullptr;
    const auto oCompressor = oRoot["compressor"];
    if( isZarrV2 )
    {
        if( !oCompressor.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "compressor missing");
            return nullptr;
        }
        if( oCompressor.GetType() == CPLJSONObject::Type::Null )
        {
            // nothing to do
        }
        else if( oCompressor.GetType() == CPLJSONObject::Type::Object )
        {
            const auto osCompressorId = oCompressor["id"].ToString();
            if( osCompressorId.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing compressor id");
                return nullptr;
            }
            psDecompressor = CPLGetDecompressor( osCompressorId.c_str() );
            if( psDecompressor == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decompressor %s not handled",
                         osCompressorId.c_str());
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
            return nullptr;
        }
    }
    else if( oCompressor.IsValid() )
    {
        const auto oCodec = oCompressor["codec"];
        if( oCodec.GetType() == CPLJSONObject::Type::String )
        {
            const auto osCodec = oCodec.ToString();
            if( osCodec.find("https://purl.org/zarr/spec/codec/") == 0 )
            {
                auto osCodecName = osCodec.substr(strlen("https://purl.org/zarr/spec/codec/"));
                auto posSlash = osCodecName.find('/');
                if( posSlash != std::string::npos )
                {
                    osCodecName.resize(posSlash);
                    psDecompressor = CPLGetDecompressor( osCodecName.c_str() );
                }
            }
            if( psDecompressor == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decompressor %s not handled",
                         osCodec.c_str());
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
            return nullptr;
        }
    }

    if( isZarrV2 )
    {
        const auto oFilters = oRoot["filters"];
        if( !oFilters.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "filters missing");
            return nullptr;
        }
        if( oFilters.GetType() == CPLJSONObject::Type::Null )
        {
        }
        else
        {
            // TODO
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported filters");
            return nullptr;
        }
    }

    auto poArray = ZarrArray::Create(GetFullName(),
                                     osArrayName,
                                     aoDims, oType, aoDtypeElts, anBlockSize,
                                     bFortranOrder);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
    poArray->SetDecompressor(psDecompressor);
    if( !abyNoData.empty() )
    {
        poArray->RegisterNoDataValue(abyNoData.data());
    }
    poArray->SetSRS(poSRS);
    poArray->SetAttributes(oAttributes);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetVersion(isZarrV2 ? 2 : 3);
    RegisterArray(poArray);

    // If this is an indexing variable, attach it to the dimension.
    if( aoDims.size() == 1 &&
        aoDims[0]->GetName() == poArray->GetName() )
    {
        auto oIter = m_oMapDimensions.find(poArray->GetName());
        if( oIter != m_oMapDimensions.end() )
        {
            oIter->second->SetIndexingVariable(poArray);
        }
    }

    return poArray;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* ZarrDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) ||
        (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 )
    {
        return nullptr;
    }

    CPLString osFilename(poOpenInfo->pszFilename);
    if( osFilename.back() == '/' )
        osFilename.resize(osFilename.size() - 1);

    auto poDS = std::unique_ptr<ZarrDataset>(new ZarrDataset());
    auto poRG = std::make_shared<ZarrGroupV2>(std::string(), "/");
    poDS->m_poRootGroup = poRG;

    const std::string osZarrayFilename(
                CPLFormFilename(poOpenInfo->pszFilename, ".zarray", nullptr));
    VSIStatBufL sStat;
    if( VSIStatL( osZarrayFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osZarrayFilename) )
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        const std::string osArrayName(CPLGetBasename(osFilename.c_str()));
        if( !poRG->LoadArray(osArrayName, osZarrayFilename, oRoot,
                             false, CPLJSONObject()) )
            return nullptr;

        return poDS.release();
    }

    poRG->SetDirectoryName(osFilename);

    const std::string osZmetadataFilename(
            CPLFormFilename(poOpenInfo->pszFilename, ".zmetadata", nullptr));
    if( CPLTestBool(CSLFetchNameValueDef(
                poOpenInfo->papszOpenOptions, "USE_ZMETADATA", "YES")) &&
        VSIStatL( osZmetadataFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osZmetadataFilename) )
            return nullptr;

        poRG->InitFromZMetadata(oDoc.GetRoot());
        return poDS.release();
    }

    const std::string osGroupFilename(
            CPLFormFilename(poOpenInfo->pszFilename, ".zgroup", nullptr));
    if( VSIStatL( osGroupFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osGroupFilename) )
            return nullptr;
        return poDS.release();
    }

    // Zarr v3
    auto poRG_V3 = std::make_shared<ZarrGroupV3>(std::string(), "/");
    poRG_V3->SetDirectoryName(osFilename);
    poDS->m_poRootGroup = poRG_V3;
    return poDS.release();
}


/************************************************************************/
/*                           ZarrDriver()                               */
/************************************************************************/

class ZarrDriver final: public GDALDriver
{
public:
    const char* GetMetadataItem(const char* pszName, const char* pszDomain) override;
};

const char* ZarrDriver::GetMetadataItem(const char* pszName, const char* pszDomain)
{
    if( (pszDomain == nullptr || pszDomain[0] == '\0') &&
        EQUAL(pszName, "COMPRESSORS") )
    {
        // A bit of a hack. Normally GetMetadata() should also return it,
        // but as this is only used for tests, just make GetMetadataItem()
        // handle it
        std::string osCompressors;
        char** decompressors = CPLGetDecompressors();
        for( auto iter = decompressors; iter && *iter; ++iter )
        {
            if( !osCompressors.empty() )
                osCompressors += ',';
            osCompressors += *iter;
        }
        CSLDestroy(decompressors);
        return CPLSPrintf("%s", osCompressors.c_str());
    }
    return GDALDriver::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if( GDALGetDriverByName( "Zarr" ) != nullptr )
        return;

    GDALDriver *poDriver = new ZarrDriver();

    poDriver->SetDescription( "Zarr" );
    // poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Zarr" );
    //poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
    //                           "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='USE_ZMETADATA' type='boolean' description='Whether to use consolidated metadata from .zmetadata' default='YES'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = ZarrDataset::Identify;
    poDriver->pfnOpen = ZarrDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
