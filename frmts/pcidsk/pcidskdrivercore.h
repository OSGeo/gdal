/******************************************************************************
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef PCIDSKDRIVERCORE_H
#define PCIDSKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "PCIDSK";

#define PCIDSKDriverIdentify PLUGIN_SYMBOL_NAME(PCIDSKDriverIdentify)
#define PCIDSKDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(PCIDSKDriverSetCommonMetadata)

int PCIDSKDriverIdentify(GDALOpenInfo *poOpenInfo);

void PCIDSKDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
