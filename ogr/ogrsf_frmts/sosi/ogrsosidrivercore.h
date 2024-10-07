/******************************************************************************
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRSOSIDRIVERCORE_H
#define OGRSOSIDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "SOSI";

#define OGRSOSIDriverIdentify PLUGIN_SYMBOL_NAME(OGRSOSIDriverIdentify)
#define OGRSOSIDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRSOSIDriverSetCommonMetadata)

int OGRSOSIDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRSOSIDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
