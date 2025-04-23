/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support functions.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcrasterdrivercore.h"

/* value for first 27 bytes of MAIN_HEADER.signature */
#ifndef CSF_SIG
#define CSF_SIG "RUU CROSS SYSTEM MAP FORMAT"
#define CSF_SIZE_SIG (sizeof(CSF_SIG) - 1)
#endif

/************************************************************************/
/*                     PCRasterDriverIdentify()                         */
/************************************************************************/

int PCRasterDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return (poOpenInfo->fpL &&
            poOpenInfo->nHeaderBytes >= static_cast<int>(CSF_SIZE_SIG) &&
            strncmp(reinterpret_cast<char *>(poOpenInfo->pabyHeader), CSF_SIG,
                    CSF_SIZE_SIG) == 0);
}

#undef CSF_SIG
#undef CSF_SIZE_SIG

/************************************************************************/
/*                    PCRasterDriverSetCommonMetadata()                 */
/************************************************************************/

void PCRasterDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCRaster Raster File");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int32 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/pcraster.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "map");

    poDriver->pfnIdentify = PCRasterDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                  DeclareDeferredPCRasterPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPCRasterPlugin()
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
    PCRasterDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
