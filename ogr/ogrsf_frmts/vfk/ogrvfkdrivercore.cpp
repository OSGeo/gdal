/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDriver class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2018, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrvfkdrivercore.h"

/************************************************************************/
/*                    OGRVFKDriverIdentify()                            */
/************************************************************************/

int OGRVFKDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr)
        return FALSE;

    if (poOpenInfo->nHeaderBytes >= 2 &&
        STARTS_WITH((const char *)poOpenInfo->pabyHeader, "&H"))
        return TRUE;

    /* valid datasource can be also SQLite DB previously created by
       VFK driver, the real check is done by VFKReaderSQLite */
    if (poOpenInfo->nHeaderBytes >= 100 &&
        STARTS_WITH((const char *)poOpenInfo->pabyHeader, "SQLite format 3") &&
        !poOpenInfo->IsExtensionEqualToCI("gpkg"))
    {
        // The driver is not ready for virtual file systems
        if (STARTS_WITH(poOpenInfo->pszFilename, "/vsi"))
            return FALSE;

        VSIStatBufL sStat;
        if (VSIStatL(poOpenInfo->pszFilename, &sStat) == 0 &&
            VSI_ISREG(sStat.st_mode))
        {
            return GDAL_IDENTIFY_UNKNOWN;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                  OGRVFKDriverSetCommonMetadata()                  */
/************************************************************************/

void OGRVFKDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Czech Cadastral Exchange Data Format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "vfk");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/vfk.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='SUPPRESS_GEOMETRY' type='boolean' "
        "description='whether to suppress geometry' default='NO'/>"
        "  <Option name='FILE_FIELD' type='boolean' description='whether to "
        "include VFK filename field' default='NO'/>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGRVFKDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRVFKPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRVFKPlugin()
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
    OGRVFKDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
