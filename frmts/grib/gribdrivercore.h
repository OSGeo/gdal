/******************************************************************************
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 * Copyright (c) 2008-2017, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GRIBDRIVERCORE_H
#define GRIBDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "GRIB";

#define GRIBDriverIdentify PLUGIN_SYMBOL_NAME(GRIBDriverIdentify)
#define GRIBDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(GRIBDriverSetCommonMetadata)

int GRIBDriverIdentify(GDALOpenInfo *poOpenInfo);

void GRIBDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
