/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteLayer class, code shared between 
 *           the direct table access, and the generic SQL results.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module supporting SpatiaLite's own 3D geometries
 * [XY, XYM, XYZ and XYZM] available since v.2.4.0
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
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
    bHasM = FALSE;
    bSpatialiteReadOnly = FALSE;
    bSpatialiteLoaded = FALSE;
    iSpatialiteVersion = -1;

    bIsVirtualShape = FALSE;

    bUseComprGeom = CSLTestBoolean(CPLGetConfigOption("COMPRESS_GEOM", "FALSE"));
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

        /* In the case of Spatialite VirtualShape, the PKUID */
        /* should be considered as a primary key */
        if( bIsVirtualShape && EQUAL(oField.GetNameRef(), "PKUID") )
        {
            CPLFree(pszFIDColumn);
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
        }

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
            else if (EQUAL(pszDeclType, "FLOAT") ||
                     EQUAL(pszDeclType, "DECIMAL"))
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
             || EQUALN(oField.GetNameRef(), "astext(", 7)
             || (EQUALN(oField.GetNameRef(), "st_", 3) && nColType == SQLITE_BLOB ) )
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
    ClearStatement();
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
    if( hStmt == NULL )
    {
        ResetStatement();
        if (hStmt == NULL)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    int rc;

    rc = sqlite3_step( hStmt );
    if( rc != SQLITE_ROW )
    {
        if ( rc != SQLITE_DONE )
        {
            sqlite3_reset(hStmt);
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
    if( osGeomColumn.size() && !poFeatureDefn->IsGeometryIgnored() )
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
        if ( poFieldDefn->IsIgnored() )
            continue;

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
    GInt32       compressedSize;

    *ppoReturn = NULL;

    if (nBytes < 4)
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Decode the geometry type.                                       */
/* -------------------------------------------------------------------- */
    memcpy( &nGType, pabyData, 4 );
    if (NEED_SWAP_SPATIALITE())
        CPL_SWAP32PTR( &nGType );

    if( ( nGType >= OGRSplitePointXY && 
          nGType <= OGRSpliteGeometryCollectionXY ) ||       // XY types
        ( nGType >= OGRSplitePointXYZ && 
          nGType <= OGRSpliteGeometryCollectionXYZ ) ||      // XYZ types
        ( nGType >= OGRSplitePointXYM && 
          nGType <= OGRSpliteGeometryCollectionXYM ) ||      // XYM types
        ( nGType >= OGRSplitePointXYZM && 
          nGType <= OGRSpliteGeometryCollectionXYZM ) ||     // XYZM types
        ( nGType >= OGRSpliteComprLineStringXY && 
          nGType <= OGRSpliteComprGeometryCollectionXY ) ||  // XY compressed
        ( nGType >= OGRSpliteComprLineStringXYZ && 
          nGType <= OGRSpliteComprGeometryCollectionXYZ ) || // XYZ compressed
        ( nGType >= OGRSpliteComprLineStringXYM && 
          nGType <= OGRSpliteComprGeometryCollectionXYM ) || // XYM compressed
        ( nGType >= OGRSpliteComprLineStringXYZM && 
          nGType <= OGRSpliteComprGeometryCollectionXYZM ) ) // XYZM compressed
        ;
    else
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Point [XY]                                                      */
/* -------------------------------------------------------------------- */
    if( nGType == OGRSplitePointXY )
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
/*      Point [XYZ]                                                     */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePointXYZ )
    {
        double  adfTuple[3];

        if( nBytes < 4 + 3 * 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( adfTuple, pabyData + 4, 3*8 );
        if (NEED_SWAP_SPATIALITE())
        {
            CPL_SWAP64PTR( adfTuple );
            CPL_SWAP64PTR( adfTuple + 1 );
            CPL_SWAP64PTR( adfTuple + 2 );
        }

        poGeom = new OGRPoint( adfTuple[0], adfTuple[1], adfTuple[2] );

        if( pnBytesConsumed )
            *pnBytesConsumed = 4 + 3 * 8;
    }

/* -------------------------------------------------------------------- */
/*      Point [XYM]                                                     */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePointXYM )
    {
        double  adfTuple[3];

        if( nBytes < 4 + 3 * 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( adfTuple, pabyData + 4, 3*8 );
        if (NEED_SWAP_SPATIALITE())
        {
            CPL_SWAP64PTR( adfTuple );
            CPL_SWAP64PTR( adfTuple + 1 );
            CPL_SWAP64PTR( adfTuple + 2 );
        }

        poGeom = new OGRPoint( adfTuple[0], adfTuple[1] );

        if( pnBytesConsumed )
            *pnBytesConsumed = 4 + 3 * 8;
    }

/* -------------------------------------------------------------------- */
/*      Point [XYZM]                                                    */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePointXYZM )
    {
        double  adfTuple[4];

        if( nBytes < 4 + 4 * 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( adfTuple, pabyData + 4, 4*8 );
        if (NEED_SWAP_SPATIALITE())
        {
            CPL_SWAP64PTR( adfTuple );
            CPL_SWAP64PTR( adfTuple + 1 );
            CPL_SWAP64PTR( adfTuple + 2 );
            CPL_SWAP64PTR( adfTuple + 3 );
        }

        poGeom = new OGRPoint( adfTuple[0], adfTuple[1], adfTuple[2] );

        if( pnBytesConsumed )
            *pnBytesConsumed = 4 + 4 * 8;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XY]                                                 */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteLineStringXY )
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
/*      LineString [XYZ]                                                */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteLineStringXYZ )
    {
        double adfTuple[3];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        if( nPointCount < 0 || nPointCount > INT_MAX / (3 * 8))
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < 3 * 8 * nPointCount )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            memcpy( adfTuple, pabyData + 8 + 3*8*iPoint, 3*8 );
            if (NEED_SWAP_SPATIALITE())
            {
                CPL_SWAP64PTR( adfTuple );
                CPL_SWAP64PTR( adfTuple + 1 );
                CPL_SWAP64PTR( adfTuple + 2 );
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = 8 + 3 * 8 * nPointCount;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XYM]                                                */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteLineStringXYM )
    {
        double adfTuple[3];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        if( nPointCount < 0 || nPointCount > INT_MAX / (3 * 8))
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < 3 * 8 * nPointCount )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            memcpy( adfTuple, pabyData + 8 + 3*8*iPoint, 3*8 );
            if (NEED_SWAP_SPATIALITE())
            {
                CPL_SWAP64PTR( adfTuple );
                CPL_SWAP64PTR( adfTuple + 1 );
                CPL_SWAP64PTR( adfTuple + 2 );
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1] );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = 8 + 3 * 8 * nPointCount;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XYZM]                                               */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteLineStringXYZM )
    {
        double adfTuple[4];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        if( nPointCount < 0 || nPointCount > INT_MAX / (4 * 8))
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < 4 * 8 * nPointCount )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            memcpy( adfTuple, pabyData + 8 + 4*8*iPoint, 4*8 );
            if (NEED_SWAP_SPATIALITE())
            {
                CPL_SWAP64PTR( adfTuple );
                CPL_SWAP64PTR( adfTuple + 1 );
                CPL_SWAP64PTR( adfTuple + 2 );
                CPL_SWAP64PTR( adfTuple + 3 );
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = 8 + 4 * 8 * nPointCount;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XY] Compressed                                      */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprLineStringXY )
    {
        double adfTuple[2];
        double adfTupleBase[2];
        float asfTuple[2];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;
        int    nNextByte;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        compressedSize = 16 * 2;                  // first and last Points 
        compressedSize += 8 * (nPointCount - 2);  // intermediate Points
        if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < compressedSize )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        nNextByte = 8;
        adfTupleBase[0] = 0.0;
        adfTupleBase[1] = 0.0;
		
        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
            {
                // first and last Points are uncompressed 
                memcpy( adfTuple, pabyData + nNextByte, 2*8 );
                nNextByte += 2 * 8;
			
                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                }
            }
            else
            {
                // any other intermediate Point is compressed
                memcpy( asfTuple, pabyData + nNextByte, 2*4 );
                nNextByte += 2 * 4;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP32PTR( asfTuple );
                    CPL_SWAP32PTR( asfTuple + 1 );
                }
                adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                adfTuple[1] = asfTuple[1] + adfTupleBase[1];
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1] );
            adfTupleBase[0] = adfTuple[0];
            adfTupleBase[1] = adfTuple[1];
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XYZ] Compressed                                     */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprLineStringXYZ )
    {
        double adfTuple[3];
        double adfTupleBase[3];
        float asfTuple[3];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;
        int    nNextByte;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        compressedSize = 24 * 2;                   // first and last Points 
        compressedSize += 12 * (nPointCount - 2);  // intermediate Points
        if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < compressedSize )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        nNextByte = 8;
        adfTupleBase[0] = 0.0;
        adfTupleBase[1] = 0.0;
        adfTupleBase[2] = 0.0;
		
        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
            {
                // first and last Points are uncompressed 
                memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                nNextByte += 3 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                    CPL_SWAP64PTR( adfTuple + 2 );
                }
            }
            else
            {
                // any other intermediate Point is compressed 
                memcpy( asfTuple, pabyData + nNextByte, 3*4 );
                nNextByte += 3 * 4;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP32PTR( asfTuple );
                    CPL_SWAP32PTR( asfTuple + 1 );
                    CPL_SWAP32PTR( asfTuple + 2 );
                }
                adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                adfTuple[2] = asfTuple[2] + adfTupleBase[2];
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
            adfTupleBase[0] = adfTuple[0];
            adfTupleBase[1] = adfTuple[1];
            adfTupleBase[2] = adfTuple[2];
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XYM] Compressed                                     */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprLineStringXYM )
    {
        double adfTuple[2];
        double adfTupleBase[2];
        float asfTuple[2];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;
        int    nNextByte;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        compressedSize = 24 * 2;                   // first and last Points 
        compressedSize += 16 * (nPointCount - 2);  // intermediate Points
        if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < compressedSize )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        nNextByte = 8;
        adfTupleBase[0] = 0.0;
        adfTupleBase[1] = 0.0;
		
        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
            {
                // first and last Points are uncompressed 
                memcpy( adfTuple, pabyData + nNextByte, 2*8 );
                nNextByte += 3 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                }
            }
            else
            {
                // any other intermediate Point is compressed
                memcpy( asfTuple, pabyData + nNextByte, 2*4 );
                nNextByte += 2 * 4 + 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP32PTR( asfTuple );
                    CPL_SWAP32PTR( asfTuple + 1 );
                }
                adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                adfTuple[1] = asfTuple[1] + adfTupleBase[1];
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1] );
            adfTupleBase[0] = adfTuple[0];
            adfTupleBase[1] = adfTuple[1];
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      LineString [XYZM] Compressed                                    */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprLineStringXYZM )
    {
        double adfTuple[3];
        double adfTupleBase[3];
        float asfTuple[3];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;
        int    nNextByte;

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nPointCount );

        compressedSize = 32 * 2;                   // first and last Points
        /* Note 20 is not an error : x,y,z are float and the m is a double */
        compressedSize += 20 * (nPointCount - 2);  // intermediate Points
        if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
            return OGRERR_CORRUPT_DATA;

        if (nBytes - 8 < compressedSize )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        nNextByte = 8;
        adfTupleBase[0] = 0.0;
        adfTupleBase[1] = 0.0;
        adfTupleBase[2] = 0.0;
		
        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
            {
                // first and last Points are uncompressed
                memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                nNextByte += 4 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                    CPL_SWAP64PTR( adfTuple + 2 );
                }
            }
            else
            {
                // any other intermediate Point is compressed
                memcpy( asfTuple, pabyData + nNextByte, 3*4 );
                nNextByte += 3 * 4 + 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP32PTR( asfTuple );
                    CPL_SWAP32PTR( asfTuple + 1 );
                    CPL_SWAP32PTR( asfTuple + 2 );
                }
                adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                adfTuple[2] = asfTuple[2] + adfTupleBase[2];
            }

            poLS->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
            adfTupleBase[0] = adfTuple[0];
            adfTupleBase[1] = adfTuple[1];
            adfTupleBase[2] = adfTuple[2];
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XY]                                                    */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePolygonXY )
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes 
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

            if( nPointCount < 0 || nPointCount > INT_MAX / (2 * 8))
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
/*      Polygon [XYZ]                                                   */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePolygonXYZ )
    {
        double adfTuple[3];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes
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

            if( nPointCount < 0 || nPointCount > INT_MAX / (3 * 8))
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;

            if( nBytes - nNextByte < 3 * 8 * nPointCount )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                nNextByte += 3 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                    CPL_SWAP64PTR( adfTuple + 2 );
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XYM]                                                   */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePolygonXYM )
    {
        double adfTuple[3];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes 
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

            if( nPointCount < 0 || nPointCount > INT_MAX / (3 * 8))
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;

            if( nBytes - nNextByte < 3 * 8 * nPointCount )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                nNextByte += 3 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                    CPL_SWAP64PTR( adfTuple + 2 );
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1] );
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XYZM]                                                  */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSplitePolygonXYZM )
    {
        double adfTuple[4];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes 
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

            if( nPointCount < 0 || nPointCount > INT_MAX / (4 * 8))
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;

            if( nBytes - nNextByte < 4 * 8 * nPointCount )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                memcpy( adfTuple, pabyData + nNextByte, 4*8 );
                nNextByte += 4 * 8;

                if (NEED_SWAP_SPATIALITE())
                {
                    CPL_SWAP64PTR( adfTuple );
                    CPL_SWAP64PTR( adfTuple + 1 );
                    CPL_SWAP64PTR( adfTuple + 2 );
                    CPL_SWAP64PTR( adfTuple + 3 );
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XY] Compressed                                         */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprPolygonXY  )
    {
        double adfTuple[2];
        double adfTupleBase[2];
        float asfTuple[2];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes
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

            compressedSize = 16 * 2;                  // first and last Points
            compressedSize += 8 * (nPointCount - 2);  // intermediate Points
            if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;
            adfTupleBase[0] = 0.0;
            adfTupleBase[1] = 0.0;

            if (nBytes - nNextByte < compressedSize )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
                {
                    // first and last Points are uncompressed 
                    memcpy( adfTuple, pabyData + nNextByte, 2*8 );
                    nNextByte += 2 * 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP64PTR( adfTuple );
                        CPL_SWAP64PTR( adfTuple + 1 );
                    }
                }
                else
                {
                    // any other intermediate Point is compressed
                    memcpy( asfTuple, pabyData + nNextByte, 2*4 );
                    nNextByte += 2 * 4;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP32PTR( asfTuple );
                        CPL_SWAP32PTR( asfTuple + 1 );
                    }
                    adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                    adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1] );
                adfTupleBase[0] = adfTuple[0];
                adfTupleBase[1] = adfTuple[1];
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XYZ] Compressed                                        */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprPolygonXYZ )
    {
        double adfTuple[3];
        double adfTupleBase[3];
        float asfTuple[3];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes
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

            compressedSize = 24 * 2;                  	// first and last Points
            compressedSize += 12 * (nPointCount - 2);  // intermediate Points
            if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;
            adfTupleBase[0] = 0.0;
            adfTupleBase[1] = 0.0;
            adfTupleBase[2] = 0.0;

            if (nBytes - nNextByte < compressedSize )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
                {
                    // first and last Points are uncompressed 
                    memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                    nNextByte += 3 * 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP64PTR( adfTuple );
                        CPL_SWAP64PTR( adfTuple + 1 );
                        CPL_SWAP64PTR( adfTuple + 2 );
                    }
                }
                else
                {
                    // any other intermediate Point is compressed
                    memcpy( asfTuple, pabyData + nNextByte, 3*4 );
                    nNextByte += 3 * 4;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP32PTR( asfTuple );
                        CPL_SWAP32PTR( asfTuple + 1 );
                        CPL_SWAP32PTR( asfTuple + 2 );
                    }
                    adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                    adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                    adfTuple[2] = asfTuple[2] + adfTupleBase[2];
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
                adfTupleBase[0] = adfTuple[0];
                adfTupleBase[1] = adfTuple[1];
                adfTupleBase[2] = adfTuple[2];
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XYM] Compressed                                        */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprPolygonXYM )
    {
        double adfTuple[2];
        double adfTupleBase[3];
        float asfTuple[2];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes
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

            compressedSize = 24 * 2;                   // first and last Points
            compressedSize += 16 * (nPointCount - 2);  // intermediate Points
            if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;
			adfTupleBase[0] = 0.0;
			adfTupleBase[1] = 0.0;

            if (nBytes - nNextByte < compressedSize )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
                {
                    // first and last Points are uncompressed 
                    memcpy( adfTuple, pabyData + nNextByte, 2*8 );
                    nNextByte += 2 * 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP64PTR( adfTuple );
                        CPL_SWAP64PTR( adfTuple + 1 );
                    }
                }
                else
                {
                    // any other intermediate Point is compressed
                    memcpy( asfTuple, pabyData + nNextByte, 2*4 );
                    nNextByte += 2 * 4 + 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP32PTR( asfTuple );
                        CPL_SWAP32PTR( asfTuple + 1 );
                    }
                    adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                    adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1] );
                adfTupleBase[0] = adfTuple[0];
                adfTupleBase[1] = adfTuple[1];
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      Polygon [XYZM] Compressed                                       */
/* -------------------------------------------------------------------- */
    else if( nGType == OGRSpliteComprPolygonXYZM )
    {
        double adfTuple[3];
        double adfTupleBase[3];
        float asfTuple[3];
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

        if (nRingCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each ring has a minimum of 4 bytes 
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

            compressedSize = 32 * 2;                   // first and last Points
            /* Note 20 is not an error : x,y,z are float and the m is a double */
            compressedSize += 20 * (nPointCount - 2);  // intermediate Points
            if( nPointCount < 0 || nPointCount > INT_MAX / compressedSize)
                return OGRERR_CORRUPT_DATA;

            nNextByte += 4;
            adfTupleBase[0] = 0.0;
            adfTupleBase[1] = 0.0;
            adfTupleBase[2] = 0.0;

            if (nBytes - nNextByte < compressedSize )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                if ( iPoint == 0 || iPoint == (nPointCount - 1 ) )
                {
                    // first and last Points are uncompressed
                    memcpy( adfTuple, pabyData + nNextByte, 3*8 );
                    nNextByte += 4 * 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP64PTR( adfTuple );
                        CPL_SWAP64PTR( adfTuple + 1 );
                        CPL_SWAP64PTR( adfTuple + 2 );
                    }
                }
                else
                {
                    // any other intermediate Point is compressed 
                    memcpy( asfTuple, pabyData + nNextByte, 3*4 );
                    nNextByte += 3 * 4 + 8;

                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP32PTR( asfTuple );
                        CPL_SWAP32PTR( asfTuple + 1 );
                        CPL_SWAP32PTR( asfTuple + 2 );
                    }
                    adfTuple[0] = asfTuple[0] + adfTupleBase[0];
                    adfTuple[1] = asfTuple[1] + adfTupleBase[1];
                    adfTuple[2] = asfTuple[2] + adfTupleBase[2];
                }

                poLR->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
                adfTupleBase[0] = adfTuple[0];
                adfTupleBase[1] = adfTuple[1];
                adfTupleBase[2] = adfTuple[2];
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      GeometryCollections of various kinds.                           */
/* -------------------------------------------------------------------- */
    else if( ( nGType >= OGRSpliteMultiPointXY &&
               nGType <= OGRSpliteGeometryCollectionXY ) ||       // XY types
             ( nGType >= OGRSpliteMultiPointXYZ &&
               nGType <= OGRSpliteGeometryCollectionXYZ ) ||      // XYZ types
             ( nGType >= OGRSpliteMultiPointXYM &&
               nGType <= OGRSpliteGeometryCollectionXYM ) ||      // XYM types 
             ( nGType >= OGRSpliteMultiPointXYZM &&
               nGType <= OGRSpliteGeometryCollectionXYZM ) ||     // XYZM types
             ( nGType >= OGRSpliteComprMultiLineStringXY &&
               nGType <= OGRSpliteComprGeometryCollectionXY ) ||  // XY compressed
             ( nGType >= OGRSpliteComprMultiLineStringXYZ &&
               nGType <= OGRSpliteComprGeometryCollectionXYZ ) || // XYZ compressed
             ( nGType >= OGRSpliteComprMultiLineStringXYM &&
               nGType <= OGRSpliteComprGeometryCollectionXYM ) || // XYM compressed
             ( nGType >= OGRSpliteComprMultiLineStringXYZM &&
               nGType <= OGRSpliteComprGeometryCollectionXYZM ) ) // XYZM compressed
    {
        OGRGeometryCollection *poGC = NULL;
        GInt32 nGeomCount = 0;
        int iGeom = 0;
        int nBytesUsed = 0;

        switch ( nGType )
        {
            case OGRSpliteMultiPointXY:
            case OGRSpliteMultiPointXYZ: 
            case OGRSpliteMultiPointXYM: 
            case OGRSpliteMultiPointXYZM:
                poGC = new OGRMultiPoint();
                break;
            case OGRSpliteMultiLineStringXY: 
            case OGRSpliteMultiLineStringXYZ: 
            case OGRSpliteMultiLineStringXYM: 
            case OGRSpliteMultiLineStringXYZM:
            case OGRSpliteComprMultiLineStringXY: 
            case OGRSpliteComprMultiLineStringXYZ: 
            case OGRSpliteComprMultiLineStringXYM: 
            case OGRSpliteComprMultiLineStringXYZM:
                poGC = new OGRMultiLineString();
                break;
            case OGRSpliteMultiPolygonXY: 
            case OGRSpliteMultiPolygonXYZ: 
            case OGRSpliteMultiPolygonXYM: 
            case OGRSpliteMultiPolygonXYZM:
            case OGRSpliteComprMultiPolygonXY: 
            case OGRSpliteComprMultiPolygonXYZ: 
            case OGRSpliteComprMultiPolygonXYM: 
            case OGRSpliteComprMultiPolygonXYZM:
                poGC = new OGRMultiPolygon();
                break;
            case OGRSpliteGeometryCollectionXY: 
            case OGRSpliteGeometryCollectionXYZ: 
            case OGRSpliteGeometryCollectionXYM: 
            case OGRSpliteGeometryCollectionXYZM:
            case OGRSpliteComprGeometryCollectionXY: 
            case OGRSpliteComprGeometryCollectionXYZ: 
            case OGRSpliteComprGeometryCollectionXYM: 
            case OGRSpliteComprGeometryCollectionXYZM:
                poGC = new OGRGeometryCollection();
                break;
        }

        assert(NULL != poGC);

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nGeomCount, pabyData + 4, 4 );
        if (NEED_SWAP_SPATIALITE())
            CPL_SWAP32PTR( &nGeomCount );

        if (nGeomCount < 0)
            return OGRERR_CORRUPT_DATA;

        // Each sub geometry takes at least 9 bytes
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
/*                CanBeCompressedSpatialiteGeometry()                   */
/************************************************************************/

