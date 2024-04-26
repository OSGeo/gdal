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

#include "tiledbmultidim.h"

/************************************************************************/
/*           TileDBAttributeHolder::~TileDBAttributeHolder()            */
/************************************************************************/

TileDBAttributeHolder::~TileDBAttributeHolder() = default;

/************************************************************************/
/*             TileDBAttributeHolder::CreateAttributeImpl()             */
/************************************************************************/

std::shared_ptr<GDALAttribute> TileDBAttributeHolder::CreateAttributeImpl(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList /*papszOptions*/)
{
    if (!IIsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }

    if (!EnsureOpenAs(TILEDB_READ))
        return nullptr;
    try
    {
        tiledb_datatype_t value_type;
        if (m_oMapAttributes.find(osName) != m_oMapAttributes.end() ||
            has_metadata(osName, &value_type))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "An attribute with same name already exists");
            return nullptr;
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "has_metadata() failed with: %s",
                 e.what());
        return nullptr;
    }
    if (!EnsureOpenAs(TILEDB_WRITE))
        return nullptr;

    auto poAttr = TileDBAttribute::Create(AsAttributeHolderSharedPtr(), osName,
                                          anDimensions, oDataType);
    if (poAttr)
        m_oMapAttributes[osName] = poAttr;
    return poAttr;
}

/************************************************************************/
/*                TileDBAttributeHolder::CreateAttribute()              */
/************************************************************************/

/* static */ std::shared_ptr<GDALAttribute>
TileDBAttributeHolder::CreateAttribute(
    const std::shared_ptr<TileDBAttributeHolder> &poSelf,
    const std::string &osName, tiledb_datatype_t value_type, uint32_t value_num,
    const void *value)
{
    if (value_type == TILEDB_STRING_ASCII || value_type == TILEDB_STRING_UTF8 ||
        (osName == "_gdal" && value_type == TILEDB_UINT8 && value &&
         CPLIsUTF8(static_cast<const char *>(value), value_num)))
    {
        return TileDBAttribute::Create(poSelf, osName, {},
                                       GDALExtendedDataType::CreateString());
    }
    else
    {
        GDALDataType eDT =
            TileDBArray::TileDBDataTypeToGDALDataType(value_type);
        if (eDT == GDT_Unknown)
        {
            const char *pszTypeName = "";
            tiledb_datatype_to_str(value_type, &pszTypeName);
            CPLDebug("TILEDB",
                     "Metadata item %s ignored because of unsupported "
                     "type %s",
                     osName.c_str(), pszTypeName);
        }
        else
        {
            return TileDBAttribute::Create(poSelf, osName, {value_num},
                                           GDALExtendedDataType::Create(eDT));
        }
    }
    return nullptr;
}

/************************************************************************/
/*                TileDBAttributeHolder::GetAttributeImpl()             */
/************************************************************************/

std::shared_ptr<GDALAttribute>
TileDBAttributeHolder::GetAttributeImpl(const std::string &osName) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return nullptr;

    auto oIter = m_oMapAttributes.find(osName);
    if (oIter != m_oMapAttributes.end())
        return oIter->second;

    try
    {
        tiledb_datatype_t value_type;
        uint32_t value_num;
        const void *value;
        get_metadata(osName, &value_type, &value_num, &value);
        if (value == nullptr)
            return nullptr;

        auto poAttr = CreateAttribute(AsAttributeHolderSharedPtr(), osName,
                                      value_type, value_num, value);
        if (poAttr)
        {
            m_oMapAttributes[osName] = poAttr;
        }
        return poAttr;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetAttribute() failed with: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                       IsSpecialAttribute()                           */
/************************************************************************/

static bool IsSpecialAttribute(const std::string &osName)
{
    return osName == CRS_ATTRIBUTE_NAME || osName == UNIT_ATTRIBUTE_NAME ||
           osName == DIM_TYPE_ATTRIBUTE_NAME ||
           osName == DIM_DIRECTION_ATTRIBUTE_NAME ||
           osName == GDAL_ATTRIBUTE_NAME;
}

/************************************************************************/
/*                TileDBAttributeHolder::GetAttributesImpl()            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
TileDBAttributeHolder::GetAttributesImpl(CSLConstList papszOptions) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return {};

    const bool bShowAll =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));

    try
    {
        std::vector<std::shared_ptr<GDALAttribute>> apoAttributes;
        const uint64_t nAttributes = metadata_num();
        auto poSelf = AsAttributeHolderSharedPtr();
        for (uint64_t i = 0; i < nAttributes; ++i)
        {
            std::string key;
            tiledb_datatype_t value_type;
            uint32_t value_num;
            const void *value;
            get_metadata_from_index(i, &key, &value_type, &value_num, &value);
            if (bShowAll || !IsSpecialAttribute(key))
            {
                auto poAttr =
                    CreateAttribute(poSelf, key, value_type, value_num, value);
                if (poAttr)
                {
                    apoAttributes.emplace_back(std::move(poAttr));
                    m_oMapAttributes[key] = apoAttributes.back();
                }
            }
        }
        return apoAttributes;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetAttributes() failed with: %s",
                 e.what());
        return {};
    }
}

/************************************************************************/
/*             TileDBAttributeHolder::DeleteAttributeImpl()             */
/************************************************************************/

bool TileDBAttributeHolder::DeleteAttributeImpl(const std::string &osName,
                                                CSLConstList /*papszOptions*/)
{
    if (!IIsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;

    auto oIter = m_oMapAttributes.find(osName);

    try
    {
        delete_metadata(osName);

        if (oIter != m_oMapAttributes.end())
        {
            oIter->second->Deleted();
            m_oMapAttributes.erase(oIter);
        }
        return true;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DeleteAttribute() failed with: %s", e.what());
        return false;
    }
}

/************************************************************************/
/*                TileDBAttributeHolder::GetMetadata()                  */
/************************************************************************/

bool TileDBAttributeHolder::GetMetadata(const std::string &key,
                                        tiledb_datatype_t *value_type,
                                        uint32_t *value_num,
                                        const void **value) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return false;
    try
    {
        get_metadata(key, value_type, value_num, value);
        return *value != nullptr;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetMetadata() failed with: %s",
                 e.what());
        return false;
    }
}

/************************************************************************/
/*                TileDBAttributeHolder::PutMetadata()                  */
/************************************************************************/

bool TileDBAttributeHolder::PutMetadata(const std::string &key,
                                        tiledb_datatype_t value_type,
                                        uint32_t value_num, const void *value)
{
    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;
    try
    {
        put_metadata(key, value_type, value_num, value);
        return true;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PutMetadata() failed with: %s",
                 e.what());
        return false;
    }
}
