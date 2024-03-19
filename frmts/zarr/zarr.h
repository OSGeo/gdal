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
#include <memory>
#include <mutex>
#include <set>

#define ZARR_DEBUG_KEY "ZARR"

#define CRS_ATTRIBUTE_NAME "_CRS"

/************************************************************************/
/*                            ZarrDataset                               */
/************************************************************************/

class ZarrDataset final : public GDALDataset
{
    friend class ZarrRasterBand;

    std::shared_ptr<GDALGroup> m_poRootGroup{};
    CPLStringList m_aosSubdatasets{};
    std::array<double, 6> m_adfGeoTransform{{0.0, 1.0, 0.0, 0.0, 0.0, 1.0}};
    bool m_bHasGT = false;
    std::shared_ptr<GDALDimension> m_poDimX{};
    std::shared_ptr<GDALDimension> m_poDimY{};
    std::shared_ptr<GDALMDArray> m_poSingleArray{};

    static GDALDataset *OpenMultidim(const char *pszFilename, bool bUpdateMode,
                                     CSLConstList papszOpenOptions);

  public:
    explicit ZarrDataset(const std::shared_ptr<GDALGroup> &poRootGroup);
    ~ZarrDataset() override;

    CPLErr FlushCache(bool bAtClosing = false) override;

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList /*papszRootGroupOptions*/,
                           CSLConstList /*papszOptions*/);

    static GDALDataset *Create(const char *pszName, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;
    char **GetMetadata(const char *pszDomain) override;

    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain) override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    CPLErr GetGeoTransform(double *padfTransform) override;
    CPLErr SetGeoTransform(double *padfTransform) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }
};

/************************************************************************/
/*                          ZarrRasterBand                              */
/************************************************************************/

class ZarrRasterBand final : public GDALRasterBand
{
    friend class ZarrDataset;

    std::shared_ptr<GDALMDArray> m_poArray;
    GDALColorInterp m_eColorInterp = GCI_Undefined;

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;
    CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void *pData) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpaceBuf,
                     GSpacing nLineSpaceBuf,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    explicit ZarrRasterBand(const std::shared_ptr<GDALMDArray> &poArray);

    double GetNoDataValue(int *pbHasNoData) override;
    int64_t GetNoDataValueAsInt64(int *pbHasNoData) override;
    uint64_t GetNoDataValueAsUInt64(int *pbHasNoData) override;
    CPLErr SetNoDataValue(double dfNoData) override;
    CPLErr SetNoDataValueAsInt64(int64_t nNoData) override;
    CPLErr SetNoDataValueAsUInt64(uint64_t nNoData) override;
    double GetOffset(int *pbSuccess = nullptr) override;
    CPLErr SetOffset(double dfNewOffset) override;
    double GetScale(int *pbSuccess = nullptr) override;
    CPLErr SetScale(double dfNewScale) override;
    const char *GetUnitType() override;
    CPLErr SetUnitType(const char *pszNewValue) override;
    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorInterpretation(GDALColorInterp eColorInterp) override;
};

/************************************************************************/
/*                        ZarrAttributeGroup()                          */
/************************************************************************/

class ZarrAttributeGroup
{
    // Use a MEMGroup as a convenient container for attributes.
    const bool m_bContainerIsGroup;
    std::shared_ptr<MEMGroup> m_poGroup;
    bool m_bModified = false;

  public:
    explicit ZarrAttributeGroup(const std::string &osParentName,
                                bool bContainerIsGroup);

    void Init(const CPLJSONObject &obj, bool bUpdatable);

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string &osName) const
    {
        return m_poGroup->GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const
    {
        return m_poGroup->GetAttributes(papszOptions);
    }

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList /* papszOptions */ = nullptr)
    {
        auto poAttr = m_poGroup->CreateAttribute(osName, anDimensions,
                                                 oDataType, nullptr);
        if (poAttr)
        {
            m_bModified = true;
        }
        return poAttr;
    }

    bool DeleteAttribute(const std::string &osName)
    {
        const bool bOK = m_poGroup->DeleteAttribute(osName, nullptr);
        if (bOK)
        {
            m_bModified = true;
        }
        return bOK;
    }

    void SetUpdatable(bool bUpdatable)
    {
        auto attrs = m_poGroup->GetAttributes(nullptr);
        for (auto &attr : attrs)
        {
            auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if (memAttr)
                memAttr->SetWritable(bUpdatable);
        }
    }

    void UnsetModified()
    {
        m_bModified = false;
        auto attrs = m_poGroup->GetAttributes(nullptr);
        for (auto &attr : attrs)
        {
            auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if (memAttr)
                memAttr->SetModified(false);
        }
    }

    bool IsModified() const
    {
        if (m_bModified)
            return true;
        const auto attrs = m_poGroup->GetAttributes(nullptr);
        for (const auto &attr : attrs)
        {
            const auto memAttr = std::dynamic_pointer_cast<MEMAttribute>(attr);
            if (memAttr && memAttr->IsModified())
                return true;
        }
        return false;
    }

    CPLJSONObject Serialize() const;

    void ParentRenamed(const std::string &osNewParentFullName);

    void ParentDeleted();
};

