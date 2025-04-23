/******************************************************************************
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
 * Copyright (c) 2008-2020, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Chiara Marmo <chiara dot marmo at u-psud dot fr>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef FITSDRIVERCORE_H
#define FITSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "FITS";

#define FITSDriverIdentify PLUGIN_SYMBOL_NAME(FITSDriverIdentify)
#define FITSDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(FITSDriverSetCommonMetadata)

int FITSDriverIdentify(GDALOpenInfo *poOpenInfo);

void FITSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
