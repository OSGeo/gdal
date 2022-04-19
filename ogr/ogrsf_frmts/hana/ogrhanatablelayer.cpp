/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaTableLayer class implementation
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

#include "ogr_hana.h"
#include "ogrhanafeaturereader.h"
#include "ogrhanautils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <regex>

#include "odbc/Exception.h"
#include "odbc/ResultSet.h"
#include "odbc/PreparedStatement.h"
#include "odbc/Types.h"

CPL_CVSID("$Id$")

namespace OGRHANA {
namespace {

constexpr const char* UNSUPPORTED_OP_READ_ONLY =
    "%s : unsupported operation on a read-only datasource.";

const char* GetColumnDefaultValue(const OGRFieldDefn& field)
{
    const char* defaultValue = field.GetDefault();
    if (field.GetType() == OFTInteger && field.GetSubType() == OFSTBoolean)
        return (EQUAL(defaultValue, "1") || EQUAL(defaultValue, "'t'"))
                   ? "TRUE"
                   : "FALSE";
    return defaultValue;
}

CPLString FindGeomFieldName(const OGRFeatureDefn& featureDefn)
{
    if (featureDefn.GetGeomFieldCount() == 0)
        return "OGR_GEOMETRY";

    int numGeomFields = featureDefn.GetGeomFieldCount();
    for (int i = 1; i <= 2 * numGeomFields; ++i)
    {
        CPLString name = CPLSPrintf("OGR_GEOMETRY_%d", i);
        if (featureDefn.GetGeomFieldIndex(name) < 0)
            return name;
    }

    return "OGR_GEOMETRY";
}

CPLString GetParameterValue(short type, const CPLString& typeName, bool isArray)
{
    if (isArray)
    {
        CPLString arrayType = "STRING";
        switch (type)
        {
        case odbc::SQLDataTypes::TinyInt:
            arrayType = "TINYINT";
            break;
        case odbc::SQLDataTypes::SmallInt:
            arrayType = "SMALLINT";
            break;
        case odbc::SQLDataTypes::Integer:
            arrayType = "INT";
            break;
        case odbc::SQLDataTypes::BigInt:
            arrayType = "BIGINT";
            break;
        case odbc::SQLDataTypes::Float:
        case odbc::SQLDataTypes::Real:
            arrayType = "REAL";
            break;
        case odbc::SQLDataTypes::Double:
            arrayType = "DOUBLE";
            break;
        case odbc::SQLDataTypes::WVarChar:
            arrayType = "STRING";
            break;
        }
        return "ARRAY(SELECT * FROM OGR_PARSE_" + arrayType + "_ARRAY(?, '"
               + ARRAY_VALUES_DELIMITER + "'))";
    }
    else if (typeName.compare("NCLOB") == 0)
        return "TO_NCLOB(?)";
    else if (typeName.compare("CLOB") == 0)
        return "TO_CLOB(?)";
    else if (typeName.compare("BLOB") == 0)
        return "TO_BLOB(?)";
    else
        return "?";
}

std::vector<int> ParseIntValues(const char* str)
{
    std::vector<int> values;
    std::stringstream stream(str);
    while (stream.good())
    {
        std::string value;
        getline(stream, value, ',');
        values.push_back(std::atoi(value.c_str()));
    }
    return values;
}

ColumnTypeInfo ParseColumnTypeInfo(const CPLString& typeDef)
{
    auto incorrectFormatErr = [&]() {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Column type '%s' has incorrect format.", typeDef.c_str());
    };

    CPLString typeName;
    std::vector<int> typeSize;

    if (std::strstr(typeDef, "(") == nullptr)
    {
        typeName = typeDef;
    }
    else
    {
        const auto regex = std::regex(R"((\w+)+\((\d+(,\d+)*)\)$)");
        std::smatch match;
        std::regex_search(typeDef, match, regex);

        if (match.size() != 0)
        {
            typeName.assign(match[1]);
            typeSize = ParseIntValues(match[2].str().c_str());
        }

        if (typeSize.empty() || typeSize.size() > 2)
        {
            incorrectFormatErr();
            return {"", odbc::SQLDataTypes::Unknown, 0, 0};
        }
    }

    if (EQUAL(typeName.c_str(), "BOOLEAN"))
        return {typeName, odbc::SQLDataTypes::Boolean, 0, 0};
    else if (EQUAL(typeName.c_str(), "TINYINT"))
        return {typeName, odbc::SQLDataTypes::TinyInt, 0, 0};
    else if (EQUAL(typeName.c_str(), "SMALLINT"))
        return {typeName, odbc::SQLDataTypes::SmallInt, 0, 0};
    else if (EQUAL(typeName.c_str(), "INTEGER"))
        return {typeName, odbc::SQLDataTypes::Integer, 0, 0};
    else if (EQUAL(typeName.c_str(), "DECIMAL"))
    {
        switch (typeSize.size())
        {
        case 0:
            return {typeName, odbc::SQLDataTypes::Decimal, 0, 0};
        case 1:
            return {typeName, odbc::SQLDataTypes::Decimal, typeSize[0], 0};
        case 2:
            return {typeName, odbc::SQLDataTypes::Decimal, typeSize[0],
                    typeSize[1]};
        }
    }
    else if (EQUAL(typeName.c_str(), "FLOAT"))
    {
        switch (typeSize.size())
        {
        case 0:
            return {typeName, odbc::SQLDataTypes::Float, 10, 0};
        case 1:
            return {typeName, odbc::SQLDataTypes::Float, typeSize[0], 0};
        default:
            incorrectFormatErr();
            return {"", odbc::SQLDataTypes::Unknown, 0, 0};
        }
    }
    else if (EQUAL(typeName.c_str(), "REAL"))
        return {typeName, odbc::SQLDataTypes::Real, 0, 0};
    else if (EQUAL(typeName.c_str(), "DOUBLE"))
        return {typeName, odbc::SQLDataTypes::Double, 0, 0};
    else if (EQUAL(typeName.c_str(), "VARCHAR"))
    {
        switch (typeSize.size())
        {
        case 0:
            return {typeName, odbc::SQLDataTypes::VarChar, 1, 0};
        case 1:
            return {typeName, odbc::SQLDataTypes::VarChar, typeSize[0], 0};
        default:
            incorrectFormatErr();
            return {"", odbc::SQLDataTypes::Unknown, 0, 0};
        }
    }
    else if (EQUAL(typeName.c_str(), "NVARCHAR"))
    {
        switch (typeSize.size())
        {
        case 0:
            return {typeName, odbc::SQLDataTypes::WVarChar, 1, 0};
        case 1:
            return {typeName, odbc::SQLDataTypes::WVarChar, typeSize[0], 0};
        case 2:
            incorrectFormatErr();
            return {"", odbc::SQLDataTypes::Unknown, 0, 0};
        }
    }
    else if (EQUAL(typeName.c_str(), "NCLOB"))
        return {typeName, odbc::SQLDataTypes::WLongVarChar, 0, 0};
    else if (EQUAL(typeName.c_str(), "DATE"))
        return {typeName, odbc::SQLDataTypes::Date, 0, 0};
    else if (EQUAL(typeName.c_str(), "TIME"))
        return {typeName, odbc::SQLDataTypes::Time, 0, 0};
    else if (EQUAL(typeName.c_str(), "TIMESTAMP"))
        return {typeName, odbc::SQLDataTypes::Timestamp, 0, 0};
    else if (EQUAL(typeName.c_str(), "VARBINARY"))
    {
        switch (typeSize.size())
        {
        case 0:
            return {typeName, odbc::SQLDataTypes::VarBinary, 1, 0};
        case 1:
            return {typeName, odbc::SQLDataTypes::VarBinary, typeSize[0], 0};
        case 2:
            incorrectFormatErr();
            return {"", odbc::SQLDataTypes::Unknown, 0, 0};
        }
    }
    else if (EQUAL(typeName.c_str(), "BLOB"))
        return {typeName, odbc::SQLDataTypes::LongVarBinary, 0, 0};

    CPLError(
        CE_Failure, CPLE_NotSupported, "Unknown column type '%s'.",
        typeName.c_str());
    return {typeName, odbc::SQLDataTypes::Unknown, 0, 0};
}

CPLString GetColumnDefinition(const ColumnTypeInfo& typeInfo)
{
    bool isArray = std::strstr(typeInfo.name, "ARRAY") != nullptr;

    if (isArray)
    {
        switch (typeInfo.type)
        {
        case odbc::SQLDataTypes::SmallInt:
            return "SMALLINT ARRAY";
        case odbc::SQLDataTypes::Integer:
            return "INTEGER ARRAY";
        case odbc::SQLDataTypes::BigInt:
            return "BIGINT ARRAY";
        case odbc::SQLDataTypes::Real:
            return "REAL ARRAY";
        case odbc::SQLDataTypes::Double:
            return "DOUBLE ARRAY";
        case odbc::SQLDataTypes::WVarChar:
            return "NVARCHAR(512) ARRAY";
        default:
            return "UNKNOWN";
        }
    }

    switch (typeInfo.type)
    {
    case odbc::SQLDataTypes::Boolean:
    case odbc::SQLDataTypes::TinyInt:
    case odbc::SQLDataTypes::SmallInt:
    case odbc::SQLDataTypes::Integer:
    case odbc::SQLDataTypes::BigInt:
    case odbc::SQLDataTypes::Float:
    case odbc::SQLDataTypes::Real:
    case odbc::SQLDataTypes::Double:
    case odbc::SQLDataTypes::Date:
    case odbc::SQLDataTypes::TypeDate:
    case odbc::SQLDataTypes::Time:
    case odbc::SQLDataTypes::TypeTime:
    case odbc::SQLDataTypes::Timestamp:
    case odbc::SQLDataTypes::TypeTimestamp:
    case odbc::SQLDataTypes::Char:
    case odbc::SQLDataTypes::WChar:
    case odbc::SQLDataTypes::LongVarChar:
    case odbc::SQLDataTypes::LongVarBinary:
        return typeInfo.name;
    case odbc::SQLDataTypes::Decimal:
    case odbc::SQLDataTypes::Numeric:
        return CPLString().Printf("DECIMAL(%d,%d)", typeInfo.width, typeInfo.precision);
    case odbc::SQLDataTypes::VarChar:
    case odbc::SQLDataTypes::WVarChar:
    case odbc::SQLDataTypes::Binary:
    case odbc::SQLDataTypes::VarBinary:
    case odbc::SQLDataTypes::WLongVarChar:
        return (typeInfo.width == 0)
                ? typeInfo.name
                : CPLString().Printf("%s(%d)", typeInfo.name.c_str(), typeInfo.width);
    default:
        return "UNKNOWN";
    }
}

void SetFieldDefn(OGRFieldDefn& field, const ColumnTypeInfo& typeInfo)
{
    auto isArray = [&typeInfo]() {
        return std::strstr(typeInfo.name, "ARRAY") != nullptr;
    };

    switch (typeInfo.type)
    {
    case odbc::SQLDataTypes::Bit:
    case odbc::SQLDataTypes::Boolean:
        field.SetType(OFTInteger);
        field.SetSubType(OFSTBoolean);
        break;
    case odbc::SQLDataTypes::TinyInt:
    case odbc::SQLDataTypes::SmallInt:
        field.SetType(isArray() ? OFTIntegerList : OFTInteger);
        field.SetSubType(OFSTInt16);
        break;
    case odbc::SQLDataTypes::Integer:
        field.SetType(isArray() ? OFTIntegerList : OFTInteger);
        break;
    case odbc::SQLDataTypes::BigInt:
        field.SetType(isArray() ? OFTInteger64List : OFTInteger64);
        break;
    case odbc::SQLDataTypes::Double:
    case odbc::SQLDataTypes::Real:
    case odbc::SQLDataTypes::Float:
        field.SetType(isArray() ? OFTRealList : OFTReal);
        if (typeInfo.type != odbc::SQLDataTypes::Double)
            field.SetSubType(OFSTFloat32);
        break;
    case odbc::SQLDataTypes::Decimal:
    case odbc::SQLDataTypes::Numeric:
        field.SetType(isArray() ? OFTRealList : OFTReal);
        break;
    case odbc::SQLDataTypes::Char:
    case odbc::SQLDataTypes::VarChar:
    case odbc::SQLDataTypes::LongVarChar:
        field.SetType(isArray() ? OFTStringList : OFTString);
        break;
    case odbc::SQLDataTypes::WChar:
    case odbc::SQLDataTypes::WVarChar:
    case odbc::SQLDataTypes::WLongVarChar:
        field.SetType(isArray() ? OFTStringList : OFTString);
        break;
    case odbc::SQLDataTypes::Date:
    case odbc::SQLDataTypes::TypeDate:
        field.SetType(OFTDate);
        break;
    case odbc::SQLDataTypes::Time:
    case odbc::SQLDataTypes::TypeTime:
        field.SetType(OFTTime);
        break;
    case odbc::SQLDataTypes::Timestamp:
    case odbc::SQLDataTypes::TypeTimestamp:
        field.SetType(OFTDateTime);
        break;
    case odbc::SQLDataTypes::Binary:
    case odbc::SQLDataTypes::VarBinary:
    case odbc::SQLDataTypes::LongVarBinary:
        field.SetType(OFTBinary);
        break;
    default:
        break;
    }

    field.SetWidth(typeInfo.width);
    field.SetPrecision(typeInfo.precision);
}

} // anonymous namespace

/************************************************************************/
/*                         OGRHanaTableLayer()                          */
/************************************************************************/

OGRHanaTableLayer::OGRHanaTableLayer(
    OGRHanaDataSource* datasource,
    const char* schemaName,
    const char* tableName,
    int update)
    : OGRHanaLayer(datasource)
    , schemaName_(schemaName)
    , tableName_(tableName)
    , updateMode_(update)
{
    rawQuery_ =
        "SELECT * FROM " + GetFullTableNameQuoted(schemaName_, tableName_);
    SetDescription(tableName_.c_str());
}

/************************************************************************/
/*                        ~OGRHanaTableLayer()                          */
/************************************************************************/

OGRHanaTableLayer::~OGRHanaTableLayer()
{
    FlushPendingFeatures();
}

/* -------------------------------------------------------------------- */
/*                             Initialize()                             */
/* -------------------------------------------------------------------- */

OGRErr OGRHanaTableLayer::Initialize()
{
    if (initialized_)
        return OGRERR_NONE;

    OGRErr err = InitFeatureDefinition(
        schemaName_, tableName_, rawQuery_, tableName_);
    if (err != OGRERR_NONE)
        return err;

    if (fidFieldIndex_ != OGRNullFID)
        CPLDebug(
            "HANA", "table %s has FID column %s.", tableName_.c_str(),
            fidFieldName_.c_str());
    else
        CPLDebug(
            "HANA", "table %s has no FID column, FIDs will not be reliable!",
            tableName_.c_str());

    return OGRERR_NONE;
}

/* -------------------------------------------------------------------- */
/*                                 ExecuteUpdate()                      */
/* -------------------------------------------------------------------- */

std::pair<OGRErr, std::size_t> OGRHanaTableLayer::ExecuteUpdate(
    odbc::PreparedStatement& statement, bool withBatch,  const char* functionName)
{
    std::size_t ret = 0;

    try
    {
        if (withBatch)
        {
            if (statement.getBatchDataSize() >= batchSize_)
                statement.executeBatch();
            ret = 1;
        }
        else
        {
            ret = statement.executeUpdate();
        }

        if (!dataSource_->IsTransactionStarted())
            dataSource_->Commit();
    }
    catch (odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to execute %s: %s",
            functionName, ex.what());
        return {OGRERR_FAILURE, 0};
    }

