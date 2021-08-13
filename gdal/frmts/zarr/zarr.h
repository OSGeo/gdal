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

#ifndef ZARR_H
#define ZARR_H

#include "cpl_compressor.h"
#include "cpl_json.h"
#include "gdal_priv.h"
#include "gdal_pam.h"
#include "memmultidim.h"

#include <array>
#include <map>
#include <set>

/************************************************************************/
/*                            ZarrDataset                               */
/************************************************************************/

class ZarrDataset final: public GDALDataset
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    CPLStringList              m_aosSubdatasets{};
    std::array<double,6>       m_adfGeoTransform{{ 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 }};
    bool                       m_bHasGT = false;
    std::shared_ptr<GDALDimension> m_poDimX{};
    std::shared_ptr<GDALDimension> m_poDimY{};

    static GDALDataset* OpenMultidim(const char* pszFilename,
                                     bool bUpdateMode,
                                     CSLConstList papszOpenOptions);

public:

    explicit ZarrDataset(const std::shared_ptr<GDALGroup>& poRootGroup);

    static int Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateMultiDimensional( const char * pszFilename,
                                                CSLConstList /*papszRootGroupOptions*/,
                                                CSLConstList /*papszOptions*/ );

    static GDALDataset*  Create( const char * pszName,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char ** papszOptions );


    const char* GetMetadataItem(const char* pszName, const char* pszDomain) override;
    char** GetMetadata(const char* pszDomain) override;

    CPLErr SetMetadata(char** papszMetadata, const char* pszDomain) override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double * padfTransform ) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }
};

/************************************************************************/
/*                          ZarrRasterBand                              */
/************************************************************************/

class ZarrRasterBand final: public GDALRasterBand
{
    friend class ZarrDataset;

    std::shared_ptr<GDALMDArray> m_poArray;

protected:
    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void * pData ) override;
    CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void * pData ) override;
    CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg ) override;

public:
    explicit ZarrRasterBand(const std::shared_ptr<GDALMDArray>& poArray);

    double GetNoDataValue(int* pbHasNoData) override;
    CPLErr SetNoDataValue(double dfNoData) override;
    double GetOffset( int *pbSuccess = nullptr ) override;
    CPLErr SetOffset( double dfNewOffset ) override;
    double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double dfNewScale ) override;
    const char *GetUnitType() override;
    CPLErr SetUnitType( const char * pszNewValue ) override;
};

/************************************************************************/
/*                        ZarrAttributeGroup()                          */
/************************************************************************/

class ZarrAttributeGroup
{
    // Use a MEMGroup as a convenient container for attributes.
    MEMGroup m_oGroup;
    bool m_bModified = false;

public:
    explicit ZarrAttributeGroup(const std::string& osParentName);

    void Init(const CPLJSONObject& obj, bool bUpdatable);

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const { return m_oGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const { return m_oGroup.GetAttributes(papszOptions); }

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList /* papszOptions */ = nullptr )
    {
        auto poAttr = m_oGroup.CreateAttribute(osName, anDimensions, oDataType, nullptr);
        if( poAttr )
        {
            m_bModified = true;
        }
        return poAttr;
    }

    void SetUpdatable(bool bUpdatable)
    {
        auto attrs = m_oGroup.GetAttributes(nullptr);
        for( auto attr: attrs )
        {
            auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if( memAttr )
                memAttr->SetWritable(bUpdatable);
        }
    }

    void UnsetModified()
    {
        m_bModified = false;
        auto attrs = m_oGroup.GetAttributes(nullptr);
        for( auto& attr: attrs )
        {
            auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if( memAttr )
                memAttr->SetModified(false);
        }
    }

    bool IsModified() const
    {
        if( m_bModified )
            return true;
        const auto attrs = m_oGroup.GetAttributes(nullptr);
        for( const auto& attr: attrs )
        {
            const auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if( memAttr && memAttr->IsModified() )
                return true;
        }
        return false;
    }

    CPLJSONObject Serialize() const;
};

/************************************************************************/
/*                         ZarrSharedResource                           */
/************************************************************************/

