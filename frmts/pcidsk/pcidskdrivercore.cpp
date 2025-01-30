/******************************************************************************
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidskdrivercore.h"

/************************************************************************/
/*                     PCIDSKDriverIdentify()                           */
/************************************************************************/

int PCIDSKDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return poOpenInfo->nHeaderBytes >= 512 &&
           memcmp(poOpenInfo->pabyHeader, "PCIDSK  ", 8) == 0;
}

/************************************************************************/
/*                     PCIDSKDriverSetCommonMetadata()                  */
/************************************************************************/

void PCIDSKDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCIDSK Database File");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/pcidsk.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "pix");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 Float32 CInt16 CFloat32");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='INTERLEAVING' type='string-select' default='BAND' "
        "description='raster data organization'>"
        "       <Value>PIXEL</Value>"
        "       <Value>BAND</Value>"
        "       <Value>FILE</Value>"
        "       <Value>TILED</Value>"
        "   </Option>"
        "   <Option name='COMPRESSION' type='string-select' default='NONE' "
        "description='compression - (INTERLEAVING=TILED only)'>"
        "       <Value>NONE</Value>"
        "       <Value>RLE</Value>"
        "       <Value>JPEG</Value>"
        "   </Option>"
        "   <Option name='TILESIZE' type='int' default='127' description='Tile "
        "Size (INTERLEAVING=TILED only)'/>"
        "   <Option name='TILEVERSION' type='int' default='2' "
        "description='Tile Version (INTERLEAVING=TILED only)'/>"
        "</CreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList/>");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Real String IntegerList");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->pfnIdentify = PCIDSKDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS,
                              "GeoTransform SRS "
                              "DatasetMetadata BandMetadata "
                              "RasterValues Features");
}

/************************************************************************/
/*                    DeclareDeferredPCIDSKPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPCIDSKPlugin()
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
    PCIDSKDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
