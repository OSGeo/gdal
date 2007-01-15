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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.9  2005/09/21 00:54:43  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.8  2005/02/22 12:50:31  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.7  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.6  2004/10/30 05:12:52  fwarmerdam
 * fixed memory leaks
 *
 * Revision 1.5  2004/08/20 21:43:12  warmerda
 * avoid doing alot of work in GetExtent() if we have no geometry
 *
 * Revision 1.4  2004/07/13 15:11:19  warmerda
 * implemented SetFeature, transaction support
 *
 * Revision 1.3  2004/07/12 20:50:46  warmerda
 * table/database creation now implemented
 *
 * Revision 1.2  2004/07/11 19:23:51  warmerda
 * read implementation working well
 *
 * Revision 1.1  2004/07/09 06:25:04  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_sqlite.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRSQLiteLayer()                           */
/************************************************************************/

OGRSQLiteLayer::OGRSQLiteLayer()

{
    poDS = NULL;

    pszGeomColumn = NULL;
    pszFIDColumn = NULL;

    hStmt = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 

    panFieldOrdinals = NULL;
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

    if( pszGeomColumn != NULL )
        CPLFree( pszGeomColumn );

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

        //oField.SetWidth( MAX(0,poStmt->GetColSize( iCol )) );

        if( pszGeomColumn != NULL 
            && EQUAL(oField.GetNameRef(),pszGeomColumn) )
            continue;

        // The geometry is for internal use, not a real column.
        if( EQUAL(oField.GetNameRef(),"wkt_geometry") )
        {
            if( pszGeomColumn == NULL )
                pszGeomColumn = CPLStrdup( oField.GetNameRef() );
            continue;
        }

        // The rowid is for internal use, not a real column.
        if( EQUAL(oField.GetNameRef(),"_rowid_") )
            continue;

        // The OGC_FID is for internal use, not a real user visible column.
        if( EQUAL(oField.GetNameRef(),"OGC_FID") )
            continue;

        switch( sqlite3_column_type( hStmt, iCol ) )
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
    if( pszGeomColumn == NULL )
        poFeatureDefn->SetGeomType( wkbNone );

    return CE_None;
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
        // we really should check for errors 
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
    if( pszGeomColumn != NULL )
    {
        int iGeomCol;

        for( iGeomCol = 0; iGeomCol < sqlite3_column_count(hStmt); iGeomCol++ )
        {
            if( EQUAL(sqlite3_column_name(hStmt,iGeomCol),
                      pszGeomColumn) )
                break;
        }

        if( iGeomCol == sqlite3_column_count(hStmt) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to find Geometry column '%s'.", 
                      pszGeomColumn );
            return NULL;
        }

        char *pszWKTCopy, *pszWKT = NULL;
        OGRGeometry *poGeometry = NULL;

        pszWKT = (char *) sqlite3_column_text( hStmt, iGeomCol );
        pszWKTCopy = pszWKT;
        if( OGRGeometryFactory::createFromWkt( &pszWKTCopy, NULL, 
                                               &poGeometry ) == OGRERR_NONE )
            poFeature->SetGeometryDirectly( poGeometry );
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

#ifdef notdef
          case OFTBinary:
          {
              int nBytes = sqlite3_column_bytes( hStmt, iRawField );

              poFeature->SetField( iField, 
                                   sqlite3_column_double( hStmt, iRawField ) );
          }
          break;
#endif

          case OFTString:
            poFeature->SetField( iField, 
                                 (const char *) 
                                 sqlite3_column_text( hStmt, iRawField ) );
            break;

          default:
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if( pszGeomColumn != NULL )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        const char *pszGeomText = poStmt->GetColData( iField );
        OGRGeometry *poGeom = NULL;

        if( pszGeomText != NULL )
            OGRGeometryFactory::createFromWkt( (char **) &pszGeomText,
                                               NULL, &poGeom );
        
        if( poGeom != NULL )
            poFeature->SetGeometryDirectly( poGeom );
    }
#endif

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
        return FALSE;

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
