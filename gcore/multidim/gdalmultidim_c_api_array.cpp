/******************************************************************************
 *
 * Name:     gdalmultidim_c_api_array.cpp
 * Project:  GDAL Core
 * Purpose:  C API for GDALMDArray
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdalmultidim_priv.h"
#include "gdal_fwd.h"

/************************************************************************/
/*                         GDALMDArrayRelease()                         */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALMDArray.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALMDArrayRelease(GDALMDArrayH hMDArray)
{
    delete hMDArray;
}

/************************************************************************/
/*                         GDALMDArrayGetName()                         */
/************************************************************************/

/** Return array name.
 *
 * This is the same as the C++ method GDALMDArray::GetName()
 */
const char *GDALMDArrayGetName(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return hArray->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                       GDALMDArrayGetFullName()                       */
/************************************************************************/

/** Return array full name.
 *
 * This is the same as the C++ method GDALMDArray::GetFullName()
 */
const char *GDALMDArrayGetFullName(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return hArray->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                         GDALMDArrayGetName()                         */
/************************************************************************/

/** Return the total number of values in the array.
 *
 * This is the same as the C++ method
 * GDALAbstractMDArray::GetTotalElementsCount()
 */
GUInt64 GDALMDArrayGetTotalElementsCount(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    return hArray->m_poImpl->GetTotalElementsCount();
}

/************************************************************************/
/*                    GDALMDArrayGetDimensionCount()                    */
/************************************************************************/

/** Return the number of dimensions.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensionCount()
 */
size_t GDALMDArrayGetDimensionCount(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    return hArray->m_poImpl->GetDimensionCount();
}

/************************************************************************/
/*                      GDALMDArrayGetDimensions()                      */
/************************************************************************/

/** Return the dimensions of the array
 *
 * The returned array must be freed with GDALReleaseDimensions(). If only the
 * array itself needs to be freed, CPLFree() should be called (and
 * GDALDimensionRelease() on individual array members).
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensions()
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 *
 * @return an array of *pnCount dimensions.
 */
GDALDimensionH *GDALMDArrayGetDimensions(GDALMDArrayH hArray, size_t *pnCount)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    const auto &dims(hArray->m_poImpl->GetDimensions());
    auto ret = static_cast<GDALDimensionH *>(
        CPLMalloc(sizeof(GDALDimensionH) * dims.size()));
    for (size_t i = 0; i < dims.size(); i++)
    {
        ret[i] = new GDALDimensionHS(dims[i]);
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                       GDALMDArrayGetDataType()                       */
/************************************************************************/

/** Return the data type
 *
 * The return must be freed with GDALExtendedDataTypeRelease().
 */
GDALExtendedDataTypeH GDALMDArrayGetDataType(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hArray->m_poImpl->GetDataType()));
}

/************************************************************************/
/*                          GDALMDArrayRead()                           */
/************************************************************************/

/** Read part or totality of a multidimensional array.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Read()
 *
 * @return TRUE in case of success.
 */
int GDALMDArrayRead(GDALMDArrayH hArray, const GUInt64 *arrayStartIdx,
                    const size_t *count, const GInt64 *arrayStep,
                    const GPtrDiff_t *bufferStride,
                    GDALExtendedDataTypeH bufferDataType, void *pDstBuffer,
                    const void *pDstBufferAllocStart,
                    size_t nDstBufferAllocSize)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    if ((arrayStartIdx == nullptr || count == nullptr) &&
        hArray->m_poImpl->GetDimensionCount() > 0)
    {
        VALIDATE_POINTER1(arrayStartIdx, __func__, FALSE);
        VALIDATE_POINTER1(count, __func__, FALSE);
    }
    VALIDATE_POINTER1(bufferDataType, __func__, FALSE);
    VALIDATE_POINTER1(pDstBuffer, __func__, FALSE);
    return hArray->m_poImpl->Read(arrayStartIdx, count, arrayStep, bufferStride,
                                  *(bufferDataType->m_poImpl), pDstBuffer,
                                  pDstBufferAllocStart, nDstBufferAllocSize);
}

/************************************************************************/
/*                          GDALMDArrayWrite()                          */
/************************************************************************/

/** Write part or totality of a multidimensional array.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Write()
 *
 * @return TRUE in case of success.
 */
int GDALMDArrayWrite(GDALMDArrayH hArray, const GUInt64 *arrayStartIdx,
                     const size_t *count, const GInt64 *arrayStep,
                     const GPtrDiff_t *bufferStride,
                     GDALExtendedDataTypeH bufferDataType,
                     const void *pSrcBuffer, const void *pSrcBufferAllocStart,
                     size_t nSrcBufferAllocSize)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    if ((arrayStartIdx == nullptr || count == nullptr) &&
        hArray->m_poImpl->GetDimensionCount() > 0)
    {
        VALIDATE_POINTER1(arrayStartIdx, __func__, FALSE);
        VALIDATE_POINTER1(count, __func__, FALSE);
    }
    VALIDATE_POINTER1(bufferDataType, __func__, FALSE);
    VALIDATE_POINTER1(pSrcBuffer, __func__, FALSE);
    return hArray->m_poImpl->Write(arrayStartIdx, count, arrayStep,
                                   bufferStride, *(bufferDataType->m_poImpl),
                                   pSrcBuffer, pSrcBufferAllocStart,
                                   nSrcBufferAllocSize);
}

/************************************************************************/
/*                       GDALMDArrayAdviseRead()                        */
/************************************************************************/

/** Advise driver of upcoming read requests.
 *
 * This is the same as the C++ method GDALMDArray::AdviseRead()
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.2
 */
int GDALMDArrayAdviseRead(GDALMDArrayH hArray, const GUInt64 *arrayStartIdx,
                          const size_t *count)
{
    return GDALMDArrayAdviseReadEx(hArray, arrayStartIdx, count, nullptr);
}

/************************************************************************/
/*                      GDALMDArrayAdviseReadEx()                       */
/************************************************************************/

/** Advise driver of upcoming read requests.
 *
 * This is the same as the C++ method GDALMDArray::AdviseRead()
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.4
 */
int GDALMDArrayAdviseReadEx(GDALMDArrayH hArray, const GUInt64 *arrayStartIdx,
                            const size_t *count, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->AdviseRead(arrayStartIdx, count, papszOptions);
}

/************************************************************************/
/*                      GDALMDArrayGetAttribute()                       */
/************************************************************************/

