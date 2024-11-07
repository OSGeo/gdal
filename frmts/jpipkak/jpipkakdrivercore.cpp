/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JPIPKAK driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "jpipkakdrivercore.h"

/************************************************************************/
/*                       JPIPKAKDatasetIdentify()                       */
/************************************************************************/

static int JPIPKAKDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "jpip://") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "jpips://");
}

/************************************************************************/
/*                   JPIPKAKDriverSetCommonMetadata()                   */
/************************************************************************/

void JPIPKAKDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPIP (based on Kakadu)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jpipkak.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jpp-stream");

    poDriver->pfnIdentify = JPIPKAKDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredJPIPKAKPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJPIPKAKPlugin()
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
    JPIPKAKDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
