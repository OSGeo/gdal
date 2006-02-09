/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDataSource class.
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
 * Revision 1.7  2006/02/09 05:54:52  hobu
 * start on CreateLayer
 *
 * Revision 1.6  2006/01/16 16:04:58  hobu
 * comment typo
 *
 * Revision 1.5  2005/09/21 01:00:01  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.4  2005/08/02 13:01:29  fwarmerdam
 * Removed old postgis create logic.
 *
 * Revision 1.3  2004/10/12 16:59:31  fwarmerdam
 * rearrange include files for win32
 *
 * Revision 1.2  2004/10/08 20:50:05  fwarmerdam
 * Implemented ExecuteSQL().
 * Added support for getting host, password, port, and user from the
 * datasource name or from the mysql init files (ie. ~/.my.cnf).
 *
 * Revision 1.1  2004/10/07 20:56:15  fwarmerdam
 * New
 *
 */


#include <string>
#include "ogr_mysql.h"
#include <my_sys.h>

#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                         OGRMySQLDataSource()                         */
/************************************************************************/

OGRMySQLDataSource::OGRMySQLDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    hConn = 0;
    nSoftTransactionLevel = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    poLongResultLayer = NULL;
}

/************************************************************************/
/*                        ~OGRMySQLDataSource()                         */
/************************************************************************/

OGRMySQLDataSource::~OGRMySQLDataSource()

{
    int         i;

//    FlushSoftTransaction();

    InterruptLongResult();

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    if( hConn != NULL )
        mysql_close( hConn );

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
}

/************************************************************************/
/*                            ReportError()                             */
/************************************************************************/

void OGRMySQLDataSource::ReportError( const char *pszDescription )

{
    if( pszDescription )
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s\n%s", pszDescription, mysql_error( hConn ) );
    else
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", mysql_error( hConn ) );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMySQLDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      Verify MySQL prefix.                                            */
/* -------------------------------------------------------------------- */
    if( !EQUALN(pszNewName,"MYSQL:",6) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s does not conform to MySQL naming convention,"
                      " MYSQL:dbname[, user=..][,password=..][,host=..][,port=..][tables=table;table;...]",
                      pszNewName );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Use options process to get .my.cnf file contents.               */
/* -------------------------------------------------------------------- */
    int nPort = 0, i;
    char **papszTableNames=NULL;
    std::string oHost, oPassword, oUser, oDB;
    char *apszArgv[2] = { "org", NULL };
    char **papszArgv = apszArgv;
    int  nArgc = 1;
    const char *client_groups[] = {"client", "ogr", NULL };

    my_init(); // I hope there is no problem with calling this multiple times!
    load_defaults( "my", client_groups, &nArgc, &papszArgv );

    for( i = 0; i < nArgc; i++ )
    {
        if( EQUALN(papszArgv[i],"--user=",7) )
            oUser = papszArgv[i] + 7;
        else if( EQUALN(papszArgv[i],"--host=",7) )
            oHost = papszArgv[i] + 7;
        else if( EQUALN(papszArgv[i],"--password=",11) )
            oPassword = papszArgv[i] + 11;
        else if( EQUALN(papszArgv[i],"--port=",7) )
            nPort = atoi(papszArgv[i] + 7);
    }

    // cleanup
    free_defaults( papszArgv );

/* -------------------------------------------------------------------- */
/*      Parse out connection information.                               */
/* -------------------------------------------------------------------- */
    char **papszItems = CSLTokenizeString2( pszNewName+6, ",", 
                                            CSLT_HONOURSTRINGS );

    if( CSLCount(papszItems) < 1 )
    {
        CSLDestroy( papszItems );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "MYSQL: request missing databasename." );
        return FALSE;
    }

    oDB = papszItems[0];

    for( i = 1; papszItems[i] != NULL; i++ )
    {
        if( EQUALN(papszItems[i],"user=",5) )
            oUser = papszItems[i] + 5;
        else if( EQUALN(papszItems[i],"password=",9) )
            oPassword = papszItems[i] + 9;
        else if( EQUALN(papszItems[i],"host=",5) )
            oHost = papszItems[i] + 5;
        else if( EQUALN(papszItems[i],"port=",5) )
            nPort = atoi(papszItems[i] + 5);
        else if( EQUALN(papszItems[i],"tables=",7) )
        {
            papszTableNames = CSLTokenizeStringComplex( 
                papszItems[i] + 7, ";", FALSE, FALSE );
        }
        else
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "'%s' in MYSQL datasource definition not recognised and ignored.", papszItems[i] );
    }

    CSLDestroy( papszItems );

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    hConn = mysql_init( NULL );
    if( hConn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "mysql_init() failed." );
    }

    if( hConn
        && mysql_real_connect( hConn, 
                               oHost.length() ? oHost.c_str() : NULL,
                               oUser.length() ? oUser.c_str() : NULL,
                               oPassword.length() ? oPassword.c_str() : NULL,
                               oDB.length() ? oDB.c_str() : NULL,
                               nPort, NULL, 0 ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MySQL connect failed for: %s\n%s", 
                  pszNewName + 6, mysql_error( hConn ) );
        mysql_close( hConn );
        hConn = NULL;
    }

    if( hConn == NULL )
    {
        CSLDestroy( papszTableNames );
        return FALSE;
    }
    
    pszName = CPLStrdup( pszNewName );
    
    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Get a list of available tables.                                 */
