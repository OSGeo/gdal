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

#ifndef OGRS101DRIVERCORE_H
#define OGRS101DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "S101";

#define OGRS101DriverIdentify PLUGIN_SYMBOL_NAME(OGRS101DriverIdentify)
#define OGRS101DriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRS101DriverSetCommonMetadata)

int OGRS101DriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRS101DriverSetCommonMetadata(GDALDriver *poDriver);

#endif
