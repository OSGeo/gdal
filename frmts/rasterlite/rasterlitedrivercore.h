/******************************************************************************
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RASTERLITEDRIVERCORE_H
#define RASTERLITEDRIVERCORE_H

#include "gdal_priv.h"

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)
// Enable accepting a SQL dump (starting with a "-- SQL SQLITE" or
// "-- SQL RASTERLITE" line) as a valid
// file. This makes fuzzer life easier
#define ENABLE_SQL_SQLITE_FORMAT
#endif

constexpr const char *DRIVER_NAME = "Rasterlite";

#define RasterliteDriverIdentify PLUGIN_SYMBOL_NAME(RasterliteDriverIdentify)
#define RasterliteDriverSetCommonMetadata                                      \
    PLUGIN_SYMBOL_NAME(RasterliteDriverSetCommonMetadata)

int RasterliteDriverIdentify(GDALOpenInfo *poOpenInfo);

void RasterliteDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
