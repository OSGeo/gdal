/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF4 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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
