/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Utility header for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include <sqlite3.h>

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

int GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType &eSubType,
                   int &nMaxWidth);
const char *GPkgFieldFromOGR(OGRFieldType eType, OGRFieldSubType eSubType,
                             int nMaxWidth);
OGRwkbGeometryType GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ,
                                         bool bHasM);

GByte *GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId,
                           const OGRGeomCoordinateBinaryPrecision *psPrecision,
                           size_t *pnWkbLen);
OGRGeometry *GPkgGeometryToOGR(const GByte *pabyGpkg, size_t nGpkgLen,
                               OGRSpatialReference *poSrs);

OGRErr GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t nGpkgLen,
                         GPkgHeader *poHeader);

bool OGRGeoPackageGetHeader(sqlite3_context *pContext, int /*argc*/,
                            sqlite3_value **argv, GPkgHeader *psHeader,
                            bool bNeedExtent, bool bNeedExtent3D,
                            int iGeomIdx = 0);

bool GPkgUpdateHeader(GByte *pabyGpkg, size_t nGpkgLen, int nSrsId, double MinX,
                      double MaxX, double MinY, double MaxY, double MinZ,
                      double MaxZ);

#endif
