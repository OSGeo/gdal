/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteLayer class, code shared between 
 *           the direct table access, and the generic SQL results.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_sqlite.h"
#include <cassert>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRSQLiteLayer()                           */
/************************************************************************/

OGRSQLiteLayer::OGRSQLiteLayer()

{
    poDS = NULL;

    pszFIDColumn = NULL;

    eGeomFormat = OSGF_None;

    hStmt = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 

    panFieldOrdinals = NULL;

    bTriedAsSpatiaLite = FALSE;
    bHasSpatialIndex = FALSE;
}

/************************************************************************/
/*                          ~OGRSQLiteLayer()                           */
/************************************************************************/

OGRSQLiteLayer::~OGRSQLiteLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "SQLite", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( hStmt != NULL )
    {
        sqlite3_finalize( hStmt );
        hStmt = NULL;
    }

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    if( poSRS != NULL )
        poSRS->Dereference();

    CPLFree( pszFIDColumn );
    CPLFree( panFieldOrdinals );
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRSQLiteLayer::BuildFeatureDefn( const char *pszLayerName, 
                                         sqlite3_stmt *hStmt )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    int    nRawColumns = sqlite3_column_count( hStmt );

    poFeatureDefn->Reference();

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField( sqlite3_column_name( hStmt, iCol ), 
                                OFTString );

        // In some cases, particularly when there is a real name for
        // the primary key/_rowid_ column we will end up getting the
        // primary key column appearing twice.  Ignore any repeated names.
        if( poFeatureDefn->GetFieldIndex( oField.GetNameRef() ) != -1 )
            continue;

        if( pszFIDColumn != NULL && EQUAL(pszFIDColumn, oField.GetNameRef()))
            continue;

        //oField.SetWidth( MAX(0,poStmt->GetColSize( iCol )) );

        if( osGeomColumn.size()
            && EQUAL(oField.GetNameRef(),osGeomColumn) )
            continue;
        
        int nColType = sqlite3_column_type( hStmt, iCol );
        const char * pszDeclType = sqlite3_column_decltype(hStmt, iCol);
        //CPLDebug("SQLITE", "decltype(%s) = %s",
        //         oField.GetNameRef(), pszDeclType ? pszDeclType : "null");
        if (pszDeclType != NULL)
        {
            if (EQUAL(pszDeclType, "INTEGER"))
                nColType = SQLITE_INTEGER;
            else if (EQUAL(pszDeclType, "FLOAT"))
                nColType = SQLITE_FLOAT;
            else if (EQUAL(pszDeclType, "BLOB"))
                nColType = SQLITE_BLOB;
            else if (EQUAL(pszDeclType, "TEXT") ||
                     EQUAL(pszDeclType, "VARCHAR"))
                nColType = SQLITE_TEXT;
        }

        // Recognise some common geometry column names.
        if( (EQUAL(oField.GetNameRef(),"wkt_geometry") 
             || EQUAL(oField.GetNameRef(),"geometry")
             || EQUALN(oField.GetNameRef(), "asbinary(", 9)
             || EQUALN(oField.GetNameRef(), "astext(", 7))
            && osGeomColumn.size() == 0 )
        {
            if( nColType == SQLITE_BLOB )
            {
                osGeomColumn = oField.GetNameRef();
                eGeomFormat = OSGF_WKB;
                /* This could also be a SpatialLite geometry, so */
                /* we'll also try to decode as SpatialLite if */
                /* bTriedAsSpatiaLite is not FALSE */
                continue;
            }
            else if( nColType == SQLITE_TEXT )
            {
                osGeomColumn = oField.GetNameRef();
                eGeomFormat = OSGF_WKT;
                continue;
            }
        }

        // SpatialLite / Gaia
        if( EQUAL(oField.GetNameRef(),"GaiaGeometry") 
            && osGeomColumn.size() == 0 )
        {
            osGeomColumn = oField.GetNameRef();
            eGeomFormat = OSGF_SpatiaLite;
            continue;
        }

        // The rowid is for internal use, not a real column.
        if( EQUAL(oField.GetNameRef(),"_rowid_") )
            continue;

        // The OGC_FID is for internal use, not a real user visible column.
        if( EQUAL(oField.GetNameRef(),"OGC_FID") )
            continue;

        switch( nColType )
        {
          case SQLITE_INTEGER:
            oField.SetType( OFTInteger );
            break;

          case SQLITE_FLOAT:
            oField.SetType( OFTReal );
            break;

          case SQLITE_BLOB:
            oField.SetType( OFTBinary );
            break;

          default:
            /* leave it as OFTString */;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

/* -------------------------------------------------------------------- */
/*      If we have no geometry source, we know our geometry type is     */
/*      none.                                                           */
/* -------------------------------------------------------------------- */
    if( osGeomColumn.size() == 0 )
        poFeatureDefn->SetGeomType( wkbNone );

    return CE_None;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRSQLiteLayer::GetFIDColumn() 

{
    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRSQLiteLayer::GetGeometryColumn() 

{
    if( osGeomColumn.size() != 0 )
        return osGeomColumn.c_str();
    else
        return "";
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteLayer::GetNextFeature()

{
    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSQLiteLayer::GetNextRawFeature()

{
    if( GetStatement() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    int rc;

    rc = sqlite3_step( hStmt );
    if( rc != SQLITE_ROW )
    {
        if ( rc != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "In GetNextRawFeature(): sqlite3_step() : %s", 
                    sqlite3_errmsg(poDS->GetDB()) );
        }

        ClearStatement();

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Set FID if we have a column to set it from.                     */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn != NULL )
    {
        int iFIDCol;

        for( iFIDCol = 0; iFIDCol < sqlite3_column_count(hStmt); iFIDCol++ )
        {
            if( EQUAL(sqlite3_column_name(hStmt,iFIDCol),
                      pszFIDColumn) )
                break;
        }

        if( iFIDCol == sqlite3_column_count(hStmt) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to find FID column '%s'.", 
                      pszFIDColumn );
            return NULL;
        }
        
        poFeature->SetFID( sqlite3_column_int( hStmt, iFIDCol ) );
    }
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;

    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Process Geometry if we have a column.                           */
/* -------------------------------------------------------------------- */
    if( osGeomColumn.size() )
    {
        int iGeomCol;

        for( iGeomCol = 0; iGeomCol < sqlite3_column_count(hStmt); iGeomCol++ )
        {
            if( EQUAL(sqlite3_column_name(hStmt,iGeomCol),
                      osGeomColumn) )
                break;
        }

        if( iGeomCol == sqlite3_column_count(hStmt) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to find Geometry column '%s'.", 
                      osGeomColumn.c_str() );
            return NULL;
        }

        OGRGeometry *poGeometry = NULL;
        if ( eGeomFormat == OSGF_WKT )
        {
            char *pszWKTCopy, *pszWKT = NULL;

            pszWKT = (char *) sqlite3_column_text( hStmt, iGeomCol );
            pszWKTCopy = pszWKT;
            if( OGRGeometryFactory::createFromWkt( 
                    &pszWKTCopy, NULL, &poGeometry ) == OGRERR_NONE )
                poFeature->SetGeometryDirectly( poGeometry );
        }
        else if ( eGeomFormat == OSGF_WKB )
        {
            const int nBytes = sqlite3_column_bytes( hStmt, iGeomCol );

            if( OGRGeometryFactory::createFromWkb( 
                    (GByte*)sqlite3_column_blob( hStmt, iGeomCol ),
                    NULL, &poGeometry, nBytes ) == OGRERR_NONE )
                poFeature->SetGeometryDirectly( poGeometry );
            else if (!bTriedAsSpatiaLite)
            {
                /* If the layer is the result of a sql select, we cannot be sure if it is */
                /* WKB or SpatialLite format */
                if( ImportSpatiaLiteGeometry( 
                    (GByte*)sqlite3_column_blob( hStmt, iGeomCol ), nBytes,
                    &poGeometry ) == OGRERR_NONE )
                {
                    poFeature->SetGeometryDirectly( poGeometry );
                    eGeomFormat = OSGF_SpatiaLite;
                }
                bTriedAsSpatiaLite = TRUE;
            }
        }
        else if ( eGeomFormat == OSGF_FGF )
        {
            const int nBytes = sqlite3_column_bytes( hStmt, iGeomCol );

            if( OGRGeometryFactory::createFromFgf( 
                    (GByte*)sqlite3_column_blob( hStmt, iGeomCol ),
                    NULL, &poGeometry, nBytes, NULL ) == OGRERR_NONE )
                poFeature->SetGeometryDirectly( poGeometry );
        }
        else if ( eGeomFormat == OSGF_SpatiaLite )
        {
            const int nBytes = sqlite3_column_bytes( hStmt, iGeomCol );

            if( ImportSpatiaLiteGeometry( 
                    (GByte*)sqlite3_column_blob( hStmt, iGeomCol ), nBytes,
                    &poGeometry ) == OGRERR_NONE )
                poFeature->SetGeometryDirectly( poGeometry );
        }

        if (poGeometry != NULL && poSRS != NULL)
            poGeometry->assignSpatialReference(poSRS);
    }

/* -------------------------------------------------------------------- */
/*      set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( iField );
        int iRawField = panFieldOrdinals[iField] - 1;

        if( sqlite3_column_type( hStmt, iRawField ) == SQLITE_NULL )
            continue;

        switch( poFieldDefn->GetType() )
        {
        case OFTInteger:
            poFeature->SetField( iField, 
                sqlite3_column_int( hStmt, iRawField ) );
            break;

        case OFTReal:
            poFeature->SetField( iField, 
                sqlite3_column_double( hStmt, iRawField ) );
            break;

        case OFTBinary:
            {
                const int nBytes = sqlite3_column_bytes( hStmt, iRawField );

                poFeature->SetField( iField, nBytes,
                    (GByte*)sqlite3_column_blob( hStmt, iRawField ) );
            }
            break;

        case OFTString:
            poFeature->SetField( iField, 
                (const char *) 
                sqlite3_column_text( hStmt, iRawField ) );
            break;

        default:
            break;
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteLayer::GetFeature( long nFeatureId )

{
    return OGRLayer::GetFeature( nFeatureId );
}


/************************************************************************/
/*                     createFromSpatialiteInternal()                   */
/************************************************************************/

/* See http://www.gaia-gis.it/spatialite/spatialite-manual-2.3.0.html#t3.3 */
/* for the specification of the spatialite BLOB geometry format */
/* Derived from WKB, but unfortunately it is not practical to reuse existing */
/* WKB encoding/decoding code */

#ifdef CPL_LSB
#define NEED_SWAP_SPATIALITE()  (eByteOrder != wkbNDR)
#else
#define NEED_SWAP_SPATIALITE()  (eByteOrder == wkbNDR)
#endif


OGRErr OGRSQLiteLayer::createFromSpatialiteInternal(const GByte *pabyData,
                                                    OGRGeometry **ppoReturn,
                                                    int nBytes,
                                                    OGRwkbByteOrder eByteOrder,
                                                    int* pnBytesConsumed)
{
    OGRErr      eErr = OGRERR_NONE;
    OGRGeometry *poGeom = NULL;
    GInt32       nGType;

    *ppoReturn = NULL;

    if (nBytes < 4)
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Decode the geometry type.                                       */
/* -------------------------------------------------------------------- */
    memcpy( &nGType, pabyData, 4 );
    if (NEED_SWAP_SPATIALITE())
        CPL_SWAP32PTR( &nGType );

    if( nGType < 1 || nGType > 7 )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    if( nGType == 1 )
    {
        double  adfTuple[2];

        if( nBytes < 4 + 2 * 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( adfTuple, pabyData + 4, 2*8 );
        if (NEED_SWAP_SPATIALITE())
        {
            CPL_SWAP64PTR( adfTuple );
            CPL_SWAP64PTR( adfTuple + 1 );
        }

        poGeom = new OGRPoint( adfTuple[0], adfTuple[1] );

        if( pnBytesConsumed )
            *pnBytesConsumed = 4 + 2 * 8;
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    else if( nGType == 2 )
    {
        double adfTuple[2];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        if( nPointCount < 0 || nPointCount > INT_MAX / (2 * 8))
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < 2 * 8 * nPointCount )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            memcpy( adfTuple, pabyData + 8 + 2*8*iPoint, 2*8 );
            if (NEED_SWAP_SPATIALITE())
            {
                CPL_SWAP64PTR( adfTuple );
                CPL_SWAP64PTR( adfTuple + 1 );
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1] );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = 8 + 2 * 8 * nPointCount;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( nGType == 3 )
    {
        double adfTuple[2];
        GInt32 nPointCount;
        GInt32 nRingCount;
        int    iPoint, iRing;
        OGRLinearRing *poLR;
        OGRPolygon *poPoly;
        int    nNextByte;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nRingCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nRingCount );

        if (nRingCount < 0 || nRingCount > INT_MAX / 4)
            return OGRERR_CORRUPT_DATA;

        /* Each ring has a minimum of 4 bytes */
        if (nBytes - 8 < nRingCount * 4)
            return OGRERR_NOT_ENOUGH_DATA;

        nNextByte = 8;
        
        poGeom = poPoly = new OGRPolygon();

        for( iRing = 0; iRing < nRingCount; iRing++ )
        {
            if( nBytes - nNextByte < 4 )
                return OGRERR_NOT_ENOUGH_DATA;

            memcpy( &nPointCount, pabyData + nNextByte, 4 );
            if (NEED_SWAP_SPATIALITE())
                CPL_SWAP32PTR( &nPointCount );

            if (nPointCount < 0 || nPointCount > INT_MAX / (2 * 8))
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;

            if( nBytes - nNextByte < 2 * 8 * nPointCount )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                memcpy( adfTuple, pabyData + nNextByte, 2*8 );
                nNextByte += 2 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1] );
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      GeometryCollections of various kinds.                           */
/* -------------------------------------------------------------------- */
    else if( nGType == 4         // MultiPoint
             || nGType == 5      // MultiLineString
             || nGType == 6      // MultiPolygon
             || nGType == 7 )    // MultiGeometry
    {
        OGRGeometryCollection *poGC = NULL;
        GInt32 nGeomCount = 0;
        int iGeom = 0;
        int nBytesUsed = 0;

        if( nGType == 4 )
            poGC = new OGRMultiPoint();
        else if( nGType == 5 )
            poGC = new OGRMultiLineString();
        else if( nGType == 6 )
            poGC = new OGRMultiPolygon();
        else if( nGType == 7 )
            poGC = new OGRGeometryCollection();

        assert(NULL != poGC);

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nGeomCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nGeomCount );

        if (nGeomCount < 0 || nGeomCount > INT_MAX / 9)
            return OGRERR_CORRUPT_DATA;

        /* Each sub geometry takes at least 9 bytes */
        if (nBytes - 8 < nGeomCount * 9)
            return OGRERR_NOT_ENOUGH_DATA;

        nBytesUsed = 8;

        for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
        {
            int nThisGeomSize;
            OGRGeometry *poThisGeom = NULL;

            if (nBytes - nBytesUsed < 5)
                return OGRERR_NOT_ENOUGH_DATA;

            if (pabyData[nBytesUsed] != 0x69)
                return OGRERR_CORRUPT_DATA;

            nBytesUsed ++;

            eErr = createFromSpatialiteInternal( pabyData + nBytesUsed,
                                                 &poThisGeom, nBytes - nBytesUsed,
                                                 eByteOrder, &nThisGeomSize);
            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }

            nBytesUsed += nThisGeomSize;
            eErr = poGC->addGeometryDirectly( poThisGeom );
            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }
        }

        poGeom = poGC;
        if( pnBytesConsumed )
            *pnBytesConsumed = nBytesUsed;
    }

/* -------------------------------------------------------------------- */
/*      Currently unsupported geometry.                                 */
/* -------------------------------------------------------------------- */
    else
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        *ppoReturn = poGeom;
    }
    else
    {
        delete poGeom;
    }

    return eErr;
}

