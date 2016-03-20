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

#include "ogrsf_frmts.h"
#include "sqlite3.h"

#ifndef OGR_GEOPACKAGEUTILITY_H_INCLUDED
#define OGR_GEOPACKAGEUTILITY_H_INCLUDED

typedef struct
{
    char** papszResult;
    int nRowCount;
    int nColCount;
    char *pszErrMsg;
    int rc;
} SQLResult;

typedef struct
{
    OGRBoolean bEmpty;
    OGRBoolean bExtended;
    OGRwkbByteOrder eByteOrder;
    int iSrsId;
    bool bExtentHasXY;
    bool bExtentHasZ;
#ifdef notdef
    bool bExtentHasM;
#endif
    double MinX, MaxX, MinY, MaxY, MinZ, MaxZ;
#ifdef notdef
    double MinM, MaxM;
#endif
    size_t szHeader;
} GPkgHeader;


OGRErr              SQLCommand(sqlite3 *poDb, const char * pszSQL);
int                 SQLGetInteger(sqlite3 * poDb, const char * pszSQL, OGRErr *err);
GIntBig             SQLGetInteger64(sqlite3 * poDb, const char * pszSQL, OGRErr *err);

OGRErr              SQLResultInit(SQLResult * poResult);
OGRErr              SQLQuery(sqlite3 *poDb, const char * pszSQL, SQLResult * poResult);
const char*         SQLResultGetColumn(const SQLResult * poResult, int iColumnNum);
const char*         SQLResultGetValue(const SQLResult * poResult, int iColumnNum, int iRowNum);
int                 SQLResultGetValueAsInteger(const SQLResult * poResult, int iColNum, int iRowNum);
OGRErr              SQLResultFree(SQLResult * poResult);

int                 SQLiteFieldFromOGR(OGRFieldType nType);

OGRFieldType        GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType& eSubType, int& nMaxWidth);
const char*         GPkgFieldFromOGR(OGRFieldType nType, OGRFieldSubType eSubType, int nMaxWidth);
OGRwkbGeometryType  GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ, bool bHasM);

GByte*              GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId, size_t *szWkb);
OGRGeometry*        GPkgGeometryToOGR(const GByte *pabyGpkg, size_t szGpkg, OGRSpatialReference *poSrs);
OGRErr              GPkgEnvelopeToOGR(GByte *pabyGpkg, size_t szGpkg, OGREnvelope *poEnv);

OGRErr              GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t szGpkg, GPkgHeader *poHeader);

#endif
