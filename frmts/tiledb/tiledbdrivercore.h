/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef TILEDBDRIVERCORE_H
#define TILEDBDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "TileDB";

#define TileDBDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(TileDBDriverSetCommonMetadata)

int TileDBDriverIdentifySimplified(GDALOpenInfo *poOpenInfo);

void TileDBDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
