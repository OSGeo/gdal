/******************************************************************************
 *
 * Project:  GDAL WMTS driver
 * Purpose:  Implement GDAL WMTS support
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Funded by Land Information New Zealand (LINZ)
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WMTSDRIVERCORE_H
#define WMTSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "WMTS";

#define WMTSDriverIdentify PLUGIN_SYMBOL_NAME(WMTSDriverIdentify)
#define WMTSDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(WMTSDriverSetCommonMetadata)

int WMTSDriverIdentify(GDALOpenInfo *poOpenInfo);

void WMTSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
