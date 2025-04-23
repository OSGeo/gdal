/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Utility functions for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrgeopackageutility.h"
#include "ogr_p.h"
#include "ogr_wkb.h"
#include "sqlite/ogrsqlitebase.h"
#include <limits>

/* Requirement 20: A GeoPackage SHALL store feature table geometries */
/* with the basic simple feature geometry types (Geometry, Point, */
/* LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, */
/* GeomCollection) */
/* http://opengis.github.io/geopackage/#geometry_types */
OGRwkbGeometryType GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ,
                                         bool bHasM)
{
    OGRwkbGeometryType oType;

    if (EQUAL("Geometry", pszGpkgType))
        oType = wkbUnknown;
    /* The 1.0 spec is not completely clear on what should be used... */
    else if (EQUAL("GeomCollection", pszGpkgType) ||
             EQUAL("GeometryCollection", pszGpkgType))
        oType = wkbGeometryCollection;
    else
    {
        oType = OGRFromOGCGeomType(pszGpkgType);
        if (oType == wkbUnknown)
            oType = wkbNone;
    }

    if ((oType != wkbNone) && bHasZ)
    {
        oType = wkbSetZ(oType);
    }
    if ((oType != wkbNone) && bHasM)
    {
        oType = wkbSetM(oType);
    }

    return oType;
}

/* Requirement 5: The columns of tables in a GeoPackage SHALL only be */
/* declared using one of the data types specified in table GeoPackage */
/* Data Types. */
/* http://opengis.github.io/geopackage/#table_column_data_types */
// return a OGRFieldType value or OFTMaxType + 1
int GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType &eSubType,
                   int &nMaxWidth)
{
    eSubType = OFSTNone;
    nMaxWidth = 0;

    /* Integer types */
    if (STRNCASECMP("INT", pszGpkgType, 3) == 0)
    {
        if (!EQUAL("INT", pszGpkgType) && !EQUAL("INTEGER", pszGpkgType))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as INT",
                     pszGpkgType);
        }
        return OFTInteger64;
    }
    else if (EQUAL("MEDIUMINT", pszGpkgType))
        return OFTInteger;
    else if (EQUAL("SMALLINT", pszGpkgType))
    {
        eSubType = OFSTInt16;
        return OFTInteger;
    }
    else if (EQUAL("TINYINT", pszGpkgType))
        return OFTInteger;  // [-128, 127]
    else if (EQUAL("BOOLEAN", pszGpkgType))
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }

    /* Real types */
    else if (EQUAL("FLOAT", pszGpkgType))
    {
        eSubType = OFSTFloat32;
        return OFTReal;
    }
    else if (EQUAL("DOUBLE", pszGpkgType))
        return OFTReal;
    else if (EQUAL("REAL", pszGpkgType))
        return OFTReal;

    // Only used normally in gpkg_data_column_constraints table, and we
    // need this only is reading it through ExecuteSQL()
    else if (EQUAL("NUMERIC", pszGpkgType))
        return OFTReal;

    /* String/binary types */
    else if (STRNCASECMP("TEXT", pszGpkgType, 4) == 0)
    {
        if (pszGpkgType[4] == '(')
            nMaxWidth = atoi(pszGpkgType + 5);
        else if (pszGpkgType[4] != '\0')
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as TEXT",
                     pszGpkgType);
        }
        return OFTString;
    }

    else if (STRNCASECMP("BLOB", pszGpkgType, 4) == 0)
    {
        if (pszGpkgType[4] != '(' && pszGpkgType[4] != '\0')
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as BLOB",
                     pszGpkgType);
        }
        return OFTBinary;
    }

    /* Date types */
    else if (EQUAL("DATE", pszGpkgType))
        return OFTDate;
    else if (EQUAL("DATETIME", pszGpkgType))
        return OFTDateTime;

    /* Illegal! */
    else
    {
        if (GPkgGeometryTypeToWKB(pszGpkgType, false, false) == wkbNone)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported", pszGpkgType);
        }
        return OFTMaxType + 1;
    }
}

