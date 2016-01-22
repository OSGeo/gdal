/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeomediaTableLayer class, access to an existing table.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_geomedia.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRGeomediaTableLayer()                     */
/************************************************************************/

OGRGeomediaTableLayer::OGRGeomediaTableLayer( OGRGeomediaDataSource *poDSIn )

{
    poDS = poDSIn;
    pszQuery = NULL;
    bUpdateAccess = TRUE;
    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;
}

/************************************************************************/
/*                       ~OGRGeomediaTableLayer()                       */
/************************************************************************/

OGRGeomediaTableLayer::~OGRGeomediaTableLayer()

{
    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRGeomediaTableLayer::Initialize( const char *pszTableName,
                                          const char *pszGeomCol,
                                          OGRSpatialReference* poSRS )


{
    CPLODBCSession *poSession = poDS->GetSession();

    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    this->poSRS = poSRS;

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    {
    CPLODBCStatement oGetKey( poSession );
    
    if( oGetKey.GetPrimaryKeys( pszTableName ) && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));
        
        if( oGetKey.Fetch() ) // more than one field in key! 
        {
            CPLFree( pszFIDColumn );
            pszFIDColumn = NULL;
            CPLDebug( "Geomedia", "%s: Compound primary key, ignoring.",
                      pszTableName );
        }
        else
            CPLDebug( "Geomedia",
                      "%s: Got primary key %s.",
                      pszTableName, pszFIDColumn );
    }
    else
        CPLDebug( "Geomedia", "%s: no primary key", pszTableName );
    }
/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GetColumns() failed on %s.\n%s",
                  pszTableName, poSession->GetLastError() );
        return CE_Failure;
    }

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

    return CE_None;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRGeomediaTableLayer::ClearStatement()

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

CPLODBCStatement *OGRGeomediaTableLayer::GetStatement()

{
    if( poStmt == NULL )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeomediaTableLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    if( pszQuery != NULL )
        poStmt->Appendf( " WHERE %s", pszQuery );

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

void OGRGeomediaTableLayer::ResetReading()

{
    ClearStatement();
    OGRGeomediaLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGeomediaTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRGeomediaLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( poFeatureDefn->GetName() );
    poStmt->Appendf( " WHERE %s = %ld", pszFIDColumn, nFeatureId );

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

OGRErr OGRGeomediaTableLayer::SetAttributeFilter( const char *pszQuery )

{
    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL 
            && EQUAL(pszQuery,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    this->pszQuery = pszQuery ? CPLStrdup( pszQuery ) : NULL; 

    ClearStatement();

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeomediaTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return OGRGeomediaLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRGeomediaTableLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL )
        return OGRGeomediaLayer::GetFeatureCount( bForce );

    CPLODBCStatement oStmt( poDS->GetSession() );
    oStmt.Append( "SELECT COUNT(*) FROM " );
    oStmt.Append( poFeatureDefn->GetName() );

    if( pszQuery != NULL )
        oStmt.Appendf( " WHERE %s", pszQuery );

    if( !oStmt.ExecuteSQL() || !oStmt.Fetch() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GetFeatureCount() failed on query %s.\n%s",
                  oStmt.GetCommand(), poDS->GetSession()->GetLastError() );
        return OGRGeomediaLayer::GetFeatureCount(bForce);
    }

    return atoi(oStmt.GetColData(0));
}
