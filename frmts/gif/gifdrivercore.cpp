/******************************************************************************
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gifdrivercore.h"

/************************************************************************/
/*                     GIFDriverIdentify()                              */
/************************************************************************/

int GIFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 8 || !poOpenInfo->fpL)
        return FALSE;

    return (memcmp(poOpenInfo->pabyHeader, "GIF87a", 6) == 0 ||
            memcmp(poOpenInfo->pabyHeader, "GIF89a", 6) == 0);
}

/************************************************************************/
/*                      BIGGIFDriverSetCommonMetadata()                 */
/************************************************************************/

void BIGGIFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(BIGGIF_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Graphics Interchange Format (.gif)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gif.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gif");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/gif");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = GIFDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                      GIFDriverSetCommonMetadata()                    */
/************************************************************************/

void GIFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(GIF_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Graphics Interchange Format (.gif)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gif.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gif");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/gif");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList>\n"
                              "   <Option name='INTERLACING' type='boolean'/>\n"
                              "   <Option name='WORLDFILE' type='boolean'/>\n"
                              "</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = GIFDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredGIFPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredGIFPlugin()
{
    if (GDALGetDriverByName(GIF_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        GIFDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        BIGGIFDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif
