/******************************************************************************
 *
 * Project:  MongoDB Translator
 * Purpose:  Implements OGRMongoDBDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014-2019, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMongoDBv3DRIVERCORE_H
#define OGRMongoDBv3DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "MongoDBv3";

#define OGRMongoDBv3DriverIdentify                                             \
    PLUGIN_SYMBOL_NAME(OGRMongoDBv3DriverIdentify)
#define OGRMongoDBv3DriverSetCommonMetadata                                    \
    PLUGIN_SYMBOL_NAME(OGRMongoDBv3DriverSetCommonMetadata)

int OGRMongoDBv3DriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRMongoDBv3DriverSetCommonMetadata(GDALDriver *poDriver);

#endif
