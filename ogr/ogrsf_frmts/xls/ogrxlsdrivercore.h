/******************************************************************************
 *
 * Project:  XLS Translator
 * Purpose:  Implements OGRXLSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRXLSDRIVERCORE_H
#define OGRXLSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "XLS";

#define OGRXLSDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGRXLSDriverSetCommonMetadata)

void OGRXLSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
