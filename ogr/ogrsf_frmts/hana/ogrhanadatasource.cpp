/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDataSource class implementation
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
#include "ogrhanautils.h"

#include "odbc/Connection.h"
#include "odbc/Environment.h"
#include "odbc/Exception.h"
#include "odbc/DatabaseMetaData.h"
#include "odbc/PreparedStatement.h"
#include "odbc/ResultSet.h"
#include "odbc/ResultSetMetaData.h"
#include "odbc/Statement.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

CPL_CVSID("$Id$")

using namespace OGRHANA;

namespace
{

class LayerCreationOptionsConstants
{
public:
    LayerCreationOptionsConstants() = delete;

public:
    static constexpr const char* OVERWRITE = "OVERWRITE";
    static constexpr const char* LAUNDER = "LAUNDER";
    static constexpr const char* PRECISION = "PRECISION";
    static constexpr const char* DEFAULT_STRING_SIZE = "DEFAULT_STRING_SIZE";
    static constexpr const char* GEOMETRY_NAME = "GEOMETRY_NAME";
    static constexpr const char* GEOMETRY_NULLABLE = "GEOMETRY_NULLABLE";
    static constexpr const char* GEOMETRY_INDEX = "GEOMETRY_INDEX";
    static constexpr const char* SRID = "SRID";
    static constexpr const char* FID = "FID";
    static constexpr const char* FID64 = "FID64";
    static constexpr const char* COLUMN_TYPES = "COLUMN_TYPES";
    static constexpr const char* BATCH_SIZE = "BATCH_SIZE";

    // clang-format off
    static const char* GetList()
    {
        return
               "<LayerCreationOptionList>"
               "  <Option name='OVERWRITE' type='boolean' description='Specifies whether to overwrite an existing table with the layer name to be created' default='NO'/>"
               "  <Option name='LAUNDER' type='boolean' description='Specifies whether layer and field names will be laundered' default='YES'/>"
               "  <Option name='PRECISION' type='boolean' description='Specifies whether fields created should keep the width and precision' default='YES'/>"
               "  <Option name='DEFAULT_STRING_SIZE' type='int' description='Specifies default string column size' default='256'/>"
               "  <Option name='GEOMETRY_NAME' type='string' description='Specifies name of geometry column.' default='OGR_GEOMETRY'/>"
               "  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Specifies whether the values of the geometry column can be NULL' default='YES'/>"
               "  <Option name='GEOMETRY_INDEX' type='string' description='Specifies which spatial index to use for the geometry column' default='DEFAULT'/>"
               "  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
               "  <Option name='FID' type='string' description='Specifies the name of the FID column to create' default='OGR_FID'/>"
               "  <Option name='FID64' type='boolean' description='Specifies whether to create the FID column with BIGINT type to handle 64bit wide ids' default='NO'/>"
               "  <Option name='COLUMN_TYPES' type='string' description='Specifies a comma-separated list of strings in the format field_name=hana_field_type that define column types.'/>"
               "  <Option name='BATCH_SIZE' type='int' description='Specifies the number of bytes to be written per one batch' default='4194304'/>"
               "</LayerCreationOptionList>";
    }
    // clang-format on
};

class OpenOptionsConstants
{
public:
    OpenOptionsConstants() = delete;

public:
    static constexpr const char* DSN = "DSN";
    static constexpr const char* DRIVER = "DRIVER";
    static constexpr const char* HOST = "HOST";
    static constexpr const char* PORT = "PORT";
    static constexpr const char* DATABASE = "DATABASE";
    static constexpr const char* USER = "USER";
    static constexpr const char* PASSWORD = "PASSWORD";
    static constexpr const char* SCHEMA = "SCHEMA";
    static constexpr const char* TABLES = "TABLES";

    static constexpr const char* ENCRYPT = "ENCRYPT";
    static constexpr const char* SSL_CRYPTO_PROVIDER = "SSL_CRYPTO_PROVIDER";
    static constexpr const char* SSL_KEY_STORE = "SSL_KEY_STORE";
    static constexpr const char* SSL_TRUST_STORE = "SSL_TRUST_STORE";
    static constexpr const char* SSL_VALIDATE_CERTIFICATE =
        "SSL_VALIDATE_CERTIFICATE";
    static constexpr const char* SSL_HOST_NAME_CERTIFICATE =
        "SSL_HOST_NAME_CERTIFICATE";

    static constexpr const char* CONNECTION_TIMEOUT = "CONNECTION_TIMEOUT";
    static constexpr const char* PACKET_SIZE = "PACKET_SIZE";
    static constexpr const char* SPLIT_BATCH_COMMANDS = "SPLIT_BATCH_COMMANDS";

    static constexpr const char* DETECT_GEOMETRY_TYPE = "DETECT_GEOMETRY_TYPE";