/* Requirement 5: The columns of tables in a GeoPackage SHALL only be */
/* declared using one of the data types specified in table GeoPackage */
/* Data Types. */
/* http://opengis.github.io/geopackage/#table_column_data_types */
const char *GPkgFieldFromOGR(OGRFieldType eType, OGRFieldSubType eSubType,
                             int nMaxWidth)
{
    switch (eType)
    {
        case OFTInteger:
        {
            if (eSubType == OFSTBoolean)
                return "BOOLEAN";
            else if (eSubType == OFSTInt16)
                return "SMALLINT";
            else
                return "MEDIUMINT";
        }
        case OFTInteger64:
            return "INTEGER";
        case OFTReal:
        {
            if (eSubType == OFSTFloat32)
                return "FLOAT";
            else
                return "REAL";
        }
        case OFTString:
        {
            if (nMaxWidth > 0)
                return CPLSPrintf("TEXT(%d)", nMaxWidth);
            else
                return "TEXT";
        }
        case OFTBinary:
            return "BLOB";
        case OFTDate:
            return "DATE";
        case OFTDateTime:
            return "DATETIME";
        default:
            return "TEXT";
    }
}

/* Requirement 19: A GeoPackage SHALL store feature table geometries
 *  with or without optional elevation (Z) and/or measure (M) values in SQL
 *  BLOBs using the Standard GeoPackageBinary format specified in table
 * GeoPackage SQL Geometry Binary Format and clause Geometry Encoding.
 *
 *  http://opengis.github.io/geopackage/#gpb_format
 *
 *   GeoPackageBinaryHeader {
 *     byte[2] magic = 0x4750;
 *     byte version;
 *     byte flags;
 *     int32 srs_id;
 *     double[] envelope;
 *    }
 *
 *   StandardGeoPackageBinary {
 *     GeoPackageBinaryHeader header;
 *     WKBGeometry geometry;
 *   }
 *
 *  Flags byte contents:
 *  Bit 7: Reserved for future
 *  Bit 6: Reserved for future
 *  Bit 5: Using Extended GPKG Binary?
 *  Bit 4: Geometry is Empty?
 *  Bit 3,2,1: Envelope contents (0 none, 1=X/Y, 2=X/Y/Z, 3=X/Y/M, 4=X/Y/Z/M)
 *  Bit 0: Byte order of header (0=big/XDR, 1=little/NDR)
 *
 */

GByte *GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId,
                           const OGRGeomCoordinateBinaryPrecision *psPrecision,
                           size_t *pnWkbLen)
{
    CPLAssert(poGeometry != nullptr);

    GByte byFlags = 0;
    GByte byEnv = 1;
    OGRwkbExportOptions wkbExportOptions;
    if (psPrecision)
        wkbExportOptions.sPrecision = *psPrecision;
    wkbExportOptions.eByteOrder = static_cast<OGRwkbByteOrder>(CPL_IS_LSB);
    OGRErr err;
    OGRBoolean bPoint = (wkbFlatten(poGeometry->getGeometryType()) == wkbPoint);
    OGRBoolean bEmpty = poGeometry->IsEmpty();
    /* We voluntarily use getCoordinateDimension() so as to get only 2 for
     * XY/XYM */
    /* and 3 for XYZ/XYZM as we currently don't write envelopes with M extent.
     */
    int iDims = poGeometry->getCoordinateDimension();

    /* Header has 8 bytes for sure, and optional extra space for bounds */
    size_t nHeaderLen = 2 + 1 + 1 + 4;
    if (!bPoint && !bEmpty)
    {
        nHeaderLen += 8 * 2 * iDims;
    }

    /* Total BLOB size is header + WKB size */
    size_t nWkbLen = nHeaderLen + poGeometry->WkbSize();
    if (nWkbLen > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "too big geometry blob");
        return nullptr;
    }
    GByte *pabyWkb = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nWkbLen));
    if (!pabyWkb)
        return nullptr;
    if (pnWkbLen)
        *pnWkbLen = nWkbLen;

    /* Header Magic */
    pabyWkb[0] = 0x47;
    pabyWkb[1] = 0x50;

    /* GPKG BLOB Version */
    pabyWkb[2] = 0;

    /* Extended? No. */

    /* Envelope dimensionality? */

    /* Don't write envelope for point type */
    if (bPoint)
        byEnv = 0;
    else
        /* 3D envelope for 3D data */
        if (iDims == 3)
            byEnv = 2;
        /* 2D envelope otherwise */
        else
            byEnv = 1;

    /* Empty? No envelope then. */
    if (bEmpty)
    {
        byEnv = 0;
        /* Set empty flag */
        byFlags |= (1 << 4);
    }

    /* Set envelope flags */
    byFlags |= (byEnv << 1);

    /* Byte order of header? */
    /* Use native endianness */
    byFlags |= wkbExportOptions.eByteOrder;

    /* Write flags byte */
    pabyWkb[3] = byFlags;

    /* Write srs_id */
    memcpy(pabyWkb + 4, &iSrsId, 4);

    /* Write envelope */
    if (!bEmpty && !bPoint)
    {
        double *padPtr = reinterpret_cast<double *>(pabyWkb + 8);
        if (iDims == 3)
        {
            OGREnvelope3D oEnv3d;
            poGeometry->getEnvelope(&oEnv3d);
            padPtr[0] = oEnv3d.MinX;
            padPtr[1] = oEnv3d.MaxX;
            padPtr[2] = oEnv3d.MinY;
            padPtr[3] = oEnv3d.MaxY;
            padPtr[4] = oEnv3d.MinZ;
            padPtr[5] = oEnv3d.MaxZ;
        }
        else
        {
            OGREnvelope oEnv;
            poGeometry->getEnvelope(&oEnv);
            padPtr[0] = oEnv.MinX;
            padPtr[1] = oEnv.MaxX;
            padPtr[2] = oEnv.MinY;
            padPtr[3] = oEnv.MaxY;
        }
    }

    GByte *pabyPtr = pabyWkb + nHeaderLen;

    /* Use the wkbVariantIso for ISO SQL/MM output (differs for 3d geometry) */
    wkbExportOptions.eWkbVariant = wkbVariantIso;
    err = poGeometry->exportToWkb(pabyPtr, &wkbExportOptions);
    if (err != OGRERR_NONE)
    {
        CPLFree(pabyWkb);
        return nullptr;
    }

    return pabyWkb;
}

