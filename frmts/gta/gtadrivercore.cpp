/******************************************************************************
 *
 * Project:  GTA read/write Driver
 * Purpose:  GDAL bindings over GTA library.
 * Author:   Martin Lambers, marlam@marlam.de
 *
 ******************************************************************************
 * Copyright (c) 2010, 2011, Martin Lambers <marlam@marlam.de>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gtadrivercore.h"

/************************************************************************/
/*                     GTADriverIdentify()                              */
/************************************************************************/

int GTADriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 5)
        return GDAL_IDENTIFY_FALSE;

    if (!STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "GTA"))
        return GDAL_IDENTIFY_FALSE;

    return GDAL_IDENTIFY_UNKNOWN;
}

/************************************************************************/
/*                      GTADriverSetCommonMetadata()                    */
/************************************************************************/

void GTADriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Generic Tagged Arrays (.gta)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gta.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gta");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Byte Int8 UInt16 Int16 UInt32 Int32 Float32 Float64 "
        "CInt16 CInt32 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList>"
                              "  <Option name='COMPRESS' type='string-select'>"
                              "    <Value>NONE</Value>"
                              "    <Value>BZIP2</Value>"
                              "    <Value>XZ</Value>"
                              "    <Value>ZLIB</Value>"
                              "    <Value>ZLIB1</Value>"
                              "    <Value>ZLIB2</Value>"
                              "    <Value>ZLIB3</Value>"
                              "    <Value>ZLIB4</Value>"
                              "    <Value>ZLIB5</Value>"
                              "    <Value>ZLIB6</Value>"
                              "    <Value>ZLIB7</Value>"
                              "    <Value>ZLIB8</Value>"
                              "    <Value>ZLIB9</Value>"
                              "  </Option>"
                              "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = GTADriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredGTAPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredGTAPlugin()
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
    GTADriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
