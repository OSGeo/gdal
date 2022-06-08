/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaLayer class implementation
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
#include "ogrhanafeaturewriter.h"
#include "ogrhanautils.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <memory>

#include "odbc/Exception.h"
#include "odbc/ResultSet.h"
#include "odbc/Statement.h"
#include "odbc/Types.h"

CPL_CVSID("$Id$")

namespace OGRHANA {
namespace {
/************************************************************************/
/*                          Helper methods                              */
/************************************************************************/

CPLString BuildQuery(
    const char* source,
    const char* columns,
    const char* where,
    const char* orderBy,
    int limit)
{
    std::ostringstream os;
    os << "SELECT " << columns << " FROM (" << source << ")";
    if (where != nullptr && strlen(where) > 0)
        os << " WHERE " << where;
    if (orderBy != nullptr && strlen(orderBy) > 0)
        os << " ORDER BY " << orderBy;
    if (limit >= 0)
        os << " LIMIT " << std::to_string(limit);
    return os.str();
}

CPLString BuildQuery(const char* source, const char* columns)
{
    return BuildQuery(source, columns, nullptr, nullptr, -1);
}

CPLString BuildSpatialFilter(int dbVersion, const OGRGeometry& geom, const CPLString& clmName, int srid)
{
    OGREnvelope env;
    geom.getEnvelope(&env);

    if( (CPLIsInf(env.MinX) || CPLIsInf(env.MinY) ||
         CPLIsInf(env.MaxX) || CPLIsInf(env.MaxY)) )
        return "";

    auto clampValue = []( double v)
    {
        constexpr double MAX_VALUE = 1e+150;
        if (v < -MAX_VALUE)
            return -MAX_VALUE;
        else if (v > MAX_VALUE)
            return MAX_VALUE;
        return v;
    };

    double minX = clampValue(env.MinX);
    double minY = clampValue(env.MinY);
    double maxX = clampValue(env.MaxX);
    double maxY = clampValue(env.MaxY);

    // TODO: add support for non-rectangular filter, see m_bFilterIsEnvelope flag.
    if ( dbVersion == 1 )
        return CPLString().Printf("\"%s\".ST_IntersectsRect(ST_GeomFromText('POINT(%.18g %.18g)', %d), ST_GeomFromText('POINT(%.18g %.18g)', %d)) = 1",
                            clmName.c_str(), minX, minY, srid, maxX, maxY, srid);
    else
        return CPLString().Printf("\"%s\".ST_IntersectsRectPlanar(ST_GeomFromText('POINT(%.18g %.18g)', %d), ST_GeomFromText('POINT(%.18g %.18g)', %d)) = 1",
                            clmName.c_str(), minX, minY, srid, maxX, maxY, srid);
}

std::unique_ptr<OGRFieldDefn> CreateFieldDefn(const AttributeColumnDescription& columnDesc)
{
    bool setFieldSize = false;
    bool setFieldPrecision = false;

    OGRFieldType ogrFieldType = OFTString;
    OGRFieldSubType ogrFieldSubType = OGRFieldSubType::OFSTNone;

    switch (columnDesc.type)
    {
    case odbc::SQLDataTypes::Bit:
    case odbc::SQLDataTypes::Boolean:
        ogrFieldType = columnDesc.isArray ? OFTIntegerList : OFTInteger;
        ogrFieldSubType = OGRFieldSubType::OFSTBoolean;
        break;
    case odbc::SQLDataTypes::TinyInt:
    case odbc::SQLDataTypes::SmallInt:
        ogrFieldType = columnDesc.isArray ? OFTIntegerList : OFTInteger;
        ogrFieldSubType = OGRFieldSubType::OFSTInt16;
        break;
    case odbc::SQLDataTypes::Integer:
        ogrFieldType = columnDesc.isArray ? OFTIntegerList : OFTInteger;
        break;
    case odbc::SQLDataTypes::BigInt:
        ogrFieldType = columnDesc.isArray ? OFTInteger64List : OFTInteger64;
        break;
    case odbc::SQLDataTypes::Double:
    case odbc::SQLDataTypes::Real:
    case odbc::SQLDataTypes::Float:
        ogrFieldType = columnDesc.isArray ? OFTRealList : OFTReal;
        if (columnDesc.type != odbc::SQLDataTypes::Double)
            ogrFieldSubType = OGRFieldSubType::OFSTFloat32;
        break;
    case odbc::SQLDataTypes::Decimal:
    case odbc::SQLDataTypes::Numeric:
        ogrFieldType = columnDesc.isArray ? OFTRealList : OFTReal;
        setFieldPrecision = true;
        break;
    case odbc::SQLDataTypes::Char:
    case odbc::SQLDataTypes::VarChar:
    case odbc::SQLDataTypes::LongVarChar:
    case odbc::SQLDataTypes::WChar:
    case odbc::SQLDataTypes::WVarChar:
    case odbc::SQLDataTypes::WLongVarChar:
        // Note: OFTWideString is deprecated
        ogrFieldType = columnDesc.isArray ? OFTStringList : OFTString;
        setFieldSize = true;
        break;
    case odbc::SQLDataTypes::Date:
    case odbc::SQLDataTypes::TypeDate:
        ogrFieldType = OFTDate;
        break;
    case odbc::SQLDataTypes::Time:
    case odbc::SQLDataTypes::TypeTime:
        ogrFieldType = OFTTime;
        break;
    case odbc::SQLDataTypes::Timestamp:
    case odbc::SQLDataTypes::TypeTimestamp:
        ogrFieldType = OFTDateTime;
        break;
    case odbc::SQLDataTypes::Binary:
    case odbc::SQLDataTypes::VarBinary:
    case odbc::SQLDataTypes::LongVarBinary:
        ogrFieldType = OFTBinary;
        setFieldSize = true;
        break;
    default:
        break;
    }

    if (columnDesc.isArray && !IsArrayField(ogrFieldType))
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array of type %s in column %s is not supported",
                 columnDesc.typeName.c_str(), columnDesc.name.c_str());

