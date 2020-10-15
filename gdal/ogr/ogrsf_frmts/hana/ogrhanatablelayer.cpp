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

#include "odbc/Exception.h"
#include "odbc/PreparedStatement.h"
#include "odbc/Types.h"

CPL_CVSID("$Id$")

using namespace hana_utils;

namespace {

constexpr const char* UNSUPPORTED_OP_READ_ONLY =
    "%s : unsupported operation on a read-only datasource.";

bool IsArrayField(OGRFieldType fieldType)
{
    return (
        fieldType == OFTIntegerList || fieldType == OFTInteger64List
        || fieldType == OFTRealList || fieldType == OFTStringList);
}

const char* GetColumnDefaultValue(const OGRFieldDefn& field)
{
    const char* defaultValue = field.GetDefault();
    if (field.GetType() == OFTInteger && field.GetSubType() == OFSTBoolean)
        return (EQUAL(defaultValue, "1") || EQUAL(defaultValue, "'t'")) ? "TRUE" : "FALSE";

    return defaultValue;
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

} // namespace

/************************************************************************/
/*                         OGRHanaTableLayer()                          */
/************************************************************************/

OGRHanaTableLayer::OGRHanaTableLayer(OGRHanaDataSource* datasource, int update)
    : OGRHanaLayer(datasource)
    , updateMode_(update)
    , insertFeatureStmtHasFID(false)
    , batchSize_(4 * 1024)
    , defaultStringSize_(256)
    , launderColumnNames_(true)
    , preservePrecision_(true)
    , parseFunctionsChecked_(false)
{
}

/************************************************************************/
/*                        ~OGRHanaTableLayer()                          */
/************************************************************************/

OGRHanaTableLayer::~OGRHanaTableLayer()
{
    FlushPendingFeatures();
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/************************************************************************/

OGRErr OGRHanaTableLayer::ReadTableDefinition()
{
    OGRErr err = ReadFeatureDefinition(
        schemaName_, tableName_, rawQuery_, tableName_.c_str());
    if (err != OGRERR_NONE)
        return err;

    if (fidFieldIndex_ != OGRNullFID)
        CPLDebug(
            "HANA", "table %s has FID column %s.", tableName_.c_str(),
            attrColumns_[static_cast<size_t>(fidFieldIndex_)].name.c_str());
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
    odbc::PreparedStatementRef& statement, const char* functionName)
{
    std::size_t ret = 0;

    try
    {
        if (dataSource_->IsTransactionStarted())
        {
            if (statement->getBatchDataSize() >= batchSize_)
                statement->executeBatch();
            ret = 1;
        }
        else
        {
            ret = statement->executeUpdate();
            dataSource_->Commit();
        }
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
    CPLString sql = StringFormat(
        "DELETE FROM %s WHERE %s = ?",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        QuotedIdentifier(GetFIDColumn()).c_str());
    return dataSource_->PrepareStatement(sql.c_str());
}

/* -------------------------------------------------------------------- */
/*                   CreateInsertFeatureStatement()                     */
/* -------------------------------------------------------------------- */

odbc::PreparedStatementRef OGRHanaTableLayer::CreateInsertFeatureStatement(
    GIntBig fidColumnID)
{
    insertFeatureStmtHasFID = false;

    std::vector<CPLString> columns;
    std::vector<CPLString> values;
    bool hasArray = false;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID)
        {
            if (fidColumnID == OGRNullFID)
                continue;
            else
                insertFeatureStmtHasFID = true;
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

    const CPLString sql = StringFormat(
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
    bool hasArray = false;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID)
            continue;
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

    const CPLString sql = StringFormat(
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
    if (!insertFeatureStmt_.isNull())
        insertFeatureStmt_ = nullptr;
    if (!deleteFeatureStmt_.isNull())
        deleteFeatureStmt_ = nullptr;
    if (!updateFeatureStmt_.isNull())
        updateFeatureStmt_ = nullptr;
}

/************************************************************************/
/*                        SetStatementParameters()                      */
/************************************************************************/

OGRErr OGRHanaTableLayer::SetStatementParameters(
    odbc::PreparedStatementRef& stmt,
    OGRFeature* feature,
    bool skipFidColumn,
    bool newFeature,
    const char* functionName)
{
    OGRHanaFeatureReader featReader(*feature);

    unsigned short paramIndex = 0;
    int fieldIndex = -1;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        if (clmDesc.isFeatureID)
        {
            if (skipFidColumn)
                continue;

            ++paramIndex;

            switch (clmDesc.type)
            {
            case odbc::SQLDataTypes::Integer:
                if (!CanCastIntBigTo<std ::int32_t>(feature->GetFID()))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "%s: Feature id with value %s cannot "
                        "be stored in a column of type INTEGER",
                        functionName,
                        std::to_string(feature->GetFID()).c_str());
                    return OGRERR_FAILURE;
                }
                stmt->setInt(
                    paramIndex,
                    odbc::Int(static_cast<std::int32_t>(feature->GetFID())));
                break;
            case odbc::SQLDataTypes::BigInt:
                if (!CanCastIntBigTo<std::int64_t>(feature->GetFID()))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "%s: Feature id with value %s cannot "
                        "be stored in a column of type BIGINT",
                        functionName,
                        std::to_string(feature->GetFID()).c_str());
                    return OGRERR_FAILURE;
                }
                stmt->setLong(
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
            stmt->setBoolean(
                paramIndex, featReader.GetFieldAsBoolean(fieldIndex));
            break;
        case odbc::SQLDataTypes::TinyInt:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                stmt->setByte(
                    paramIndex, featReader.GetFieldAsByte(fieldIndex));
            break;
        case odbc::SQLDataTypes::SmallInt:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                stmt->setShort(
                    paramIndex, featReader.GetFieldAsShort(fieldIndex));
            break;
        case odbc::SQLDataTypes::Integer:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsIntArray(fieldIndex));
            else
                stmt->setInt(paramIndex, featReader.GetFieldAsInt(fieldIndex));
            break;
        case odbc::SQLDataTypes::BigInt:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsBigIntArray(fieldIndex));
            else
                stmt->setLong(
                    paramIndex, featReader.GetFieldAsLong(fieldIndex));
            break;
        case odbc::SQLDataTypes::Float:
        case odbc::SQLDataTypes::Real:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsRealArray(fieldIndex));
            else
                stmt->setFloat(
                    paramIndex, featReader.GetFieldAsFloat(fieldIndex));
            break;
        case odbc::SQLDataTypes::Double:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsDoubleArray(fieldIndex));
            else
                stmt->setDouble(
                    paramIndex, featReader.GetFieldAsDouble(fieldIndex));
            break;
        case odbc::SQLDataTypes::Decimal:
        case odbc::SQLDataTypes::Numeric:
            if ((!feature->IsFieldSet(fieldIndex)
                 || feature->IsFieldNull(fieldIndex))
                && feature->GetFieldDefnRef(fieldIndex)->GetDefault()
                       == nullptr)
                stmt->setDecimal(paramIndex, odbc::Decimal());
            else
                stmt->setDouble(
                    paramIndex, featReader.GetFieldAsDouble(fieldIndex));
            break;
        case odbc::SQLDataTypes::Char:
        case odbc::SQLDataTypes::VarChar:
        case odbc::SQLDataTypes::LongVarChar:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsStringArray(fieldIndex));
            else
                stmt->setString(
                    paramIndex,
                    featReader.GetFieldAsString(fieldIndex, clmDesc.length));
            break;
        case odbc::SQLDataTypes::WChar:
        case odbc::SQLDataTypes::WVarChar:
        case odbc::SQLDataTypes::WLongVarChar:
            if (clmDesc.isArray)
                stmt->setString(
                    paramIndex, featReader.GetFieldAsStringArray(fieldIndex));
            else
                stmt->setString(
                    paramIndex,
                    featReader.GetFieldAsNString(fieldIndex, clmDesc.length));
            break;
        case odbc::SQLDataTypes::Binary:
        case odbc::SQLDataTypes::VarBinary:
        case odbc::SQLDataTypes::LongVarBinary: {
            Binary bin = featReader.GetFieldAsBinary(fieldIndex);
            stmt->setBytes(paramIndex, bin.data, bin.size);
        }
        break;
        case odbc::SQLDataTypes::DateTime:
        case odbc::SQLDataTypes::TypeDate:
            stmt->setDate(paramIndex, featReader.GetFieldAsDate(fieldIndex));
            break;
        case odbc::SQLDataTypes::Time:
        case odbc::SQLDataTypes::TypeTime:
            stmt->setTime(paramIndex, featReader.GetFieldAsTime(fieldIndex));
            break;
        case odbc::SQLDataTypes::Timestamp:
        case odbc::SQLDataTypes::TypeTimestamp:
            stmt->setTimestamp(
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
        stmt->setBytes(paramIndex, wkb.data, wkb.size);
    }

    if (!newFeature)
    {
        ++paramIndex;
        if (!CanCastIntBigTo<std::int64_t>(feature->GetFID()))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "%s: Feature id with value %s cannot "
                "be stored in a column of type INTEGER",
                functionName, std::to_string(feature->GetFID()).c_str());
            return OGRERR_FAILURE;
        }

        stmt->setLong(
            paramIndex,
            odbc::Long(static_cast<std::int64_t>(feature->GetFID())));
    }

    if (dataSource_->IsTransactionStarted())
        stmt->addBatch();

    return OGRERR_NONE;
}