/************************************************************************/
/*                         ZarrSharedResource                           */
/************************************************************************/

class ZarrGroupBase;

class ZarrSharedResource
    : public std::enable_shared_from_this<ZarrSharedResource>
{
    bool m_bUpdatable = false;
    std::string m_osRootDirectoryName{};
    bool m_bZMetadataEnabled = false;
    CPLJSONObject m_oObj{};  // For .zmetadata
    bool m_bZMetadataModified = false;
    std::shared_ptr<GDALPamMultiDim> m_poPAM{};
    CPLStringList m_aosOpenOptions{};
    std::weak_ptr<ZarrGroupBase> m_poWeakRootGroup{};
    std::set<std::string> m_oSetArrayInLoading{};

    explicit ZarrSharedResource(const std::string &osRootDirectoryName,
                                bool bUpdatable);

    std::shared_ptr<ZarrGroupBase> OpenRootGroup();

  public:
    static std::shared_ptr<ZarrSharedResource>
    Create(const std::string &osRootDirectoryName, bool bUpdatable);

    ~ZarrSharedResource();

    bool IsUpdatable() const
    {
        return m_bUpdatable;
    }

    void EnableZMetadata()
    {
        m_bZMetadataEnabled = true;
    }

    void SetZMetadataItem(const std::string &osFilename,
                          const CPLJSONObject &obj);

    void DeleteZMetadataItemRecursive(const std::string &osFilename);

    void RenameZMetadataRecursive(const std::string &osOldFilename,
                                  const std::string &osNewFilename);

    const std::shared_ptr<GDALPamMultiDim> &GetPAM()
    {
        return m_poPAM;
    }

    const CPLStringList &GetOpenOptions() const
    {
        return m_aosOpenOptions;
    }

    void SetOpenOptions(CSLConstList papszOpenOptions)
    {
        m_aosOpenOptions = papszOpenOptions;
    }

    void
    UpdateDimensionSize(const std::shared_ptr<GDALDimension> &poUpdatedDim);

    std::shared_ptr<ZarrGroupBase> GetRootGroup()
    {
        auto poRootGroup = m_poWeakRootGroup.lock();
        if (poRootGroup)
            return poRootGroup;
        poRootGroup = OpenRootGroup();
        m_poWeakRootGroup = poRootGroup;
        return poRootGroup;
    }

    void SetRootGroup(const std::shared_ptr<ZarrGroupBase> &poRootGroup)
    {
        m_poWeakRootGroup = poRootGroup;
    }

    bool AddArrayInLoading(const std::string &osZarrayFilename);
    void RemoveArrayInLoading(const std::string &osZarrayFilename);

    struct SetFilenameAdder
    {
        std::shared_ptr<ZarrSharedResource> m_poSharedResource;
        const std::string m_osFilename;
        const bool m_bOK;

        SetFilenameAdder(
            const std::shared_ptr<ZarrSharedResource> &poSharedResource,
            const std::string &osFilename)
            : m_poSharedResource(poSharedResource), m_osFilename(osFilename),
              m_bOK(m_poSharedResource->AddArrayInLoading(m_osFilename))
        {
        }

        ~SetFilenameAdder()
        {
            if (m_bOK)
                m_poSharedResource->RemoveArrayInLoading(m_osFilename);
        }

        bool ok() const
        {
            return m_bOK;
        }
    };
};

/************************************************************************/
/*                             ZarrGroup                                */
/************************************************************************/

class ZarrArray;
class ZarrDimension;

class ZarrGroupBase CPL_NON_FINAL : public GDALGroup
{
  protected:
    friend class ZarrV2Group;
    friend class ZarrV3Group;

    // For ZarrV2, this is the directory of the group
    // For ZarrV3, this is the root directory of the dataset
    std::shared_ptr<ZarrSharedResource> m_poSharedResource;
    std::string m_osDirectoryName{};
    std::weak_ptr<ZarrGroupBase>
        m_poParent{};  // weak reference to owning parent
    std::shared_ptr<ZarrGroupBase>
        m_poParentStrongRef{};  // strong reference, used only when opening from
                                // a subgroup
    mutable std::map<CPLString, std::shared_ptr<ZarrGroupBase>> m_oMapGroups{};
    mutable std::map<CPLString, std::shared_ptr<ZarrArray>> m_oMapMDArrays{};
    mutable std::map<CPLString, std::shared_ptr<ZarrDimension>>
        m_oMapDimensions{};
    mutable bool m_bDirectoryExplored = false;
    mutable std::vector<std::string> m_aosGroups{};
    mutable std::vector<std::string> m_aosArrays{};
    mutable ZarrAttributeGroup m_oAttrGroup;
    mutable bool m_bAttributesLoaded = false;
    bool m_bReadFromZMetadata = false;
    mutable bool m_bDimensionsInstantiated = false;
    bool m_bUpdatable = false;
    bool m_bDimSizeInUpdate = false;

