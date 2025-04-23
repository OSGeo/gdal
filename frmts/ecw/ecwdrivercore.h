/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  ECW driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ECWDRIVERCORE_H
#define ECWDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *ECW_DRIVER_NAME = "ECW";

constexpr const char *JP2ECW_DRIVER_NAME = "JP2ECW";

#define ECWDatasetIdentifyECW PLUGIN_SYMBOL_NAME(ECWDatasetIdentifyECW)
#define ECWDatasetIdentifyJPEG2000                                             \
    PLUGIN_SYMBOL_NAME(ECWDatasetIdentifyJPEG2000)
#define ECWDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(ECWDriverSetCommonMetadata)
#define JP2ECWDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(JP2ECWDriverSetCommonMetadata)

int ECWDatasetIdentifyECW(GDALOpenInfo *poOpenInfo);

int ECWDatasetIdentifyJPEG2000(GDALOpenInfo *poOpenInfo);

void ECWDriverSetCommonMetadata(GDALDriver *poDriver);

void JP2ECWDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
