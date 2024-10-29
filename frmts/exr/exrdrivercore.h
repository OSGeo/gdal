/******************************************************************************
 *
 * Project:  EXR read/write Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef EXRDRIVERCORE_H
#define EXRDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "EXR";

#define EXRDriverIdentify PLUGIN_SYMBOL_NAME(EXRDriverIdentify)
#define EXRDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(EXRDriverSetCommonMetadata)

int EXRDriverIdentify(GDALOpenInfo *poOpenInfo);

void EXRDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