    return {OGRERR_NONE, ret};
}

/* -------------------------------------------------------------------- */
/*                   CreateDeleteFeatureStatement()                     */
/* -------------------------------------------------------------------- */

odbc::PreparedStatementRef OGRHanaTableLayer::CreateDeleteFeatureStatement()
{
    CPLString sql = CPLString().Printf(
        "DELETE FROM %s WHERE %s = ?",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        QuotedIdentifier(GetFIDColumn()).c_str());
    return dataSource_->PrepareStatement(sql.c_str());
}

/* -------------------------------------------------------------------- */
/*                   CreateInsertFeatureStatement()                     */
/* -------------------------------------------------------------------- */

odbc::PreparedStatementRef OGRHanaTableLayer::CreateInsertFeatureStatement(bool withFID)
{
    std::vector<CPLString> columns;
    std::vector<CPLString> values;
    bool hasArray = false;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID && !withFID)
        {
            if (clmDesc.isAutoIncrement)
                continue;
        }

        columns.push_back(QuotedIdentifier(clmDesc.name));
        values.push_back(
            GetParameterValue(clmDesc.type, clmDesc.typeName, clmDesc.isArray));
        if (clmDesc.isArray)
            hasArray = true;
    }

    for (const GeometryColumnDescription& geomClmDesc : geomColumns_)
    {
        columns.push_back(QuotedIdentifier(geomClmDesc.name));
        values.push_back(
            "ST_GeomFromWKB(? , " + std::to_string(geomClmDesc.srid) + ")");
    }

    if (hasArray && !parseFunctionsChecked_)
    {
        // Create helper functions if needed.
        if (!dataSource_->ParseArrayFunctionsExist(schemaName_.c_str()))
            dataSource_->CreateParseArrayFunctions(schemaName_.c_str());
        parseFunctionsChecked_ = true;
    }

    const CPLString sql = CPLString().Printf(
        "INSERT INTO %s (%s) VALUES(%s)",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        JoinStrings(columns, ", ").c_str(), JoinStrings(values, ", ").c_str());

    return dataSource_->PrepareStatement(sql.c_str());
}