    // clang-format off
    static const char* GetList()
    {
        return
               "<OpenOptionList>"
               "  <Option name='DRIVER' type='string' description='Name or a path to a driver.For example, DRIVER={HDBODBC} or DRIVER=/usr/sap/hdbclient/libodbcHDB.so' required='true'/>"
               "  <Option name='HOST' type='string' description='Server hostname' required='true'/>"
               "  <Option name='PORT' type='int' description='Port number' required='true'/>"
               "  <Option name='DATABASE' type='string' description='Specifies the name of the database to connect to' required='true'/>"
               "  <Option name='USER' type='string' description='Specifies the user name' required='true'/>"
               "  <Option name='PASSWORD' type='string' description='Specifies the user password' required='true'/>"
               "  <Option name='SCHEMA' type='string' description='Specifies the schema used for tables listed in TABLES option' required='true'/>"
               "  <Option name='TABLES' type='string' description='Restricted set of tables to list (comma separated)'/>"
               "  <Option name='ENCRYPT' type='boolean' description='Enables or disables TLS/SSL encryption' default='NO'/>"
               "  <Option name='SSL_CRYPTO_PROVIDER' type='string' description='Cryptographic library provider used for SSL communication (commoncrypto| sapcrypto | openssl)'/>"
               "  <Option name='SSL_KEY_STORE' type='string' description='Path to the keystore file that contains the server&apos;s private key'/>"
               "  <Option name='SSL_TRUST_STORE' type='string' description='Path to trust store file that contains the server&apos;s public certificate(s) (OpenSSL only)'/>"
               "  <Option name='SSL_VALIDATE_CERTIFICATE' type='boolean' description='If set to true, the server&apos;s certificate is validated' default='YES'/>"
               "  <Option name='SSL_HOST_NAME_IN_CERTIFICATE' type='string' description='Host name used to verify server&apos;s identity'/>"
               "  <Option name='CONNECTION_TIMEOUT' type='int' description='Connection timeout measured in milliseconds. Setting this option to 0 disables the timeout'/>"
               "  <Option name='PACKET_SIZE' type='int' description='Sets the maximum size of a request packet sent from the client to the server, in bytes. The minimum is 1 MB.'/>"
               "  <Option name='SPLIT_BATCH_COMMANDS' type='boolean' description='Allows split and parallel execution of batch commands on partitioned tables' default='YES'/>"
               "  <Option name='DETECT_GEOMETRY_TYPE' type='boolean' description='Specifies whether to detect the type of geometry columns. Note, the detection may take a significant amount of time for large tables' default='YES'/>"
               "</OpenOptionList>";
    }
    // clang-format on
};

CPLString BuildConnectionString(char** openOptions)
{
    std::vector<CPLString> params;
    auto addParameter = [&](const char* optionName, const char* paramName) {
        const char* paramValue =
            CSLFetchNameValueDef(openOptions, optionName, nullptr);
        if (paramValue == nullptr)
            return;
        params.push_back(CPLString(paramName) + "=" + CPLString(paramValue));
    };

    const char* paramDSN =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::DSN, nullptr);
    const char* paramDriver =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::DRIVER, "");
    const char* paramHost =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::HOST, "");
    const char* paramPort =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::PORT, "");
    const char* paramDatabase =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::DATABASE, "");
    const char* paramUser =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::USER, "");
    const char* paramPassword =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::PASSWORD, "");
    const char* paramSchema =
        CSLFetchNameValueDef(openOptions, OpenOptionsConstants::SCHEMA, "");

    if (CPLFetchBool(openOptions, OpenOptionsConstants::ENCRYPT, false))
    {
        params.push_back("encrypt=true");
        addParameter(
            OpenOptionsConstants::SSL_CRYPTO_PROVIDER, "sslCryptoProvider");
        addParameter(OpenOptionsConstants::SSL_KEY_STORE, "sslKeyStore");
        addParameter(OpenOptionsConstants::SSL_TRUST_STORE, "sslTrustStore");
        addParameter(
            OpenOptionsConstants::SSL_VALIDATE_CERTIFICATE,
            "sslValidateCertificate");
        addParameter(
            OpenOptionsConstants::SSL_HOST_NAME_CERTIFICATE,
            "sslHostNameInCertificate");
    }

    addParameter(OpenOptionsConstants::PACKET_SIZE, "PACKETSIZE");
    addParameter(
        OpenOptionsConstants::SPLIT_BATCH_COMMANDS, "SPLITBATCHCOMMANDS");

    // For more details on how to escape special characters in passwords,
    // see
    // https://stackoverflow.com/questions/55150362/maybe-illegal-character-in-odbc-sql-server-connection-string-pwd
    if (paramDSN != nullptr)
        return CPLString().Printf(
            "DSN=%s;UID=%s;PWD={%s};CURRENTSCHEMA=\"%s\";CHAR_AS_UTF8=1;%s",
            paramDSN, paramUser, paramPassword, paramSchema,
            JoinStrings(params, ";").c_str());
    else
        return CPLString().Printf(
            "DRIVER={%s};SERVERNODE=%s:%s;DATABASENAME=%s;UID=%s;"
            "PWD={%s};CURRENTSCHEMA=\"%s\";CHAR_AS_UTF8=1;%s",
            paramDriver, paramHost, paramPort, paramDatabase, paramUser,
            paramPassword, paramSchema, JoinStrings(params, ";").c_str());
}

int CPLFetchInt(CSLConstList papszStrList, const char *pszKey, int defaultValue)
{
    const char * const pszValue =
        CSLFetchNameValue( papszStrList, pszKey );
    if( pszValue == nullptr )
        return defaultValue;
    return atoi( pszValue );
}

int GetSrid(odbc::ResultSet& resultSet)
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

int GetColumnSrid(
    odbc::Connection& conn,
    const CPLString& schemaName,
    const CPLString& tableName,
    const CPLString& columnName)
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

int GetColumnSrid(
    odbc::Connection& conn, const CPLString& query, const CPLString& columnName)
{
    CPLString clmName = QuotedIdentifier(columnName);

    CPLString sql = CPLString().Printf(
        "SELECT %s.ST_SRID() FROM (%s) WHERE %s IS NOT NULL", clmName.c_str(),
        query.c_str(), clmName.c_str());

    odbc::StatementRef stmt = conn.createStatement();
    return GetSrid(*stmt->executeQuery(sql.c_str()));
}

