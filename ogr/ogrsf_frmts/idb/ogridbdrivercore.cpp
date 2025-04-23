/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDriver class.
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogridbdrivercore.h"

/************************************************************************/
/*                     OGRIDBDriverIdentify()                           */
/************************************************************************/

int OGRIDBDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "IDB:");
}

/************************************************************************/
/*                  OGRIDBDriverSetCommonMetadata()                     */
/************************************************************************/

void OGRIDBDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "IDB");

    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "IDB:");

    poDriver->pfnIdentify = OGRIDBDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRIDBPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRIDBPlugin()
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
    OGRIDBDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
