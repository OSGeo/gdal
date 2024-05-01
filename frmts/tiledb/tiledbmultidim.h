/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB multidimensional support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#ifndef TILEDBMULTIDIM_H_INCLUDED
#define TILEDBMULTIDIM_H_INCLUDED

#include "tiledbheaders.h"

#include <set>

constexpr const char *CRS_ATTRIBUTE_NAME = "_CRS";
constexpr const char *UNIT_ATTRIBUTE_NAME = "_UNIT";
constexpr const char *DIM_TYPE_ATTRIBUTE_NAME = "_DIM_TYPE";
constexpr const char *DIM_DIRECTION_ATTRIBUTE_NAME = "_DIM_DIRECTION";

/************************************************************************/
/*                     TileDBSharedResource                            */
/************************************************************************/

class TileDBSharedResource
{
    std::unique_ptr<tiledb::Context> m_ctx{};
    bool m_bUpdatable = false;
    bool m_bStats = false;
    uint64_t m_nTimestamp = 0;

  public:
    TileDBSharedResource(std::unique_ptr<tiledb::Context> &&ctx,
                         bool bUpdatable)
        : m_ctx(std::move(ctx)), m_bUpdatable(bUpdatable)
    {
    }

    inline bool IsUpdatable() const
    {
        return m_bUpdatable;
    }

    inline tiledb::Context &GetCtx() const
    {
        return *(m_ctx.get());
    }

    static std::string SanitizeNameForPath(const std::string &osName);

    void SetDumpStats(bool b)
    {
        m_bStats = b;
    }

    bool GetDumpStats() const
    {
        return m_bStats;
    }

    void SetTimestamp(uint64_t t)
    {
        m_nTimestamp = t;
    }

    uint64_t GetTimestamp() const
    {
        return m_nTimestamp;
    }
};

/************************************************************************/
/*                      TileDBAttributeHolder                           */
/************************************************************************/

class TileDBAttributeHolder
{
  private:
    mutable std::map<std::string, std::shared_ptr<GDALAttribute>>
        m_oMapAttributes{};

    virtual uint64_t metadata_num() const = 0;
    virtual void get_metadata_from_index(uint64_t index, std::string *key,
                                         tiledb_datatype_t *value_type,
                                         uint32_t *value_num,
                                         const void **value) const = 0;
    virtual bool has_metadata(const std::string &key,
                              tiledb_datatype_t *value_type) const = 0;
    virtual void get_metadata(const std::string &key,
                              tiledb_datatype_t *value_type,
                              uint32_t *value_num,
                              const void **value) const = 0;
    virtual void put_metadata(const std::string &key,
                              tiledb_datatype_t value_type, uint32_t value_num,
                              const void *value) = 0;
    virtual void delete_metadata(const std::string &key) = 0;

    virtual bool EnsureOpenAs(tiledb_query_type_t mode) const = 0;
    virtual std::shared_ptr<TileDBAttributeHolder>
    AsAttributeHolderSharedPtr() const = 0;

    static std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::shared_ptr<TileDBAttributeHolder> &poSelf,
                    const std::string &osName, tiledb_datatype_t value_type,
                    uint32_t value_num, const void *value);

  public:
    virtual ~TileDBAttributeHolder() = 0;

    virtual bool IIsWritable() const = 0;
    virtual const std::string &IGetFullName() const = 0;

    std::shared_ptr<GDALAttribute>
    CreateAttributeImpl(const std::string &osName,
                        const std::vector<GUInt64> &anDimensions,
                        const GDALExtendedDataType &oDataType,
                        CSLConstList papszOptions = nullptr);

    std::shared_ptr<GDALAttribute>
    GetAttributeImpl(const std::string &osName) const;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributesImpl(CSLConstList papszOptions = nullptr) const;

    bool DeleteAttributeImpl(const std::string &osName,
                             CSLConstList papszOptions = nullptr);

    bool GetMetadata(const std::string &key, tiledb_datatype_t *value_type,
                     uint32_t *value_num, const void **value) const;
    bool PutMetadata(const std::string &key, tiledb_datatype_t value_type,
                     uint32_t value_num, const void *value);
};

