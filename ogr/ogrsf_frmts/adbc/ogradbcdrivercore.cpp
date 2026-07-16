/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdalplugindriverproxy.h"

#include "ogrsf_frmts.h"
#include "ogradbcdrivercore.h"
#include "gdal_adbc.h"

/************************************************************************/
/*                              IsDuckDB()                              */
/************************************************************************/

bool OGRADBCDriverIsDuckDB(const GDALOpenInfo *poOpenInfo)
{
    return poOpenInfo->nHeaderBytes >= 20 &&
           memcmp(poOpenInfo->pabyHeader + 8, "DUCK\x40\0\0\0\0\0\0\0",
                  4 + 8) == 0;
}

/************************************************************************/
/*                             IsSQLite3()                              */
/************************************************************************/

bool OGRADBCDriverIsSQLite3(const GDALOpenInfo *poOpenInfo)
{
    constexpr const char *SQLITE3_SIGNATURE = "SQLite format 3";
    return poOpenInfo->nHeaderBytes >= 512 &&
           memcmp(poOpenInfo->pabyHeader, SQLITE3_SIGNATURE,
                  strlen(SQLITE3_SIGNATURE)) == 0;
}

/************************************************************************/
/*                             IsParquet()                              */
/************************************************************************/

bool OGRADBCDriverIsParquet(const GDALOpenInfo *poOpenInfo)
{
    constexpr const char *PARQUET_SIGNATURE = "PAR1";
    return poOpenInfo->nHeaderBytes >=
               static_cast<int>(strlen(PARQUET_SIGNATURE)) &&
           memcmp(poOpenInfo->pabyHeader, PARQUET_SIGNATURE,
                  strlen(PARQUET_SIGNATURE)) == 0;
}

/************************************************************************/
/*                       OGRADBCDriverIdentify()                        */
/************************************************************************/

int OGRADBCDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, "ADBC:"))
        return true;

    if (STARTS_WITH(poOpenInfo->pszFilename, "/vsi"))
        return false;

    if (OGRADBCDriverIsDuckDB(poOpenInfo) || OGRADBCDriverIsParquet(poOpenInfo))
    {
#ifdef OGR_ADBC_HAS_DRIVER_MANAGER
        return true;
#else
        const char *pszADBCDriverName =
#ifdef _WIN32
            "duckdb.dll"
#elif defined(__MACH__) && defined(__APPLE__)
            "libduckdb.dylib"
#else
            "libduckdb.so"
#endif
            ;
        return (GDALGetAdbcLoadDriverOverride() != nullptr ||
                CPLGetSymbol(pszADBCDriverName, "duckdb_adbc_init") != nullptr);
#endif
    }
    else
    {
        return !poOpenInfo->IsExtensionEqualToCI("mbtiles") &&
               (OGRADBCDriverIsSQLite3(poOpenInfo) &&
                ((!poOpenInfo->IsExtensionEqualToCI("gpkg") &&
                  (GDALGetDriverByName("SQLite") == nullptr ||
                   poOpenInfo->IsSingleAllowedDriver("ADBC"))) ||
                 (poOpenInfo->IsExtensionEqualToCI("gpkg") &&
                  (GDALGetDriverByName("GPKG") == nullptr ||
                   poOpenInfo->IsSingleAllowedDriver("ADBC")))))
#ifndef OGR_ADBC_HAS_DRIVER_MANAGER
               && GDALGetAdbcLoadDriverOverride() != nullptr
#endif
            ;
    }
}

/************************************************************************/
/*                   OGRADBCDriverSetCommonMetadata()                   */
/************************************************************************/

void OGRADBCDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Arrow Database Connectivity");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/adbc.html");

    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "ADBC:");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='ADBC_DRIVER' type='string' description='ADBC driver "
        "name'/>"
        "  <Option name='SQL' type='string' "
        "description='SQL statement from which to build layer'/>"
        "  <Option name='ADBC_OPTION_*' type='string' "
        "description='Option to pass to AdbcDatabaseSetOption()'/>"
        "  <Option name='PRELUDE_STATEMENTS' type='string' description='SQL "
        "statement(s) to send on the database connection before any other "
        "ones'/>"
#ifdef OGR_ADBC_HAS_DRIVER_MANAGER
        "  <Option name='BIGQUERY_PROJECT_ID' type='string' "
        "description='Google Project ID'/>"
        "  <Option name='BIGQUERY_DATASET_ID' type='string' "
        "description='Google BigQuery dataset ID'/>"
        "  <Option name='BIGQUERY_JSON_CREDENTIAL_STRING' type='string' "
        "description='JSON string containing Google credentials'/>"
        "  <Option name='BIGQUERY_JSON_CREDENTIAL_FILE' type='string' "
        "description='Filename containing Google credentials'/>"
#endif
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");
#ifdef OGR_ADBC_HAS_DRIVER_MANAGER
    poDriver->SetMetadataItem("HAS_ADBC_DRIVER_MANAGER", "YES");

    // For BigQuery
    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "<Option name='FID' type='string' "
        "description='Name of the FID column to create' default='ogc_fid'/>"
        "</LayerCreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date Time DateTime Binary StringList "
        "IntegerList Integer64List RealList");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean JSON");
    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS, "Features");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->pfnIdentify = OGRADBCDriverIdentify;
}

/************************************************************************/
/*                    DeclareDeferredOGRADBCPlugin()                    */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRADBCPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    OGRADBCDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
