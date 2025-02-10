/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrodbcdrivercore.h"

/************************************************************************/
/*                OGRODBCDriverIsSupportedMsAccessFileExtension()       */
/************************************************************************/

bool OGRODBCDriverIsSupportedMsAccessFileExtension(const char *pszExtension)
{
    // these are all possible extensions for MS Access databases
    return EQUAL(pszExtension, "MDB") || EQUAL(pszExtension, "ACCDB") ||
           EQUAL(pszExtension, "STYLE");
}

/************************************************************************/
/*                    OGRODBCDriverIdentify()                           */
/************************************************************************/

int OGRODBCDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:"))
    {
        return FALSE;
    }

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ODBC:"))
        return TRUE;

    const char *psExtension = poOpenInfo->osExtension.c_str();
    if (EQUAL(psExtension, "mdb"))
        return -1;  // Could potentially be a PGeo MDB database

    if (OGRODBCDriverIsSupportedMsAccessFileExtension(psExtension))
        return TRUE;  // An Access database which isn't a .MDB file (we checked
                      // that above), so this is the only candidate driver

    // doesn't start with "ODBC:", and isn't an access database => not supported
    return FALSE;
}

/************************************************************************/
/*                  OGRODBCDriverSetCommonMetadata()                    */
/************************************************************************/

void OGRODBCDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Open Database Connectivity (ODBC)");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "ODBC:");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "mdb accdb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/odbc.html");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' "
        "description='Whether all tables, including system and internal tables "
        "(such as MSys* tables) should be listed' default='NO'>"
        "    <Value>YES</Value>"
        "    <Value>NO</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGRODBCDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRODBCPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRODBCPlugin()
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
    OGRODBCDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