int GetSridWithFilter(odbc::Connection& conn, const CPLString& whereCondition)
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

CPLString GetSrsWktById(odbc::Connection& conn, int srid)
{
    CPLString ret;
    const char* sql = "SELECT DEFINITION FROM "
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

OGRwkbGeometryType GetGeometryType(
    odbc::Connection& conn,
    const CPLString& query,
    const CPLString& columnName)
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
    odbc::Connection& conn,
    const CPLString& schemaName,
    const CPLString& tableName,
    const CPLString& columnName,
    bool detectGeometryType)
{
    OGRwkbGeometryType type = detectGeometryType ?
        GetGeometryType(conn, GetFullTableNameQuoted(schemaName, tableName), columnName) :
        OGRwkbGeometryType::wkbUnknown;
    int srid = GetColumnSrid(conn, schemaName, tableName, columnName);

    return {columnName, type, srid, false};
}

GeometryColumnDescription GetGeometryColumnDescription(
    odbc::Connection& conn,
    const CPLString& query,
    const CPLString& columnName,
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
            columnName.end(), [](char ch1, char ch2) {
                return std::toupper(ch1) == std::toupper(ch2);
            });

        if (it != preparedQuery.end())
        {
            auto pos = it - preparedQuery.begin();
            CPLString newName = columnName + " AS \"tmp_geom_field\"";
            preparedQuery.replace(
                static_cast<std::size_t>(pos), columnName.length(), newName, 0,
                newName.length());
            clmName = "tmp_geom_field";
        }
    }

    OGRwkbGeometryType type = detectGeometryType ?
        GetGeometryType(conn, "(" + preparedQuery + ")", clmName) :
        OGRwkbGeometryType::wkbUnknown;
    int srid = GetColumnSrid(conn, preparedQuery, clmName);

    return {columnName, type, srid, false};
}

CPLString FormatDefaultValue(const char* value, short dataType)
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
    case odbc::SQLDataTypes::Bit:
    case odbc::SQLDataTypes::Boolean:
        return value;
    case odbc::SQLDataTypes::TinyInt:
    case odbc::SQLDataTypes::SmallInt:
    case odbc::SQLDataTypes::Integer:
    case odbc::SQLDataTypes::BigInt:
    case odbc::SQLDataTypes::Real:
    case odbc::SQLDataTypes::Float:
    case odbc::SQLDataTypes::Double:
    case odbc::SQLDataTypes::Decimal:
    case odbc::SQLDataTypes::Numeric:
        return value;
    case odbc::SQLDataTypes::Char:
    case odbc::SQLDataTypes::VarChar:
    case odbc::SQLDataTypes::LongVarChar:
    case odbc::SQLDataTypes::WChar:
    case odbc::SQLDataTypes::WVarChar:
    case odbc::SQLDataTypes::WLongVarChar:
        return Literal(value);
    case odbc::SQLDataTypes::Binary:
    case odbc::SQLDataTypes::VarBinary:
    case odbc::SQLDataTypes::LongVarBinary:
        return value;
    case odbc::SQLDataTypes::Date:
    case odbc::SQLDataTypes::TypeDate:
        if (EQUAL(value, "CURRENT_DATE"))
            return value;
        return Literal(value);
    case odbc::SQLDataTypes::Time:
    case odbc::SQLDataTypes::TypeTime:
        if (EQUAL(value, "CURRENT_TIME"))
            return value;
        return Literal(value);
    case odbc::SQLDataTypes::Timestamp:
    case odbc::SQLDataTypes::TypeTimestamp:
        if (EQUAL(value, "CURRENT_TIMESTAMP"))
            return value;
        return Literal(value);
    default:
        return value;
    }
}

short GetArrayDataType(const CPLString& typeName)
{
    if (typeName == "BOOLEAN ARRAY")
        return odbc::SQLDataTypes::Boolean;
    else if (typeName == "TINYINT ARRAY")
        return odbc::SQLDataTypes::TinyInt;
    else if (typeName == "SMALLINT ARRAY")
        return odbc::SQLDataTypes::SmallInt;
    else if (typeName == "INTEGER ARRAY")
        return odbc::SQLDataTypes::Integer;
    else if (typeName == "BIGINT ARRAY")
        return odbc::SQLDataTypes::BigInt;
    else if (typeName == "DOUBLE ARRAY")
        return odbc::SQLDataTypes::Double;
    else if (typeName == "REAL ARRAY")
        return odbc::SQLDataTypes::Float;
    else if (typeName == "DECIMAL ARRAY" || typeName == "SMALLDECIMAL ARRAY")
        return odbc::SQLDataTypes::Decimal;
    else if (typeName == "CHAR ARRAY")
        return odbc::SQLDataTypes::Char;
    else if (typeName == "VARCHAR ARRAY")
        return odbc::SQLDataTypes::VarChar;
    else if (typeName == "NCHAR ARRAY")
        return odbc::SQLDataTypes::WChar;
    else if (typeName == "NVARCHAR ARRAY")
        return odbc::SQLDataTypes::WVarChar;
    else if (typeName == "DATE ARRAY")
        return odbc::SQLDataTypes::Date;
    else if (typeName == "TIME ARRAY")
        return odbc::SQLDataTypes::Time;
    else if (typeName == "TIMESTAMP ARRAY" || typeName == "SECONDDATE ARRAY")
        return odbc::SQLDataTypes::Timestamp;

    return odbc::SQLDataTypes::Unknown;
}