/************************************************************************/
/*                          TileDBGroup                                 */
/************************************************************************/

class TileDBArray;

class TileDBGroup final : public GDALGroup, public TileDBAttributeHolder
{
    std::shared_ptr<TileDBSharedResource> m_poSharedResource{};
    const std::string m_osPath;
    mutable std::unique_ptr<tiledb::Group> m_poTileDBGroup{};
    mutable std::map<std::string, std::shared_ptr<TileDBGroup>> m_oMapGroups{};
    mutable std::map<std::string, std::shared_ptr<TileDBArray>> m_oMapArrays{};
    mutable std::map<std::string, std::shared_ptr<GDALDimension>>
        m_oMapDimensions{};

    //! To prevent OpenMDArray() to indefinitely recursing
    mutable std::set<std::string> m_oSetArrayInOpening{};

    TileDBGroup(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::string &osPath)
        : GDALGroup(osParentName, osName), m_poSharedResource(poSharedResource),
          m_osPath(osPath)
    {
    }

    static std::shared_ptr<TileDBGroup>
    Create(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::string &osPath)
    {
        auto poGroup = std::shared_ptr<TileDBGroup>(
            new TileDBGroup(poSharedResource, osParentName, osName, osPath));
        poGroup->SetSelf(poGroup);
        return poGroup;
    }

    bool HasObjectOfSameName(const std::string &osName) const;

  protected:
    // BEGIN: interfaces of TileDBAttributeHolder

    bool EnsureOpenAs(tiledb_query_type_t mode) const override;

    uint64_t metadata_num() const override
    {
        return m_poTileDBGroup->metadata_num();
    }

    void get_metadata_from_index(uint64_t index, std::string *key,
                                 tiledb_datatype_t *value_type,
                                 uint32_t *value_num,
                                 const void **value) const override
    {
        m_poTileDBGroup->get_metadata_from_index(index, key, value_type,
                                                 value_num, value);
    }

    bool has_metadata(const std::string &key,
                      tiledb_datatype_t *value_type) const override
    {
        return m_poTileDBGroup->has_metadata(key, value_type);
    }

    void get_metadata(const std::string &key, tiledb_datatype_t *value_type,
                      uint32_t *value_num, const void **value) const override
    {
        m_poTileDBGroup->get_metadata(key, value_type, value_num, value);
    }

    void put_metadata(const std::string &key, tiledb_datatype_t value_type,
                      uint32_t value_num, const void *value) override
    {
        m_poTileDBGroup->put_metadata(key, value_type, value_num, value);
    }

    void delete_metadata(const std::string &key) override
    {
        m_poTileDBGroup->delete_metadata(key);
    }

    std::shared_ptr<TileDBAttributeHolder>
    AsAttributeHolderSharedPtr() const override
    {
        return std::dynamic_pointer_cast<TileDBAttributeHolder>(m_pSelf.lock());
    }

    bool IIsWritable() const override
    {
        return m_poSharedResource->IsUpdatable();
    }

    const std::string &IGetFullName() const override
    {
        return GetFullName();
    }

    // END: interfaces of TileDBAttributeHolder

  public:
    ~TileDBGroup() override;

    static std::shared_ptr<TileDBGroup>
    CreateOnDisk(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
                 const std::string &osParentName, const std::string &osName,
                 const std::string &osPath);

    static std::shared_ptr<TileDBGroup>
    OpenFromDisk(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
                 const std::string &osParentName, const std::string &osName,
                 const std::string &osPath);

    const std::string &GetPath() const
    {
        return m_osPath;
    }

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALDimension>
    CreateDimension(const std::string &osName, const std::string &osType,
                    const std::string &osDirection, GUInt64 nSize,
                    CSLConstList) override;

    std::shared_ptr<GDALGroup>
    CreateGroup(const std::string &osName,
                CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override;

    std::shared_ptr<GDALMDArray> CreateMDArray(
        const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions = nullptr) const override;

    bool AddMember(const std::string &osPath, const std::string &osName);

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions = nullptr) override;
};

