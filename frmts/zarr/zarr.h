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

#ifndef ZARR_H
#define ZARR_H

#include "cpl_compressor.h"
#include "cpl_json.h"
#include "gdal_priv.h"
#include "gdal_pam_multidim.h"
#include "memmultidim.h"

#include <array>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>

#define ZARR_DEBUG_KEY "ZARR"

#define CRS_ATTRIBUTE_NAME "_CRS"

const CPLCompressor *ZarrGetShuffleCompressor();
const CPLCompressor *ZarrGetShuffleDecompressor();
const CPLCompressor *ZarrGetQuantizeDecompressor();
const CPLCompressor *ZarrGetTIFFDecompressor();
const CPLCompressor *ZarrGetFixedScaleOffsetDecompressor();

/************************************************************************/
/*                           MultiplyElements()                         */
/************************************************************************/

/** Return the product of elements in the vector
 */
template <class T> inline T MultiplyElements(const std::vector<T> &vector)
{
    return std::reduce(vector.begin(), vector.end(), T{1},
                       std::multiplies<T>{});
}

/************************************************************************/
/*                            ZarrDataset                               */
/************************************************************************/

class ZarrGroupBase;

class ZarrDataset final : public GDALDataset
{
    friend class ZarrRasterBand;

    std::shared_ptr<ZarrGroupBase> m_poRootGroup{};
    CPLStringList m_aosSubdatasets{};
    GDALGeoTransform m_gt{};
    bool m_bHasGT = false;
    std::shared_ptr<GDALDimension> m_poDimX{};
    std::shared_ptr<GDALDimension> m_poDimY{};
    std::shared_ptr<GDALMDArray> m_poSingleArray{};

    static GDALDataset *OpenMultidim(const char *pszFilename, bool bUpdateMode,
                                     CSLConstList papszOpenOptions);

  public:
    explicit ZarrDataset(const std::shared_ptr<ZarrGroupBase> &poRootGroup);
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

