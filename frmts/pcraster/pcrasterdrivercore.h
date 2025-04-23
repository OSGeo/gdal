/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support functions.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef PCRASTERDRIVERCORE_H
#define PCRASTERDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PCRaster";

#define PCRasterDriverIdentify PLUGIN_SYMBOL_NAME(PCRasterDriverIdentify)
#define PCRasterDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(PCRasterDriverSetCommonMetadata)

int PCRasterDriverIdentify(GDALOpenInfo *poOpenInfo);

void PCRasterDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
