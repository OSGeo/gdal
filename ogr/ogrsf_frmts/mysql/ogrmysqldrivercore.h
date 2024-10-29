/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMYSQLDRIVERCORE_H
#define OGRMYSQLDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "MySQL";

#define OGRMySQLDriverIdentify PLUGIN_SYMBOL_NAME(OGRMySQLDriverIdentify)
#define OGRMySQLDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(OGRMySQLDriverSetCommonMetadata)

int OGRMySQLDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRMySQLDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
