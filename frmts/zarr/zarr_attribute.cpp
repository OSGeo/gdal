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

#include "zarr.h"

#include <algorithm>
#include <cassert>
#include <map>

/************************************************************************/
/*             ZarrAttributeGroup::ZarrAttributeGroup()                 */
/************************************************************************/

ZarrAttributeGroup::ZarrAttributeGroup(const std::string& osParentName):
                                            m_oGroup(osParentName, nullptr)
{
}

/************************************************************************/
/*                   ZarrAttributeGroup::Init()                         */
/************************************************************************/

void ZarrAttributeGroup::Init(const CPLJSONObject& obj, bool bUpdatable)
{
    if( obj.GetType() != CPLJSONObject::Type::Object )
        return;
    const auto children = obj.GetChildren();
    for( const auto& item: children )
    {
        const auto itemType = item.GetType();
        bool bDone = false;
        std::shared_ptr<GDALAttribute> poAttr;
        if( itemType == CPLJSONObject::Type::String )
        {
            bDone = true;
            poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {},
                GDALExtendedDataType::CreateString(), nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const std::string str = item.ToString();
                const char* c_str = str.c_str();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              poAttr->GetDataType(),
                              &c_str);
            }
        }
        else if( itemType == CPLJSONObject::Type::Integer ||
                 itemType == CPLJSONObject::Type::Long ||
                 itemType == CPLJSONObject::Type::Double )
        {
            bDone = true;
            poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {},
                GDALExtendedDataType::Create(
                    itemType == CPLJSONObject::Type::Integer ?
                        GDT_Int32 : GDT_Float64),
                nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const double val = item.ToDouble();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              GDALExtendedDataType::Create(GDT_Float64),
                              &val);
            }
        }
        else if( itemType == CPLJSONObject::Type::Array )
        {
            const auto array = item.ToArray();
            bool isFirst = true;
            bool isString = false;
            bool isNumeric = false;
            bool foundInt64 = false;
            bool foundDouble = false;
            bool mixedType = false;
            size_t countItems = 0;
            for( const auto& subItem: array )
            {
                const auto subItemType = subItem.GetType();
                if( subItemType == CPLJSONObject::Type::String )
                {
                    if( isFirst )
                    {
                        isString = true;
                    }
                    else if( !isString )
                    {
                        mixedType = true;
                        break;
                    }
                    countItems ++;
                }
                else if( subItemType == CPLJSONObject::Type::Integer ||
                         subItemType == CPLJSONObject::Type::Long ||
                         subItemType == CPLJSONObject::Type::Double )
                {
                    if( isFirst )
                    {
                        isNumeric = true;
                    }
                    else if( !isNumeric )
                    {
                        mixedType = true;
                        break;
                    }
                    if( subItemType == CPLJSONObject::Type::Double )
                        foundDouble = true;
                    else if( subItemType == CPLJSONObject::Type::Long )
                        foundInt64 = true;
                    countItems ++;
                }
                else
                {
                    mixedType = true;
                    break;
                }
                isFirst = false;
            }

            if( !mixedType && !isFirst )
            {
                bDone = true;
                poAttr = m_oGroup.CreateAttribute(
                    item.GetName(), { countItems },
                    isString ?
                        GDALExtendedDataType::CreateString():
                        GDALExtendedDataType::Create(
                            (foundDouble || foundInt64) ? GDT_Float64 : GDT_Int32),
                    nullptr);
                if( poAttr )
                {
                    size_t idx = 0;
                    for( const auto& subItem: array )
                    {
                        const GUInt64 arrayStartIdx = idx;
                        const size_t count = 1;
                        const GInt64 arrayStep = 0;
                        const GPtrDiff_t bufferStride = 0;
                        const auto subItemType = subItem.GetType();
                        if( subItemType == CPLJSONObject::Type::String )
                        {
                            const std::string str = subItem.ToString();
                            const char* c_str = str.c_str();
                            poAttr->Write(&arrayStartIdx,
                                          &count,
                                          &arrayStep,
                                          &bufferStride,
                                          poAttr->GetDataType(),
                                          &c_str);
                        }
                        else if( subItemType == CPLJSONObject::Type::Integer ||
                                 subItemType == CPLJSONObject::Type::Long ||
                                 subItemType == CPLJSONObject::Type::Double )
                        {
                            const double val = subItem.ToDouble();
                            poAttr->Write(&arrayStartIdx,
                                          &count,
                                          &arrayStep,
                                          &bufferStride,
                                          GDALExtendedDataType::Create(GDT_Float64),
                                          &val);
                        }
                        ++idx;
                    }
                }
            }
        }

        if( !bDone )
        {
            constexpr size_t nMaxStringLength = 0;
            const auto eDT = GDALExtendedDataType::CreateString(nMaxStringLength, GEDTST_JSON);
            poAttr = m_oGroup.CreateAttribute(
                item.GetName(), {}, eDT, nullptr);
            if( poAttr )
            {
                const GUInt64 arrayStartIdx = 0;
                const size_t count = 1;
                const GInt64 arrayStep = 0;
                const GPtrDiff_t bufferStride = 0;
                const std::string str = item.ToString();
                const char* c_str = str.c_str();
                poAttr->Write(&arrayStartIdx,
                              &count,
                              &arrayStep,
                              &bufferStride,
                              poAttr->GetDataType(),
                              &c_str);
            }
        }

        auto poMemAttr = std::dynamic_pointer_cast<MEMAttribute>(poAttr);
        if( poMemAttr )
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
    const auto attrs = m_oGroup.GetAttributes(nullptr);
    for( const auto& attr: attrs )
    {
        const auto oType = attr->GetDataType();
        if( oType.GetClass() == GEDTC_STRING )
        {
            const auto anDims = attr->GetDimensionsSize();
            if( anDims.size() == 0 )
            {
                const char* pszStr = attr->ReadAsString();
                if( pszStr )
                {
                    CPLJSONDocument oDoc;
                    if(  oType.GetSubType() == GEDTST_JSON && oDoc.LoadMemory(pszStr) )
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
            else if ( anDims.size() == 1 )
            {
                const auto list = attr->ReadAsStringArray();
                CPLJSONArray arr;
                for( int i = 0; i < list.size(); ++i )
                {
                    arr.Add(list[i]);
                }
                o.Add(attr->GetName(), arr);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot serialize attribute %s of dimension count >= 2",
                         attr->GetName().c_str());
            }
        }
        else if( oType.GetClass() == GEDTC_NUMERIC )
        {
            const auto anDims = attr->GetDimensionsSize();
            const auto eDT = oType.GetNumericDataType();
            if( anDims.size() == 0 )
            {
                const double dfVal = attr->ReadAsDouble();
                if( eDT == GDT_Byte || eDT == GDT_UInt16 || eDT == GDT_UInt32 ||
                    eDT == GDT_Int16 || eDT == GDT_Int32 )
                {
                    o.Add(attr->GetName(), static_cast<GInt64>(dfVal));
                }
                else
                {
                    o.Add(attr->GetName(), dfVal);
                }
            }
            else if ( anDims.size() == 1 )
            {
                const auto list = attr->ReadAsDoubleArray();
                CPLJSONArray arr;
                for( const auto dfVal: list )
                {
                    if( eDT == GDT_Byte || eDT == GDT_UInt16 || eDT == GDT_UInt32 ||
                        eDT == GDT_Int16 || eDT == GDT_Int32 )
                    {
                        arr.Add(static_cast<GInt64>(dfVal));
                    }
                    else
                    {
                        arr.Add(dfVal);
                    }
                }
                o.Add(attr->GetName(), arr);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot serialize attribute %s of dimension count >= 2",
                         attr->GetName().c_str());
            }
        }
    }
    return o;
}
