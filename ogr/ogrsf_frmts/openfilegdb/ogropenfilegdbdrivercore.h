/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGROPENFILEGDBDRIVERCORE_H
#define OGROPENFILEGDBDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "OpenFileGDB";

#define OGROpenFileGDBDriverIdentify                                           \
    PLUGIN_SYMBOL_NAME(OGROpenFileGDBDriverIdentify)
#define OGROpenFileGDBDriverSetCommonMetadata                                  \
    PLUGIN_SYMBOL_NAME(OGROpenFileGDBDriverSetCommonMetadata)

GDALIdentifyEnum OGROpenFileGDBDriverIdentify(GDALOpenInfo *poOpenInfo,
                                              const char *&pszFilename);

void OGROpenFileGDBDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
