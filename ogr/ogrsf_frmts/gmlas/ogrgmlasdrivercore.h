/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRGMLASDRIVERCORE_H
#define OGRGMLASDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "GMLAS";

#define OGRGMLASDriverIdentify PLUGIN_SYMBOL_NAME(OGRGMLASDriverIdentify)
#define OGRGMLASDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(OGRGMLASDriverSetCommonMetadata)

int OGRGMLASDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRGMLASDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
