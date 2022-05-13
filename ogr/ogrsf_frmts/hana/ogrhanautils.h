/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaUtils class declaration
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

#ifndef OGRHANAUTILS_H_INCLUDED
#define OGRHANAUTILS_H_INCLUDED

#include "ogr_core.h"
#include "ogr_feature.h"

#include "cpl_string.h"

#include <limits>
#include <vector>

namespace OGRHANA {

constexpr const char* ARRAY_VALUES_DELIMITER = "^%^";

const char* SkipLeadingSpaces(const char* value);
CPLString JoinStrings(
    const std::vector<CPLString>& strs,
    const char* delimiter,
    CPLString (*decorator)(const CPLString& str) = nullptr);
std::vector<CPLString> SplitStrings(const char* str, const char* delimiter);

CPLString GetFullColumnNameQuoted(
    const CPLString& schemaName,
    const CPLString& tableName,
    const CPLString& columnName);
CPLString GetFullTableName(
    const CPLString& schemaName, const CPLString& tableName);
CPLString GetFullTableNameQuoted(
    const CPLString& schemaName, const CPLString& tableName);
CPLString LaunderName(const char* name);
CPLString Literal(const CPLString& value);
CPLString QuotedIdentifier(const CPLString& value);

bool IsArrayField(OGRFieldType fieldType);
bool IsGeometryTypeSupported(OGRwkbGeometryType wkbType);
OGRwkbGeometryType ToWkbType(const char* type, bool hasZ, bool hasM);
int ToPlanarSRID(int srid);

} /* end of OGRHANA namespace */

#endif /* ndef OGRHANAUTILS_H_INCLUDED */
