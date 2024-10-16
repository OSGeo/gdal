/******************************************************************************
 * Project:  GDAL
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:
 * JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2014-2016, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JP2LURADRIVERCORE_H
#define JP2LURADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JP2Lura";

constexpr unsigned char jpc_header[] = {0xff, 0x4f, 0xff,
                                        0x51};  // SOC + RSIZ markers
constexpr unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */

#define JP2LuraDriverIdentify PLUGIN_SYMBOL_NAME(JP2LuraDriverIdentify)
#define JP2LuraDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(JP2LuraDriverSetCommonMetadata)

int JP2LuraDriverIdentify(GDALOpenInfo *poOpenInfo);

void JP2LuraDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
