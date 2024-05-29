/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF4 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#ifndef HDF4DRIVERCORE_H
#define HDF4DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *HDF4_DRIVER_NAME = "HDF4";

constexpr const char *HDF4_IMAGE_DRIVER_NAME = "HDF4Image";

#define HDF4DatasetIdentify PLUGIN_SYMBOL_NAME(HDF4DatasetIdentify)

#define HDF4ImageDatasetIdentify PLUGIN_SYMBOL_NAME(HDF4ImageDatasetIdentify)

#define HDF4DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(HDF4DriverSetCommonMetadata)

#define HDF4ImageDriverSetCommonMetadata                                       \
    PLUGIN_SYMBOL_NAME(HDF4ImageDriverSetCommonMetadata)

int HDF4DatasetIdentify(GDALOpenInfo *poOpenInfo);

int HDF4ImageDatasetIdentify(GDALOpenInfo *poOpenInfo);

void HDF4DriverSetCommonMetadata(GDALDriver *poDriver);

void HDF4ImageDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
