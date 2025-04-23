/******************************************************************************
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Alan Boudreault, aboudreault@mapgears.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Alan Boudreault
 * Copyright (c) 2013,2019, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ddsdrivercore.h"

/************************************************************************/
/*                     DDSDriverIdentify()                              */
/************************************************************************/

int DDSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    constexpr int sizeof_DDSURFACEDESC2 = 31 * 4;
    if (poOpenInfo->fpL == nullptr || poOpenInfo->eAccess == GA_Update ||
        static_cast<size_t>(poOpenInfo->nHeaderBytes) <
            strlen(DDS_SIGNATURE) + sizeof_DDSURFACEDESC2)
    {
        return false;
    }

    // Check signature and dwSize member of DDSURFACEDESC2
    return memcmp(poOpenInfo->pabyHeader, DDS_SIGNATURE,
                  strlen(DDS_SIGNATURE)) == 0 &&
           CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + strlen(DDS_SIGNATURE)) ==
               sizeof_DDSURFACEDESC2;
}

/************************************************************************/
/*                      DDSDriverSetCommonMetadata()                    */
/************************************************************************/

void DDSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "DirectDraw Surface");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/dds.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dds");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/dds");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='FORMAT' type='string-select' description='Texture "
        "format' default='DXT3'>\n"
        "     <Value>DXT1</Value>\n"
        "     <Value>DXT1A</Value>\n"
        "     <Value>DXT3</Value>\n"
        "     <Value>DXT5</Value>\n"
        "     <Value>ETC1</Value>\n"
        "   </Option>\n"
        "   <Option name='QUALITY' type='string-select' "
        "description='Compression Quality' default='NORMAL'>\n"
        "     <Value>SUPERFAST</Value>\n"
        "     <Value>FAST</Value>\n"
        "     <Value>NORMAL</Value>\n"
        "     <Value>BETTER</Value>\n"
        "     <Value>UBER</Value>\n"
        "   </Option>\n"
        "</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = DDSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredDDSPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredDDSPlugin()
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
    DDSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