int OGRSQLiteLayer::CanBeCompressedSpatialiteGeometry(const OGRGeometry *poGeometry)
{
    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbLineString:
        case wkbLinearRing:
        {
            int nPoints = ((OGRLineString*)poGeometry)->getNumPoints();
            return nPoints >= 2;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = (OGRPolygon*) poGeometry;
            if (poPoly->getExteriorRing() != NULL)
            {
                if (!CanBeCompressedSpatialiteGeometry(poPoly->getExteriorRing()))
                    return FALSE;

                int nInteriorRingCount = poPoly->getNumInteriorRings();
                for(int i=0;i<nInteriorRingCount;i++)
                {
                    if (!CanBeCompressedSpatialiteGeometry(poPoly->getInteriorRing(i)))
                        return FALSE;
                }
            }
            return TRUE;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGeomCollection = (OGRGeometryCollection*) poGeometry;
            int nParts = poGeomCollection->getNumGeometries();
            for(int i=0;i<nParts;i++)
            {
                if (!CanBeCompressedSpatialiteGeometry(poGeomCollection->getGeometryRef(i)))
                    return FALSE;
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}

/************************************************************************/
/*                  ComputeSpatiaLiteGeometrySize()                     */
/************************************************************************/

int OGRSQLiteLayer::ComputeSpatiaLiteGeometrySize(const OGRGeometry *poGeometry,
                                                  int bHasM, int bSpatialite2D,
                                                  int bUseComprGeom)
{
    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbPoint:
            if ( bSpatialite2D == TRUE )
                return 16;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return 32;
                else
                    return 24;
            }
            else
            {
                if (bHasM == TRUE)
                    return 24;
                else
                    return 16;
            }

        case wkbLineString:
        case wkbLinearRing:
        {
            int nPoints = ((OGRLineString*)poGeometry)->getNumPoints();
            int nDimension;
            int nPointsDouble = nPoints;
            int nPointsFloat = 0;
            if ( bSpatialite2D == TRUE )
            {
                nDimension = 2;
                bHasM = FALSE;
            }
            else
            {
                if ( bUseComprGeom && nPoints >= 2 )
                {
                    nPointsDouble = 2;
                    nPointsFloat = nPoints - 2;
                }
                nDimension = poGeometry->getCoordinateDimension();
            }
            return 4 + nDimension * (8 * nPointsDouble + 4 * nPointsFloat) + ((bHasM) ? nPoints * 8 : 0);
        }

        case wkbPolygon:
        {
            int nSize = 4;
            OGRPolygon* poPoly = (OGRPolygon*) poGeometry;
            bUseComprGeom = bUseComprGeom && !bSpatialite2D && CanBeCompressedSpatialiteGeometry(poGeometry);
            if (poPoly->getExteriorRing() != NULL)
            {
                nSize += ComputeSpatiaLiteGeometrySize(poPoly->getExteriorRing(),
                                                       bHasM, bSpatialite2D, bUseComprGeom);

                int nInteriorRingCount = poPoly->getNumInteriorRings();
                for(int i=0;i<nInteriorRingCount;i++)
                    nSize += ComputeSpatiaLiteGeometrySize(poPoly->getInteriorRing(i),
                                                           bHasM, bSpatialite2D, bUseComprGeom );
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
                nSize += 5 + ComputeSpatiaLiteGeometrySize(poGeomCollection->getGeometryRef(i),
                                                           bHasM, bSpatialite2D, bUseComprGeom );
            return nSize;
        }

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected geometry type");
            return 0;
    }
}

/************************************************************************/
/*                    GetSpatialiteGeometryCode()                       */
/************************************************************************/

int OGRSQLiteLayer::GetSpatialiteGeometryCode(const OGRGeometry *poGeometry,
                                              int bHasM, int bSpatialite2D,
                                              int bUseComprGeom,
                                              int bAcceptMultiGeom)
{
    OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
    switch (eType)
    {
        case wkbPoint:
            if ( bSpatialite2D == TRUE )
                return OGRSplitePointXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return OGRSplitePointXYZM;
                else
                    return OGRSplitePointXYZ;
             }
             else
             {
                if (bHasM == TRUE)
                    return OGRSplitePointXYM;
                else
                    return OGRSplitePointXY;
            }
            break;

        case wkbLineString:
        case wkbLinearRing:
            if ( bSpatialite2D == TRUE )
                return OGRSpliteLineStringXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return (bUseComprGeom) ? OGRSpliteComprLineStringXYZM : OGRSpliteLineStringXYZM;
                else
                    return (bUseComprGeom) ? OGRSpliteComprLineStringXYZ : OGRSpliteLineStringXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return (bUseComprGeom) ? OGRSpliteComprLineStringXYM : OGRSpliteLineStringXYM;
                else
                    return (bUseComprGeom) ? OGRSpliteComprLineStringXY : OGRSpliteLineStringXY;
            }
            break;

        case wkbPolygon:
            if ( bSpatialite2D == TRUE )
                return OGRSplitePolygonXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return (bUseComprGeom) ? OGRSpliteComprPolygonXYZM : OGRSplitePolygonXYZM;
                else
                    return (bUseComprGeom) ? OGRSpliteComprPolygonXYZ : OGRSplitePolygonXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return (bUseComprGeom) ? OGRSpliteComprPolygonXYM : OGRSplitePolygonXYM;
                else
                    return (bUseComprGeom) ? OGRSpliteComprPolygonXY : OGRSplitePolygonXY;
            }
            break;

        default:
            break;
    }

    if (!bAcceptMultiGeom)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected geometry type");
        return 0;
    }

    switch (eType)
    {
        case wkbMultiPoint:
            if ( bSpatialite2D == TRUE )
                return OGRSpliteMultiPointXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return OGRSpliteMultiPointXYZM;
                else
                    return OGRSpliteMultiPointXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return OGRSpliteMultiPointXYM;
                else
                    return OGRSpliteMultiPointXY;
            }
            break;

        case wkbMultiLineString:
            if ( bSpatialite2D == TRUE )
                return OGRSpliteMultiLineStringXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiLineStringXYZM :*/ OGRSpliteMultiLineStringXYZM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiLineStringXYZ :*/ OGRSpliteMultiLineStringXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiLineStringXYM :*/ OGRSpliteMultiLineStringXYM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiLineStringXY :*/ OGRSpliteMultiLineStringXY;
            }
            break;

        case wkbMultiPolygon:
            if ( bSpatialite2D == TRUE )
                return OGRSpliteMultiPolygonXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiPolygonXYZM :*/ OGRSpliteMultiPolygonXYZM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiPolygonXYZ :*/ OGRSpliteMultiPolygonXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiPolygonXYM :*/ OGRSpliteMultiPolygonXYM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprMultiPolygonXY :*/ OGRSpliteMultiPolygonXY;
            }
            break;


        case wkbGeometryCollection:
            if ( bSpatialite2D == TRUE )
                return OGRSpliteGeometryCollectionXY;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprGeometryCollectionXYZM :*/ OGRSpliteGeometryCollectionXYZM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprGeometryCollectionXYZ :*/ OGRSpliteGeometryCollectionXYZ;
            }
            else
            {
                if (bHasM == TRUE)
                    return /*(bUseComprGeom) ? OGRSpliteComprGeometryCollectionXYM :*/ OGRSpliteGeometryCollectionXYM;
                else
                    return /*(bUseComprGeom) ? OGRSpliteComprGeometryCollectionXY :*/ OGRSpliteGeometryCollectionXY;
            }
            break;

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
                                                     int bHasM, int bSpatialite2D,
                                                     int bUseComprGeom,
                                                     GByte* pabyData )
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
            if ( bSpatialite2D == TRUE )
                return 16;
            else if (poGeometry->getCoordinateDimension() == 3)
            {
                double z = poPoint->getZ();
                memcpy(pabyData + 16, &z, 8);
                if (NEED_SWAP_SPATIALITE())
                    CPL_SWAP64PTR( pabyData + 16 );
                if (bHasM == TRUE)
                {
                    double m = 0.0;
                    memcpy(pabyData + 24, &m, 8);
                    if (NEED_SWAP_SPATIALITE())
                        CPL_SWAP64PTR( pabyData + 24 );
                    return 32;
                }
                else
                    return 24;
            }
            else
            {
                if (bHasM == TRUE)
                {
                    double m = 0.0;
                    memcpy(pabyData + 16, &m, 8);
                    if (NEED_SWAP_SPATIALITE())
                        CPL_SWAP64PTR( pabyData + 16 );
                    return 24;
                }
                else
                    return 16;
            }
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

                if (!bUseComprGeom || i == 0 || i == nPointCount - 1)
                {
                    memcpy(pabyData + nTotalSize, &x, 8);
                    memcpy(pabyData + nTotalSize + 8, &y, 8);
                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP64PTR( pabyData + nTotalSize );
                        CPL_SWAP64PTR( pabyData + nTotalSize + 8 );
                    }
                    if (poGeometry->getCoordinateDimension() == 3)
                    {
                        double z = poLineString->getZ(i);
                        memcpy(pabyData + nTotalSize + 16, &z, 8);
                        if (NEED_SWAP_SPATIALITE())
                            CPL_SWAP64PTR( pabyData + nTotalSize + 16 );
                        if (bHasM == TRUE)
                        {
                            double m = 0.0;
                            memcpy(pabyData + nTotalSize + 24, &m, 8);
                            if (NEED_SWAP_SPATIALITE())
                                CPL_SWAP64PTR( pabyData + nTotalSize + 24 );
                            nTotalSize += 32;
                        }
                        else
                            nTotalSize += 24;
                    }
                    else
                    {
                        if (bHasM == TRUE)
                        {
                            double m = 0.0;
                            memcpy(pabyData + nTotalSize + 16, &m, 8);
                            if (NEED_SWAP_SPATIALITE())
                                CPL_SWAP64PTR( pabyData + nTotalSize + 16 );
                            nTotalSize += 24;
                        }
                        else
                            nTotalSize += 16;
                    }
                }
                else /* Compressed intermediate points */
                {
                    float deltax = (float)(x - poLineString->getX(i-1));
                    float deltay = (float)(y - poLineString->getY(i-1));
                    memcpy(pabyData + nTotalSize, &deltax, 4);
                    memcpy(pabyData + nTotalSize + 4, &deltay, 4);
                    if (NEED_SWAP_SPATIALITE())
                    {
                        CPL_SWAP32PTR( pabyData + nTotalSize );
                        CPL_SWAP32PTR( pabyData + nTotalSize + 4 );
                    }
                    if (poGeometry->getCoordinateDimension() == 3)
                    {
                        double z = poLineString->getZ(i);
                        float deltaz = (float)(z - poLineString->getZ(i-1));
                        memcpy(pabyData + nTotalSize + 8, &deltaz, 4);
                        if (NEED_SWAP_SPATIALITE())
                            CPL_SWAP32PTR( pabyData + nTotalSize + 8 );
                        if (bHasM == TRUE)
                        {
                            double m = 0.0;
                            memcpy(pabyData + nTotalSize + 12, &m, 8);
                            if (NEED_SWAP_SPATIALITE())
                                CPL_SWAP64PTR( pabyData + nTotalSize + 12 );
                            nTotalSize += 20;
                        }
                        else
                            nTotalSize += 12;
                    }
                    else
                    {
                        if (bHasM == TRUE)
                        {
                            double m = 0.0;
                            memcpy(pabyData + nTotalSize + 8, &m, 8);
                            if (NEED_SWAP_SPATIALITE())
                                CPL_SWAP64PTR( pabyData + nTotalSize + 8 );
                            nTotalSize += 16;
                        }
                        else
                            nTotalSize += 8;
                    }
                }
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
                                                              eByteOrder,
                                                              bHasM, bSpatialite2D,
                                                              bUseComprGeom,
                                                              pabyData + nTotalSize);

                for(int i=0;i<nInteriorRingCount;i++)
                {
                    nTotalSize += ExportSpatiaLiteGeometryInternal(poPoly->getInteriorRing(i),
                                                                   eByteOrder,
                                                                   bHasM, bSpatialite2D,
                                                                   bUseComprGeom,
                                                                   pabyData + nTotalSize);
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
                int nCode = GetSpatialiteGeometryCode(poGeomCollection->getGeometryRef(i),
                                                      bHasM, bSpatialite2D,
                                                      bUseComprGeom, FALSE);
                if (nCode == 0)
                    return 0;
                memcpy(pabyData + nTotalSize, &nCode, 4);
                if (NEED_SWAP_SPATIALITE())
                    CPL_SWAP32PTR( pabyData + nTotalSize );
                nTotalSize += 4;
                nTotalSize += ExportSpatiaLiteGeometryInternal(poGeomCollection->getGeometryRef(i),
                                                               eByteOrder,
                                                               bHasM, bSpatialite2D,
                                                               bUseComprGeom,
                                                               pabyData + nTotalSize);
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
                                                 int bHasM, int bSpatialite2D,
                                                 int bUseComprGeom,
                                                 GByte **ppabyData,
                                                 int *pnDataLenght )

