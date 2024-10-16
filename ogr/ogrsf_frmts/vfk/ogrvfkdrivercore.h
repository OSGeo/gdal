/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDriver class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2018, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRVFKDRIVERCORE_H
#define OGRVFKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "VFK";

#define OGRVFKDriverIdentify PLUGIN_SYMBOL_NAME(OGRVFKDriverIdentify)
#define OGRVFKDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGRVFKDriverSetCommonMetadata)

int OGRVFKDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRVFKDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
