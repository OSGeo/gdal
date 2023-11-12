/******************************************************************************
 *
 * Project:  EXR read/write Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "exrdrivercore.h"

/************************************************************************/
/*                     EXRDriverIdentify()                              */
/************************************************************************/

int EXRDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "EXR:"))
        return true;

    // Check magic number
    return poOpenInfo->fpL != nullptr && poOpenInfo->nHeaderBytes >= 4 &&
           poOpenInfo->pabyHeader[0] == 0x76 &&
           poOpenInfo->pabyHeader[1] == 0x2f &&
           poOpenInfo->pabyHeader[2] == 0x31 &&
           poOpenInfo->pabyHeader[3] == 0x01;
}

/************************************************************************/
/*                      EXRDriverSetCommonMetadata()                    */
/************************************************************************/

void EXRDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Extended Dynamic Range Image File Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/exr.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "exr");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='COMPRESS' type='string-select' default='ZIP'>"
        "     <Value>NONE</Value>"
        "     <Value>RLE</Value>"
        "     <Value>ZIPS</Value>"
        "     <Value>ZIP</Value>"
        "     <Value>PIZ</Value>"
        "     <Value>PXR24</Value>"
        "     <Value>B44</Value>"
        "     <Value>B44A</Value>"
        "     <Value>DWAA</Value>"
        "     <Value>DWAB</Value>"
        "   </Option>"
        "   <Option name='PIXEL_TYPE' type='string-select'>"
        "     <Value>HALF</Value>"
        "     <Value>FLOAT</Value>"
        "     <Value>UINT</Value>"
        "   </Option>"
        "   <Option name='TILED' type='boolean' description='Use tiling' "
        "default='YES'/>"
        "   <Option name='BLOCKXSIZE' type='int' description='Tile width' "
        "default='256'/>"
        "   <Option name='BLOCKYSIZE' type='int' description='Tile height' "
        "default='256'/>"
        "   <Option name='OVERVIEWS' type='boolean' description='Whether to "
        "create overviews' default='NO'/>"
        "   <Option name='OVERVIEW_RESAMPLING' type='string' "
        "description='Resampling method' default='CUBIC'/>"
        "   <Option name='PREVIEW' type='boolean' description='Create a "
        "preview' default='NO'/>"
        "   <Option name='AUTO_RESCALE' type='boolean' description='Whether to "
        "rescale Byte RGB(A) values to 0-1' default='YES'/>"
        "   <Option name='DWA_COMPRESSION_LEVEL' type='int' description='DWA "
        "compression level'/>"
        "</CreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = EXRDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredEXRPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredEXRPlugin()
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
    EXRDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
