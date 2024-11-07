/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaUtils class declaration
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRHANAUTILS_H_INCLUDED
#define OGRHANAUTILS_H_INCLUDED

#include "ogr_core.h"
#include "ogr_feature.h"

#include "cpl_string.h"

#include <limits>
#include <vector>

namespace OGRHANA
{

constexpr const char *ARRAY_VALUES_DELIMITER = "^%^";

class HanaVersion
{

  public:
    explicit HanaVersion(unsigned int major, unsigned int minor,
                         unsigned int patch)
        : components_{major, minor, patch}
    {
    }

    HanaVersion() : components_{0, 0, 0}
    {
    }

    unsigned int major() const
    {
        return components_[0];
    }

    unsigned int minor() const
    {
        return components_[1];
    }

    unsigned int patch() const
    {
        return components_[2];
    }

    bool operator<=(const HanaVersion &other)
    {
        for (size_t i = 0; i < 3; ++i)
            if (components_[i] != other.components_[i])
                return components_[i] < other.components_[i];
        return true;
    }

    bool operator>=(const HanaVersion &other)
    {
        return !(*this <= other) || (*this == other);
    }

    bool operator==(const HanaVersion &other)
    {
        for (size_t i = 0; i < 3; ++i)
            if (components_[i] != other.components_[i])
                return false;
        return true;
    }

  public:
    static HanaVersion fromString(const char *str);

  private:
    unsigned int components_[3];
};

const char *SkipLeadingSpaces(const char *value);
CPLString JoinStrings(const std::vector<CPLString> &strs, const char *delimiter,
                      CPLString (*decorator)(const CPLString &str) = nullptr);
std::vector<CPLString> SplitStrings(const char *str, const char *delimiter);

CPLString GetFullColumnNameQuoted(const CPLString &schemaName,
                                  const CPLString &tableName,
                                  const CPLString &columnName);
CPLString GetFullTableName(const CPLString &schemaName,
                           const CPLString &tableName);
CPLString GetFullTableNameQuoted(const CPLString &schemaName,
                                 const CPLString &tableName);
CPLString Literal(const CPLString &value);
CPLString QuotedIdentifier(const CPLString &value);

bool IsArrayField(OGRFieldType fieldType);
bool IsGeometryTypeSupported(OGRwkbGeometryType wkbType);
OGRwkbGeometryType ToWkbType(const char *type, bool hasZ, bool hasM);
int ToPlanarSRID(int srid);

}  // namespace OGRHANA

#endif /* ndef OGRHANAUTILS_H_INCLUDED */