    auto field = cpl::make_unique<OGRFieldDefn>(columnDesc.name.c_str(), ogrFieldType);
    field->SetSubType(ogrFieldSubType);
    field->SetNullable(columnDesc.isNullable);
    if (!columnDesc.isArray)
    {
        if (setFieldSize)
            field->SetWidth(columnDesc.length);
        if (setFieldPrecision)
        {
            field->SetWidth(columnDesc.precision);
            field->SetPrecision(columnDesc.scale);
        }
    }
    if (columnDesc.defaultValue.empty())
        field->SetDefault(nullptr);
    else
        field->SetDefault(columnDesc.defaultValue.c_str());
    return field;
}

OGRGeometry* CreateGeometryFromWkb(const void* data, std::size_t size)
{
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        CPLError(
            CE_Failure, CPLE_AppDefined, "createFromWkb(): %s",
            "Geometry size is larger than maximum integer value");

    int len = static_cast<int>(size);

    OGRGeometry* geom = nullptr;
    OGRErr err = OGRGeometryFactory::createFromWkb(data, nullptr, &geom, len);

    if (OGRERR_NONE == err)
        return geom;

    auto cplError = [](const char* message) {
        CPLError(CE_Failure, CPLE_AppDefined, "ReadFeature(): %s", message);
    };

    switch (err)
    {
    case OGRERR_NOT_ENOUGH_DATA:
        cplError("Not enough data to deserialize");
        return nullptr;
    case OGRERR_UNSUPPORTED_GEOMETRY_TYPE:
        cplError("Unsupported geometry type");
        return nullptr;
    case OGRERR_CORRUPT_DATA:
        cplError("Corrupt data");
        return nullptr;
    default:
        cplError("Unrecognized error");
        return nullptr;
    }
}

} // anonymous namespace

/************************************************************************/
/*                           OGRHanaLayer()                             */
/************************************************************************/

OGRHanaLayer::OGRHanaLayer(OGRHanaDataSource* datasource)
    : dataSource_(datasource)
    , rawQuery_("")
    , queryStatement_("")
    , whereClause_("")
{
}

/************************************************************************/
/*                          ~OGRHanaLayer()                             */
/************************************************************************/

OGRHanaLayer::~OGRHanaLayer()
{
    if (featureDefn_)
        featureDefn_->Release();
}