    virtual void ExploreDirectory() const = 0;
    virtual void LoadAttributes() const = 0;

    ZarrGroupBase(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                  const std::string &osParentName, const std::string &osName)
        : GDALGroup(osParentName, osName), m_poSharedResource(poSharedResource),
          m_oAttrGroup(m_osFullName, /*bContainerIsGroup=*/true)
    {
    }

  protected:
    friend class ZarrDimension;
    bool RenameDimension(const std::string &osOldName,
                         const std::string &osNewName);

    void NotifyChildrenOfRenaming() override;

    void NotifyChildrenOfDeletion() override;

  public:
    ~ZarrGroupBase() override;

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        LoadAttributes();
        return m_oAttrGroup.GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override
    {
        LoadAttributes();
        return m_oAttrGroup.GetAttributes(papszOptions);
    }

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr) override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions = nullptr) override;

    std::vector<std::shared_ptr<GDALDimension>>
    GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALDimension>
    CreateDimension(const std::string &osName, const std::string &osType,
                    const std::string &osDirection, GUInt64 nSize,
                    CSLConstList papszOptions = nullptr) override;

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions = nullptr) const override;

    virtual std::shared_ptr<ZarrGroupBase>
    OpenZarrGroup(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const = 0;

    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override
    {
        return std::static_pointer_cast<GDALGroup>(
            OpenZarrGroup(osName, papszOptions));
    }

    bool DeleteGroup(const std::string &osName,
                     CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions = nullptr) const override
    {
        return std::static_pointer_cast<GDALMDArray>(
            OpenZarrArray(osName, papszOptions));
    }

    bool DeleteMDArray(const std::string &osName,
                       CSLConstList papszOptions = nullptr) override;

    virtual std::shared_ptr<ZarrArray>
    OpenZarrArray(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const = 0;

    void SetDirectoryName(const std::string &osDirectoryName)
    {
        m_osDirectoryName = osDirectoryName;
    }

    const std::string &GetDirectoryName() const
    {
        return m_osDirectoryName;
    }

    void RegisterArray(const std::shared_ptr<ZarrArray> &array) const;

    void SetUpdatable(bool bUpdatable)
    {
        m_bUpdatable = bUpdatable;
    }

    void UpdateDimensionSize(const std::shared_ptr<GDALDimension> &poDim);

    static bool IsValidObjectName(const std::string &osName);

    bool Rename(const std::string &osNewName) override;

    //! Returns false in case of error
    bool
    CheckArrayOrGroupWithSameNameDoesNotExist(const std::string &osName) const;

    void ParentRenamed(const std::string &osNewParentFullName) override;

    void NotifyArrayRenamed(const std::string &osOldName,
                            const std::string &osNewName);
};

/************************************************************************/
/*                             ZarrV2Group                              */
/************************************************************************/

class ZarrV2Group final : public ZarrGroupBase
{
    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    std::shared_ptr<ZarrV2Group>
    GetOrCreateSubGroup(const std::string &osSubGroupFullname);

    ZarrV2Group(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName)
        : ZarrGroupBase(poSharedResource, osParentName, osName)
    {
    }

  public:
    static std::shared_ptr<ZarrV2Group>
    Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName);

    ~ZarrV2Group() override;

    static std::shared_ptr<ZarrV2Group>
    CreateOnDisk(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                 const std::string &osParentName, const std::string &osName,
                 const std::string &osDirectoryName);

    std::shared_ptr<ZarrArray>
    OpenZarrArray(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<ZarrGroupBase>
    OpenZarrGroup(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALGroup>
    CreateGroup(const std::string &osName,
                CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<ZarrArray>
    LoadArray(const std::string &osArrayName,
              const std::string &osZarrayFilename, const CPLJSONObject &oRoot,
              bool bLoadedFromZMetadata,
              const CPLJSONObject &oAttributes) const;

    std::shared_ptr<GDALMDArray> CreateMDArray(
        const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        CSLConstList papszOptions = nullptr) override;

    void InitFromZMetadata(const CPLJSONObject &oRoot);
    bool InitFromZGroup(const CPLJSONObject &oRoot);
};

/************************************************************************/
/*                             ZarrV3Group                              */
/************************************************************************/

class ZarrV3Group final : public ZarrGroupBase
{
    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    ZarrV3Group(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::string &osDirectoryName);

  public:
    ~ZarrV3Group() override;

    static std::shared_ptr<ZarrV3Group>
    Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::string &osDirectoryName);

    std::shared_ptr<ZarrArray>
    OpenZarrArray(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<ZarrGroupBase>
    OpenZarrGroup(const std::string &osName,
                  CSLConstList papszOptions = nullptr) const override;

    static std::shared_ptr<ZarrV3Group>
    CreateOnDisk(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                 const std::string &osParentFullName, const std::string &osName,
                 const std::string &osDirectoryName);

    std::shared_ptr<GDALGroup>
    CreateGroup(const std::string &osName,
                CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<ZarrArray> LoadArray(const std::string &osArrayName,
                                         const std::string &osZarrayFilename,
                                         const CPLJSONObject &oRoot) const;

    std::shared_ptr<GDALMDArray> CreateMDArray(
        const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        CSLConstList papszOptions = nullptr) override;

    void SetExplored()
    {
        m_bDirectoryExplored = true;
    }
};

/************************************************************************/
/*                           ZarrDimension                              */
/************************************************************************/

class ZarrDimension final : public GDALDimensionWeakIndexingVar
{
    const bool m_bUpdatable;
    std::weak_ptr<ZarrGroupBase> m_poParentGroup;
    bool m_bModified = false;
    bool m_bXArrayDim = false;

  public:
    ZarrDimension(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                  const std::weak_ptr<ZarrGroupBase> &poParentGroup,
                  const std::string &osParentName, const std::string &osName,
                  const std::string &osType, const std::string &osDirection,
                  GUInt64 nSize)
        : GDALDimensionWeakIndexingVar(osParentName, osName, osType,
                                       osDirection, nSize),
          m_bUpdatable(poSharedResource->IsUpdatable()),
          m_poParentGroup(poParentGroup)
    {
    }

    bool Rename(const std::string &osNewName) override;

    bool IsModified() const
    {
        return m_bModified;
    }

    void SetXArrayDimension()
    {
        m_bXArrayDim = true;
    }

    bool IsXArrayDimension() const
    {
        return m_bXArrayDim;
    }
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
        STRING_ASCII,
        STRING_UNICODE
    };

    NativeType nativeType = NativeType::BOOLEAN;
    size_t nativeOffset = 0;
    size_t nativeSize = 0;
    bool needByteSwapping = false;
    bool gdalTypeIsApproxOfNative = false;
    GDALExtendedDataType gdalType = GDALExtendedDataType::Create(GDT_Unknown);
    size_t gdalOffset = 0;
    size_t gdalSize = 0;
};

/************************************************************************/
/*                      ZarrByteVectorQuickResize                       */
/************************************************************************/

/* std::vector<GByte> with quick resizing (ie that doesn't zero out when
 * growing back to a previously reached greater size).
 */
class ZarrByteVectorQuickResize
{
    std::vector<GByte> m_oVec{};
    size_t m_nSize = 0;

  public:
    ZarrByteVectorQuickResize() = default;

    ZarrByteVectorQuickResize(const ZarrByteVectorQuickResize &) = delete;
    ZarrByteVectorQuickResize &
    operator=(const ZarrByteVectorQuickResize &) = delete;

    ZarrByteVectorQuickResize(ZarrByteVectorQuickResize &&) = default;
    ZarrByteVectorQuickResize &
    operator=(ZarrByteVectorQuickResize &&) = default;

    void resize(size_t nNewSize)
    {
        if (nNewSize > m_oVec.size())
            m_oVec.resize(nNewSize);
        m_nSize = nNewSize;
    }

    inline bool empty() const
    {
        return m_nSize == 0;
    }

    inline size_t size() const
    {
        return m_nSize;
    }

    inline size_t capacity() const
    {
        // Not a typo: the capacity of this object is the size
        // of the underlying std::vector
        return m_oVec.size();
    }

    inline GByte *data()
    {
        return m_oVec.data();
    }

    inline const GByte *data() const
    {
        return m_oVec.data();
    }

    inline GByte operator[](size_t idx) const
    {
        return m_oVec[idx];
    }

    inline GByte &operator[](size_t idx)
    {
        return m_oVec[idx];
    }
};

/************************************************************************/
/*                             ZarrArray                                */
/************************************************************************/

class ZarrArray CPL_NON_FINAL : public GDALPamMDArray
{
  protected:
    std::shared_ptr<ZarrSharedResource> m_poSharedResource;
    const std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    const GDALExtendedDataType m_oType;
    const std::vector<DtypeElt> m_aoDtypeElts;
    const std::vector<GUInt64> m_anBlockSize;
    CPLJSONObject m_dtype{};
    GByte *m_pabyNoData = nullptr;
    std::string m_osDimSeparator{"."};
    std::string m_osFilename{};
    size_t m_nTileSize = 0;
    mutable ZarrByteVectorQuickResize m_abyRawTileData{};
    mutable ZarrByteVectorQuickResize m_abyDecodedTileData{};
    mutable std::vector<uint64_t> m_anCachedTiledIndices{};
    mutable bool m_bCachedTiledValid = false;
    mutable bool m_bCachedTiledEmpty = false;
    mutable bool m_bDirtyTile = false;
    bool m_bUseOptimizedCodePaths = true;
    mutable ZarrAttributeGroup m_oAttrGroup;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};
    mutable bool m_bAllocateWorkingBuffersDone = false;
    mutable bool m_bWorkingBuffersOK = false;
    bool m_bUpdatable = false;
    bool m_bDefinitionModified = false;
    bool m_bSRSModified = false;
    bool m_bNew = false;
    std::string m_osUnit{};
    bool m_bUnitModified = false;
    double m_dfOffset = 0.0;
    bool m_bHasOffset = false;
    bool m_bOffsetModified = false;
    double m_dfScale = 1.0;
    bool m_bHasScale = false;
    bool m_bScaleModified = false;
    std::weak_ptr<ZarrGroupBase> m_poGroupWeak{};
    uint64_t m_nTotalTileCount = 0;
    mutable bool m_bHasTriedCacheTilePresenceArray = false;
    mutable std::shared_ptr<GDALMDArray> m_poCacheTilePresenceArray{};
    mutable std::mutex m_oMutex{};

    struct CachedTile
    {
        ZarrByteVectorQuickResize abyDecoded{};
    };

    mutable std::map<uint64_t, CachedTile> m_oMapTileIndexToCachedTile{};

    static uint64_t
    ComputeTileCount(const std::string &osName,
                     const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                     const std::vector<GUInt64> &anBlockSize);

    ZarrArray(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
              const std::string &osParentName, const std::string &osName,
              const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
              const GDALExtendedDataType &oType,
              const std::vector<DtypeElt> &aoDtypeElts,
              const std::vector<GUInt64> &anBlockSize);

    virtual bool LoadTileData(const uint64_t *tileIndices,
                              bool &bMissingTileOut) const = 0;

    void BlockTranspose(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst, bool bDecode) const;

    virtual bool AllocateWorkingBuffers() const = 0;

    void SerializeNumericNoData(CPLJSONObject &oRoot) const;

    void DeallocateDecodedTileData();

    virtual std::string GetDataDirectory() const = 0;

    virtual CPLStringList
    GetTileIndicesFromFilename(const char *pszFilename) const = 0;

    virtual bool FlushDirtyTile() const = 0;

    std::shared_ptr<GDALMDArray> OpenTilePresenceCache(bool bCanCreate) const;

    void NotifyChildrenOfRenaming() override;

    void NotifyChildrenOfDeletion() override;

    static void EncodeElt(const std::vector<DtypeElt> &elts, const GByte *pSrc,
                          GByte *pDst);

    // Disable copy constructor and assignment operator
    ZarrArray(const ZarrArray &) = delete;
    ZarrArray &operator=(const ZarrArray &) = delete;

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

    bool IsEmptyTile(const ZarrByteVectorQuickResize &abyTile) const;

    bool IAdviseReadCommon(const GUInt64 *arrayStartIdx, const size_t *count,
                           CSLConstList papszOptions,
                           std::vector<uint64_t> &anIndicesCur,
                           int &nThreadsMax,
                           std::vector<uint64_t> &anReqTilesIndices,
                           size_t &nReqTiles) const;

    CPLJSONObject SerializeSpecialAttributes();

    virtual std::string
    BuildTileFilename(const uint64_t *tileIndices) const = 0;

    bool SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                       double dfMean, double dfStdDev, GUInt64 nValidCount,
                       CSLConstList papszOptions) override;

  public:
    ~ZarrArray() override;

    static bool ParseChunkSize(const CPLJSONArray &oChunks,
                               const GDALExtendedDataType &oType,
                               std::vector<GUInt64> &anBlockSize);

    static bool FillBlockSize(
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        std::vector<GUInt64> &anBlockSize, CSLConstList papszOptions);

    bool IsWritable() const override
    {
        return m_bUpdatable;
    }

    const std::string &GetFilename() const override
    {
        return m_osFilename;
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_aoDims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_oType;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_anBlockSize;
    }

    const void *GetRawNoDataValue() const override
    {
        return m_pabyNoData;
    }

    const std::string &GetUnit() const override
    {
        return m_osUnit;
    }

    bool SetUnit(const std::string &osUnit) override;

    void RegisterUnit(const std::string &osUnit)
    {
        m_osUnit = osUnit;
    }

    void RegisterGroup(const std::weak_ptr<ZarrGroupBase> &group)
    {
        m_poGroupWeak = group;
    }

    double GetOffset(bool *pbHasOffset,
                     GDALDataType *peStorageType) const override;

    double GetScale(bool *pbHasScale,
                    GDALDataType *peStorageType) const override;

    bool SetOffset(double dfOffset, GDALDataType eStorageType) override;

    bool SetScale(double dfScale, GDALDataType eStorageType) override;

    std::vector<std::shared_ptr<GDALMDArray>>
    GetCoordinateVariables() const override;

    bool Resize(const std::vector<GUInt64> &anNewDimSizes,
                CSLConstList) override;

    void RegisterOffset(double dfOffset)
    {
        m_bHasOffset = true;
        m_dfOffset = dfOffset;
    }

    void RegisterScale(double dfScale)
    {
        m_bHasScale = true;
        m_dfScale = dfScale;
    }

    bool SetRawNoDataValue(const void *pRawNoData) override;

    void RegisterNoDataValue(const void *);

    void SetFilename(const std::string &osFilename)
    {
        m_osFilename = osFilename;
    }

    void SetDimSeparator(const std::string &osDimSeparator)
    {
        m_osDimSeparator = osDimSeparator;
    }

    void ParseSpecialAttributes(const std::shared_ptr<GDALGroup> &poGroup,
                                CPLJSONObject &oAttributes);

    void SetAttributes(const CPLJSONObject &attrs)
    {
        m_oAttrGroup.Init(attrs, m_bUpdatable);
    }

    void SetSRS(const std::shared_ptr<OGRSpatialReference> &srs)
    {
        m_poSRS = srs;
    }

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        return m_oAttrGroup.GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions) const override
    {
        return m_oAttrGroup.GetAttributes(papszOptions);
    }

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr) override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;

    bool SetSpatialRef(const OGRSpatialReference *poSRS) override;

    void SetUpdatable(bool bUpdatable)
    {
        m_bUpdatable = bUpdatable;
    }

    void SetDtype(const CPLJSONObject &dtype)
    {
        m_dtype = dtype;
    }

    void SetDefinitionModified(bool bModified)
    {
        m_bDefinitionModified = bModified;
    }

    void SetNew(bool bNew)
    {
        m_bNew = bNew;
    }

    bool Rename(const std::string &osNewName) override;

    void ParentRenamed(const std::string &osNewParentFullName) override;

    virtual void Flush() = 0;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poSharedResource->GetRootGroup();
    }

    bool CacheTilePresence();

    static void DecodeSourceElt(const std::vector<DtypeElt> &elts,
                                const GByte *pSrc, GByte *pDst);

    static void GetDimensionTypeDirection(CPLJSONObject &oAttributes,
                                          std::string &osType,
                                          std::string &osDirection);
};

