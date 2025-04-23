/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPGDRIVERCORE_H
#define OGRPGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PostgreSQL";

#define OGRPGDriverIdentify PLUGIN_SYMBOL_NAME(OGRPGDriverIdentify)
#define OGRPGDriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(OGRPGDriverSetCommonMetadata)

int OGRPGDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRPGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
