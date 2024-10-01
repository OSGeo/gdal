/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JPIPKAK driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JPIPKAKDRIVERCORE_H
#define JPIPKAKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JPIPKAK";

#define JPIPKAKDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(JPIPKAKDriverSetCommonMetadata)

void JPIPKAKDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
