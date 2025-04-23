/******************************************************************************
 * File :    PostGISRasterDriver.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Implements PostGIS Raster driver class methods
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *
 *
 ******************************************************************************
 * Copyright (c) 2010, Jorge Arevalo, jorge.arevalo@deimos-space.com
 * Copyright (c) 2013, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef POSTGISRASTERDRIVERCORE_H
#define POSTGISRASTERDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PostGISRaster";

#define PostGISRasterParseConnectionString                                     \
    PLUGIN_SYMBOL_NAME(PostGISRasterParseConnectionString)
#define PostGISRasterDriverIdentify                                            \
    PLUGIN_SYMBOL_NAME(PostGISRasterDriverIdentify)
#define PostGISRasterDriverSetCommonMetadata                                   \
    PLUGIN_SYMBOL_NAME(PostGISRasterDriverSetCommonMetadata)

char **PostGISRasterParseConnectionString(const char *);

int PostGISRasterDriverIdentify(GDALOpenInfo *poOpenInfo);

void PostGISRasterDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
