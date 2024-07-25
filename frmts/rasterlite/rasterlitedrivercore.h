/******************************************************************************
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
