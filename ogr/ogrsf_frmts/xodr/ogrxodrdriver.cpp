/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of OGRXODRDriver.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)        
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_xodr.h"
#include "ogrxodrdrivercore.h"
#include "cpl_conv.h"
#include "cpl_error.h"

static GDALDataset *OGRXODRDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr)
        return nullptr;

    auto dataSource = std::make_unique<OGRXODRDataSource>();

    if (!dataSource->Open(poOpenInfo->pszFilename,
                          poOpenInfo->papszOpenOptions))
    {
        return nullptr;
    }

    return dataSource.release();
}

void RegisterOGRXODR()
{
    if (!GDAL_CHECK_VERSION(DRIVER_NAME))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRXODRDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRXODRDriverOpen;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
