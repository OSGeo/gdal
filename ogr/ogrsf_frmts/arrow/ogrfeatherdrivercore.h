/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRFEATHERDRIVERCORE_H
#define OGRFEATHERDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Arrow";

#define OGRFeatherDriverIsArrowFileFormat                                      \
    PLUGIN_SYMBOL_NAME(OGRFeatherDriverIsArrowFileFormat)
#define OGRFeatherDriverIdentify PLUGIN_SYMBOL_NAME(OGRFeatherDriverIdentify)
#define OGRFeatherDriverSetCommonMetadata                                      \
    PLUGIN_SYMBOL_NAME(OGRFeatherDriverSetCommonMetadata)

bool OGRFeatherDriverIsArrowFileFormat(GDALOpenInfo *poOpenInfo);

int OGRFeatherDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRFeatherDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