{

    bUseComprGeom = bUseComprGeom && !bSpatialite2D && CanBeCompressedSpatialiteGeometry(poGeometry);

    int     nDataLen = 44 + ComputeSpatiaLiteGeometrySize( poGeometry,
                                                           bHasM, 
                                                           bSpatialite2D,
                                                           bUseComprGeom );
    OGREnvelope sEnvelope;

    *ppabyData =  (GByte *) CPLMalloc( nDataLen );

    (*ppabyData)[0] = 0x00;
    (*ppabyData)[1] = (GByte) eByteOrder;

    // Write out SRID
    memcpy( *ppabyData + 2, &nSRID, 4 );

    // Write out the geometry bounding rectangle
    poGeometry->getEnvelope( &sEnvelope );
    memcpy( *ppabyData + 6, &sEnvelope.MinX, 8 );
    memcpy( *ppabyData + 14, &sEnvelope.MinY, 8 );
    memcpy( *ppabyData + 22, &sEnvelope.MaxX, 8 );
    memcpy( *ppabyData + 30, &sEnvelope.MaxY, 8 );

    (*ppabyData)[38] = 0x7C;

    int nCode = GetSpatialiteGeometryCode(poGeometry,
                                          bHasM, bSpatialite2D,
                                          bUseComprGeom, TRUE);
    if (nCode == 0)
    {
        CPLFree(*ppabyData);
        *ppabyData = NULL;
        *pnDataLenght = 0;
        return CE_Failure;
    }
    memcpy( *ppabyData + 39, &nCode, 4 );

    int nWritten = ExportSpatiaLiteGeometryInternal(poGeometry, 
                                                    eByteOrder, 
                                                    bHasM, bSpatialite2D,
                                                    bUseComprGeom,
                                                    *ppabyData + 43);
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

    else if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE; 

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

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRSQLiteLayer::ClearStatement()

{
    if( hStmt != NULL )
    {
        CPLDebug( "OGR_SQLITE", "finalize %p", hStmt );
        sqlite3_finalize( hStmt );
        hStmt = NULL;
    }
}
