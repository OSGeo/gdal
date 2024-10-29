/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRDGNV8DRIVERCORE_H
#define OGRDGNV8DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "DGNV8";

#define OGRDGNV8DriverIdentify PLUGIN_SYMBOL_NAME(OGRDGNV8DriverIdentify)
#define OGRDGNV8DriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(OGRDGNV8DriverSetCommonMetadata)

int OGRDGNV8DriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRDGNV8DriverSetCommonMetadata(GDALDriver *poDriver);

#endif
