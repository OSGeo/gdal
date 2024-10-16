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

#ifndef KTX2DRIVERCORE_H
#define KTX2DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *KTX2_DRIVER_NAME = "KTX2";

#define KTX2DriverIdentify PLUGIN_SYMBOL_NAME(KTX2DriverIdentify)
#define KTX2DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(KTX2DriverSetCommonMetadata)

int KTX2DriverIdentify(GDALOpenInfo *poOpenInfo);

void KTX2DriverSetCommonMetadata(GDALDriver *poDriver);

#endif