std::vector<CPLString> GetSupportedArrayTypes()
{
    return {"TINYINT", "SMALLINT", "INT", "BIGINT", "REAL", "DOUBLE", "STRING"};
}

bool IsKnownDataType(short dataType)
{
    return dataType == odbc::SQLDataTypes::Bit
           || dataType == odbc::SQLDataTypes::Boolean
           || dataType == odbc::SQLDataTypes::TinyInt
           || dataType == odbc::SQLDataTypes::SmallInt
           || dataType == odbc::SQLDataTypes::Integer
           || dataType == odbc::SQLDataTypes::BigInt
           || dataType == odbc::SQLDataTypes::Double
           || dataType == odbc::SQLDataTypes::Real
           || dataType == odbc::SQLDataTypes::Float
           || dataType == odbc::SQLDataTypes::Decimal
           || dataType == odbc::SQLDataTypes::Numeric
           || dataType == odbc::SQLDataTypes::Char
           || dataType == odbc::SQLDataTypes::VarChar
           || dataType == odbc::SQLDataTypes::LongVarChar
           || dataType == odbc::SQLDataTypes::WChar
           || dataType == odbc::SQLDataTypes::WVarChar
           || dataType == odbc::SQLDataTypes::WLongVarChar
           || dataType == odbc::SQLDataTypes::Date
           || dataType == odbc::SQLDataTypes::TypeDate
           || dataType == odbc::SQLDataTypes::Time
           || dataType == odbc::SQLDataTypes::TypeTime
           || dataType == odbc::SQLDataTypes::Timestamp
           || dataType == odbc::SQLDataTypes::TypeTimestamp
           || dataType == odbc::SQLDataTypes::Binary
           || dataType == odbc::SQLDataTypes::VarBinary
           || dataType == odbc::SQLDataTypes::LongVarBinary;
}

} // anonymous namespace

/************************************************************************/
/*                               GetPrefix()                            */
/************************************************************************/

const char* OGRHanaDataSource::GetPrefix()
{
    return "HANA:";
}

/************************************************************************/
/*                         GetLayerCreationOptions()                    */
/************************************************************************/

const char* OGRHanaDataSource::GetLayerCreationOptions()
{
    return LayerCreationOptionsConstants::GetList();
}

/************************************************************************/
/*                           GetOpenOptions()                           */
/************************************************************************/

const char* OGRHanaDataSource::GetOpenOptions()
{
    return OpenOptionsConstants::GetList();
}

/************************************************************************/
/*                         GetSupportedDataTypes()                      */
/************************************************************************/

const char* OGRHanaDataSource::GetSupportedDataTypes()
{
    return "Integer Integer64 Real String Date DateTime Time IntegerList "
           "Integer64List RealList StringList Binary";
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

    for (const auto& kv : srsCache_)
    {
        OGRSpatialReference* srs = kv.second;
        if (srs != nullptr)
            srs->Release();
    }
    srsCache_.clear();
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

int OGRHanaDataSource::Open(const char* newName, char** openOptions, int update)
{
    CPLAssert(layers_.size() == 0);

    if (!STARTS_WITH_CI(newName, GetPrefix()))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "%s does not conform to HANA driver naming convention,"
            " %s*\n",
            newName, GetPrefix());
        return FALSE;
    }

    updateMode_ = update;
    detectGeometryType_ =
        CPLFetchBool(openOptions, OpenOptionsConstants::DETECT_GEOMETRY_TYPE, true);

    std::size_t prefixLength = strlen(GetPrefix());
    char** connOptions = CSLTokenizeStringComplex(newName + prefixLength, ";", TRUE, FALSE);

    int ret = FALSE;

    const char* paramSchema = CSLFetchNameValueDef(
        connOptions, OpenOptionsConstants::SCHEMA, nullptr);
    if (paramSchema == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "HANA parameter '%s' is missing:\n", "SCHEMA");
    }
    else
    {
        schemaName_ = paramSchema;

        connEnv_ = odbc::Environment::create();
        conn_ = connEnv_->createConnection();
        conn_->setAutoCommit(false);

        const char* paramConnTimeout = CSLFetchNameValueDef(
            connOptions, OpenOptionsConstants::CONNECTION_TIMEOUT, nullptr);
        if (paramConnTimeout != nullptr)
            conn_->setConnectionTimeout(
                static_cast<unsigned long>(atoi(paramConnTimeout)));

        try
        {
            CPLString connectionStr = BuildConnectionString(connOptions);
            conn_->connect(connectionStr.c_str());
        }
        catch (const odbc::Exception& ex)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "HANA connection failed: %s\n",
                ex.what());
        }

        if (conn_->connected())
        {
            odbc::DatabaseMetaDataRef dbmd = conn_->getDatabaseMetaData();
            CPLString dbVersion(dbmd->getDBMSVersion());
            majorVersion_ = atoi(dbVersion.substr(0u, dbVersion.find('.')).c_str());

            const char* paramTables =
                CSLFetchNameValueDef(connOptions, OpenOptionsConstants::TABLES, "");
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

    const std::unique_ptr<OGRLayer>& layer =
        layers_[static_cast<std::size_t>(index)];
    CPLDebug("HANA", "DeleteLayer(%s)", layer->GetName());

    if (auto tableLayer = dynamic_cast<OGRHanaTableLayer*>(layer.get()))
    {
        OGRErr err = tableLayer->DropTable();
        if (OGRERR_NONE == err)
            return err;
    }

    layers_.erase(layers_.begin() + index);

    return OGRERR_NONE;
}

