/******************************************************************************
 *
 * Name:     gdal_multidim.h
 * Project:  GDAL Core
 * Purpose:  Declaration of classes for multidimensional support
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMULTIDIM_H_INCLUDED
#define GDALMULTIDIM_H_INCLUDED

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_geotransform.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

/* ******************************************************************** */
/*                       Multidimensional array API                     */
/* ******************************************************************** */

class GDALMDArray;
class GDALAttribute;
class GDALDataset;
class GDALDimension;
class GDALEDTComponent;
class GDALRasterAttributeTable;
class GDALRasterBand;
class OGRLayer;
class OGRSpatialReference;

/* ******************************************************************** */
/*                         GDALExtendedDataType                         */
/* ******************************************************************** */

/**
 * Class used to represent potentially complex data types.
 * Several classes of data types are supported: numeric (based on GDALDataType),
 * compound or string.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALExtendedDataType
{
  public:
    ~GDALExtendedDataType();

    GDALExtendedDataType(GDALExtendedDataType &&);
    GDALExtendedDataType(const GDALExtendedDataType &);

    GDALExtendedDataType &operator=(const GDALExtendedDataType &);
    GDALExtendedDataType &operator=(GDALExtendedDataType &&);

    static GDALExtendedDataType Create(GDALDataType eType);
    static GDALExtendedDataType
    Create(const std::string &osName, GDALDataType eBaseType,
           std::unique_ptr<GDALRasterAttributeTable>);
    static GDALExtendedDataType
    Create(const std::string &osName, size_t nTotalSize,
           std::vector<std::unique_ptr<GDALEDTComponent>> &&components);
    static GDALExtendedDataType
    CreateString(size_t nMaxStringLength = 0,
                 GDALExtendedDataTypeSubType eSubType = GEDTST_NONE);

    bool operator==(const GDALExtendedDataType &) const;

    /** Non-equality operator */
    bool operator!=(const GDALExtendedDataType &other) const
    {
        return !(operator==(other));
    }

    /** Return type name.
     *
     * This is the same as the C function GDALExtendedDataTypeGetName()
     */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Return type class.
     *
     * This is the same as the C function GDALExtendedDataTypeGetClass()
     */
    GDALExtendedDataTypeClass GetClass() const
    {
        return m_eClass;
    }

    /** Return numeric data type (only valid when GetClass() == GEDTC_NUMERIC)
     *
     * This is the same as the C function
     * GDALExtendedDataTypeGetNumericDataType()
     */
    GDALDataType GetNumericDataType() const
    {
        return m_eNumericDT;
    }

    /** Return subtype.
     *
     * This is the same as the C function GDALExtendedDataTypeGetSubType()
     *
     * @since 3.4
     */
    GDALExtendedDataTypeSubType GetSubType() const
    {
        return m_eSubType;
    }

    /** Return the components of the data type (only valid when GetClass() ==
     * GEDTC_COMPOUND)
     *
     * This is the same as the C function GDALExtendedDataTypeGetComponents()
     */
    const std::vector<std::unique_ptr<GDALEDTComponent>> &GetComponents() const
    {
        return m_aoComponents;
    }

    /** Return data type size in bytes.
     *
     * For a string, this will be size of a char* pointer.
     *
     * This is the same as the C function GDALExtendedDataTypeGetSize()
     */
    size_t GetSize() const
    {
        return m_nSize;
    }

    /** Return the maximum length of a string in bytes.
     *
     * 0 indicates unknown/unlimited string.
     */
    size_t GetMaxStringLength() const
    {
        return m_nMaxStringLength;
    }

    /** Return associated raster attribute table, when there is one.
     *
     * For the netCDF driver, the RAT will capture enumerated types, with
     * a "value" column with an integer value and a "name" column with the
     * associated name.
     *
     * This is the same as the C function GDALExtendedDataTypeGetRAT()
     *
     * @since 3.12
     */
    const GDALRasterAttributeTable *GetRAT() const
    {
        return m_poRAT.get();
    }

    bool CanConvertTo(const GDALExtendedDataType &other) const;

    bool NeedsFreeDynamicMemory() const;

    void FreeDynamicMemory(void *pBuffer) const;

    static bool CopyValue(const void *pSrc, const GDALExtendedDataType &srcType,
                          void *pDst, const GDALExtendedDataType &dstType);

    static bool CopyValues(const void *pSrc,
                           const GDALExtendedDataType &srcType,
                           GPtrDiff_t nSrcStrideInElts, void *pDst,
                           const GDALExtendedDataType &dstType,
                           GPtrDiff_t nDstStrideInElts, size_t nValues);

  private:
    GDALExtendedDataType(size_t nMaxStringLength,
                         GDALExtendedDataTypeSubType eSubType);
    explicit GDALExtendedDataType(GDALDataType eType);
    GDALExtendedDataType(const std::string &osName, GDALDataType eBaseType,
                         std::unique_ptr<GDALRasterAttributeTable>);
    GDALExtendedDataType(
        const std::string &osName, size_t nTotalSize,
        std::vector<std::unique_ptr<GDALEDTComponent>> &&components);

    std::string m_osName{};
    GDALExtendedDataTypeClass m_eClass = GEDTC_NUMERIC;
    GDALExtendedDataTypeSubType m_eSubType = GEDTST_NONE;
    GDALDataType m_eNumericDT = GDT_Unknown;
    std::vector<std::unique_ptr<GDALEDTComponent>> m_aoComponents{};
    size_t m_nSize = 0;
    size_t m_nMaxStringLength = 0;
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};
};

