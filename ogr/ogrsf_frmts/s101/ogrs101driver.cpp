/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"
#include "ogrs101drivercore.h"

/************************************************************************/
/*                          RegisterOGRS101()                           */
/************************************************************************/

void RegisterOGRS101()

{
    if (GDALGetDriverByName("S101") != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();

    OGRS101DriverSetCommonMetadata(poDriver.get());

    poDriver->pfnOpen = OGRS101Dataset::Open;
    poDriver->pfnUnloadDriver = OGRS101Dataset::UnloadDriver;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