/* -------------------------------------------------------------------- */
/*                     CreateUpdateFeatureStatement()                   */
/* -------------------------------------------------------------------- */

odbc::PreparedStatementRef OGRHanaTableLayer::CreateUpdateFeatureStatement()
{
    std::vector<CPLString> values;
    values.reserve(attrColumns_.size());
    bool hasArray = false;

    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID)
        {
            if (clmDesc.isAutoIncrement)
                continue;
        }
        values.push_back(
            QuotedIdentifier(clmDesc.name) + " = "
            + GetParameterValue(
                clmDesc.type, clmDesc.typeName, clmDesc.isArray));
        if (clmDesc.isArray)
            hasArray = true;
    }

    for (const GeometryColumnDescription& geomClmDesc : geomColumns_)
    {
        values.push_back(
            QuotedIdentifier(geomClmDesc.name) + " = " + "ST_GeomFromWKB(?, "
            + std::to_string(geomClmDesc.srid) + ")");
    }

    if (hasArray && !parseFunctionsChecked_)
    {
        // Create helper functions if needed.
        if (!dataSource_->ParseArrayFunctionsExist(schemaName_.c_str()))
            dataSource_->CreateParseArrayFunctions(schemaName_.c_str());
        parseFunctionsChecked_ = true;
    }

    const CPLString sql = CPLString().Printf(
        "UPDATE %s SET %s WHERE %s = ?",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        JoinStrings(values, ", ").c_str(),
        QuotedIdentifier(GetFIDColumn()).c_str());

    return dataSource_->PrepareStatement(sql.c_str());
}

/* -------------------------------------------------------------------- */
/*                     ResetPreparedStatements()                        */
/* -------------------------------------------------------------------- */

void OGRHanaTableLayer::ResetPreparedStatements()
{
    if (!currentIdentityValueStmt_.isNull())
        currentIdentityValueStmt_ = nullptr;
    if (!insertFeatureStmtWithFID_.isNull())
        insertFeatureStmtWithFID_ = nullptr;
    if (!insertFeatureStmtWithoutFID_.isNull())
        insertFeatureStmtWithoutFID_ = nullptr;
    if (!deleteFeatureStmt_.isNull())
        deleteFeatureStmt_ = nullptr;
    if (!updateFeatureStmt_.isNull())
        updateFeatureStmt_ = nullptr;
}

/************************************************************************/
/*                        SetStatementParameters()                      */
/************************************************************************/

