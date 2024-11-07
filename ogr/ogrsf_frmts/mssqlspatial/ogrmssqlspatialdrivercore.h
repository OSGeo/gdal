/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMSSQLSPATIALDRIVERCORE_H
#define OGRMSSQLSPATIALDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "MSSQLSpatial";

#define OGRMSSQLSPATIALDriverIdentify                                          \
    PLUGIN_SYMBOL_NAME(OGRMSSQLSPATIALDriverIdentify)
#define OGRMSSQLSPATIALDriverSetCommonMetadata                                 \
    PLUGIN_SYMBOL_NAME(OGRMSSQLSPATIALDriverSetCommonMetadata)

int OGRMSSQLSPATIALDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRMSSQLSPATIALDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