/************************************************************************/
/*                           ZarrV2Array                                */
/************************************************************************/

class ZarrV2Array final : public ZarrArray
{
    CPLJSONObject m_oCompressorJSon{};
    const CPLCompressor *m_psCompressor = nullptr;
    std::string m_osDecompressorId{};
    const CPLCompressor *m_psDecompressor = nullptr;
    CPLJSONArray m_oFiltersArray{};  // ZarrV2 specific
    bool m_bFortranOrder = false;
    mutable ZarrByteVectorQuickResize
        m_abyTmpRawTileData{};  // used for Fortran order

    ZarrV2Array(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                const GDALExtendedDataType &oType,
                const std::vector<DtypeElt> &aoDtypeElts,
                const std::vector<GUInt64> &anBlockSize, bool bFortranOrder);

    void Serialize();

    bool LoadTileData(const uint64_t *tileIndices, bool bUseMutex,
                      const CPLCompressor *psDecompressor,
                      ZarrByteVectorQuickResize &abyRawTileData,
                      ZarrByteVectorQuickResize &abyTmpRawTileData,
                      ZarrByteVectorQuickResize &abyDecodedTileData,
                      bool &bMissingTileOut) const;

    bool NeedDecodedBuffer() const;

    bool
    AllocateWorkingBuffers(ZarrByteVectorQuickResize &abyRawTileData,
                           ZarrByteVectorQuickResize &abyTmpRawTileData,
                           ZarrByteVectorQuickResize &abyDecodedTileData) const;