/** Return an attribute by its name.
 *
 * This is the same as the C++ method GDALIHasAttribute::GetAttribute()
 *
 * The returned attribute must be freed with GDALAttributeRelease().
 */
GDALAttributeH GDALMDArrayGetAttribute(GDALMDArrayH hArray, const char *pszName)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    auto attr = hArray->m_poImpl->GetAttribute(std::string(pszName));
    if (attr)
        return new GDALAttributeHS(attr);
    return nullptr;
}

/************************************************************************/
/*                      GDALMDArrayGetAttributes()                      */
/************************************************************************/

/** Return the list of attributes contained in this array.
 *
 * The returned array must be freed with GDALReleaseAttributes(). If only the
 * array itself needs to be freed, CPLFree() should be called (and
 * GDALAttributeRelease() on individual array members).
 *
 * This is the same as the C++ method GDALMDArray::GetAttributes().
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return an array of *pnCount attributes.
 */
GDALAttributeH *GDALMDArrayGetAttributes(GDALMDArrayH hArray, size_t *pnCount,
                                         CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    auto attrs = hArray->m_poImpl->GetAttributes(papszOptions);
    auto ret = static_cast<GDALAttributeH *>(
        CPLMalloc(sizeof(GDALAttributeH) * attrs.size()));
    for (size_t i = 0; i < attrs.size(); i++)
    {
        ret[i] = new GDALAttributeHS(attrs[i]);
    }
    *pnCount = attrs.size();
    return ret;
}

/************************************************************************/
/*                     GDALMDArrayCreateAttribute()                     */
/************************************************************************/

/** Create a attribute within an array.
 *
 * This is the same as the C++ method GDALMDArray::CreateAttribute().
 *
 * @return the attribute, to be freed with GDALAttributeRelease(), or nullptr.
 */
GDALAttributeH GDALMDArrayCreateAttribute(GDALMDArrayH hArray,
                                          const char *pszName,
                                          size_t nDimensions,
                                          const GUInt64 *panDimensions,
                                          GDALExtendedDataTypeH hEDT,
                                          CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(hEDT, __func__, nullptr);
    std::vector<GUInt64> dims;
    dims.reserve(nDimensions);
    for (size_t i = 0; i < nDimensions; i++)
        dims.push_back(panDimensions[i]);
    auto ret = hArray->m_poImpl->CreateAttribute(
        std::string(pszName), dims, *(hEDT->m_poImpl), papszOptions);
    if (!ret)
        return nullptr;
    return new GDALAttributeHS(ret);
}

/************************************************************************/
/*                     GDALMDArrayDeleteAttribute()                     */
/************************************************************************/

/** Delete an attribute from an array.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * This is the same as the C++ method GDALMDArray::DeleteAttribute().
 *
 * @return true in case of success.
 * @since GDAL 3.8
 */
bool GDALMDArrayDeleteAttribute(GDALMDArrayH hArray, const char *pszName,
                                CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, false);
    VALIDATE_POINTER1(pszName, __func__, false);
    return hArray->m_poImpl->DeleteAttribute(std::string(pszName),
                                             papszOptions);
}

/************************************************************************/
/*                    GDALMDArrayGetRawNoDataValue()                    */
/************************************************************************/

/** Return the nodata value as a "raw" value.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * The returned value should not be modified or freed.
 *
 * This is the same as the ++ method GDALMDArray::GetRawNoDataValue().
 *
 * @return nullptr or a pointer to GetDataType().GetSize() bytes.
 */
const void *GDALMDArrayGetRawNoDataValue(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return hArray->m_poImpl->GetRawNoDataValue();
}

/************************************************************************/
/*                 GDALMDArrayGetNoDataValueAsDouble()                  */
/************************************************************************/

/** Return the nodata value as a double.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * This is the same as the C++ method GDALMDArray::GetNoDataValueAsDouble().
 *
 * @param hArray Array handle.
 * @param pbHasNoDataValue Pointer to a output boolean that will be set to true
 * if a nodata value exists and can be converted to double. Might be nullptr.
 *
 * @return the nodata value as a double. A 0.0 value might also indicate the
 * absence of a nodata value or an error in the conversion (*pbHasNoDataValue
 * will be set to false then).
 */
double GDALMDArrayGetNoDataValueAsDouble(GDALMDArrayH hArray,
                                         int *pbHasNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    bool bHasNodataValue = false;
    double ret = hArray->m_poImpl->GetNoDataValueAsDouble(&bHasNodataValue);
    if (pbHasNoDataValue)
        *pbHasNoDataValue = bHasNodataValue;
    return ret;
}

/************************************************************************/
/*                  GDALMDArrayGetNoDataValueAsInt64()                  */
/************************************************************************/

/** Return the nodata value as a Int64.
 *
 * This is the same as the C++ method GDALMDArray::GetNoDataValueAsInt64().
 *
 * @param hArray Array handle.
 * @param pbHasNoDataValue Pointer to a output boolean that will be set to true
 * if a nodata value exists and can be converted to Int64. Might be nullptr.
 *
 * @return the nodata value as a Int64.
 * @since GDAL 3.5
 */
int64_t GDALMDArrayGetNoDataValueAsInt64(GDALMDArrayH hArray,
                                         int *pbHasNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    bool bHasNodataValue = false;
    const auto ret = hArray->m_poImpl->GetNoDataValueAsInt64(&bHasNodataValue);
    if (pbHasNoDataValue)
        *pbHasNoDataValue = bHasNodataValue;
    return ret;
}

/************************************************************************/
/*                 GDALMDArrayGetNoDataValueAsUInt64()                  */
/************************************************************************/

/** Return the nodata value as a UInt64.
 *
 * This is the same as the C++ method GDALMDArray::GetNoDataValueAsInt64().
 *
 * @param hArray Array handle.
 * @param pbHasNoDataValue Pointer to a output boolean that will be set to true
 * if a nodata value exists and can be converted to UInt64. Might be nullptr.
 *
 * @return the nodata value as a UInt64.
 * @since GDAL 3.5
 */
uint64_t GDALMDArrayGetNoDataValueAsUInt64(GDALMDArrayH hArray,
                                           int *pbHasNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    bool bHasNodataValue = false;
    const auto ret = hArray->m_poImpl->GetNoDataValueAsUInt64(&bHasNodataValue);
    if (pbHasNoDataValue)
        *pbHasNoDataValue = bHasNodataValue;
    return ret;
}

/************************************************************************/
/*                    GDALMDArraySetRawNoDataValue()                    */
/************************************************************************/