void OGRHanaDataSource::CreateTable(
    const CPLString& tableName,
    const CPLString& fidName,
    const CPLString& fidType,
    const CPLString& geomColumnName,
    OGRwkbGeometryType geomType,
    bool geomColumnNullable,
    const CPLString& geomColumnIndexType,
    int geomSrid)
{
    CPLString sql;
    if (geomType == OGRwkbGeometryType::wkbNone || !(!geomColumnName.empty() && geomSrid >= 0))
    {
        sql = "CREATE COLUMN TABLE "
              + GetFullTableNameQuoted(schemaName_, tableName) + " ("
              + QuotedIdentifier(fidName) + " " + fidType
              + " GENERATED BY DEFAULT AS IDENTITY, PRIMARY KEY ( "
              + QuotedIdentifier(fidName) + "));";
    }
    else
    {
        sql = "CREATE COLUMN TABLE "
              + GetFullTableNameQuoted(schemaName_, tableName) + " ("
              + QuotedIdentifier(fidName) + " " + fidType
              + " GENERATED BY DEFAULT AS IDENTITY, "
              + QuotedIdentifier(geomColumnName) + " ST_GEOMETRY ("
              + std::to_string(geomSrid) + ")" + (geomColumnNullable ? "" : " NOT NULL") + " SPATIAL INDEX PREFERENCE " + geomColumnIndexType
              + ", PRIMARY KEY ( " + QuotedIdentifier(fidName) + "));";
    }

    ExecuteSQL(sql.c_str());
}

/************************************************************************/
/*                            FindSchemaAndTableNames()                 */
/************************************************************************/

std::pair<CPLString, CPLString> OGRHanaDataSource::FindSchemaAndTableNames(
    const char* query)
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

int OGRHanaDataSource::FindLayerByName(const char* name)
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

