/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WCSDRIVERCORE_H
#define WCSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "WCS";

#define WCSDriverIdentify PLUGIN_SYMBOL_NAME(WCSDriverIdentify)
#define WCSDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(WCSDriverSetCommonMetadata)

int WCSDriverIdentify(GDALOpenInfo *poOpenInfo);

void WCSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
