/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  PlanetLabs scene driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015-2016, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPLSCENESDRIVERCORE_H
#define OGRPLSCENESDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PLSCENES";

#define OGRPLSCENESDriverIdentify PLUGIN_SYMBOL_NAME(OGRPLSCENESDriverIdentify)
#define OGRPLSCENESDriverSetCommonMetadata                                     \
    PLUGIN_SYMBOL_NAME(OGRPLSCENESDriverSetCommonMetadata)

int OGRPLSCENESDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRPLSCENESDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
