/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WMSDRIVERCORE_H
#define WMSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "WMS";

#define WMSDriverIdentify PLUGIN_SYMBOL_NAME(WMSDriverIdentify)
#define WMSDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(WMSDriverSetCommonMetadata)

int WMSDriverIdentify(GDALOpenInfo *poOpenInfo);

void WMSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
