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

#include <set>
#include <string>
#include <memory>

class SQLResult
{
    public:
        SQLResult(char** result, int nRow, int nCol);
        ~SQLResult ();

        int         RowCount() const { return nRowCount; }
        int         ColCount() const { return nColCount; }
        void        LimitRowCount(int nLimit);

        const char* GetValue(int iColumnNum, int iRowNum) const;
        int         GetValueAsInteger(int iColNum, int iRowNum) const;
    private:
        char** papszResult = nullptr;
        int nRowCount = 0;
        int nColCount = 0;
};


OGRErr              SQLCommand(sqlite3 *poDb, const char * pszSQL);
int                 SQLGetInteger(sqlite3 * poDb, const char * pszSQL, OGRErr *err);
GIntBig             SQLGetInteger64(sqlite3 * poDb, const char * pszSQL, OGRErr *err);

std::unique_ptr<SQLResult> SQLQuery(sqlite3 *poDb, const char * pszSQL);

int                 SQLiteFieldFromOGR(OGRFieldType eType);

/* To escape literals. The returned string doesn't contain the surrounding single quotes */
CPLString           SQLEscapeLiteral( const char *pszLiteral );

/* To escape table or field names. The returned string doesn't contain the surrounding double quotes */
CPLString           SQLEscapeName( const char* pszName );

/* Remove leading ' or " and unescape in that case. Or return string unmodified */
CPLString           SQLUnescape(const char* pszVal);

char**              SQLTokenize( const char* pszSQL );

std::set<std::string> SQLGetUniqueFieldUCConstraints(sqlite3* poDb,
                                                     const char* pszTableName);
#endif // OGR_SQLITEUTILITY_H_INCLUDED
