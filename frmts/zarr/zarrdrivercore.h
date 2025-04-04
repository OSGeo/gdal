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
int ZARRDriverIdentify(GDALOpenInfo *poOpenInfo);

#define ZARRDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(ZARRDriverSetCommonMetadata)
void ZARRDriverSetCommonMetadata(GDALDriver *poDriver);

#define ZARRIsLikelyKerchunkJSONRef                                            \
    PLUGIN_SYMBOL_NAME(ZARRIsLikelyKerchunkJSONRef)
bool ZARRIsLikelyKerchunkJSONRef(const GDALOpenInfo *poOpenInfo);

#endif
