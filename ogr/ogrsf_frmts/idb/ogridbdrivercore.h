/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDriver class.
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRIDBDRIVERCORE_H
#define OGRIDBDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "IDB";

#define OGRIDBDriverIdentify PLUGIN_SYMBOL_NAME(OGRIDBDriverIdentify)
#define OGRIDBDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGRIDBDriverSetCommonMetadata)

int OGRIDBDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRIDBDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
