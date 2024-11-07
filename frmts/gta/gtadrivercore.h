/******************************************************************************
 *
 * Project:  GTA read/write Driver
 * Purpose:  GDAL bindings over GTA library.
 * Author:   Martin Lambers, marlam@marlam.de
 *
 ******************************************************************************
 * Copyright (c) 2010, 2011, Martin Lambers <marlam@marlam.de>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GTADRIVERCORE_H
#define GTADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "GTA";

#define GTADriverIdentify PLUGIN_SYMBOL_NAME(GTADriverIdentify)

#define GTADriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(GTADriverSetCommonMetadata)

int GTADriverIdentify(GDALOpenInfo *poOpenInfo);

void GTADriverSetCommonMetadata(GDALDriver *poDriver);

#endif
