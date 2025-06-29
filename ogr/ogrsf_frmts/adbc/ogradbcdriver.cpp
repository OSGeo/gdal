/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_adbc.h"
#include "ogradbcdrivercore.h"

/************************************************************************/
/*                         OGRADBCDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRADBCDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRADBCDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update)
        return nullptr;
    auto poDS = std::make_unique<OGRADBCDataset>();
    if (!poDS->Open(poOpenInfo))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                          RegisterOGRADBC()                           */
/************************************************************************/

void RegisterOGRADBC()
{
    if (!GDAL_CHECK_VERSION(DRIVER_NAME))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRADBCDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRADBCDriverOpen;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
