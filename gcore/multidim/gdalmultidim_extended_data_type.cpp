/******************************************************************************
 *
 * Name:     gdalmultidim_group.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALExtendedDataType class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdal_rat.h"

#include <limits>
#include <map>

/************************************************************************/
/*                             CopyValues()                             */
/************************************************************************/

/** Convert several values from a source type to a destination type.
 *
 * If dstType is GEDTC_STRING, the written value will be a pointer to a char*,
 * that must be freed with CPLFree().
 */
bool GDALExtendedDataType::CopyValues(const void *pSrc,
                                      const GDALExtendedDataType &srcType,
                                      GPtrDiff_t nSrcStrideInElts, void *pDst,
                                      const GDALExtendedDataType &dstType,
                                      GPtrDiff_t nDstStrideInElts,
                                      size_t nValues)
{
    const auto nSrcStrideInBytes =
        nSrcStrideInElts * static_cast<GPtrDiff_t>(srcType.GetSize());
    const auto nDstStrideInBytes =
        nDstStrideInElts * static_cast<GPtrDiff_t>(dstType.GetSize());
    if (srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_NUMERIC &&
        nSrcStrideInBytes >= std::numeric_limits<int>::min() &&
        nSrcStrideInBytes <= std::numeric_limits<int>::max() &&
        nDstStrideInBytes >= std::numeric_limits<int>::min() &&
        nDstStrideInBytes <= std::numeric_limits<int>::max())
    {
        GDALCopyWords64(pSrc, srcType.GetNumericDataType(),
                        static_cast<int>(nSrcStrideInBytes), pDst,
                        dstType.GetNumericDataType(),
                        static_cast<int>(nDstStrideInBytes), nValues);
    }
    else
    {
        const GByte *pabySrc = static_cast<const GByte *>(pSrc);
        GByte *pabyDst = static_cast<GByte *>(pDst);
        for (size_t i = 0; i < nValues; ++i)
        {
            if (!CopyValue(pabySrc, srcType, pabyDst, dstType))
                return false;
            pabySrc += nSrcStrideInBytes;
            pabyDst += nDstStrideInBytes;
        }
    }
    return true;
}

/************************************************************************/
/*                       ~GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::~GDALExtendedDataType() = default;

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(size_t nMaxStringLength,
                                           GDALExtendedDataTypeSubType eSubType)
    : m_eClass(GEDTC_STRING), m_eSubType(eSubType), m_nSize(sizeof(char *)),
      m_nMaxStringLength(nMaxStringLength)
{
}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(GDALDataType eType)
    : m_eClass(GEDTC_NUMERIC), m_eNumericDT(eType),
      m_nSize(GDALGetDataTypeSizeBytes(eType))
{
}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(
    const std::string &osName, GDALDataType eBaseType,
    std::unique_ptr<GDALRasterAttributeTable> poRAT)
    : m_osName(osName), m_eClass(GEDTC_NUMERIC), m_eNumericDT(eBaseType),
      m_nSize(GDALGetDataTypeSizeBytes(eBaseType)), m_poRAT(std::move(poRAT))
{
}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(
    const std::string &osName, size_t nTotalSize,
    std::vector<std::unique_ptr<GDALEDTComponent>> &&components)
    : m_osName(osName), m_eClass(GEDTC_COMPOUND),
      m_aoComponents(std::move(components)), m_nSize(nTotalSize)
{
}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

/** Move constructor. */
GDALExtendedDataType::GDALExtendedDataType(GDALExtendedDataType &&) = default;

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

/** Copy constructor. */
GDALExtendedDataType::GDALExtendedDataType(const GDALExtendedDataType &other)
    : m_osName(other.m_osName), m_eClass(other.m_eClass),
      m_eSubType(other.m_eSubType), m_eNumericDT(other.m_eNumericDT),
      m_nSize(other.m_nSize), m_nMaxStringLength(other.m_nMaxStringLength),
      m_poRAT(other.m_poRAT ? other.m_poRAT->Clone() : nullptr)
{
    if (m_eClass == GEDTC_COMPOUND)
    {
        for (const auto &elt : other.m_aoComponents)
        {
            m_aoComponents.emplace_back(new GDALEDTComponent(*elt));
        }
    }
}

/************************************************************************/
/*                             operator= ()                             */
/************************************************************************/

