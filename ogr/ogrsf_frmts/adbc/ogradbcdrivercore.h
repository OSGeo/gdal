/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "ADBC";

#define OGRADBCDriverIsDuckDB PLUGIN_SYMBOL_NAME(OGRADBCDriverIsDuckDB)
#define OGRADBCDriverIsSQLite3 PLUGIN_SYMBOL_NAME(OGRADBCDriverIsSQLite3)
#define OGRADBCDriverIsParquet PLUGIN_SYMBOL_NAME(OGRADBCDriverIsParquet)
#define OGRADBCDriverIdentify PLUGIN_SYMBOL_NAME(OGRADBCDriverIdentify)
#define OGRADBCDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRADBCDriverSetCommonMetadata)

bool OGRADBCDriverIsDuckDB(const GDALOpenInfo *poOpenInfo);

bool OGRADBCDriverIsSQLite3(const GDALOpenInfo *poOpenInfo);

bool OGRADBCDriverIsParquet(const GDALOpenInfo *poOpenInfo);

int OGRADBCDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRADBCDriverSetCommonMetadata(GDALDriver *poDriver);
