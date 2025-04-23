/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGRLIBKMLDRIVERCORE_H
#define OGRLIBKMLDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "LIBKML";

#define OGRLIBKMLDriverIdentify PLUGIN_SYMBOL_NAME(OGRLIBKMLDriverIdentify)
#define OGRLIBKMLDriverSetCommonMetadata                                       \
    PLUGIN_SYMBOL_NAME(OGRLIBKMLDriverSetCommonMetadata)

int OGRLIBKMLDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRLIBKMLDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