class ZarrSharedResource
{
    std::string m_osRootDirectoryName{};
    bool m_bZMetadataEnabled = false;
    CPLJSONObject m_oObj{}; // For .zmetadata
    bool m_bZMetadataModified = false;
    std::shared_ptr<GDALPamMultiDim> m_poPAM{};

public:
    explicit ZarrSharedResource(const std::string& osRootDirectoryName);

    ~ZarrSharedResource();

    void EnableZMetadata() { m_bZMetadataEnabled = true; }

    void InitFromZMetadata(const CPLJSONObject& obj) { m_oObj = obj; }

    void SetZMetadataItem(const std::string& osFilename, const CPLJSONObject& obj);

    const std::shared_ptr<GDALPamMultiDim>& GetPAM() { return m_poPAM; }
};

/************************************************************************/
/*                             ZarrGroup                                */
/************************************************************************/

class ZarrArray;

class ZarrGroupBase CPL_NON_FINAL: public GDALGroup
{
protected:
    // For ZarrV2, this is the directory of the group
    // For ZarrV3, this is the root directory of the dataset
    std::shared_ptr<ZarrSharedResource> m_poSharedResource;
    std::string m_osDirectoryName{};
    std::weak_ptr<ZarrGroupBase> m_poParent{}; // weak reference to owning parent
    std::shared_ptr<ZarrGroupBase> m_poParentStrongRef{}; // strong reference, used only when opening from a subgroup
    mutable std::map<CPLString, std::shared_ptr<GDALGroup>> m_oMapGroups{};
    mutable std::map<CPLString, std::shared_ptr<ZarrArray>> m_oMapMDArrays{};
    mutable std::map<CPLString, std::shared_ptr<GDALDimensionWeakIndexingVar>> m_oMapDimensions{};
    mutable bool m_bDirectoryExplored = false;
    mutable std::vector<std::string> m_aosGroups{};
    mutable std::vector<std::string> m_aosArrays{};
    mutable ZarrAttributeGroup                        m_oAttrGroup;
    mutable bool                                      m_bAttributesLoaded = false;
    bool                                              m_bReadFromZMetadata = false;
    mutable bool                                      m_bDimensionsInstantiated = false;
    bool                                              m_bUpdatable = false;
    std::weak_ptr<ZarrGroupBase> m_pSelf{};

    virtual void ExploreDirectory() const = 0;
    virtual void LoadAttributes() const = 0;

    ZarrGroupBase(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                  const std::string& osParentName, const std::string& osName):
        GDALGroup(osParentName, osName),
        m_poSharedResource(poSharedResource),
        m_oAttrGroup(osParentName) {}

public:

    ~ZarrGroupBase() override;

    void SetSelf(std::weak_ptr<ZarrGroupBase> self) { m_pSelf = self; }

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override
        { LoadAttributes(); return m_oAttrGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override
        { LoadAttributes(); return m_oAttrGroup.GetAttributes(papszOptions); }

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions = nullptr) override;

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALDimension> CreateDimension(const std::string& osName,
                                                   const std::string& osType,
                                                   const std::string& osDirection,
                                                   GUInt64 nSize,
                                                   CSLConstList papszOptions = nullptr) override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions = nullptr) const override;

    void SetDirectoryName(const std::string& osDirectoryName) { m_osDirectoryName = osDirectoryName; }

    std::shared_ptr<ZarrArray> LoadArray(const std::string& osArrayName,
                                         const std::string& osZarrayFilename,
                                         const CPLJSONObject& oRoot,
                                         bool bLoadedFromZMetadata,
                                         const CPLJSONObject& oAttributes,
                                         std::set<std::string>& oSetFilenamesInLoading) const;
    void RegisterArray(const std::shared_ptr<ZarrArray>& array) const;

    void SetUpdatable(bool bUpdatable) { m_bUpdatable = bUpdatable; }
};

/************************************************************************/
/*                             ZarrGroupV2                              */
/************************************************************************/

class ZarrGroupV2 final: public ZarrGroupBase
{
    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    std::shared_ptr<ZarrGroupV2> GetOrCreateSubGroup(
                                        const std::string& osSubGroupFullname);

    ZarrGroupV2(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                const std::string& osParentName, const std::string& osName):
        ZarrGroupBase(poSharedResource, osParentName, osName) {}

public:
    static std::shared_ptr<ZarrGroupV2> Create(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                               const std::string& osParentName,
                                               const std::string& osName);

