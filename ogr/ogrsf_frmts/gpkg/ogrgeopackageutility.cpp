/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Utility functions for OGR GeoPackage driver.
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

#include "ogrgeopackageutility.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/* Requirement 20: A GeoPackage SHALL store feature table geometries */
/* with the basic simple feature geometry types (Geometry, Point, */
/* LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, */
/* GeomCollection) */
/* http://opengis.github.io/geopackage/#geometry_types */
OGRwkbGeometryType GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ, bool bHasM)
{
    OGRwkbGeometryType oType;

    if ( EQUAL("Geometry", pszGpkgType) )
        oType = wkbUnknown;
    /* The 1.0 spec is not completely clear on what should be used... */
    else if ( EQUAL("GeomCollection", pszGpkgType) ||
              EQUAL("GeometryCollection", pszGpkgType) )
        oType =  wkbGeometryCollection;
    else
    {
        oType = OGRFromOGCGeomType(pszGpkgType);
        if( oType == wkbUnknown )
            oType = wkbNone;
    }

    if ( (oType != wkbNone) && bHasZ )
    {
        oType = wkbSetZ(oType);
    }
    if ( (oType != wkbNone) && bHasM )
    {
        oType = wkbSetM(oType);
    }

    return oType;
}

/* Requirement 5: The columns of tables in a GeoPackage SHALL only be */
/* declared using one of the data types specified in table GeoPackage */
/* Data Types. */
/* http://opengis.github.io/geopackage/#table_column_data_types */
OGRFieldType GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType& eSubType,
                            int& nMaxWidth)
{
    eSubType = OFSTNone;
    nMaxWidth = 0;

    /* Integer types */
    if ( STRNCASECMP("INT", pszGpkgType, 3) == 0 )
    {
        if( !EQUAL("INT", pszGpkgType) && !EQUAL("INTEGER", pszGpkgType) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as INT", pszGpkgType);
        }
        return OFTInteger64;
    }
    else if ( EQUAL("MEDIUMINT", pszGpkgType) )
        return OFTInteger;
    else if ( EQUAL("SMALLINT", pszGpkgType) )
    {
        eSubType = OFSTInt16;
        return OFTInteger;
    }
    else if ( EQUAL("TINYINT", pszGpkgType) )
        return OFTInteger; // [-128, 127]
    else if ( EQUAL("BOOLEAN", pszGpkgType) )
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }

    /* Real types */
    else if ( EQUAL("FLOAT", pszGpkgType) )
    {
        eSubType = OFSTFloat32;
        return OFTReal;
    }
    else if ( EQUAL("DOUBLE", pszGpkgType) )
        return OFTReal;
    else if ( EQUAL("REAL", pszGpkgType) )
        return OFTReal;

    // Only used normally in gpkg_data_column_constraints table, and we
    // need this only is reading it through ExecuteSQL()
    else if ( EQUAL("NUMERIC", pszGpkgType) )
        return OFTReal;

    /* String/binary types */
    else if ( STRNCASECMP("TEXT", pszGpkgType, 4) == 0 )
    {
        if( pszGpkgType[4] == '(' )
            nMaxWidth = atoi(pszGpkgType+5);
        else if( pszGpkgType[4] != '\0' )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as TEXT", pszGpkgType);
        }
        return OFTString;
    }

    else if ( STRNCASECMP("BLOB", pszGpkgType, 4) == 0 )
    {
        if( pszGpkgType[4] != '(' && pszGpkgType[4] != '\0' )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Field format '%s' not supported. "
                     "Interpreted as BLOB", pszGpkgType);
        }
        return OFTBinary;
    }

    /* Date types */
    else if ( EQUAL("DATE", pszGpkgType) )
        return OFTDate;
    else if ( EQUAL("DATETIME", pszGpkgType) )
        return OFTDateTime;

    /* Illegal! */
    else
    {
        if( GPkgGeometryTypeToWKB(pszGpkgType, false, false) == wkbNone )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Field format '%s' not supported", pszGpkgType);
        }
        return (OGRFieldType)(OFTMaxType + 1);
    }
}