OGRErr OGRHanaTableLayer::SetStatementParameters(
    odbc::PreparedStatement& statement,
    OGRFeature* feature,
    bool newFeature,
    bool withFID,
    const char* functionName)
{
    OGRHanaFeatureReader featReader(*feature);

    unsigned short paramIndex = 0;
    int fieldIndex = -1;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID)
        {
            if (!withFID && clmDesc.isAutoIncrement)
                continue;

            ++paramIndex;

            switch (clmDesc.type)
            {
            case odbc::SQLDataTypes::Integer:
                if (feature->GetFID() == OGRNullFID)
                    statement.setInt(paramIndex, odbc::Int());
                else
                {
                    if (std::numeric_limits<std::int32_t>::min() > feature->GetFID() ||
                        std::numeric_limits<std::int32_t>::max() < feature->GetFID())
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "%s: Feature id with value %s cannot "
                            "be stored in a column of type INTEGER",
                            functionName,
                            std::to_string(feature->GetFID()).c_str());
                        return OGRERR_FAILURE;
                    }

                    statement.setInt(
                        paramIndex,
                        odbc::Int(static_cast<std::int32_t>(feature->GetFID())));
                }
                break;
            case odbc::SQLDataTypes::BigInt:
                if (feature->GetFID() == OGRNullFID)
                    statement.setLong(paramIndex, odbc::Long());
                else
                    statement.setLong(
                        paramIndex,
                        odbc::Long(static_cast<std::int64_t>(feature->GetFID())));
                break;
            default:
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "%s: Unexpected type ('%s') in the field "
                    "'%s'",
                    functionName, std::to_string(clmDesc.type).c_str(),
                    clmDesc.name.c_str());
                return OGRERR_FAILURE;
            }
            continue;
        }
        else
            ++paramIndex;

        ++fieldIndex;

        switch (clmDesc.type)
        {
        case odbc::SQLDataTypes::Bit:
        case odbc::SQLDataTypes::Boolean:
            statement.setBoolean(
                paramIndex, featReader.GetFieldAsBoolean(fieldIndex));
            break;
        case odbc::SQLDataTypes::TinyInt:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                statement.setByte(
                    paramIndex, featReader.GetFieldAsByte(fieldIndex));
            break;
        case odbc::SQLDataTypes::SmallInt:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                statement.setShort(
                    paramIndex, featReader.GetFieldAsShort(fieldIndex));
            break;
        case odbc::SQLDataTypes::Integer:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                statement.setInt(
                    paramIndex, featReader.GetFieldAsInt(fieldIndex));
            break;
        case odbc::SQLDataTypes::BigInt:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsBigIntArray(fieldIndex));
            else
                statement.setLong(
                    paramIndex, featReader.GetFieldAsLong(fieldIndex));
            break;
        case odbc::SQLDataTypes::Float:
        case odbc::SQLDataTypes::Real:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsRealArray(fieldIndex));
            else
                statement.setFloat(
                    paramIndex, featReader.GetFieldAsFloat(fieldIndex));
            break;
        case odbc::SQLDataTypes::Double:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsDoubleArray(fieldIndex));
            else
                statement.setDouble(
                    paramIndex, featReader.GetFieldAsDouble(fieldIndex));
            break;
        case odbc::SQLDataTypes::Decimal:
        case odbc::SQLDataTypes::Numeric:
            if ((!feature->IsFieldSet(fieldIndex)
                 || feature->IsFieldNull(fieldIndex))
                && feature->GetFieldDefnRef(fieldIndex)->GetDefault()
                       == nullptr)
                statement.setDecimal(paramIndex, odbc::Decimal());
            else
                statement.setDouble(
                    paramIndex, featReader.GetFieldAsDouble(fieldIndex));
            break;
        case odbc::SQLDataTypes::Char:
        case odbc::SQLDataTypes::VarChar:
        case odbc::SQLDataTypes::LongVarChar:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsStringArray(fieldIndex));
            else
                statement.setString(
                    paramIndex,
                    featReader.GetFieldAsString(fieldIndex, clmDesc.length));
            break;
        case odbc::SQLDataTypes::WChar:
        case odbc::SQLDataTypes::WVarChar:
        case odbc::SQLDataTypes::WLongVarChar:
            if (clmDesc.isArray)
                statement.setString(
                    paramIndex, featReader.GetFieldAsStringArray(fieldIndex));
            else
                statement.setString(
                    paramIndex,
                    featReader.GetFieldAsNString(fieldIndex, clmDesc.length));
            break;
        case odbc::SQLDataTypes::Binary:
        case odbc::SQLDataTypes::VarBinary:
        case odbc::SQLDataTypes::LongVarBinary: {
            Binary bin = featReader.GetFieldAsBinary(fieldIndex);
            statement.setBytes(paramIndex, bin.data, bin.size);
        }
        break;
        case odbc::SQLDataTypes::DateTime:
        case odbc::SQLDataTypes::TypeDate:
            statement.setDate(
                paramIndex, featReader.GetFieldAsDate(fieldIndex));
            break;
        case odbc::SQLDataTypes::Time:
        case odbc::SQLDataTypes::TypeTime:
            statement.setTime(
                paramIndex, featReader.GetFieldAsTime(fieldIndex));
            break;
        case odbc::SQLDataTypes::Timestamp:
        case odbc::SQLDataTypes::TypeTimestamp:
            statement.setTimestamp(
                paramIndex, featReader.GetFieldAsTimestamp(fieldIndex));
            break;
        }
    }

    for (std::size_t i = 0; i < geomColumns_.size(); ++i)
    {
        ++paramIndex;
        Binary wkb{nullptr, 0};
        OGRErr err = GetGeometryWkb(feature, static_cast<int>(i), wkb);
        if (OGRERR_NONE != err)
            return err;
        statement.setBytes(paramIndex, wkb.data, wkb.size);
    }

    if (!newFeature)
    {
        ++paramIndex;

        statement.setLong(
            paramIndex,
            odbc::Long(static_cast<std::int64_t>(feature->GetFID())));
    }

    return OGRERR_NONE;
}

/* -------------------------------------------------------------------- */
/*                            DropTable()                               */
/* -------------------------------------------------------------------- */

