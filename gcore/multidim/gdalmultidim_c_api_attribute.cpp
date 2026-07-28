/******************************************************************************
 *
 * Name:     gdalmultidim_c_api_attribute.cpp
 * Project:  GDAL Core
 * Purpose:  C API for GDALAttribute
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
/*                        GDALAttributeRelease()                        */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALAttribute.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALAttributeRelease(GDALAttributeH hAttr)
{
    delete hAttr;
}

/************************************************************************/
/*                       GDALReleaseAttributes()                        */
/************************************************************************/

/** Free the return of GDALGroupGetAttributes() or GDALMDArrayGetAttributes()
 *
 * @param attributes return pointer of above methods
 * @param nCount *pnCount value returned by above methods
 */
void GDALReleaseAttributes(GDALAttributeH *attributes, size_t nCount)
{
    for (size_t i = 0; i < nCount; i++)
    {
        delete attributes[i];
    }
    CPLFree(attributes);
}

/************************************************************************/
/*                        GDALAttributeGetName()                        */
/************************************************************************/

/** Return the name of the attribute.
 *
 * The returned pointer is valid until hAttr is released.
 *
 * This is the same as the C++ method GDALAttribute::GetName().
 */
const char *GDALAttributeGetName(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    return hAttr->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                      GDALAttributeGetFullName()                      */
/************************************************************************/

/** Return the full name of the attribute.
 *
 * The returned pointer is valid until hAttr is released.
 *
 * This is the same as the C++ method GDALAttribute::GetFullName().
 */
const char *GDALAttributeGetFullName(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    return hAttr->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                 GDALAttributeGetTotalElementsCount()                 */
/************************************************************************/

/** Return the total number of values in the attribute.
 *
 * This is the same as the C++ method
 * GDALAbstractMDArray::GetTotalElementsCount()
 */
GUInt64 GDALAttributeGetTotalElementsCount(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, 0);
    return hAttr->m_poImpl->GetTotalElementsCount();
}

/************************************************************************/
/*                   GDALAttributeGetDimensionCount()                   */
/************************************************************************/

/** Return the number of dimensions.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensionCount()
 */
size_t GDALAttributeGetDimensionCount(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, 0);
    return hAttr->m_poImpl->GetDimensionCount();
}

/************************************************************************/
/*                   GDALAttributeGetDimensionsSize()                   */
/************************************************************************/

/** Return the dimension sizes of the attribute.
 *
 * The returned array must be freed with CPLFree()
 *
 * @param hAttr Attribute.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 *
 * @return an array of *pnCount values.
 */
GUInt64 *GDALAttributeGetDimensionsSize(GDALAttributeH hAttr, size_t *pnCount)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    const auto &dims = hAttr->m_poImpl->GetDimensions();
    auto ret = static_cast<GUInt64 *>(CPLMalloc(sizeof(GUInt64) * dims.size()));
    for (size_t i = 0; i < dims.size(); i++)
    {
        ret[i] = dims[i]->GetSize();
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                      GDALAttributeGetDataType()                      */
/************************************************************************/

/** Return the data type
 *
 * The return must be freed with GDALExtendedDataTypeRelease().
 */
GDALExtendedDataTypeH GDALAttributeGetDataType(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hAttr->m_poImpl->GetDataType()));
}

/************************************************************************/
/*                       GDALAttributeReadAsRaw()                       */
/************************************************************************/

/** Return the raw value of an attribute.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsRaw().
 *
 * The returned buffer must be freed with GDALAttributeFreeRawResult()
 *
 * @param hAttr Attribute.
 * @param pnSize Pointer to the number of bytes returned. Must NOT be NULL.
 *
 * @return a buffer of *pnSize bytes.
 */
GByte *GDALAttributeReadAsRaw(GDALAttributeH hAttr, size_t *pnSize)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    VALIDATE_POINTER1(pnSize, __func__, nullptr);
    auto res(hAttr->m_poImpl->ReadAsRaw());
    *pnSize = res.size();
    auto ret = res.StealData();
    if (!ret)
    {
        *pnSize = 0;
        return nullptr;
    }
    return ret;
}

/************************************************************************/
/*                     GDALAttributeFreeRawResult()                     */
/************************************************************************/

/** Free the return of GDALAttributeAsRaw()
 */