/** Copy assignment. */
GDALExtendedDataType &
GDALExtendedDataType::operator=(const GDALExtendedDataType &other)
{
    if (this != &other)
    {
        m_osName = other.m_osName;
        m_eClass = other.m_eClass;
        m_eSubType = other.m_eSubType;
        m_eNumericDT = other.m_eNumericDT;
        m_nSize = other.m_nSize;
        m_nMaxStringLength = other.m_nMaxStringLength;
        m_poRAT.reset(other.m_poRAT ? other.m_poRAT->Clone() : nullptr);
        m_aoComponents.clear();
        if (m_eClass == GEDTC_COMPOUND)
        {
            for (const auto &elt : other.m_aoComponents)
            {
                m_aoComponents.emplace_back(new GDALEDTComponent(*elt));
            }
        }
    }
    return *this;
}

/************************************************************************/
/*                             operator= ()                             */
/************************************************************************/

/** Move assignment. */
GDALExtendedDataType &
GDALExtendedDataType::operator=(GDALExtendedDataType &&other) = default;

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_NUMERIC.
 *
 * This is the same as the C function GDALExtendedDataTypeCreate()
 *
 * @param eType Numeric data type. Must be different from GDT_Unknown and
 * GDT_TypeCount
 */
GDALExtendedDataType GDALExtendedDataType::Create(GDALDataType eType)
{
    return GDALExtendedDataType(eType);
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/** Return a new GDALExtendedDataType from a raster attribute table.
 *
 * @param osName Type name
 * @param eBaseType Base integer data type.
 * @param poRAT Raster attribute table. Must not be NULL.
 * @since 3.12
 */
GDALExtendedDataType
GDALExtendedDataType::Create(const std::string &osName, GDALDataType eBaseType,
                             std::unique_ptr<GDALRasterAttributeTable> poRAT)
{
    return GDALExtendedDataType(osName, eBaseType, std::move(poRAT));
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_COMPOUND.
 *
 * This is the same as the C function GDALExtendedDataTypeCreateCompound()
 *
 * @param osName Type name.
 * @param nTotalSize Total size of the type in bytes.
 *                   Should be large enough to store all components.
 * @param components Components of the compound type.
 */
GDALExtendedDataType GDALExtendedDataType::Create(
    const std::string &osName, size_t nTotalSize,
    std::vector<std::unique_ptr<GDALEDTComponent>> &&components)
{
    size_t nLastOffset = 0;
    // Some arbitrary threshold to avoid potential integer overflows
    if (nTotalSize > static_cast<size_t>(std::numeric_limits<int>::max() / 2))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
        return GDALExtendedDataType(GDT_Unknown);
    }
    for (const auto &comp : components)
    {
        // Check alignment too ?
        if (comp->GetOffset() < nLastOffset)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
            return GDALExtendedDataType(GDT_Unknown);
        }
        nLastOffset = comp->GetOffset() + comp->GetType().GetSize();
    }
    if (nTotalSize < nLastOffset)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
        return GDALExtendedDataType(GDT_Unknown);
    }
    if (nTotalSize == 0 || components.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty compound not allowed");
        return GDALExtendedDataType(GDT_Unknown);
    }
    return GDALExtendedDataType(osName, nTotalSize, std::move(components));
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_STRING.
 *
 * This is the same as the C function GDALExtendedDataTypeCreateString().
 *
 * @param nMaxStringLength maximum length of a string in bytes. 0 if
 * unknown/unlimited
 * @param eSubType Subtype.
 */
GDALExtendedDataType
GDALExtendedDataType::CreateString(size_t nMaxStringLength,
                                   GDALExtendedDataTypeSubType eSubType)
{
    return GDALExtendedDataType(nMaxStringLength, eSubType);
}

/************************************************************************/
/*                             operator==()                             */
/************************************************************************/

/** Equality operator.
 *
 * This is the same as the C function GDALExtendedDataTypeEquals().
 */