/** Set the nodata value as a "raw" value.
 *
 * This is the same as the C++ method GDALMDArray::SetRawNoDataValue(const
 * void*).
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetRawNoDataValue(GDALMDArrayH hArray, const void *pNoData)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetRawNoDataValue(pNoData);
}

/************************************************************************/
/*                 GDALMDArraySetNoDataValueAsDouble()                  */
/************************************************************************/

/** Set the nodata value as a double.
 *
 * If the natural data type of the attribute/array is not double, type
 * conversion will occur to the type returned by GetDataType().
 *
 * This is the same as the C++ method GDALMDArray::SetNoDataValue(double).
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetNoDataValueAsDouble(GDALMDArrayH hArray, double dfNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetNoDataValue(dfNoDataValue);
}

/************************************************************************/
/*                  GDALMDArraySetNoDataValueAsInt64()                  */
/************************************************************************/

/** Set the nodata value as a Int64.
 *
 * If the natural data type of the attribute/array is not Int64, type conversion
 * will occur to the type returned by GetDataType().
 *
 * This is the same as the C++ method GDALMDArray::SetNoDataValue(int64_t).
 *
 * @return TRUE in case of success.
 * @since GDAL 3.5
 */
int GDALMDArraySetNoDataValueAsInt64(GDALMDArrayH hArray, int64_t nNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetNoDataValue(nNoDataValue);
}

/************************************************************************/
/*                 GDALMDArraySetNoDataValueAsUInt64()                  */
/************************************************************************/

/** Set the nodata value as a UInt64.
 *
 * If the natural data type of the attribute/array is not UInt64, type
 * conversion will occur to the type returned by GetDataType().
 *
 * This is the same as the C++ method GDALMDArray::SetNoDataValue(uint64_t).
 *
 * @return TRUE in case of success.
 * @since GDAL 3.5
 */
int GDALMDArraySetNoDataValueAsUInt64(GDALMDArrayH hArray,
                                      uint64_t nNoDataValue)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetNoDataValue(nNoDataValue);
}

/************************************************************************/
/*                         GDALMDArrayResize()                          */
/************************************************************************/

/** Resize an array to new dimensions.
 *
 * Not all drivers may allow this operation, and with restrictions (e.g.
 * for netCDF, this is limited to growing of "unlimited" dimensions)
 *
 * Resizing a dimension used in other arrays will cause those other arrays
 * to be resized.
 *
 * This is the same as the C++ method GDALMDArray::Resize().
 *
 * @param hArray Array.
 * @param panNewDimSizes Array of GetDimensionCount() values containing the
 *                       new size of each indexing dimension.
 * @param papszOptions Options. (Driver specific)
 * @return true in case of success.
 * @since GDAL 3.7
 */
bool GDALMDArrayResize(GDALMDArrayH hArray, const GUInt64 *panNewDimSizes,
                       CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, false);
    VALIDATE_POINTER1(panNewDimSizes, __func__, false);
    std::vector<GUInt64> anNewDimSizes(hArray->m_poImpl->GetDimensionCount());
    for (size_t i = 0; i < anNewDimSizes.size(); ++i)
    {
        anNewDimSizes[i] = panNewDimSizes[i];
    }
    return hArray->m_poImpl->Resize(anNewDimSizes, papszOptions);
}

/************************************************************************/
/*                        GDALMDArraySetScale()                         */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetScale().
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetScale(GDALMDArrayH hArray, double dfScale)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetScale(dfScale);
}

/************************************************************************/
/*                       GDALMDArraySetScaleEx()                        */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetScale().
 *
 * @return TRUE in case of success.
 * @since GDAL 3.3
 */
int GDALMDArraySetScaleEx(GDALMDArrayH hArray, double dfScale,
                          GDALDataType eStorageType)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetScale(dfScale, eStorageType);
}

/************************************************************************/
/*                        GDALMDArraySetOffset()                        */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetOffset().
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetOffset(GDALMDArrayH hArray, double dfOffset)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetOffset(dfOffset);
}

/************************************************************************/
/*                       GDALMDArraySetOffsetEx()                       */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetOffset() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetOffset().
 *
 * @return TRUE in case of success.
 * @since GDAL 3.3
 */
int GDALMDArraySetOffsetEx(GDALMDArrayH hArray, double dfOffset,
                           GDALDataType eStorageType)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetOffset(dfOffset, eStorageType);
}

/************************************************************************/
/*                        GDALMDArrayGetScale()                         */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::GetScale().
 *
 * @return the scale value
 */
