/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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
 * Revision 1.10  2005/11/18 14:40:10  fwarmerdam
 * More debug info.
 *
 * Revision 1.9  2005/11/18 14:36:11  fwarmerdam
 * More debug info on FID.
 *
 * Revision 1.8  2005/11/18 14:32:20  fwarmerdam
 * added debug message about fid.
 *
 * Revision 1.7  2005/11/02 21:58:26  fwarmerdam
 * preliminary support for ODBC spatial querying
 *
 * Revision 1.6  2005/09/09 05:02:12  fwarmerdam
 * added wkb support
 *
 * Revision 1.5  2005/02/22 12:53:56  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.4  2003/12/29 22:48:45  warmerda
 * added error check on whether any columns found
 *
 * Revision 1.3  2003/10/29 17:47:38  warmerda
 * Added FIDcolumn (based on primary key) support
 *
 * Revision 1.2  2003/09/26 18:22:41  warmerda
 * Fixed SetAttributeFilter().
 *
 * Revision 1.1  2003/09/25 17:08:37  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_odbc.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                          OGRODBCTableLayer()                         */
/************************************************************************/

OGRODBCTableLayer::OGRODBCTableLayer( OGRODBCDataSource *poDSIn )

{
    poDS = poDSIn;

    pszQuery = NULL;

    bUpdateAccess = TRUE;
    bHaveSpatialExtents = FALSE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = NULL;
}

/************************************************************************/
/*                          ~OGRODBCTableLayer()                          */
/************************************************************************/

OGRODBCTableLayer::~OGRODBCTableLayer()

{
    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRODBCTableLayer::Initialize( const char *pszTableName, 
                                      const char *pszGeomCol )

{
    CPLODBCSession *poSession = poDS->GetSession();

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );
    
    if( oGetKey.GetPrimaryKeys( pszTableName ) && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));
        
        if( oGetKey.Fetch() ) // more than one field in key! 
        {
            CPLFree( pszFIDColumn );
            pszFIDColumn = NULL;

            CPLDebug( "OGR_ODBC", "Table %s has multiple primary key fields, ignoring them all.", 
                      pszTableName );
        }
    }

    if( pszFIDColumn != NULL )
        CPLDebug( "OGR_ODBC", "Using column %s as FID for table %s.",
                  pszFIDColumn, pszTableName );
    else
        CPLDebug( "OGR_ODBC", "Table %s has no identified FID column.",
                  pszTableName );

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName ) )
        return CE_Failure;

    eErr = BuildFeatureDefn( pszTableName, &oGetCol );
    if( eErr != CE_None )
        return eErr;

    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No column definitions found for table '%s', layer not usable.", 
                  pszTableName );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Do we have XMIN, YMIN, XMAX, YMAX extent fields?                */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetFieldIndex( "XMIN" ) != -1 
        && poFeatureDefn->GetFieldIndex( "XMAX" ) != -1 
        && poFeatureDefn->GetFieldIndex( "YMIN" ) != -1 
        && poFeatureDefn->GetFieldIndex( "YMAX" ) != -1 )
    {
        bHaveSpatialExtents = TRUE;
        CPLDebug( "OGR_ODBC", "Table %s has geometry extent fields.",
                  pszTableName );
    }
        
/* -------------------------------------------------------------------- */
/*      If we got a geometry column, does it exist?  Is it binary?      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iColumn = oGetCol.GetColId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Column %s requested for geometry, but it does not exist.", 
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = NULL;
        }
        else
        {
            if( oGetCol.GetColType( iColumn ) == SQL_BINARY
                || oGetCol.GetColType( iColumn ) == SQL_VARBINARY
                || oGetCol.GetColType( iColumn ) == SQL_LONGVARBINARY )
                bGeomColumnWKB = TRUE;
        }
    }


    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRODBCTableLayer::ClearStatement()

{
    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRODBCTableLayer::GetStatement()

{
    if( poStmt == NULL )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRODBCTableLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );

    /* Append attribute query if we have it */
    if( pszQuery != NULL )
        poStmt->Appendf( " WHERE %s", pszQuery );

    /* If we have a spatial filter, and per record extents, query on it */
    if( m_poFilterGeom != NULL && bHaveSpatialExtents )
    {
        if( pszQuery == NULL )
            poStmt->Append( " WHERE" );
        else
            poStmt->Append( " AND" );
        
        poStmt->Appendf( " XMAX > %.8f AND XMIN < %.8f"
                         " AND YMAX > %.8f AND YMIN < %.8f", 
                         m_sFilterEnvelope.MinX, m_sFilterEnvelope.MaxX, 
                         m_sFilterEnvelope.MinY, m_sFilterEnvelope.MaxY );
    }

    CPLDebug( "OGR_ODBC", "ExecuteSQL(%s)", poStmt->GetCommand() );
    if( poStmt->ExecuteSQL() )
        return OGRERR_NONE;
    else
    {
        delete poStmt;
        poStmt = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRODBCTableLayer::ResetReading()

{
    ClearStatement();
    OGRODBCLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRODBCTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRODBCLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    poStmt->Appendf( " WHERE %s = %d", pszFIDColumn, nFeatureId );

    if( !poStmt->ExecuteSQL() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRODBCTableLayer::SetAttributeFilter( const char *pszQuery )

{
    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL 
            && EQUAL(pszQuery,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    this->pszQuery = CPLStrdup( pszQuery );

    ClearStatement();

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return OGRODBCLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRODBCTableLayer::GetFeatureCount( int bForce )

{
    return OGRODBCLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is -2 (meaning we           */
/*      haven't yet even looked for it).                                */
/************************************************************************/

OGRSpatialReference *OGRODBCTableLayer::GetSpatialRef()

{
#ifdef notdef
    if( nSRSId == -2 )
    {
        PGconn          *hPGConn = poDS->GetPGConn();
        PGresult        *hResult;
        char            szCommand[1024];

        nSRSId = -1;

        poDS->SoftStartTransaction();

        sprintf( szCommand, 
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s'",
                 poFeatureDefn->GetName() );
        hResult = PQexec(hPGConn, szCommand );

        if( hResult 
            && PQresultStatus(hResult) == PGRES_TUPLES_OK 
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }

        poDS->SoftCommit();
    }
#endif

    return OGRODBCLayer::GetSpatialRef();
}
