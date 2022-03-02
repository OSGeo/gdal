/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaUtils class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
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

#include "ogrhanautils.h"

#include <algorithm>

CPL_CVSID("$Id$")

namespace OGRHANA {

const char* SkipLeadingSpaces(const char* value)
{
    while (*value == ' ')
        value++;
    return value;
}

CPLString JoinStrings(
    const std::vector<CPLString>& strs,
    const char* delimiter,
    CPLString (*decorator)(const CPLString& str))
{
    CPLString ret;
    for (std::size_t i = 0; i < strs.size(); ++i)
    {
        ret += ((decorator != nullptr) ? decorator(strs[i]) : strs[i]);
        if (i != strs.size() - 1)
            ret += delimiter;
    }
    return ret;
}

std::vector<CPLString> SplitStrings(const char* str, const char* delimiter)
{
    std::vector<CPLString> ret;
    if (str != nullptr)
    {
        char** items = CSLTokenizeString2(str, delimiter, CSLT_HONOURSTRINGS);
        for (int i = 0; items[i] != nullptr; ++i)
        {
            CPLString item(items[i]);
            ret.push_back(item.Trim());
        }

        CSLDestroy(items);
    }

    return ret;
}

CPLString GetFullTableName(
    const CPLString& schemaName, const CPLString& tableName)
{
    if (schemaName.empty())
        return tableName;
    return schemaName + "." + tableName;
}

CPLString GetFullTableNameQuoted(
    const CPLString& schemaName, const CPLString& tableName)
{
    if (schemaName.empty())
        return QuotedIdentifier(tableName);
    return QuotedIdentifier(schemaName) + "." + QuotedIdentifier(tableName);
}

CPLString GetFullColumnNameQuoted(
    const CPLString& schemaName,
    const CPLString& tableName,
    const CPLString& columnName)
{
    return GetFullTableNameQuoted(schemaName, tableName) + "."
           + QuotedIdentifier(columnName);
}

CPLString LaunderName(const char* name)
{
    if (name == nullptr)
        return nullptr;

    CPLString newName(name);
    for (std::size_t i = 0; newName[i] != '\0'; ++i)
    {
        char c = static_cast<char>(toupper(newName[i]));
        if (c == '-' || c == '#')
            c = '_';
        newName[i] = c;
    }
    return newName;
}

CPLString Literal(const CPLString& value)
{
    CPLString ret("'");
    char* tmp = CPLEscapeString(value, -1, CPLES_SQL);
    ret += tmp;
    CPLFree(tmp);
    ret += "'";
    return ret;
}

CPLString QuotedIdentifier(const CPLString& value)
{
    return "\"" + value + "\"";
}

bool IsArrayField(OGRFieldType fieldType)
{
    return (
        fieldType == OFTIntegerList || fieldType == OFTInteger64List
        || fieldType == OFTRealList || fieldType == OFTStringList  || fieldType == OFTWideStringList);
}

bool IsGeometryTypeSupported(OGRwkbGeometryType wkbType)
{
    switch (wkbFlatten(wkbType))
    {
    case OGRwkbGeometryType::wkbPoint:
    case OGRwkbGeometryType::wkbLineString:
    case OGRwkbGeometryType::wkbPolygon:
    case OGRwkbGeometryType::wkbMultiPoint:
    case OGRwkbGeometryType::wkbMultiLineString:
    case OGRwkbGeometryType::wkbMultiPolygon:
    case OGRwkbGeometryType::wkbCircularString:
    case OGRwkbGeometryType::wkbGeometryCollection:
        return true;
    default:
        return false;
    }
}

OGRwkbGeometryType ToWkbType(const char* type, bool hasZ, bool hasM)
{
    if (strcmp(type, "ST_POINT") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbPoint, hasZ, hasM);
    else if (strcmp(type, "ST_MULTIPOINT") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiPoint, hasZ, hasM);
    else if (strcmp(type, "ST_LINESTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbLineString, hasZ, hasM);
    else if (strcmp(type, "ST_MULTILINESTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiLineString, hasZ, hasM);
    else if (strcmp(type, "ST_POLYGON") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbPolygon, hasZ, hasM);
    else if (strcmp(type, "ST_MULTIPOLYGON") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiPolygon, hasZ, hasM);
    else if (strcmp(type, "ST_CIRCULARSTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbCircularString, hasZ, hasM);
    else if (strcmp(type, "ST_GEOMETRYCOLLECTION") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbGeometryCollection, hasZ, hasM);
    return OGRwkbGeometryType::wkbUnknown;
}

constexpr int PLANAR_SRID_OFFSET = 1000000000;

int ToPlanarSRID(int srid)
{
    return srid < PLANAR_SRID_OFFSET ? PLANAR_SRID_OFFSET + srid : srid;
}

} // namespace hana_utils
