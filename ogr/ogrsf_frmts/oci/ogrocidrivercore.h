/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGROCIDRIVERCORE_H
#define OGROCIDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "OCI";

#define OGROCIDriverIdentify PLUGIN_SYMBOL_NAME(OGROCIDriverIdentify)
#define OGROCIDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGROCIDriverSetCommonMetadata)

int OGROCIDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGROCIDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