double GDALMDArrayGetScale(GDALMDArrayH hArray, int *pbHasValue)
{
    VALIDATE_POINTER1(hArray, __func__, 0.0);
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetScale(&bHasValue);
    if (pbHasValue)
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                       GDALMDArrayGetScaleEx()                        */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetScale()
 *
 * This is the same as the C++ method GDALMDArray::GetScale().
 *
 * @return the scale value
 * @since GDAL 3.3
 */
double GDALMDArrayGetScaleEx(GDALMDArrayH hArray, int *pbHasValue,
                             GDALDataType *peStorageType)
{
    VALIDATE_POINTER1(hArray, __func__, 0.0);
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetScale(&bHasValue, peStorageType);
    if (pbHasValue)
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                        GDALMDArrayGetOffset()                        */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::GetOffset().
 *
 * @return the scale value
 */
double GDALMDArrayGetOffset(GDALMDArrayH hArray, int *pbHasValue)
{
    VALIDATE_POINTER1(hArray, __func__, 0.0);
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetOffset(&bHasValue);
    if (pbHasValue)
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                       GDALMDArrayGetOffsetEx()                       */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::GetOffset().
 *
 * @return the scale value
 * @since GDAL 3.3
 */
double GDALMDArrayGetOffsetEx(GDALMDArrayH hArray, int *pbHasValue,
                              GDALDataType *peStorageType)
{
    VALIDATE_POINTER1(hArray, __func__, 0.0);
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetOffset(&bHasValue, peStorageType);
    if (pbHasValue)
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                      GDALMDArrayGetBlockSize()                       */
/************************************************************************/

/** Return the "natural" block size of the array along all dimensions.
 *
 * Some drivers might organize the array in tiles/blocks and reading/writing
 * aligned on those tile/block boundaries will be more efficient.
 *
 * The returned number of elements in the vector is the same as
 * GetDimensionCount(). A value of 0 should be interpreted as no hint regarding
 * the natural block size along the considered dimension.
 * "Flat" arrays will typically return a vector of values set to 0.
 *
 * The default implementation will return a vector of values set to 0.
 *
 * This method is used by GetProcessingChunkSize().
 *
 * Pedantic note: the returned type is GUInt64, so in the highly unlikely
 * theoretical case of a 32-bit platform, this might exceed its size_t
 * allocation capabilities.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetBlockSize().
 *
 * @return the block size, in number of elements along each dimension.
 */
GUInt64 *GDALMDArrayGetBlockSize(GDALMDArrayH hArray, size_t *pnCount)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    auto res = hArray->m_poImpl->GetBlockSize();
    auto ret = static_cast<GUInt64 *>(CPLMalloc(sizeof(GUInt64) * res.size()));
    for (size_t i = 0; i < res.size(); i++)
    {
        ret[i] = res[i];
    }
    *pnCount = res.size();
    return ret;
}

/************************************************************************/
/*                 GDALMDArrayGetProcessingChunkSize()                  */
/************************************************************************/

/** \brief Return an optimal chunk size for read/write operations, given the
 * natural block size and memory constraints specified.
 *
 * This method will use GetBlockSize() to define a chunk whose dimensions are
 * multiple of those returned by GetBlockSize() (unless the block define by
 * GetBlockSize() is larger than nMaxChunkMemory, in which case it will be
 * returned by this method).
 *
 * This is the same as the C++ method
 * GDALAbstractMDArray::GetProcessingChunkSize().
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param nMaxChunkMemory Maximum amount of memory, in bytes, to use for the
 * chunk.
 *
 * @return the chunk size, in number of elements along each dimension.
 */

size_t *GDALMDArrayGetProcessingChunkSize(GDALMDArrayH hArray, size_t *pnCount,
                                          size_t nMaxChunkMemory)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    auto res = hArray->m_poImpl->GetProcessingChunkSize(nMaxChunkMemory);
    auto ret = static_cast<size_t *>(CPLMalloc(sizeof(size_t) * res.size()));
    for (size_t i = 0; i < res.size(); i++)
    {
        ret[i] = res[i];
    }
    *pnCount = res.size();
    return ret;
}

/************************************************************************/
/*                    GDALMDArrayGetStructuralInfo()                    */
/************************************************************************/

/** Return structural information on the array.
 *
 * This may be the compression, etc..
 *
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 *
 * This is the same as the C++ method GDALMDArray::GetStructuralInfo().
 */
CSLConstList GDALMDArrayGetStructuralInfo(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return hArray->m_poImpl->GetStructuralInfo();
}

/************************************************************************/
/*                         GDALMDArrayGetView()                         */
/************************************************************************/

/** Return a view of the array using slicing or field access.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::GetView().
 */
GDALMDArrayH GDALMDArrayGetView(GDALMDArrayH hArray, const char *pszViewExpr)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pszViewExpr, __func__, nullptr);
    auto sliced = hArray->m_poImpl->GetView(std::string(pszViewExpr));
    if (!sliced)
        return nullptr;
    return new GDALMDArrayHS(sliced);
}

/************************************************************************/
/*                        GDALMDArrayTranspose()                        */
/************************************************************************/

/** Return a view of the array whose axis have been reordered.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::Transpose().
 */
GDALMDArrayH GDALMDArrayTranspose(GDALMDArrayH hArray, size_t nNewAxisCount,
                                  const int *panMapNewAxisToOldAxis)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    std::vector<int> anMapNewAxisToOldAxis(nNewAxisCount);
    if (nNewAxisCount)
    {
        memcpy(&anMapNewAxisToOldAxis[0], panMapNewAxisToOldAxis,
               nNewAxisCount * sizeof(int));
    }
    auto reordered = hArray->m_poImpl->Transpose(anMapNewAxisToOldAxis);
    if (!reordered)
        return nullptr;
    return new GDALMDArrayHS(reordered);
}

/************************************************************************/
/*                       GDALMDArrayGetUnscaled()                       */
/************************************************************************/

/** Return an array that is the unscaled version of the current one.
 *
 * That is each value of the unscaled array will be
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * Starting with GDAL 3.3, the Write() method is implemented and will convert
 * from unscaled values to raw values.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::GetUnscaled().
 */
GDALMDArrayH GDALMDArrayGetUnscaled(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    auto unscaled = hArray->m_poImpl->GetUnscaled();
    if (!unscaled)
        return nullptr;
    return new GDALMDArrayHS(unscaled);
}

/************************************************************************/
/*                         GDALMDArrayGetMask()                         */
/************************************************************************/

/** Return an array that is a mask for the current array
 *
 * This array will be of type Byte, with values set to 0 to indicate invalid
 * pixels of the current array, and values set to 1 to indicate valid pixels.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::GetMask().
 */
GDALMDArrayH GDALMDArrayGetMask(GDALMDArrayH hArray, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    auto unscaled = hArray->m_poImpl->GetMask(papszOptions);
    if (!unscaled)
        return nullptr;
    return new GDALMDArrayHS(unscaled);
}

/************************************************************************/
/*                      GDALMDArrayGetResampled()                       */
/************************************************************************/

/** Return an array that is a resampled / reprojected view of the current array
 *
 * This is the same as the C++ method GDALMDArray::GetResampled().
 *
 * Currently this method can only resample along the last 2 dimensions, unless
 * orthorectifying a NASA EMIT dataset.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * @since 3.4
 */
GDALMDArrayH GDALMDArrayGetResampled(GDALMDArrayH hArray, size_t nNewDimCount,
                                     const GDALDimensionH *pahNewDims,
                                     GDALRIOResampleAlg resampleAlg,
                                     OGRSpatialReferenceH hTargetSRS,
                                     CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pahNewDims, __func__, nullptr);
    std::vector<std::shared_ptr<GDALDimension>> apoNewDims(nNewDimCount);
    for (size_t i = 0; i < nNewDimCount; ++i)
    {
        if (pahNewDims[i])
            apoNewDims[i] = pahNewDims[i]->m_poImpl;
    }
    auto poNewArray = hArray->m_poImpl->GetResampled(
        apoNewDims, resampleAlg, OGRSpatialReference::FromHandle(hTargetSRS),
        papszOptions);
    if (!poNewArray)
        return nullptr;
    return new GDALMDArrayHS(poNewArray);
}

/************************************************************************/
/*                         GDALMDArraySetUnit()                         */
/************************************************************************/

/** Set the variable unit.
 *
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 *
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArraySetUnit()
 *
 * @param hArray array.
 * @param pszUnit unit name.
 * @return TRUE in case of success.
 */
