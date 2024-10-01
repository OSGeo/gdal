/******************************************************************************
 *
 * Project:  AVIF Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "avifdrivercore.h"

/************************************************************************/
/*                        AVIFDriverIdentify()                          */
/************************************************************************/

int AVIFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "AVIF:"))
        return true;

    if (poOpenInfo->nHeaderBytes < 12 || poOpenInfo->fpL == nullptr)
        return false;

    return memcmp(poOpenInfo->pabyHeader + 4, "ftypavif", 8) == 0 ||
           memcmp(poOpenInfo->pabyHeader + 4, "ftypavis", 8) == 0;
}

/************************************************************************/
/*                     AVIFDriverSetCommonMetadata()                    */
/************************************************************************/

void AVIFDriverSetCommonMetadata(GDALDriver *poDriver,
                                 bool bMayHaveWriteSupport)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "AV1 Image File Format");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/avif");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/avif.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "avif");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnIdentify = AVIFDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");

    if (bMayHaveWriteSupport)
    {
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16");
        poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
    }
}

/************************************************************************/
/*                     DeclareDeferredAVIFPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredAVIFPlugin()
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
    AVIFDriverSetCommonMetadata(poDriver, /* bMayHaveWriteSupport = */ true);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
