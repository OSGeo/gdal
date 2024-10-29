/******************************************************************************
 *
 * Project:  AVIF Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef AVIFDRIVERCORE_H
#define AVIFDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "AVIF";

#define AVIFDriverIdentify PLUGIN_SYMBOL_NAME(AVIFDriverIdentify)
#define AVIFDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(AVIFDriverSetCommonMetadata)

int AVIFDriverIdentify(GDALOpenInfo *poOpenInfo);

void AVIFDriverSetCommonMetadata(GDALDriver *poDriver,
                                 bool bMayHaveWriteSupport);

#endif
