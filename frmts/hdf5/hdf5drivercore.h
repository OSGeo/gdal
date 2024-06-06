/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF5 driver
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

#ifndef HDF5DRIVERCORE_H
#define HDF5DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *HDF5_DRIVER_NAME = "HDF5";

constexpr const char *HDF5_IMAGE_DRIVER_NAME = "HDF5Image";

constexpr const char *BAG_DRIVER_NAME = "BAG";

constexpr const char *S102_DRIVER_NAME = "S102";

constexpr const char *S104_DRIVER_NAME = "S104";

constexpr const char *S111_DRIVER_NAME = "S111";

#define HDF5DatasetIdentify PLUGIN_SYMBOL_NAME(HDF5DatasetIdentify)
#define HDF5ImageDatasetIdentify PLUGIN_SYMBOL_NAME(HDF5ImageDatasetIdentify)
#define BAGDatasetIdentify PLUGIN_SYMBOL_NAME(BAGDatasetIdentify)
#define S102DatasetIdentify PLUGIN_SYMBOL_NAME(S102DatasetIdentify)
#define S104DatasetIdentify PLUGIN_SYMBOL_NAME(S104DatasetIdentify)
#define S111DatasetIdentify PLUGIN_SYMBOL_NAME(S111DatasetIdentify)
#define HDF5DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(HDF5DriverSetCommonMetadata)
#define HDF5ImageDriverSetCommonMetadata                                       \
    PLUGIN_SYMBOL_NAME(HDF5ImageDriverSetCommonMetadata)
#define BAGDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(BAGDriverSetCommonMetadata)
#define S102DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(S102DriverSetCommonMetadata)
#define S104DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(S104DriverSetCommonMetadata)
#define S111DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(S111DriverSetCommonMetadata)

int HDF5DatasetIdentify(GDALOpenInfo *poOpenInfo);

int HDF5ImageDatasetIdentify(GDALOpenInfo *poOpenInfo);

int BAGDatasetIdentify(GDALOpenInfo *poOpenInfo);

int S102DatasetIdentify(GDALOpenInfo *poOpenInfo);

int S104DatasetIdentify(GDALOpenInfo *poOpenInfo);

int S111DatasetIdentify(GDALOpenInfo *poOpenInfo);

void HDF5DriverSetCommonMetadata(GDALDriver *poDriver);

void HDF5ImageDriverSetCommonMetadata(GDALDriver *poDriver);

void BAGDriverSetCommonMetadata(GDALDriver *poDriver);

void S102DriverSetCommonMetadata(GDALDriver *poDriver);

void S104DriverSetCommonMetadata(GDALDriver *poDriver);

void S111DriverSetCommonMetadata(GDALDriver *poDriver);

#endif