int GDALMDArraySetUnit(GDALMDArrayH hArray, const char *pszUnit)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetUnit(pszUnit ? pszUnit : "");
}

/************************************************************************/
/*                         GDALMDArrayGetUnit()                         */
/************************************************************************/

/** Return the array unit.
 *
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 *
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 *
 * This is the same as the C++ method GDALMDArray::GetUnit().
 */
const char *GDALMDArrayGetUnit(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return hArray->m_poImpl->GetUnit().c_str();
}

/************************************************************************/
/*                      GDALMDArrayGetSpatialRef()                      */
/************************************************************************/

/** Assign a spatial reference system object to the array.
 *
 * This is the same as the C++ method GDALMDArray::SetSpatialRef().
 * @return TRUE in case of success.
 */
int GDALMDArraySetSpatialRef(GDALMDArrayH hArray, OGRSpatialReferenceH hSRS)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->SetSpatialRef(
        OGRSpatialReference::FromHandle(hSRS));
}

/************************************************************************/
/*                      GDALMDArrayGetSpatialRef()                      */
/************************************************************************/

/** Return the spatial reference system object associated with the array.
 *
 * This is the same as the C++ method GDALMDArray::GetSpatialRef().
 *
 * The returned object must be freed with OSRDestroySpatialReference().
 */
OGRSpatialReferenceH GDALMDArrayGetSpatialRef(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    auto poSRS = hArray->m_poImpl->GetSpatialRef();
    return poSRS ? OGRSpatialReference::ToHandle(poSRS->Clone()) : nullptr;
}

/************************************************************************/
/*                      GDALMDArrayGetStatistics()                      */
/************************************************************************/

/**
 * \brief Fetch statistics.
 *
 * This is the same as the C++ method GDALMDArray::GetStatistics().
 *
 * @since GDAL 3.2
 */

CPLErr GDALMDArrayGetStatistics(GDALMDArrayH hArray, GDALDatasetH /*hDS*/,
                                int bApproxOK, int bForce, double *pdfMin,
                                double *pdfMax, double *pdfMean,
                                double *pdfStdDev, GUInt64 *pnValidCount,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData)
{
    VALIDATE_POINTER1(hArray, __func__, CE_Failure);
    return hArray->m_poImpl->GetStatistics(
        CPL_TO_BOOL(bApproxOK), CPL_TO_BOOL(bForce), pdfMin, pdfMax, pdfMean,
        pdfStdDev, pnValidCount, pfnProgress, pProgressData);
}

/************************************************************************/
/*                    GDALMDArrayComputeStatistics()                    */
/************************************************************************/

/**
 * \brief Compute statistics.
 *
 * This is the same as the C++ method GDALMDArray::ComputeStatistics().
 *
 * @since GDAL 3.2
 * @see GDALMDArrayComputeStatisticsEx()
 */

int GDALMDArrayComputeStatistics(GDALMDArrayH hArray, GDALDatasetH /* hDS */,
                                 int bApproxOK, double *pdfMin, double *pdfMax,
                                 double *pdfMean, double *pdfStdDev,
                                 GUInt64 *pnValidCount,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->ComputeStatistics(
        CPL_TO_BOOL(bApproxOK), pdfMin, pdfMax, pdfMean, pdfStdDev,
        pnValidCount, pfnProgress, pProgressData, nullptr);
}

/************************************************************************/
/*                   GDALMDArrayComputeStatisticsEx()                   */
/************************************************************************/

/**
 * \brief Compute statistics.
 *
 * Same as GDALMDArrayComputeStatistics() with extra papszOptions argument.
 *
 * This is the same as the C++ method GDALMDArray::ComputeStatistics().
 *
 * @since GDAL 3.8
 */

int GDALMDArrayComputeStatisticsEx(GDALMDArrayH hArray, GDALDatasetH /* hDS */,
                                   int bApproxOK, double *pdfMin,
                                   double *pdfMax, double *pdfMean,
                                   double *pdfStdDev, GUInt64 *pnValidCount,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->ComputeStatistics(
        CPL_TO_BOOL(bApproxOK), pdfMin, pdfMax, pdfMean, pdfStdDev,
        pnValidCount, pfnProgress, pProgressData, papszOptions);
}

/************************************************************************/
/*                 GDALMDArrayGetCoordinateVariables()                  */
/************************************************************************/

/** Return coordinate variables.
 *
 * The returned array must be freed with GDALReleaseArrays(). If only the array
 * itself needs to be freed, CPLFree() should be called (and
 * GDALMDArrayRelease() on individual array members).
 *
 * This is the same as the C++ method GDALMDArray::GetCoordinateVariables()
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 *
 * @return an array of *pnCount arrays.
 * @since 3.4
 */
GDALMDArrayH *GDALMDArrayGetCoordinateVariables(GDALMDArrayH hArray,
                                                size_t *pnCount)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    const auto coordinates(hArray->m_poImpl->GetCoordinateVariables());
    auto ret = static_cast<GDALMDArrayH *>(
        CPLMalloc(sizeof(GDALMDArrayH) * coordinates.size()));
    for (size_t i = 0; i < coordinates.size(); i++)
    {
        ret[i] = new GDALMDArrayHS(coordinates[i]);
    }
    *pnCount = coordinates.size();
    return ret;
}

/************************************************************************/
/*                       GDALMDArrayGetGridded()                        */
/************************************************************************/

/** Return a gridded array from scattered point data, that is from an array
 * whose last dimension is the indexing variable of X and Y arrays.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::GetGridded().
 *
 * @since GDAL 3.7
 */
GDALMDArrayH GDALMDArrayGetGridded(GDALMDArrayH hArray,
                                   const char *pszGridOptions,
                                   GDALMDArrayH hXArray, GDALMDArrayH hYArray,
                                   CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    VALIDATE_POINTER1(pszGridOptions, __func__, nullptr);
    auto gridded = hArray->m_poImpl->GetGridded(
        pszGridOptions, hXArray ? hXArray->m_poImpl : nullptr,
        hYArray ? hYArray->m_poImpl : nullptr, papszOptions);
    if (!gridded)
        return nullptr;
    return new GDALMDArrayHS(gridded);
}

/************************************************************************/
/*                       GDALMDArrayGetMeshGrid()                       */
/************************************************************************/