    // Disable copy constructor and assignment operator
    ZarrV2Array(const ZarrV2Array &) = delete;
    ZarrV2Array &operator=(const ZarrV2Array &) = delete;

  public:
    ~ZarrV2Array() override;

    static std::shared_ptr<ZarrV2Array>
    Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
           const GDALExtendedDataType &oType,
           const std::vector<DtypeElt> &aoDtypeElts,
           const std::vector<GUInt64> &anBlockSize, bool bFortranOrder);

    void SetCompressorJson(const CPLJSONObject &oCompressor)
    {
        m_oCompressorJSon = oCompressor;
    }

    void SetCompressorDecompressor(const std::string &osDecompressorId,
                                   const CPLCompressor *psComp,
                                   const CPLCompressor *psDecomp)
    {
        m_psCompressor = psComp;
        m_osDecompressorId = osDecompressorId;
        m_psDecompressor = psDecomp;
    }

    void SetFilters(const CPLJSONArray &oFiltersArray)
    {
        m_oFiltersArray = oFiltersArray;
    }

    void Flush() override;

  protected:
    std::string GetDataDirectory() const override;

    CPLStringList
    GetTileIndicesFromFilename(const char *pszFilename) const override;

    bool FlushDirtyTile() const override;

    std::string BuildTileFilename(const uint64_t *tileIndices) const override;

    bool AllocateWorkingBuffers() const override;

    bool LoadTileData(const uint64_t *tileIndices,
                      bool &bMissingTileOut) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                        ZarrArrayMetadata                             */
