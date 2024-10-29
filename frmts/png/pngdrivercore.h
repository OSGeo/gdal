/******************************************************************************
 *
 * Project:  PNG Driver
 * Purpose:  Implement GDAL PNG Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef PNGDRIVERCORE_H
#define PNGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PNG";

#define PNGDriverIdentify PLUGIN_SYMBOL_NAME(PNGDriverIdentify)
#define PNGDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(PNGDriverSetCommonMetadata)

int PNGDriverIdentify(GDALOpenInfo *poOpenInfo);

void PNGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