/* ******************************************************************** */
/*                            GDALEDTComponent                          */
/* ******************************************************************** */

/**
 * Class for a component of a compound extended data type.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALEDTComponent
{
  public:
    ~GDALEDTComponent();
    GDALEDTComponent(const std::string &name, size_t offset,
                     const GDALExtendedDataType &type);
    GDALEDTComponent(const GDALEDTComponent &);

    bool operator==(const GDALEDTComponent &) const;

    /** Return the name.
     *
     * This is the same as the C function GDALEDTComponentGetName().
     */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Return the offset (in bytes) of the component in the compound data type.
     *
     * This is the same as the C function GDALEDTComponentGetOffset().
     */
    size_t GetOffset() const
    {
        return m_nOffset;
    }

    /** Return the data type of the component.
     *
     * This is the same as the C function GDALEDTComponentGetType().
     */
    const GDALExtendedDataType &GetType() const
    {
        return m_oType;
    }

  private:
    std::string m_osName;
    size_t m_nOffset;
    GDALExtendedDataType m_oType;
};

/* ******************************************************************** */
/*                            GDALIHasAttribute                         */
/* ******************************************************************** */

/**
 * Interface used to get a single GDALAttribute or a set of GDALAttribute
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALIHasAttribute
{
  protected:
    std::shared_ptr<GDALAttribute>
    GetAttributeFromAttributes(const std::string &osName) const;

  public:
    virtual ~GDALIHasAttribute();

    virtual std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const;

    virtual std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const;

    virtual std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr);

    virtual bool DeleteAttribute(const std::string &osName,
                                 CSLConstList papszOptions = nullptr);
};

/* ******************************************************************** */
/*                               GDALGroup                              */
/* ******************************************************************** */

/* clang-format off */
/**
 * Class modeling a named container of GDALAttribute, GDALMDArray, OGRLayer or
 * other GDALGroup. Hence GDALGroup can describe a hierarchy of objects.
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_group">HDF5 group
 * concept</a>
 *
 * @since GDAL 3.1
 */
/* clang-format on */

class CPL_DLL GDALGroup : public GDALIHasAttribute
{
  protected:
    //! @cond Doxygen_Suppress
    std::string m_osName{};

    // This is actually a path of the form "/parent_path/{m_osName}"
    std::string m_osFullName{};

    // Used for example by GDALSubsetGroup to distinguish a derived group
    //from its original, without altering its name
    const std::string m_osContext{};

    // List of types owned by the group.
    std::vector<std::shared_ptr<GDALExtendedDataType>> m_apoTypes{};

    //! Weak pointer to this
    std::weak_ptr<GDALGroup> m_pSelf{};

    //! Can be set to false by the owing group, when deleting this object
    bool m_bValid = true;

    GDALGroup(const std::string &osParentName, const std::string &osName,
              const std::string &osContext = std::string());

    const GDALGroup *
    GetInnerMostGroup(const std::string &osPathOrArrayOrDim,
                      std::shared_ptr<GDALGroup> &curGroupHolder,
                      std::string &osLastPart) const;

    void BaseRename(const std::string &osNewName);

    bool CheckValidAndErrorOutIfNot() const;

    void SetSelf(const std::shared_ptr<GDALGroup> &self)
    {
        m_pSelf = self;
    }

    virtual void NotifyChildrenOfRenaming()
    {
    }

    virtual void NotifyChildrenOfDeletion()
    {
    }

    //! @endcond

  public:
    ~GDALGroup() override;

    /** Return the name of the group.
     *
     * This is the same as the C function GDALGroupGetName().
     */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Return the full name of the group.
     *
     * This is the same as the C function GDALGroupGetFullName().
     */
    const std::string &GetFullName() const
    {
        return m_osFullName;
    }

    /** Return data types associated with the group (typically enumerations)
     *
     * This is the same as the C function GDALGroupGetDataTypeCount() and GDALGroupGetDataType()
     *
     * @since 3.12
     */
    const std::vector<std::shared_ptr<GDALExtendedDataType>> &
    GetDataTypes() const
    {
        return m_apoTypes;
    }

