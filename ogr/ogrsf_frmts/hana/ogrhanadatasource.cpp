/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDataSource class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_hana.h"
#include "ogrhanautils.h"
#include "ogrhanadrivercore.h"

#include "odbc/Connection.h"
#include "odbc/Environment.h"
#include "odbc/Exception.h"
#include "odbc/DatabaseMetaData.h"
#include "odbc/PreparedStatement.h"
#include "odbc/ResultSet.h"
#include "odbc/ResultSetMetaData.h"
#include "odbc/Statement.h"
#include "odbc/StringConverter.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

using namespace OGRHANA;

namespace
{

CPLString BuildConnectionString(char **openOptions)
{
    // See notes for constructing connection string for HANA
    // https://help.sap.com/docs/SAP_HANA_CLIENT/f1b440ded6144a54ada97ff95dac7adf/7cab593774474f2f8db335710b2f5c50.html

    std::vector<CPLString> params;
    bool isValid = true;
    const CPLString specialChars("[]{}(),;?*=!@");

    auto getOptValue = [&](const char *optionName, bool mandatory = false)
    {
        const char *paramValue =
            CSLFetchNameValueDef(openOptions, optionName, nullptr);
        if (mandatory && paramValue == nullptr)
        {
            isValid = false;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Mandatory connection parameter '%s' is missing.",
                     optionName);
        }
        return paramValue;
    };

    auto addParameter = [&](const char *paramName, const char *paramValue)
    {
        if (paramValue == nullptr)
            return;

        CPLString value(paramValue);
        if (value.find_first_of(specialChars) != std::string::npos)
        {
            value.replaceAll("}", "}}");
            params.push_back(CPLString(paramName) + "={" + value + "}");
        }
        else
        {
            params.push_back(CPLString(paramName) + "=" + value);
        }
    };

    auto addOptParameter = [&](const char *optionName, const char *paramName,
                               bool mandatory = false)
    {
        const char *paramValue = getOptValue(optionName, mandatory);
        addParameter(paramName, paramValue);
    };

    auto checkIgnoredOptParameter = [&](const char *optionName)
    {
        if (getOptValue(optionName))
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Connection parameter '%s' is ignored in the current "
                     "combination.",
                     optionName);
    };

    if (const char *paramUserStoreKey =
            getOptValue(OGRHanaOpenOptionsConstants::USER_STORE_KEY))
    {
        addOptParameter(OGRHanaOpenOptionsConstants::DRIVER, "DRIVER", true);
        CPLString node = CPLString().Printf("@%s", paramUserStoreKey);
        addParameter("SERVERNODE", node.c_str());

        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::DSN);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::HOST);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::PORT);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::DATABASE);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::USER);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::PASSWORD);
    }
    else if (const char *paramDSN =
                 getOptValue(OGRHanaOpenOptionsConstants::DSN))
    {
        addParameter(OGRHanaOpenOptionsConstants::DSN, paramDSN);
        addOptParameter(OGRHanaOpenOptionsConstants::USER, "UID", true);
        addOptParameter(OGRHanaOpenOptionsConstants::PASSWORD, "PWD", true);

        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::DRIVER);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::HOST);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::PORT);
        checkIgnoredOptParameter(OGRHanaOpenOptionsConstants::DATABASE);
    }
    else
    {
        addOptParameter(OGRHanaOpenOptionsConstants::DRIVER, "DRIVER", true);
        const char *paramHost =
            getOptValue(OGRHanaOpenOptionsConstants::HOST, true);
        const char *paramPort =
            getOptValue(OGRHanaOpenOptionsConstants::PORT, true);
        if (paramHost != nullptr && paramPort != nullptr)
        {
            CPLString node = CPLString().Printf("%s:%s", paramHost, paramPort);
            addParameter("SERVERNODE", node.c_str());
        }
        addOptParameter(OGRHanaOpenOptionsConstants::USER, "UID", true);
        addOptParameter(OGRHanaOpenOptionsConstants::PASSWORD, "PWD", true);
        addOptParameter(OGRHanaOpenOptionsConstants::DATABASE, "DATABASENAME");
    }

    if (const char *paramSchema =
            getOptValue(OGRHanaOpenOptionsConstants::SCHEMA, true))
    {
        CPLString schema = CPLString().Printf("\"%s\"", paramSchema);
        addParameter("CURRENTSCHEMA", schema.c_str());
    }

    if (CPLFetchBool(openOptions, OGRHanaOpenOptionsConstants::ENCRYPT, false))
    {
        addOptParameter(OGRHanaOpenOptionsConstants::ENCRYPT, "ENCRYPT");
        addOptParameter(OGRHanaOpenOptionsConstants::SSL_CRYPTO_PROVIDER,
                        "sslCryptoProvider");
        addOptParameter(OGRHanaOpenOptionsConstants::SSL_KEY_STORE,
                        "sslKeyStore");
        addOptParameter(OGRHanaOpenOptionsConstants::SSL_TRUST_STORE,
                        "sslTrustStore");
        addOptParameter(OGRHanaOpenOptionsConstants::SSL_VALIDATE_CERTIFICATE,
                        "sslValidateCertificate");
        addOptParameter(OGRHanaOpenOptionsConstants::SSL_HOST_NAME_CERTIFICATE,
                        "sslHostNameInCertificate");
    }

    addOptParameter(OGRHanaOpenOptionsConstants::PACKET_SIZE, "PACKETSIZE");
    addOptParameter(OGRHanaOpenOptionsConstants::SPLIT_BATCH_COMMANDS,
                    "SPLITBATCHCOMMANDS");
    addParameter("CHAR_AS_UTF8", "1");

    CPLString appName;
    appName.Printf("GDAL %s", GDALVersionInfo("RELEASE_NAME"));
    addParameter("sessionVariable:APPLICATION", appName.c_str());

    return isValid ? JoinStrings(params, ";") : "";
}

int CPLFetchInt(CSLConstList papszStrList, const char *pszKey, int defaultValue)
{
    const char *const pszValue = CSLFetchNameValue(papszStrList, pszKey);
    if (pszValue == nullptr)
        return defaultValue;
    return atoi(pszValue);
}

int GetSrid(odbc::ResultSet &resultSet)
{
    int srid = UNDETERMINED_SRID;
    while (resultSet.next())
    {
        odbc::Int val = resultSet.getInt(1);
        if (!val.isNull())
        {
            srid = *val;
            break;
        }
    }
    resultSet.close();
    return srid;
}

int GetColumnSrid(odbc::Connection &conn, const CPLString &schemaName,
                  const CPLString &tableName, const CPLString &columnName)
{
    CPLString sql =
        "SELECT SRS_ID FROM SYS.ST_GEOMETRY_COLUMNS WHERE SCHEMA_NAME = ?"
        " AND TABLE_NAME = ? AND COLUMN_NAME = ?";
    odbc::PreparedStatementRef stmt = conn.prepareStatement(sql.c_str());
    stmt->setString(1, odbc::String(schemaName));
    stmt->setString(2, odbc::String(tableName));
    if (columnName != nullptr)
        stmt->setString(3, odbc::String(columnName));
    return GetSrid(*stmt->executeQuery());
}

int GetColumnSrid(odbc::Connection &conn, const CPLString &query,
                  const CPLString &columnName)
{
    CPLString clmName = QuotedIdentifier(columnName);

    CPLString sql =
        CPLString().Printf("SELECT %s.ST_SRID() FROM (%s) WHERE %s IS NOT NULL",
                           clmName.c_str(), query.c_str(), clmName.c_str());

    odbc::StatementRef stmt = conn.createStatement();
    return GetSrid(*stmt->executeQuery(sql.c_str()));
}

