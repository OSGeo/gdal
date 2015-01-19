/******************************************************************************
 * $Id$
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

/* Runs a SQL command and ignores the result (good for INSERT/UPDATE/CREATE) */
OGRErr SQLCommand(sqlite3 * poDb, const char * pszSQL)
{
    CPLAssert( poDb != NULL );
    CPLAssert( pszSQL != NULL );

    char *pszErrMsg = NULL;
    //CPLDebug("GPKG", "exec(%s)", pszSQL);
    int rc = sqlite3_exec(poDb, pszSQL, NULL, NULL, &pszErrMsg);
    
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_exec(%s) failed: %s",
                  pszSQL, pszErrMsg ? pszErrMsg : "" );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}


OGRErr SQLResultInit(SQLResult * poResult)
{
    poResult->papszResult = NULL;
    poResult->pszErrMsg = NULL;
    poResult->nRowCount = 0;
    poResult->nColCount = 0;
    poResult->rc = 0;
    return OGRERR_NONE;
}


OGRErr SQLQuery(sqlite3 * poDb, const char * pszSQL, SQLResult * poResult)
{
    CPLAssert( poDb != NULL );
    CPLAssert( pszSQL != NULL );
    CPLAssert( poResult != NULL );

    SQLResultInit(poResult);

    poResult->rc = sqlite3_get_table(
        poDb, pszSQL,
        &(poResult->papszResult), 
        &(poResult->nRowCount), 
        &(poResult->nColCount), 
        &(poResult->pszErrMsg) );
    
    if( poResult->rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "sqlite3_get_table(%s) failed: %s", pszSQL, poResult->pszErrMsg );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}


OGRErr SQLResultFree(SQLResult * poResult)
{
    if ( poResult->papszResult )
        sqlite3_free_table(poResult->papszResult);

    if ( poResult->pszErrMsg )
        sqlite3_free(poResult->pszErrMsg);
        
    return OGRERR_NONE;
}

const char* SQLResultGetColumn(const SQLResult * poResult, int iColNum)
{
    if ( ! poResult ) 
        return NULL;
        
    if ( iColNum < 0 || iColNum >= poResult->nColCount )
        return NULL;
    
    return poResult->papszResult[iColNum];
}

const char* SQLResultGetValue(const SQLResult * poResult, int iColNum, int iRowNum)
{
    if ( ! poResult ) 
        return NULL;

    int nCols = poResult->nColCount;
    int nRows = poResult->nRowCount;    
        
    if ( iColNum < 0 || iColNum >= nCols )
        return NULL;

    if ( iRowNum < 0 || iRowNum >= nRows )
        return NULL;
        
    return poResult->papszResult[ nCols + iRowNum * nCols + iColNum ];
}

int SQLResultGetValueAsInteger(const SQLResult * poResult, int iColNum, int iRowNum)
{
    if ( ! poResult ) 
        return 0;
        
    int nCols = poResult->nColCount;
    int nRows = poResult->nRowCount;
    
    if ( iColNum < 0 || iColNum >= nCols )
        return 0;

    if ( iRowNum < 0 || iRowNum >= nRows )
        return 0;
    
    char *pszValue = poResult->papszResult[ nCols + iRowNum * nCols + iColNum ];
    if ( ! pszValue )
        return 0;

    return atoi(pszValue);
}

/* Returns the first row of first column of SQL as integer */
int SQLGetInteger(sqlite3 * poDb, const char * pszSQL, OGRErr *err)
{
    CPLAssert( poDb != NULL );
    
    sqlite3_stmt *poStmt;
    int rc, i;
    
    /* Prepare the SQL */
    rc = sqlite3_prepare_v2(poDb, pszSQL, strlen(pszSQL), &poStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare_v2(%s) failed: %s",
                  pszSQL, sqlite3_errmsg( poDb ) );
        if ( err ) *err = OGRERR_FAILURE;
        return 0;
    }
    
    /* Execute and fetch first row */
    rc = sqlite3_step(poStmt);
    if ( rc != SQLITE_ROW )
    {
        if ( err ) *err = OGRERR_FAILURE;
        sqlite3_finalize(poStmt);
        return 0;
    }
    
    /* Read the integer from the row */
    i = sqlite3_column_int(poStmt, 0);
    sqlite3_finalize(poStmt);
    
    if ( err ) *err = OGRERR_NONE;
    return i;
}


