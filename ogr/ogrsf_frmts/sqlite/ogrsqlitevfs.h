/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Creation of a SQLite3 VFS
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITE_VFS_H_INCLUDED
#define OGR_SQLITE_VFS_H_INCLUDED

#include "cpl_vsi.h"

#include <sqlite3.h>

typedef void (*pfnNotifyFileOpenedType)(void *pfnUserData,
                                        const char *pszFilename, VSILFILE *fp);

sqlite3_vfs *OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void *pfnUserData);

#endif  // OGR_SQLITE_VFS_H_INCLUDED