int GetSridWithFilter(odbc::Connection &conn, const CPLString &whereCondition)
{
    CPLAssert(whereCondition != nullptr);

    int ret = UNDETERMINED_SRID;

    odbc::StatementRef stmt = conn.createStatement();
    CPLString sql = CPLString().Printf(
        "SELECT SRS_ID FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS WHERE %s",
        whereCondition.c_str());
    odbc::ResultSetRef rsSrs = stmt->executeQuery(sql.c_str());
    while (rsSrs->next())
    {
        odbc::Int val = rsSrs->getInt(1);
        if (!val.isNull())
        {
            ret = *val;
            break;
        }
    }
    rsSrs->close();

    return ret;
}

CPLString GetSrsWktById(odbc::Connection &conn, int srid)
{
    CPLString ret;
    const char *sql = "SELECT DEFINITION FROM "
                      "SYS.ST_SPATIAL_REFERENCE_SYSTEMS WHERE SRS_ID = ?";
    odbc::PreparedStatementRef stmt = conn.prepareStatement(sql);
    stmt->setInt(1, odbc::Int(srid));
    odbc::ResultSetRef rs = stmt->executeQuery();
    while (rs->next())
    {
        odbc::String wkt = rs->getString(1);
        if (!wkt.isNull())
        {
            ret = *wkt;
            if (!ret.empty())
                break;
        }
    }
    rs->close();

    return ret;
}

OGRwkbGeometryType GetGeometryType(odbc::Connection &conn,
                                   const CPLString &query,
                                   const CPLString &columnName)
{
    CPLString clmName = QuotedIdentifier(columnName);

    CPLString sql = CPLString().Printf(
        "SELECT DISTINCT UPPER(%s.ST_GeometryType()), %s.ST_Is3D(), "
        "%s.ST_IsMeasured() FROM %s WHERE %s IS NOT NULL",
        clmName.c_str(), clmName.c_str(), clmName.c_str(), query.c_str(),
        clmName.c_str());

    odbc::StatementRef stmt = conn.createStatement();
    odbc::ResultSetRef rsGeomInfo = stmt->executeQuery(sql.c_str());
    OGRwkbGeometryType ret = wkbUnknown;
    std::size_t i = 0;
    while (rsGeomInfo->next())
    {
        ++i;
        auto typeName = rsGeomInfo->getString(1);
        auto hasZ = rsGeomInfo->getInt(2);
        auto hasM = rsGeomInfo->getInt(3);
        OGRwkbGeometryType geomType =
            ToWkbType(typeName->c_str(), *hasZ == 1, *hasM == 1);
        if (geomType == OGRwkbGeometryType::wkbUnknown)
            continue;
        if (ret == OGRwkbGeometryType::wkbUnknown)
            ret = geomType;
        else if (ret != geomType)
        {
            ret = OGRwkbGeometryType::wkbUnknown;
            break;
        }
    }
    rsGeomInfo->close();

    if (i == 0)
        ret = OGRwkbGeometryType::wkbUnknown;
    return ret;
}

GeometryColumnDescription GetGeometryColumnDescription(
    odbc::Connection &conn, const CPLString &schemaName,
    const CPLString &tableName, const CPLString &columnName,
    bool detectGeometryType)
{
    OGRwkbGeometryType type =
        detectGeometryType
            ? GetGeometryType(conn,
                              GetFullTableNameQuoted(schemaName, tableName),
                              columnName)
            : OGRwkbGeometryType::wkbUnknown;
    int srid = GetColumnSrid(conn, schemaName, tableName, columnName);

    return {columnName, type, srid, false};
}

GeometryColumnDescription
GetGeometryColumnDescription(odbc::Connection &conn, const CPLString &query,
                             const CPLString &columnName,
                             bool detectGeometryType)
{
    // For some queries like this SELECT ST_GeomFROMWKT('POINT(0 0)') FROM DUMMY
    // we need to have a proper column name.
    bool needColumnName = false;
    std::vector<char> specialChars = {'(', ')', '\'', ' '};
    for (const char c : specialChars)
    {
        if (columnName.find(c) != CPLString::npos)
        {
            needColumnName = true;
            break;
        }
    }

    CPLString preparedQuery = query;
    CPLString clmName = columnName;
    if (needColumnName)
    {
        auto it = std::search(
            preparedQuery.begin(), preparedQuery.end(), columnName.begin(),
            columnName.end(),
            [](char ch1, char ch2)
            {
                return CPLToupper(static_cast<unsigned char>(ch1)) ==
                       CPLToupper(static_cast<unsigned char>(ch2));
            });

        if (it != preparedQuery.end())
        {
            auto pos = it - preparedQuery.begin();
            CPLString newName = columnName + " AS \"tmp_geom_field\"";
            preparedQuery.replace(static_cast<std::size_t>(pos),
                                  columnName.length(), newName, 0,
                                  newName.length());
            clmName = "tmp_geom_field";
        }
    }

    OGRwkbGeometryType type =
        detectGeometryType
            ? GetGeometryType(conn, "(" + preparedQuery + ")", clmName)
            : OGRwkbGeometryType::wkbUnknown;
    int srid = GetColumnSrid(conn, preparedQuery, clmName);

    return {columnName, type, srid, false};
}

CPLString FormatDefaultValue(const char *value, short dataType)
{
    /*
     The values that can be set as default values are :
       - literal string values enclosed in single-quote characters and properly
     escaped like: 'Nice weather. Isn''t it ?'
       - numeric values (unquoted)
       - reserved keywords (unquoted): CURRENT_TIMESTAMP, CURRENT_DATE,
     CURRENT_TIME, NULL
       - datetime literal values enclosed in single-quote characters with the
     following defined format: ‘YYYY/MM/DD HH:MM:SS[.sss]’
       - any other driver specific expression. e.g. for SQLite:
     (strftime(‘%Y-%m-%dT%H:%M:%fZ’,’now’))
     */

    if (EQUAL(value, "NULL"))
        return value;

    switch (dataType)
    {
        case QGRHanaDataTypes::Bit:
        case QGRHanaDataTypes::Boolean:
            return value;
        case QGRHanaDataTypes::TinyInt:
        case QGRHanaDataTypes::SmallInt:
        case QGRHanaDataTypes::Integer:
        case QGRHanaDataTypes::BigInt:
        case QGRHanaDataTypes::Real:
        case QGRHanaDataTypes::Float:
        case QGRHanaDataTypes::Double:
        case QGRHanaDataTypes::Decimal:
        case QGRHanaDataTypes::Numeric:
            return value;
        case QGRHanaDataTypes::Char:
        case QGRHanaDataTypes::VarChar:
        case QGRHanaDataTypes::LongVarChar:
        case QGRHanaDataTypes::WChar:
        case QGRHanaDataTypes::WVarChar:
        case QGRHanaDataTypes::WLongVarChar:
            return Literal(value);
        case QGRHanaDataTypes::Binary:
        case QGRHanaDataTypes::VarBinary:
        case QGRHanaDataTypes::LongVarBinary:
        case QGRHanaDataTypes::RealVector:
            return value;
        case QGRHanaDataTypes::Date:
        case QGRHanaDataTypes::TypeDate:
            if (EQUAL(value, "CURRENT_DATE"))
                return value;
            return Literal(value);
        case QGRHanaDataTypes::Time:
        case QGRHanaDataTypes::TypeTime:
            if (EQUAL(value, "CURRENT_TIME"))
                return value;
            return Literal(value);
        case QGRHanaDataTypes::Timestamp:
        case QGRHanaDataTypes::TypeTimestamp:
            if (EQUAL(value, "CURRENT_TIMESTAMP"))
                return value;
            return Literal(value);
        default:
            return value;
    }
}

