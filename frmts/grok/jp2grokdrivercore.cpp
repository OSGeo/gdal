/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JP2Grok driver
 * Author:   Aaron Boxer
 *
 ******************************************************************************
 * Copyright (c) 2026, Grok Image Compression Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"

#ifdef PLUGIN_FILENAME
#include "gdalplugindriverproxy.h"
#endif

#include "jp2grokdrivercore.h"

/************************************************************************/
/*                   JP2GrokDriverSetCommonMetadata()                   */
/************************************************************************/

void JP2GrokDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "JPEG-2000 driver based on Grok library");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jp2grok.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jp2 j2k");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description="
        "'Whether a 1-bit alpha channel should be promoted to 8-bit' "
        "default='YES'/>"
        "   <Option name='OPEN_REMOTE_GML' type='boolean' description="
        "'Whether to load remote vector layers referenced by "
        "a link in a GMLJP2 v2 box' default='NO'/>"
        "   <Option name='GEOREF_SOURCES' type='string' description="
        "'Comma separated list made with values "
        "INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority "
        "order "
        "for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
        "   <Option name='USE_TILE_AS_BLOCK' type='boolean' "
        "description='Whether to always use the JPEG-2000 block size as the "
        "GDAL block size' default='NO'/>"
        "</OpenOptionList>");

    /* Creation options set in JP2GRKDatasetBase::setMetaData() at runtime */

    poDriver->pfnIdentify = JP2GrokDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

#ifndef jpc_header_defined
#define jpc_header_defined
static const unsigned char _jpc_header[] = {0xff, 0x4f, 0xff,
                                            0x51};  // SOC + RSIZ markers
static const unsigned char _jp2_box_jp[] = {0x6a, 0x50, 0x20,
                                            0x20}; /* 'jP  ' */
#endif

int JP2GrokDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes >= 16 &&
        (memcmp(poOpenInfo->pabyHeader, _jpc_header, sizeof(_jpc_header)) ==
             0 ||
         memcmp(poOpenInfo->pabyHeader + 4, _jp2_box_jp, sizeof(_jp2_box_jp)) ==
             0))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                    DeclareDeferredJP2GrokPlugin()                    */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJP2GrokPlugin()
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
    JP2GrokDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
