/******************************************************************************
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_pds.h"

extern "C" void RegisterOGRPDS();

using namespace OGRPDS;

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRPDSDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (strstr((const char *)poOpenInfo->pabyHeader, "PDS_VERSION_ID") ==
        nullptr)
        return nullptr;

    OGRPDSDataSource *poDS = new OGRPDSDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRPDS()                           */
/************************************************************************/

void RegisterOGRPDS()

{
    if (GDALGetDriverByName("OGR_PDS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OGR_PDS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Planetary Data Systems TABLE");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/pds.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRPDSDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
