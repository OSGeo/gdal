/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite Virtual Table module using OGR layers
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
#define OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED

#include "ogr_sqlite.h"

class OGR2SQLITEModule;

OGR2SQLITEModule *OGR2SQLITE_Setup(GDALDataset *poDS,
                                   OGRSQLiteDataSource *poSQLiteDS);

void OGR2SQLITE_SetCaseSensitiveLike(OGR2SQLITEModule *poModule, bool b);

int OGR2SQLITE_AddExtraDS(OGR2SQLITEModule *poModule, GDALDataset *poDS);

void OGR2SQLITE_Register();

CPLString OGR2SQLITE_GetNameForGeometryColumn(OGRLayer *poLayer);

#endif  // OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
