/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTODriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRCARTODRIVERCORE_H
#define OGRCARTODRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Carto";

#define OGRCartoDriverIdentify PLUGIN_SYMBOL_NAME(OGRCartoDriverIdentify)
#define OGRCartoDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(OGRCartoDriverSetCommonMetadata)

int OGRCartoDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRCartoDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