void OGRHanaLayer::EnsureInitialized()
{
    if (initialized_)
        return;

    OGRErr err = Initialize();
    if (OGRERR_NONE != err)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Failed to initialize layer: %s", GetName());
    }
    initialized_ = (OGRERR_NONE == err);
}

/************************************************************************/
/*                         ClearQueryStatement()                        */
/************************************************************************/

void OGRHanaLayer::ClearQueryStatement()
{
    queryStatement_.clear();
}

/************************************************************************/
/*                          GetQueryStatement()                         */
/************************************************************************/

const CPLString& OGRHanaLayer::GetQueryStatement()
{
    if (!queryStatement_.empty())
        return queryStatement_;

    EnsureInitialized();

    if (!geomColumns_.empty())
    {
        std::vector<CPLString> columns;
        for (const GeometryColumnDescription& geometryColumnDesc : geomColumns_)
            columns.push_back(
                QuotedIdentifier(geometryColumnDesc.name) + ".ST_AsBinary() AS "
                + QuotedIdentifier(geometryColumnDesc.name));

        for (const AttributeColumnDescription& attributeColumnDesc :
             attrColumns_)
            columns.push_back(QuotedIdentifier(attributeColumnDesc.name));

        queryStatement_ = CPLString().Printf(
            "SELECT %s FROM (%s) %s", JoinStrings(columns, ", ").c_str(),
            rawQuery_.c_str(), whereClause_.c_str());
    }
    else
    {
        if (whereClause_.empty())
            queryStatement_ = rawQuery_;
        else
            queryStatement_ = CPLString().Printf(
                "SELECT * FROM (%s) %s", rawQuery_.c_str(),
                whereClause_.c_str());
    }

    return queryStatement_;
}

/************************************************************************/
/*                         BuildWhereClause()                           */
/************************************************************************/

void OGRHanaLayer::BuildWhereClause()
{
    whereClause_ = "";

    CPLString spatialFilter;
    if (m_poFilterGeom != nullptr)
    {
        EnsureInitialized();

        OGRGeomFieldDefn* geomFieldDefn = nullptr;
        if( featureDefn_->GetGeomFieldCount() != 0 )
            geomFieldDefn = featureDefn_->GetGeomFieldDefn(m_iGeomFieldFilter);

        if ( geomFieldDefn != nullptr)
        {
            const GeometryColumnDescription& geomClmDesc =
                geomColumns_[static_cast<std::size_t>(m_iGeomFieldFilter)];
            spatialFilter = BuildSpatialFilter(dataSource_->GetMajorVersion(), *m_poFilterGeom, geomClmDesc.name, geomClmDesc.srid);
        }
    }

    if (!attrFilter_.empty())
    {
        whereClause_ = " WHERE " + attrFilter_;
        if (!spatialFilter.empty())
            whereClause_ += " AND " + spatialFilter;
    }
    else if (!spatialFilter.empty())
        whereClause_ = " WHERE " + spatialFilter;
}

/************************************************************************/
/*                       EnsureBufferCapacity()                         */
/************************************************************************/

void OGRHanaLayer::EnsureBufferCapacity(std::size_t size)
{
    if (size > dataBuffer_.size())
        dataBuffer_.resize(size);
}

/************************************************************************/
/*                         GetNextFeatureInternal()                     */
/************************************************************************/

OGRFeature* OGRHanaLayer::GetNextFeatureInternal()
{
    if (nextFeatureId_ == 0)
    {
        const CPLString& queryStatement = GetQueryStatement();
        CPLAssert(!queryStatement.empty());

        try
        {
            odbc::StatementRef stmt = dataSource_->CreateStatement();
            resultSet_ = stmt->executeQuery(queryStatement.c_str());
        }
        catch (const odbc::Exception& ex)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to execute query : %s", ex.what());
            return nullptr;
        }
    }

    OGRFeature* feature = ReadFeature();
    ++nextFeatureId_;

    return feature;
}

/************************************************************************/
/*                         GetGeometryColumnSrid()                      */
/************************************************************************/

int OGRHanaLayer::GetGeometryColumnSrid(int columnIndex) const
{
    if (columnIndex < 0
        || static_cast<std::size_t>(columnIndex) >= geomColumns_.size())
        return -1;
    return geomColumns_[static_cast<std::size_t>(columnIndex)].srid;
}