/************************************************************************/
/*                      ImportSpatiaLiteGeometry()                      */
/************************************************************************/

OGRErr OGRSQLiteLayer::ImportSpatiaLiteGeometry( const GByte *pabyData,
                                                 int nBytes,
                                                 OGRGeometry **ppoGeometry )

{
    OGRwkbByteOrder eByteOrder;

    *ppoGeometry = NULL;

    if( nBytes < 44
        || pabyData[0] != 0
        || pabyData[38] != 0x7C
        || pabyData[nBytes-1] != 0xFE )
        return OGRERR_CORRUPT_DATA;

    eByteOrder = (OGRwkbByteOrder) pabyData[1];

    return createFromSpatialiteInternal(pabyData + 39, ppoGeometry,
                                        nBytes - 39, eByteOrder, NULL);
}

/************************************************************************/
/*                  ComputeSpatiaLiteGeometrySize()                     */
/************************************************************************/

int OGRSQLiteLayer::ComputeSpatiaLiteGeometrySize(const OGRGeometry *poGeometry)
{
    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbPoint:
            return 16;

        case wkbLineString:
        case wkbLinearRing:
            return 4 + 16 * ((OGRLineString*)poGeometry)->getNumPoints();

        case wkbPolygon:
        {
            int nSize = 4;
            OGRPolygon* poPoly = (OGRPolygon*) poGeometry;
            if (poPoly->getExteriorRing() != NULL)
            {
                nSize += ComputeSpatiaLiteGeometrySize(poPoly->getExteriorRing());

                int nInteriorRingCount = poPoly->getNumInteriorRings();
                for(int i=0;i<nInteriorRingCount;i++)
                    nSize += ComputeSpatiaLiteGeometrySize(poPoly->getInteriorRing(i));
            }
            return nSize;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int nSize = 4;
            OGRGeometryCollection* poGeomCollection = (OGRGeometryCollection*) poGeometry;
            int nParts = poGeomCollection->getNumGeometries();
            for(int i=0;i<nParts;i++)
                nSize += 5 + ComputeSpatiaLiteGeometrySize(poGeomCollection->getGeometryRef(i));
            return nSize;
        }

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected geometry type");
            return 0;
    }
}