/* -------------------------------------------------------------------- */
    if( papszTableNames == NULL )
    {
        MYSQL_RES *hResultSet;
        MYSQL_ROW papszRow;

        if( mysql_query( hConn, "SHOW TABLES" ) )
        {
            ReportError( "SHOW TABLES Failed" );
            return FALSE;
        }

        hResultSet = mysql_store_result( hConn );
        if( hResultSet == NULL )
        {
            ReportError( "mysql_store_result() failed on SHOW TABLES result.");
            return FALSE;
        }
    
        while( (papszRow = mysql_fetch_row( hResultSet )) != NULL )
        {
            if( papszRow[0] == NULL )
                continue;

            if( EQUAL(papszRow[0],"spatial_ref_sys")
                || EQUAL(papszRow[0],"geometry_columns") )
                continue;

            papszTableNames = CSLAddString(papszTableNames, papszRow[0] );
        }

        mysql_free_result( hResultSet );
    }

/* -------------------------------------------------------------------- */
/*      Get the schema of the available tables.                         */
/* -------------------------------------------------------------------- */
    int iRecord;

    for( iRecord = 0; 
         papszTableNames != NULL && papszTableNames[iRecord] != NULL;
         iRecord++ )
    {
        OpenTable( papszTableNames[iRecord], bUpdate, FALSE );
    }

    CSLDestroy( papszTableNames );
    
    return nLayers > 0 || bUpdate;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRMySQLDataSource::OpenTable( const char *pszNewName, int bUpdate,
                                int bTestOpen )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMySQLLayer  *poLayer;

    poLayer = new OGRMySQLTableLayer( this, pszNewName, bUpdate );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRMySQLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRMySQLLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMySQLDataSource::TestCapability( const char * pszCap )

{
	
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
	if( EQUAL(pszCap, ODsCDeleteLayer))
		return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMySQLDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

#ifdef notdef
/************************************************************************/
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS).                                              */
/************************************************************************/

OGRErr OGRMySQLDataSource::InitializeMetadataTables()

