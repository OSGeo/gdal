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

#ifndef BASISUDRIVERCORE_H
#define BASISUDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *BASISU_DRIVER_NAME = "BASISU";

#define BASISUDriverIdentify PLUGIN_SYMBOL_NAME(BASISUDriverIdentify)
#define BASISUDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(BASISUDriverSetCommonMetadata)

int BASISUDriverIdentify(GDALOpenInfo *poOpenInfo);

void BASISUDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