/* Requirement 20: A GeoPackage SHALL store feature table geometries */
/* with the basic simple feature geometry types (Geometry, Point, */
/* LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, */
/* GeomCollection) */
/* http://opengis.github.io/geopackage/#geometry_types */
OGRwkbGeometryType GPkgGeometryTypeToWKB(const char *pszGpkgType, int bHasZ)
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
        return OFTInteger;
    else if ( EQUAL("MEDIUMINT", pszGpkgType) )
        return OFTInteger;
    else if ( EQUAL("SMALLINT", pszGpkgType) )
    {
        eSubType = OFSTInt16;
        return OFTInteger;
    }
    else if ( EQUAL("TINYINT", pszGpkgType) )
        return OFTInteger;
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
        
    /* String/binary types */
    else if ( STRNCASECMP("TEXT", pszGpkgType, 4) == 0 )
    {
        if( pszGpkgType[4] == '(' )
            nMaxWidth = atoi(pszGpkgType+5);
        return OFTString;
    }
        
    else if ( STRNCASECMP("BLOB", pszGpkgType, 4) == 0 )
        return OFTBinary;
        
    /* Date types */
    else if ( EQUAL("DATE", pszGpkgType) )
        return OFTDate;
    else if ( EQUAL("DATETIME", pszGpkgType) )
        return OFTDateTime;

    /* Illegal! */
    else 
        return (OGRFieldType)(OFTMaxType + 1);
}

/* Requirement 5: The columns of tables in a GeoPackage SHALL only be */
/* declared using one of the data types specified in table GeoPackage */
/* Data Types. */
/* http://opengis.github.io/geopackage/#table_column_data_types */
const char* GPkgFieldFromOGR(OGRFieldType nType, OGRFieldSubType eSubType,
                             int nMaxWidth)
{
    switch(nType)
    {
        case OFTInteger:
        {
            if( eSubType == OFSTBoolean )
                return "BOOLEAN";
            else if( eSubType == OFSTInt16 )
                return "SMALLINT";
            else
                return "INTEGER";
        }
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
            return NULL;
    }
}