short GetArrayDataType(const CPLString &typeName)
{
    if (typeName == "BOOLEAN ARRAY")
        return QGRHanaDataTypes::Boolean;
    else if (typeName == "TINYINT ARRAY")
        return QGRHanaDataTypes::TinyInt;
    else if (typeName == "SMALLINT ARRAY")
        return QGRHanaDataTypes::SmallInt;
    else if (typeName == "INTEGER ARRAY")
        return QGRHanaDataTypes::Integer;
    else if (typeName == "BIGINT ARRAY")
        return QGRHanaDataTypes::BigInt;
    else if (typeName == "DOUBLE ARRAY")
        return QGRHanaDataTypes::Double;
    else if (typeName == "REAL ARRAY")
        return QGRHanaDataTypes::Float;
    else if (typeName == "DECIMAL ARRAY" || typeName == "SMALLDECIMAL ARRAY")
        return QGRHanaDataTypes::Decimal;
    else if (typeName == "CHAR ARRAY")
        return QGRHanaDataTypes::Char;
    else if (typeName == "VARCHAR ARRAY")
        return QGRHanaDataTypes::VarChar;
    else if (typeName == "NCHAR ARRAY")
        return QGRHanaDataTypes::WChar;
    else if (typeName == "NVARCHAR ARRAY")
        return QGRHanaDataTypes::WVarChar;
    else if (typeName == "DATE ARRAY")
        return QGRHanaDataTypes::Date;
    else if (typeName == "TIME ARRAY")
        return QGRHanaDataTypes::Time;
    else if (typeName == "TIMESTAMP ARRAY" || typeName == "SECONDDATE ARRAY")
        return QGRHanaDataTypes::Timestamp;

    return QGRHanaDataTypes::Unknown;
}

std::vector<CPLString> GetSupportedArrayTypes()
{
    return {"TINYINT", "SMALLINT", "INT", "BIGINT", "REAL", "DOUBLE", "STRING"};
}

bool IsKnownDataType(short dataType)
{
    return dataType == QGRHanaDataTypes::Bit ||
           dataType == QGRHanaDataTypes::Boolean ||
           dataType == QGRHanaDataTypes::TinyInt ||
           dataType == QGRHanaDataTypes::SmallInt ||
           dataType == QGRHanaDataTypes::Integer ||
           dataType == QGRHanaDataTypes::BigInt ||
           dataType == QGRHanaDataTypes::Double ||
           dataType == QGRHanaDataTypes::Real ||
           dataType == QGRHanaDataTypes::Float ||
           dataType == QGRHanaDataTypes::Decimal ||
           dataType == QGRHanaDataTypes::Numeric ||
           dataType == QGRHanaDataTypes::Char ||
           dataType == QGRHanaDataTypes::VarChar ||
           dataType == QGRHanaDataTypes::LongVarChar ||
           dataType == QGRHanaDataTypes::WChar ||
           dataType == QGRHanaDataTypes::WVarChar ||
           dataType == QGRHanaDataTypes::WLongVarChar ||
           dataType == QGRHanaDataTypes::Date ||
           dataType == QGRHanaDataTypes::TypeDate ||
           dataType == QGRHanaDataTypes::Time ||
           dataType == QGRHanaDataTypes::TypeTime ||
           dataType == QGRHanaDataTypes::Timestamp ||
           dataType == QGRHanaDataTypes::TypeTimestamp ||
           dataType == QGRHanaDataTypes::Binary ||
           dataType == QGRHanaDataTypes::VarBinary ||
           dataType == QGRHanaDataTypes::LongVarBinary ||
           dataType == QGRHanaDataTypes::Geometry ||
           dataType == QGRHanaDataTypes::RealVector;
}

}  // anonymous namespace

/************************************************************************/
/*                               GetPrefix()                            */
/************************************************************************/

const char *OGRHanaDataSource::GetPrefix()
{
    return HANA_PREFIX;
}

/************************************************************************/
/*                         OGRHanaDataSource()                          */
/************************************************************************/

OGRHanaDataSource::OGRHanaDataSource()
{
}

/************************************************************************/
/*                        ~OGRHanaDataSource()                          */
/************************************************************************/

OGRHanaDataSource::~OGRHanaDataSource()
{
    layers_.clear();

    for (const auto &kv : srsCache_)
    {
        OGRSpatialReference *srs = kv.second;
        if (srs != nullptr)
            srs->Release();
    }
    srsCache_.clear();
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

int OGRHanaDataSource::Open(const char *newName, char **openOptions, int update)
{
    CPLAssert(layers_.size() == 0);

    if (!STARTS_WITH_CI(newName, GetPrefix()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s does not conform to HANA driver naming convention,"
                 " %s*\n",
                 newName, GetPrefix());
        return FALSE;
    }

    updateMode_ = update;
    detectGeometryType_ = CPLFetchBool(
        openOptions, OGRHanaOpenOptionsConstants::DETECT_GEOMETRY_TYPE, true);

    std::size_t prefixLength = strlen(GetPrefix());
    char **connOptions =
        CSLTokenizeStringComplex(newName + prefixLength, ";", TRUE, FALSE);

    const char *paramSchema = CSLFetchNameValueDef(
        connOptions, OGRHanaOpenOptionsConstants::SCHEMA, nullptr);
    if (paramSchema != nullptr)
        schemaName_ = paramSchema;

    int ret = FALSE;

    CPLString connectionStr = BuildConnectionString(connOptions);

    if (!connectionStr.empty())
    {
        connEnv_ = odbc::Environment::create();
        conn_ = connEnv_->createConnection();
        conn_->setAutoCommit(false);

        const char *paramConnTimeout = CSLFetchNameValueDef(
            connOptions, OGRHanaOpenOptionsConstants::CONNECTION_TIMEOUT,
            nullptr);
        if (paramConnTimeout != nullptr)
            conn_->setConnectionTimeout(
                static_cast<unsigned long>(atoi(paramConnTimeout)));

        try
        {
            conn_->connect(connectionStr.c_str());
        }
        catch (const odbc::Exception &ex)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HANA connection failed: %s\n", ex.what());
        }

        if (conn_->connected())
        {
            DetermineVersions();

            const char *paramTables = CSLFetchNameValueDef(
                connOptions, OGRHanaOpenOptionsConstants::TABLES, "");
            InitializeLayers(paramSchema, paramTables);
            ret = TRUE;
        }
    }

    CSLDestroy(connOptions);

    return ret;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRHanaDataSource::DeleteLayer(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size())
        return OGRERR_FAILURE;

    const std::unique_ptr<OGRLayer> &layer =
        layers_[static_cast<std::size_t>(index)];
    CPLDebug("HANA", "DeleteLayer(%s)", layer->GetName());

    if (auto tableLayer = dynamic_cast<OGRHanaTableLayer *>(layer.get()))
    {
        OGRErr err = tableLayer->DropTable();
        if (OGRERR_NONE == err)
            return err;
    }

    layers_.erase(layers_.begin() + index);

    return OGRERR_NONE;
}