OGRErr GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t nGpkgLen,
                         GPkgHeader *poHeader)
{
    CPLAssert(pabyGpkg != nullptr);
    CPLAssert(poHeader != nullptr);

    /* Magic (match required) */
    if (nGpkgLen < 8 || pabyGpkg[0] != 0x47 || pabyGpkg[1] != 0x50 ||
        pabyGpkg[2] != 0) /* Version (only 0 supported at this time)*/
    {
        memset(poHeader, 0, sizeof(*poHeader));
        return OGRERR_FAILURE;
    }

    /* Flags */
    GByte byFlags = pabyGpkg[3];
    poHeader->bEmpty = (byFlags & (0x01 << 4)) >> 4;
    poHeader->bExtended = (byFlags & (0x01 << 5)) >> 5;
    poHeader->eByteOrder = static_cast<OGRwkbByteOrder>(byFlags & 0x01);
    poHeader->bExtentHasXY = false;
    poHeader->bExtentHasZ = false;
#ifdef notdef
    poHeader->bExtentHasM = false;
#endif
    OGRBoolean bSwap = OGR_SWAP(poHeader->eByteOrder);

    /* Envelope */
    int iEnvelope = (byFlags & (0x07 << 1)) >> 1;
    int nEnvelopeDim = 0;
    if (iEnvelope)
    {
        poHeader->bExtentHasXY = true;
        if (iEnvelope == 1)
        {
            nEnvelopeDim = 2; /* 2D envelope */
        }
        else if (iEnvelope == 2)
        {
            poHeader->bExtentHasZ = true;
            nEnvelopeDim = 3; /* 2D+Z envelope */
        }
        else if (iEnvelope == 3)
        {
#ifdef notdef
            poHeader->bExtentHasM = true;
#endif
            nEnvelopeDim = 3; /* 2D+M envelope */
        }
        else if (iEnvelope == 4)
        {
            poHeader->bExtentHasZ = true;
#ifdef notdef
            poHeader->bExtentHasM = true;
#endif
            nEnvelopeDim = 4; /* 2D+ZM envelope */
        }
        else
        {
            return OGRERR_FAILURE;
        }
    }

    /* SrsId */
    int iSrsId = 0;
    memcpy(&iSrsId, pabyGpkg + 4, 4);
    if (bSwap)
    {
        iSrsId = CPL_SWAP32(iSrsId);
    }
    poHeader->iSrsId = iSrsId;

    if (nGpkgLen < static_cast<size_t>(8 + 8 * 2 * nEnvelopeDim))
    {
        // Not enough bytes
        return OGRERR_FAILURE;
    }

    /* Envelope */
    const double *padPtr = reinterpret_cast<const double *>(pabyGpkg + 8);
    if (poHeader->bExtentHasXY)
    {
        poHeader->MinX = padPtr[0];
        poHeader->MaxX = padPtr[1];
        poHeader->MinY = padPtr[2];
        poHeader->MaxY = padPtr[3];
        if (bSwap)
        {
            CPL_SWAPDOUBLE(&(poHeader->MinX));
            CPL_SWAPDOUBLE(&(poHeader->MaxX));
            CPL_SWAPDOUBLE(&(poHeader->MinY));
            CPL_SWAPDOUBLE(&(poHeader->MaxY));
        }
    }
    if (poHeader->bExtentHasZ)
    {
        poHeader->MinZ = padPtr[4];
        poHeader->MaxZ = padPtr[5];
        if (bSwap)
        {
            CPL_SWAPDOUBLE(&(poHeader->MinZ));
            CPL_SWAPDOUBLE(&(poHeader->MaxZ));
        }
    }
#ifdef notdef
    if (poHeader->bExtentHasM)
    {
        poHeader->MinM = padPtr[(poHeader->bExtentHasZ) ? 6 : 4];
        poHeader->MaxM = padPtr[(poHeader->bExtentHasZ) ? 7 : 5];
        if (bSwap)
        {
            CPL_SWAPDOUBLE(&(poHeader->MinM));
            CPL_SWAPDOUBLE(&(poHeader->MaxM));
        }
    }
#endif

    /* Header size in byte stream */
    poHeader->nHeaderLen = 8 + 8 * 2 * nEnvelopeDim;

    return OGRERR_NONE;
}