/************************************************************************/

struct ZarrArrayMetadata
{
    DtypeElt oElt{};
    std::vector<size_t> anBlockSizes{};

    size_t GetEltCount() const
    {
        size_t n = 1;
        for (auto i : anBlockSizes)
            n *= i;
        return n;
    }
};

/************************************************************************/
/*                            ZarrV3Codec                               */
/************************************************************************/

class ZarrV3Codec CPL_NON_FINAL
{
  protected:
    const std::string m_osName;
    CPLJSONObject m_oConfiguration{};
    ZarrArrayMetadata m_oInputArrayMetadata{};

    ZarrV3Codec(const std::string &osName);

  public:
    virtual ~ZarrV3Codec() = 0;

    enum class IOType
    {
        BYTES,
        ARRAY
    };

    virtual IOType GetInputType() const = 0;
    virtual IOType GetOutputType() const = 0;

    virtual bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata) = 0;

    virtual std::unique_ptr<ZarrV3Codec> Clone() const = 0;

    virtual bool IsNoOp() const
    {
        return false;
    }

    virtual bool Encode(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst) const = 0;
    virtual bool Decode(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst) const = 0;

    const std::string &GetName() const
    {
        return m_osName;
    }

    const CPLJSONObject &GetConfiguration() const
    {
        return m_oConfiguration;
    }
};

