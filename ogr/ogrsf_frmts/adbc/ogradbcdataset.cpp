/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Dewey Dunnington <dewey@voltrondata.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_adbc.h"
#include "ogradbcdrivercore.h"
#include "memdataset.h"
#include "ogr_p.h"
#include "cpl_error.h"
#include "cpl_json.h"
#include "gdal_adbc.h"

#if defined(OGR_ADBC_HAS_DRIVER_MANAGER)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <arrow-adbc/adbc_driver_manager.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

#define OGR_ADBC_VERSION ADBC_VERSION_1_1_0
static_assert(sizeof(AdbcDriver) == ADBC_DRIVER_1_1_0_SIZE);

namespace
{

AdbcStatusCode OGRADBCLoadDriver(const char *driver_name,
                                 const char *entrypoint, void *driver,
                                 struct AdbcError *error)
{
    GDALAdbcLoadDriverFunc load_driver_override =
        GDALGetAdbcLoadDriverOverride();
    if (load_driver_override)
    {
        return load_driver_override(driver_name, entrypoint, OGR_ADBC_VERSION,
                                    driver, error);
    }
    else
    {
#if defined(OGR_ADBC_HAS_DRIVER_MANAGER)
        return AdbcLoadDriver(driver_name, entrypoint, OGR_ADBC_VERSION, driver,
                              error);
#else
        return ADBC_STATUS_NOT_IMPLEMENTED;
#endif
    }
}

}  // namespace

// Helper to wrap driver callbacks
#define ADBC_CALL(func, ...) m_driver.func(__VA_ARGS__)

/************************************************************************/
/*                           ~OGRADBCDataset()                          */
/************************************************************************/

OGRADBCDataset::~OGRADBCDataset()
{
    // Layers must be closed before the connection
    m_apoLayers.clear();
    OGRADBCError error;
    if (m_connection)
        ADBC_CALL(ConnectionRelease, m_connection.get(), error);
    error.clear();
    if (m_driver.release)
    {
        ADBC_CALL(DatabaseRelease, &m_database, error);
        m_driver.release(&m_driver, error);
    }
}

/************************************************************************/
/*                           CreateLayer()                              */
/************************************************************************/

