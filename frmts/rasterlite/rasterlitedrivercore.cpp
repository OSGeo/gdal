/******************************************************************************
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "rasterlitedrivercore.h"

/************************************************************************/
/*                    RasterliteDriverIdentify()                        */
/************************************************************************/

int RasterliteDriverIdentify(GDALOpenInfo *poOpenInfo)

{
#ifdef ENABLE_SQL_SQLITE_FORMAT
    if (poOpenInfo->pabyHeader &&
        STARTS_WITH((const char *)poOpenInfo->pabyHeader, "-- SQL RASTERLITE"))
    {
        return TRUE;
    }
#endif

    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") &&
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GPKG") &&
        poOpenInfo->nHeaderBytes >= 1024 && poOpenInfo->pabyHeader &&
        STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader,
                       "SQLite Format 3") &&
        // Do not match direct Amazon S3 signed URLs that contains .mbtiles in
        // the middle of the URL
        strstr(poOpenInfo->pszFilename, ".mbtiles") == nullptr)
    {
        // Could be a SQLite/Spatialite file as well
        return -1;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "RASTERLITE:"))
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                  RasterliteDriverSetCommonMetadata()                 */
/************************************************************************/

void RasterliteDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Rasterlite");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/rasterlite.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "sqlite");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CInt16 CInt32 CFloat32 CFloat64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='WIPE' type='boolean' default='NO' description='Erase "
        "all preexisting data in the specified table'/>"
        "   <Option name='TILED' type='boolean' default='YES' description='Use "
        "tiling'/>"
        "   <Option name='BLOCKXSIZE' type='int' default='256' "
        "description='Tile Width'/>"
        "   <Option name='BLOCKYSIZE' type='int' default='256' "
        "description='Tile Height'/>"
        "   <Option name='DRIVER' type='string' description='GDAL driver to "
        "use for storing tiles' default='GTiff'/>"
        "   <Option name='COMPRESS' type='string' description='(GTiff driver) "
        "Compression method' default='NONE'/>"
        "   <Option name='QUALITY' type='int' description='(JPEG-compressed "
        "GTiff, JPEG and WEBP drivers) JPEG/WEBP Quality 1-100' default='75'/>"
        "   <Option name='PHOTOMETRIC' type='string-select' "
        "description='(GTiff driver) Photometric interpretation'>"
        "       <Value>MINISBLACK</Value>"
        "       <Value>MINISWHITE</Value>"
        "       <Value>PALETTE</Value>"
        "       <Value>RGB</Value>"
        "       <Value>CMYK</Value>"
        "       <Value>YCBCR</Value>"
        "       <Value>CIELAB</Value>"
        "       <Value>ICCLAB</Value>"
        "       <Value>ITULAB</Value>"
        "   </Option>"
        "</CreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

#ifdef ENABLE_SQL_SQLITE_FORMAT
    poDriver->SetMetadataItem("ENABLE_SQL_SQLITE_FORMAT", "YES");
#endif

    poDriver->pfnIdentify = RasterliteDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                  DeclareDeferredRasterlitePlugin()                   */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredRasterlitePlugin()
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
    RasterliteDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