/************************************************************************/
/*                          TileDBArray                                 */
/************************************************************************/

class TileDBArray final : public GDALMDArray, public TileDBAttributeHolder
{
    std::shared_ptr<TileDBSharedResource> m_poSharedResource{};
    const std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    const GDALExtendedDataType m_oType;
    const std::string m_osPath;
    std::vector<GUInt64> m_anBlockSize{};
    // Starting offset of each dimension (if not zero)
    std::vector<uint64_t> m_anStartDimOffset{};
    mutable bool m_bFinalized = true;
    mutable std::unique_ptr<tiledb::ArraySchema> m_poSchema{};

    std::string m_osAttrName{};  // (TileDB) attribute name
    mutable std::unique_ptr<tiledb::Attribute> m_poAttr{};
    mutable std::unique_ptr<tiledb::Array> m_poTileDBArray{};
    mutable std::vector<GByte> m_abyNoData{};
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    std::string m_osUnit{};
    bool m_bStats = false;

    // inclusive ending timestamp when opening this array
    uint64_t m_nTimestamp = 0;

    std::weak_ptr<TileDBGroup> m_poParent{};  // used for creation path
    std::string m_osParentPath{};             // used for creation path
    // used for creation path.
    // To keep a reference on the indexing variables in CreateOnDisk(),
    // so they are still alive at Finalize() time
    std::vector<std::shared_ptr<GDALMDArray>> m_apoIndexingVariables{};

    CPLStringList m_aosStructuralInfo{};

    TileDBArray(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
                const std::string &osParentName, const std::string &osName,
                const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                const GDALExtendedDataType &oType, const std::string &osPath);

    static std::shared_ptr<TileDBArray>
    Create(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
           const std::string &osParentName, const std::string &osName,
           const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
           const GDALExtendedDataType &oType, const std::string &osPath);

    bool Finalize() const;

  protected:
    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

    // BEGIN: interfaces of TileDBAttributeHolder

    bool EnsureOpenAs(tiledb_query_type_t mode) const override;

    uint64_t metadata_num() const override
    {
        return m_poTileDBArray->metadata_num();
    }

    void get_metadata_from_index(uint64_t index, std::string *key,
                                 tiledb_datatype_t *value_type,
                                 uint32_t *value_num,
                                 const void **value) const override
    {
        m_poTileDBArray->get_metadata_from_index(index, key, value_type,
                                                 value_num, value);
    }

    bool has_metadata(const std::string &key,
                      tiledb_datatype_t *value_type) const override
    {
        return m_poTileDBArray->has_metadata(key, value_type);
    }

    void get_metadata(const std::string &key, tiledb_datatype_t *value_type,
                      uint32_t *value_num, const void **value) const override
    {
        m_poTileDBArray->get_metadata(key, value_type, value_num, value);
    }

    void put_metadata(const std::string &key, tiledb_datatype_t value_type,
                      uint32_t value_num, const void *value) override
    {
        m_poTileDBArray->put_metadata(key, value_type, value_num, value);
    }

    void delete_metadata(const std::string &key) override
    {
        m_poTileDBArray->delete_metadata(key);
    }

    std::shared_ptr<TileDBAttributeHolder>
    AsAttributeHolderSharedPtr() const override
    {
        return std::dynamic_pointer_cast<TileDBAttributeHolder>(m_pSelf.lock());
    }

    bool IIsWritable() const override
    {
        return IsWritable();
    }

    const std::string &IGetFullName() const override
    {
        return GetFullName();
    }

    // END: interfaces of TileDBAttributeHolder

  public:
    ~TileDBArray() override;

    static std::shared_ptr<TileDBArray>
    OpenFromDisk(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
                 const std::shared_ptr<GDALGroup> &poParent,
                 const std::string &osParentName, const std::string &osName,
                 const std::string &osAttributeName, const std::string &osPath,
                 CSLConstList papszOptions);