OGRErr OGRHanaTableLayer::DropTable()
{
    CPLString sql =
        "DROP TABLE " + GetFullTableNameQuoted(schemaName_, tableName_);
    try
    {
        dataSource_->ExecuteSQL(sql.c_str());
        CPLDebug("HANA", "Dropped table %s.", GetName());
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Unable to delete layer '%s': %s",
            tableName_.c_str(), ex.what());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/* -------------------------------------------------------------------- */
/*                        FlushPendingFeatures()                        */
/* -------------------------------------------------------------------- */

void OGRHanaTableLayer::FlushPendingFeatures()
{
    if (HasPendingFeatures())
        dataSource_->Commit();
}

/* -------------------------------------------------------------------- */
/*                        HasPendingFeatures()                          */
/* -------------------------------------------------------------------- */

bool OGRHanaTableLayer::HasPendingFeatures() const
{
    return (!deleteFeatureStmt_.isNull()
            && deleteFeatureStmt_->getBatchDataSize() > 0)
            || (!insertFeatureStmtWithFID_.isNull()
                && insertFeatureStmtWithFID_->getBatchDataSize() > 0)
           || (!insertFeatureStmtWithoutFID_.isNull()
               && insertFeatureStmtWithoutFID_->getBatchDataSize() > 0)
           || (!updateFeatureStmt_.isNull()
               && updateFeatureStmt_->getBatchDataSize() > 0);
}

/* -------------------------------------------------------------------- */
/*                            GetColumnTypeInfo()                       */
/* -------------------------------------------------------------------- */

ColumnTypeInfo OGRHanaTableLayer::GetColumnTypeInfo(const OGRFieldDefn& field) const
{
    for (const auto& clmType : customColumnDefs_)
    {
        if (EQUAL(clmType.name.c_str(), field.GetNameRef()))
            return ParseColumnTypeInfo(clmType.typeDef);
    }

    switch (field.GetType())
    {
    case OFTInteger:
        if (preservePrecision_ && field.GetWidth() > 10)
        {
            return {"DECIMAL", odbc::SQLDataTypes::Decimal, field.GetWidth(), 0};
        }
        else
        {
            if (field.GetSubType() == OFSTBoolean)
                return {"BOOLEAN", odbc::SQLDataTypes::Boolean,
                        field.GetWidth(), 0};
            else if (field.GetSubType() == OFSTInt16)
                return {"SMALLINT", odbc::SQLDataTypes::SmallInt,
                        field.GetWidth(), 0};
            else
                return {"INTEGER", odbc::SQLDataTypes::Integer,
                        field.GetWidth(), 0};
        }
        break;
    case OFTInteger64:
        if (preservePrecision_ && field.GetWidth() > 20)
        {
            return {"DECIMAL", odbc::SQLDataTypes::Decimal,
                    field.GetWidth(), 0};
        }
        else
            return {"BIGINT", odbc::SQLDataTypes::BigInt,
                    field.GetWidth(), 0};
        break;
    case OFTReal:
        if (preservePrecision_ && field.GetWidth() != 0)
        {
            return {"DECIMAL", odbc::SQLDataTypes::Decimal,
                    field.GetWidth(), field.GetPrecision()};
        }
        else
        {
            if (field.GetSubType() == OFSTFloat32)
                return {"REAL", odbc::SQLDataTypes::Real, field.GetWidth(),
                        field.GetPrecision()};
            else
                return {"DOUBLE", odbc::SQLDataTypes::Double, field.GetWidth(),
                        field.GetPrecision()};
        }
    case OFTString:
        if (field.GetWidth() == 0 || !preservePrecision_)
        {
            int width = static_cast<int>(defaultStringSize_);
            return {"NVARCHAR", odbc::SQLDataTypes::WLongVarChar, width, 0};
        }
        else
        {
            if (field.GetWidth() <= 5000)
                return {"NVARCHAR", odbc::SQLDataTypes::WLongVarChar,
                        field.GetWidth(), 0};
            else
                return {"NCLOB", odbc::SQLDataTypes::WLongVarChar,
                        0, 0};
        }
    case OFTBinary:
        if (field.GetWidth() <= 5000)
            return {"VARBINARY", odbc::SQLDataTypes::VarBinary,
                    field.GetWidth(), 0};
        else
            return {"BLOB", odbc::SQLDataTypes::LongVarBinary, field.GetWidth(),
                    0};
    case OFTDate:
        return {"DATE", odbc::SQLDataTypes::TypeDate, field.GetWidth(), 0};
    case OFTTime:
        return {"TIME", odbc::SQLDataTypes::TypeTime, field.GetWidth(), 0};
    case OFTDateTime:
        return {"TIMESTAMP", odbc::SQLDataTypes::TypeTimestamp,
                field.GetWidth(), 0};
    case OFTIntegerList:
        if (field.GetSubType() == OGRFieldSubType::OFSTInt16)
            return {"ARRAY", odbc::SQLDataTypes::SmallInt,
                    field.GetWidth(), 0};
        else
            return {"ARRAY", odbc::SQLDataTypes::Integer,
                    field.GetWidth(), 0};
    case OFTInteger64List:
        return {"ARRAY", odbc::SQLDataTypes::BigInt, field.GetWidth(),
                0};
    case OFTRealList:
        if (field.GetSubType() == OGRFieldSubType::OFSTFloat32)
            return {"ARRAY", odbc::SQLDataTypes::Real, field.GetWidth(),
                    0};
        else
            return {"ARRAY", odbc::SQLDataTypes::Double,
                    field.GetWidth(), 0};
        break;
    case OFTStringList:
        return {"ARRAY", odbc::SQLDataTypes::WVarChar, 512, 0};
    default:
        break;
    }

    return {"", odbc::SQLDataTypes::Unknown, 0, 0};
}

/* -------------------------------------------------------------------- */
/*                           GetGeometryWkb()                           */
/* -------------------------------------------------------------------- */
OGRErr OGRHanaTableLayer::GetGeometryWkb(
    OGRFeature* feature, int fieldIndex, Binary& binary)
{
    OGRGeometry* geom = feature->GetGeomFieldRef(fieldIndex);
    if (geom == nullptr || !IsGeometryTypeSupported(geom->getIsoGeometryType()))
        return OGRERR_NONE;

    // Rings must be closed, otherwise HANA throws an exception
    geom->closeRings();
    std::size_t size = static_cast<std::size_t>(geom->WkbSize());
    EnsureBufferCapacity(size);
    unsigned char* data = reinterpret_cast<unsigned char*>(dataBuffer_.data());
    OGRErr err = geom->exportToWkb(
        OGRwkbByteOrder::wkbNDR, data, OGRwkbVariant::wkbVariantIso);
    if (OGRERR_NONE == err)
    {
        binary.data = data;
        binary.size = size;
    }
    return err;
}

/************************************************************************/
/*                              ResetReading()                          */
/************************************************************************/

void OGRHanaTableLayer::ResetReading()
{
    FlushPendingFeatures();

    OGRHanaLayer::ResetReading();
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRHanaTableLayer::TestCapability(const char* capabilities)
{
    if (EQUAL(capabilities, OLCRandomRead))
    {
        EnsureInitialized();
        return fidFieldIndex_ != OGRNullFID;
    }
    if (EQUAL(capabilities, OLCFastFeatureCount))
        return TRUE;
    if (EQUAL(capabilities, OLCFastSpatialFilter))
    {
        EnsureInitialized();
        return !geomColumns_.empty();
    }
    if (EQUAL(capabilities, OLCFastGetExtent))
    {
        EnsureInitialized();
        return !geomColumns_.empty();
    }
    if (EQUAL(capabilities, OLCCreateField))
        return updateMode_;
    if (EQUAL(capabilities, OLCCreateGeomField)
        || EQUAL(capabilities, ODsCCreateGeomFieldAfterCreateLayer))
        return updateMode_;
    if (EQUAL(capabilities, OLCDeleteField))
        return updateMode_;
    if (EQUAL(capabilities, OLCDeleteFeature))
    {
        EnsureInitialized();
        return updateMode_ && fidFieldIndex_ != OGRNullFID;
    }
    if (EQUAL(capabilities, OLCAlterFieldDefn))
        return updateMode_;
    if (EQUAL(capabilities, OLCRandomWrite))
        return updateMode_;
    if (EQUAL(capabilities, OLCMeasuredGeometries))
        return TRUE;
    if (EQUAL(capabilities, OLCSequentialWrite))
        return updateMode_;
    if (EQUAL(capabilities, OLCTransactions))
        return updateMode_;
    if (EQUAL(capabilities, OLCStringsAsUTF8))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRHanaTableLayer::ICreateFeature(OGRFeature* feature)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( nullptr == feature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    GIntBig nFID = feature->GetFID();
    bool withFID = nFID != OGRNullFID;
    bool withBatch = withFID && dataSource_->IsTransactionStarted();

    try
    {
        odbc::PreparedStatementRef& stmt = withFID ? insertFeatureStmtWithFID_ : insertFeatureStmtWithoutFID_;

        if (stmt.isNull())
        {
            stmt = CreateInsertFeatureStatement(withFID);
            if (stmt.isNull())
                return OGRERR_FAILURE;
        }

        OGRErr err = SetStatementParameters(*stmt, feature, true, withFID, "CreateFeature");

        if (OGRERR_NONE != err)
            return err;

        if (withBatch)
            stmt->addBatch();

        auto ret = ExecuteUpdate(*stmt, withBatch, "CreateFeature");

        err = ret.first;
        if (OGRERR_NONE != err)
            return err;

        if (!withFID)
        {
            const CPLString sql = CPLString().Printf(
                "SELECT CURRENT_IDENTITY_VALUE() \"current identity value\" FROM %s",
                GetFullTableNameQuoted(schemaName_, tableName_).c_str());

            if (currentIdentityValueStmt_.isNull())
                currentIdentityValueStmt_ = dataSource_->PrepareStatement(sql.c_str());

            odbc::ResultSetRef rsIdentity = currentIdentityValueStmt_->executeQuery();
            if ( rsIdentity->next() )
            {
              odbc::Long id = rsIdentity->getLong( 1 );
              if ( !id.isNull() )
                feature->SetFID(static_cast<GIntBig>( *id ) );
            }
            rsIdentity->close();
        }

        return err;
    }
    catch (const std::exception& ex)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to create feature: %s", ex.what());
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRHanaTableLayer::DeleteFeature(GIntBig nFID)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if (nFID == OGRNullFID)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "DeleteFeature(" CPL_FRMT_GIB
            ") failed.  Unable to delete features "
            "in tables without\n a recognised FID column.",
            nFID);
        return OGRERR_FAILURE;
    }

    if (OGRNullFID == fidFieldIndex_)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "DeleteFeature(" CPL_FRMT_GIB
            ") failed.  Unable to delete features "
            "in tables without\n a recognised FID column.",
            nFID);
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    if (deleteFeatureStmt_.isNull())
    {
        deleteFeatureStmt_ = CreateDeleteFeatureStatement();
        if (deleteFeatureStmt_.isNull())
            return OGRERR_FAILURE;
    }

    deleteFeatureStmt_->setLong(1, odbc::Long(static_cast<std::int64_t>(nFID)));
    bool withBatch = dataSource_->IsTransactionStarted();
    if (withBatch)
       deleteFeatureStmt_->addBatch();

    auto ret = ExecuteUpdate(*deleteFeatureStmt_, withBatch, "DeleteFeature");
    return (OGRERR_NONE == ret.first && ret.second != 1)
               ? OGRERR_NON_EXISTING_FEATURE
               : ret.first;
}