    static GDALDataset *CreateCopy(const char *, GDALDataset *, int,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;
    CSLConstList GetMetadata(const char *pszDomain) override;

    CPLErr SetMetadata(CSLConstList papszMetadata,
                       const char *pszDomain) override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override;
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

    bool Close();

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

class ZarrSharedResource
    : public std::enable_shared_from_this<ZarrSharedResource>
{
  public:
    enum class ConsolidatedMetadataKind
    {
        NONE,
        EXTERNAL,  // Zarr V2 .zmetadata
        INTERNAL,  // Zarr V3 consolidated_metadata
    };

  private:
    bool m_bUpdatable = false;
    std::string m_osRootDirectoryName{};

    ConsolidatedMetadataKind m_eConsolidatedMetadataKind =
        ConsolidatedMetadataKind::NONE;
    CPLJSONObject m_oObjConsolidatedMetadata{};
    CPLJSONObject m_oRootAttributes{};
    bool m_bConsolidatedMetadataModified = false;

    std::shared_ptr<GDALPamMultiDim> m_poPAM{};
    CPLStringList m_aosOpenOptions{};
    std::weak_ptr<ZarrGroupBase> m_poWeakRootGroup{};
    std::set<std::string> m_oSetArrayInLoading{};

    explicit ZarrSharedResource(const std::string &osRootDirectoryName,
                                bool bUpdatable);

    std::shared_ptr<ZarrGroupBase> OpenRootGroup();
    void InitConsolidatedMetadataIfNeeded();

  public:
    static std::shared_ptr<ZarrSharedResource>
    Create(const std::string &osRootDirectoryName, bool bUpdatable);

    ~ZarrSharedResource();

    bool IsUpdatable() const
    {
        return m_bUpdatable;
    }

    const CPLJSONObject &GetConsolidatedMetadataObj() const
    {
        return m_oObjConsolidatedMetadata;
    }

    bool IsConsolidatedMetadataEnabled() const
    {
        return m_eConsolidatedMetadataKind != ConsolidatedMetadataKind::NONE;
    }

    void EnableConsolidatedMetadata(ConsolidatedMetadataKind kind)
    {
        m_eConsolidatedMetadataKind = kind;
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
    mutable std::set<std::string> m_oSetGroupNames{};
    mutable std::vector<std::string> m_aosGroups{};
    mutable std::set<std::string> m_oSetArrayNames{};
    mutable std::vector<std::string> m_aosArrays{};
    mutable ZarrAttributeGroup m_oAttrGroup;
    mutable bool m_bAttributesLoaded = false;
    bool m_bReadFromConsolidatedMetadata = false;
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

    virtual bool Close();

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

    bool Close() override;

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

    void InitFromConsolidatedMetadata(const CPLJSONObject &oRoot);

    bool InitFromZGroup(const CPLJSONObject &oRoot);
};

/************************************************************************/
/*                             ZarrV3Group                              */
/************************************************************************/

class ZarrV3Group final : public ZarrGroupBase
{
    bool m_bFileHasBeenWritten = false;

    void ExploreDirectory() const override;
    void LoadAttributes() const override;

    ZarrV3Group(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::string &osDirectoryName);

    std::shared_ptr<ZarrV3Group>
    GetOrCreateSubGroup(const std::string &osSubGroupFullname);

    bool Close() override;

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

    void
    InitFromConsolidatedMetadata(const CPLJSONObject &oConsolidatedMetadata,
                                 const CPLJSONObject &oRootAttributes);
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

    inline void clear()
    {
        m_nSize = 0;
    }

    inline std::vector<GByte>::iterator begin()
    {
        return m_oVec.begin();
    }

    inline std::vector<GByte>::const_iterator begin() const
    {
        return m_oVec.begin();
    }

    inline std::vector<GByte>::iterator end()
    {
        return m_oVec.begin() + m_nSize;
    }

    inline std::vector<GByte>::const_iterator end() const
    {
        return m_oVec.begin() + m_nSize;
    }

    template <class InputIt>
    inline std::vector<GByte>::iterator
    insert(std::vector<GByte>::const_iterator pos, InputIt first, InputIt last)
    {
        const size_t nCount = std::distance(first, last);
        const auto &oVec = m_oVec;
        const size_t nStart = std::distance(oVec.begin(), pos);
        if (nStart == m_nSize && nStart + nCount <= m_oVec.size())
        {
            // Insert at end of user-visible vector, but fully inside the
            // container vector. We can just copy
            std::copy(first, last, m_oVec.begin() + nStart);
            m_nSize += nCount;
            return m_oVec.begin() + nStart;
        }
        else
        {
            // Generic case
            auto ret = m_oVec.insert(pos, first, last);
            m_nSize += nCount;
            return ret;
        }
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

    //! Array (several in case of compound data type) of native Zarr data types
    const std::vector<DtypeElt> m_aoDtypeElts;

    /** m_anOuterBlockSize is the chunk_size at the Zarr array level, which
     * determines the files/objects
     */
    const std::vector<GUInt64> m_anOuterBlockSize;

    /** m_anInnerBlockSize is the inner most block size of sharding, which
     * is the one exposed to the user with GetBlockSize()
     * When no sharding is involved m_anOuterBlockSize == m_anInnerBlockSize
     * Note that m_anOuterBlockSize might be equal to m_anInnerBlockSize, even
     * when sharding is involved, and it is actually a common use case.
     */
    const std::vector<GUInt64> m_anInnerBlockSize;

    /** m_anCountInnerBlockInOuter[i] = m_anOuterBlockSize[i] / m_anInnerBlockSize[i]
     * That is the number of inner blocks in one outer block
     */
    const std::vector<GUInt64> m_anCountInnerBlockInOuter;

    //! Total number of inner chunks in the array
    const uint64_t m_nTotalInnerChunkCount;

    //! Size in bytes of a inner chunk using the Zarr native data type
    const size_t m_nInnerBlockSizeBytes;

    mutable ZarrAttributeGroup m_oAttrGroup;

    const bool m_bUseOptimizedCodePaths;

    CPLStringList m_aosStructuralInfo{};
    CPLJSONObject m_dtype{};
    GByte *m_pabyNoData = nullptr;
    std::string m_osDimSeparator{"."};
    std::string m_osFilename{};
    mutable ZarrByteVectorQuickResize m_abyRawBlockData{};
    mutable ZarrByteVectorQuickResize m_abyDecodedBlockData{};

    /** Inner block index of the cached block
     * i.e. m_anCachedBlockIndices[i] < cpl::round_up(m_aoDims[i]->GetSize, m_anInnerBlockSize[i])
     */
    mutable std::vector<uint64_t> m_anCachedBlockIndices{};

    mutable bool m_bCachedBlockValid = false;
    mutable bool m_bCachedBlockEmpty = false;
    mutable bool m_bDirtyBlock = false;
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
    mutable bool m_bHasTriedBlockCachePresenceArray = false;
    mutable std::shared_ptr<GDALMDArray> m_poBlockCachePresenceArray{};
    mutable std::mutex m_oMutex{};

    struct CachedBlock
    {
        ZarrByteVectorQuickResize abyDecoded{};
    };

    mutable std::map<std::vector<uint64_t>, CachedBlock> m_oChunkCache{};

    static uint64_t
    ComputeBlockCount(const std::string &osName,
                      const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                      const std::vector<GUInt64> &anBlockSize);

    ZarrArray(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
              const std::string &osParentName, const std::string &osName,
              const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
              const GDALExtendedDataType &oType,
              const std::vector<DtypeElt> &aoDtypeElts,
              const std::vector<GUInt64> &anOuterBlockSize,
              const std::vector<GUInt64> &anInnerBlockSize);

    virtual bool LoadBlockData(const uint64_t *blockIndices,
                               bool &bMissingBlockOut) const = 0;

    virtual bool AllocateWorkingBuffers() const = 0;

    void SerializeNumericNoData(CPLJSONObject &oRoot) const;

    void DeallocateDecodedBlockData();

    virtual std::string GetDataDirectory() const = 0;

    virtual CPLStringList
    GetChunkIndicesFromFilename(const char *pszFilename) const = 0;

    virtual bool FlushDirtyBlock() const = 0;

    std::shared_ptr<GDALMDArray> OpenBlockPresenceCache(bool bCanCreate) const;

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

    bool IsEmptyBlock(const ZarrByteVectorQuickResize &abyBlock) const;

    bool IAdviseReadCommon(const GUInt64 *arrayStartIdx, const size_t *count,
                           CSLConstList papszOptions,
                           std::vector<uint64_t> &anIndicesCur,
                           int &nThreadsMax,
                           std::vector<uint64_t> &anReqBlocksIndices,
                           size_t &nReqBlocks) const;

    CPLJSONObject SerializeSpecialAttributes();

    virtual std::string
    BuildChunkFilename(const uint64_t *blockIndices) const = 0;

    bool SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                       double dfMean, double dfStdDev, GUInt64 nValidCount,
                       CSLConstList papszOptions) override;

    bool IsBlockMissingFromCacheInfo(const std::string &osFilename,
                                     const uint64_t *blockIndices) const;

    virtual CPLStringList GetRawBlockInfoInfo() const = 0;

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
        return m_anInnerBlockSize;
    }

    CSLConstList GetStructuralInfo() const override
    {
        return m_aosStructuralInfo.List();
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

    virtual bool Flush() = 0;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poSharedResource->GetRootGroup();
    }

    bool GetRawBlockInfo(const uint64_t *panBlockCoordinates,
                         GDALMDArrayRawBlockInfo &info) const override;

    bool BlockCachePresence();

    void SetStructuralInfo(const char *pszKey, const char *pszValue)
    {
        m_aosStructuralInfo.SetNameValue(pszKey, pszValue);
    }

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
        m_abyTmpRawBlockData{};  // used for Fortran order

    ZarrV2Array(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                const GDALExtendedDataType &oType,
                const std::vector<DtypeElt> &aoDtypeElts,
                const std::vector<GUInt64> &anOuterBlockSize,
                bool bFortranOrder);

    bool Serialize();

    bool LoadBlockData(const uint64_t *blockIndices, bool bUseMutex,
                       const CPLCompressor *psDecompressor,
                       ZarrByteVectorQuickResize &abyRawBlockData,
                       ZarrByteVectorQuickResize &abyTmpRawBlockData,
                       ZarrByteVectorQuickResize &abyDecodedBlockData,
                       bool &bMissingBlockOut) const;

    bool NeedDecodedBuffer() const;

    bool AllocateWorkingBuffers(
        ZarrByteVectorQuickResize &abyRawBlockData,
        ZarrByteVectorQuickResize &abyTmpRawBlockData,
        ZarrByteVectorQuickResize &abyDecodedBlockData) const;

    void BlockTranspose(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst, bool bDecode) const;

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

    void SetCompressorJson(const CPLJSONObject &oCompressor);

    void SetCompressorDecompressor(const std::string &osDecompressorId,
                                   const CPLCompressor *psComp,
                                   const CPLCompressor *psDecomp)
    {
        m_psCompressor = psComp;
        m_osDecompressorId = osDecompressorId;
        m_psDecompressor = psDecomp;
    }

    void SetFilters(const CPLJSONArray &oFiltersArray);

    bool Flush() override;

  protected:
    std::string GetDataDirectory() const override;

    CPLStringList
    GetChunkIndicesFromFilename(const char *pszFilename) const override;

    bool FlushDirtyBlock() const override;

    std::string BuildChunkFilename(const uint64_t *blockIndices) const override;

    bool AllocateWorkingBuffers() const override;

    bool LoadBlockData(const uint64_t *blockIndices,
                       bool &bMissingBlockOut) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;

    CPLStringList GetRawBlockInfoInfo() const override;
};

/************************************************************************/
/*                           ZarrV3Array                                */
/************************************************************************/

class ZarrV3CodecSequence;

class ZarrV3Array final : public ZarrArray
{
    bool m_bV2ChunkKeyEncoding = false;
    std::unique_ptr<ZarrV3CodecSequence> m_poCodecs{};
    CPLJSONArray m_oJSONCodecs{};

    ZarrV3Array(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                const GDALExtendedDataType &oType,
                const std::vector<DtypeElt> &aoDtypeElts,
                const std::vector<GUInt64> &anOuterBlockSize,
                const std::vector<GUInt64> &anInnerBlockSize);

    bool Serialize(const CPLJSONObject &oAttrs);

    bool NeedDecodedBuffer() const;

    bool AllocateWorkingBuffers(
        ZarrByteVectorQuickResize &abyRawBlockData,
        ZarrByteVectorQuickResize &abyDecodedBlockData) const;

    bool LoadBlockData(const uint64_t *blockIndices, bool bUseMutex,
                       ZarrV3CodecSequence *poCodecs,
                       ZarrByteVectorQuickResize &abyRawBlockData,
                       ZarrByteVectorQuickResize &abyDecodedBlockData,
                       bool &bMissingBlockOut) const;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

  public:
    ~ZarrV3Array() override;

    static std::shared_ptr<ZarrV3Array>
    Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
           const GDALExtendedDataType &oType,
           const std::vector<DtypeElt> &aoDtypeElts,
           const std::vector<GUInt64> &anOuterBlockSize,
           const std::vector<GUInt64> &anInnerBlockSize);

    void SetIsV2ChunkKeyEncoding(bool b)
    {
        m_bV2ChunkKeyEncoding = b;
    }

    void SetCodecs(const CPLJSONArray &oJSONCodecs,
                   std::unique_ptr<ZarrV3CodecSequence> &&poCodecs);

    bool Flush() override;

    static std::unique_ptr<ZarrV3CodecSequence>
    SetupCodecs(const CPLJSONArray &oCodecs,
                const std::vector<GUInt64> &anOuterBlockSize,
                std::vector<GUInt64> &anInnerBlockSize, DtypeElt &zarrDataType,
                const std::vector<GByte> &abyNoData);

  protected:
    std::string GetDataDirectory() const override;

    CPLStringList
    GetChunkIndicesFromFilename(const char *pszFilename) const override;

    bool AllocateWorkingBuffers() const override;

    bool FlushDirtyBlock() const override;

    std::string BuildChunkFilename(const uint64_t *blockIndices) const override;

    bool LoadBlockData(const uint64_t *blockIndices,
                       bool &bMissingBlockOut) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;

    CPLStringList GetRawBlockInfoInfo() const override;
};

#endif  // ZARR_H
