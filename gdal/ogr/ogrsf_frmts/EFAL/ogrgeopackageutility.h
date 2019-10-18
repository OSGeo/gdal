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

#ifndef OGR_GEOPACKAGEUTILITY_H_INCLUDED
#define OGR_GEOPACKAGEUTILITY_H_INCLUDED

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
    size_t nHeaderLen;
} GPkgHeader;

OGRFieldType        GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType& eSubType, int& nMaxWidth);
const char*         GPkgFieldFromOGR(OGRFieldType eType, OGRFieldSubType eSubType, int nMaxWidth);
OGRwkbGeometryType  GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ, bool bHasM);

GByte*              GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId, size_t *pnWkbLen);
OGRGeometry*        GPkgGeometryToOGR(const GByte *pabyGpkg, size_t nGpkgLen, OGRSpatialReference *poSrs);

OGRErr              GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t nGpkgLen, GPkgHeader *poHeader);

#endif