/************************************************************************/
/*                          ReadFeature()                               */
/************************************************************************/

OGRFeature* OGRHanaLayer::ReadFeature()
{
    if (!resultSet_->next())
        return nullptr;

    auto feature = cpl::make_unique<OGRFeature>(featureDefn_);
    feature->SetFID(nextFeatureId_);

    unsigned short paramIndex = 0;

    // Read geometries
    for (std::size_t i = 0; i < geomColumns_.size(); ++i)
    {
        ++paramIndex;
        int geomIndex = static_cast<int>(i);

        OGRGeomFieldDefn* geomFieldDef =
            featureDefn_->GetGeomFieldDefn(geomIndex);

        if (geomFieldDef->IsIgnored())
            continue;

        std::size_t bufLength = resultSet_->getBinaryLength(paramIndex);
        if (bufLength == 0 || bufLength == odbc::ResultSet::NULL_DATA)
        {
            feature->SetGeomFieldDirectly(geomIndex, nullptr);
            continue;
        }

        OGRGeometry* geom = nullptr;
        if (bufLength != odbc::ResultSet::UNKNOWN_LENGTH)
        {
            EnsureBufferCapacity(bufLength);
            resultSet_->getBinaryData(
                paramIndex, dataBuffer_.data(), bufLength);
            geom =
                CreateGeometryFromWkb(dataBuffer_.data(), dataBuffer_.size());
        }
        else
        {
            odbc::Binary wkb = resultSet_->getBinary(paramIndex);
            if (!wkb.isNull() && wkb->size() > 0)
                geom = CreateGeometryFromWkb(
                    static_cast<const void*>(wkb->data()), wkb->size());
        }

        if (geom != nullptr)
            geom->assignSpatialReference(geomFieldDef->GetSpatialRef());
        feature->SetGeomFieldDirectly(geomIndex, geom);
    }

    // Read feature attributes
    OGRHanaFeatureWriter featWriter(*feature);
    int fieldIndex = -1;
    for (const AttributeColumnDescription& clmDesc : attrColumns_)
    {
        ++paramIndex;

        if (clmDesc.isFeatureID)
        {
            if (clmDesc.type == odbc::SQLDataTypes::Integer)
            {
                odbc::Int val = resultSet_->getInt(paramIndex);
                if (!val.isNull())
                    feature->SetFID(static_cast<GIntBig>(*val));
            }
            else if (clmDesc.type == odbc::SQLDataTypes::BigInt)
            {
                odbc::Long val = resultSet_->getLong(paramIndex);
                if (!val.isNull())
                    feature->SetFID(static_cast<GIntBig>(*val));
            }
            continue;
        }

        ++fieldIndex;

        OGRFieldDefn* fieldDefn = featureDefn_->GetFieldDefn(fieldIndex);
        if (fieldDefn->IsIgnored())
            continue;

        if (clmDesc.isArray)
        {
            odbc::Binary val = resultSet_->getBinary(paramIndex);
            if (val.isNull())
            {
                feature->SetFieldNull(fieldIndex);
                continue;
            }

            switch (clmDesc.type)
            {
            case odbc::SQLDataTypes::Boolean:
                featWriter.SetFieldValueAsArray<uint8_t, int32_t>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::TinyInt:
                featWriter.SetFieldValueAsArray<uint8_t, int32_t>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::SmallInt:
                featWriter.SetFieldValueAsArray<int16_t, int32_t>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::Integer:
                featWriter.SetFieldValueAsArray<int32_t, int32_t>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::BigInt:
                featWriter.SetFieldValueAsArray<GIntBig, GIntBig>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::Float:
            case odbc::SQLDataTypes::Real:
                featWriter.SetFieldValueAsArray<float, double>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::Double:
                featWriter.SetFieldValueAsArray<double, double>(fieldIndex, val);
                break;
            case odbc::SQLDataTypes::Char:
            case odbc::SQLDataTypes::VarChar:
            case odbc::SQLDataTypes::LongVarChar:
            case odbc::SQLDataTypes::WChar:
            case odbc::SQLDataTypes::WVarChar:
            case odbc::SQLDataTypes::WLongVarChar:
                featWriter.SetFieldValueAsStringArray(fieldIndex, val);
                break;
            }

            continue;
        }

        switch (clmDesc.type)
        {
        case odbc::SQLDataTypes::Bit:
        case odbc::SQLDataTypes::Boolean: {
            odbc::Boolean val = resultSet_->getBoolean(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::TinyInt: {
            odbc::Byte val = resultSet_->getByte(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::SmallInt: {
            odbc::Short val = resultSet_->getShort(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::Integer: {
            odbc::Int val = resultSet_->getInt(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::BigInt: {
            odbc::Long val = resultSet_->getLong(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::Real:
        case odbc::SQLDataTypes::Float: {
            odbc::Float val = resultSet_->getFloat(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::Double: {
            odbc::Double val = resultSet_->getDouble(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::Decimal:
        case odbc::SQLDataTypes::Numeric: {
            odbc::Decimal val = resultSet_->getDecimal(paramIndex);
            featWriter.SetFieldValue(fieldIndex, val);
        }
        break;
        case odbc::SQLDataTypes::Char:
        case odbc::SQLDataTypes::VarChar:
        case odbc::SQLDataTypes::LongVarChar:
        // Note: NVARCHAR data type is converted to UTF-8 on the HANA side
        // when using a connection setting CHAR_AS_UTF8=1.
        case odbc::SQLDataTypes::WChar:
        case odbc::SQLDataTypes::WVarChar:
        case odbc::SQLDataTypes::WLongVarChar: {
            std::size_t len = resultSet_->getStringLength(paramIndex);
            if (len == odbc::ResultSet::NULL_DATA)
                feature->SetFieldNull(fieldIndex);
            else if (len == 0)
                feature->SetField(fieldIndex, "");
            else if (len != odbc::ResultSet::UNKNOWN_LENGTH)
            {
                EnsureBufferCapacity(len + 1);
                resultSet_->getStringData(
                    paramIndex, dataBuffer_.data(), len + 1);
                featWriter.SetFieldValue(
                    fieldIndex, dataBuffer_.data());
            }
            else
            {
                odbc::String data = resultSet_->getString(paramIndex);
                featWriter.SetFieldValue(fieldIndex, data);
            }
        }
        break;
        case odbc::SQLDataTypes::Binary:
        case odbc::SQLDataTypes::VarBinary:
        case odbc::SQLDataTypes::LongVarBinary: {
            std::size_t len = resultSet_->getBinaryLength(paramIndex);
            if (len == 0)
                feature->SetField(fieldIndex, 0, static_cast<GByte*>(nullptr));
            else if (len == odbc::ResultSet::NULL_DATA)
                feature->SetFieldNull(fieldIndex);
            else if (len != odbc::ResultSet::UNKNOWN_LENGTH)
            {
                EnsureBufferCapacity(len);
                resultSet_->getBinaryData(paramIndex, dataBuffer_.data(), len);
                featWriter.SetFieldValue(fieldIndex, dataBuffer_.data(), len);
            }
            else
            {
                odbc::Binary binData = resultSet_->getBinary(paramIndex);
                featWriter.SetFieldValue(fieldIndex, binData);
            }
        }
        break;
        case odbc::SQLDataTypes::Date:
        case odbc::SQLDataTypes::TypeDate: {
            odbc::Date date = resultSet_->getDate(paramIndex);
            featWriter.SetFieldValue(fieldIndex, date);
        }
        break;
        case odbc::SQLDataTypes::Time:
        case odbc::SQLDataTypes::TypeTime: {
            odbc::Time time = resultSet_->getTime(paramIndex);
            featWriter.SetFieldValue(fieldIndex, time);
        }
        break;
        case odbc::SQLDataTypes::Timestamp:
        case odbc::SQLDataTypes::TypeTimestamp: {
            odbc::Timestamp timestamp = resultSet_->getTimestamp(paramIndex);
            featWriter.SetFieldValue(fieldIndex, timestamp);
        }
        break;
        default:
            break;
        }
    }

    return feature.release();
}

/************************************************************************/
/*                      InitFeatureDefinition()                         */
/************************************************************************/

OGRErr OGRHanaLayer::InitFeatureDefinition(
    const CPLString& schemaName,
    const CPLString& tableName,
    const CPLString& query,
    const CPLString& featureDefName)
{
    attrColumns_.clear();
    geomColumns_.clear();
    fidFieldIndex_ = OGRNullFID;
    fidFieldName_.clear();
    featureDefn_ = new OGRFeatureDefn(featureDefName.c_str());
    featureDefn_->Reference();

    std::vector<ColumnDescription> columnDescriptions;
    OGRErr err =
        dataSource_->GetQueryColumns(schemaName, query, columnDescriptions);
    if (err != OGRERR_NONE)
        return err;

    std::vector<CPLString> primKeys =
        dataSource_->GetTablePrimaryKeys(schemaName, tableName);

    if (featureDefn_->GetGeomFieldCount() == 1)
        featureDefn_->DeleteGeomFieldDefn(0);

    for (const ColumnDescription& clmDesc : columnDescriptions)
    {
        if (clmDesc.isGeometry)
        {
            const GeometryColumnDescription& geometryColumnDesc =
                clmDesc.geometryDescription;

            auto geomFieldDefn = cpl::make_unique<OGRGeomFieldDefn>(
                        geometryColumnDesc.name.c_str(), geometryColumnDesc.type);
            geomFieldDefn->SetNullable(geometryColumnDesc.isNullable);

            if (geometryColumnDesc.srid >= 0)
            {
                OGRSpatialReference* srs =
                    dataSource_->GetSrsById(geometryColumnDesc.srid);
                geomFieldDefn->SetSpatialRef(srs);
            }
            geomColumns_.push_back(geometryColumnDesc);
            featureDefn_->AddGeomFieldDefn(std::move(geomFieldDefn));
            continue;
        }

        AttributeColumnDescription attributeColumnDesc =
            clmDesc.attributeDescription;
        auto field = CreateFieldDefn(attributeColumnDesc);

        if ((field->GetType() == OFTInteger || field->GetType() == OFTInteger64)
            && (fidFieldIndex_ == OGRNullFID && primKeys.size() > 0))
        {
            for (const CPLString& key : primKeys)
            {
                if (key.compare(attributeColumnDesc.name) == 0)
                {
                    fidFieldIndex_ = static_cast<int>(attrColumns_.size());
                    fidFieldName_ = field->GetNameRef();
                    attributeColumnDesc.isFeatureID = true;
                    break;
                }
            }
        }

        if (!attributeColumnDesc.isFeatureID)
            featureDefn_->AddFieldDefn(field.get());
        attrColumns_.push_back(attributeColumnDesc);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         ReadGeometryExtent()                         */
/************************************************************************/

void OGRHanaLayer::ReadGeometryExtent(int geomField, OGREnvelope* extent)
{
    EnsureInitialized();

    OGRGeomFieldDefn* geomFieldDef = featureDefn_->GetGeomFieldDefn(geomField);
    const char* clmName = geomFieldDef->GetNameRef();
    int srid = GetGeometryColumnSrid(geomField);
    CPLString sql;
    if (dataSource_->IsSrsRoundEarth(srid))
    {
        CPLString quotedClmName = QuotedIdentifier(clmName);
        bool hasSrsPlanarEquivalent = dataSource_->HasSrsPlanarEquivalent(srid);
        CPLString geomColumn = !hasSrsPlanarEquivalent
                                   ? quotedClmName
                                   : CPLString().Printf(
                                       "%s.ST_SRID(%d)", quotedClmName.c_str(),
                                       ToPlanarSRID(srid));
        CPLString columns = CPLString().Printf(
            "MIN(%s.ST_XMin()), MIN(%s.ST_YMin()), MAX(%s.ST_XMax()), "
            "MAX(%s.ST_YMax())",
            geomColumn.c_str(), geomColumn.c_str(), geomColumn.c_str(),
            geomColumn.c_str());
        sql = BuildQuery(rawQuery_.c_str(), columns.c_str());
    }
    else
    {
        CPLString columns = CPLString().Printf(
            "ST_EnvelopeAggr(%s) AS ext", QuotedIdentifier(clmName).c_str());
        CPLString subQuery = BuildQuery(rawQuery_.c_str(), columns);
        sql = CPLString().Printf(
            "SELECT ext.ST_XMin(),ext.ST_YMin(),ext.ST_XMax(),ext.ST_YMax() "
            "FROM (%s)",
            subQuery.c_str());
    }

    extent->MinX = 0.0;
    extent->MaxX = 0.0;
    extent->MinY = 0.0;
    extent->MaxY = 0.0;

    odbc::StatementRef stmt = dataSource_->CreateStatement();
    odbc::ResultSetRef rsExtent = stmt->executeQuery(sql.c_str());
    if (rsExtent->next())
    {
        odbc::Double val = rsExtent->getDouble(1);
        if (!val.isNull())
        {
            extent->MinX = *val;
            extent->MinY = *rsExtent->getDouble(2);
            extent->MaxX = *rsExtent->getDouble(3);
            extent->MaxY = *rsExtent->getDouble(4);
        }
    }
    rsExtent->close();
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRHanaLayer::ResetReading()
{
    nextFeatureId_ = 0;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRHanaLayer::GetExtent(int iGeomField, OGREnvelope* extent, int force)
{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        extent->MinX = 0.0;
        extent->MaxX = 0.0;
        extent->MinY = 0.0;
        extent->MaxY = 0.0;

        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    try
    {
        ReadGeometryExtent(iGeomField, extent);
        return OGRERR_NONE;
    }
    catch (const std::exception& ex)
    {
        CPLString clmName =
            (iGeomField < static_cast<int>(geomColumns_.size()))
                ? geomColumns_[static_cast<std::size_t>(
                                   geomColumns_.size())].name
                : "unknown column";
        CPLError(CE_Failure, CPLE_AppDefined,
            "Unable to query extent of '%s' using fast method: %s",
            clmName.c_str(), ex.what());
    }

    if( iGeomField == 0 )
        return OGRLayer::GetExtent( extent, force );
    else
        return OGRLayer::GetExtent( iGeomField, extent, force);
}

/************************************************************************/
/*                            GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRHanaLayer::GetFeatureCount(CPL_UNUSED int force)
{
    EnsureInitialized();

    GIntBig ret = 0;
    CPLString sql = CPLString().Printf(
        "SELECT COUNT(*) FROM (%s) AS tmp", GetQueryStatement().c_str());
    odbc::StatementRef stmt = dataSource_->CreateStatement();
    odbc::ResultSetRef rs = stmt->executeQuery(sql.c_str());
    if (rs->next())
        ret = *rs->getLong(1);
    rs->close();
    return ret;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn* OGRHanaLayer::GetLayerDefn()
{
    EnsureInitialized();
    return featureDefn_;
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char *OGRHanaLayer::GetName()
{
    return GetDescription();
}

/************************************************************************/
/*                         GetNextFeature()                             */
/************************************************************************/

OGRFeature* OGRHanaLayer::GetNextFeature()
{
    EnsureInitialized();

    while (true)
    {
        OGRFeature* feature = GetNextFeatureInternal();
        if (feature == nullptr)
            return nullptr;

        if ((m_poFilterGeom == nullptr
             || FilterGeometry(feature->GetGeometryRef()))
            && (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(feature)))
            return feature;

        delete feature;
    }
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char* OGRHanaLayer::GetFIDColumn()
{
    EnsureInitialized();
    return fidFieldName_.c_str();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRHanaLayer::SetAttributeFilter(const char* pszQuery)
{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = pszQuery ? CPLStrdup(pszQuery) : nullptr;

    if (pszQuery == nullptr || strlen(pszQuery) == 0)
        attrFilter_ = "";
    else
        attrFilter_.assign(pszQuery, strlen(pszQuery));

    ClearQueryStatement();
    BuildWhereClause();
    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRHanaLayer::SetSpatialFilter(int iGeomField, OGRGeometry* poGeom)
{
    m_iGeomFieldFilter = 0;

    if (iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid geometry field index : %d", iGeomField);
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    if (!InstallFilter(poGeom))
        return;

    ClearQueryStatement();
    BuildWhereClause();
    ResetReading();
}

} /* end of OGRHANA namespace */
