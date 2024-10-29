/******************************************************************************
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena <ivan dot lucena at oracle dot com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef GEORASTERDRIVERCORE_H
#define GEORASTERDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "GeoRaster";

#define GEORDriverIdentify PLUGIN_SYMBOL_NAME(GEORDriverIdentify)
#define GEORDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(GEORDriverSetCommonMetadata)

int GEORDriverIdentify(GDALOpenInfo *poOpenInfo);

void GEORDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
