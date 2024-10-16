/******************************************************************************
 *
 * Project:  OGDI Bridge
 * Purpose:  Implements OGROGDIDriver class.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000, Daniel Morissette
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGROGDIDRIVERCORE_H
#define OGROGDIDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "OGR_OGDI";

#define OGROGDIDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGROGDIDriverSetCommonMetadata)

void OGROGDIDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