    ~ZarrGroupV2() override;

    static std::shared_ptr<ZarrGroupV2> CreateOnDisk(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                                     const std::string& osParentName,
                                                     const std::string& osName,
                                                     const std::string& osDirectoryName);

    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                           CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                               const GDALExtendedDataType& oDataType,
                                               CSLConstList papszOptions = nullptr) override;

    void InitFromZMetadata(const CPLJSONObject& oRoot);
    bool InitFromZGroup(const CPLJSONObject& oRoot);
};

/************************************************************************/
/*                             ZarrGroupV3                              */
/************************************************************************/

class ZarrGroupV3 final: public ZarrGroupBase
{
    std::string m_osGroupFilename;
    bool m_bNew = false;

    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    ZarrGroupV3(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                const std::string& osParentName,
                const std::string& osName,
                const std::string& osRootDirectoryName);
public:

    ~ZarrGroupV3() override;

    static std::shared_ptr<ZarrGroupV3> Create(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                               const std::string& osParentName,
                                               const std::string& osName,
                                               const std::string& osRootDirectoryName);

    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    static std::shared_ptr<ZarrGroupV3> CreateOnDisk(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                                     const std::string& osParentFullName,
                                                     const std::string& osName,
                                                     const std::string& osRootDirectoryName);

    std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                           CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                               const GDALExtendedDataType& oDataType,
                                               CSLConstList papszOptions = nullptr) override;
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

class ZarrArray final: public GDALPamMDArray
{
    std::shared_ptr<ZarrSharedResource>               m_poSharedResource;
    const std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    const GDALExtendedDataType                        m_oType;
    const std::vector<DtypeElt>                       m_aoDtypeElts;
    const std::vector<GUInt64>                        m_anBlockSize;
    CPLJSONObject                                     m_dtype{};
    GByte                                            *m_pabyNoData = nullptr;
    std::string                                       m_osDimSeparator { "." };
    std::string                                       m_osFilename{};
    mutable std::vector<GByte>                        m_abyRawTileData{};
    mutable std::vector<GByte>                        m_abyDecodedTileData{};
    mutable std::vector<uint64_t>                     m_anCachedTiledIndices{};
    mutable bool                                      m_bCachedTiledValid = false;
    mutable bool                                      m_bCachedTiledEmpty = false;
    mutable bool                                      m_bDirtyTile = false;
    bool                                              m_bUseOptimizedCodePaths = true;
    bool                                              m_bFortranOrder = false;
    const CPLCompressor                              *m_psCompressor = nullptr;
    const CPLCompressor                              *m_psDecompressor = nullptr;
    CPLJSONObject                                     m_oCompressorJSonV2{};
    CPLJSONObject                                     m_oCompressorJSonV3{};
    CPLJSONArray                                      m_oFiltersArray{};
    mutable std::vector<GByte>                        m_abyTmpRawTileData{}; // used for Fortran order
    mutable ZarrAttributeGroup                        m_oAttrGroup;
    mutable std::shared_ptr<OGRSpatialReference>      m_poSRS{};
    mutable bool                                      m_bAllocateWorkingBuffersDone = false;
    mutable bool                                      m_bWorkingBuffersOK = false;
    std::string                                       m_osRootDirectoryName{};
    int                                               m_nVersion = 0;
    bool                                              m_bUpdatable = false;
    bool                                              m_bDefinitionModified = false;
    bool                                              m_bSRSModified = false;
    bool                                              m_bNew = false;
    std::string                                       m_osUnit{};
    bool                                              m_bUnitModified = false;
    double                                            m_dfOffset = 0.0;
    bool                                              m_bHasOffset = false;
    bool                                              m_bOffsetModified = false;
    double                                            m_dfScale = 1.0;
    bool                                              m_bHasScale = false;
    bool                                              m_bScaleModified = false;
    std::weak_ptr<GDALGroup>                          m_poGroupWeak{};

    ZarrArray(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
              const std::string& osParentName,
              const std::string& osName,
              const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
              const GDALExtendedDataType& oType,
              const std::vector<DtypeElt>& aoDtypeElts,
              const std::vector<GUInt64>& anBlockSize,
              bool bFortranOrder);

