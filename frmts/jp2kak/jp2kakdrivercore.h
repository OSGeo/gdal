/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JP2KAK driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JP2KAKDRIVERCORE_H
#define JP2KAKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JP2KAK";

constexpr unsigned char jp2_header[] = {0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50,
                                        0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

constexpr unsigned char jpc_header[] = {0xff, 0x4f};

#define JP2KAKDatasetIdentify PLUGIN_SYMBOL_NAME(JP2KAKDatasetIdentify)
#define JP2KAKDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(JP2KAKDriverSetCommonMetadata)

int JP2KAKDatasetIdentify(GDALOpenInfo *poOpenInfo);

void JP2KAKDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
