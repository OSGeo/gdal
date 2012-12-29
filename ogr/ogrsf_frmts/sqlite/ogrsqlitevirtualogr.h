/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite Virtual Table module using OGR layers
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
#define _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED

#include "ogr_sqlite.h"
#include <map>
#include <vector>

#ifdef HAVE_SQLITE_VFS

/************************************************************************/
/*                           OGR2SQLITEModule                           */
/************************************************************************/

class OGR2SQLITEModule
{
#ifdef DEBUG
    void* pDummy; /* to track memory leaks */
#endif
    sqlite3* hDB; /* *NOT* to be freed */

    OGRDataSource* poDS; /* *NOT* to be freed */
    std::vector<OGRDataSource*> apoExtraDS; /* each datasource to be freed */

    OGRSQLiteDataSource* poSQLiteDS;  /* *NOT* to be freed, might be NULL */

    std::map< CPLString, OGRLayer* > oMapVTableToOGRLayer;

    void* hHandleSQLFunctions;

  public:
                                 OGR2SQLITEModule(OGRDataSource* poDS);
                                ~OGR2SQLITEModule();

    int                          Setup(OGRSQLiteDataSource* poSQLiteDS);
    int                          Setup(sqlite3* hDB, int bAutoDestroy = FALSE);
    sqlite3*                     GetDBHandle() { return hDB; }

    OGRDataSource*               GetDS() { return poDS; }

    int                          AddExtraDS(OGRDataSource* poDS);
    OGRDataSource               *GetExtraDS(int nIndex);

    int                          FetchSRSId(OGRSpatialReference* poSRS);

    void                         RegisterVTable(const char* pszVTableName, OGRLayer* poLayer);
    void                         UnregisterVTable(const char* pszVTableName);
    OGRLayer*                    GetLayerForVTable(const char* pszVTableName);

    void                         SetHandleSQLFunctions(void* hHandleSQLFunctionsIn) { hHandleSQLFunctions = hHandleSQLFunctionsIn; }
};

void OGR2SQLITE_Register();

CPLString OGR2SQLITE_GetNameForGeometryColumn(OGRLayer* poLayer);

#endif // HAVE_SQLITE_VFS

#endif // _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
