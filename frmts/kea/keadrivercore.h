/*
 *  keadrivercore.h
 *
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEADRIVERCORE_H
#define KEADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "KEA";

#define KEADriverIdentify PLUGIN_SYMBOL_NAME(KEADriverIdentify)
#define KEADriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(KEADriverSetCommonMetadata)

int KEADriverIdentify(GDALOpenInfo *poOpenInfo);

void KEADriverSetCommonMetadata(GDALDriver *poDriver);

#endif