/** Return a list of multidimensional arrays from a list of one-dimensional
 * arrays.
 *
 * This is typically used to transform one-dimensional longitude, latitude
 * arrays into 2D ones.
 *
 * More formally, for one-dimensional arrays x1, x2,..., xn with lengths
 * Ni=len(xi), returns (N1, N2, ..., Nn) shaped arrays if indexing="ij" or
 * (N2, N1, ..., Nn) shaped arrays if indexing="xy" with the elements of xi
 * repeated to fill the matrix along the first dimension for x1, the second
 * for x2 and so on.
 *
 * For example, if x = [1, 2], and y = [3, 4, 5],
 * GetMeshGrid([x, y], ["INDEXING=xy"]) will return [xm, ym] such that
 * xm=[[1, 2],[1, 2],[1, 2]] and ym=[[3, 3],[4, 4],[5, 5]],
 * or more generally xm[any index][i] = x[i] and ym[i][any index]=y[i]
 *
 * and
 * GetMeshGrid([x, y], ["INDEXING=ij"]) will return [xm, ym] such that
 * xm=[[1, 1, 1],[2, 2, 2]] and ym=[[3, 4, 5],[3, 4, 5]],
 * or more generally xm[i][any index] = x[i] and ym[any index][i]=y[i]
 *
 * The currently supported options are:
 * <ul>
 * <li>INDEXING=xy/ij: Cartesian ("xy", default) or matrix ("ij") indexing of
 * output.
 * </li>
 * </ul>
 *
 * This is the same as
 * <a href="https://numpy.org/doc/stable/reference/generated/numpy.meshgrid.html">numpy.meshgrid()</a>
 * function.
 *
 * The returned array (of arrays) must be freed with GDALReleaseArrays().
 * If only the array itself needs to be freed, CPLFree() should be called
 * (and GDALMDArrayRelease() on individual array members).
 *
 * This is the same as the C++ method GDALMDArray::GetMeshGrid()
 *
 * @param pahInputArrays Input arrays
 * @param nCountInputArrays Number of input arrays
 * @param pnCountOutputArrays Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions NULL, or NULL terminated list of options.
 *
 * @return an array of *pnCountOutputArrays arrays.
 * @since 3.10
 */
GDALMDArrayH *GDALMDArrayGetMeshGrid(const GDALMDArrayH *pahInputArrays,
                                     size_t nCountInputArrays,
                                     size_t *pnCountOutputArrays,
                                     CSLConstList papszOptions)
{
    VALIDATE_POINTER1(pahInputArrays, __func__, nullptr);
    VALIDATE_POINTER1(pnCountOutputArrays, __func__, nullptr);

    std::vector<std::shared_ptr<GDALMDArray>> apoInputArrays;
    for (size_t i = 0; i < nCountInputArrays; ++i)
        apoInputArrays.push_back(pahInputArrays[i]->m_poImpl);

    const auto apoOutputArrays =
        GDALMDArray::GetMeshGrid(apoInputArrays, papszOptions);
    auto ret = static_cast<GDALMDArrayH *>(
        CPLMalloc(sizeof(GDALMDArrayH) * apoOutputArrays.size()));
    for (size_t i = 0; i < apoOutputArrays.size(); i++)
    {
        ret[i] = new GDALMDArrayHS(apoOutputArrays[i]);
    }
    *pnCountOutputArrays = apoOutputArrays.size();
    return ret;
}

/************************************************************************/
/*                         GDALReleaseArrays()                          */
/************************************************************************/

/** Free the return of GDALMDArrayGetCoordinateVariables()
 *
 * @param arrays return pointer of above methods
 * @param nCount *pnCount value returned by above methods
 */
void GDALReleaseArrays(GDALMDArrayH *arrays, size_t nCount)
{
    for (size_t i = 0; i < nCount; i++)
    {
        delete arrays[i];
    }
    CPLFree(arrays);
}

/************************************************************************/
/*                          GDALMDArrayCache()                          */
/************************************************************************/

/**
 * \brief Cache the content of the array into an auxiliary filename.
 *
 * This is the same as the C++ method GDALMDArray::Cache().
 *
 * @since GDAL 3.4
 */

int GDALMDArrayCache(GDALMDArrayH hArray, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, FALSE);
    return hArray->m_poImpl->Cache(papszOptions);
}

/************************************************************************/
/*                         GDALMDArrayRename()                          */
/************************************************************************/

/** Rename the array.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF, Zarr.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Rename()
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALMDArrayRename(GDALMDArrayH hArray, const char *pszNewName)
{
    VALIDATE_POINTER1(hArray, __func__, false);
    VALIDATE_POINTER1(pszNewName, __func__, false);
    return hArray->m_poImpl->Rename(pszNewName);
}

/************************************************************************/
/*                    GDALMDArrayAsClassicDataset()                     */
/************************************************************************/

/** Return a view of this array as a "classic" GDALDataset (ie 2D)
 *
 * Only 2D or more arrays are supported.
 *
 * In the case of > 2D arrays, additional dimensions will be represented as
 * raster bands.
 *
 * The "reverse" methods are GDALRasterBand::AsMDArray() and
 * GDALDataset::AsMDArray()
 *
 * This is the same as the C++ method GDALMDArray::AsClassicDataset().
 *
 * @param hArray Array.
 * @param iXDim Index of the dimension that will be used as the X/width axis.
 * @param iYDim Index of the dimension that will be used as the Y/height axis.
 * @return a new GDALDataset that must be freed with GDALClose(), or nullptr
 */
GDALDatasetH GDALMDArrayAsClassicDataset(GDALMDArrayH hArray, size_t iXDim,
                                         size_t iYDim)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return GDALDataset::ToHandle(
        hArray->m_poImpl->AsClassicDataset(iXDim, iYDim));
}

/************************************************************************/
/*                   GDALMDArrayAsClassicDatasetEx()                    */
/************************************************************************/

/** Return a view of this array as a "classic" GDALDataset (ie 2D)
 *
 * Only 2D or more arrays are supported.
 *
 * In the case of > 2D arrays, additional dimensions will be represented as
 * raster bands.
 *
 * The "reverse" method is GDALRasterBand::AsMDArray().
 *
 * This is the same as the C++ method GDALMDArray::AsClassicDataset().
 * @param hArray Array.
 * @param iXDim Index of the dimension that will be used as the X/width axis.
 * @param iYDim Index of the dimension that will be used as the Y/height axis.
 *              Ignored if the dimension count is 1.
 * @param hRootGroup Root group, or NULL. Used with the BAND_METADATA and
 *                   BAND_IMAGERY_METADATA option.
 * @param papszOptions Cf GDALMDArray::AsClassicDataset()
 * @return a new GDALDataset that must be freed with GDALClose(), or nullptr
 * @since GDAL 3.8
 */
