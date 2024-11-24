/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite REGEXP function
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITE_REGEXP_INCLUDED
#define OGR_SQLITE_REGEXP_INCLUDED

#include "sqlite3.h"

static void *OGRSQLiteRegisterRegExpFunction(sqlite3 *hDB);
static void OGRSQLiteFreeRegExpCache(void *hRegExpCache);

#endif  // OGR_SQLITE_REGEXP_INCLUDED