/* -------------------------------------------------------------------- */
/*                            DropTable()                               */
/* -------------------------------------------------------------------- */

void OGRHanaTableLayer::DropTable()
{
    CPLString sql =
        "DROP TABLE " + GetFullTableNameQuoted(schemaName_, tableName_);
    dataSource_->ExecuteSQL(sql.c_str());

    CPLDebug("HANA", "Dropped table %s.", GetName());
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
           || (!insertFeatureStmt_.isNull()
               && insertFeatureStmt_->getBatchDataSize() > 0)
           || (!updateFeatureStmt_.isNull()
               && updateFeatureStmt_->getBatchDataSize() > 0);
}

/* -------------------------------------------------------------------- */
/*                            GetFieldTypeInfo()                        */
/* -------------------------------------------------------------------- */

FieldTypeInfo OGRHanaTableLayer::GetFieldTypeInfo(OGRFieldDefn* field) const
{
    CPLString fieldTypeName;
    short fieldType = UNKNOWN_DATA_TYPE;

    switch (field->GetType())
    {
    case OFTInteger:
        if (preservePrecision_ && field->GetWidth() > 10)
        {
            fieldTypeName = StringFormat("DECIMAL(%d)", field->GetWidth());
            fieldType = odbc::SQLDataTypes::Decimal;
        }
        else
        {
            if (field->GetSubType() == OFSTBoolean)
            {
                fieldTypeName = "BOOLEAN";
                fieldType = odbc::SQLDataTypes::Boolean;
            }
            else if (field->GetSubType() == OFSTInt16)
            {
                fieldTypeName = "SMALLINT";
                fieldType = odbc::SQLDataTypes::SmallInt;
            }
            else
            {
                fieldTypeName = "INTEGER";
                fieldType = odbc::SQLDataTypes::Integer;
            }
        }
        break;
    case OFTInteger64:
        if (preservePrecision_ && field->GetWidth() > 20)
        {
            fieldTypeName = StringFormat("DECIMAL(%d)", field->GetWidth());
            fieldType = odbc::SQLDataTypes::Decimal;
        }
        else
        {
            fieldTypeName = "BIGINT";
            fieldType = odbc::SQLDataTypes::BigInt;
        }
        break;
    case OFTReal:
        if (preservePrecision_ && field->GetWidth() != 0)
        {
            fieldTypeName = StringFormat(
                "DECIMAL(%d,%d)", field->GetWidth(), field->GetPrecision());
            fieldType = odbc::SQLDataTypes::Decimal;
        }
        else
        {
            if (field->GetSubType() == OFSTFloat32)
            {
                fieldTypeName = "REAL";
                fieldType = odbc::SQLDataTypes::Real;
            }
            else
            {
                fieldTypeName = "DOUBLE";
                fieldType = odbc::SQLDataTypes::Double;
            }
        }
        break;
    case OFTString:
        if (field->GetWidth() == 0 || !preservePrecision_)
        {
            fieldTypeName =
                (defaultStringSize_ == 0)
                    ? "NVARCHAR"
                    : StringFormat(
                        "NVARCHAR(%d)", static_cast<int>(defaultStringSize_));
            fieldType = odbc::SQLDataTypes::WLongVarChar;
        }
        else
        {
            if (field->GetWidth() <= 5000)
            {
                fieldTypeName = StringFormat("NVARCHAR(%d)", field->GetWidth());
                fieldType = odbc::SQLDataTypes::WLongVarChar;
            }
            else
            {
                fieldTypeName = "NCLOB";
                fieldType = odbc::SQLDataTypes::WLongVarChar;
            }
        }
        break;
    case OFTBinary:
        if (field->GetWidth() <= 5000)
        {
            fieldTypeName =
                (field->GetWidth() == 0)
                    ? "VARBINARY"
                    : StringFormat("VARBINARY(%d)", field->GetWidth());
            fieldType = odbc::SQLDataTypes::VarBinary;
        }
        else
        {
            fieldTypeName = "BLOB";
            fieldType = odbc::SQLDataTypes::LongVarBinary;
        }
        break;
    case OFTDate:
        fieldTypeName = "DATE";
        fieldType = odbc::SQLDataTypes::TypeDate;
        break;
    case OFTTime:
        fieldTypeName = "TIME";
        fieldType = odbc::SQLDataTypes::TypeTime;
        break;
    case OFTDateTime:
        fieldTypeName = "TIMESTAMP";
        fieldType = odbc::SQLDataTypes::TypeTimestamp;
        break;
    case OFTIntegerList:
        if (field->GetSubType() == OGRFieldSubType::OFSTInt16)
        {
            fieldTypeName = "SMALLINT ARRAY";
            fieldType = odbc::SQLDataTypes::SmallInt;
        }
        else
        {
            fieldTypeName = "INTEGER ARRAY";
            fieldType = odbc::SQLDataTypes::Integer;
        }
        break;
    case OFTInteger64List:
        fieldTypeName = "BIGINT ARRAY";
        fieldType = odbc::SQLDataTypes::BigInt;
        break;
    case OFTRealList:
        if (field->GetSubType() == OGRFieldSubType::OFSTFloat32)
        {
            fieldTypeName = "REAL ARRAY";
            fieldType = odbc::SQLDataTypes::Real;
        }
        else
        {
            fieldTypeName = "DOUBLE ARRAY";
            fieldType = odbc::SQLDataTypes::Double;
        }
        break;
    case OFTStringList:
        fieldTypeName = "NVARCHAR(512) ARRAY";
        fieldType = odbc::SQLDataTypes::WVarChar;
        break;
    default:
        break;
    }

    if (!customColumnDefs_.empty())
    {
        for (const auto& clmType : customColumnDefs_)
        {
            if (strcmp(clmType.name.c_str(), field->GetNameRef()) == 0)
            {
                fieldTypeName = clmType.typeName;
                break;
            }
        }
    }

    return {fieldTypeName, fieldType};
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

/* -------------------------------------------------------------------- */
/*                            Initialize()                               */
/* -------------------------------------------------------------------- */

OGRErr OGRHanaTableLayer::Initialize(
    const char* schemaName, const char* tableName)
{
    schemaName_ = schemaName;
    tableName_ = tableName;
    rawQuery_ =
        "SELECT * FROM " + GetFullTableNameQuoted(schemaName, tableName);

    OGRErr err = ReadTableDefinition();
    if (err != OGRERR_NONE)
        return err;

    SetDescription(featureDefn_->GetName());

    ResetReading();
    return OGRERR_NONE;
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
        return fidFieldIndex_ != OGRNullFID;
    if (EQUAL(capabilities, OLCFastFeatureCount))
        return TRUE;
    if (EQUAL(capabilities, OLCFastSpatialFilter))
        return !geomColumns_.empty();
    if (EQUAL(capabilities, OLCFastGetExtent))
        return !geomColumns_.empty();
    if (EQUAL(capabilities, OLCCreateField))
        return updateMode_;
    if (EQUAL(capabilities, OLCCreateGeomField)
        || EQUAL(capabilities, ODsCCreateGeomFieldAfterCreateLayer))
        return updateMode_;
    if (EQUAL(capabilities, OLCDeleteField))
        return updateMode_;
    if (EQUAL(capabilities, OLCDeleteFeature))
        return updateMode_ && fidFieldIndex_ != OGRNullFID;
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

    return FALSE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRHanaTableLayer::SetAttributeFilter(const char* attrFilter)
{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = attrFilter ? CPLStrdup(attrFilter) : nullptr;

    if (attrFilter == nullptr || strlen(attrFilter) == 0)
        attrFilter_ = "";
    else
        attrFilter_.assign(attrFilter, strlen(attrFilter));

    rebuildQueryStatement_ = true;
    BuildWhereClause();
    ResetReading();

    return OGRERR_NONE;
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

    try
    {
        if (insertFeatureStmt_.isNull())
        {
            insertFeatureStmt_ =
                CreateInsertFeatureStatement(feature->GetFID());
            if (insertFeatureStmt_.isNull())
                return OGRERR_FAILURE;
        }

        OGRErr err = SetStatementParameters(
            insertFeatureStmt_, feature, !insertFeatureStmtHasFID, true,
            "CreateFeature");

        if (OGRERR_NONE != err)
            return err;

        auto ret = ExecuteUpdate(insertFeatureStmt_, "CreateFeature");
        return ret.first;
    }
    catch (const std::exception& ex)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Error: %s", ex.what());
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

    if (deleteFeatureStmt_.isNull())
    {
        deleteFeatureStmt_ = CreateDeleteFeatureStatement();
        if (deleteFeatureStmt_.isNull())
            return OGRERR_FAILURE;
    }

    deleteFeatureStmt_->setLong(1, odbc::Long(static_cast<std::int64_t>(nFID)));
    if (dataSource_->IsTransactionStarted())
        deleteFeatureStmt_->addBatch();

    auto ret = ExecuteUpdate(deleteFeatureStmt_, "DeleteFeature");
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

    if (feature->GetFID() == OGRNullFID)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "FID required on features given to SetFeature().");
        return OGRERR_FAILURE;
    }

    if (nullptr == feature)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "NULL pointer to OGRFeature passed to SetFeature().");
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

    if (updateFeatureStmt_.isNull())
    {
        updateFeatureStmt_ = CreateUpdateFeatureStatement();
        if (updateFeatureStmt_.isNull())
            return OGRERR_FAILURE;
    }

    OGRErr err = SetStatementParameters(
        updateFeatureStmt_, feature, true, false, "SetFeature");

    if (OGRERR_NONE != err)
        return err;

    auto ret = ExecuteUpdate(updateFeatureStmt_, "SetFeature");
    return (OGRERR_NONE == ret.first && ret.second != 1)
               ? OGRERR_NON_EXISTING_FEATURE
               : ret.first;
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

    OGRFieldDefn dstField(srsField);

    if (launderColumnNames_)
        dstField.SetName(LaunderName(dstField.GetNameRef()).c_str());

    FieldTypeInfo fieldTypeInfo = GetFieldTypeInfo(&dstField);

    if (fieldTypeInfo.type == UNKNOWN_DATA_TYPE)
    {
        if (approxOK)
        {
            dstField.SetDefault(nullptr);
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "Unable to create field %s with type %s on HANA layers. "
                "Creating as VARCHAR.",
                dstField.GetNameRef(),
                OGRFieldDefn::GetFieldTypeName(dstField.GetType()));
            fieldTypeInfo.name =
                "VARCHAR(" + std::to_string(defaultStringSize_) + ")";
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
        QuotedIdentifier(dstField.GetNameRef()) + " " + fieldTypeInfo.name;
    if (!dstField.IsNullable())
        clmClause += " NOT NULL";
    if (dstField.GetDefault() != nullptr && !dstField.IsDefaultDriverSpecific())
    {
        if (IsArrayField(dstField.GetType())
            || fieldTypeInfo.type == odbc::SQLDataTypes::LongVarBinary)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Default value cannot be created on column of data type %s: "
                "%s.",
                fieldTypeInfo.name.c_str(), dstField.GetNameRef());

            return OGRERR_FAILURE;
        }

        clmClause +=
            StringFormat(" DEFAULT %s", GetColumnDefaultValue(dstField));
    }

    const CPLString sql = StringFormat(
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

    AttributeColumnDescription clmDesc;
    clmDesc.name = dstField.GetNameRef();
    clmDesc.type = fieldTypeInfo.type;
    clmDesc.typeName = fieldTypeInfo.name;
    clmDesc.isArray = IsArrayField(dstField.GetType());
    clmDesc.length = dstField.GetWidth();
    clmDesc.isNullable = dstField.IsNullable();
    clmDesc.isAutoIncrement = false; // TODO
    clmDesc.scale = static_cast<unsigned short>(dstField.GetPrecision());
    clmDesc.precision = static_cast<unsigned short>(dstField.GetWidth());
    if (dstField.GetDefault() != nullptr)
        clmDesc.defaultValue = dstField.GetDefault();

    featureDefn_->AddFieldDefn(&dstField);
    attrColumns_.push_back(clmDesc);

    rebuildQueryStatement_ = true;
    ResetPreparedStatements();
    ResetReading();

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

    if (EQUALN(geomField->GetNameRef(), "OGR_GEOMETRY", strlen("OGR_GEOMETRY")))
        return OGRERR_NONE;

    CPLString clmName = (launderColumnNames_)
                            ? LaunderName(geomField->GetNameRef())
                            : CPLString(geomField->GetNameRef());
    int srid = dataSource_->GetSrsId(geomField->GetSpatialRef());
    CPLString sql = StringFormat(
        "ALTER TABLE %s ADD(%s ST_GEOMETRY(%d))",
        GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
        QuotedIdentifier(clmName).c_str(), srid);

    if (!IsGeometryTypeSupported(geomField->GetType()))
    {
        CPLError(
            CE_Warning, CPLE_NotSupported,
            "Geometry field '%s' in layer '%s' has unsupported type %s",
            clmName.c_str(), tableName_.c_str(),
            OGRGeometryTypeToName(geomField->GetType()));
    }

    try
    {
        dataSource_->ExecuteSQL(sql.c_str());
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Failed to execute create geometry field %s: %s",
            geomField->GetNameRef(), ex.what());
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn* newGeomField =
        new OGRGeomFieldDefn(clmName.c_str(), geomField->GetType());
    newGeomField->SetNullable(geomField->IsNullable());
    newGeomField->SetSpatialRef(geomField->GetSpatialRef());
    featureDefn_->AddGeomFieldDefn(newGeomField, FALSE);
    geomColumns_.push_back(
        {clmName, geomField->GetType(), srid, geomField->IsNullable() != 0});

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

    CPLString clmName = featureDefn_->GetFieldDefn(field)->GetNameRef();
    CPLString sql = StringFormat(
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

    if (field < 0 || field >= featureDefn_->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Field index is out of range");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn* fieldDefn = featureDefn_->GetFieldDefn(field);
    CPLString clmName = launderColumnNames_
                            ? LaunderName(newFieldDefn->GetNameRef())
                            : CPLString(newFieldDefn->GetNameRef());

    try
    {
        if ((flagsIn & ALTER_NAME_FLAG)
            && (strcmp(fieldDefn->GetNameRef(), newFieldDefn->GetNameRef())
                != 0))
        {
            CPLString sql = StringFormat(
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
            CPLString fieldTypeInfo = GetFieldTypeInfo(newFieldDefn).name;
            if ((flagsIn & ALTER_NULLABLE_FLAG)
                && fieldDefn->IsNullable() != newFieldDefn->IsNullable())
            {
                if (fieldDefn->IsNullable())
                    fieldTypeInfo += " NULL";
                else
                    fieldTypeInfo += " NOT NULL";
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
                fieldTypeInfo +=
                    " DEFAULT "
                    + ((fieldDefn->GetType() == OFTString)
                           ? Literal(newFieldDefn->GetDefault())
                           : CPLString(newFieldDefn->GetDefault()));
            }

            CPLString sql = StringFormat(
                "ALTER TABLE %s ALTER(%s %s)",
                GetFullTableNameQuoted(schemaName_, tableName_).c_str(),
                QuotedIdentifier(clmName).c_str(), fieldTypeInfo.c_str());

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

    // TODO change an entry in attrColumns_;
    // Update field definition
    if (flagsIn & ALTER_NAME_FLAG)
        fieldDefn->SetName(newFieldDefn->GetNameRef());

    if (flagsIn & ALTER_TYPE_FLAG)
    {
        fieldDefn->SetSubType(OFSTNone);
        fieldDefn->SetType(newFieldDefn->GetType());
        fieldDefn->SetSubType(newFieldDefn->GetSubType());
    }

    if (flagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        fieldDefn->SetWidth(newFieldDefn->GetWidth());
        fieldDefn->SetPrecision(newFieldDefn->GetPrecision());
    }

    if (flagsIn & ALTER_NULLABLE_FLAG)
        fieldDefn->SetNullable(newFieldDefn->IsNullable());

    if (flagsIn & ALTER_DEFAULT_FLAG)
        fieldDefn->SetDefault(newFieldDefn->GetDefault());

    rebuildQueryStatement_ = true;
    ResetReading();
    ResetPreparedStatements();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          ClearBatches()                              */
/************************************************************************/

void OGRHanaTableLayer::ClearBatches()
{
    if (!insertFeatureStmt_.isNull())
        insertFeatureStmt_->clearBatch();
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
    if (!HasPendingFeatures())
        return OGRERR_NONE;

    try
    {
        if (!deleteFeatureStmt_.isNull()
            && deleteFeatureStmt_->getBatchDataSize() > 0)
            deleteFeatureStmt_->executeBatch();
        if (!insertFeatureStmt_.isNull()
            && insertFeatureStmt_->getBatchDataSize() > 0)
            insertFeatureStmt_->executeBatch();
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