int SQLiteFieldFromOGR(OGRFieldType nType)
{
    switch(nType)
    {
        case OFTInteger:
            return SQLITE_INTEGER;
        case OFTReal:
            return SQLITE_FLOAT;
        case OFTString:
            return SQLITE_TEXT;
        case OFTBinary:
            return SQLITE_BLOB;
        case OFTDate:
            return SQLITE_TEXT;
        case OFTDateTime:
            return SQLITE_TEXT;
        default:
            return 0;
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

GByte* GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId, size_t *pszWkb)
{
    CPLAssert( poGeometry != NULL );
    
    GByte *pabyPtr;
    GByte byFlags = 0;
    GByte byEnv = 1;
    OGRwkbByteOrder eByteOrder = (OGRwkbByteOrder)CPL_IS_LSB;
    OGRErr err;
    OGRBoolean bPoint = (wkbFlatten(poGeometry->getGeometryType()) == wkbPoint);
    OGRBoolean bEmpty = poGeometry->IsEmpty();
    int iDims = poGeometry->getCoordinateDimension();

    /* Header has 8 bytes for sure, and optional extra space for bounds */
    size_t szHeader = 2+1+1+4;    
    if ( ! bPoint && ! bEmpty )
    {
        szHeader += 8*2*iDims;
    }
    
    /* Total BLOB size is header + WKB size */
    size_t szWkb = szHeader + poGeometry->WkbSize();
    GByte *pabyWkb = (GByte *)CPLMalloc(szWkb);
    if (pszWkb)
        *pszWkb = szWkb;
    
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
    
    pabyPtr = pabyWkb + szHeader;
    
    /* Use the wkbVariantIso for ISO SQL/MM output (differs for 3d geometry) */
    err = poGeometry->exportToWkb(eByteOrder, pabyPtr, wkbVariantIso);
    if ( err != OGRERR_NONE )
    {
        CPLFree(pabyWkb);
        return NULL;
    }
    
    return pabyWkb; 
}


OGRErr GPkgHeaderFromWKB(const GByte *pabyGpkg, GPkgHeader *poHeader)
{
    CPLAssert( pabyGpkg != NULL );
    CPLAssert( poHeader != NULL );

    /* Magic (match required) */
    if ( pabyGpkg[0] != 0x47 || 
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
    OGRBoolean bSwap = OGR_SWAP(poHeader->eByteOrder);

    /* Envelope */
    int iEnvelope = (byFlags & (0x07 << 1)) >> 1;
    if ( iEnvelope == 1 )
        poHeader->iDims = 2; /* 2D envelope */
    else if ( iEnvelope == 2 )
        poHeader->iDims = 3; /* 3D envelope */
    else 
        poHeader->iDims = 0; /* No envelope */

    /* SrsId */
    int iSrsId;
    memcpy(&iSrsId, pabyGpkg+4, 4);    
    if ( bSwap )
    {
        iSrsId = CPL_SWAP32(iSrsId);
    }
    poHeader->iSrsId = iSrsId;
    
    /* Envelope */
    double *padPtr = (double*)(pabyGpkg+8);
    if ( poHeader->iDims >= 2 )
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
    if ( poHeader->iDims == 3 )
    {
        poHeader->MinZ = padPtr[4];
        poHeader->MaxZ = padPtr[5];
        if ( bSwap )
        {
            CPL_SWAPDOUBLE(&(poHeader->MinZ));
            CPL_SWAPDOUBLE(&(poHeader->MaxZ));
        }
    }
    
    /* Header size in byte stream */
    poHeader->szHeader = 8 + 8*2*(poHeader->iDims);
    
    return OGRERR_NONE;
}

OGRGeometry* GPkgGeometryToOGR(const GByte *pabyGpkg, size_t szGpkg, OGRSpatialReference *poSrs)
{
    CPLAssert( pabyGpkg != NULL );
    
    GPkgHeader oHeader;
    OGRGeometry *poGeom;
    
    /* Read header */
    OGRErr err = GPkgHeaderFromWKB(pabyGpkg, &oHeader);
    if ( err != OGRERR_NONE )
        return NULL;

    /* WKB pointer */
    const GByte *pabyWkb = pabyGpkg + oHeader.szHeader;
    size_t szWkb = szGpkg - oHeader.szHeader;

    /* Parse WKB */
    err = OGRGeometryFactory::createFromWkb((GByte*)pabyWkb, poSrs, &poGeom, szWkb);
    if ( err != OGRERR_NONE )
        return NULL;

    return poGeom;
}


OGRErr GPkgEnvelopeToOGR(GByte *pabyGpkg,
                         CPL_UNUSED size_t szGpkg,
                         OGREnvelope *poEnv)
{
    CPLAssert( poEnv != NULL );
    CPLAssert( pabyGpkg != NULL );

    GPkgHeader oHeader;

    /* Read header */
    OGRErr err = GPkgHeaderFromWKB(pabyGpkg, &oHeader);
    if ( err != OGRERR_NONE )
        return err;

    if ( oHeader.bEmpty || oHeader.iDims == 0 )
    {
        return OGRERR_FAILURE;
    }
    
    poEnv->MinX = oHeader.MinX;
    poEnv->MaxX = oHeader.MaxX;
    poEnv->MinY = oHeader.MinY;
    poEnv->MaxY = oHeader.MaxY;
    
    // if ( oHeader.iDims == 3 )
    // {
    //     poEnv->MinZ = oHeader.MinZ;
    //     poEnv->MaxZ = oHeader.MaxZ;
    // }
    
    return OGRERR_NONE;
}
