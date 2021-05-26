/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDataSource class
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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

#include "ogr_idb.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                         OGRIDBDataSource()                          */
/************************************************************************/

OGRIDBDataSource::OGRIDBDataSource()

{
    pszName = nullptr;
    papoLayers = nullptr;
    nLayers = 0;
    poConn = nullptr;
    bDSUpdate = FALSE;
}

/************************************************************************/
/*                         ~OGRIDBDataSource()                         */
/************************************************************************/

OGRIDBDataSource::~OGRIDBDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    if (poConn != nullptr && poConn->IsOpen() )
    {
           poConn->Close();
           CPLDebug( "OGR_IDB",
              "Closing connection" );
    }

    delete poConn;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRIDBDataSource::Open( const char * pszNewName, int bUpdate,
                             int /* bTestOpen */ )

{
    CPLAssert( poConn == nullptr );

/* -------------------------------------------------------------------- */
/*  URL: DBI:dbname=.. server=.. user=.. pass=.. table=..               */
/*  Any of params is optional, table param can repeat more than once    */
/* -------------------------------------------------------------------- */
    char * pszDbName    = nullptr;
    char * pszServer    = nullptr;
    char * pszUser      = nullptr;
    char * pszPass      = nullptr;
    char **papszTables  = nullptr;
    char **papszGeomCol = nullptr;

    char ** papszTokens = CSLTokenizeString2(pszNewName + 4, " ", 0);
    char * pszToken = nullptr;
    int i = 0;

    while ( (pszToken = papszTokens[i++]) != nullptr )
    {
        if ( STARTS_WITH_CI(pszToken, "dbname=") )
            pszDbName = CPLStrdup( pszToken + 7 );
        else if ( STARTS_WITH_CI(pszToken, "server=") )
            pszServer = CPLStrdup( pszToken + 7 );
        else if ( STARTS_WITH_CI(pszToken, "user=") )
            pszUser = CPLStrdup( pszToken + 5 );
        else if ( STARTS_WITH_CI(pszToken, "pass=") )
            pszPass = CPLStrdup( pszToken + 5 );
        else if ( STARTS_WITH_CI(pszToken, "table=") )
        {
            papszTables = CSLAddString( papszTables, pszToken + 6 );
            papszGeomCol = CSLAddString( papszGeomCol, "" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    ITDBInfo oDbInfo(pszDbName,pszUser,pszServer,pszPass);
    CPLDebug( "OGR_IDB",
              "Connect to: db:'%s' server:'%s', user:'%s', pass:'%s'",
              pszDbName ? pszDbName : "",
              pszServer ? pszServer : "",
              pszUser ? pszUser : "",
              pszPass ? pszPass : "" );

    CPLFree( pszDbName );
    CPLFree( pszServer );
    CPLFree( pszUser );
    CPLFree( pszPass );

    poConn = new ITConnection( oDbInfo );

    poConn->AddCallback( IDBErrorHandler, nullptr );

    if( !poConn->Open() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to initialize IDB connection to %s",
                  pszNewName+4);
        CSLDestroy( papszTables );
        return FALSE;
    }

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      If no explicit list of tables was given, check for a list in    */
/*      a geometry_columns table.                                       */
/* -------------------------------------------------------------------- */
    if( papszTables == nullptr )
    {
        ITCursor oCurr( *poConn );

        if( oCurr.Prepare(" SELECT f_table_name, f_geometry_column,"
                          " geometry_type FROM geometry_columns" ) &&
            oCurr.Open(ITCursor::ReadOnly) )
        {
            ITRow * row = nullptr;
            while( (row = oCurr.NextRow()) )
            {
                papszTables =
                    CSLAddString( papszTables, row->Column(0)->Printable() );
                papszGeomCol =
                    CSLAddString( papszGeomCol, row->Column(1)->Printable() );
                row->Release();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables as           */
/*      non-spatial tables.                                             */
/* -------------------------------------------------------------------- */
    if( papszTables == nullptr )
    {
        ITCursor oTableList( *poConn );

        if ( oTableList.Prepare("select tabname from systables where tabtype='T' and tabid > 99") &&
             oTableList.Open(ITCursor::ReadOnly) )
        {
            ITRow * row = nullptr;
            while( (row = oTableList.NextRow()) )
            {
                papszTables =
                    CSLAddString( papszTables, row->Column(0)->Printable() );
                papszGeomCol = CSLAddString(papszGeomCol,"");
                row->Release();
            }
        }
        else
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to open cursor for '%s'",
                      oTableList.QueryText().Data() );
    }

/* -------------------------------------------------------------------- */
/*      If we have an explicit list of requested tables, use them       */
/*      (non-spatial).                                                  */
/* -------------------------------------------------------------------- */
    for( int iTable = 0;
         papszTables != nullptr && papszTables[iTable] != nullptr;
         iTable++ )
    {
        char * pszGeomCol = nullptr;

        if( strlen(papszGeomCol[iTable]) > 0 )
            pszGeomCol = papszGeomCol[iTable];

        OpenTable( papszTables[iTable], pszGeomCol, bUpdate );
    }

    CSLDestroy( papszTables );
    CSLDestroy( papszGeomCol );

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRIDBDataSource::OpenTable( const char *pszNewName,
                                 const char *pszGeomCol,
                                 int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRIDBTableLayer  *poLayer;

    poLayer = new OGRIDBTableLayer( this );

    if( poLayer->Initialize( pszNewName, pszGeomCol, bUpdate ) )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRIDBLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRIDBLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRIDBDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}
/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRIDBDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Execute statement.                                              */
/* -------------------------------------------------------------------- */
    ITCursor *poCurr = new ITCursor( *poConn );

    poCurr->Prepare( pszSQLCommand );
    if( !poCurr->Open(ITCursor::ReadOnly) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error execute SQL: %s", pszSQLCommand );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poCurr->RowType()->ColumnCount() == 0 )
    {
        delete poCurr;
        CPLErrorReset();
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */
    OGRIDBSelectLayer* poLayer = new OGRIDBSelectLayer( this, poCurr );

    if( poSpatialFilter != nullptr )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRIDBDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

ITCallbackResult
IDBErrorHandler( const ITErrorManager &err, void * , long )
{
    if ( err.Error() )
        CPLError( CE_Failure, CPLE_AppDefined,
                  "IDB Error: %s", err.ErrorText().Data() );

    /*if ( err.Warn() )
        CPLError( CE_Warning, CPLE_AppDefined,
                  "IDB Warning: %s", err.WarningText().Data() );
    */
    return IT_NOTHANDLED;
}
