/******************************************************************************
 *
 * Name:     gdalmultidim_attribute.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALIHasAttribute and GDALAttribute classes
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"

#include <limits>

#if defined(__clang__) || defined(_MSC_VER)
#define COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT
#endif

/************************************************************************/
/*                         ~GDALIHasAttribute()                         */
/************************************************************************/

GDALIHasAttribute::~GDALIHasAttribute() = default;

/************************************************************************/
/*                            GetAttribute()                            */
/************************************************************************/

/** Return an attribute by its name.
 *
 * If the attribute does not exist, nullptr should be silently returned.
 *
 * @note Driver implementation: this method will fallback to
 * GetAttributeFromAttributes() is not explicitly implemented
 *
 * Drivers known to implement it for groups and arrays: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetAttribute() or
 * GDALMDArrayGetAttribute().
 *
 * @param osName Attribute name
 * @return the attribute, or nullptr if it does not exist or an error occurred.
 */
std::shared_ptr<GDALAttribute>
GDALIHasAttribute::GetAttribute(const std::string &osName) const
{
    return GetAttributeFromAttributes(osName);
}

/************************************************************************/
/*                     GetAttributeFromAttributes()                     */
/************************************************************************/

/** Possible fallback implementation for GetAttribute() using GetAttributes().
 */
std::shared_ptr<GDALAttribute>
GDALIHasAttribute::GetAttributeFromAttributes(const std::string &osName) const
{
    auto attrs(GetAttributes());
    for (const auto &attr : attrs)
    {
        if (attr->GetName() == osName)
            return attr;
    }
    return nullptr;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

/** Return the list of attributes contained in a GDALMDArray or GDALGroup.
 *
 * If the attribute does not exist, nullptr should be silently returned.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * GetAttribute() should also be implemented.
 *
 * Drivers known to implement it for groups and arrays: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetAttributes() or
 * GDALMDArrayGetAttributes().

 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the attributes.
 */
std::vector<std::shared_ptr<GDALAttribute>>
GDALIHasAttribute::GetAttributes(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                          CreateAttribute()                           */
/************************************************************************/

/** Create an attribute within a GDALMDArray or GDALGroup.
 *
 * The attribute might not be "physically" created until a value is written
 * into it.
 *
 * Optionally implemented.
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateAttribute() or
 * GDALMDArrayCreateAttribute()
 *
 * @param osName Attribute name.
 * @param anDimensions List of dimension sizes, ordered from the slowest varying
 *                     dimension first to the fastest varying dimension last.
 *                     Empty for a scalar attribute (common case)
 * @param oDataType  Attribute data type.
 * @param papszOptions Driver specific options determining how the attribute.
 * should be created.
 *
 * @return the new attribute, or nullptr if case of error
 */
std::shared_ptr<GDALAttribute> GDALIHasAttribute::CreateAttribute(
    CPL_UNUSED const std::string &osName,
    CPL_UNUSED const std::vector<GUInt64> &anDimensions,
    CPL_UNUSED const GDALExtendedDataType &oDataType,
    CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateAttribute() not implemented");
    return nullptr;
}

/************************************************************************/
/*                          DeleteAttribute()                           */
/************************************************************************/

/** Delete an attribute from a GDALMDArray or GDALGroup.
 *
 * Optionally implemented.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupDeleteAttribute() or
 * GDALMDArrayDeleteAttribute()
 *
 * @param osName Attribute name.
 * @param papszOptions Driver specific options determining how the attribute.
 * should be deleted.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALIHasAttribute::DeleteAttribute(CPL_UNUSED const std::string &osName,
                                        CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "DeleteAttribute() not implemented");
    return false;
}

/************************************************************************/
/*                           GDALAttribute()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALAttribute::GDALAttribute(CPL_UNUSED const std::string &osParentName,
                             CPL_UNUSED const std::string &osName)
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
    : GDALAbstractMDArray(osParentName, osName)
#endif
{
}

GDALAttribute::~GDALAttribute() = default;

//! @endcond

/************************************************************************/
/*                          GetDimensionSize()                          */
/************************************************************************/

/** Return the size of the dimensions of the attribute.
 *
 * This will be an empty array for a scalar (single value) attribute.
 *
 * This is the same as the C function GDALAttributeGetDimensionsSize().
 */
std::vector<GUInt64> GDALAttribute::GetDimensionsSize() const
{
    const auto &dims = GetDimensions();
    std::vector<GUInt64> ret;
    ret.reserve(dims.size());
    for (const auto &dim : dims)
        ret.push_back(dim->GetSize());
    return ret;
}

//! @cond Doxygen_Suppress

GDALAttributeString::GDALAttributeString(const std::string &osParentName,
                                         const std::string &osName,
                                         const std::string &osValue,
                                         GDALExtendedDataTypeSubType eSubType)
    : GDALAbstractMDArray(osParentName, osName),
      GDALAttribute(osParentName, osName),
      m_dt(GDALExtendedDataType::CreateString(0, eSubType)), m_osValue(osValue)
{
}

const std::vector<std::shared_ptr<GDALDimension>> &
GDALAttributeString::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALAttributeString::GetDataType() const
{
    return m_dt;
}

bool GDALAttributeString::IRead(const GUInt64 *, const size_t *, const GInt64 *,
                                const GPtrDiff_t *,
                                const GDALExtendedDataType &bufferDataType,
                                void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_STRING)
        return false;
    char *pszStr = static_cast<char *>(VSIMalloc(m_osValue.size() + 1));
    if (!pszStr)
        return false;
    memcpy(pszStr, m_osValue.c_str(), m_osValue.size() + 1);
    *static_cast<char **>(pDstBuffer) = pszStr;
    return true;
}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string &osParentName,
                                           const std::string &osName,
                                           double dfValue)
    : GDALAbstractMDArray(osParentName, osName),
      GDALAttribute(osParentName, osName),
      m_dt(GDALExtendedDataType::Create(GDT_Float64)), m_dfValue(dfValue)
{
}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string &osParentName,
                                           const std::string &osName,
                                           int nValue)
    : GDALAbstractMDArray(osParentName, osName),
      GDALAttribute(osParentName, osName),
      m_dt(GDALExtendedDataType::Create(GDT_Int32)), m_nValue(nValue)
{
}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string &osParentName,
                                           const std::string &osName,
                                           const std::vector<GUInt32> &anValues)
    : GDALAbstractMDArray(osParentName, osName),
      GDALAttribute(osParentName, osName),
      m_dt(GDALExtendedDataType::Create(GDT_UInt32)), m_anValuesUInt32(anValues)
{
    m_dims.push_back(std::make_shared<GDALDimension>(
        std::string(), "dim0", std::string(), std::string(),
        m_anValuesUInt32.size()));
}