GDALDatasetH GDALMDArrayAsClassicDatasetEx(GDALMDArrayH hArray, size_t iXDim,
                                           size_t iYDim, GDALGroupH hRootGroup,
                                           CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    return GDALDataset::ToHandle(hArray->m_poImpl->AsClassicDataset(
        iXDim, iYDim, hRootGroup ? hRootGroup->m_poImpl : nullptr,
        papszOptions));
}

/************************************************************************/
/*                    GDALMDArray::GetRawBlockInfo()                    */
/************************************************************************/

/** Return information on a raw block.
 *
 * The block coordinates must be between 0 and
 * (GetDimensions()[i]->GetSize() / GetBlockSize()[i]) - 1, for all i between
 * 0 and GetDimensionCount()-1.
 *
 * If the queried block has valid coordinates but is missing in the dataset,
 * all fields of info will be set to 0/nullptr, but the function will return
 * true.
 *
 * This method is only implemented by a subset of drivers. The base
 * implementation just returns false and empty info.
 *
 * The values returned in psBlockInfo->papszInfo are driver dependent.
 *
 * For multi-byte data types, drivers should return a "ENDIANNESS" key whose
 * value is "LITTLE" or "BIG".
 *
 * For HDF5 and netCDF 4, the potential keys are "COMPRESSION" (possible values
 * "DEFLATE" or "SZIP") and "FILTER" (if several filters, names are
 * comma-separated)
 *
 * For ZARR, the potential keys are "COMPRESSOR" (value is the JSON encoded
 * content from the array definition), "FILTERS" (for Zarr V2, value is JSON
 * encoded content) and "TRANSPOSE_ORDER" (value is a string like
 * "[idx0,...,idxN]" with the permutation).
 *
 * For VRT, the potential keys are the ones of the underlying source(s). Note
 * that GetRawBlockInfo() on VRT only works when the VRT declares a block size,
 * that for each queried VRT block, there is one and only one source that
 * is used to fill the VRT block and that the block size of this source is
 * exactly the one of the VRT block.
 *
 * This is the same as C function GDALMDArrayGetRawBlockInfo().
 *
 * @param panBlockCoordinates array of GetDimensionCount() values with the block
 *                            coordinates.
 * @param[out] info structure to fill with block information.
 * @return true in case of success, or false if an error occurs.
 * @since 3.12
 */
bool GDALMDArray::GetRawBlockInfo(const uint64_t *panBlockCoordinates,
                                  GDALMDArrayRawBlockInfo &info) const
{
    (void)panBlockCoordinates;
    info.clear();
    return false;
}

/************************************************************************/
/*                     GDALMDArrayGetRawBlockInfo()                     */
/************************************************************************/

/** Return information on a raw block.
 *
 * The block coordinates must be between 0 and
 * (GetDimensions()[i]->GetSize() / GetBlockSize()[i]) - 1, for all i between
 * 0 and GetDimensionCount()-1.
 *
 * If the queried block has valid coordinates but is missing in the dataset,
 * all fields of info will be set to 0/nullptr, but the function will return
 * true.
 *
 * This method is only implemented by a subset of drivers. The base
 * implementation just returns false and empty info.
 *
 * The values returned in psBlockInfo->papszInfo are driver dependent.
 *
 * For multi-byte data types, drivers should return a "ENDIANNESS" key whose
 * value is "LITTLE" or "BIG".
 *
 * For HDF5 and netCDF 4, the potential keys are "COMPRESSION" (possible values
 * "DEFLATE" or "SZIP") and "FILTER" (if several filters, names are
 * comma-separated)
 *
 * For ZARR, the potential keys are "COMPRESSOR" (value is the JSON encoded
 * content from the array definition), "FILTERS" (for Zarr V2, value is JSON
 * encoded content) and "TRANSPOSE_ORDER" (value is a string like
 * "[idx0,...,idxN]" with the permutation).
 *
 * For VRT, the potential keys are the ones of the underlying source(s). Note
 * that GetRawBlockInfo() on VRT only works when the VRT declares a block size,
 * that for each queried VRT block, there is one and only one source that
 * is used to fill the VRT block and that the block size of this source is
 * exactly the one of the VRT block.
 *
 * This is the same as C++ method GDALMDArray::GetRawBlockInfo().
 *
 * @param hArray handle to array.
 * @param panBlockCoordinates array of GetDimensionCount() values with the block
 *                            coordinates.
 * @param[out] psBlockInfo structure to fill with block information.
 *                         Must be allocated with GDALMDArrayRawBlockInfoCreate(),
 *                         and freed with GDALMDArrayRawBlockInfoRelease().
 * @return true in case of success, or false if an error occurs.
 * @since 3.12
 */
bool GDALMDArrayGetRawBlockInfo(GDALMDArrayH hArray,
                                const uint64_t *panBlockCoordinates,
                                GDALMDArrayRawBlockInfo *psBlockInfo)
{
    VALIDATE_POINTER1(hArray, __func__, false);
    VALIDATE_POINTER1(panBlockCoordinates, __func__, false);
    VALIDATE_POINTER1(psBlockInfo, __func__, false);
    return hArray->m_poImpl->GetRawBlockInfo(panBlockCoordinates, *psBlockInfo);
}

/************************************************************************/
/*                   GDALMDArrayRawBlockInfoCreate()                    */
/************************************************************************/

/** Allocate a new instance of GDALMDArrayRawBlockInfo.
 *
 * Returned pointer must be freed with GDALMDArrayRawBlockInfoRelease().
 *
 * @since 3.12
 */
GDALMDArrayRawBlockInfo *GDALMDArrayRawBlockInfoCreate(void)
{
    return new GDALMDArrayRawBlockInfo();
}

/************************************************************************/
/*                   GDALMDArrayRawBlockInfoRelease()                   */
/************************************************************************/

/** Free an instance of GDALMDArrayRawBlockInfo.
 *
 * @since 3.12
 */
void GDALMDArrayRawBlockInfoRelease(GDALMDArrayRawBlockInfo *psBlockInfo)
{
    delete psBlockInfo;
}

/************************************************************************/
/*                   GDALMDArray::GetOverviewCount()                    */
/************************************************************************/

/**
 * \brief Return the number of overview arrays available.
 *
 * This method is the same as the C function GDALMDArrayGetOverviewCount().
 *
 * @return overview count, zero if none.
 *
 * @since 3.13
 */