    virtual std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions = nullptr) const;
    virtual std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions = nullptr) const;

    std::vector<std::string> GetMDArrayFullNamesRecursive(
        CSLConstList papszGroupOptions = nullptr,
        CSLConstList papszArrayOptions = nullptr) const;

    virtual std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions = nullptr) const;
    virtual std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const;

    virtual std::vector<std::string>
    GetVectorLayerNames(CSLConstList papszOptions = nullptr) const;
    virtual OGRLayer *
    OpenVectorLayer(const std::string &osName,
                    CSLConstList papszOptions = nullptr) const;

    virtual std::vector<std::shared_ptr<GDALDimension>>
    GetDimensions(CSLConstList papszOptions = nullptr) const;

    virtual std::shared_ptr<GDALGroup>
    CreateGroup(const std::string &osName, CSLConstList papszOptions = nullptr);

    virtual bool DeleteGroup(const std::string &osName,
                             CSLConstList papszOptions = nullptr);

    virtual std::shared_ptr<GDALDimension>
    CreateDimension(const std::string &osName, const std::string &osType,
                    const std::string &osDirection, GUInt64 nSize,
                    CSLConstList papszOptions = nullptr);

    virtual std::shared_ptr<GDALMDArray> CreateMDArray(
        const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        CSLConstList papszOptions = nullptr);

    virtual bool DeleteMDArray(const std::string &osName,
                               CSLConstList papszOptions = nullptr);

    GUInt64 GetTotalCopyCost() const;

    virtual bool CopyFrom(const std::shared_ptr<GDALGroup> &poDstRootGroup,
                          GDALDataset *poSrcDS,
                          const std::shared_ptr<GDALGroup> &poSrcGroup,
                          bool bStrict, GUInt64 &nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress, void *pProgressData,
                          CSLConstList papszOptions = nullptr);

    virtual CSLConstList GetStructuralInfo() const;

    std::shared_ptr<GDALMDArray>
    OpenMDArrayFromFullname(const std::string &osFullName,
                            CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALAttribute>
    OpenAttributeFromFullname(const std::string &osFullName,
                              CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALMDArray>
    ResolveMDArray(const std::string &osName, const std::string &osStartingPath,
                   CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALGroup>
    OpenGroupFromFullname(const std::string &osFullName,
                          CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALDimension>
    OpenDimensionFromFullname(const std::string &osFullName) const;

    virtual void ClearStatistics();

    virtual bool Rename(const std::string &osNewName);

    std::shared_ptr<GDALGroup>
    SubsetDimensionFromSelection(const std::string &osSelection) const;

    //! @cond Doxygen_Suppress
    virtual void ParentRenamed(const std::string &osNewParentFullName);

    virtual void Deleted();

    virtual void ParentDeleted();

    const std::string &GetContext() const
    {
        return m_osContext;
    }

    //! @endcond

    //! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 1000;
    //! @endcond
};

/* ******************************************************************** */
/*                          GDALAbstractMDArray                         */
/* ******************************************************************** */

/**
 * Abstract class, implemented by GDALAttribute and GDALMDArray.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALAbstractMDArray
{
  protected:
    //! @cond Doxygen_Suppress
    std::string m_osName{};

    // This is actually a path of the form "/parent_path/{m_osName}"
    std::string m_osFullName{};
    std::weak_ptr<GDALAbstractMDArray> m_pSelf{};

    //! Can be set to false by the owing object, when deleting this object
    bool m_bValid = true;

    GDALAbstractMDArray(const std::string &osParentName,
                        const std::string &osName);

    void SetSelf(const std::shared_ptr<GDALAbstractMDArray> &self)
    {
        m_pSelf = self;
    }

    bool CheckValidAndErrorOutIfNot() const;

    bool CheckReadWriteParams(const GUInt64 *arrayStartIdx, const size_t *count,
                              const GInt64 *&arrayStep,
                              const GPtrDiff_t *&bufferStride,
                              const GDALExtendedDataType &bufferDataType,
                              const void *buffer,
                              const void *buffer_alloc_start,
                              size_t buffer_alloc_size,
                              std::vector<GInt64> &tmp_arrayStep,
                              std::vector<GPtrDiff_t> &tmp_bufferStride) const;

    virtual bool
    IRead(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
          const size_t *count,             // array of size GetDimensionCount()
          const GInt64 *arrayStep,         // step in elements
          const GPtrDiff_t *bufferStride,  // stride in elements
          const GDALExtendedDataType &bufferDataType,
          void *pDstBuffer) const = 0;

    virtual bool
    IWrite(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
           const size_t *count,             // array of size GetDimensionCount()
           const GInt64 *arrayStep,         // step in elements
           const GPtrDiff_t *bufferStride,  // stride in elements
           const GDALExtendedDataType &bufferDataType, const void *pSrcBuffer);

    void BaseRename(const std::string &osNewName);

    virtual void NotifyChildrenOfRenaming()
    {
    }

    virtual void NotifyChildrenOfDeletion()
    {
    }

    //! @endcond

  public:
    virtual ~GDALAbstractMDArray();

    /** Return the name of an array or attribute.
     *
     * This is the same as the C function GDALMDArrayGetName() or
     * GDALAttributeGetName().
     */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Return the name of an array or attribute.
     *
     * This is the same as the C function GDALMDArrayGetFullName() or
     * GDALAttributeGetFullName().
     */
    const std::string &GetFullName() const
    {
        return m_osFullName;
    }

    GUInt64 GetTotalElementsCount() const;

    virtual size_t GetDimensionCount() const;

    virtual const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const = 0;

    virtual const GDALExtendedDataType &GetDataType() const = 0;

    virtual std::vector<GUInt64> GetBlockSize() const;

    virtual std::vector<size_t>
    GetProcessingChunkSize(size_t nMaxChunkMemory) const;

    /* clang-format off */
    /** Type of pfnFunc argument of ProcessPerChunk().
     * @param array Array on which ProcessPerChunk was called.
     * @param chunkArrayStartIdx Values representing the starting index to use
     *                           in each dimension (in [0, aoDims[i].GetSize()-1] range)
     *                           for the current chunk.
     *                           Will be nullptr for a zero-dimensional array.
     * @param chunkCount         Values representing the number of values to use in
     *                           each dimension for the current chunk.
     *                           Will be nullptr for a zero-dimensional array.
     * @param iCurChunk          Number of current chunk being processed.
     *                           In [1, nChunkCount] range.
     * @param nChunkCount        Total number of chunks to process.
     * @param pUserData          User data.
     * @return return true in case of success.
     */
    typedef bool (*FuncProcessPerChunkType)(
                        GDALAbstractMDArray *array,
                        const GUInt64 *chunkArrayStartIdx,
                        const size_t *chunkCount,
                        GUInt64 iCurChunk,
                        GUInt64 nChunkCount,
                        void *pUserData);
    /* clang-format on */

    virtual bool ProcessPerChunk(const GUInt64 *arrayStartIdx,
                                 const GUInt64 *count, const size_t *chunkSize,
                                 FuncProcessPerChunkType pfnFunc,
                                 void *pUserData);

    virtual bool
    Read(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
         const size_t *count,             // array of size GetDimensionCount()
         const GInt64 *arrayStep,         // step in elements
         const GPtrDiff_t *bufferStride,  // stride in elements
         const GDALExtendedDataType &bufferDataType, void *pDstBuffer,
         const void *pDstBufferAllocStart = nullptr,
         size_t nDstBufferAllocSize = 0) const;

    bool
    Write(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
          const size_t *count,             // array of size GetDimensionCount()
          const GInt64 *arrayStep,         // step in elements
          const GPtrDiff_t *bufferStride,  // stride in elements
          const GDALExtendedDataType &bufferDataType, const void *pSrcBuffer,
          const void *pSrcBufferAllocStart = nullptr,
          size_t nSrcBufferAllocSize = 0);

    virtual bool Rename(const std::string &osNewName);

    //! @cond Doxygen_Suppress
    virtual void Deleted();

    virtual void ParentDeleted();

    virtual void ParentRenamed(const std::string &osNewParentFullName);
    //! @endcond
};

/* ******************************************************************** */
/*                              GDALRawResult                           */
/* ******************************************************************** */

/**
 * Store the raw result of an attribute value, which might contain dynamically
 * allocated structures (like pointer to strings).
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALRawResult
{
  private:
    GDALExtendedDataType m_dt;
    size_t m_nEltCount;
    size_t m_nSize;
    GByte *m_raw;

    void FreeMe();

    GDALRawResult(const GDALRawResult &) = delete;
    GDALRawResult &operator=(const GDALRawResult &) = delete;

  protected:
    friend class GDALAttribute;
    //! @cond Doxygen_Suppress
    GDALRawResult(GByte *raw, const GDALExtendedDataType &dt, size_t nEltCount);
    //! @endcond

  public:
    ~GDALRawResult();
    GDALRawResult(GDALRawResult &&);
    GDALRawResult &operator=(GDALRawResult &&);

    /** Return byte at specified index. */
    const GByte &operator[](size_t idx) const
    {
        return m_raw[idx];
    }

    /** Return pointer to the start of data. */
    const GByte *data() const
    {
        return m_raw;
    }

    /** Return the size in bytes of the raw result. */
    size_t size() const
    {
        return m_nSize;
    }

    //! @cond Doxygen_Suppress
    GByte *StealData();
    //! @endcond
};

/* ******************************************************************** */
/*                              GDALAttribute                           */
/* ******************************************************************** */

/* clang-format off */
/**
 * Class modeling an attribute that has a name, a value and a type, and is
 * typically used to describe a metadata item. The value can be (for the
 * HDF5 format) in the general case a multidimensional array of "any" type
 * (in most cases, this will be a single value of string or numeric type)
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_attribute">HDF5
 * attribute concept</a>
 *
 * @since GDAL 3.1
 */
/* clang-format on */

class CPL_DLL GDALAttribute : virtual public GDALAbstractMDArray
{
    mutable std::string m_osCachedVal{};

  protected:
    //! @cond Doxygen_Suppress
    GDALAttribute(const std::string &osParentName, const std::string &osName);
    //! @endcond

  public:
    //! @cond Doxygen_Suppress
    ~GDALAttribute() override;
    //! @endcond

    std::vector<GUInt64> GetDimensionsSize() const;

    GDALRawResult ReadAsRaw() const;
    const char *ReadAsString() const;
    int ReadAsInt() const;
    int64_t ReadAsInt64() const;
    double ReadAsDouble() const;
    CPLStringList ReadAsStringArray() const;
    std::vector<int> ReadAsIntArray() const;
    std::vector<int64_t> ReadAsInt64Array() const;
    std::vector<double> ReadAsDoubleArray() const;

    using GDALAbstractMDArray::Write;
    bool Write(const void *pabyValue, size_t nLen);
    bool Write(const char *);
    bool WriteInt(int);
    bool WriteInt64(int64_t);
    bool Write(double);
    bool Write(CSLConstList);
    bool Write(const int *, size_t);
    bool Write(const int64_t *, size_t);
    bool Write(const double *, size_t);

    //! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 100;
    //! @endcond
};

/************************************************************************/
/*                            GDALAttributeString                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALAttributeString final : public GDALAttribute
{
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::CreateString();
    std::string m_osValue;

  protected:
    bool IRead(const GUInt64 *, const size_t *, const GInt64 *,
               const GPtrDiff_t *, const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    GDALAttributeString(const std::string &osParentName,
                        const std::string &osName, const std::string &osValue,
                        GDALExtendedDataTypeSubType eSubType = GEDTST_NONE);

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;
};

//! @endcond

/************************************************************************/
/*                           GDALAttributeNumeric                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALAttributeNumeric final : public GDALAttribute
{
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt;
    int m_nValue = 0;
    double m_dfValue = 0;
    std::vector<GUInt32> m_anValuesUInt32{};

  protected:
    bool IRead(const GUInt64 *, const size_t *, const GInt64 *,
               const GPtrDiff_t *, const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    GDALAttributeNumeric(const std::string &osParentName,
                         const std::string &osName, double dfValue);
    GDALAttributeNumeric(const std::string &osParentName,
                         const std::string &osName, int nValue);
    GDALAttributeNumeric(const std::string &osParentName,
                         const std::string &osName,
                         const std::vector<GUInt32> &anValues);

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;
};

//! @endcond

/* ******************************************************************** */
/*                              GDALMDArray                             */
/* ******************************************************************** */

/* clang-format off */
/**
 * Class modeling a multi-dimensional array. It has a name, values organized
 * as an array and a list of GDALAttribute.
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_dataset">HDF5
 * dataset concept</a>
 *
 * @since GDAL 3.1
 */
/* clang-format on */

class CPL_DLL GDALMDArray : virtual public GDALAbstractMDArray,
                            public GDALIHasAttribute
{
    friend class GDALMDArrayResampled;
    std::shared_ptr<GDALMDArray>
    GetView(const std::vector<GUInt64> &indices) const;

    inline std::shared_ptr<GDALMDArray>
    atInternal(const std::vector<GUInt64> &indices) const
    {
        return GetView(indices);
    }

    template <typename... GUInt64VarArg>
    // cppcheck-suppress functionStatic
    inline std::shared_ptr<GDALMDArray>
    atInternal(std::vector<GUInt64> &indices, GUInt64 idx,
               GUInt64VarArg... tail) const
    {
        indices.push_back(idx);
        return atInternal(indices, tail...);
    }

    // Used for example by GDALSubsetGroup to distinguish a derived group
    //from its original, without altering its name
    const std::string m_osContext{};

    mutable bool m_bHasTriedCachedArray = false;
    mutable std::shared_ptr<GDALMDArray> m_poCachedArray{};

  protected:
    //! @cond Doxygen_Suppress
    GDALMDArray(const std::string &osParentName, const std::string &osName,
                const std::string &osContext = std::string());

    virtual bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                             CSLConstList papszOptions) const;

    virtual bool IsCacheable() const
    {
        return true;
    }

    virtual bool SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                               double dfMean, double dfStdDev,
                               GUInt64 nValidCount, CSLConstList papszOptions);

    static std::string MassageName(const std::string &inputName);

    std::shared_ptr<GDALGroup>
    GetCacheRootGroup(bool bCanCreate, std::string &osCacheFilenameOut) const;

    // Returns if bufferStride values express a transposed view of the array
    bool IsTransposedRequest(const size_t *count,
                             const GPtrDiff_t *bufferStride) const;

    // Should only be called if IsTransposedRequest() returns true
    bool ReadForTransposedRequest(const GUInt64 *arrayStartIdx,
                                  const size_t *count, const GInt64 *arrayStep,
                                  const GPtrDiff_t *bufferStride,
                                  const GDALExtendedDataType &bufferDataType,
                                  void *pDstBuffer) const;

    bool IsStepOneContiguousRowMajorOrderedSameDataType(
        const size_t *count, const GInt64 *arrayStep,
        const GPtrDiff_t *bufferStride,
        const GDALExtendedDataType &bufferDataType) const;

    // Should only be called if IsStepOneContiguousRowMajorOrderedSameDataType()
    // returns false
    bool ReadUsingContiguousIRead(const GUInt64 *arrayStartIdx,
                                  const size_t *count, const GInt64 *arrayStep,
                                  const GPtrDiff_t *bufferStride,
                                  const GDALExtendedDataType &bufferDataType,
                                  void *pDstBuffer) const;

    static std::shared_ptr<GDALMDArray> CreateGLTOrthorectified(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::shared_ptr<GDALGroup> &poRootGroup,
        const std::shared_ptr<GDALMDArray> &poGLTX,
        const std::shared_ptr<GDALMDArray> &poGLTY, int nGLTIndexOffset,
        const std::vector<double> &adfGeoTransform, CSLConstList papszOptions);

    //! @endcond

  public:
    GUInt64 GetTotalCopyCost() const;

    virtual bool CopyFrom(GDALDataset *poSrcDS, const GDALMDArray *poSrcArray,
                          bool bStrict, GUInt64 &nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress, void *pProgressData);

    /** Return whether an array is writable. */
    virtual bool IsWritable() const = 0;

    /** Return the filename that contains that array.
     *
     * This is used in particular for caching.
     *
     * Might be empty if the array is not linked to a file.
     *
     * @since GDAL 3.4
     */
    virtual const std::string &GetFilename() const = 0;

    virtual CSLConstList GetStructuralInfo() const;

    virtual const std::string &GetUnit() const;

    virtual bool SetUnit(const std::string &osUnit);

    virtual bool SetSpatialRef(const OGRSpatialReference *poSRS);

    virtual std::shared_ptr<OGRSpatialReference> GetSpatialRef() const;

    virtual const void *GetRawNoDataValue() const;

    double GetNoDataValueAsDouble(bool *pbHasNoData = nullptr) const;

    int64_t GetNoDataValueAsInt64(bool *pbHasNoData = nullptr) const;

    uint64_t GetNoDataValueAsUInt64(bool *pbHasNoData = nullptr) const;

    virtual bool SetRawNoDataValue(const void *pRawNoData);

    //! @cond Doxygen_Suppress
    bool SetNoDataValue(int nNoData)
    {
        return SetNoDataValue(static_cast<int64_t>(nNoData));
    }

    //! @endcond

    bool SetNoDataValue(double dfNoData);

    bool SetNoDataValue(int64_t nNoData);

    bool SetNoDataValue(uint64_t nNoData);

    virtual bool Resize(const std::vector<GUInt64> &anNewDimSizes,
                        CSLConstList papszOptions);

    virtual double GetOffset(bool *pbHasOffset = nullptr,
                             GDALDataType *peStorageType = nullptr) const;

    virtual double GetScale(bool *pbHasScale = nullptr,
                            GDALDataType *peStorageType = nullptr) const;

    virtual bool SetOffset(double dfOffset,
                           GDALDataType eStorageType = GDT_Unknown);

    virtual bool SetScale(double dfScale,
                          GDALDataType eStorageType = GDT_Unknown);

    std::shared_ptr<GDALMDArray> GetView(const std::string &viewExpr) const;

    std::shared_ptr<GDALMDArray> operator[](const std::string &fieldName) const;

    /** Return a view of the array using integer indexing.
     *
     * Equivalent of GetView("[indices_0,indices_1,.....,indices_last]")
     *
     * Example:
     * \code
     * ar->at(0,3,2)
     * \endcode
     */
    // sphinx 4.1.0 / breathe 4.30.0 don't like typename...
    //! @cond Doxygen_Suppress
    template <typename... GUInt64VarArg>
    //! @endcond
    // cppcheck-suppress functionStatic
    std::shared_ptr<GDALMDArray> at(GUInt64 idx, GUInt64VarArg... tail) const
    {
        std::vector<GUInt64> indices;
        indices.push_back(idx);
        return atInternal(indices, tail...);
    }

    virtual std::shared_ptr<GDALMDArray>
    Transpose(const std::vector<int> &anMapNewAxisToOldAxis) const;

    std::shared_ptr<GDALMDArray> GetUnscaled(
        double dfOverriddenScale = std::numeric_limits<double>::quiet_NaN(),
        double dfOverriddenOffset = std::numeric_limits<double>::quiet_NaN(),
        double dfOverriddenDstNodata =
            std::numeric_limits<double>::quiet_NaN()) const;

    virtual std::shared_ptr<GDALMDArray>
    GetMask(CSLConstList papszOptions) const;

    virtual std::shared_ptr<GDALMDArray>
    GetResampled(const std::vector<std::shared_ptr<GDALDimension>> &apoNewDims,
                 GDALRIOResampleAlg resampleAlg,
                 const OGRSpatialReference *poTargetSRS,
                 CSLConstList papszOptions) const;

    std::shared_ptr<GDALMDArray>
    GetGridded(const std::string &osGridOptions,
               const std::shared_ptr<GDALMDArray> &poXArray = nullptr,
               const std::shared_ptr<GDALMDArray> &poYArray = nullptr,
               CSLConstList papszOptions = nullptr) const;

    static std::vector<std::shared_ptr<GDALMDArray>>
    GetMeshGrid(const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
                CSLConstList papszOptions = nullptr);

    virtual GDALDataset *
    AsClassicDataset(size_t iXDim, size_t iYDim,
                     const std::shared_ptr<GDALGroup> &poRootGroup = nullptr,
                     CSLConstList papszOptions = nullptr) const;

    virtual CPLErr GetStatistics(bool bApproxOK, bool bForce, double *pdfMin,
                                 double *pdfMax, double *pdfMean,
                                 double *padfStdDev, GUInt64 *pnValidCount,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData);

    virtual bool ComputeStatistics(bool bApproxOK, double *pdfMin,
                                   double *pdfMax, double *pdfMean,
                                   double *pdfStdDev, GUInt64 *pnValidCount,
                                   GDALProgressFunc, void *pProgressData,
                                   CSLConstList papszOptions);

    virtual void ClearStatistics();

    virtual std::vector<std::shared_ptr<GDALMDArray>>
    GetCoordinateVariables() const;

    bool AdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                    CSLConstList papszOptions = nullptr) const;

    bool IsRegularlySpaced(double &dfStart, double &dfIncrement) const;

    bool GuessGeoTransform(size_t nDimX, size_t nDimY, bool bPixelIsPoint,
                           GDALGeoTransform &gt) const;

    bool GuessGeoTransform(size_t nDimX, size_t nDimY, bool bPixelIsPoint,
                           double adfGeoTransform[6]) const;

    bool Cache(CSLConstList papszOptions = nullptr) const;

    bool
    Read(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
         const size_t *count,             // array of size GetDimensionCount()
         const GInt64 *arrayStep,         // step in elements
         const GPtrDiff_t *bufferStride,  // stride in elements
         const GDALExtendedDataType &bufferDataType, void *pDstBuffer,
         const void *pDstBufferAllocStart = nullptr,
         size_t nDstBufferAllocSize = 0) const override final;

    virtual std::shared_ptr<GDALGroup> GetRootGroup() const;

    virtual bool GetRawBlockInfo(const uint64_t *panBlockCoordinates,
                                 GDALMDArrayRawBlockInfo &info) const;

    //! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 1000;

    bool CopyFromAllExceptValues(const GDALMDArray *poSrcArray, bool bStrict,
                                 GUInt64 &nCurCost, const GUInt64 nTotalCost,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData);

    struct Range
    {
        GUInt64 m_nStartIdx;
        GInt64 m_nIncr;

        explicit Range(GUInt64 nStartIdx = 0, GInt64 nIncr = 0)
            : m_nStartIdx(nStartIdx), m_nIncr(nIncr)
        {
        }
    };

    struct ViewSpec
    {
        std::string m_osFieldName{};

        // or

        std::vector<size_t>
            m_mapDimIdxToParentDimIdx{};  // of size m_dims.size()
        std::vector<Range>
            m_parentRanges{};  // of size m_poParent->GetDimensionCount()
    };

    virtual std::shared_ptr<GDALMDArray>
    GetView(const std::string &viewExpr, bool bRenameDimensions,
            std::vector<ViewSpec> &viewSpecs) const;

    const std::string &GetContext() const
    {
        return m_osContext;
    }

    //! @endcond
};