const std::vector<std::shared_ptr<GDALDimension>> &
GDALAttributeNumeric::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALAttributeNumeric::GetDataType() const
{
    return m_dt;
}

bool GDALAttributeNumeric::IRead(const GUInt64 *arrayStartIdx,
                                 const size_t *count, const GInt64 *arrayStep,
                                 const GPtrDiff_t *bufferStride,
                                 const GDALExtendedDataType &bufferDataType,
                                 void *pDstBuffer) const
{
    if (m_dims.empty())
    {
        if (m_dt.GetNumericDataType() == GDT_Float64)
            GDALExtendedDataType::CopyValue(&m_dfValue, m_dt, pDstBuffer,
                                            bufferDataType);
        else
        {
            CPLAssert(m_dt.GetNumericDataType() == GDT_Int32);
            GDALExtendedDataType::CopyValue(&m_nValue, m_dt, pDstBuffer,
                                            bufferDataType);
        }
    }
    else
    {
        CPLAssert(m_dt.GetNumericDataType() == GDT_UInt32);
        GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
        for (size_t i = 0; i < count[0]; ++i)
        {
            GDALExtendedDataType::CopyValue(
                &m_anValuesUInt32[static_cast<size_t>(arrayStartIdx[0] +
                                                      i * arrayStep[0])],
                m_dt, pabyDstBuffer, bufferDataType);
            pabyDstBuffer += bufferDataType.GetSize() * bufferStride[0];
        }
    }
    return true;
}

//! @endcond

/************************************************************************/
/*                             ReadAsRaw()                              */
/************************************************************************/

/** Return the raw value of an attribute.
 *
 *
 * This is the same as the C function GDALAttributeReadAsRaw().
 */
GDALRawResult GDALAttribute::ReadAsRaw() const
{
    const auto nEltCount(GetTotalElementsCount());
    const auto &dt(GetDataType());
    const auto nDTSize(dt.GetSize());
    GByte *res = static_cast<GByte *>(
        VSI_MALLOC2_VERBOSE(static_cast<size_t>(nEltCount), nDTSize));
    if (!res)
        return GDALRawResult(nullptr, dt, 0);
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    if (!Read(startIdx.data(), count.data(), nullptr, nullptr, dt, &res[0],
              &res[0], static_cast<size_t>(nEltCount * nDTSize)))
    {
        VSIFree(res);
        return GDALRawResult(nullptr, dt, 0);
    }
    return GDALRawResult(res, dt, static_cast<size_t>(nEltCount));
}