std::unique_ptr<OGRADBCLayer>
OGRADBCDataset::CreateLayer(const char *pszStatement, const char *pszLayerName,
                            bool bInternalUse)
{

    OGRADBCError error;

    CPLString osStatement(pszStatement);
    if (!m_osParquetFilename.empty())
    {
        const char *pszSrcLayerName = m_apoLayers.size() == 1
                                          ? m_apoLayers[0]->GetDescription()
                                          : pszLayerName;
        // Substitute the OGR layer name with the DuckDB expected filename,
        // single-quoted
        const std::string osFrom =
            std::string(" FROM ").append(pszSrcLayerName);
        const auto nPos = osStatement.ifind(osFrom);
        if (nPos != std::string::npos)
        {
            osStatement =
                osStatement.substr(0, nPos)
                    .append(" FROM '")
                    .append(OGRDuplicateCharacter(m_osParquetFilename, '\''))
                    .append("'")
                    .append(osStatement.substr(nPos + osFrom.size()));
        }
        else
        {
            const std::string osFrom2 =
                std::string(" FROM \"")
                    .append(OGRDuplicateCharacter(pszSrcLayerName, '"'))
                    .append("\"");
            const auto nPos2 = osStatement.ifind(osFrom2);
            if (nPos2 != std::string::npos)
            {
                osStatement =
                    osStatement.substr(0, nPos2)
                        .append(" FROM '")
                        .append(
                            OGRDuplicateCharacter(m_osParquetFilename, '\''))
                        .append("'")
                        .append(osStatement.substr(nPos2 + osFrom2.size()));
            }
        }
    }

    auto statement = std::make_unique<AdbcStatement>();
    if (ADBC_CALL(StatementNew, m_connection.get(), statement.get(), error) !=
        ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcStatementNew() failed: %s",
                 error.message());
        return nullptr;
    }

    if (ADBC_CALL(StatementSetSqlQuery, statement.get(), osStatement.c_str(),
                  error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AdbcStatementSetSqlQuery() failed: %s", error.message());
        error.clear();
        ADBC_CALL(StatementRelease, statement.get(), error);
        return nullptr;
    }

    auto stream = std::make_unique<OGRArrowArrayStream>();
    int64_t rows_affected = -1;
    if (ADBC_CALL(StatementExecuteQuery, statement.get(), stream->get(),
                  &rows_affected, error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AdbcStatementExecuteQuery() failed: %s", error.message());
        error.clear();
        ADBC_CALL(StatementRelease, statement.get(), error);
        return nullptr;
    }

    ArrowSchema schema = {};
    if (stream->get_schema(&schema) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed");
        ADBC_CALL(StatementRelease, statement.get(), error);
        return nullptr;
    }

    return std::make_unique<OGRADBCLayer>(
        this, pszLayerName, osStatement.c_str(), std::move(statement),
        std::move(stream), &schema, bInternalUse);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer *OGRADBCDataset::ExecuteSQL(const char *pszStatement,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect)
{
    if (pszDialect && pszDialect[0] != 0 && !EQUAL(pszDialect, "NATIVE"))
    {
        return GDALDataset::ExecuteSQL(pszStatement, poSpatialFilter,
                                       pszDialect);
    }

    auto poLayer = CreateLayer(pszStatement, "RESULTSET", false);
    if (poLayer && poSpatialFilter)
    {
        if (poLayer->GetGeomType() == wkbNone)
            return nullptr;
        poLayer->SetSpatialFilter(poSpatialFilter);
    }
    return poLayer.release();
}

/************************************************************************/
/*                       IsParquetExtension()                           */
/************************************************************************/

static bool IsParquetExtension(const char *pszStr)
{
    const std::string osExt = CPLGetExtensionSafe(pszStr);
    return EQUAL(osExt.c_str(), "parquet") || EQUAL(osExt.c_str(), "parq");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRADBCDataset::Open(const GDALOpenInfo *poOpenInfo)
{
    OGRADBCError error;

    const char *pszFilename = poOpenInfo->pszFilename;
    std::unique_ptr<GDALOpenInfo> poTmpOpenInfo;
    if (STARTS_WITH(pszFilename, "ADBC:"))
    {
        pszFilename += strlen("ADBC:");
        poTmpOpenInfo =
            std::make_unique<GDALOpenInfo>(pszFilename, GA_ReadOnly);
        poTmpOpenInfo->papszOpenOptions = poOpenInfo->papszOpenOptions;
        poOpenInfo = poTmpOpenInfo.get();
    }
    const char *pszADBCDriverName =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ADBC_DRIVER");
    m_bIsDuckDBDataset = OGRADBCDriverIsDuckDB(poOpenInfo);
    const bool bIsSQLite3 =
        (pszADBCDriverName && EQUAL(pszADBCDriverName, "adbc_driver_sqlite")) ||
        OGRADBCDriverIsSQLite3(poOpenInfo);
    bool bIsParquet =
        OGRADBCDriverIsParquet(poOpenInfo) || IsParquetExtension(pszFilename);
    const char *pszSQL = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "SQL");
    if (!bIsParquet && pszSQL)
    {
        CPLString osSQL(pszSQL);
        auto iPos = osSQL.find("FROM '");
        if (iPos != std::string::npos)
        {
            iPos += strlen("FROM '");
            const auto iPos2 = osSQL.find("'", iPos);
            if (iPos2 != std::string::npos)
            {
                const std::string osFilename = osSQL.substr(iPos, iPos2 - iPos);
                if (IsParquetExtension(osFilename.c_str()))
                {
                    m_osParquetFilename = osFilename;
                    bIsParquet = true;
                }
            }
        }
    }
    const bool bIsPostgreSQL = STARTS_WITH(pszFilename, "postgresql://");

    if (!pszADBCDriverName)
    {
        if (m_bIsDuckDBDataset || bIsParquet)
        {
            pszADBCDriverName =
#ifdef _WIN32
                "duckdb.dll"
#elif defined(__MACH__) && defined(__APPLE__)
                "libduckdb.dylib"
#else
                "libduckdb.so"
#endif
                ;
        }
        else if (bIsPostgreSQL)
            pszADBCDriverName = "adbc_driver_postgresql";
        else if (bIsSQLite3)
        {
            pszADBCDriverName = "adbc_driver_sqlite";
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Open option ADBC_DRIVER must be specified");
            return false;
        }
    }

    m_bIsDuckDBDriver =
        m_bIsDuckDBDataset || bIsParquet ||
        (pszADBCDriverName && strstr(pszADBCDriverName, "duckdb"));

    // Load the driver
    if (m_bIsDuckDBDriver)
    {
        if (OGRADBCLoadDriver(pszADBCDriverName, "duckdb_adbc_init", &m_driver,
                              error) != ADBC_STATUS_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "AdbcLoadDriver() failed: %s",
                     error.message());
            return false;
        }
    }
    else
    {
        if (OGRADBCLoadDriver(pszADBCDriverName, nullptr, &m_driver, error) !=
            ADBC_STATUS_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "AdbcLoadDriver() failed: %s",
                     error.message());
            return false;
        }
    }

    // Allocate the database
    if (ADBC_CALL(DatabaseNew, &m_database, error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcDatabaseNew() failed: %s",
                 error.message());
        return false;
    }

    // Set options
    if (m_bIsDuckDBDriver && pszFilename[0])
    {
        VSIStatBuf sStatBuf;
        if (!bIsParquet && VSIStat(pszFilename, &sStatBuf) != 0 &&
            strcmp(pszFilename, ":memory:") != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s does not exist",
                     pszFilename);
            return false;
        }
        if (ADBC_CALL(DatabaseSetOption, &m_database, "path",
                      bIsParquet ? ":memory:" : pszFilename,
                      error) != ADBC_STATUS_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "AdbcDatabaseSetOption() failed: %s", error.message());
            return false;
        }
    }
    else if (pszFilename[0])
    {
        if (ADBC_CALL(DatabaseSetOption, &m_database, "uri", pszFilename,
                      error) != ADBC_STATUS_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "AdbcDatabaseSetOption() failed: %s", error.message());
            return false;
        }
    }

    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(
             static_cast<CSLConstList>(poOpenInfo->papszOpenOptions)))
    {
        if (STARTS_WITH_CI(pszKey, "ADBC_OPTION_"))
        {
            if (ADBC_CALL(DatabaseSetOption, &m_database,
                          pszKey + strlen("ADBC_OPTION_"), pszValue,
                          error) != ADBC_STATUS_OK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "AdbcDatabaseSetOption() failed: %s", error.message());
                return false;
            }
        }
    }

    if (ADBC_CALL(DatabaseInit, &m_database, error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcDatabaseInit() failed: %s",
                 error.message());
        return false;
    }

    m_connection = std::make_unique<AdbcConnection>();
    if (ADBC_CALL(ConnectionNew, m_connection.get(), error) != ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcConnectionNew() failed: %s",
                 error.message());
        return false;
    }

    if (ADBC_CALL(ConnectionInit, m_connection.get(), &m_database, error) !=
        ADBC_STATUS_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AdbcConnectionInit() failed: %s",
                 error.message());
        return false;
    }

    char **papszPreludeStatements = CSLFetchNameValueMultiple(
        poOpenInfo->papszOpenOptions, "PRELUDE_STATEMENTS");
    for (const char *pszStatement :
         cpl::Iterate(CSLConstList(papszPreludeStatements)))
    {
        CreateInternalLayer(pszStatement);
    }
    CSLDestroy(papszPreludeStatements);
    if (m_bIsDuckDBDriver && CPLTestBool(CPLGetConfigOption(
                                 "OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", "ON")))
    {
        auto poTmpLayer =
            CreateInternalLayer("SELECT 1 FROM duckdb_extensions() WHERE "
                                "extension_name='spatial' AND loaded = false");
        if (poTmpLayer && std::unique_ptr<OGRFeature>(
                              poTmpLayer->GetNextFeature()) != nullptr)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CreateInternalLayer("LOAD spatial");
        }

        poTmpLayer =
            CreateInternalLayer("SELECT 1 FROM duckdb_extensions() WHERE "
                                "extension_name='spatial' AND loaded = true");
        m_bSpatialLoaded =
            poTmpLayer && std::unique_ptr<OGRFeature>(
                              poTmpLayer->GetNextFeature()) != nullptr;
    }

    std::string osLayerName = "RESULTSET";
    std::string osSQL;
    bool bIsParquetLayer = false;
    if (bIsParquet)
    {
        if (m_osParquetFilename.empty())
            m_osParquetFilename = pszFilename;
        osLayerName = CPLGetBasenameSafe(m_osParquetFilename.c_str());
        if (osLayerName == "*")
            osLayerName = CPLGetBasenameSafe(
                CPLGetDirnameSafe(m_osParquetFilename.c_str()).c_str());
        if (!pszSQL)
        {
            osSQL =
                CPLSPrintf("SELECT * FROM read_parquet('%s')",
                           OGRDuplicateCharacter(pszFilename, '\'').c_str());
            pszSQL = osSQL.c_str();
            bIsParquetLayer = true;
        }
    }

    if (pszSQL)
    {
        if (pszSQL[0])
        {
            std::unique_ptr<OGRADBCLayer> poLayer;
            if ((bIsParquet || m_bIsDuckDBDataset) && m_bSpatialLoaded)
            {
                std::string osErrorMsg;
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    poLayer = CreateLayer(pszSQL, osLayerName.c_str(), false);
                    if (!poLayer)
                        osErrorMsg = CPLGetLastErrorMsg();
                }
                if (!poLayer)
                {
                    CPLDebug("ADBC",
                             "Connecting with 'LOAD spatial' did not work "
                             "(%s). Retrying without it",
                             osErrorMsg.c_str());
                    ADBC_CALL(ConnectionRelease, m_connection.get(), error);
                    m_connection.reset();

                    ADBC_CALL(DatabaseRelease, &m_database, error);
                    memset(&m_database, 0, sizeof(m_database));

                    if (ADBC_CALL(DatabaseNew, &m_database, error) !=
                        ADBC_STATUS_OK)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "AdbcDatabaseNew() failed: %s",
                                 error.message());
                        return false;
                    }
                    if (ADBC_CALL(DatabaseSetOption, &m_database, "path",
                                  ":memory:", error) != ADBC_STATUS_OK)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "AdbcDatabaseSetOption() failed: %s",
                                 error.message());
                        return false;
                    }

                    if (ADBC_CALL(DatabaseInit, &m_database, error) !=
                        ADBC_STATUS_OK)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "AdbcDatabaseInit() failed: %s",
                                 error.message());
                        return false;
                    }

                    m_connection = std::make_unique<AdbcConnection>();
                    if (ADBC_CALL(ConnectionNew, m_connection.get(), error) !=
                        ADBC_STATUS_OK)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "AdbcConnectionNew() failed: %s",
                                 error.message());
                        return false;
                    }

                    if (ADBC_CALL(ConnectionInit, m_connection.get(),
                                  &m_database, error) != ADBC_STATUS_OK)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "AdbcConnectionInit() failed: %s",
                                 error.message());
                        return false;
                    }
                }
            }
            if (!poLayer)
            {
                poLayer = CreateLayer(pszSQL, osLayerName.c_str(), false);
                if (!poLayer)
                    return false;
            }

            poLayer->m_bIsParquetLayer = bIsParquetLayer;
            m_apoLayers.emplace_back(std::move(poLayer));
        }
    }
    else if (m_bIsDuckDBDataset || bIsSQLite3)
    {
        auto poLayerList = CreateInternalLayer(
            "SELECT name FROM sqlite_master WHERE type IN ('table', 'view')");
        if (!poLayerList || poLayerList->GetLayerDefn()->GetFieldCount() != 1)
        {
            return false;
        }

        for (const auto &poFeature : poLayerList.get())
        {
            const char *pszLayerName = poFeature->GetFieldAsString(0);
            if (bIsSQLite3 && EQUAL(pszLayerName, "SpatialIndex"))
                continue;
            const std::string osStatement =
                CPLSPrintf("SELECT * FROM \"%s\"",
                           OGRDuplicateCharacter(pszLayerName, '"').c_str());
            CPLTurnFailureIntoWarningBackuper oErrorsToWarnings{};
            auto poLayer =
                CreateLayer(osStatement.c_str(), pszLayerName, false);
            if (poLayer)
            {
                m_apoLayers.emplace_back(std::move(poLayer));
            }
        }
    }
    else if (bIsPostgreSQL)
    {
        auto poLayerList = CreateInternalLayer(
            "SELECT n.nspname, c.relname FROM pg_class c "
            "JOIN pg_namespace n ON c.relnamespace = n.oid "
            "AND c.relkind in ('r','v','m','f') "
            "AND n.nspname NOT IN ('pg_catalog', 'information_schema') "
            "ORDER BY c.oid");
        if (!poLayerList || poLayerList->GetLayerDefn()->GetFieldCount() != 2)
        {
            return false;
        }

        for (const auto &poFeature : poLayerList.get())
        {
            const char *pszNamespace = poFeature->GetFieldAsString(0);
            const char *pszTableName = poFeature->GetFieldAsString(1);
            const std::string osStatement =
                CPLSPrintf("SELECT * FROM \"%s\".\"%s\"",
                           OGRDuplicateCharacter(pszNamespace, '"').c_str(),
                           OGRDuplicateCharacter(pszTableName, '"').c_str());

            CPLTurnFailureIntoWarningBackuper oErrorsToWarnings{};
            auto poLayer = CreateLayer(
                osStatement.c_str(),
                CPLSPrintf("%s.%s", pszNamespace, pszTableName), false);
            if (poLayer)
            {
                m_apoLayers.emplace_back(std::move(poLayer));
            }
        }
    }

    return true;
}

