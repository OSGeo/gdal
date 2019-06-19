/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_odbc.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                          OGRODBCTableLayer()                         */
/************************************************************************/

OGRODBCTableLayer::OGRODBCTableLayer( OGRODBCDataSource *poDSIn ) :
    pszQuery(nullptr),
    bHaveSpatialExtents(FALSE),
    pszTableName(nullptr),
    pszSchemaName(nullptr)
{
    poDS = poDSIn;
    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = nullptr;
}

/************************************************************************/
/*                          ~OGRODBCTableLayer()                          */
/************************************************************************/

OGRODBCTableLayer::~OGRODBCTableLayer()

{
    CPLFree( pszTableName );
    CPLFree( pszSchemaName );

    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRODBCTableLayer::Initialize( const char *pszLayerName,
                                      const char *pszGeomCol )

{
    CPLODBCSession *poSession = poDS->GetSession();

    CPLFree( pszFIDColumn );
    pszFIDColumn = nullptr;

    SetDescription( pszLayerName );

/* -------------------------------------------------------------------- */
/*      Parse out schema name if present in layer.  We assume a         */
/*      schema is provided if there is a dot in the name, and that      */
/*      it is in the form <schema>.<tablename>                          */
/* -------------------------------------------------------------------- */
    const char *pszDot = strstr(pszLayerName,".");
    if( pszDot != nullptr )
    {
        pszTableName = CPLStrdup(pszDot + 1);
        pszSchemaName = CPLStrdup(pszLayerName);
        pszSchemaName[pszDot - pszLayerName] = '\0';
    }
    else
    {
        pszTableName = CPLStrdup(pszLayerName);
    }

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );

    if( oGetKey.GetPrimaryKeys( pszTableName, nullptr, pszSchemaName )
        && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));

        if( oGetKey.Fetch() ) // more than one field in key!
        {
            CPLFree( pszFIDColumn );
            pszFIDColumn = nullptr;

            CPLDebug( "OGR_ODBC", "Table %s has multiple primary key fields, "
                      "ignoring them all.", pszTableName );
        }
    }

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == nullptr )
        pszGeomColumn = nullptr;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName, nullptr, pszSchemaName ) )
        return CE_Failure;

    eErr = BuildFeatureDefn( pszLayerName, &oGetCol );
    if( eErr != CE_None )
        return eErr;

    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "No column definitions found for table '%s', layer not usable.",
                  pszLayerName );
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
                  pszLayerName );
    }

/* -------------------------------------------------------------------- */
/*      If we got a geometry column, does it exist?  Is it binary?      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != nullptr )
    {
        int iColumn = oGetCol.GetColId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column %s requested for geometry, but it does not exist.",
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = nullptr;
        }
        else
        {
            if( CPLODBCStatement::GetTypeMapping(
                    oGetCol.GetColType( iColumn )) == SQL_C_BINARY )
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
    if( poStmt != nullptr )
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRODBCTableLayer::GetStatement()

{
    if( poStmt == nullptr )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                      EscapeAndQuoteIdentifier()                      */
/************************************************************************/

static CPLString EscapeAndQuoteIdentifier(const CPLString& osStr)
{
    CPLString osRet; int num_dots = 0;
    for( size_t i = 0; i < osStr.size(); i++ )
    {
        if( osStr[i] == '"' )
        {
            osRet += "\\\"";
        }
        else if (osStr[i] == '.' && num_dots == 0){
            /* It's schema qualified, so first segment we assume is the schema and should be quoted separately */
            osRet += "\".\"";
            num_dots += 1;
        }
        else
        {
            osRet += osStr[i];
        }
    }
    return '"' + osRet + '"';
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
    poStmt->Append( EscapeAndQuoteIdentifier(poFeatureDefn->GetName()) );

    /* Append attribute query if we have it */
    if( pszQuery != nullptr )
        poStmt->Appendf( " WHERE %s", pszQuery );

    /* If we have a spatial filter, and per record extents, query on it */
    if( m_poFilterGeom != nullptr && bHaveSpatialExtents )
    {
        if( pszQuery == nullptr )
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
        poStmt = nullptr;
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

OGRFeature *OGRODBCTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == nullptr )
        return OGRODBCLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( "SELECT * FROM " );
    poStmt->Append( EscapeAndQuoteIdentifier(poFeatureDefn->GetName()) );
    poStmt->Appendf( " WHERE %s = " CPL_FRMT_GIB,
                     EscapeAndQuoteIdentifier(pszFIDColumn).c_str(),
                     nFeatureId );

    if( !poStmt->ExecuteSQL() )
    {
        delete poStmt;
        poStmt = nullptr;
        return nullptr;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRODBCTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQueryIn) ? CPLStrdup(pszQueryIn) : nullptr;

    if( (pszQueryIn == nullptr && pszQuery == nullptr)
        || (pszQueryIn != nullptr && pszQuery != nullptr
            && EQUAL(pszQueryIn, pszQuery)) )
        return OGRERR_NONE;

    CPLFree( pszQuery );
    pszQuery = pszQueryIn != nullptr ? CPLStrdup( pszQueryIn ) : nullptr;

    ClearStatement();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

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

GIntBig OGRODBCTableLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != nullptr )
        return OGRODBCLayer::GetFeatureCount( bForce );

    CPLODBCStatement oStmt( poDS->GetSession() );
    oStmt.Append( "SELECT COUNT(*) FROM " );
    oStmt.Append( EscapeAndQuoteIdentifier(poFeatureDefn->GetName()) );

    if( pszQuery != nullptr )
        oStmt.Appendf( " WHERE %s", pszQuery );

    if( !oStmt.ExecuteSQL() || !oStmt.Fetch() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GetFeatureCount() failed on query %s.\n%s",
                  oStmt.GetCommand(), poDS->GetSession()->GetLastError() );
        return OGRODBCLayer::GetFeatureCount(bForce);
    }

    return CPLAtoGIntBig(oStmt.GetColData(0));
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

        nSRSId = -1;

        poDS->SoftStartTransaction();

        char szCommand[1024] = {};
        sprintf( szCommand,
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s'",
                 poFeatureDefn->GetName() );
        PGresult *hResult = PQexec(hPGConn, szCommand );

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