/************************************************************************/
/*                            ReadAsString()                            */
/************************************************************************/

/** Return the value of an attribute as a string.
 *
 * The returned string should not be freed, and its lifetime does not
 * excess a next call to ReadAsString() on the same object, or the deletion
 * of the object itself.
 *
 * This function will only return the first element if there are several.
 *
 * This is the same as the C function GDALAttributeReadAsString()
 *
 * @return a string, or nullptr.
 */
const char *GDALAttribute::ReadAsString() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    char *szRet = nullptr;
    if (!Read(startIdx.data(), count.data(), nullptr, nullptr,
              GDALExtendedDataType::CreateString(), &szRet, &szRet,
              sizeof(szRet)) ||
        szRet == nullptr)
    {
        return nullptr;
    }
    m_osCachedVal = szRet;
    CPLFree(szRet);
    return m_osCachedVal.c_str();
}

/************************************************************************/
/*                             ReadAsInt()                              */
/************************************************************************/

/** Return the value of an attribute as a integer.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to integer.
 *
 * This is the same as the C function GDALAttributeReadAsInt()
 *
 * @return a integer, or INT_MIN in case of error.
 */
int GDALAttribute::ReadAsInt() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    int nRet = INT_MIN;
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int32), &nRet, &nRet, sizeof(nRet));
    return nRet;
}

/************************************************************************/
/*                            ReadAsInt64()                             */
/************************************************************************/

/** Return the value of an attribute as an int64_t.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to long.
 *
 * This is the same as the C function GDALAttributeReadAsInt64()
 *
 * @return an int64_t, or INT64_MIN in case of error.
 */
int64_t GDALAttribute::ReadAsInt64() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    int64_t nRet = INT64_MIN;
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int64), &nRet, &nRet, sizeof(nRet));
    return nRet;
}

/************************************************************************/
/*                            ReadAsDouble()                            */
/************************************************************************/

/** Return the value of an attribute as a double.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can not be converted to double.
 *
 * This is the same as the C function GDALAttributeReadAsInt()
 *
 * @return a double value.
 */
double GDALAttribute::ReadAsDouble() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    double dfRet = 0;
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Float64), &dfRet, &dfRet,
         sizeof(dfRet));
    return dfRet;
}

/************************************************************************/
/*                         ReadAsStringArray()                          */
/************************************************************************/

/** Return the value of an attribute as an array of strings.
 *
 * This is the same as the C function GDALAttributeReadAsStringArray()
 */
CPLStringList GDALAttribute::ReadAsStringArray() const
{
    const auto nElts = GetTotalElementsCount();
    if (nElts > static_cast<unsigned>(std::numeric_limits<int>::max() - 1))
        return CPLStringList();
    char **papszList = static_cast<char **>(
        VSI_CALLOC_VERBOSE(static_cast<int>(nElts) + 1, sizeof(char *)));
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::CreateString(), papszList, papszList,
         sizeof(char *) * static_cast<int>(nElts));
    for (int i = 0; i < static_cast<int>(nElts); i++)
    {
        if (papszList[i] == nullptr)
            papszList[i] = CPLStrdup("");
    }
    return CPLStringList(papszList);
}

/************************************************************************/
/*                           ReadAsIntArray()                           */
/************************************************************************/

/** Return the value of an attribute as an array of integers.
 *
 * This is the same as the C function GDALAttributeReadAsIntArray().
 */
std::vector<int> GDALAttribute::ReadAsIntArray() const
{
    const auto nElts = GetTotalElementsCount();
#if SIZEOF_VOIDP == 4
    if (nElts > static_cast<size_t>(nElts))
        return {};
#endif
    std::vector<int> res(static_cast<size_t>(nElts));
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int32), &res[0], res.data(),
         res.size() * sizeof(res[0]));
    return res;
}

/************************************************************************/
/*                          ReadAsInt64Array()                          */
/************************************************************************/

/** Return the value of an attribute as an array of int64_t.
 *
 * This is the same as the C function GDALAttributeReadAsInt64Array().
 */
