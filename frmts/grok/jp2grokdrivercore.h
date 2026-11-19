/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JP2Grok driver
 * Author:   Aaron Boxer
 *
 ******************************************************************************
 * Copyright (c) 2026, Grok Image Compression Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JP2GROKDRIVERCORE_H
#define JP2GROKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JP2Grok";

#define JP2GrokDatasetIdentify PLUGIN_SYMBOL_NAME(JP2GrokDatasetIdentify)
#define JP2GrokDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(JP2GrokDriverSetCommonMetadata)

int JP2GrokDatasetIdentify(GDALOpenInfo *poOpenInfo);

void JP2GrokDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