/************************************************************************/
/*                             ISetFeature()                            */
/************************************************************************/

OGRErr OGRHanaTableLayer::ISetFeature(OGRFeature* feature)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "SetFeature");
        return OGRERR_FAILURE;
    }

    if( nullptr == feature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to SetFeature()." );
        return OGRERR_FAILURE;
    }

    if (feature->GetFID() == OGRNullFID)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "FID required on features given to SetFeature().");
        return OGRERR_FAILURE;
    }

    if (OGRNullFID == fidFieldIndex_)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to update features in tables without\n"
            "a recognised FID column.");
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    if (updateFeatureStmt_.isNull())
    {
        updateFeatureStmt_ = CreateUpdateFeatureStatement();
        if (updateFeatureStmt_.isNull())
            return OGRERR_FAILURE;
    }

    try
    {
        OGRErr err = SetStatementParameters(
            *updateFeatureStmt_, feature, false, false, "SetFeature");

        if (OGRERR_NONE != err)
            return err;

        bool withBatch = dataSource_->IsTransactionStarted();
        if (withBatch)
            updateFeatureStmt_->addBatch();

        auto ret = ExecuteUpdate(*updateFeatureStmt_, withBatch, "SetFeature");
        return (OGRERR_NONE == ret.first && ret.second != 1)
                   ? OGRERR_NON_EXISTING_FEATURE
                   : ret.first;
    }
    catch (const std::exception& ex)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                  "Unable to create feature: %s", ex.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRHanaTableLayer::CreateField(OGRFieldDefn* srsField, int approxOK)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "CreateField");
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    OGRFieldDefn dstField(srsField);

    if (launderColumnNames_)
    {
        CPLString launderName = LaunderName(dstField.GetNameRef());
        dstField.SetName(launderName.c_str());
    }

    if (fidFieldIndex_ != OGRNullFID
        && EQUAL(dstField.GetNameRef(), GetFIDColumn())
        && dstField.GetType() != OFTInteger
        && dstField.GetType() != OFTInteger64)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
            dstField.GetNameRef());
        return OGRERR_FAILURE;
    }

    ColumnTypeInfo columnTypeInfo = GetColumnTypeInfo(dstField);
    CPLString columnDef = GetColumnDefinition(columnTypeInfo);

    if (columnTypeInfo.type == odbc::SQLDataTypes::Unknown)
    {
        if (columnTypeInfo.name.empty())
            return OGRERR_FAILURE;

        if (approxOK)
        {
            dstField.SetDefault(nullptr);
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "Unable to create field %s with type %s on HANA layers. "
                "Creating as VARCHAR.",
                dstField.GetNameRef(),
                OGRFieldDefn::GetFieldTypeName(dstField.GetType()));
            columnTypeInfo.name = "VARCHAR";
            columnTypeInfo.width = static_cast<int>(defaultStringSize_);
            columnDef = "VARCHAR(" + std::to_string(defaultStringSize_) + ")";
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Unable to create field %s with type %s on HANA layers.",
                dstField.GetNameRef(),
                OGRFieldDefn::GetFieldTypeName(dstField.GetType()));

            return OGRERR_FAILURE;
        }
    }

    CPLString clmClause =
        QuotedIdentifier(dstField.GetNameRef()) + " " + columnDef;
    if (!dstField.IsNullable())
        clmClause += " NOT NULL";
    if (dstField.GetDefault() != nullptr && !dstField.IsDefaultDriverSpecific())
    {
        if (IsArrayField(dstField.GetType())
            || columnTypeInfo.type == odbc::SQLDataTypes::LongVarBinary)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Default value cannot be created on column of data type %s: "
                "%s.",
                columnTypeInfo.name.c_str(), dstField.GetNameRef());

            return OGRERR_FAILURE;
        }

        clmClause +=
            CPLString().Printf(" DEFAULT %s", GetColumnDefaultValue(dstField));
    }

    const CPLString sql = CPLString().Printf(
        "ALTER TABLE %s ADD(%s)",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        clmClause.c_str());

    try
    {
        dataSource_->ExecuteSQL(sql.c_str());
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Failed to execute create attribute field %s: %s",
            dstField.GetNameRef(), ex.what());
        return OGRERR_FAILURE;
    }

    // columnTypeInfo might contain a different defintion due to custom column types
    SetFieldDefn(dstField, columnTypeInfo);

    AttributeColumnDescription clmDesc;
    clmDesc.name = dstField.GetNameRef();
    clmDesc.type = columnTypeInfo.type;
    clmDesc.typeName = columnTypeInfo.name;
    clmDesc.isArray = IsArrayField(dstField.GetType());
    clmDesc.length = columnTypeInfo.width;
    clmDesc.isNullable = dstField.IsNullable();
    clmDesc.isAutoIncrement = false; // TODO
    clmDesc.scale = static_cast<unsigned short>(columnTypeInfo.precision);
    clmDesc.precision = static_cast<unsigned short>(columnTypeInfo.width);
    if (dstField.GetDefault() != nullptr)
        clmDesc.defaultValue = dstField.GetDefault();

    attrColumns_.push_back(clmDesc);
    featureDefn_->AddFieldDefn(&dstField);

    ClearQueryStatement();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRHanaTableLayer::CreateGeomField(OGRGeomFieldDefn* geomField, int)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, UNSUPPORTED_OP_READ_ONLY,
            "CreateGeomField");
        return OGRERR_FAILURE;
    }

    if (!IsGeometryTypeSupported(geomField->GetType()))
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Geometry field '%s' in layer '%s' has unsupported type %s",
            geomField->GetNameRef(), tableName_.c_str(),
            OGRGeometryTypeToName(geomField->GetType()));
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    if (featureDefn_->GetGeomFieldIndex(geomField->GetNameRef()) >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  geomField->GetNameRef());
        return OGRERR_FAILURE;
    }

    int srid = dataSource_->GetSrsId(geomField->GetSpatialRef());
    if (srid == UNDETERMINED_SRID)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to determine the srs-id for field name: %s",
                  geomField->GetNameRef());
        return OGRERR_FAILURE;
    }


    CPLString clmName(launderColumnNames_
                      ? LaunderName(geomField->GetNameRef()).c_str()
                      : geomField->GetNameRef());

    if (clmName.empty())
        clmName = FindGeomFieldName(*featureDefn_);

    CPLString sql = CPLString().Printf(
        "ALTER TABLE %s ADD(%s ST_GEOMETRY(%d))",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        QuotedIdentifier(clmName).c_str(), srid);

    try
    {
        dataSource_->ExecuteSQL(sql.c_str());
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Failed to execute CreateGeomField() with field name '%s': %s",
            geomField->GetNameRef(), ex.what());
        return OGRERR_FAILURE;
    }

    auto newGeomField = cpl::make_unique<OGRGeomFieldDefn>(
        clmName.c_str(), geomField->GetType());
    newGeomField->SetNullable(geomField->IsNullable());
    newGeomField->SetSpatialRef(geomField->GetSpatialRef());
    geomColumns_.push_back(
        {newGeomField->GetNameRef(), newGeomField->GetType(),
         srid, newGeomField->IsNullable() == TRUE});
    featureDefn_->AddGeomFieldDefn(std::move(newGeomField));

    ResetPreparedStatements();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRHanaTableLayer::DeleteField(int field)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "DeleteField");
        return OGRERR_FAILURE;
    }

    if (field < 0 || field >= featureDefn_->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Field index is out of range");
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    CPLString clmName = featureDefn_->GetFieldDefn(field)->GetNameRef();
    CPLString sql = CPLString().Printf(
        "ALTER TABLE %s DROP (%s)",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        QuotedIdentifier(clmName).c_str());

    try
    {
        dataSource_->ExecuteSQL(sql.c_str());
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to drop column %s: %s",
            clmName.c_str(), ex.what());
        return OGRERR_FAILURE;
    }

    auto it = std::find_if(
        attrColumns_.begin(), attrColumns_.end(),
        [&](const AttributeColumnDescription& cd) {
            return cd.name == clmName;
        });
    attrColumns_.erase(it);
    OGRErr ret = featureDefn_->DeleteFieldDefn(field);

    ResetPreparedStatements();

    return ret;
}

