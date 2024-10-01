/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRODBCDRIVERCORE_H
#define OGRODBCDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "ODBC";

#define OGRODBCDriverIsSupportedMsAccessFileExtension                          \
    PLUGIN_SYMBOL_NAME(OGRODBCDriverIsSupportedMsAccessFileExtension)
#define OGRODBCDriverIdentify PLUGIN_SYMBOL_NAME(OGRODBCDriverIdentify)
#define OGRODBCDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRODBCDriverSetCommonMetadata)

bool OGRODBCDriverIsSupportedMsAccessFileExtension(const char *pszExtension);

int OGRODBCDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRODBCDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
