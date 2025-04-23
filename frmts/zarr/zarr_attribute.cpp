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

#include "zarr.h"

#include <algorithm>
#include <cassert>
#include <map>

constexpr const char *ATTRIBUTE_GROUP_SUFFIX = "/_GLOBAL_";

/************************************************************************/
/*             ZarrAttributeGroup::ZarrAttributeGroup()                 */
/************************************************************************/

ZarrAttributeGroup::ZarrAttributeGroup(const std::string &osParentName,
                                       bool bContainerIsGroup)
    : m_bContainerIsGroup(bContainerIsGroup),
      m_poGroup(MEMGroup::Create(
          bContainerIsGroup
              ? (osParentName == "/" ? ATTRIBUTE_GROUP_SUFFIX
                                     : osParentName + ATTRIBUTE_GROUP_SUFFIX)
              : osParentName,
          nullptr))
{
}

/************************************************************************/
/*                   ZarrAttributeGroup::Init()                         */
/************************************************************************/

void ZarrAttributeGroup::Init(const CPLJSONObject &obj, bool bUpdatable)
{
    if (obj.GetType() != CPLJSONObject::Type::Object)
        return;
    const auto children = obj.GetChildren();
    for (const auto &item : children)
    {
        const auto itemType = item.GetType();
        bool bDone = false;
        std::shared_ptr<GDALAttribute> poAttr;
        switch (itemType)
        {
            case CPLJSONObject::Type::String:
            {
                bDone = true;
                poAttr = m_poGroup->CreateAttribute(
                    item.GetName(), {}, GDALExtendedDataType::CreateString(),
                    nullptr);
                if (poAttr)
                {
                    const GUInt64 arrayStartIdx = 0;
                    const size_t count = 1;
                    const GInt64 arrayStep = 0;
                    const GPtrDiff_t bufferStride = 0;
                    const std::string str = item.ToString();
                    const char *c_str = str.c_str();
                    poAttr->Write(&arrayStartIdx, &count, &arrayStep,
                                  &bufferStride, poAttr->GetDataType(), &c_str);
                }
                break;
            }
            case CPLJSONObject::Type::Integer:
            {
                bDone = true;
                poAttr = m_poGroup->CreateAttribute(
                    item.GetName(), {}, GDALExtendedDataType::Create(GDT_Int32),
                    nullptr);
                if (poAttr)
                {
                    const GUInt64 arrayStartIdx = 0;
                    const size_t count = 1;
                    const GInt64 arrayStep = 0;
                    const GPtrDiff_t bufferStride = 0;
                    const int val = item.ToInteger();
                    poAttr->Write(
                        &arrayStartIdx, &count, &arrayStep, &bufferStride,
                        GDALExtendedDataType::Create(GDT_Int32), &val);
                }
                break;
            }
            case CPLJSONObject::Type::Long:
            {
                bDone = true;
                poAttr = m_poGroup->CreateAttribute(
                    item.GetName(), {}, GDALExtendedDataType::Create(GDT_Int64),
                    nullptr);
                if (poAttr)
                {
                    const GUInt64 arrayStartIdx = 0;
                    const size_t count = 1;
                    const GInt64 arrayStep = 0;
                    const GPtrDiff_t bufferStride = 0;
                    const int64_t val = item.ToLong();
                    poAttr->Write(
                        &arrayStartIdx, &count, &arrayStep, &bufferStride,
                        GDALExtendedDataType::Create(GDT_Int64), &val);
                }
                break;
            }
            case CPLJSONObject::Type::Double:
            {
                bDone = true;
                poAttr = m_poGroup->CreateAttribute(
                    item.GetName(), {},
                    GDALExtendedDataType::Create(GDT_Float64), nullptr);
                if (poAttr)
                {
                    const GUInt64 arrayStartIdx = 0;
                    const size_t count = 1;
                    const GInt64 arrayStep = 0;
                    const GPtrDiff_t bufferStride = 0;
                    const double val = item.ToDouble();
                    poAttr->Write(
                        &arrayStartIdx, &count, &arrayStep, &bufferStride,
                        GDALExtendedDataType::Create(GDT_Float64), &val);
                }
                break;
            }
            case CPLJSONObject::Type::Array:
            {
                const auto array = item.ToArray();
                bool isFirst = true;
                bool isString = false;
                bool isNumeric = false;
                bool foundInt64 = false;
                bool foundDouble = false;
                bool mixedType = false;
                size_t countItems = 0;
                for (const auto &subItem : array)
                {
                    const auto subItemType = subItem.GetType();
                    if (subItemType == CPLJSONObject::Type::String)
                    {
                        if (isFirst)
                        {
                            isString = true;
                        }
                        else if (!isString)
                        {
                            mixedType = true;
                            break;
                        }
                        countItems++;
                    }
                    else if (subItemType == CPLJSONObject::Type::Integer ||
                             subItemType == CPLJSONObject::Type::Long ||
                             subItemType == CPLJSONObject::Type::Double)
                    {
                        if (isFirst)
                        {
                            isNumeric = true;
                        }
                        else if (!isNumeric)
                        {
                            mixedType = true;
                            break;
                        }
                        if (subItemType == CPLJSONObject::Type::Double)
                            foundDouble = true;
                        else if (subItemType == CPLJSONObject::Type::Long)
                            foundInt64 = true;
                        countItems++;
                    }
                    else
                    {
                        mixedType = true;
                        break;
                    }
                    isFirst = false;
                }

                if (!mixedType && !isFirst)
                {
                    bDone = true;
                    poAttr = m_poGroup->CreateAttribute(
                        item.GetName(), {countItems},
                        isString ? GDALExtendedDataType::CreateString()
                                 : GDALExtendedDataType::Create(
                                       foundDouble  ? GDT_Float64
                                       : foundInt64 ? GDT_Int64
                                                    : GDT_Int32),
                        nullptr);
                    if (poAttr)
                    {
                        size_t idx = 0;
                        for (const auto &subItem : array)
                        {
                            const GUInt64 arrayStartIdx = idx;
                            const size_t count = 1;
                            const GInt64 arrayStep = 0;
                            const GPtrDiff_t bufferStride = 0;
                            const auto subItemType = subItem.GetType();
                            switch (subItemType)
                            {
                                case CPLJSONObject::Type::String:
                                {
                                    const std::string str = subItem.ToString();
                                    const char *c_str = str.c_str();
                                    poAttr->Write(&arrayStartIdx, &count,
                                                  &arrayStep, &bufferStride,
                                                  poAttr->GetDataType(),
                                                  &c_str);
                                    break;
                                }
                                case CPLJSONObject::Type::Integer:
                                {
                                    const int val = subItem.ToInteger();
                                    poAttr->Write(
                                        &arrayStartIdx, &count, &arrayStep,
                                        &bufferStride,
                                        GDALExtendedDataType::Create(GDT_Int32),
                                        &val);
                                    break;
                                }
                                case CPLJSONObject::Type::Long:
                                {
                                    const int64_t val = subItem.ToLong();
                                    poAttr->Write(
                                        &arrayStartIdx, &count, &arrayStep,
                                        &bufferStride,
                                        GDALExtendedDataType::Create(GDT_Int64),
                                        &val);
                                    break;
                                }
                                case CPLJSONObject::Type::Double:
                                {
                                    const double val = subItem.ToDouble();
                                    poAttr->Write(&arrayStartIdx, &count,
                                                  &arrayStep, &bufferStride,
                                                  GDALExtendedDataType::Create(
                                                      GDT_Float64),
                                                  &val);
                                    break;
                                }
                                default:
                                    // Ignore other JSON object types
                                    break;
                            }
                            ++idx;
                        }
                    }
                }
                break;
            }
            default:
                // Ignore other JSON object types
                break;
        }

        if (!bDone)
        {
            constexpr size_t nMaxStringLength = 0;
            const auto eDT = GDALExtendedDataType::CreateString(
                nMaxStringLength, GEDTST_JSON);
            poAttr =
                m_poGroup->CreateAttribute(item.GetName(), {}, eDT, nullptr);
            if (poAttr)
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const std::string str = item.ToString();
                const char *c_str = str.c_str();
                poAttr->Write(&arrayStartIdx, &count, &arrayStep, &bufferStride,
                              poAttr->GetDataType(), &c_str);
            }
        }

        auto poMemAttr = std::dynamic_pointer_cast<MEMAttribute>(poAttr);
        if (poMemAttr)
            poMemAttr->SetModified(false);
    }
    SetUpdatable(bUpdatable);
}