void OGRHanaDataSource::CreateTable(
    const CPLString &tableName, const CPLString &fidName,
    const CPLString &fidType, const CPLString &geomColumnName,
    OGRwkbGeometryType geomType, bool geomColumnNullable,
    const CPLString &geomColumnIndexType, int geomSrid)
{
    CPLString sql;
    if (geomType == OGRwkbGeometryType::wkbNone ||
        !(!geomColumnName.empty() && geomSrid >= 0))
    {
        sql = "CREATE COLUMN TABLE " +
              GetFullTableNameQuoted(schemaName_, tableName) + " (" +
              QuotedIdentifier(fidName) + " " + fidType +
              " GENERATED BY DEFAULT AS IDENTITY, PRIMARY KEY ( " +
              QuotedIdentifier(fidName) + "));";
    }
    else
    {
        sql = "CREATE COLUMN TABLE " +
              GetFullTableNameQuoted(schemaName_, tableName) + " (" +
              QuotedIdentifier(fidName) + " " + fidType +
              " GENERATED BY DEFAULT AS IDENTITY, " +
              QuotedIdentifier(geomColumnName) + " ST_GEOMETRY (" +
              std::to_string(geomSrid) + ")" +
              (geomColumnNullable ? "" : " NOT NULL") +
              " SPATIAL INDEX PREFERENCE " + geomColumnIndexType +
              ", PRIMARY KEY ( " + QuotedIdentifier(fidName) + "));";
    }

    ExecuteSQL(sql);
}

void OGRHanaDataSource::DetermineVersions()
{
    odbc::DatabaseMetaDataRef dbmd = conn_->getDatabaseMetaData();
    CPLString dbVersion(dbmd->getDBMSVersion());
    hanaVersion_ = HanaVersion::fromString(dbVersion);

    if (hanaVersion_.major() < 4)
    {
        cloudVersion_ = HanaVersion(0, 0, 0);
        return;
    }

    odbc::StatementRef stmt = conn_->createStatement();
    const char *sql = "SELECT CLOUD_VERSION FROM SYS.M_DATABASE;";

    odbc::ResultSetRef rsVersion = stmt->executeQuery(sql);
    if (rsVersion->next())
        cloudVersion_ =
            HanaVersion::fromString(rsVersion->getString(1)->c_str());

    rsVersion->close();
}

/************************************************************************/
/*                            FindSchemaAndTableNames()                 */
/************************************************************************/

std::pair<CPLString, CPLString>
OGRHanaDataSource::FindSchemaAndTableNames(const char *query)
{
    odbc::PreparedStatementRef stmt = PrepareStatement(query);
    if (stmt.get() == nullptr)
        return {"", ""};

    odbc::ResultSetMetaDataRef rsmd = stmt->getMetaData();

    // Note, getTableName returns correct table name also in the case
    // when the original sql query uses a view
    CPLString tableName = rsmd->getTableName(1);
    if (tableName == "M_DATABASE_")
        tableName = "M_DATABASE";
    CPLString schemaName = rsmd->getSchemaName(1);
    if (schemaName.empty() && !tableName.empty())
        schemaName = FindSchemaName(tableName.c_str());
    return {schemaName, tableName};
}

/************************************************************************/
/*                            FindLayerByName()                         */
/************************************************************************/

int OGRHanaDataSource::FindLayerByName(const char *name)
{
    for (size_t i = 0; i < layers_.size(); ++i)
    {
        if (EQUAL(name, layers_[i]->GetName()))
            return static_cast<int>(i);
    }
    return -1;
}

/************************************************************************/
/*                            FindSchemaName()                          */
/************************************************************************/

CPLString OGRHanaDataSource::FindSchemaName(const char *objectName)
{
    auto getSchemaName = [&](const char *sql)
    {
        odbc::PreparedStatementRef stmt = PrepareStatement(sql);
        stmt->setString(1, odbc::String(objectName));
        odbc::ResultSetRef rsEntries = stmt->executeQuery();
        CPLString ret;
        while (rsEntries->next())
        {
            // return empty string if there is more than one schema.
            if (!ret.empty())
            {
                ret.clear();
                break;
            }
            ret = *rsEntries->getString(1);
        }
        rsEntries->close();

        return ret;
    };

    CPLString ret = getSchemaName(
        "SELECT SCHEMA_NAME FROM SYS.TABLES WHERE TABLE_NAME = ?");
    if (ret.empty())
        ret = getSchemaName(
            "SELECT SCHEMA_NAME FROM SYS.VIEWS WHERE VIEW_NAME = ?");

    return ret;
}

/************************************************************************/
/*                              CreateStatement()                       */
/************************************************************************/

odbc::StatementRef OGRHanaDataSource::CreateStatement()
{
    return conn_->createStatement();
}

/************************************************************************/
/*                              PrepareStatement()                      */
/************************************************************************/

odbc::PreparedStatementRef OGRHanaDataSource::PrepareStatement(const char *sql)
{
    CPLAssert(sql != nullptr);

    try
    {
        CPLDebug("HANA", "Prepare statement %s.", sql);

        std::u16string sqlUtf16 = odbc::StringConverter::utf8ToUtf16(sql);
        return conn_->prepareStatement(sqlUtf16.c_str());
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to prepare statement: %s",
                 ex.what());
    }
    return nullptr;
}

/************************************************************************/
/*                              Commit()                                */
/************************************************************************/

void OGRHanaDataSource::Commit()
{
    conn_->commit();
}

/************************************************************************/
/*                            ExecuteSQL()                              */
/************************************************************************/

void OGRHanaDataSource::ExecuteSQL(const CPLString &sql)
{
    std::u16string sqlUtf16 =
        odbc::StringConverter::utf8ToUtf16(sql.c_str(), sql.length());
    odbc::StatementRef stmt = conn_->createStatement();
    stmt->execute(sqlUtf16.c_str());
    if (!IsTransactionStarted())
        conn_->commit();
}

/************************************************************************/
/*                            GetSrsById()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  The returned    */
/*      object has its reference counter incremented. Consequently      */
/*      the caller should call Release() on it (if not null) once done  */
/*      with it.                                                        */
/************************************************************************/

OGRSpatialReference *OGRHanaDataSource::GetSrsById(int srid)
{
    if (srid < 0)
        return nullptr;

    auto it = srsCache_.find(srid);
    if (it != srsCache_.end())
    {
        it->second->Reference();
        return it->second;
    }

    OGRSpatialReference *srs = nullptr;

    CPLString wkt = GetSrsWktById(*conn_, srid);
    if (!wkt.empty())
    {
        srs = new OGRSpatialReference();
        OGRErr err = srs->importFromWkt(wkt.c_str());
        if (OGRERR_NONE != err)
        {
            delete srs;
            srs = nullptr;
        }
    }

    srsCache_.insert({srid, srs});

    if (srs)
        srs->Reference();
    return srs;
}

/************************************************************************/
/*                               GetSrsId()                             */
/************************************************************************/

