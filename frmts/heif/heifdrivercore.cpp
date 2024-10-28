/******************************************************************************
 *
 * Project:  HEIF Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "include_libheif.h"

#include "heifdrivercore.h"

/************************************************************************/
/*                    HEIFDriverIdentifySimplified()                    */
/************************************************************************/

static const GByte FTYP_4CC[] = {'f', 't', 'y', 'p'};
static const GByte supportedBrands[][4]{
    {'a', 'v', 'i', 'f'}, {'h', 'e', 'i', 'c'}, {'h', 'e', 'i', 'x'},
    {'j', '2', 'k', 'i'}, {'j', 'p', 'e', 'g'}, {'m', 'i', 'a', 'f'},
    {'m', 'i', 'f', '1'}, {'m', 'i', 'f', '2'}, {'v', 'v', 'i', 'c'}};

int HEIFDriverIdentifySimplified(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "HEIF:"))
    {
        return true;
    }
    if (poOpenInfo->nHeaderBytes < 16 || poOpenInfo->fpL == nullptr)
    {
        return false;
    }
    if (memcmp(poOpenInfo->pabyHeader + 4, FTYP_4CC, sizeof(FTYP_4CC)) != 0)
    {
        return false;
    }
    uint32_t lengthBigEndian;
    memcpy(&lengthBigEndian, poOpenInfo->pabyHeader, sizeof(uint32_t));
    uint32_t lengthHostEndian = CPL_MSBWORD32(lengthBigEndian);
    if (lengthHostEndian > static_cast<uint32_t>(poOpenInfo->nHeaderBytes))
    {
        lengthHostEndian = static_cast<uint32_t>(poOpenInfo->nHeaderBytes);
    }
    for (const GByte *supportedBrand : supportedBrands)
    {
        if (memcmp(poOpenInfo->pabyHeader + 8, supportedBrand, 4) == 0)
        {
            return true;
        }
    }
    for (uint32_t offset = 16; offset + 4 <= lengthHostEndian; offset += 4)
    {
        for (const GByte *supportedBrand : supportedBrands)
        {
            if (memcmp(poOpenInfo->pabyHeader + offset, supportedBrand, 4) == 0)
            {
                return true;
            }
        }
    }
    return false;
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
        "ISO/IEC 23008-12:2017 High Efficiency Image File Format");
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
