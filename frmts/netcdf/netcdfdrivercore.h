/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  netCDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef NETCDFDRIVERCORE_H
#define NETCDFDRIVERCORE_H

#include "gdal_priv.h"

#include "netcdfformatenum.h"

constexpr const char *DRIVER_NAME = "netCDF";

#define netCDFIdentifyFormat PLUGIN_SYMBOL_NAME(netCDFIdentifyFormat)
#define netCDFDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(netCDFDriverSetCommonMetadata)

NetCDFFormatEnum netCDFIdentifyFormat(GDALOpenInfo *poOpenInfo, bool bCheckExt);

void netCDFDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