int OGRHanaDataSource::GetSrsId(const OGRSpatialReference *srs)
{
    if (srs == nullptr)
        return UNDETERMINED_SRID;

    /* -------------------------------------------------------------------- */
    /*      Try to find srs id using authority name and code (EPSG:3857).   */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference srsLocal(*srs);

    const char *authorityName = srsLocal.GetAuthorityName(nullptr);
    if (authorityName == nullptr || strlen(authorityName) == 0)
    {
        srsLocal.AutoIdentifyEPSG();
        authorityName = srsLocal.GetAuthorityName(nullptr);
        if (authorityName != nullptr && EQUAL(authorityName, "EPSG"))
        {
            const char *authorityCode = srsLocal.GetAuthorityCode(nullptr);
            if (authorityCode != nullptr && strlen(authorityCode) > 0)
            {
                srsLocal.importFromEPSG(atoi(authorityCode));
                authorityName = srsLocal.GetAuthorityName(nullptr);
            }
        }
    }

    int authorityCode = 0;
    if (authorityName != nullptr)
    {
        authorityCode = atoi(srsLocal.GetAuthorityCode(nullptr));
        if (authorityCode > 0)
        {
            int ret = GetSridWithFilter(
                *conn_,
                CPLString().Printf("SRS_ID = %d AND ORGANIZATION = '%s'",
                                   authorityCode, authorityName));
            if (ret != UNDETERMINED_SRID)
                return ret;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to find srs id using wkt content.                           */
    /* -------------------------------------------------------------------- */

    char *wkt = nullptr;
    OGRErr err = srsLocal.exportToWkt(&wkt);
    CPLString strWkt(wkt);
    CPLFree(wkt);

    if (OGRERR_NONE != err)
        return UNDETERMINED_SRID;

    int srid = GetSridWithFilter(
        *conn_, CPLString().Printf("DEFINITION = '%s'", strWkt.c_str()));
    if (srid != UNDETERMINED_SRID)
        return srid;

    /* -------------------------------------------------------------------- */
    /*      Try to add a new spatial reference system to the database       */
    /* -------------------------------------------------------------------- */

    char *proj4 = nullptr;
    err = srsLocal.exportToProj4(&proj4);
    CPLString strProj4(proj4);
    CPLFree(proj4);

    if (OGRERR_NONE != err)
        return srid;

    if (authorityName != nullptr && authorityCode > 0)
    {
        srid = authorityCode;
    }
    else
    {
        odbc::StatementRef stmt = conn_->createStatement();
        const char *sql =
            "SELECT MAX(SRS_ID) FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS WHERE "
            "SRS_ID >= 10000000 AND SRS_ID < 20000000";
        odbc::ResultSetRef rsSrid = stmt->executeQuery(sql);
        while (rsSrid->next())
        {
            odbc::Int val = rsSrid->getInt(1);
            srid = val.isNull() ? 10000000 : *val + 1;
        }
        rsSrid->close();
    }

    try
    {
        CreateSpatialReferenceSystem(srsLocal, srid, authorityName,
                                     authorityCode, strWkt, strProj4);
        return srid;
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create an SRS in the database: %s.\n", ex.what());
    }

    return UNDETERMINED_SRID;
}

/************************************************************************/
/*                           IsSrsRoundEarth()                          */
/************************************************************************/

bool OGRHanaDataSource::IsSrsRoundEarth(int srid)
{
    const char *sql =
        "SELECT ROUND_EARTH FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS "
        "WHERE SRS_ID = ?";
    odbc::PreparedStatementRef stmt = PrepareStatement(sql);
    stmt->setInt(1, odbc::Int(srid));
    odbc::ResultSetRef rs = stmt->executeQuery();
    bool ret = false;
    if (rs->next())
        ret = (*rs->getString(1) == "TRUE");
    rs->close();
    return ret;
}

/************************************************************************/
/*                        HasSrsPlanarEquivalent()                      */
/************************************************************************/

bool OGRHanaDataSource::HasSrsPlanarEquivalent(int srid)
{
    const char *sql = "SELECT COUNT(*) FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS "
                      "WHERE SRS_ID = ?";
    odbc::PreparedStatementRef stmt = PrepareStatement(sql);
    stmt->setInt(1, ToPlanarSRID(srid));
    odbc::ResultSetRef rs = stmt->executeQuery();
    std::int64_t count = 0;
    if (rs->next())
        count = *rs->getLong(1);
    rs->close();
    return count > 0;
}

/************************************************************************/
/*                           GetQueryColumns()                          */
/************************************************************************/

OGRErr OGRHanaDataSource::GetQueryColumns(
    const CPLString &schemaName, const CPLString &query,
    std::vector<ColumnDescription> &columnDescriptions)
{
    columnDescriptions.clear();

    odbc::PreparedStatementRef stmtQuery = PrepareStatement(query);

    if (stmtQuery.isNull())
        return OGRERR_FAILURE;

    odbc::ResultSetMetaDataRef rsmd = stmtQuery->getMetaData();
    std::size_t numColumns = rsmd->getColumnCount();
    if (numColumns == 0)
        return OGRERR_NONE;

    columnDescriptions.reserve(numColumns);

    odbc::DatabaseMetaDataRef dmd = conn_->getDatabaseMetaData();
    odbc::PreparedStatementRef stmtArrayTypeInfo =
        PrepareStatement("SELECT DATA_TYPE_NAME FROM "
                         "SYS.TABLE_COLUMNS_ODBC WHERE SCHEMA_NAME = ? "
                         "AND TABLE_NAME = ? AND COLUMN_NAME = ? AND "
                         "DATA_TYPE_NAME LIKE '% ARRAY'");

    for (unsigned short clmIndex = 1; clmIndex <= numColumns; ++clmIndex)
    {
        CPLString typeName = rsmd->getColumnTypeName(clmIndex);

        if (typeName.empty())
            continue;

        bool isArray = false;
        CPLString tableName = rsmd->getTableName(clmIndex);
        CPLString columnName = rsmd->getColumnName(clmIndex);
        CPLString defaultValue;
        short dataType = rsmd->getColumnType(clmIndex);

        if (!schemaName.empty() && !tableName.empty())
        {
            // Retrieve information about default value in column
            odbc::ResultSetRef rsColumns =
                dmd->getColumns(nullptr, schemaName.c_str(), tableName.c_str(),
                                columnName.c_str());
            if (rsColumns->next())
            {
                odbc::String defaultValueStr =
                    rsColumns->getString(13 /*COLUMN_DEF*/);
                if (!defaultValueStr.isNull())
                    defaultValue =
                        FormatDefaultValue(defaultValueStr->c_str(), dataType);
            }
            rsColumns->close();

            // Retrieve information about array type
            stmtArrayTypeInfo->setString(1, schemaName);
            stmtArrayTypeInfo->setString(2, tableName);
            stmtArrayTypeInfo->setString(3, columnName);
            odbc::ResultSetRef rsArrayTypes = stmtArrayTypeInfo->executeQuery();
            if (rsArrayTypes->next())
            {
                typeName = *rsArrayTypes->getString(1);
                dataType = GetArrayDataType(typeName);

                if (dataType == QGRHanaDataTypes::Unknown)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "GetQueryColumns(): Unsupported type of array (%s)",
                        typeName.c_str());
                    return OGRERR_FAILURE;
                }

                isArray = true;
            }
            rsArrayTypes->close();
        }

        if (!isArray && !IsKnownDataType(dataType))
        {
            odbc::ResultSetRef rsTypeInfo = dmd->getTypeInfo(dataType);
            if (rsTypeInfo->next())
            {
                odbc::String name = rsTypeInfo->getString(1);
                if (name.isNull())
                    continue;
                if (name->compare("SHORTTEXT") == 0 ||
                    name->compare("ALPHANUM") == 0)
                {
                    dataType = QGRHanaDataTypes::WVarChar;
                }
            }
            rsTypeInfo->close();
        }

        if (dataType == QGRHanaDataTypes::Geometry)
        {
            GeometryColumnDescription geometryColumnDesc;
            if (schemaName.empty() || tableName.empty())
                geometryColumnDesc = GetGeometryColumnDescription(
                    *conn_, query, columnName, detectGeometryType_);
            else
                geometryColumnDesc = GetGeometryColumnDescription(
                    *conn_, schemaName, tableName, columnName,
                    detectGeometryType_);
            geometryColumnDesc.isNullable = rsmd->isNullable(clmIndex);

            columnDescriptions.push_back({true, AttributeColumnDescription(),
                                          std::move(geometryColumnDesc)});
        }
        else
        {
            AttributeColumnDescription attributeColumnDesc;
            attributeColumnDesc.name = std::move(columnName);
            attributeColumnDesc.type = dataType;
            attributeColumnDesc.typeName = std::move(typeName);
            attributeColumnDesc.isArray = isArray;
            attributeColumnDesc.isNullable = rsmd->isNullable(clmIndex);
            attributeColumnDesc.isAutoIncrement =
                rsmd->isAutoIncrement(clmIndex);
            attributeColumnDesc.length =
                static_cast<int>(rsmd->getColumnLength(clmIndex));
            attributeColumnDesc.precision = rsmd->getPrecision(clmIndex);
            attributeColumnDesc.scale = rsmd->getScale(clmIndex);
            attributeColumnDesc.defaultValue = std::move(defaultValue);

            columnDescriptions.push_back({false, std::move(attributeColumnDesc),
                                          GeometryColumnDescription()});
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetTablePrimaryKeys()                       */
/************************************************************************/

std::vector<CPLString>
OGRHanaDataSource::GetTablePrimaryKeys(const char *schemaName,
                                       const char *tableName)
{
    std::vector<CPLString> ret;

    odbc::DatabaseMetaDataRef dmd = conn_->getDatabaseMetaData();
    odbc::ResultSetRef rsPrimaryKeys =
        dmd->getPrimaryKeys(nullptr, schemaName, tableName);
    while (rsPrimaryKeys->next())
    {
        ret.push_back(*rsPrimaryKeys->getString(4));
    }
    rsPrimaryKeys->close();

    return ret;
}

/************************************************************************/
/*                          InitializeLayers()                          */
/************************************************************************/

void OGRHanaDataSource::InitializeLayers(const char *schemaName,
                                         const char *tableNames)
{
    std::vector<CPLString> tablesToFind = SplitStrings(tableNames, ",");
    const bool hasTablesToFind = !tablesToFind.empty();

    auto addLayersFromQuery = [&](const char *query, bool updatable)
    {
        odbc::PreparedStatementRef stmt = PrepareStatement(query);
        stmt->setString(1, odbc::String(schemaName));
        odbc::ResultSetRef rsTables = stmt->executeQuery();
        while (rsTables->next())
        {
            odbc::String tableName = rsTables->getString(1);
            if (tableName.isNull())
                continue;
            auto pos =
                std::find(tablesToFind.begin(), tablesToFind.end(), *tableName);
            if (pos != tablesToFind.end())
                tablesToFind.erase(pos);

            auto layer = std::make_unique<OGRHanaTableLayer>(
                this, schemaName_.c_str(), tableName->c_str(), updatable);
            layers_.push_back(std::move(layer));
        }
        rsTables->close();
    };

    // Look for layers in Tables
    std::ostringstream osTables;
    osTables << "SELECT TABLE_NAME FROM SYS.TABLES WHERE SCHEMA_NAME = ?";
    if (!tablesToFind.empty())
        osTables << " AND TABLE_NAME IN ("
                 << JoinStrings(tablesToFind, ",", Literal) << ")";

    addLayersFromQuery(osTables.str().c_str(), updateMode_);

    if (!(hasTablesToFind && tablesToFind.empty()))
    {
        // Look for layers in Views
        std::ostringstream osViews;
        osViews << "SELECT VIEW_NAME FROM SYS.VIEWS WHERE SCHEMA_NAME = ?";
        // cppcheck-suppress knownConditionTrueFalse
        if (!tablesToFind.empty())
            osViews << " AND VIEW_NAME IN ("
                    << JoinStrings(tablesToFind, ",", Literal) << ")";

        addLayersFromQuery(osViews.str().c_str(), false);
    }

    // Report about tables that could not be found
    for (const auto &tableName : tablesToFind)
    {
        const char *layerName = tableName.c_str();
        if (GetLayerByName(layerName) == nullptr)
            CPLDebug("HANA",
                     "Table '%s' not found or does not "
                     "have any geometry column.",
                     layerName);
    }
}

/************************************************************************/
/*                          LaunderName()                               */
/************************************************************************/

std::pair<OGRErr, CPLString> OGRHanaDataSource::LaunderName(const char *name)
{
    CPLAssert(name != nullptr);

    if (!CPLIsUTF8(name, -1))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s is not a valid UTF-8 string.",
                 name);
        return {OGRERR_FAILURE, ""};
    }

    auto getUTF8SequenceLength = [](char c)
    {
        if ((c & 0x80) == 0x00)
            return 1;
        if ((c & 0xE0) == 0xC0)
            return 2;
        if ((c & 0xF0) == 0xE0)
            return 3;
        if ((c & 0xF8) == 0xF0)
            return 4;

        throw std::runtime_error("Invalid UTF-8 sequence");
    };

    CPLString newName(name);
    bool hasNonASCII = false;
    size_t i = 0;

    while (name[i] != '\0')
    {
        char c = name[i];
        int len = getUTF8SequenceLength(c);
        if (len == 1)
        {
            if (c == '-' || c == '#')
                newName[i] = '_';
            else
                newName[i] = static_cast<char>(
                    CPLToupper(static_cast<unsigned char>(c)));
        }
        else
        {
            hasNonASCII = true;
        }

        i += len;
    }

    if (!hasNonASCII)
        return {OGRERR_NONE, newName};

    const char *sql = "SELECT UPPER(?) FROM DUMMY";
    odbc::PreparedStatementRef stmt = PrepareStatement(sql);
    stmt->setString(1, odbc::String(newName.c_str()));
    odbc::ResultSetRef rsName = stmt->executeQuery();
    OGRErr err = OGRERR_NONE;
    if (rsName->next())
    {
        newName.swap(*rsName->getString(1));
    }
    else
    {
        err = OGRERR_FAILURE;
        newName.clear();
    }
    rsName->close();
    return {err, newName};
}

/************************************************************************/
/*                       CreateSpatialReference()                       */
/************************************************************************/

void OGRHanaDataSource::CreateSpatialReferenceSystem(
    const OGRSpatialReference &srs, int srid, const char *authorityName,
    int authorityCode, const CPLString &wkt, const CPLString &proj4)
{
    CPLString refName((srs.IsProjected()) ? srs.GetAttrValue("PROJCS")
                                          : srs.GetAttrValue("GEOGCS"));
    if (refName.empty() || EQUAL(refName.c_str(), "UNKNOWN"))
        refName = "OGR_PROJECTION_" + std::to_string(srid);

    OGRErr err = OGRERR_NONE;
    CPLString ellipsoidParams;
    const double semiMajor = srs.GetSemiMajor(&err);
    if (OGRERR_NONE == err)
        ellipsoidParams += " SEMI MAJOR AXIS " + std::to_string(semiMajor);
    const double semiMinor = srs.GetSemiMinor(&err);
    const double invFlattening = srs.GetInvFlattening(&err);
    if (OGRERR_NONE == err)
        ellipsoidParams +=
            " INVERSE FLATTENING " + std::to_string(invFlattening);
    else
        ellipsoidParams += " SEMI MINOR AXIS " + std::to_string(semiMinor);

    const char *linearUnits = nullptr;
    srs.GetLinearUnits(&linearUnits);
    const char *angularUnits = nullptr;
    srs.GetAngularUnits(&angularUnits);

    CPLString xRange, yRange;
    double dfWestLongitudeDeg, dfSouthLatitudeDeg, dfEastLongitudeDeg,
        dfNorthLatitudeDeg;
    if (srs.GetAreaOfUse(&dfWestLongitudeDeg, &dfSouthLatitudeDeg,
                         &dfEastLongitudeDeg, &dfNorthLatitudeDeg, nullptr))
    {
        xRange = CPLString().Printf("%s BETWEEN %f AND %f",
                                    srs.IsGeographic() ? "LONGITUDE" : "X",
                                    dfWestLongitudeDeg, dfEastLongitudeDeg);
        yRange = CPLString().Printf("%s BETWEEN %f AND %f",
                                    srs.IsGeographic() ? "LATITUDE" : "Y",
                                    dfSouthLatitudeDeg, dfNorthLatitudeDeg);
    }
    else
    {
        xRange = CPLString().Printf("%s UNBOUNDED",
                                    srs.IsGeographic() ? "LONGITUDE" : "X");
        yRange = CPLString().Printf("%s UNBOUNDED ",
                                    srs.IsGeographic() ? "LATITUDE" : "Y");
    }

    CPLString organization;
    if (authorityName != nullptr && authorityCode > 0)
    {
        organization = CPLString().Printf(
            "ORGANIZATION %s IDENTIFIED BY %d",
            QuotedIdentifier(authorityName).c_str(), authorityCode);
    }

    CPLString sql = CPLString().Printf(
        "CREATE SPATIAL REFERENCE SYSTEM %s "
        "IDENTIFIED BY %d "
        "TYPE %s "
        "LINEAR UNIT OF MEASURE %s "
        "ANGULAR UNIT OF MEASURE %s "
        "%s "  // ELLIPSOID
        "COORDINATE %s "
        "COORDINATE %s "
        "%s "  // ORGANIZATION
        "DEFINITION %s "
        "TRANSFORM DEFINITION %s",
        QuotedIdentifier(refName).c_str(), srid,
        srs.IsGeographic() ? "ROUND EARTH" : "PLANAR",
        QuotedIdentifier(
            (linearUnits == nullptr || EQUAL(linearUnits, "unknown"))
                ? "metre"
                : linearUnits)
            .tolower()
            .c_str(),
        QuotedIdentifier(
            (angularUnits == nullptr || EQUAL(angularUnits, "unknown"))
                ? "degree"
                : angularUnits)
            .tolower()
            .c_str(),
        (ellipsoidParams.empty() ? ""
                                 : ("ELLIPSOID" + ellipsoidParams).c_str()),
        xRange.c_str(), yRange.c_str(), organization.c_str(),
        Literal(wkt).c_str(), Literal(proj4).c_str());

    ExecuteSQL(sql);
}

/************************************************************************/
/*                       CreateParseArrayFunctions()                    */
/************************************************************************/

void OGRHanaDataSource::CreateParseArrayFunctions(const char *schemaName)
{
    auto replaceAll = [](const CPLString &str, const CPLString &before,
                         const CPLString &after) -> CPLString
    {
        CPLString res = str;
        return res.replaceAll(before, after);
    };

    // clang-format off
    const CPLString parseStringArrayFunc =
        "CREATE OR REPLACE FUNCTION {SCHEMA}.OGR_PARSE_STRING_ARRAY(IN str NCLOB, IN delimiter NVARCHAR(10))\n"
          "RETURNS TABLE(VALUE NVARCHAR(512))\n"
          "LANGUAGE SQLSCRIPT\n"
          "SQL SECURITY INVOKER AS\n"
        "BEGIN\n"
            "DECLARE arrValues NVARCHAR(512) ARRAY;\n"
            "DECLARE idx INTEGER = 1;\n"
            "DECLARE curPos INTEGER = 1;\n"
            "DECLARE lastPos INTEGER = 1;\n"
            "DECLARE delimiterLength INTEGER = LENGTH(delimiter);\n"

            "IF(NOT(:str IS NULL)) THEN\n"
               "WHILE(:curPos > 0) DO\n"
                   "curPos = LOCATE(:str, :delimiter, :lastPos);\n"
                   "IF :curPos = 0 THEN\n"
                        "BREAK;\n"
                    "END IF;\n"

                    "arrValues[:idx] = SUBSTRING(:str, :lastPos, :curPos - :lastPos);\n"
                    "lastPos = :curPos + :delimiterLength;\n"
                    "idx = :idx + 1;\n"
                "END WHILE;\n"

                "arrValues[:idx] = SUBSTRING(:str, :lastPos, LENGTH(:str));\n"
            "END IF;\n"

            "ret = UNNEST(:arrValues) AS(\"VALUE\");\n"
            "RETURN SELECT * FROM :ret;\n"
        "END;\n";
    // clang-format on

    CPLString sql = replaceAll(parseStringArrayFunc, "{SCHEMA}",
                               QuotedIdentifier(schemaName));
    ExecuteSQL(sql);

    // clang-format off
    const CPLString parseTypeArrayFunc =
        "CREATE OR REPLACE FUNCTION {SCHEMA}.OGR_PARSE_{TYPE}_ARRAY(IN str NCLOB, IN delimiter NVARCHAR(10))\n"
           "RETURNS TABLE(VALUE {TYPE})\n"
           "LANGUAGE SQLSCRIPT\n"
           "SQL SECURITY INVOKER AS\n"
        "BEGIN\n"
            "DECLARE arrValues {TYPE} ARRAY;\n"
            "DECLARE elemValue STRING;\n"
            "DECLARE idx INTEGER = 1;\n"
            "DECLARE CURSOR cursor_values FOR\n"
                  "SELECT * FROM OGR_PARSE_STRING_ARRAY(:str, :delimiter);\n"

            "FOR row_value AS cursor_values DO\n"
                "elemValue = TRIM(row_value.VALUE);\n"
                "IF(UPPER(elemValue) = 'NULL') THEN\n"
                    "arrValues[:idx] = CAST(NULL AS {TYPE});\n"
                "ELSE\n"
                    "arrValues[:idx] = CAST(:elemValue AS {TYPE});\n"
                "END IF;\n"
                "idx = :idx + 1;\n"
            "END FOR;\n"

            "ret = UNNEST(:arrValues) AS(\"VALUE\");\n"
            "RETURN SELECT * FROM :ret;\n"
        "END;\n";
    // clang-format on

    sql = replaceAll(parseTypeArrayFunc, "{SCHEMA}",
                     QuotedIdentifier(schemaName));

    for (const CPLString &type : GetSupportedArrayTypes())
    {
        if (type == "STRING")
            continue;
        ExecuteSQL(replaceAll(sql, "{TYPE}", type));
    }
}

/************************************************************************/
/*                       ParseArrayFunctionsExist()                     */
/************************************************************************/

bool OGRHanaDataSource::ParseArrayFunctionsExist(const char *schemaName)
{
    const char *sql =
        "SELECT COUNT(*) FROM FUNCTIONS WHERE SCHEMA_NAME = ? AND "
        "FUNCTION_NAME LIKE 'OGR_PARSE_%_ARRAY'";
    odbc::PreparedStatementRef stmt = PrepareStatement(sql);
    stmt->setString(1, odbc::String(schemaName));
    odbc::ResultSetRef rsFunctions = stmt->executeQuery();
    auto numFunctions = rsFunctions->next() ? *rsFunctions->getLong(1) : 0;
    rsFunctions->close();
    return (static_cast<std::size_t>(numFunctions) ==
            GetSupportedArrayTypes().size());
}

/************************************************************************/
/*                               GetLayer()                             */
/************************************************************************/

OGRLayer *OGRHanaDataSource::GetLayer(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size())
        return nullptr;
    return layers_[static_cast<std::size_t>(index)].get();
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRHanaDataSource::GetLayerByName(const char *name)
{
    return GetLayer(FindLayerByName(name));
}

/************************************************************************/
/*                              ICreateLayer()                          */
/************************************************************************/

OGRLayer *
OGRHanaDataSource::ICreateLayer(const char *layerNameIn,
                                const OGRGeomFieldDefn *poGeomFieldDefn,
                                CSLConstList options)
{
    if (layerNameIn == nullptr)
        return nullptr;

    const auto geomType =
        poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto srs =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    // Check if we are allowed to create new objects in the database
    odbc::DatabaseMetaDataRef dmd = conn_->getDatabaseMetaData();
    if (dmd->isReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create Layer %s.\n"
                 "Database %s is read only.",
                 layerNameIn, dmd->getDatabaseName().c_str());
        return nullptr;
    }

    bool launderNames = CPLFetchBool(
        options, OGRHanaLayerCreationOptionsConstants::LAUNDER, true);
    CPLString layerName(layerNameIn);
    if (launderNames)
    {
        auto nameRes = LaunderName(layerNameIn);
        if (nameRes.first != OGRERR_NONE)
            return nullptr;
        layerName.swap(nameRes.second);
    }

    CPLDebug("HANA", "Creating layer %s.", layerName.c_str());

    int layerIndex = FindLayerByName(layerName.c_str());
    if (layerIndex >= 0)
    {
        bool overwriteLayer = CPLFetchBool(
            options, OGRHanaLayerCreationOptionsConstants::OVERWRITE, false);
        if (!overwriteLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Layer %s already exists, CreateLayer failed.\n"
                     "Use the layer creation option OVERWRITE=YES to "
                     "replace it.",
                     layerName.c_str());
            return nullptr;
        }

        DeleteLayer(layerIndex);
    }

    int batchSize =
        CPLFetchInt(options, OGRHanaLayerCreationOptionsConstants::BATCH_SIZE,
                    DEFAULT_BATCH_SIZE);
    if (batchSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create layer %s. The value of %s parameter must be "
                 "greater than 0.",
                 layerName.c_str(),
                 OGRHanaLayerCreationOptionsConstants::BATCH_SIZE);
        return nullptr;
    }

    int defaultStringSize = CPLFetchInt(
        options, OGRHanaLayerCreationOptionsConstants::DEFAULT_STRING_SIZE,
        DEFAULT_STRING_SIZE);
    if (defaultStringSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create layer %s. The value of %s parameter must be "
                 "greater than 0.",
                 layerName.c_str(),
                 OGRHanaLayerCreationOptionsConstants::DEFAULT_STRING_SIZE);
        return nullptr;
    }

    CPLString geomColumnName(CSLFetchNameValueDef(
        options, OGRHanaLayerCreationOptionsConstants::GEOMETRY_NAME,
        "OGR_GEOMETRY"));
    if (launderNames)
    {
        auto nameRes = LaunderName(geomColumnName.c_str());
        if (nameRes.first != OGRERR_NONE)
            return nullptr;
        geomColumnName.swap(nameRes.second);
    }

    const bool geomColumnNullable = CPLFetchBool(
        options, OGRHanaLayerCreationOptionsConstants::GEOMETRY_NULLABLE, true);
    CPLString geomColumnIndexType(CSLFetchNameValueDef(
        options, OGRHanaLayerCreationOptionsConstants::GEOMETRY_INDEX,
        "DEFAULT"));

    const char *paramFidName = CSLFetchNameValueDef(
        options, OGRHanaLayerCreationOptionsConstants::FID, "OGR_FID");
    CPLString fidName(paramFidName);
    if (launderNames)
    {
        auto nameRes = LaunderName(paramFidName);
        if (nameRes.first != OGRERR_NONE)
            return nullptr;
        fidName.swap(nameRes.second);
    }

    CPLString fidType =
        CPLFetchBool(options, OGRHanaLayerCreationOptionsConstants::FID64,
                     false)
            ? "BIGINT"
            : "INTEGER";

    CPLDebug("HANA", "Geometry Column Name %s.", geomColumnName.c_str());
    CPLDebug("HANA", "FID Column Name %s, Type %s.", fidName.c_str(),
             fidType.c_str());

    int srid = CPLFetchInt(options, OGRHanaLayerCreationOptionsConstants::SRID,
                           UNDETERMINED_SRID);
    if (srid < 0 && srs != nullptr)
        srid = GetSrsId(srs);

    try
    {
        CreateTable(layerName, fidName, fidType, geomColumnName, geomType,
                    geomColumnNullable, geomColumnIndexType, srid);
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create layer %s. CreateLayer failed:%s\n",
                 layerName.c_str(), ex.what());
        return nullptr;
    }

    // Create new layer object
    auto layer = std::make_unique<OGRHanaTableLayer>(this, schemaName_.c_str(),
                                                     layerName.c_str(), true);
    if (geomType != wkbNone && layer->GetLayerDefn()->GetGeomFieldCount() > 0)
        layer->GetLayerDefn()->GetGeomFieldDefn(0)->SetNullable(FALSE);
    if (batchSize > 0)
        layer->SetBatchSize(static_cast<std::size_t>(batchSize));
    if (defaultStringSize > 0)
        layer->SetDefaultStringSize(
            static_cast<std::size_t>(defaultStringSize));
    layer->SetLaunderFlag(launderNames);
    layer->SetPrecisionFlag(CPLFetchBool(
        options, OGRHanaLayerCreationOptionsConstants::PRECISION, true));
    layer->SetCustomColumnTypes(CSLFetchNameValue(
        options, OGRHanaLayerCreationOptionsConstants::COLUMN_TYPES));

    layers_.push_back(std::move(layer));

    return layers_.back().get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHanaDataSource::TestCapability(const char *capabilities)
{
    if (EQUAL(capabilities, ODsCCreateLayer))
        return updateMode_;
    else if (EQUAL(capabilities, ODsCDeleteLayer))
        return updateMode_;
    else if (EQUAL(capabilities, ODsCCreateGeomFieldAfterCreateLayer))
        return updateMode_;
    else if (EQUAL(capabilities, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(capabilities, ODsCRandomLayerWrite))
        return updateMode_;
    else if (EQUAL(capabilities, ODsCTransactions))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer *OGRHanaDataSource::ExecuteSQL(const char *sqlCommand,
                                        OGRGeometry *spatialFilter,
                                        const char *dialect)
{
    sqlCommand = SkipLeadingSpaces(sqlCommand);

    if (IsGenericSQLDialect(dialect))
        return GDALDataset::ExecuteSQL(sqlCommand, spatialFilter, dialect);

    if (STARTS_WITH_CI(sqlCommand, "DELLAYER:"))
    {
        const char *layerName = SkipLeadingSpaces(sqlCommand + 9);
        int layerIndex = FindLayerByName(layerName);
        if (layerIndex >= 0)
            DeleteLayer(layerIndex);
        return nullptr;
    }
    if (STARTS_WITH_CI(sqlCommand, "SELECT"))
    {
        auto stmt = PrepareStatement(sqlCommand);
        if (stmt.isNull())
            return nullptr;

        auto layer = std::make_unique<OGRHanaResultLayer>(this, sqlCommand);
        if (spatialFilter != nullptr)
            layer->SetSpatialFilter(spatialFilter);
        return layer.release();
    }

    try
    {
        ExecuteSQL(sqlCommand);
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to execute SQL statement '%s': %s", sqlCommand,
                 ex.what());
    }

    return nullptr;
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

OGRErr OGRHanaDataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if (isTransactionStarted_)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction already established");
        return OGRERR_FAILURE;
    }

    isTransactionStarted_ = true;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr OGRHanaDataSource::CommitTransaction()
{
    if (!isTransactionStarted_)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    isTransactionStarted_ = false;

    try
    {
        for (size_t i = 0; i < layers_.size(); ++i)
        {
            OGRHanaLayer *layer = static_cast<OGRHanaLayer *>(layers_[i].get());
            if (layer->IsTableLayer())
            {
                OGRHanaTableLayer *tableLayer =
                    static_cast<OGRHanaTableLayer *>(layer);
                tableLayer->FlushPendingBatches(false);
            }
        }

        conn_->commit();
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to commit transaction: %s", ex.what());
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                           RollbackTransaction()                      */
/************************************************************************/

OGRErr OGRHanaDataSource::RollbackTransaction()
{
    if (!isTransactionStarted_)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    isTransactionStarted_ = false;

    try
    {
        conn_->rollback();
    }
    catch (const odbc::Exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to roll back transaction: %s", ex.what());
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}
