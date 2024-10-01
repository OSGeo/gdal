/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPARQUETDRIVERCORE_H
#define OGRPARQUETDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Parquet";

#define OGRParquetDriverIdentify PLUGIN_SYMBOL_NAME(OGRParquetDriverIdentify)
#define OGRParquetDriverSetCommonMetadata                                      \
    PLUGIN_SYMBOL_NAME(OGRParquetDriverSetCommonMetadata)

int OGRParquetDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRParquetDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
