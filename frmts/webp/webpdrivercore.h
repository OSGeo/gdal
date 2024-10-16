/******************************************************************************
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WEBPDRIVERCORE_H
#define WEBPDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "WEBP";

#define WEBPDriverIdentify PLUGIN_SYMBOL_NAME(WEBPDriverIdentify)
#define WEBPDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(WEBPDriverSetCommonMetadata)

int WEBPDriverIdentify(GDALOpenInfo *poOpenInfo);

void WEBPDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
