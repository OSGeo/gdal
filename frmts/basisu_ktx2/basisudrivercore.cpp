/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "basisudrivercore.h"
#include "commoncore.h"

/************************************************************************/
/*                     BASISUDriverIdentify()                           */
/************************************************************************/

int BASISUDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "BASISU:"))
        return true;
    // See
    // https://github.com/BinomialLLC/basis_universal/blob/master/spec/basis_spec.txt
    constexpr int HEADER_SIZE = 77;
    if (!(poOpenInfo->fpL != nullptr &&
          poOpenInfo->nHeaderBytes >= HEADER_SIZE &&
          poOpenInfo->pabyHeader[0] == 0x73 &&         // Signature
          poOpenInfo->pabyHeader[1] == 0x42 &&         // Signature
          poOpenInfo->pabyHeader[4] == HEADER_SIZE &&  // Header size
          poOpenInfo->pabyHeader[5] == 0))             // Header size
    {
        return false;
    }
    const uint32_t nDataSize = CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + 8);
    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
    const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
    return nDataSize + HEADER_SIZE == nFileSize;
}

/************************************************************************/
/*                      BASISUDriverSetCommonMetadata()                 */
/************************************************************************/

void BASISUDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(BASISU_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Basis Universal texture format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/basisu.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "basis");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        GDAL_KTX2_BASISU_GetCreationOptions(/* bIsKTX2 = */ false).c_str());

    poDriver->pfnIdentify = BASISUDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredBASISUPlugin()                    */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredBASISUPlugin()
{
    if (GDALGetDriverByName(BASISU_DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    BASISUDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}

void DeclareDeferredBASISU_KTX2Plugin()
{
    DeclareDeferredBASISUPlugin();
    DeclareDeferredKTX2Plugin();
}

#endif