bool GPkgUpdateHeader(GByte *pabyGpkg, size_t nGpkgLen, int nSrsId, double MinX,
                      double MaxX, double MinY, double MaxY, double MinZ,
                      double MaxZ)
{
    CPLAssert(nGpkgLen >= 8);

    /* Flags */
    const GByte byFlags = pabyGpkg[3];
    const auto eByteOrder = static_cast<OGRwkbByteOrder>(byFlags & 0x01);
    const OGRBoolean bSwap = OGR_SWAP(eByteOrder);

    /* SrsId */
    if (bSwap)
    {
        nSrsId = CPL_SWAP32(nSrsId);
    }
    memcpy(pabyGpkg + 4, &nSrsId, 4);

    /* Envelope */
    const int iEnvelope = (byFlags & (0x07 << 1)) >> 1;
    int nEnvelopeDim = 0;
    if (iEnvelope)
    {
        if (iEnvelope == 1)
        {
            nEnvelopeDim = 2; /* 2D envelope */
        }
        else if (iEnvelope == 2)
        {
            nEnvelopeDim = 3; /* 2D+Z envelope */
        }
        else if (iEnvelope == 3)
        {
            nEnvelopeDim = 3; /* 2D+M envelope */
        }
        else if (iEnvelope == 4)
        {
            nEnvelopeDim = 4; /* 2D+ZM envelope */
        }
        else
        {
            return false;
        }
    }
    else
    {
        return true;
    }

    if (nGpkgLen < static_cast<size_t>(8 + 8 * 2 * nEnvelopeDim))
    {
        // Not enough bytes
        return false;
    }

    /* Envelope */
    if (bSwap)
    {
        CPL_SWAPDOUBLE(&(MinX));
        CPL_SWAPDOUBLE(&(MaxX));
        CPL_SWAPDOUBLE(&(MinY));
        CPL_SWAPDOUBLE(&(MaxY));
        CPL_SWAPDOUBLE(&(MinZ));
        CPL_SWAPDOUBLE(&(MaxZ));
    }

    double *padPtr = reinterpret_cast<double *>(pabyGpkg + 8);
    memcpy(&padPtr[0], &MinX, sizeof(double));
    memcpy(&padPtr[1], &MaxX, sizeof(double));
    memcpy(&padPtr[2], &MinY, sizeof(double));
    memcpy(&padPtr[3], &MaxY, sizeof(double));

    if (iEnvelope == 2 || iEnvelope == 4)
    {
        memcpy(&padPtr[4], &MinZ, sizeof(double));
        memcpy(&padPtr[5], &MaxZ, sizeof(double));
    }

    return true;
}

