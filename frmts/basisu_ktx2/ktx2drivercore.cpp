/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ktx2drivercore.h"
#include "commoncore.h"

/************************************************************************/
/*                     KTX2DriverIdentify()                             */
/************************************************************************/

int KTX2DriverIdentify(GDALOpenInfo *poOpenInfo)

{
    constexpr GByte KTX2Signature[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                                       0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    if (STARTS_WITH(poOpenInfo->pszFilename, "KTX2:"))
        return true;
    return poOpenInfo->fpL != nullptr &&
           poOpenInfo->nHeaderBytes >=
               static_cast<int>(sizeof(KTX2Signature)) &&
           memcmp(poOpenInfo->pabyHeader, KTX2Signature,
                  sizeof(KTX2Signature)) == 0;
}

/************************************************************************/
/*                      KTX2DriverSetCommonMetadata()                   */
/************************************************************************/

void KTX2DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(KTX2_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "KTX2 texture format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/ktx2.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "ktx2");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        GDAL_KTX2_BASISU_GetCreationOptions(/* bIsKTX2 = */ true).c_str());

    poDriver->pfnIdentify = KTX2DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredKTX2Plugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredKTX2Plugin()
{
    if (GDALGetDriverByName(KTX2_DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    KTX2DriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
