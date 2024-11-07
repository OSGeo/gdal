/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JPEGXL driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JPEGXLDRIVERCORE_H
#define JPEGXLDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JPEGXL";

#define IsJPEGXLContainer PLUGIN_SYMBOL_NAME(IsJPEGXLContainer)
#define JPEGXLDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(JPEGXLDriverSetCommonMetadata)

bool IsJPEGXLContainer(GDALOpenInfo *poOpenInfo);

void JPEGXLDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