CPLString OGRHanaDataSource::FindSchemaName(const char* objectName)
{
    auto getSchemaName = [&](const char* sql) {
        odbc::PreparedStatementRef stmt = conn_->prepareStatement(sql);
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

odbc::PreparedStatementRef OGRHanaDataSource::PrepareStatement(const char* sql)
{
    try
    {
        CPLDebug("HANA", "Prepare statement %s.", sql);
        return conn_->prepareStatement(sql);
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to prepare statement: %s",
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

void OGRHanaDataSource::ExecuteSQL(const char* sql)
{
    odbc::StatementRef stmt = conn_->createStatement();
    stmt->execute(sql);
    conn_->commit();
}

/************************************************************************/
/*                            GetSrsById()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference* OGRHanaDataSource::GetSrsById(int srid)
{
    if (srid < 0)
        return nullptr;

    auto it = srsCache_.find(srid);
    if (it != srsCache_.end())
        return it->second;

    std::unique_ptr<OGRSpatialReference> srs;

    CPLString wkt = GetSrsWktById(*conn_, srid);
    if (!wkt.empty())
    {
        srs = cpl::make_unique<OGRSpatialReference>();
        OGRErr err = srs->importFromWkt(wkt.c_str());
        if (OGRERR_NONE != err)
            srs.reset(nullptr);
    }

    srsCache_.insert({srid, srs.get()});

    return srs.release();
}

/************************************************************************/
/*                               GetSrsId()                             */
/************************************************************************/

int OGRHanaDataSource::GetSrsId(OGRSpatialReference* srs)
{
    if (srs == nullptr)
        return UNDETERMINED_SRID;

    /* -------------------------------------------------------------------- */
    /*      Try to find srs id using authority name and code (EPSG:3857).   */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference srsLocal(*srs);

    const char* authorityName = srsLocal.GetAuthorityName(nullptr);
    if (authorityName == nullptr || strlen(authorityName) == 0)
    {
        srsLocal.AutoIdentifyEPSG();
        authorityName = srsLocal.GetAuthorityName(nullptr);
        if (authorityName != nullptr && EQUAL(authorityName, "EPSG"))
        {
            const char* authorityCode = srsLocal.GetAuthorityCode(nullptr);
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
                *conn_, CPLString().Printf(
                            "SRS_ID = %d AND ORGANIZATION = '%s'", authorityCode,
                            authorityName));
            if (ret != UNDETERMINED_SRID)
                return ret;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to find srs id using wkt content.                           */
    /* -------------------------------------------------------------------- */

    char* wkt = nullptr;
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

    char* proj4 = nullptr;
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
        const char* sql = "SELECT MAX(SRS_ID) FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS WHERE SRS_ID >= 10000000 AND SRS_ID < 20000000";
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
        CreateSpatialReferenceSystem(
            srsLocal, srid,
            authorityName, authorityCode,
            strWkt, strProj4);
        return srid;
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to create an SRS in the database: %s.\n", ex.what());
    }

    return UNDETERMINED_SRID;
}

/************************************************************************/
/*                           IsSrsRoundEarth()                          */
/************************************************************************/

bool OGRHanaDataSource::IsSrsRoundEarth(int srid)
{
    const char* sql =
        "SELECT ROUND_EARTH FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS "
        "WHERE SRS_ID = ?";
    odbc::PreparedStatementRef stmt = conn_->prepareStatement(sql);
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
    const char* sql = "SELECT COUNT(*) FROM SYS.ST_SPATIAL_REFERENCE_SYSTEMS "
                      "WHERE SRS_ID = ?";
    odbc::PreparedStatementRef stmt = conn_->prepareStatement(sql);
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
    const CPLString& schemaName,
    const CPLString& query,
    std::vector<ColumnDescription>& columnDescriptions)
{
    columnDescriptions.clear();

    odbc::PreparedStatementRef stmtQuery;

    try
    {
        stmtQuery = conn_->prepareStatement(query);
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Unable to prepare statement: %s",
            ex.what());
        return OGRERR_FAILURE;
    }

    odbc::ResultSetMetaDataRef rsmd = stmtQuery->getMetaData();
    std::size_t numColumns = rsmd->getColumnCount();
    if (numColumns == 0)
        return OGRERR_NONE;

    columnDescriptions.reserve(numColumns);

    CPLString tableName = rsmd->getTableName(1);
    odbc::DatabaseMetaDataRef dmd = conn_->getDatabaseMetaData();
    odbc::PreparedStatementRef stmtArrayTypeInfo =
        conn_->prepareStatement("SELECT DATA_TYPE_NAME FROM "
                                "SYS.TABLE_COLUMNS_ODBC WHERE SCHEMA_NAME = ? "
                                "AND TABLE_NAME = ? AND COLUMN_NAME = ? AND "
                                "DATA_TYPE_NAME LIKE '% ARRAY'");

    for (unsigned short clmIndex = 1; clmIndex <= numColumns; ++clmIndex)
    {
        CPLString typeName = rsmd->getColumnTypeName(clmIndex);

        if (typeName.empty())
            continue;

        bool isArray = false;
        bool isGeometry = false;
        CPLString columnName = rsmd->getColumnName(clmIndex);
        CPLString defaultValue;
        short dataType = rsmd->getColumnType(clmIndex);

        if (!schemaName.empty() && !tableName.empty())
        {
            // Retrieve information about default value in column
            odbc::ResultSetRef rsColumns = dmd->getColumns(
                nullptr, schemaName.c_str(), tableName.c_str(),
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

                if (dataType == odbc::SQLDataTypes::Unknown)
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
                if (name->compare("SHORTTEXT") == 0
                    || name->compare("ALPHANUM") == 0)
                {
                    dataType = odbc::SQLDataTypes::WVarChar;
                }
                else if (
                    name->compare("ST_GEOMETRY") == 0
                    || name->compare("ST_POINT") == 0)
                {
                    isGeometry = true;
                }
            }
            rsTypeInfo->close();
        }

        if (isGeometry)
        {
            GeometryColumnDescription geometryColumnDesc;
            if (schemaName.empty() || tableName.empty())
                geometryColumnDesc =
                    GetGeometryColumnDescription(*conn_, query, columnName, detectGeometryType_);
            else
                geometryColumnDesc = GetGeometryColumnDescription(
                    *conn_, schemaName, tableName, columnName, detectGeometryType_);
            geometryColumnDesc.isNullable = rsmd->isNullable(clmIndex);

            columnDescriptions.push_back(
                {true, AttributeColumnDescription(), geometryColumnDesc});
        }
        else
        {
            AttributeColumnDescription attributeColumnDesc;
            attributeColumnDesc.name = columnName;
            attributeColumnDesc.type = dataType;
            attributeColumnDesc.typeName = typeName;
            attributeColumnDesc.isArray = isArray;
            attributeColumnDesc.isNullable = rsmd->isNullable(clmIndex);
            attributeColumnDesc.isAutoIncrement =
                rsmd->isAutoIncrement(clmIndex);
            attributeColumnDesc.length =
                static_cast<int>(rsmd->getColumnLength(clmIndex));
            attributeColumnDesc.precision = rsmd->getPrecision(clmIndex);
            attributeColumnDesc.scale = rsmd->getScale(clmIndex);
            attributeColumnDesc.defaultValue = defaultValue;

            columnDescriptions.push_back(
                {false, attributeColumnDesc, GeometryColumnDescription()});
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetTablePrimaryKeys()                       */
/************************************************************************/

std::vector<CPLString> OGRHanaDataSource::GetTablePrimaryKeys(
    const char* schemaName, const char* tableName)
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

void OGRHanaDataSource::InitializeLayers(
    const char* schemaName, const char* tableNames)
{
    std::vector<CPLString> tables = SplitStrings(tableNames, ",");

    auto addLayersFromQuery = [&](const char* query, bool updatable) {
        odbc::PreparedStatementRef stmt = conn_->prepareStatement(query);
        stmt->setString(1, odbc::String(schemaName));
        odbc::ResultSetRef rsTables = stmt->executeQuery();
        while (rsTables->next())
        {
            odbc::String tableName = rsTables->getString(1);
            if (tableName.isNull())
                continue;
            auto pos = std::find(tables.begin(), tables.end(), *tableName);
            if (pos != tables.end())
                tables.erase(pos);

            auto layer = cpl::make_unique<OGRHanaTableLayer>(this, schemaName_.c_str(), tableName->c_str(), updatable);
            layers_.push_back(std::move(layer));
        }
        rsTables->close();
    };

    // Look for layers in Tables
    std::ostringstream osTables;
    osTables << "SELECT TABLE_NAME FROM SYS.TABLES WHERE SCHEMA_NAME = ?";
    if (!tables.empty())
        osTables << " AND TABLE_NAME IN (" << JoinStrings(tables, ",", Literal)
                 << ")";

    addLayersFromQuery(osTables.str().c_str(), updateMode_);

    // Look for layers in Views
    std::ostringstream osViews;
    osViews << "SELECT VIEW_NAME FROM SYS.VIEWS WHERE SCHEMA_NAME = ?";
    if (!tables.empty())
        osViews << " AND VIEW_NAME IN (" << JoinStrings(tables, ",", Literal)
                << ")";

    addLayersFromQuery(osViews.str().c_str(), false);

    // Report about tables that could not be found
    for (const auto& tableName : tables)
    {
        const char* layerName = tableName.c_str();
        if (GetLayerByName(layerName) == nullptr)
            CPLDebug(
                "HANA",
                "Table '%s' not found or does not "
                "have any geometry column.",
                layerName);
    }
}

/************************************************************************/
/*                       CreateSpatialReference()                       */
/************************************************************************/

void OGRHanaDataSource::CreateSpatialReferenceSystem(
    const OGRSpatialReference& srs,
    int srid,
    const char* authorityName,
    int authorityCode,
    const CPLString& wkt,
    const CPLString& proj4)
{
    CPLString refName(
        (srs.IsProjected()) ? srs.GetAttrValue("PROJCS")
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
    double dfWestLongitudeDeg, dfSouthLatitudeDeg,
            dfEastLongitudeDeg, dfNorthLatitudeDeg;
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
            QuotedIdentifier(authorityName).c_str(),
            authorityCode);
    }

    CPLString sql = CPLString().Printf(
        "CREATE SPATIAL REFERENCE SYSTEM %s "
        "IDENTIFIED BY %d "
        "TYPE %s "
        "LINEAR UNIT OF MEASURE %s "
        "ANGULAR UNIT OF MEASURE %s "
        "%s " // ELLIPSOID
        "COORDINATE %s "
        "COORDINATE %s "
        "%s " // ORGANIZATION
        "DEFINITION %s "
        "TRANSFORM DEFINITION %s",
        QuotedIdentifier(refName).c_str(),
        srid,
        srs.IsGeographic() ? "ROUND EARTH" : "PLANAR",
        QuotedIdentifier((linearUnits == nullptr || EQUAL(linearUnits, "unknown")) ? "metre" : linearUnits).tolower().c_str(),
        QuotedIdentifier((angularUnits == nullptr || EQUAL(angularUnits, "unknown")) ? "degree" : angularUnits).tolower().c_str(),
        (ellipsoidParams.empty() ? "" : ("ELLIPSOID" + ellipsoidParams).c_str()),
        xRange.c_str(), yRange.c_str(),
        organization.c_str(),
        Literal(wkt).c_str(),
        Literal(proj4).c_str());

    ExecuteSQL(sql.c_str());
}

/************************************************************************/
/*                       CreateParseArrayFunctions()                    */
/************************************************************************/

void OGRHanaDataSource::CreateParseArrayFunctions(const char* schemaName)
{
    auto replaceAll = [](const CPLString& str, const CPLString& before,
                         const CPLString& after) {
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

    CPLString sql = replaceAll(
        parseStringArrayFunc, "{SCHEMA}", QuotedIdentifier(schemaName));
    ExecuteSQL(sql.c_str());

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

    sql = replaceAll(
        parseTypeArrayFunc, "{SCHEMA}", QuotedIdentifier(schemaName));

    for (const CPLString& type : GetSupportedArrayTypes())
    {
        if (type == "STRING")
            continue;
        ExecuteSQL(replaceAll(sql, "{TYPE}", type).c_str());
    }
}

/************************************************************************/
/*                       ParseArrayFunctionsExist()                     */
/************************************************************************/

bool OGRHanaDataSource::ParseArrayFunctionsExist(const char* schemaName)
{
    const char* sql =
        "SELECT COUNT(*) FROM FUNCTIONS WHERE SCHEMA_NAME = ? AND "
        "FUNCTION_NAME LIKE 'OGR_PARSE_%_ARRAY'";
    odbc::PreparedStatementRef stmt = conn_->prepareStatement(sql);
    stmt->setString(1, odbc::String(schemaName));
    odbc::ResultSetRef rsFunctions = stmt->executeQuery();
    auto numFunctions = rsFunctions->next() ? *rsFunctions->getLong(1) : 0;
    rsFunctions->close();
    return (
        static_cast<std::size_t>(numFunctions)
        == GetSupportedArrayTypes().size());
}

/************************************************************************/
/*                               GetLayer()                             */
/************************************************************************/

OGRLayer* OGRHanaDataSource::GetLayer(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size())
        return nullptr;
    return layers_[static_cast<std::size_t>(index)].get();
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer* OGRHanaDataSource::GetLayerByName(const char* name)
{
    return GetLayer(FindLayerByName(name));
}

/************************************************************************/
/*                              ICreateLayer()                          */
/************************************************************************/

OGRLayer* OGRHanaDataSource::ICreateLayer(
    const char* layerNameIn,
    OGRSpatialReference* srs,
    OGRwkbGeometryType geomType,
    char** options)
{
    if (layerNameIn == nullptr)
        return nullptr;

    // Check if we are allowed to create new objects in the database
    odbc::DatabaseMetaDataRef dmd = conn_->getDatabaseMetaData();
    if (dmd->isReadOnly())
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to create Layer %s.\n"
            "Database %s is read only.",
            layerNameIn, dmd->getDatabaseName().c_str());
        return nullptr;
    }

    bool launderNames =
        CPLFetchBool(options, LayerCreationOptionsConstants::LAUNDER, true);
    CPLString layerName =
        launderNames ? LaunderName(layerNameIn) : CPLString(layerNameIn);

    CPLDebug("HANA", "Creating layer %s.", layerName.c_str());

    int layerIndex = FindLayerByName(layerName.c_str());
    if (layerIndex >= 0)
    {
        bool overwriteLayer = CPLFetchBool(options, LayerCreationOptionsConstants::OVERWRITE, false);
        if (!overwriteLayer)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Layer %s already exists, CreateLayer failed.\n"
                "Use the layer creation option OVERWRITE=YES to "
                "replace it.",
                layerName.c_str());
            return nullptr;
        }

        DeleteLayer(layerIndex);
    }

    int batchSize = CPLFetchInt(options, LayerCreationOptionsConstants::BATCH_SIZE,
                                DEFAULT_BATCH_SIZE);
    if (batchSize <= 0)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to create layer %s. The value of %s parameter must be "
            "greater than 0.",
            layerName.c_str(), LayerCreationOptionsConstants::BATCH_SIZE);
        return nullptr;
    }

    int defaultStringSize = CPLFetchInt(options, LayerCreationOptionsConstants::DEFAULT_STRING_SIZE,
                                        DEFAULT_STRING_SIZE);
    if (defaultStringSize <= 0)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to create layer %s. The value of %s parameter must be "
            "greater than 0.",
            layerName.c_str(),
            LayerCreationOptionsConstants::DEFAULT_STRING_SIZE);
        return nullptr;
    }

    CPLString geomColumnName(CSLFetchNameValueDef(options, LayerCreationOptionsConstants::GEOMETRY_NAME, "OGR_GEOMETRY"));
    const bool geomColumnNullable = CPLFetchBool(options, LayerCreationOptionsConstants::GEOMETRY_NULLABLE, true);
    CPLString geomColumnIndexType(CSLFetchNameValueDef(options, LayerCreationOptionsConstants::GEOMETRY_INDEX, "DEFAULT"));

    const char* paramFidName = CSLFetchNameValueDef(options, LayerCreationOptionsConstants::FID, "OGR_FID");
    CPLString fidName(launderNames ? LaunderName(paramFidName).c_str() : paramFidName);
    CPLString fidType = CPLFetchBool(options, LayerCreationOptionsConstants::FID64, false) ? "BIGINT" : "INTEGER";

    CPLDebug("HANA", "Geometry Column Name %s.", geomColumnName.c_str());
    CPLDebug("HANA", "FID Column Name %s, Type %s.", fidName.c_str(),
             fidType.c_str());

    int srid = CPLFetchInt(options, LayerCreationOptionsConstants::SRID, UNDETERMINED_SRID);
    if (srid < 0 && srs != nullptr)
        srid = GetSrsId(srs);

    try
    {
        CreateTable(
            layerName, fidName, fidType,
            geomColumnName, geomType, geomColumnNullable, geomColumnIndexType, srid);
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Unable to create layer %s. CreateLayer failed:%s\n",
            layerName.c_str(), ex.what());
        return nullptr;
    }

    // Create new layer object
    auto layer = cpl::make_unique<OGRHanaTableLayer>(this, schemaName_.c_str(), layerName.c_str(), true);
    if (geomType != wkbNone && layer->GetLayerDefn()->GetGeomFieldCount() > 0)
        layer->GetLayerDefn()->GetGeomFieldDefn(0)->SetNullable(FALSE);
    if (batchSize > 0)
        layer->SetBatchSize(static_cast<std::size_t>(batchSize));
    if (defaultStringSize > 0)
        layer->SetDefaultStringSize(
            static_cast<std::size_t>(defaultStringSize));
    layer->SetLaunderFlag(launderNames);
    layer->SetPrecisionFlag(
        CPLFetchBool(options, LayerCreationOptionsConstants::PRECISION, true));
    layer->SetCustomColumnTypes(CSLFetchNameValue(
        options, LayerCreationOptionsConstants::COLUMN_TYPES));

    layers_.push_back(std::move(layer));

    return layers_[layers_.size() - 1].get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHanaDataSource::TestCapability(const char* capabilities)
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

OGRLayer* OGRHanaDataSource::ExecuteSQL(
    const char* sqlCommand, OGRGeometry* spatialFilter, const char* dialect)
{
    sqlCommand = SkipLeadingSpaces(sqlCommand);

    if (IsGenericSQLDialect(dialect))
        return GDALDataset::ExecuteSQL(sqlCommand, spatialFilter, dialect);

    if (STARTS_WITH_CI(sqlCommand, "DELLAYER:"))
    {
        const char* layerName = SkipLeadingSpaces(sqlCommand + 9);
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

        auto layer = cpl::make_unique<OGRHanaResultLayer>(this, sqlCommand);
        if (spatialFilter != nullptr)
            layer->SetSpatialFilter( spatialFilter );
        return layer.release();
    }

    try
    {
        ExecuteSQL(sqlCommand);
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Failed to execute SQL statement '%s': %s", sqlCommand, ex.what());
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
        CPLError(
            CE_Failure, CPLE_AppDefined, "Transaction already established");
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
        conn_->commit();
    }
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to commit transaction: %s",
            ex.what());
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
    catch (const odbc::Exception& ex)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "Failed to roll back transaction: %s",
            ex.what());
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}