/* Requirement 5: The columns of tables in a GeoPackage SHALL only be */
/* declared using one of the data types specified in table GeoPackage */
/* Data Types. */
/* http://opengis.github.io/geopackage/#table_column_data_types */
const char* GPkgFieldFromOGR(OGRFieldType eType, OGRFieldSubType eSubType,
                             int nMaxWidth)
{
    switch(eType)
    {
        case OFTInteger:
        {
            if( eSubType == OFSTBoolean )
                return "BOOLEAN";
            else if( eSubType == OFSTInt16 )
                return "SMALLINT";
            else
                return "MEDIUMINT";
        }
        case OFTInteger64:
            return "INTEGER";
        case OFTReal:
        {
            if( eSubType == OFSTFloat32 )
                return "FLOAT";
            else
                return "REAL";
        }
        case OFTString:
        {
            if( nMaxWidth > 0 )
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
*  BLOBs using the Standard GeoPackageBinary format specified in table GeoPackage
*  SQL Geometry Binary Format and clause Geometry Encoding.
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

GByte* GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId,
                           size_t *pnWkbLen)
{
    CPLAssert( poGeometry != nullptr );

    GByte byFlags = 0;
    GByte byEnv = 1;
    OGRwkbByteOrder eByteOrder = (OGRwkbByteOrder)CPL_IS_LSB;
    OGRErr err;
    OGRBoolean bPoint = (wkbFlatten(poGeometry->getGeometryType()) == wkbPoint);
    OGRBoolean bEmpty = poGeometry->IsEmpty();
    /* We voluntarily use getCoordinateDimension() so as to get only 2 for XY/XYM */
    /* and 3 for XYZ/XYZM as we currently don't write envelopes with M extent. */
    int iDims = poGeometry->getCoordinateDimension();

    /* Header has 8 bytes for sure, and optional extra space for bounds */
    size_t nHeaderLen = 2+1+1+4;
    if ( ! bPoint && ! bEmpty )
    {
        nHeaderLen += 8*2*iDims;
    }

    /* Total BLOB size is header + WKB size */
    size_t nWkbLen = nHeaderLen + poGeometry->WkbSize();
    GByte *pabyWkb = (GByte *)CPLMalloc(nWkbLen);
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
    if ( bPoint )
        byEnv = 0;
    else
        /* 3D envelope for 3D data */
        if ( iDims == 3 )
            byEnv = 2;
        /* 2D envelope otherwise */
        else
            byEnv = 1;

    /* Empty? No envelope then. */
    if ( bEmpty )
    {
        byEnv = 0;
        /* Set empty flag */
        byFlags |= (1 << 4);
    }

    /* Set envelope flags */
    byFlags |= (byEnv << 1);

    /* Byte order of header? */
    /* Use native endianness */
    byFlags |= eByteOrder;

    /* Write flags byte */
    pabyWkb[3] = byFlags;

    /* Write srs_id */
    memcpy(pabyWkb+4, &iSrsId, 4);

    /* Write envelope */
    if ( ! bEmpty && ! bPoint )
    {
        double *padPtr = (double*)(pabyWkb+8);
        if ( iDims == 3 )
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
    err = poGeometry->exportToWkb(eByteOrder, pabyPtr, wkbVariantIso);
    if ( err != OGRERR_NONE )
    {
        CPLFree(pabyWkb);
        return nullptr;
    }

    return pabyWkb;
}

OGRErr GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t nGpkgLen, GPkgHeader *poHeader)
{
    CPLAssert( pabyGpkg != nullptr );
    CPLAssert( poHeader != nullptr );

    /* Magic (match required) */
    if ( nGpkgLen < 8 ||
         pabyGpkg[0] != 0x47 ||
         pabyGpkg[1] != 0x50 ||
         pabyGpkg[2] != 0 )  /* Version (only 0 supported at this time)*/
    {
        return OGRERR_FAILURE;
    }

    /* Flags */
    GByte byFlags = pabyGpkg[3];
    poHeader->bEmpty = (byFlags & (0x01 << 4)) >> 4;
    poHeader->bExtended = (byFlags & (0x01 << 5)) >> 5;
    poHeader->eByteOrder = (OGRwkbByteOrder)(byFlags & 0x01);
    poHeader->bExtentHasXY = false;
    poHeader->bExtentHasZ = false;
#ifdef notdef
    poHeader->bExtentHasM = false;
#endif
    OGRBoolean bSwap = OGR_SWAP(poHeader->eByteOrder);

    /* Envelope */
    int iEnvelope = (byFlags & (0x07 << 1)) >> 1;
    int nEnvelopeDim = 0;
    if( iEnvelope )
    {
        poHeader->bExtentHasXY = true;
        if( iEnvelope == 1 )
        {
            nEnvelopeDim = 2; /* 2D envelope */
        }
        else if ( iEnvelope == 2 )
        {
            poHeader->bExtentHasZ = true;
            nEnvelopeDim = 3; /* 2D+Z envelope */
        }
        else if ( iEnvelope == 3 )
        {
#ifdef notdef
            poHeader->bExtentHasM = true;
#endif
            nEnvelopeDim = 3; /* 2D+M envelope */
        }
        else if ( iEnvelope == 4 )
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
    memcpy(&iSrsId, pabyGpkg+4, 4);
    if ( bSwap )
    {
        iSrsId = CPL_SWAP32(iSrsId);
    }
    poHeader->iSrsId = iSrsId;

    if( nGpkgLen < static_cast<size_t>(8 + 8*2*nEnvelopeDim) )
    {
        // Not enough bytes
        return OGRERR_FAILURE;
    }

    /* Envelope */
    double *padPtr = (double*)(pabyGpkg+8);
    if ( poHeader->bExtentHasXY )
    {
        poHeader->MinX = padPtr[0];
        poHeader->MaxX = padPtr[1];
        poHeader->MinY = padPtr[2];
        poHeader->MaxY = padPtr[3];
        if ( bSwap )
        {
            CPL_SWAPDOUBLE(&(poHeader->MinX));
            CPL_SWAPDOUBLE(&(poHeader->MaxX));
            CPL_SWAPDOUBLE(&(poHeader->MinY));
            CPL_SWAPDOUBLE(&(poHeader->MaxY));
        }
    }
    if ( poHeader->bExtentHasZ )
    {
        poHeader->MinZ = padPtr[4];
        poHeader->MaxZ = padPtr[5];
        if ( bSwap )
        {
            CPL_SWAPDOUBLE(&(poHeader->MinZ));
            CPL_SWAPDOUBLE(&(poHeader->MaxZ));
        }
    }
#ifdef notdef
    if ( poHeader->bExtentHasM )
    {
        poHeader->MinM = padPtr[ ( poHeader->bExtentHasZ ) ? 6 : 4 ];
        poHeader->MaxM = padPtr[ ( poHeader->bExtentHasZ ) ? 7 : 5 ];
        if ( bSwap )
        {
            CPL_SWAPDOUBLE(&(poHeader->MinM));
            CPL_SWAPDOUBLE(&(poHeader->MaxM));
        }
    }
#endif

    /* Header size in byte stream */
    poHeader->nHeaderLen = 8 + 8*2*nEnvelopeDim;

    return OGRERR_NONE;
}

OGRGeometry* GPkgGeometryToOGR(const GByte *pabyGpkg, size_t nGpkgLen, OGRSpatialReference *poSrs)
{
    CPLAssert( pabyGpkg != nullptr );

    GPkgHeader oHeader;

    /* Read header */
    OGRErr err = GPkgHeaderFromWKB(pabyGpkg, nGpkgLen, &oHeader);
    if ( err != OGRERR_NONE )
        return nullptr;

    /* WKB pointer */
    const GByte *pabyWkb = pabyGpkg + oHeader.nHeaderLen;
    size_t nWkbLen = nGpkgLen - oHeader.nHeaderLen;

    /* Parse WKB */
    OGRGeometry *poGeom = nullptr;
    err = OGRGeometryFactory::createFromWkb(pabyWkb, poSrs, &poGeom,
                                            static_cast<int>(nWkbLen));
    if ( err != OGRERR_NONE )
        return nullptr;

    return poGeom;
}
