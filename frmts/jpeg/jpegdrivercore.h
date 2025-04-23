/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JPEGDRIVERCORE_H
#define JPEGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JPEG";

#define JPEGDatasetIsJPEGLS PLUGIN_SYMBOL_NAME(JPEGDatasetIsJPEGLS)
#define JPEGDriverIdentify PLUGIN_SYMBOL_NAME(JPEGDriverIdentify)
#define JPEGDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(JPEGDriverSetCommonMetadata)

bool JPEGDatasetIsJPEGLS(GDALOpenInfo *poOpenInfo);

int JPEGDriverIdentify(GDALOpenInfo *poOpenInfo);

void JPEGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
