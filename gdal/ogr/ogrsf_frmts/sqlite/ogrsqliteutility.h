/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Utility header for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#ifndef OGR_SQLITEUTILITY_H_INCLUDED
#define OGR_SQLITEUTILITY_H_INCLUDED

#include "ogr_core.h"
#include "cpl_string.h"
#include "sqlite3.h"

typedef struct
{
    char** papszResult;
    int nRowCount;
    int nColCount;
    char *pszErrMsg;
    int rc;
} SQLResult;


OGRErr              SQLCommand(sqlite3 *poDb, const char * pszSQL);
int                 SQLGetInteger(sqlite3 * poDb, const char * pszSQL, OGRErr *err);
GIntBig             SQLGetInteger64(sqlite3 * poDb, const char * pszSQL, OGRErr *err);

OGRErr              SQLResultInit(SQLResult * poResult);
OGRErr              SQLQuery(sqlite3 *poDb, const char * pszSQL, SQLResult * poResult);
const char*         SQLResultGetValue(const SQLResult * poResult, int iColumnNum, int iRowNum);
int                 SQLResultGetValueAsInteger(const SQLResult * poResult, int iColNum, int iRowNum);
OGRErr              SQLResultFree(SQLResult * poResult);

int                 SQLiteFieldFromOGR(OGRFieldType eType);

/* To escape literals. The returned string doesn't contain the surrounding single quotes */
CPLString           SQLEscapeLiteral( const char *pszLiteral );

/* To escape table or field names. The returned string doesn't contain the surrounding double quotes */
CPLString           SQLEscapeName( const char* pszName );

/* Remove leading ' or " and unescape in that case. Or return string unmodified */
CPLString           SQLUnescape(const char* pszVal);

char**              SQLTokenize( const char* pszSQL );

#endif // OGR_SQLITEUTILITY_H_INCLUDED