void GDALAttributeFreeRawResult(GDALAttributeH hAttr, GByte *raw,
                                CPL_UNUSED size_t nSize)
{
    VALIDATE_POINTER0(hAttr, __func__);
    if (raw)
    {
        const auto &dt(hAttr->m_poImpl->GetDataType());
        const auto nDTSize(dt.GetSize());
        GByte *pabyPtr = raw;
        const auto nEltCount(hAttr->m_poImpl->GetTotalElementsCount());
        CPLAssert(nSize == nDTSize * nEltCount);
        for (size_t i = 0; i < nEltCount; ++i)
        {
            dt.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
        CPLFree(raw);
    }
}

/************************************************************************/
/*                     GDALAttributeReadAsString()                      */
/************************************************************************/

/** Return the value of an attribute as a string.
 *
 * The returned string should not be freed, and its lifetime does not
 * excess a next call to ReadAsString() on the same object, or the deletion
 * of the object itself.
 *
 * This function will only return the first element if there are several.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsString()
 *
 * @return a string, or nullptr.
 */
const char *GDALAttributeReadAsString(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    return hAttr->m_poImpl->ReadAsString();
}

/************************************************************************/
/*                       GDALAttributeReadAsInt()                       */
/************************************************************************/

/** Return the value of an attribute as a integer.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to integer.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsInt()
 *
 * @return a integer, or INT_MIN in case of error.
 */
int GDALAttributeReadAsInt(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, 0);
    return hAttr->m_poImpl->ReadAsInt();
}

/************************************************************************/
/*                      GDALAttributeReadAsInt64()                      */
/************************************************************************/

/** Return the value of an attribute as a int64_t.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to integer.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsInt64()
 *
 * @return an int64_t, or INT64_MIN in case of error.
 */
int64_t GDALAttributeReadAsInt64(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, 0);
    return hAttr->m_poImpl->ReadAsInt64();
}

/************************************************************************/
/*                     GDALAttributeReadAsDouble()                      */
/************************************************************************/

/** Return the value of an attribute as a double.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to double.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsDouble()
 *
 * @return a double value.
 */
double GDALAttributeReadAsDouble(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, 0);
    return hAttr->m_poImpl->ReadAsDouble();
}

/************************************************************************/
/*                   GDALAttributeReadAsStringArray()                   */
/************************************************************************/

/** Return the value of an attribute as an array of strings.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsStringArray()
 *
 * The return value must be freed with CSLDestroy().
 */
char **GDALAttributeReadAsStringArray(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    return hAttr->m_poImpl->ReadAsStringArray().StealList();
}

/************************************************************************/
/*                    GDALAttributeReadAsIntArray()                     */
/************************************************************************/

/** Return the value of an attribute as an array of integers.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsIntArray()
 *
 * @param hAttr Attribute
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return array to be freed with CPLFree(), or nullptr.
 */
int *GDALAttributeReadAsIntArray(GDALAttributeH hAttr, size_t *pnCount)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    *pnCount = 0;
    auto tmp(hAttr->m_poImpl->ReadAsIntArray());
    if (tmp.empty())
        return nullptr;
    auto ret = static_cast<int *>(VSI_MALLOC2_VERBOSE(tmp.size(), sizeof(int)));
    if (!ret)
        return nullptr;
    memcpy(ret, tmp.data(), tmp.size() * sizeof(int));
    *pnCount = tmp.size();
    return ret;
}

/************************************************************************/
/*                   GDALAttributeReadAsInt64Array()                    */
/************************************************************************/

/** Return the value of an attribute as an array of int64_t.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsInt64Array()
 *
 * @param hAttr Attribute
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return array to be freed with CPLFree(), or nullptr.
 */
int64_t *GDALAttributeReadAsInt64Array(GDALAttributeH hAttr, size_t *pnCount)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    *pnCount = 0;
    auto tmp(hAttr->m_poImpl->ReadAsInt64Array());
    if (tmp.empty())
        return nullptr;
    auto ret = static_cast<int64_t *>(
        VSI_MALLOC2_VERBOSE(tmp.size(), sizeof(int64_t)));
    if (!ret)
        return nullptr;
    memcpy(ret, tmp.data(), tmp.size() * sizeof(int64_t));
    *pnCount = tmp.size();
    return ret;
}

/************************************************************************/
/*                   GDALAttributeReadAsDoubleArray()                   */
/************************************************************************/

/** Return the value of an attribute as an array of doubles.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsDoubleArray()
 *
 * @param hAttr Attribute
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return array to be freed with CPLFree(), or nullptr.
 */