{
    // implement later.
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRMySQLDataSource::FetchSRS( int nId )

{
    if( nId < 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    int  i;

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( panSRID[i] == nId )
            return papoSRS[i];
    }

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table.                        */
/* -------------------------------------------------------------------- */
    PGresult        *hResult;
    char            szCommand[1024];
    OGRSpatialReference *poSRS = NULL;
        
    SoftStartTransaction();

    sprintf( szCommand, 
             "SELECT srtext FROM spatial_ref_sys "
             "WHERE srid = %d", 
             nId );
    hResult = PQexec(hPGConn, szCommand );

    if( hResult 
        && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) == 1 )
    {
        char *pszWKT;

        pszWKT = PQgetvalue(hResult,0,0);
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    PQclear( hResult );
    SoftCommit();

/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    panSRID = (int *) CPLRealloc(panSRID,sizeof(int) * (nKnownSRID+1) );
    papoSRS = (OGRSpatialReference **) 
        CPLRealloc(papoSRS, sizeof(void*) * (nKnownSRID + 1) );
    panSRID[nKnownSRID] = nId;
    papoSRS[nKnownSRID] = poSRS;

    return poSRS;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRMySQLDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    PGresult            *hResult;
    char                szCommand[10000];
    char                *pszWKT = NULL;
    int                 nSRSId;

    if( poSRS == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    if( poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
        return -1;
    
    CPLAssert( strlen(pszWKT) < sizeof(szCommand) - 500 );

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");

    sprintf( szCommand, 
             "SELECT srid FROM spatial_ref_sys WHERE srtext = '%s'",
             pszWKT );
    hResult = PQexec(hPGConn, szCommand );
                     
/* -------------------------------------------------------------------- */
/*      We got it!  Return it.                                          */
/* -------------------------------------------------------------------- */
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) > 0 )
    {
        nSRSId = atoi(PQgetvalue( hResult, 0, 0 ));
        
        PQclear( hResult );

        hResult = PQexec(hPGConn, "COMMIT");
        PQclear( hResult );

        return nSRSId;
    }
    
/* -------------------------------------------------------------------- */
/*      If the command actually failed, then the metadata table is      */
/*      likely missing. Try defining it.                                */
/* -------------------------------------------------------------------- */
    int         bTableMissing;

    bTableMissing = 
        hResult == NULL || PQresultStatus(hResult) == PGRES_NONFATAL_ERROR;

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    if( bTableMissing )
    {
        if( InitializeMetadataTables() != OGRERR_NONE )
            return -1;
    }

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    hResult = PQexec(hPGConn, "SELECT MAX(srid) FROM spatial_ref_sys" );

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK )
    {
        nSRSId = atoi(PQgetvalue(hResult,0,0)) + 1;
        PQclear( hResult );
    }
    else
        nSRSId = 1;
    
/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    sprintf( szCommand, 
             "INSERT INTO spatial_ref_sys (srid,srtext) VALUES (%d,'%s')",
             nSRSId, pszWKT );
             
    hResult = PQexec(hPGConn, szCommand );
    PQclear( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    return nSRSId;
}

/************************************************************************/
/*                        SoftStartTransaction()                        */
/*                                                                      */
/*      Create a transaction scope.  If we already have a               */
/*      transaction active this isn't a real transaction, but just      */
/*      an increment to the scope count.                                */
/************************************************************************/

OGRErr OGRMySQLDataSource::SoftStartTransaction()

{
    nSoftTransactionLevel++;

    if( nSoftTransactionLevel == 1 )
    {
        PGresult            *hResult;
        PGconn          *hPGConn = GetPGConn();

        //CPLDebug( "OGR_MYSQL", "BEGIN Transaction" );
        hResult = PQexec(hPGConn, "BEGIN");

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLDebug( "OGR_MYSQL", "BEGIN Transaction failed:\n%s",
                      PQerrorMessage( hPGConn ) );
            return OGRERR_FAILURE;
        }

        PQclear( hResult );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SoftCommit()                             */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRMySQLDataSource::SoftCommit()

{
    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "OGR_MYSQL", "SoftCommit() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel--;

    if( nSoftTransactionLevel == 0 )
    {
        PGresult            *hResult;
        PGconn          *hPGConn = GetPGConn();

        //CPLDebug( "OGR_MYSQL", "COMMIT Transaction" );
        hResult = PQexec(hPGConn, "COMMIT");

        if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLDebug( "OGR_MYSQL", "COMMIT Transaction failed:\n%s",
                      PQerrorMessage( hPGConn ) );
            return OGRERR_FAILURE;
        }

        PQclear( hResult );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SoftRollback()                            */
/*                                                                      */
/*      Force a rollback of the current transaction if there is one,    */
/*      even if we are nested several levels deep.                      */
/************************************************************************/

OGRErr OGRMySQLDataSource::SoftRollback()

{
    if( nSoftTransactionLevel <= 0 )
    {
        CPLDebug( "OGR_MYSQL", "SoftRollback() with no transaction active." );
        return OGRERR_FAILURE;
    }

    nSoftTransactionLevel = 0;

    PGresult            *hResult;
    PGconn              *hPGConn = GetPGConn();
    
    hResult = PQexec(hPGConn, "ROLLBACK");
    
    if( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        return OGRERR_FAILURE;
    
    PQclear( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        FlushSoftTransaction()                        */
/*                                                                      */
/*      Force the unwinding of any active transaction, and it's         */
/*      commit.                                                         */
/************************************************************************/

OGRErr OGRMySQLDataSource::FlushSoftTransaction()

{
    if( nSoftTransactionLevel <= 0 )
        return OGRERR_NONE;

    nSoftTransactionLevel = 1;

    return SoftCommit();
}
#endif

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRMySQLDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    if( poSpatialFilter != NULL )
    {
        CPLDebug( "OGR_MYSQL", 
          "Spatial filter ignored for now in OGRMySQLDataSource::ExecuteSQL()" );
    }

/* -------------------------------------------------------------------- */
/*      Use generic implementation for OGRSQL dialect.                  */
/* -------------------------------------------------------------------- */
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        DeleteLayer( pszLayerName );
        return NULL;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Make sure there isn't an active transaction already.            */
/* -------------------------------------------------------------------- */
    InterruptLongResult();

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    MYSQL_RES *hResultSet;

    if( mysql_query( hConn, pszSQLCommand ) )
    {
        ReportError( pszSQLCommand );
        return NULL;
    }

    hResultSet = mysql_use_result( hConn );
    if( hResultSet == NULL )
    {
        if( mysql_field_count( hConn ) == 0 )
        {
            CPLDebug( "MYSQL", "Command '%s' succeeded, %d rows affected.", 
                      pszSQLCommand, 
                      (int) mysql_affected_rows(hConn) );
            return NULL;
        }
        else
        {
            ReportError( pszSQLCommand );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a tuple result? If so, instantiate a results         */
/*      layer for it.                                                   */
/* -------------------------------------------------------------------- */

    OGRMySQLResultLayer *poLayer = NULL;

    poLayer = new OGRMySQLResultLayer( this, pszSQLCommand, hResultSet );
        
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRMySQLDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRMySQLDataSource::LaunderName( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );
    int     i;

    for( i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    return pszSafeName;
}

/************************************************************************/
/*                         RequestLongResult()                          */
/*                                                                      */
/*      Layers need to use mysql_use_result() instead of                */
/*      mysql_store_result() so that we won't have to load entire       */
/*      result sets into RAM.  But only one "streamed" resultset can    */
/*      be active on a database connection at a time.  So we need to    */
/*      maintain a way of closing off an active streaming resultset     */
/*      before any other sort of query with a resultset is              */
/*      executable.  This method (and InterruptLongResult())            */
/*      implement that exclusion.                                       */
/************************************************************************/

void OGRMySQLDataSource::RequestLongResult( OGRMySQLLayer * poNewLayer )

{
    InterruptLongResult();
    poLongResultLayer = poNewLayer;
}

/************************************************************************/
/*                        InterruptLongResult()                         */
/************************************************************************/

void OGRMySQLDataSource::InterruptLongResult()

{
    if( poLongResultLayer != NULL )
    {
        poLongResultLayer->ResetReading();
        poLongResultLayer = NULL;
    }
}


/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRMySQLDataSource::DeleteLayer( int iLayer)

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;
/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = papoLayers[iLayer]->GetLayerDefn()->GetName();
    
    CPLDebug( "MYSQL", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
	char        		szCommand[1024];

	sprintf( szCommand,
	         "DROP TABLE %s ",
	         osLayerName.c_str() );

		if( !mysql_query(GetConn(), szCommand ) ){

			 if( mysql_field_count( GetConn() ) == 0 )
		        {
		            CPLDebug("MYSQL","Dropped table %s.", osLayerName.c_str());
					return OGRERR_NONE;
		        }
		        else
		        {
		            ReportError( szCommand );
					return OGRERR_FAILURE;
		        }
		}

}




/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRMySQLDataSource::CreateLayer( const char * pszLayerNameIn,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    MYSQL_RES           *hResult=NULL;
	char        		szCommand[1024];
    const char          *pszGeomType;
	const char			*pszGeomColumnName;
	const char 			*pszExpectedFIDName; 
	
    char                *pszLayerName;
    int                 nDimension = 3; // MySQL only supports 2d currently


    if( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

    if( wkbFlatten(eType) == eType )
        nDimension = 2;
CPLDebug("MYSQL:","Attempting to create layer %s.", pszLayerName);

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */

    int iLayer;
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
			
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( iLayer );
            }
            else
            {
                CPLFree( pszLayerName );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                return NULL;
            }
        }
    }


    pszGeomColumnName = CSLFetchNameValue( papszOptions, "MYSQL_GEOM_COLUMN" );
    if (!pszGeomColumnName)
        pszGeomColumnName="SHAPE";

    pszExpectedFIDName = CSLFetchNameValue( papszOptions, "MYSQL_FID" );
    if (!pszExpectedFIDName)
        pszExpectedFIDName="OGR_FID";



	CPLDebug("MYSQL:","Geometry Column Name %s.", pszGeomColumnName);
	CPLDebug("MYSQL:","FID Column Name %s.", pszExpectedFIDName);

    sprintf( szCommand,
             "CREATE TABLE %s ( "
             "   %s INT UNIQUE NOT NULL, "
             "   %s GEOMETRY NOT NULL )",
             pszLayerName, pszExpectedFIDName, pszGeomColumnName );


	
	if( !mysql_query(GetConn(), szCommand ) ){
		 if( mysql_field_count( GetConn() ) == 0 )
	        {
	            CPLDebug("MYSQL","Created table %s.", pszLayerName);
	        }
	        else
	        {
	            ReportError( szCommand );
	            return NULL;
	        }
	}
		
	return NULL;
}