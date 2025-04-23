/******************************************************************************
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Alan Boudreault, aboudreault@mapgears.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Alan Boudreault
 * Copyright (c) 2013,2019, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef DDSDRIVERCORE_H
#define DDSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "DDS";

#define DDS_SIGNATURE "DDS "

#define DDSDriverIdentify PLUGIN_SYMBOL_NAME(DDSDriverIdentify)
#define DDSDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(DDSDriverSetCommonMetadata)

int DDSDriverIdentify(GDALOpenInfo *poOpenInfo);

void DDSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