/************************************************************************/
/*                    ZarrAttributeGroup::Serialize()                   */
/************************************************************************/

CPLJSONObject ZarrAttributeGroup::Serialize() const
{
    CPLJSONObject o;
    const auto attrs = m_poGroup->GetAttributes(nullptr);
    for (const auto &attr : attrs)
    {
        const auto &oType = attr->GetDataType();
        if (oType.GetClass() == GEDTC_STRING)
        {
            const auto anDims = attr->GetDimensionsSize();
            if (anDims.size() == 0)
            {
                const char *pszStr = attr->ReadAsString();
                if (pszStr)
                {
                    CPLJSONDocument oDoc;
                    if (oType.GetSubType() == GEDTST_JSON &&
                        oDoc.LoadMemory(pszStr))
                    {
                        o.Add(attr->GetName(), oDoc.GetRoot());
                    }
                    else
                    {
                        o.Add(attr->GetName(), pszStr);
                    }
                }
                else
                {
                    o.AddNull(attr->GetName());
                }
            }
            else if (anDims.size() == 1)
            {
                const auto list = attr->ReadAsStringArray();
                CPLJSONArray arr;
                for (int i = 0; i < list.size(); ++i)
                {
                    arr.Add(list[i]);
                }
                o.Add(attr->GetName(), arr);
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Cannot serialize attribute %s of dimension count >= 2",
                    attr->GetName().c_str());
            }
        }
        else if (oType.GetClass() == GEDTC_NUMERIC)
        {
            const auto anDims = attr->GetDimensionsSize();
            const auto eDT = oType.GetNumericDataType();
            if (anDims.size() == 0)
            {
                if (eDT == GDT_Int8 || eDT == GDT_Int16 || eDT == GDT_Int32 ||
                    eDT == GDT_Int64)
                {
                    const int64_t nVal = attr->ReadAsInt64();
                    o.Add(attr->GetName(), static_cast<GInt64>(nVal));
                }
                else if (eDT == GDT_Byte || eDT == GDT_UInt16 ||
                         eDT == GDT_UInt32 || eDT == GDT_UInt64)
                {
                    const int64_t nVal = attr->ReadAsInt64();
                    o.Add(attr->GetName(), static_cast<uint64_t>(nVal));
                }
                else
                {
                    const double dfVal = attr->ReadAsDouble();
                    o.Add(attr->GetName(), dfVal);
                }
            }
            else if (anDims.size() == 1)
            {
                CPLJSONArray arr;
                if (eDT == GDT_Int8 || eDT == GDT_Int16 || eDT == GDT_Int32 ||
                    eDT == GDT_Int64)
                {
                    const auto list = attr->ReadAsInt64Array();
                    for (const auto nVal : list)
                    {
                        arr.Add(static_cast<GInt64>(nVal));
                    }
                }
                else if (eDT == GDT_Byte || eDT == GDT_UInt16 ||
                         eDT == GDT_UInt32 || eDT == GDT_UInt64)
                {
                    const auto list = attr->ReadAsInt64Array();
                    for (const auto nVal : list)
                    {
                        arr.Add(static_cast<uint64_t>(nVal));
                    }
                }
                else
                {
                    const auto list = attr->ReadAsDoubleArray();
                    for (const auto dfVal : list)
                    {
                        arr.Add(dfVal);
                    }
                }
                o.Add(attr->GetName(), arr);
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Cannot serialize attribute %s of dimension count >= 2",
                    attr->GetName().c_str());
            }
        }
    }
    return o;
}

/************************************************************************/
/*                          ParentRenamed()                             */
/************************************************************************/

void ZarrAttributeGroup::ParentRenamed(const std::string &osNewParentFullName)
{
    if (m_bContainerIsGroup)
        m_poGroup->SetFullName(osNewParentFullName + ATTRIBUTE_GROUP_SUFFIX);
    else
        m_poGroup->SetFullName(osNewParentFullName);
    const auto attrs = m_poGroup->GetAttributes(nullptr);
    for (auto &attr : attrs)
    {
        attr->ParentRenamed(m_poGroup->GetFullName());
    }
}

/************************************************************************/
/*                          ParentDeleted()                             */
/************************************************************************/

void ZarrAttributeGroup::ParentDeleted()
{
    m_poGroup->Deleted();
}