//! @cond Doxygen_Suppress
bool GDALMDRasterIOFromBand(GDALRasterBand *poBand, GDALRWFlag eRWFlag,
                            size_t iDimX, size_t iDimY,
                            const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            void *pBuffer);

//! @endcond

/************************************************************************/
/*                     GDALMDArrayRegularlySpaced                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALMDArrayRegularlySpaced final : public GDALMDArray
{
    double m_dfStart = 0;
    double m_dfIncrement = 0;
    double m_dfOffsetInIncrement = 0;
    const GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Float64);
    const std::vector<std::shared_ptr<GDALDimension>> m_dims;
    std::vector<std::shared_ptr<GDALAttribute>> m_attributes{};
    const std::string m_osEmptyFilename{};

  protected:
    bool IRead(const GUInt64 *, const size_t *, const GInt64 *,
               const GPtrDiff_t *, const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    GDALMDArrayRegularlySpaced(const std::string &osParentName,
                               const std::string &osName,
                               const std::shared_ptr<GDALDimension> &poDim,
                               double dfStart, double dfIncrement,
                               double dfOffsetInIncrement);

    static std::shared_ptr<GDALMDArrayRegularlySpaced>
    Create(const std::string &osParentName, const std::string &osName,
           const std::shared_ptr<GDALDimension> &poDim, double dfStart,
           double dfIncrement, double dfOffsetInIncrement);

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        return m_osEmptyFilename;
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;

    std::vector<std::shared_ptr<GDALAttribute>>
        GetAttributes(CSLConstList) const override;

    void AddAttribute(const std::shared_ptr<GDALAttribute> &poAttr);
};

//! @endcond

/* ******************************************************************** */
/*                            GDALDimension                             */
/* ******************************************************************** */

