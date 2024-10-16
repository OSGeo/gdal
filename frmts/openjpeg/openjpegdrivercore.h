/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  OPENJPEG driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OPENJPEGDRIVERCORE_H
#define OPENJPEGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JP2OpenJPEG";

#define OPENJPEGDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(OPENJPEGDriverSetCommonMetadata)

void OPENJPEGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
