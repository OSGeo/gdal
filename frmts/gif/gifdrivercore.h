/******************************************************************************
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GIFDRIVERCORE_H
#define GIFDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *GIF_DRIVER_NAME = "GIF";

constexpr const char *BIGGIF_DRIVER_NAME = "BIGGIF";

#define GIFDriverIdentify PLUGIN_SYMBOL_NAME(GIFDriverIdentify)
#define BIGGIFDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(BIGGIFDriverSetCommonMetadata)
#define GIFDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(GIFDriverSetCommonMetadata)

int GIFDriverIdentify(GDALOpenInfo *poOpenInfo);

void BIGGIFDriverSetCommonMetadata(GDALDriver *poDriver);

void GIFDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