/**
 * Class modeling a a dimension / axis used to index multidimensional arrays.
 * It has a name, a size (that is the number of values that can be indexed along
 * the dimension), a type (see GDALDimension::GetType()), a direction
 * (see GDALDimension::GetDirection()), a unit and can optionally point to a
 * GDALMDArray variable, typically one-dimensional, describing the values taken
 * by the dimension. For a georeferenced GDALMDArray and its X dimension, this
 * will be typically the values of the easting/longitude for each grid point.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALDimension
{
  public:
    //! @cond Doxygen_Suppress
    GDALDimension(const std::string &osParentName, const std::string &osName,
                  const std::string &osType, const std::string &osDirection,
                  GUInt64 nSize);
    //! @endcond

    virtual ~GDALDimension();

    /** Return the name.
     *
     * This is the same as the C function GDALDimensionGetName()
     */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Return the full name.
     *
     * This is the same as the C function GDALDimensionGetFullName()
     */
    const std::string &GetFullName() const
    {
        return m_osFullName;
    }

    /** Return the axis type.
     *
     * Predefined values are:
     * HORIZONTAL_X, HORIZONTAL_Y, VERTICAL, TEMPORAL, PARAMETRIC
     * Other values might be returned. Empty value means unknown.
     *
     * This is the same as the C function GDALDimensionGetType()
     */
    const std::string &GetType() const
    {
        return m_osType;
    }

    /** Return the axis direction.
     *
     * Predefined values are:
     * EAST, WEST, SOUTH, NORTH, UP, DOWN, FUTURE, PAST
     * Other values might be returned. Empty value means unknown.
     *
     * This is the same as the C function GDALDimensionGetDirection()
     */
    const std::string &GetDirection() const
    {
        return m_osDirection;
    }

    /** Return the size, that is the number of values along the dimension.
     *
     * This is the same as the C function GDALDimensionGetSize()
     */
    GUInt64 GetSize() const
    {
        return m_nSize;
    }

    virtual std::shared_ptr<GDALMDArray> GetIndexingVariable() const;

    virtual bool
    SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable);

    virtual bool Rename(const std::string &osNewName);

    //! @cond Doxygen_Suppress
    virtual void ParentRenamed(const std::string &osNewParentFullName);

    virtual void ParentDeleted();
    //! @endcond

  protected:
    //! @cond Doxygen_Suppress
    std::string m_osName;
    std::string m_osFullName;
    std::string m_osType;
    std::string m_osDirection;
    GUInt64 m_nSize;

    void BaseRename(const std::string &osNewName);

    //! @endcond
};

/************************************************************************/
/*                   GDALDimensionWeakIndexingVar()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALDimensionWeakIndexingVar : public GDALDimension
{
    std::weak_ptr<GDALMDArray> m_poIndexingVariable{};

  public:
    GDALDimensionWeakIndexingVar(const std::string &osParentName,
                                 const std::string &osName,
                                 const std::string &osType,
                                 const std::string &osDirection, GUInt64 nSize);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;

    bool SetIndexingVariable(
        std::shared_ptr<GDALMDArray> poIndexingVariable) override;

    void SetSize(GUInt64 nNewSize);
};

//! @endcond

#endif
