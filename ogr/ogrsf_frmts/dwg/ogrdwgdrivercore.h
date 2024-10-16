/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRDWGDRIVERCORE_H
#define OGRDWGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DWG_DRIVER_NAME = "DWG";

#define OGRDWGDriverIdentify PLUGIN_SYMBOL_NAME(OGRDWGDriverIdentify)
#define OGRDWGDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGRDWGDriverSetCommonMetadata)

int OGRDWGDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRDWGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