    static std::shared_ptr<TileDBArray> CreateOnDisk(
        const std::shared_ptr<TileDBSharedResource> &poSharedResource,
        const std::shared_ptr<TileDBGroup> &poParent, const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType, CSLConstList papszOptions);

    bool IsWritable() const override
    {
        return m_poSharedResource->IsUpdatable();
    }

    const std::string &GetFilename() const override
    {
        return m_osPath;
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

    const void *GetRawNoDataValue() const override;

    bool SetRawNoDataValue(const void *pRawNoData) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poSRS;
    }

    bool SetSpatialRef(const OGRSpatialReference *poSRS) override;

    const std::string &GetUnit() const override
    {
        return m_osUnit;
    }

    bool SetUnit(const std::string &osUnit) override;

    CSLConstList GetStructuralInfo() const override;

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions = nullptr) override;
    static GDALDataType
    TileDBDataTypeToGDALDataType(tiledb_datatype_t tiledb_dt);

    static bool GDALDataTypeToTileDB(GDALDataType dt,
                                     tiledb_datatype_t &tiledb_dt);
};

/************************************************************************/
/*                       TileDBAttribute                                */
/************************************************************************/

// Caution: TileDBAttribute implements a GDAL multidim attribute, which
// in TileDB terminology maps to a TileDB metadata item.
class TileDBAttribute final : public GDALAttribute
{
    std::shared_ptr<GDALAttribute> m_poMemAttribute;
    std::weak_ptr<TileDBAttributeHolder> m_poParent;

    TileDBAttribute(const std::string &osParentName, const std::string &osName);

  protected:
    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_poMemAttribute->GetDimensions();
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_poMemAttribute->GetDataType();
    }

  public:
    static std::shared_ptr<GDALAttribute>
    Create(const std::shared_ptr<TileDBAttributeHolder> &poParent,
           const std::string &osName, const std::vector<GUInt64> &anDimensions,
           const GDALExtendedDataType &oDataType);
};

/************************************************************************/
/*                           TileDBDimension                            */
/************************************************************************/

class TileDBDimension final : public GDALDimension
{
    // OK as a shared_ptr rather a weak_ptr, given that for the use we make
    // of it, m_poIndexingVariable doesn't point to a TileDBDimension
    std::shared_ptr<GDALMDArray> m_poIndexingVariable{};

  public:
    TileDBDimension(const std::string &osParentName, const std::string &osName,
                    const std::string &osType, const std::string &osDirection,
                    GUInt64 nSize)
        : GDALDimension(osParentName, osName, osType, osDirection, nSize)
    {
    }

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override
    {
        return m_poIndexingVariable;
    }

    void SetIndexingVariableOneTime(
        const std::shared_ptr<GDALMDArray> &poIndexingVariable)
    {
        m_poIndexingVariable = poIndexingVariable;
    }
};

/************************************************************************/
/*                       TileDBArrayGroup                               */
/************************************************************************/

class TileDBArrayGroup final : public GDALGroup
{
    std::vector<std::shared_ptr<GDALMDArray>> m_apoArrays;

  public:
    explicit TileDBArrayGroup(
        const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays)
        : GDALGroup(std::string(), "/"), m_apoArrays(apoArrays)
    {
    }

    static std::shared_ptr<GDALGroup>
    Create(const std::shared_ptr<TileDBSharedResource> &poSharedResource,
           const std::string &osArrayPath);

    std::vector<std::string>
        GetMDArrayNames(CSLConstList /*papszOptions*/ = nullptr) const override;

    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList /*papszOptions*/ = nullptr) const override;
};

/************************************************************************/
/*                     TileDBMultiDimDataset                            */
/************************************************************************/

class TileDBMultiDimDataset final : public GDALDataset
{
    friend class TileDBDataset;

    std::shared_ptr<GDALGroup> m_poRG{};

  public:
    explicit TileDBMultiDimDataset(const std::shared_ptr<GDALGroup> &poRG)
        : m_poRG(poRG)
    {
    }

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRG;
    }
};

#endif  // TILEDBMULTIDIM_H_INCLUDED