bool GDALExtendedDataType::operator==(const GDALExtendedDataType &other) const
{
    if (m_eClass != other.m_eClass || m_eSubType != other.m_eSubType ||
        m_nSize != other.m_nSize || m_osName != other.m_osName)
    {
        return false;
    }
    if (m_eClass == GEDTC_NUMERIC)
    {
        return m_eNumericDT == other.m_eNumericDT;
    }
    if (m_eClass == GEDTC_STRING)
    {
        return true;
    }
    CPLAssert(m_eClass == GEDTC_COMPOUND);
    if (m_aoComponents.size() != other.m_aoComponents.size())
    {
        return false;
    }
    for (size_t i = 0; i < m_aoComponents.size(); i++)
    {
        if (!(*m_aoComponents[i] == *other.m_aoComponents[i]))
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                            CanConvertTo()                            */
/************************************************************************/

/** Return whether this data type can be converted to the other one.
 *
 * This is the same as the C function GDALExtendedDataTypeCanConvertTo().
 *
 * @param other Target data type for the conversion being considered.
 */
bool GDALExtendedDataType::CanConvertTo(const GDALExtendedDataType &other) const
{
    if (m_eClass == GEDTC_NUMERIC)
    {
        if (m_eNumericDT == GDT_Unknown)
            return false;
        if (other.m_eClass == GEDTC_NUMERIC &&
            other.m_eNumericDT == GDT_Unknown)
            return false;
        return other.m_eClass == GEDTC_NUMERIC ||
               other.m_eClass == GEDTC_STRING;
    }
    if (m_eClass == GEDTC_STRING)
    {
        return other.m_eClass == m_eClass;
    }
    CPLAssert(m_eClass == GEDTC_COMPOUND);
    if (other.m_eClass != GEDTC_COMPOUND)
        return false;
    std::map<std::string, const std::unique_ptr<GDALEDTComponent> *>
        srcComponents;
    for (const auto &srcComp : m_aoComponents)
    {
        srcComponents[srcComp->GetName()] = &srcComp;
    }
    for (const auto &dstComp : other.m_aoComponents)
    {
        auto oIter = srcComponents.find(dstComp->GetName());
        if (oIter == srcComponents.end())
            return false;
        if (!(*(oIter->second))->GetType().CanConvertTo(dstComp->GetType()))
            return false;
    }
    return true;
}

/************************************************************************/
/*                       NeedsFreeDynamicMemory()                       */
/************************************************************************/

/** Return whether the data type holds dynamically allocated memory, that
 * needs to be freed with FreeDynamicMemory().
 *
 */
bool GDALExtendedDataType::NeedsFreeDynamicMemory() const
{
    switch (m_eClass)
    {
        case GEDTC_STRING:
            return true;

        case GEDTC_NUMERIC:
            return false;

        case GEDTC_COMPOUND:
        {
            for (const auto &comp : m_aoComponents)
            {
                if (comp->GetType().NeedsFreeDynamicMemory())
                    return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*                         FreeDynamicMemory()                          */
/************************************************************************/

/** Release the dynamic memory (strings typically) from a raw value.
 *
 * This is the same as the C function GDALExtendedDataTypeFreeDynamicMemory().
 *
 * @param pBuffer Raw buffer of a single element of an attribute or array value.
 */
void GDALExtendedDataType::FreeDynamicMemory(void *pBuffer) const
{
    switch (m_eClass)
    {
        case GEDTC_STRING:
        {
            char *pszStr;
            memcpy(&pszStr, pBuffer, sizeof(char *));
            if (pszStr)
            {
                VSIFree(pszStr);
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            break;
        }

        case GEDTC_COMPOUND:
        {
            GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
            for (const auto &comp : m_aoComponents)
            {
                comp->GetType().FreeDynamicMemory(pabyBuffer +
                                                  comp->GetOffset());
            }
            break;
        }
    }
}

/************************************************************************/
/*                         ~GDALEDTComponent()                          */
/************************************************************************/

GDALEDTComponent::~GDALEDTComponent() = default;

/************************************************************************/
/*                          GDALEDTComponent()                          */
/************************************************************************/

/** constructor of a GDALEDTComponent
 *
 * This is the same as the C function GDALEDTComponendCreate()
 *
 * @param name Component name
 * @param offset Offset in byte of the component in the compound data type.
 *               In case of nesting of compound data type, this should be
 *               the offset to the immediate belonging data type, not to the
 *               higher level one.
 * @param type   Component data type.
 */
GDALEDTComponent::GDALEDTComponent(const std::string &name, size_t offset,
                                   const GDALExtendedDataType &type)
    : m_osName(name), m_nOffset(offset), m_oType(type)
{
}

/************************************************************************/
/*                          GDALEDTComponent()                          */
/************************************************************************/

/** Copy constructor. */
GDALEDTComponent::GDALEDTComponent(const GDALEDTComponent &) = default;

/************************************************************************/
/*                             operator==()                             */
/************************************************************************/

/** Equality operator.
 */
bool GDALEDTComponent::operator==(const GDALEDTComponent &other) const
{
    return m_osName == other.m_osName && m_nOffset == other.m_nOffset &&
           m_oType == other.m_oType;
}
