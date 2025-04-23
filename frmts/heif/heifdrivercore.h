/******************************************************************************
 *
 * Project:  HEIF Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HEIFDRIVERCORE_H
#define HEIFDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "HEIF";

#define HEIFDriverIdentifySimplified                                           \
    PLUGIN_SYMBOL_NAME(HEIFDriverIdentifySimplified)
#define HEIFDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(HEIFDriverSetCommonMetadata)

int HEIFDriverIdentifySimplified(GDALOpenInfo *poOpenInfo);

void HEIFDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