std::vector<int64_t> GDALAttribute::ReadAsInt64Array() const
{
    const auto nElts = GetTotalElementsCount();
#if SIZEOF_VOIDP == 4
    if (nElts > static_cast<size_t>(nElts))
        return {};
#endif
    std::vector<int64_t> res(static_cast<size_t>(nElts));
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int64), &res[0], res.data(),
         res.size() * sizeof(res[0]));
    return res;
}

/************************************************************************/
/*                         ReadAsDoubleArray()                          */
/************************************************************************/

/** Return the value of an attribute as an array of double.
 *
 * This is the same as the C function GDALAttributeReadAsDoubleArray().
 */
std::vector<double> GDALAttribute::ReadAsDoubleArray() const
{
    const auto nElts = GetTotalElementsCount();
#if SIZEOF_VOIDP == 4
    if (nElts > static_cast<size_t>(nElts))
        return {};
#endif
    std::vector<double> res(static_cast<size_t>(nElts));
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Float64), &res[0], res.data(),
         res.size() * sizeof(res[0]));
    return res;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from raw values expressed in GetDataType()
 *
 * The values should be provided in the type of GetDataType() and there should
 * be exactly GetTotalElementsCount() of them.
 * If GetDataType() is a string, each value should be a char* pointer.
 *
 * This is the same as the C function GDALAttributeWriteRaw().
 *
 * @param pabyValue Buffer of nLen bytes.
 * @param nLen Size of pabyValue in bytes. Should be equal to
 *             GetTotalElementsCount() * GetDataType().GetSize()
 * @return true in case of success.
 */
bool GDALAttribute::Write(const void *pabyValue, size_t nLen)
{
    if (nLen != GetTotalElementsCount() * GetDataType().GetSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Length is not of expected value");
        return false;
    }
    const auto &dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    return Write(startIdx.data(), count.data(), nullptr, nullptr, GetDataType(),
                 pabyValue, pabyValue, nLen);
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from a string value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteString().
 *
 * @param pszValue Pointer to a string.
 * @return true in case of success.
 */
bool GDALAttribute::Write(const char *pszValue)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::CreateString(), &pszValue, &pszValue,
                 sizeof(pszValue));
}

/************************************************************************/
/*                              WriteInt()                              */
/************************************************************************/

/** Write an attribute from a integer value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteInt().
 *
 * @param nVal Value.
 * @return true in case of success.
 */
bool GDALAttribute::WriteInt(int nVal)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Int32), &nVal, &nVal,
                 sizeof(nVal));
}

/************************************************************************/
/*                             WriteInt64()                             */
/************************************************************************/

/** Write an attribute from an int64_t value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteInt().
 *
 * @param nVal Value.
 * @return true in case of success.
 */
bool GDALAttribute::WriteInt64(int64_t nVal)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Int64), &nVal, &nVal,
                 sizeof(nVal));
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from a double value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteDouble().
 *
 * @param dfVal Value.
 * @return true in case of success.
 */
bool GDALAttribute::Write(double dfVal)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Float64), &dfVal, &dfVal,
                 sizeof(dfVal));
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from an array of strings.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteStringArray().
 *
 * @param vals Array of strings.
 * @return true in case of success.
 */
bool GDALAttribute::Write(CSLConstList vals)
{
    if (static_cast<size_t>(CSLCount(vals)) != GetTotalElementsCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto &dims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::CreateString(), vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) * sizeof(char *));
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from an array of int.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteIntArray()
 *
 * @param vals Array of int.
 * @param nVals Should be equal to GetTotalElementsCount().
 * @return true in case of success.
 */
bool GDALAttribute::Write(const int *vals, size_t nVals)
{
    if (nVals != GetTotalElementsCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto &dims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Int32), vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) * sizeof(GInt32));
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from an array of int64_t.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteLongArray()
 *
 * @param vals Array of int64_t.
 * @param nVals Should be equal to GetTotalElementsCount().
 * @return true in case of success.
 */
bool GDALAttribute::Write(const int64_t *vals, size_t nVals)
{
    if (nVals != GetTotalElementsCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto &dims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Int64), vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) *
                     sizeof(int64_t));
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from an array of double.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteDoubleArray()
 *
 * @param vals Array of double.
 * @param nVals Should be equal to GetTotalElementsCount().
 * @return true in case of success.
 */
bool GDALAttribute::Write(const double *vals, size_t nVals)
{
    if (nVals != GetTotalElementsCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto &dims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Float64), vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) * sizeof(double));
}