/************************************************************************/
/*                            AlterFieldDefn()                          */
/************************************************************************/

OGRErr OGRHanaTableLayer::AlterFieldDefn(
    int field, OGRFieldDefn* newFieldDefn, int flagsIn)
{
    if (!updateMode_)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
            "AlterFieldDefn");
        return OGRERR_FAILURE;
    }

    EnsureInitialized();

    if (field < 0 || field >= featureDefn_->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Field index is out of range");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn* fieldDefn = featureDefn_->GetFieldDefn(field);

    int64_t columnDescIdx = -1;
    for (size_t i = 0; i < attrColumns_.size(); ++i)
    {
        if (EQUAL(attrColumns_[i].name.c_str(), fieldDefn->GetNameRef()))
        {
            columnDescIdx = i;
            break;
        }
    }

    if (columnDescIdx < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Column description cannot be found");
        return OGRERR_FAILURE;
    }

    CPLString clmName = launderColumnNames_
                            ? LaunderName(newFieldDefn->GetNameRef())
                            : CPLString(newFieldDefn->GetNameRef());

    ColumnTypeInfo columnTypeInfo = GetColumnTypeInfo(*newFieldDefn);

    try
    {
        if ((flagsIn & ALTER_NAME_FLAG)
            && (strcmp(fieldDefn->GetNameRef(), newFieldDefn->GetNameRef())
                != 0))
        {
            CPLString sql = CPLString().Printf(
                "RENAME COLUMN %s TO %s",
                GetFullColumnNameQuoted(
                    schemaName_, tableName_, fieldDefn->GetNameRef())
                    .c_str(),
                QuotedIdentifier(clmName).c_str());
            dataSource_->ExecuteSQL(sql.c_str());
        }

        if ((flagsIn & ALTER_TYPE_FLAG)
            || (flagsIn & ALTER_WIDTH_PRECISION_FLAG)
            || (flagsIn & ALTER_NULLABLE_FLAG)
            || (flagsIn & ALTER_DEFAULT_FLAG))
        {
            CPLString fieldTypeDef = GetColumnDefinition(columnTypeInfo);
            if ((flagsIn & ALTER_NULLABLE_FLAG)
                && fieldDefn->IsNullable() != newFieldDefn->IsNullable())
            {
                if (fieldDefn->IsNullable())
                    fieldTypeDef += " NULL";
                else
                    fieldTypeDef += " NOT NULL";
            }

            if ((flagsIn & ALTER_DEFAULT_FLAG)
                && ((fieldDefn->GetDefault() == nullptr
                     && newFieldDefn->GetDefault() != nullptr)
                    || (fieldDefn->GetDefault() != nullptr
                        && newFieldDefn->GetDefault() == nullptr)
                    || (fieldDefn->GetDefault() != nullptr
                        && newFieldDefn->GetDefault() != nullptr
                        && strcmp(
                               fieldDefn->GetDefault(),
                               newFieldDefn->GetDefault())
                               != 0)))
            {
                fieldTypeDef +=
                    " DEFAULT "
                    + ((fieldDefn->GetType() == OFTString)
                           ? Literal(newFieldDefn->GetDefault())
                           : CPLString(newFieldDefn->GetDefault()));
            }

            CPLString sql = CPLString().Printf(
                "ALTER TABLE %s ALTER(%s %s)",
                GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
                QuotedIdentifier(clmName).c_str(), fieldTypeDef.c_str());

            dataSource_->ExecuteSQL(sql.c_str());
        }
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to alter field %s: %s",
            fieldDefn->GetNameRef(), ex.what());
        return OGRERR_FAILURE;
    }

    // Update field definition and column description

    AttributeColumnDescription& attrClmDesc = attrColumns_[columnDescIdx];

    if (flagsIn & ALTER_NAME_FLAG)
    {
        fieldDefn->SetName(clmName.c_str());
        attrClmDesc.name.assign(clmName.c_str());
    }

    if (flagsIn & ALTER_TYPE_FLAG)
    {
        fieldDefn->SetSubType(OFSTNone);
        fieldDefn->SetType(newFieldDefn->GetType());
        fieldDefn->SetSubType(newFieldDefn->GetSubType());
        attrClmDesc.isArray = IsArrayField(newFieldDefn->GetType());
        attrClmDesc.type = columnTypeInfo.type;
        attrClmDesc.typeName = columnTypeInfo.name;
    }

    if (flagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        fieldDefn->SetWidth(newFieldDefn->GetWidth());
        fieldDefn->SetPrecision(newFieldDefn->GetPrecision());
        attrClmDesc.length = newFieldDefn->GetWidth();
        attrClmDesc.scale = newFieldDefn->GetWidth();
        attrClmDesc.precision = newFieldDefn->GetPrecision();
    }

    if (flagsIn & ALTER_NULLABLE_FLAG)
    {
        fieldDefn->SetNullable(newFieldDefn->IsNullable());
        attrClmDesc.isNullable = newFieldDefn->IsNullable();
    }

    if (flagsIn & ALTER_DEFAULT_FLAG)
    {
        fieldDefn->SetDefault(newFieldDefn->GetDefault());
        attrClmDesc.name.assign(newFieldDefn->GetDefault());
    }

    ClearQueryStatement();
    ResetReading();
    ResetPreparedStatements();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          ClearBatches()                              */