/************************************************************************/
/*                           ZarrV3CodecGZip                            */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/gzip/v1.0.html
class ZarrV3CodecGZip final : public ZarrV3Codec
{
    CPLStringList m_aosCompressorOptions{};
    const CPLCompressor *m_pDecompressor = nullptr;
    const CPLCompressor *m_pCompressor = nullptr;

    ZarrV3CodecGZip(const ZarrV3CodecGZip &) = delete;
    ZarrV3CodecGZip &operator=(const ZarrV3CodecGZip &) = delete;

  public:
    static constexpr const char *NAME = "gzip";

    ZarrV3CodecGZip();
    ~ZarrV3CodecGZip() override;

    IOType GetInputType() const override
    {
        return IOType::BYTES;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    static CPLJSONObject GetConfiguration(int nLevel);

    bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                          ZarrV3CodecBlosc                            */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/blosc/v1.0.html
class ZarrV3CodecBlosc final : public ZarrV3Codec
{
    CPLStringList m_aosCompressorOptions{};
    const CPLCompressor *m_pDecompressor = nullptr;
    const CPLCompressor *m_pCompressor = nullptr;

    ZarrV3CodecBlosc(const ZarrV3CodecBlosc &) = delete;
    ZarrV3CodecBlosc &operator=(const ZarrV3CodecBlosc &) = delete;

  public:
    static constexpr const char *NAME = "blosc";

    ZarrV3CodecBlosc();
    ~ZarrV3CodecBlosc() override;

    IOType GetInputType() const override
    {
        return IOType::BYTES;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    static CPLJSONObject GetConfiguration(const char *cname, int clevel,
                                          const char *shuffle, int typesize,
                                          int blocksize);

    bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                           ZarrV3CodecEndian                          */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/endian/v1.0.html
class ZarrV3CodecEndian final : public ZarrV3Codec
{
    bool m_bLittle = true;

  public:
    static constexpr const char *NAME = "endian";

    ZarrV3CodecEndian();
    ~ZarrV3CodecEndian() override;

    IOType GetInputType() const override
    {
        return IOType::ARRAY;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    static CPLJSONObject GetConfiguration(bool bLittle);

    bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata) override;

#if CPL_IS_LSB
    bool IsNoOp() const override
    {
        return m_oInputArrayMetadata.oElt.nativeSize == 1 || m_bLittle;
    }
#else
    bool IsNoOp() const override
    {
        return m_oInputArrayMetadata.oElt.nativeSize == 1 || !m_bLittle;
    }
#endif

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                         ZarrV3CodecTranspose                         */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/transpose/v1.0.html
class ZarrV3CodecTranspose final : public ZarrV3Codec
{
    // m_anOrder is such that dest_shape[i] = source_shape[m_anOrder[i]]
    // where source_shape[] is the size of the array before the Encode() operation
    // and dest_shape[] its size after.
    // m_anOrder[] describes a bijection of [0,N-1] to [0,N-1]
    std::vector<int> m_anOrder{};

    // m_anReverseOrder is such that m_anReverseOrder[m_anOrder[i]] = i
    std::vector<int> m_anReverseOrder{};

    bool Transpose(const ZarrByteVectorQuickResize &abySrc,
                   ZarrByteVectorQuickResize &abyDst,
                   bool bEncodeDirection) const;

  public:
    static constexpr const char *NAME = "transpose";

    ZarrV3CodecTranspose();
    ~ZarrV3CodecTranspose() override;

    IOType GetInputType() const override
    {
        return IOType::ARRAY;
    }

    IOType GetOutputType() const override
    {
        return IOType::ARRAY;
    }

    static CPLJSONObject GetConfiguration(const std::vector<int> &anOrder);
    static CPLJSONObject GetConfiguration(const std::string &osOrder);

    bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata) override;

    bool IsNoOp() const override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                          ZarrV3CodecSequence                         */
/************************************************************************/

class ZarrV3CodecSequence
{
    const ZarrArrayMetadata m_oInputArrayMetadata;
    std::vector<std::unique_ptr<ZarrV3Codec>> m_apoCodecs{};
    CPLJSONObject m_oCodecArray{};
    ZarrByteVectorQuickResize m_abyTmp{};

    bool AllocateBuffer(ZarrByteVectorQuickResize &abyBuffer);

  public:
    explicit ZarrV3CodecSequence(const ZarrArrayMetadata &oInputArrayMetadata)
        : m_oInputArrayMetadata(oInputArrayMetadata)
    {
    }

    // This method is not thread safe due to cloning a JSON object
    std::unique_ptr<ZarrV3CodecSequence> Clone() const;

    bool InitFromJson(const CPLJSONObject &oCodecs);

    const CPLJSONObject &GetJSon() const
    {
        return m_oCodecArray;
    }

    bool Encode(ZarrByteVectorQuickResize &abyBuffer);
    bool Decode(ZarrByteVectorQuickResize &abyBuffer);
};

/************************************************************************/
/*                           ZarrV3Array                                */
/************************************************************************/

class ZarrV3Array final : public ZarrArray
{
    bool m_bV2ChunkKeyEncoding = false;
    std::unique_ptr<ZarrV3CodecSequence> m_poCodecs{};

    ZarrV3Array(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                const GDALExtendedDataType &oType,
                const std::vector<DtypeElt> &aoDtypeElts,
                const std::vector<GUInt64> &anBlockSize);

    void Serialize(const CPLJSONObject &oAttrs);

    bool NeedDecodedBuffer() const;

    bool
    AllocateWorkingBuffers(ZarrByteVectorQuickResize &abyRawTileData,
                           ZarrByteVectorQuickResize &abyDecodedTileData) const;

    bool LoadTileData(const uint64_t *tileIndices, bool bUseMutex,
                      ZarrV3CodecSequence *poCodecs,
                      ZarrByteVectorQuickResize &abyRawTileData,
                      ZarrByteVectorQuickResize &abyDecodedTileData,
                      bool &bMissingTileOut) const;

  public:
    ~ZarrV3Array() override;

    static std::shared_ptr<ZarrV3Array>
    Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
           const GDALExtendedDataType &oType,
           const std::vector<DtypeElt> &aoDtypeElts,
           const std::vector<GUInt64> &anBlockSize);

    void SetIsV2ChunkKeyEncoding(bool b)
    {
        m_bV2ChunkKeyEncoding = b;
    }

    void SetCodecs(std::unique_ptr<ZarrV3CodecSequence> &&poCodecs)
    {
        m_poCodecs = std::move(poCodecs);
    }

    void Flush() override;

  protected:
    std::string GetDataDirectory() const override;

    CPLStringList
    GetTileIndicesFromFilename(const char *pszFilename) const override;

    bool AllocateWorkingBuffers() const override;

    bool FlushDirtyTile() const override;

    std::string BuildTileFilename(const uint64_t *tileIndices) const override;

    bool LoadTileData(const uint64_t *tileIndices,
                      bool &bMissingTileOut) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;
};

#endif  // ZARR_H