OGRGeometry *GPkgGeometryToOGR(const GByte *pabyGpkg, size_t nGpkgLen,
                               OGRSpatialReference *poSrs)
{
    CPLAssert(pabyGpkg != nullptr);

    GPkgHeader oHeader;

    /* Read header */
    OGRErr err = GPkgHeaderFromWKB(pabyGpkg, nGpkgLen, &oHeader);
    if (err != OGRERR_NONE)
        return nullptr;

    /* WKB pointer */
    const GByte *pabyWkb = pabyGpkg + oHeader.nHeaderLen;
    size_t nWkbLen = nGpkgLen - oHeader.nHeaderLen;

    /* Parse WKB */
    OGRGeometry *poGeom = nullptr;
    err = OGRGeometryFactory::createFromWkb(pabyWkb, poSrs, &poGeom,
                                            static_cast<int>(nWkbLen));
    if (err != OGRERR_NONE)
        return nullptr;

    return poGeom;
}

/************************************************************************/
/*                     OGRGeoPackageGetHeader()                         */
/************************************************************************/

bool OGRGeoPackageGetHeader(sqlite3_context * /*pContext*/, int /*argc*/,
                            sqlite3_value **argv, GPkgHeader *psHeader,
                            bool bNeedExtent, bool bNeedExtent3D, int iGeomIdx)
{

    // Extent3D implies extent
    const bool bNeedAnyExtent{bNeedExtent || bNeedExtent3D};

    if (sqlite3_value_type(argv[iGeomIdx]) != SQLITE_BLOB)
    {
        memset(psHeader, 0, sizeof(*psHeader));
        return false;
    }
    const int nBLOBLen = sqlite3_value_bytes(argv[iGeomIdx]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[iGeomIdx]));

    if (nBLOBLen < 8)
    {
        memset(psHeader, 0, sizeof(*psHeader));
        return false;
    }
    else if (GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, psHeader) != OGRERR_NONE)
    {
        bool bEmpty = false;
        memset(psHeader, 0, sizeof(*psHeader));
        if (OGRSQLiteGetSpatialiteGeometryHeader(
                pabyBLOB, nBLOBLen, &(psHeader->iSrsId), nullptr, &bEmpty,
                &(psHeader->MinX), &(psHeader->MinY), &(psHeader->MaxX),
                &(psHeader->MaxY)) == OGRERR_NONE)
        {
            psHeader->bEmpty = bEmpty;
            psHeader->bExtentHasXY = !bEmpty;
            if (!bNeedExtent3D && !(bEmpty && bNeedAnyExtent))
                return true;
        }

        return false;
    }

    if (psHeader->bEmpty && bNeedAnyExtent)
    {
        return false;
    }
    else if (!psHeader->bExtentHasXY && bNeedExtent && !bNeedExtent3D)
    {
        OGREnvelope sEnvelope;
        if (OGRWKBGetBoundingBox(pabyBLOB + psHeader->nHeaderLen,
                                 static_cast<size_t>(nBLOBLen) -
                                     psHeader->nHeaderLen,
                                 sEnvelope))
        {
            psHeader->MinX = sEnvelope.MinX;
            psHeader->MaxX = sEnvelope.MaxX;
            psHeader->MinY = sEnvelope.MinY;
            psHeader->MaxY = sEnvelope.MaxY;
            return true;
        }
        return false;
    }
    else if (!psHeader->bExtentHasZ && bNeedExtent3D)
    {
        OGREnvelope3D sEnvelope3D;
        if (OGRWKBGetBoundingBox(pabyBLOB + psHeader->nHeaderLen,
                                 static_cast<size_t>(nBLOBLen) -
                                     psHeader->nHeaderLen,
                                 sEnvelope3D))
        {
            psHeader->MinX = sEnvelope3D.MinX;
            psHeader->MaxX = sEnvelope3D.MaxX;
            psHeader->MinY = sEnvelope3D.MinY;
            psHeader->MaxY = sEnvelope3D.MaxY;
            psHeader->MinZ = sEnvelope3D.MinZ;
            psHeader->MaxZ = sEnvelope3D.MaxZ;
            return true;
        }
        return false;
    }
    return true;
}