int GDALMDArray::GetOverviewCount() const
{
    return 0;
}

/************************************************************************/
/*                    GDALMDArrayGetOverviewCount()                     */
/************************************************************************/
/**
 * \brief Return the number of overview arrays available.
 *
 * This method is the same as the C++ method GDALMDArray::GetOverviewCount().
 *
 * @param hArray Array.
 * @return overview count, zero if none.
 *
 * @since 3.13
 */

int GDALMDArrayGetOverviewCount(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hArray, __func__, 0);
    return hArray->m_poImpl->GetOverviewCount();
}

/************************************************************************/
/*                      GDALMDArray::GetOverview()                      */
/************************************************************************/

/**
 * \brief Get overview array object.
 *
 * This method is the same as the C function GDALMDArrayGetOverview().
 *
 * @param nIdx overview index between 0 and GetOverviewCount()-1.
 *
 * @return overview GDALMDArray, or nullptr
 *
 * @since 3.13
 */

std::shared_ptr<GDALMDArray> GDALMDArray::GetOverview(int nIdx) const
{
    (void)nIdx;
    return nullptr;
}

/************************************************************************/
/*                       GDALMDArrayGetOverview()                       */
/************************************************************************/

/**
 * \brief Get overview array object.
 *
 * This method is the same as the C++ method GDALMDArray::GetOverview().
 *
 * @param hArray Array.
 * @param nIdx overview index between 0 and GDALMDArrayGetOverviewCount()-1.
 *
 * @return overview GDALMDArray, or nullptr.
 * Must be released with GDALMDArrayRelease()
 *
 * @since 3.13
 */

GDALMDArrayH GDALMDArrayGetOverview(GDALMDArrayH hArray, int nIdx)
{
    VALIDATE_POINTER1(hArray, __func__, nullptr);
    auto poOverview = hArray->m_poImpl->GetOverview(nIdx);
    if (!poOverview)
        return nullptr;
    return new GDALMDArrayHS(poOverview);
}

/************************************************************************/
/*                    GDALMDArray::BuildOverviews()                     */
/************************************************************************/

/** Build overviews for this array.
 *
 * Creates reduced resolution copies of this array using the specified
 * resampling method. The driver is responsible for storing the overview
 * arrays and any associated metadata (e.g., multiscales convention for Zarr).
 *
 * For arrays with more than 2 dimensions, only the spatial dimensions
 * (last two by default, or as specified by the spatial:dimensions
 * attribute) are downsampled. Non-spatial dimensions are preserved.
 *
 * Overview factors need not be sorted; the implementation will sort and
 * deduplicate them. Each level is resampled sequentially from the
 * previous level (e.g., 4x is built from 2x, not from the base).
 *
 * This method can also be invoked via GDALDataset::BuildOverviews()
 * when the dataset was obtained through GDALMDArray::AsClassicDataset().
 *
 * @note The Zarr v3 implementation replaces all existing overviews on each
 * call, unlike GDALDataset::BuildOverviews() which may add new levels.
 *
 * @note Currently only implemented by the Zarr v3 driver.
 *
 * @param pszResampling Resampling method name (e.g., "NEAREST", "AVERAGE").
 *                      If nullptr or empty, defaults to "NEAREST".
 * @param nOverviews Number of overview levels to build. Pass 0 to remove
 *                   all existing overviews.
 * @param panOverviewList Array of overview decimation factors (e.g., 2, 4, 8).
 *                        Each factor must be >= 2. May be nullptr when
 *                        nOverviews is 0.
 * @param pfnProgress Progress callback, or nullptr.
 * @param pProgressData Progress callback user data.
 * @param papszOptions Driver-specific options, or nullptr.
 * @return CE_None on success, CE_Failure otherwise.
 * @since GDAL 3.13
 */
CPLErr GDALMDArray::BuildOverviews(CPL_UNUSED const char *pszResampling,
                                   CPL_UNUSED int nOverviews,
                                   CPL_UNUSED const int *panOverviewList,
                                   CPL_UNUSED GDALProgressFunc pfnProgress,
                                   CPL_UNUSED void *pProgressData,
                                   CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "BuildOverviews() not supported by this driver");
    return CE_Failure;
}

/************************************************************************/
/*                     GDALMDArrayBuildOverviews()                      */
/************************************************************************/

/** \brief Build overviews for a multidimensional array.
 *
 * This is the same as the C++ method GDALMDArray::BuildOverviews().
 *
 * @since GDAL 3.13
 */
CPLErr GDALMDArrayBuildOverviews(GDALMDArrayH hArray, const char *pszResampling,
                                 int nOverviews, const int *panOverviewList,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hArray, __func__, CE_Failure);
    return hArray->m_poImpl->BuildOverviews(pszResampling, nOverviews,
                                            panOverviewList, pfnProgress,
                                            pProgressData, papszOptions);
}

/************************************************************************/
/*                    GDALMDArrayGuessGeoTransform()                    */
/************************************************************************/

/** \brief Returns whether 2 specified dimensions form a geotransform.
 *
 * This is the same as the C++ method GDALMDArray::GuessGeoTransform().
 *
 * @since GDAL 3.14
 */
bool GDALMDArrayGuessGeoTransform(GDALMDArrayH hArray, size_t nDimX,
                                  size_t nDimY, bool bPixelIsPoint,
                                  double padfGeoTransform[6])
{
    VALIDATE_POINTER1(hArray, __func__, false);

    const auto dimCount = hArray->m_poImpl->GetDimensionCount();
    if (nDimX >= dimCount || nDimY >= dimCount)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Dimension index out of range");
        return false;
    }
    // we allow nDimX and nDimY to be equal, harmless if not meaningful
    return hArray->m_poImpl->GuessGeoTransform(nDimX, nDimY, bPixelIsPoint,
                                               padfGeoTransform);
}

/************************************************************************/
/*                    GDALMDArrayIsRegularlySpaced()                    */
/************************************************************************/

/** \brief Returns whether an array is a 1D regularly spaced array.
 *
 * This is the same as the C++ method GDALMDArray::IsRegularlySpaced().
 *
 * @since GDAL 3.14
 */
bool GDALMDArrayIsRegularlySpaced(GDALMDArrayH hArray, double *pdfStart,
                                  double *pdfIncrement)
{
    VALIDATE_POINTER1(hArray, __func__, false);
    return hArray->m_poImpl->IsRegularlySpaced(*pdfStart, *pdfIncrement);
}
