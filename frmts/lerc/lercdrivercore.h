/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  LERC driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef LERCDRIVERCORE_H
#define LERCDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "LERC";

#define LERCDriverIdentify PLUGIN_SYMBOL_NAME(LERCDriverIdentify)
#define LERCDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(LERCDriverSetCommonMetadata)

int LERCDriverIdentify(GDALOpenInfo *poOpenInfo);

void LERCDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