/************************************************************************/

void OGRHanaTableLayer::ClearBatches()
{
    if (!insertFeatureStmtWithFID_.isNull())
        insertFeatureStmtWithFID_->clearBatch();
    if (!insertFeatureStmtWithoutFID_.isNull())
        insertFeatureStmtWithoutFID_->clearBatch();
    if (!updateFeatureStmt_.isNull())
        updateFeatureStmt_->clearBatch();
}

/************************************************************************/
/*                          SetCustomColumnTypes()                      */
/************************************************************************/

void OGRHanaTableLayer::SetCustomColumnTypes(const char* columnTypes)
{
    if (columnTypes == nullptr)
        return;

    const char* ptr = columnTypes;
    const char* start = ptr;
    while (*ptr != '\0')
    {
        if (*ptr == '(')
        {
            // Skip commas inside brackets, for example decimal(20,5)
            while (*ptr != '\0' && *ptr != ')')
            {
                ++ptr;
            }
        }

        ++ptr;

        if (*ptr == ',' || *ptr == '\0')
        {
            std::size_t len = static_cast<std::size_t>(ptr - start);
            const char* sep = std::find(start, start + len, '=');
            if (sep != nullptr)
            {
                std::size_t pos = static_cast<std::size_t>(sep - start);
                customColumnDefs_.push_back(
                    {CPLString(start, pos),
                     CPLString(start + pos + 1, len - pos - 1)});
            }

            start = ptr + 1;
        }
    }
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRHanaTableLayer::StartTransaction()
{
    return dataSource_->StartTransaction();
}

/************************************************************************/
/*                          CommitTransaction()                         */
/************************************************************************/

OGRErr OGRHanaTableLayer::CommitTransaction()
{
    try
    {
        if (!deleteFeatureStmt_.isNull()
            && deleteFeatureStmt_->getBatchDataSize() > 0)
            deleteFeatureStmt_->executeBatch();
        if (!insertFeatureStmtWithFID_.isNull()
            && insertFeatureStmtWithFID_->getBatchDataSize() > 0)
            insertFeatureStmtWithFID_->executeBatch();
        if (!insertFeatureStmtWithoutFID_.isNull()
            && insertFeatureStmtWithoutFID_->getBatchDataSize() > 0)
            insertFeatureStmtWithoutFID_->executeBatch();
        if (!updateFeatureStmt_.isNull()
            && updateFeatureStmt_->getBatchDataSize() > 0)
            updateFeatureStmt_->executeBatch();

        ClearBatches();
    }
    catch (const odbc::Exception& ex)
    {
        ClearBatches();
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to execute batch insert: %s",
            ex.what());
        return OGRERR_FAILURE;
    }

    dataSource_->CommitTransaction();
    return OGRERR_NONE;
}

/************************************************************************/
/*                          RollbackTransaction()                       */
/************************************************************************/

OGRErr OGRHanaTableLayer::RollbackTransaction()
{
    ClearBatches();
    return dataSource_->RollbackTransaction();
}

} /* end of OGRHANA namespace */