double *GDALAttributeReadAsDoubleArray(GDALAttributeH hAttr, size_t *pnCount)
{
    VALIDATE_POINTER1(hAttr, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    *pnCount = 0;
    auto tmp(hAttr->m_poImpl->ReadAsDoubleArray());
    if (tmp.empty())
        return nullptr;
    auto ret =
        static_cast<double *>(VSI_MALLOC2_VERBOSE(tmp.size(), sizeof(double)));
    if (!ret)
        return nullptr;
    memcpy(ret, tmp.data(), tmp.size() * sizeof(double));
    *pnCount = tmp.size();
    return ret;
}

/************************************************************************/
/*                       GDALAttributeWriteRaw()                        */
/************************************************************************/

/** Write an attribute from raw values expressed in GetDataType()
 *
 * The values should be provided in the type of GetDataType() and there should
 * be exactly GetTotalElementsCount() of them.
 * If GetDataType() is a string, each value should be a char* pointer.
 *
 * This is the same as the C++ method GDALAttribute::Write(const void*, size_t).
 *
 * @param hAttr Attribute
 * @param pabyValue Buffer of nLen bytes.
 * @param nLength Size of pabyValue in bytes. Should be equal to
 *             GetTotalElementsCount() * GetDataType().GetSize()
 * @return TRUE in case of success.
 */
int GDALAttributeWriteRaw(GDALAttributeH hAttr, const void *pabyValue,
                          size_t nLength)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(pabyValue, nLength);
}

/************************************************************************/
/*                      GDALAttributeWriteString()                      */
/************************************************************************/

/** Write an attribute from a string value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::Write(const char*)
 *
 * @param hAttr Attribute
 * @param pszVal Pointer to a string.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteString(GDALAttributeH hAttr, const char *pszVal)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(pszVal);
}

/************************************************************************/
/*                       GDALAttributeWriteInt()                        */
/************************************************************************/

/** Write an attribute from a integer value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::WriteInt()
 *
 * @param hAttr Attribute
 * @param nVal Value.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteInt(GDALAttributeH hAttr, int nVal)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->WriteInt(nVal);
}

/************************************************************************/
/*                      GDALAttributeWriteInt64()                       */
/************************************************************************/

/** Write an attribute from an int64_t value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::WriteLong()
 *
 * @param hAttr Attribute
 * @param nVal Value.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteInt64(GDALAttributeH hAttr, int64_t nVal)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->WriteInt64(nVal);
}

/************************************************************************/
/*                      GDALAttributeWriteDouble()                      */
/************************************************************************/

/** Write an attribute from a double value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::Write(double);
 *
 * @param hAttr Attribute
 * @param dfVal Value.
 *
 * @return TRUE in case of success.
 */
int GDALAttributeWriteDouble(GDALAttributeH hAttr, double dfVal)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(dfVal);
}

/************************************************************************/
/*                   GDALAttributeWriteStringArray()                    */
/************************************************************************/

/** Write an attribute from an array of strings.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(CSLConstList)
 *
 * @param hAttr Attribute
 * @param papszValues Array of strings.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteStringArray(GDALAttributeH hAttr,
                                  CSLConstList papszValues)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(papszValues);
}

/************************************************************************/
/*                     GDALAttributeWriteIntArray()                     */
/************************************************************************/

/** Write an attribute from an array of int.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(const int *,
 * size_t)
 *
 * @param hAttr Attribute
 * @param panValues Array of int.
 * @param nCount Should be equal to GetTotalElementsCount().
 * @return TRUE in case of success.
 */
int GDALAttributeWriteIntArray(GDALAttributeH hAttr, const int *panValues,
                               size_t nCount)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(panValues, nCount);
}

/************************************************************************/
/*                    GDALAttributeWriteInt64Array()                    */
/************************************************************************/

/** Write an attribute from an array of int64_t.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(const int64_t *,
 * size_t)
 *
 * @param hAttr Attribute
 * @param panValues Array of int64_t.
 * @param nCount Should be equal to GetTotalElementsCount().
 * @return TRUE in case of success.
 */
int GDALAttributeWriteInt64Array(GDALAttributeH hAttr, const int64_t *panValues,
                                 size_t nCount)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(panValues, nCount);
}

/************************************************************************/
/*                   GDALAttributeWriteDoubleArray()                    */
/************************************************************************/

/** Write an attribute from an array of double.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(const double *,
 * size_t)
 *
 * @param hAttr Attribute
 * @param padfValues Array of double.
 * @param nCount Should be equal to GetTotalElementsCount().
 * @return TRUE in case of success.
 */
int GDALAttributeWriteDoubleArray(GDALAttributeH hAttr,
                                  const double *padfValues, size_t nCount)
{
    VALIDATE_POINTER1(hAttr, __func__, FALSE);
    return hAttr->m_poImpl->Write(padfValues, nCount);
}

/************************************************************************/
/*                        GDALAttributeRename()                         */
/************************************************************************/

/** Rename the attribute.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Rename()
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALAttributeRename(GDALAttributeH hAttr, const char *pszNewName)
{
    VALIDATE_POINTER1(hAttr, __func__, false);
    VALIDATE_POINTER1(pszNewName, __func__, false);
    return hAttr->m_poImpl->Rename(pszNewName);
}
