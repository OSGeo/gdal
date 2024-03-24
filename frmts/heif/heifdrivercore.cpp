/******************************************************************************
 *
 * Project:  HEIF Driver
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

#include "include_libheif.h"

#include "heifdrivercore.h"

/************************************************************************/
/*                    HEIFDriverIdentifySimplified()                    */
/************************************************************************/

constexpr const char *FTYP_BOX_SIGNATURE = "ftyp";
constexpr const char *MAJOR_BRANDS[] = {"heic", "heix", "avif", "jpeg", "j2ki"};
constexpr const char *MAJOR_BRANDS_MAYBE[] = {"mif1", "mif2"};

int HEIFDriverIdentifySimplified(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "HEIF:"))
        return true;

    if (poOpenInfo->nHeaderBytes < 12 || poOpenInfo->fpL == nullptr)
        return false;

    if (memcmp(poOpenInfo->pabyHeader + 4, FTYP_BOX_SIGNATURE, 4) != 0)
    {
        return GDAL_IDENTIFY_FALSE;
    }
    for (const char *brand : MAJOR_BRANDS)
    {
        if (memcmp(poOpenInfo->pabyHeader + 8, brand, 4) == 0)
        {
            return GDAL_IDENTIFY_TRUE;
        }
    }
    for (const char *brand : MAJOR_BRANDS_MAYBE)
    {
        if (memcmp(poOpenInfo->pabyHeader + 8, brand, 4) == 0)
        {
            return GDAL_IDENTIFY_UNKNOWN;
        }
    }
    return GDAL_IDENTIFY_FALSE;
}

/************************************************************************/
/*                     HEIFDriverSetCommonMetadata()                    */
/************************************************************************/

void HEIFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME,
        "ISO/IEC 23008-12 High Efficiency Image File Format");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/heic");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/heif.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "heic");
#ifdef HAS_CUSTOM_FILE_READER
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
#endif

    poDriver->SetMetadataItem("LIBHEIF_VERSION", LIBHEIF_VERSION);

    poDriver->pfnIdentify = HEIFDriverIdentifySimplified;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
#ifdef HAS_CUSTOM_FILE_WRITER
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
#endif
}

/************************************************************************/
/*                     DeclareDeferredHEIFPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredHEIFPlugin()
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
    HEIFDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
