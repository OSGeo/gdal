/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  MrSID driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MrSIDDRIVERCORE_H
#define MrSIDDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *MRSID_DRIVER_NAME = "MrSID";

constexpr const char *JP2MRSID_DRIVER_NAME = "JP2MrSID";

#define MrSIDIdentify PLUGIN_SYMBOL_NAME(MrSIDIdentify)
#define MrSIDJP2Identify PLUGIN_SYMBOL_NAME(MrSIDJP2Identify)
#define MrSIDDriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(MrSIDDriverSetCommonMetadata)
#define JP2MrSIDDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(JP2MrSIDDriverSetCommonMetadata)

int MrSIDIdentify(GDALOpenInfo *poOpenInfo);

int MrSIDJP2Identify(GDALOpenInfo *poOpenInfo);

void MrSIDDriverSetCommonMetadata(GDALDriver *poDriver);

void JP2MrSIDDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
