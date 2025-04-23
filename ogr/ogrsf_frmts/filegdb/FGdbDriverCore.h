/******************************************************************************
 *
 * Project:  FileGDB Translator
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRFILEGDBDRIVERCORE_H
#define OGRFILEGDBDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "FileGDB";

#define OGRFileGDBDriverIdentifyInternal                                       \
    PLUGIN_SYMBOL_NAME(OGRFileGDBDriverIdentifyInternal)
#define OGRFileGDBDriverSetCommonMetadata                                      \
    PLUGIN_SYMBOL_NAME(OGRFileGDBDriverSetCommonMetadata)

GDALIdentifyEnum OGRFileGDBDriverIdentifyInternal(GDALOpenInfo *poOpenInfo,
                                                  const char *&pszFilename);

void OGRFileGDBDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