    bool LoadTileData(const std::vector<uint64_t>& tileIndices,
                      bool& bMissingTileOut) const;
    void BlockTranspose(const std::vector<GByte>& abySrc,
                        std::vector<GByte>& abyDst,
                        bool bDecode) const;

    bool AllocateWorkingBuffers() const;

    void SerializeV2();

    void SerializeV3(const CPLJSONObject& oAttrs);

    void DeallocateDecodedTileData();

    bool FlushDirtyTile() const;

    // Disable copy constructor and assignment operator
    ZarrArray(const ZarrArray&) = delete;
    ZarrArray& operator= (const ZarrArray&) = delete;

protected:
    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;

    bool IsCacheable() const override { return false; }

public:
    ~ZarrArray() override;

    static std::shared_ptr<ZarrArray> Create(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                             const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder);

    bool IsWritable() const override { return m_bUpdatable; }

    const std::string& GetFilename() const override { return m_osFilename; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_aoDims; }

    const GDALExtendedDataType& GetDataType() const override { return m_oType; }

    std::vector<GUInt64> GetBlockSize() const override { return m_anBlockSize; }

    const void* GetRawNoDataValue() const override { return m_pabyNoData; }

    const std::string& GetUnit() const override { return m_osUnit; }

    bool SetUnit(const std::string& osUnit) override;

    void RegisterUnit(const std::string& osUnit) { m_osUnit = osUnit; }

    void RegisterGroup(std::weak_ptr<GDALGroup> group) { m_poGroupWeak = group; }

    double GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const override;

    double GetScale(bool* pbHasScale, GDALDataType* peStorageType) const override;

    bool SetOffset(double dfOffset, GDALDataType eStorageType) override;

    bool SetScale(double dfScale, GDALDataType eStorageType) override;

    std::vector<std::shared_ptr<GDALMDArray>> GetCoordinateVariables() const override;

    void RegisterOffset(double dfOffset) { m_bHasOffset = true; m_dfOffset = dfOffset; }

    void RegisterScale(double dfScale) { m_bHasScale = true; m_dfScale = dfScale; }

    bool SetRawNoDataValue(const void* pRawNoData) override;

    void RegisterNoDataValue(const void*);

    void SetFilename(const std::string& osFilename ) { m_osFilename = osFilename; }

    void SetDimSeparator(const std::string& osDimSeparator) { m_osDimSeparator = osDimSeparator; }

    void SetCompressorDecompressor(const CPLCompressor* psComp,
                                   const CPLCompressor* psDecomp) {
        m_psCompressor = psComp;
        m_psDecompressor = psDecomp;
    }

    void SetFilters(const CPLJSONArray& oFiltersArray) { m_oFiltersArray = oFiltersArray; }

    void SetAttributes(const CPLJSONObject& attrs) { m_oAttrGroup.Init(attrs, m_bUpdatable); }

    void SetSRS(const std::shared_ptr<OGRSpatialReference>& srs) { m_poSRS = srs; }

    void SetRootDirectoryName(const std::string& osRootDirectoryName) { m_osRootDirectoryName = osRootDirectoryName; }

    void SetVersion(int nVersion) { m_nVersion = nVersion; }

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override
        { return m_oAttrGroup.GetAttribute(osName); }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override
        { return m_oAttrGroup.GetAttributes(papszOptions); }

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;

    bool SetSpatialRef(const OGRSpatialReference* poSRS) override;

    void SetUpdatable(bool bUpdatable) { m_bUpdatable = bUpdatable; }

    void SetDtype(const CPLJSONObject& dtype) { m_dtype = dtype; }

    void SetDefinitionModified(bool bModified) { m_bDefinitionModified = bModified; }

    void SetCompressorJsonV2(const CPLJSONObject& oCompressor) { m_oCompressorJSonV2 = oCompressor; }

    void SetCompressorJsonV3(const CPLJSONObject& oCompressor) { m_oCompressorJSonV3 = oCompressor; }

    void SetNew(bool bNew) { m_bNew = bNew; }

    void Flush();
};

#endif // ZARR_H