/************************************************************************/
/*                         GetLayerByName()                             */
/************************************************************************/

OGRLayer *OGRADBCDataset::GetLayerByName(const char *pszName)
{
    OGRLayer *poLayer = GDALDataset::GetLayerByName(pszName);
    if (poLayer || !EQUAL(pszName, "table_list"))
        return poLayer;

    OGRADBCError error;
    auto objectsStream = std::make_unique<OGRArrowArrayStream>();
    ADBC_CALL(ConnectionGetObjects, m_connection.get(),
              ADBC_OBJECT_DEPTH_TABLES,
              /* catalog = */ nullptr,
              /* db_schema = */ nullptr,
              /* table_name = */ nullptr,
              /* table_type = */ nullptr,
              /* column_name = */ nullptr, objectsStream->get(), error);

    ArrowSchema schema = {};
    if (objectsStream->get_schema(&schema) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed");
        return nullptr;
    }

    auto statement = std::make_unique<AdbcStatement>();
    OGRADBCLayer tmpLayer(this, "", "", std::move(statement),
                          std::move(objectsStream), &schema,
                          /* bInternalUse = */ true);
    const auto tmpLayerDefn = tmpLayer.GetLayerDefn();
    if (tmpLayerDefn->GetFieldIndex("catalog_name") < 0 ||
        tmpLayerDefn->GetFieldIndex("catalog_db_schemas") < 0)
    {
        return nullptr;
    }

    auto poTableListLayer =
        std::make_unique<OGRMemLayer>("table_list", nullptr, wkbNone);
    {
        OGRFieldDefn oField("catalog_name", OFTString);
        poTableListLayer->CreateField(&oField);
    }
    {
        OGRFieldDefn oField("schema_name", OFTString);
        poTableListLayer->CreateField(&oField);
    }
    {
        OGRFieldDefn oField("table_name", OFTString);
        poTableListLayer->CreateField(&oField);
    }
    {
        OGRFieldDefn oField("table_type", OFTString);
        poTableListLayer->CreateField(&oField);
    }

    for (const auto &poFeature : tmpLayer)
    {
        const char *pszCatalogName =
            poFeature->GetFieldAsString("catalog_name");
        const char *pszCatalogDBSchemas =
            poFeature->GetFieldAsString("catalog_db_schemas");
        if (pszCatalogDBSchemas)
        {
            CPLJSONDocument oDoc;
            if (oDoc.LoadMemory(pszCatalogDBSchemas))
            {
                auto oRoot = oDoc.GetRoot();
                if (oRoot.GetType() == CPLJSONObject::Type::Array)
                {
                    for (const auto &oSchema : oRoot.ToArray())
                    {
                        if (oSchema.GetType() == CPLJSONObject::Type::Object)
                        {
                            const std::string osSchemaName =
                                oSchema.GetString("schema_name");
                            const auto oTables =
                                oSchema.GetArray("db_schema_tables");
                            if (oTables.IsValid())
                            {
                                for (const auto &oTable : oTables)
                                {
                                    if (oTable.GetType() ==
                                        CPLJSONObject::Type::Object)
                                    {
                                        const std::string osTableName =
                                            oTable.GetString("table_name");
                                        const std::string osTableType =
                                            oTable.GetString("table_type");
                                        if (!osTableName.empty() &&
                                            osTableType != "index" &&
                                            osTableType != "trigger")
                                        {
                                            OGRFeature oFeat(
                                                poTableListLayer
                                                    ->GetLayerDefn());
                                            if (pszCatalogName)
                                                oFeat.SetField("catalog_name",
                                                               pszCatalogName);
                                            if (oSchema.GetObj("schema_name")
                                                    .IsValid())
                                                oFeat.SetField(
                                                    "schema_name",
                                                    osSchemaName.c_str());
                                            oFeat.SetField("table_name",
                                                           osTableName.c_str());
                                            if (oTable.GetObj("table_type")
                                                    .IsValid())
                                                oFeat.SetField(
                                                    "table_type",
                                                    osTableType.c_str());
                                            CPL_IGNORE_RET_VAL(
                                                poTableListLayer->CreateFeature(
                                                    &oFeat));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    m_apoLayers.emplace_back(std::move(poTableListLayer));
    return m_apoLayers.back().get();
}

#undef ADBC_CALL