/************************************************************************/
/*                      ExportSpatiaLiteGeometry()                      */
/************************************************************************/

int OGRSQLiteLayer::ExportSpatiaLiteGeometryInternal(const OGRGeometry *poGeometry,
                                                     OGRwkbByteOrder eByteOrder,
                                                     GByte* pabyData)
{
    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbPoint:
        {
            OGRPoint* poPoint = (OGRPoint*) poGeometry;
            double x = poPoint->getX();
            double y = poPoint->getY();
            memcpy(pabyData, &x, 8);
            memcpy(pabyData + 8, &y, 8);
            if (NEED_SWAP_SPATIALITE())
            {
                CPL_SWAP64PTR( pabyData );
                CPL_SWAP64PTR( pabyData + 8 );
            }
            return 16;
        }

        case wkbLineString:
        case wkbLinearRing:
        {
            OGRLineString* poLineString = (OGRLineString*) poGeometry;
            int nTotalSize = 4;
            int nPointCount = poLineString->getNumPoints();
            memcpy(pabyData, &nPointCount, 4);
            if (NEED_SWAP_SPATIALITE())
                CPL_SWAP32PTR( pabyData );
            for(int i=0;i<nPointCount;i++)
            {
                double x = poLineString->getX(i);
                double y = poLineString->getY(i);
                memcpy(pabyData + nTotalSize, &x, 8);
                memcpy(pabyData + nTotalSize + 8, &y, 8);
                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( pabyData + nTotalSize );
                    CPL_SWAP64PTR( pabyData + nTotalSize + 8 );
                }
                nTotalSize += 16;
            }
            return nTotalSize;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = (OGRPolygon*) poGeometry;
            int nParts = 0;
            int nTotalSize = 4;
            if (poPoly->getExteriorRing() != NULL)
            {
                int nInteriorRingCount = poPoly->getNumInteriorRings();
                nParts = 1 + nInteriorRingCount;
                memcpy(pabyData, &nParts, 4);
                if (NEED_SWAP_SPATIALITE())
                    CPL_SWAP32PTR( pabyData );

                nTotalSize += ExportSpatiaLiteGeometryInternal(poPoly->getExteriorRing(),
                                                              eByteOrder, pabyData + nTotalSize);

                for(int i=0;i<nInteriorRingCount;i++)
                {
                    nTotalSize += ExportSpatiaLiteGeometryInternal(poPoly->getInteriorRing(i),
                                                                   eByteOrder, pabyData + nTotalSize);
                }
            }
            else
            {
                memset(pabyData, 0, 4);
            }
            return nTotalSize;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGeomCollection = (OGRGeometryCollection*) poGeometry;
            int nTotalSize = 4;
            int nParts = poGeomCollection->getNumGeometries();
            memcpy(pabyData, &nParts, 4);
            if (NEED_SWAP_SPATIALITE())
                CPL_SWAP32PTR( pabyData );

            for(int i=0;i<nParts;i++)
            {
                pabyData[nTotalSize] = 0x69;
                nTotalSize ++;
                int nCode;
                switch (wkbFlatten(poGeomCollection->getGeometryRef(i)->getGeometryType()))
                {
                    case wkbPoint: nCode = 1; break;
                    case wkbLineString: nCode = 2; break;
                    case wkbPolygon: nCode = 3; break;
                    default: CPLError(CE_Failure, CPLE_AppDefined, "Unexpected geometry type"); return 0;
                }
                memcpy(pabyData + nTotalSize, &nCode, 4);
                if (NEED_SWAP_SPATIALITE())
                    CPL_SWAP32PTR( pabyData + nTotalSize );
                nTotalSize += 4;
                nTotalSize += ExportSpatiaLiteGeometryInternal(poGeomCollection->getGeometryRef(i),
                                                               eByteOrder, pabyData + nTotalSize);
            }
            return nTotalSize;
        }

        default:
            return 0;
    }
}


