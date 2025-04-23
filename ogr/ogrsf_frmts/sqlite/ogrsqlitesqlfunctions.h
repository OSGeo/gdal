/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Extension SQL functions
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITE_SQL_FUNCTIONS_INCLUDED
#define OGR_SQLITE_SQL_FUNCTIONS_INCLUDED

#include "ogr_sqlite.h"

static void *OGRSQLiteRegisterSQLFunctions(sqlite3 *hDB);
static void OGRSQLiteUnregisterSQLFunctions(void *hHandle);

#endif  // OGR_SQLITE_SQL_FUNCTIONS_INCLUDED
