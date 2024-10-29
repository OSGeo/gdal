/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaUtils class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrhanautils.h"
#include "cpl_string.h"

#include <algorithm>

namespace OGRHANA
{

HanaVersion HanaVersion::fromString(const char *version)
{
    CPLString splVersion(version);
    splVersion.replaceAll('-', '.').replaceAll(' ', '.');

    const CPLStringList parts(CSLTokenizeString2(splVersion, ".", 0));
    if (parts.size() < 3)
        return HanaVersion(0, 0, 0);

    return HanaVersion(atoi(parts[0]), atoi(parts[1]), atoi(parts[2]));
}

const char *SkipLeadingSpaces(const char *value)
{
    while (*value == ' ')
        value++;
    return value;
}

CPLString JoinStrings(const std::vector<CPLString> &strs, const char *delimiter,
                      CPLString (*decorator)(const CPLString &str))
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

std::vector<CPLString> SplitStrings(const char *str, const char *delimiter)
{
    std::vector<CPLString> ret;
    if (str != nullptr)
    {
        char **items = CSLTokenizeString2(str, delimiter, CSLT_HONOURSTRINGS);
        for (int i = 0; items[i] != nullptr; ++i)
        {
            CPLString item(items[i]);
            ret.push_back(item.Trim());
        }

        CSLDestroy(items);
    }

    return ret;
}

CPLString GetFullTableName(const CPLString &schemaName,
                           const CPLString &tableName)
{
    if (schemaName.empty())
        return tableName;
    return schemaName + "." + tableName;
}

CPLString GetFullTableNameQuoted(const CPLString &schemaName,
                                 const CPLString &tableName)
{
    if (schemaName.empty())
        return QuotedIdentifier(tableName);
    return QuotedIdentifier(schemaName) + "." + QuotedIdentifier(tableName);
}

CPLString GetFullColumnNameQuoted(const CPLString &schemaName,
                                  const CPLString &tableName,
                                  const CPLString &columnName)
{
    return GetFullTableNameQuoted(schemaName, tableName) + "." +
           QuotedIdentifier(columnName);
}

CPLString Literal(const CPLString &value)
{
    CPLString ret("'");
    char *tmp = CPLEscapeString(value, -1, CPLES_SQL);
    ret += tmp;
    CPLFree(tmp);
    ret += "'";
    return ret;
}

CPLString QuotedIdentifier(const CPLString &value)
{
    return "\"" + value + "\"";
}

bool IsArrayField(OGRFieldType fieldType)
{
    return (fieldType == OFTIntegerList || fieldType == OFTInteger64List ||
            fieldType == OFTRealList || fieldType == OFTStringList ||
            fieldType == OFTWideStringList);
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

OGRwkbGeometryType ToWkbType(const char *type, bool hasZ, bool hasM)
{
    if (strcmp(type, "ST_POINT") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbPoint, hasZ, hasM);
    else if (strcmp(type, "ST_MULTIPOINT") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiPoint, hasZ,
                                  hasM);
    else if (strcmp(type, "ST_LINESTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbLineString, hasZ,
                                  hasM);
    else if (strcmp(type, "ST_MULTILINESTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiLineString, hasZ,
                                  hasM);
    else if (strcmp(type, "ST_POLYGON") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbPolygon, hasZ, hasM);
    else if (strcmp(type, "ST_MULTIPOLYGON") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbMultiPolygon, hasZ,
                                  hasM);
    else if (strcmp(type, "ST_CIRCULARSTRING") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbCircularString, hasZ,
                                  hasM);
    else if (strcmp(type, "ST_GEOMETRYCOLLECTION") == 0)
        return OGR_GT_SetModifier(OGRwkbGeometryType::wkbGeometryCollection,
                                  hasZ, hasM);
    return OGRwkbGeometryType::wkbUnknown;
}

constexpr int PLANAR_SRID_OFFSET = 1000000000;

int ToPlanarSRID(int srid)
{
    return srid < PLANAR_SRID_OFFSET ? PLANAR_SRID_OFFSET + srid : srid;
}

}  // namespace OGRHANA