OGRErr OGRSQLiteLayer::ExportSpatiaLiteGeometry( const OGRGeometry *poGeometry,
                                                 GInt32 nSRID,
                                                 OGRwkbByteOrder eByteOrder,
                                                 GByte **ppabyData,
                                                 int *pnDataLenght )

{
    int     nDataLen = 44 + ComputeSpatiaLiteGeometrySize(poGeometry);
    OGREnvelope sEnvelope;

    *ppabyData =  (GByte *) CPLMalloc( nDataLen );

    (*ppabyData)[0] = 0x00;
    (*ppabyData)[1] = eByteOrder;

    // Write out SRID
    memcpy( *ppabyData + 2, &nSRID, 4 );

    // Write out the geometry bounding rectangle
    poGeometry->getEnvelope( &sEnvelope );
    memcpy( *ppabyData + 6, &sEnvelope.MinX, 8 );
    memcpy( *ppabyData + 14, &sEnvelope.MinY, 8 );
    memcpy( *ppabyData + 22, &sEnvelope.MaxX, 8 );
    memcpy( *ppabyData + 30, &sEnvelope.MaxY, 8 );

    (*ppabyData)[38] = 0x7C;

    int nCode = 0;
    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbPoint:
            nCode = 1;
            break;

        case wkbLineString:
        case wkbLinearRing:
            nCode = 2;
            break;

        case wkbPolygon:
            nCode = 3;
            break;

        case wkbMultiPoint:
            nCode = 4;
            break;

        case wkbMultiLineString:
            nCode = 5;
            break;

        case wkbMultiPolygon:
            nCode = 6;
            break;

        case wkbGeometryCollection:
            nCode = 7;
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected geometry type");
            CPLFree(*ppabyData);
            *ppabyData = NULL;
            *pnDataLenght = 0;
            return CE_Failure;
    }
    memcpy( *ppabyData + 39, &nCode, 4 );

    int nWritten = ExportSpatiaLiteGeometryInternal(poGeometry, eByteOrder, *ppabyData + 43);
    if (nWritten == 0)
    {
        CPLFree(*ppabyData);
        *ppabyData = NULL;
        *pnDataLenght = 0;
        return CE_Failure;
    }

    (*ppabyData)[nDataLen - 1] = 0xFE;

    if( NEED_SWAP_SPATIALITE() )
    {
        CPL_SWAP32PTR( *ppabyData + 2 );
        CPL_SWAP64PTR( *ppabyData + 6 );
        CPL_SWAP64PTR( *ppabyData + 14 );
        CPL_SWAP64PTR( *ppabyData + 22 );
        CPL_SWAP64PTR( *ppabyData + 30 );
        CPL_SWAP32PTR( *ppabyData + 39 );
    }

    *pnDataLenght = nDataLen;

    return CE_None;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRSQLiteLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRSQLiteLayer::StartTransaction()

{
    return poDS->SoftStartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRSQLiteLayer::CommitTransaction()

{
    return poDS->SoftCommit();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRSQLiteLayer::RollbackTransaction()

{
    return poDS->SoftRollback();
}
