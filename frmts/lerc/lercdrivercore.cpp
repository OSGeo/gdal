/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  LERC driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdalplugindriverproxy.h"

#include "lercdrivercore.h"
#include "cpl_string.h"

/************************************************************************/
/*                         LERCDriverIdentify()                         */
/************************************************************************/

int LERCDriverIdentify(GDALOpenInfo *poOpenInfo)

{
#if !defined(GDAL_USE_LERC_INTERNAL)
    static const char L1sig[] = "CntZImage ";
#endif
    static const char L2sig[] = "Lerc2 ";
    const char *pszHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    return pszHeader && poOpenInfo->nHeaderBytes >= 10 &&
           (
#if !defined(GDAL_USE_LERC_INTERNAL)
               strncmp(pszHeader, L1sig, sizeof(L1sig) - 1) == 0 ||
#endif
               strncmp(pszHeader, L2sig, sizeof(L2sig) - 1) == 0);
}

/************************************************************************/
/*                    LERCDriverSetCommonMetadata()                     */
/************************************************************************/

void LERCDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Limited Error Raster Compression");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/lerc.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "lrc");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");

    poDriver->pfnIdentify = LERCDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                     DeclareDeferredLERCPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredLERCPlugin()
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
    LERCDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
