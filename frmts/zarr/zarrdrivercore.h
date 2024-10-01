/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ZARRDRIVERCORE_H
#define ZARRDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Zarr";

#define ZARRDriverIdentify PLUGIN_SYMBOL_NAME(ZARRDriverIdentify)
#define ZARRDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(ZARRDriverSetCommonMetadata)

int ZARRDriverIdentify(GDALOpenInfo *poOpenInfo);

void ZARRDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
