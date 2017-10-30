/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <string>
#include "ogr_mysql.h"

#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                         OGRMySQLDataSource()                         */
/************************************************************************/

OGRMySQLDataSource::OGRMySQLDataSource() :
    papoLayers(NULL),
    nLayers(0),
    pszName(NULL),
    bDSUpdate(FALSE),
    hConn(NULL),
    nKnownSRID(0),
    panSRID(NULL),
    papoSRS(NULL),
    poLongResultLayer(NULL)
{}

/************************************************************************/
/*                        ~OGRMySQLDataSource()                         */
/************************************************************************/

OGRMySQLDataSource::~OGRMySQLDataSource()

{
    InterruptLongResult();

    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    if( hConn != NULL )
        mysql_close( hConn );

    for( int i = 0; i < nKnownSRID; i++ )
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
                  "MySQL error message:%s Description: %s",
                  mysql_error( hConn ),
                  pszDescription );
    else
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", mysql_error( hConn ) );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMySQLDataSource::Open( const char * pszNewName, char** papszOpenOptionsIn,
                              int bUpdate )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      Use options process to get .my.cnf file contents.               */
/* -------------------------------------------------------------------- */
    int nPort = 0;
    char **papszTableNames=NULL;
    std::string oHost, oPassword, oUser, oDB;

    CPLString osNewName(pszNewName);
    const char* apszOpenOptions[] = { "dbname", "port", "user", "password",
                                      "host", "tables" };
    for(int i=0; i <(int)(sizeof(apszOpenOptions)/sizeof(char*));i++)
    {
        const char* pszVal = CSLFetchNameValue(papszOpenOptionsIn, apszOpenOptions[i]);
        if( pszVal )
        {
            if( osNewName.back() != ':' )
                osNewName += ",";
            if( i > 0 )
            {
                osNewName += apszOpenOptions[i];
                osNewName += "=";
            }
            if( EQUAL(apszOpenOptions[i], "tables") )
            {
                for( ; *pszVal; ++pszVal )
                {
                    if( *pszVal == ',' )
                        osNewName += ";";
                    else
                        osNewName += *pszVal;
                }
            }
            else
                osNewName += pszVal;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse out connection information.                               */
/* -------------------------------------------------------------------- */
    char **papszItems = CSLTokenizeString2( osNewName+6, ",",
                                            CSLT_HONOURSTRINGS );

    if( CSLCount(papszItems) < 1 )
    {
        CSLDestroy( papszItems );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MYSQL: request missing databasename." );
        return FALSE;
    }

    oDB = papszItems[0];

    for( int i = 1; papszItems[i] != NULL; i++ )
    {
        if( STARTS_WITH_CI(papszItems[i], "user=") )
            oUser = papszItems[i] + 5;
        else if( STARTS_WITH_CI(papszItems[i], "password=") )
            oPassword = papszItems[i] + 9;
        else if( STARTS_WITH_CI(papszItems[i], "host=") )
            oHost = papszItems[i] + 5;
        else if( STARTS_WITH_CI(papszItems[i], "port=") )
            nPort = atoi(papszItems[i] + 5);
        else if( STARTS_WITH_CI(papszItems[i], "tables=") )
        {
            CSLDestroy(papszTableNames);
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

/* -------------------------------------------------------------------- */
/*      Set desired options on the connection: charset and timeout.     */
/* -------------------------------------------------------------------- */
    if( hConn )
    {
        const char *pszTimeoutLength =
            CPLGetConfigOption( "MYSQL_TIMEOUT", "0" );

        unsigned int timeout = atoi(pszTimeoutLength);
        mysql_options(hConn, MYSQL_OPT_CONNECT_TIMEOUT, (char*)&timeout);

        mysql_options(hConn, MYSQL_SET_CHARSET_NAME, "utf8" );
    }

/* -------------------------------------------------------------------- */
/*      Perform connection.                                             */
/* -------------------------------------------------------------------- */
    if( hConn
        && mysql_real_connect( hConn,
                               oHost.length() ? oHost.c_str() : NULL,
                               oUser.length() ? oUser.c_str() : NULL,
                               oPassword.length() ? oPassword.c_str() : NULL,
                               oDB.length() ? oDB.c_str() : NULL,
                               nPort, NULL, CLIENT_INTERACTIVE ) == NULL )
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
    else
    {
        // Enable automatic reconnection
        // Must be called after mysql_real_connect() on MySQL < 5.0.19
        // and at any point on more recent versions.
        my_bool reconnect = 1;
        mysql_options(hConn, MYSQL_OPT_RECONNECT, &reconnect);
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
    for( int iRecord = 0;
         papszTableNames != NULL && papszTableNames[iRecord] != NULL;
         iRecord++ )
    {
        //  FIXME: This should be fixed to deal with tables
        //  for which we can't open because the name is bad/
        OpenTable( papszTableNames[iRecord], bUpdate );
    }

    CSLDestroy( papszTableNames );

    return nLayers > 0 || bUpdate;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRMySQLDataSource::OpenTable( const char *pszNewName, int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMySQLTableLayer  *poLayer;
    OGRErr eErr;

    poLayer = new OGRMySQLTableLayer( this, pszNewName, bUpdate );
    eErr = poLayer->Initialize(pszNewName);
    if (eErr == OGRERR_FAILURE)
        return FALSE;

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
    if( EQUAL(pszCap,ODsCRandomLayerWrite) )
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

/************************************************************************/
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS). This method "does no harm" if the tables     */
/*      exist and can be called at will.                                */
/************************************************************************/

OGRErr OGRMySQLDataSource::InitializeMetadataTables()

{
    const char*      pszCommand;
    MYSQL_RES       *hResult;
    OGRErr          eErr = OGRERR_NONE;

    pszCommand = "DESCRIBE geometry_columns";
    if( mysql_query(GetConn(), pszCommand ) )
    {
        pszCommand =
                "CREATE TABLE geometry_columns "
                "( F_TABLE_CATALOG VARCHAR(256), "
                "F_TABLE_SCHEMA VARCHAR(256), "
                "F_TABLE_NAME VARCHAR(256) NOT NULL,"
                "F_GEOMETRY_COLUMN VARCHAR(256) NOT NULL, "
                "COORD_DIMENSION INT, "
                "SRID INT,"
                "TYPE VARCHAR(256) NOT NULL)";
        if( mysql_query(GetConn(), pszCommand ) )
        {
            ReportError( pszCommand );
            eErr = OGRERR_FAILURE;
        }
        else
            CPLDebug("MYSQL","Creating geometry_columns metadata table");
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
    {
        mysql_free_result( hResult );
        hResult = NULL;
    }

    pszCommand = "DESCRIBE spatial_ref_sys";
    if( mysql_query(GetConn(), pszCommand ) )
    {
        pszCommand =
                "CREATE TABLE spatial_ref_sys "
                "(SRID INT NOT NULL, "
                "AUTH_NAME VARCHAR(256), "
                "AUTH_SRID INT, "
                "SRTEXT VARCHAR(2048))";
        if( mysql_query(GetConn(), pszCommand ) )
        {
            ReportError( pszCommand );
            eErr = OGRERR_FAILURE;
        }
        else
            CPLDebug("MYSQL","Creating spatial_ref_sys metadata table");
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
    {
        mysql_free_result( hResult );
        hResult = NULL;
    }

    return eErr;
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
    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( panSRID[i] == nId )
            return papoSRS[i];
    }

    OGRSpatialReference *poSRS = NULL;

    // make sure to attempt to free any old results
    MYSQL_RES *hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

    char szCommand[128] = {};
    snprintf( szCommand, sizeof(szCommand),
              "SELECT srtext FROM spatial_ref_sys WHERE srid = %d",
              nId );

    if( !mysql_query( GetConn(), szCommand ) )
        hResult = mysql_store_result( GetConn() );

    char  *pszWKT = NULL;
    char **papszRow = NULL;

    if( hResult != NULL )
        papszRow = mysql_fetch_row( hResult );

    if( papszRow != NULL && papszRow[0] != NULL )
    {
        pszWKT = CPLStrdup(papszRow[0]);
    }

    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

    poSRS = new OGRSpatialReference();
    char* pszWKTOri = pszWKT;
    if( pszWKT == NULL || poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
    {
        delete poSRS;
        poSRS = NULL;
    }

    CPLFree(pszWKTOri);

/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    panSRID = (int *) CPLRealloc(panSRID,sizeof(int) * (nKnownSRID+1) );
    papoSRS = (OGRSpatialReference **)
        CPLRealloc(papoSRS, sizeof(void*) * (nKnownSRID + 1) );
    panSRID[nKnownSRID] = nId;
    papoSRS[nKnownSRID] = poSRS;
    nKnownSRID ++;

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
    if( poSRS == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    char *pszWKT = NULL;
    if( poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
        return -1;

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    CPLString osCommand;
    osCommand.Printf(
             "SELECT srid FROM spatial_ref_sys WHERE srtext = '%s'",
             pszWKT );

    MYSQL_RES *hResult = NULL;
    if( !mysql_query( GetConn(), osCommand ) )
        hResult = mysql_store_result( GetConn() );

    if (!mysql_num_rows(hResult))
    {
        CPLDebug("MYSQL", "No rows exist currently exist in spatial_ref_sys");
        mysql_free_result( hResult );
        hResult = NULL;
    }
    char **papszRow = NULL;
    if( hResult != NULL )
        papszRow = mysql_fetch_row( hResult );

    if( papszRow != NULL && papszRow[0] != NULL )
    {
        const int nSRSId = atoi(papszRow[0]);
        if( hResult != NULL )
            mysql_free_result( hResult );
        hResult = NULL;
        CPLFree(pszWKT);
        return nSRSId;
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    osCommand = "SELECT MAX(srid) FROM spatial_ref_sys";
    if( !mysql_query( GetConn(), osCommand ) )
    {
        hResult = mysql_store_result( GetConn() );
        papszRow = mysql_fetch_row( hResult );
    }

    const int nSRSId = papszRow != NULL && papszRow[0] != NULL
        ? atoi(papszRow[0]) + 1
        : 1;

    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    osCommand.Printf(
             "INSERT INTO spatial_ref_sys (srid,srtext) VALUES (%d,'%s')",
             nSRSId, pszWKT );

    if( !mysql_query( GetConn(), osCommand ) )
        /*hResult = */ mysql_store_result( GetConn() ); /* FIXME ? */

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

    CPLFree(pszWKT);

    return nSRSId;
}

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
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
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
    char *pszSafeName = CPLStrdup( pszSrcName );

    for( int i = 0; pszSafeName[i] != '\0'; i++ )
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

OGRErr OGRMySQLDataSource::DeleteLayer( int iLayer)

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
    CPLString osCommand;

    osCommand.Printf(
             "DROP TABLE `%s` ",
             osLayerName.c_str() );

    if( !mysql_query(GetConn(), osCommand ) )
    {
        CPLDebug("MYSQL","Dropped table %s.", osLayerName.c_str());
        return OGRERR_NONE;
    }
    else
    {
        ReportError( osCommand );
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRMySQLDataSource::ICreateLayer( const char * pszLayerNameIn,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    MYSQL_RES  *hResult=NULL;
    CPLString   osCommand;
    const char *pszGeometryType;
    const char *pszGeomColumnName;
    const char *pszExpectedFIDName;
    char       *pszLayerName;
    // int        nDimension = 3; // MySQL only supports 2d currently

/* -------------------------------------------------------------------- */
/*      Make sure there isn't an active transaction already.            */
/* -------------------------------------------------------------------- */
    InterruptLongResult();

    if( CPLFetchBool(papszOptions, "LAUNDER", true) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

    // if( wkbFlatten(eType) == eType )
    //    nDimension = 2;

    CPLDebug("MYSQL","Creating layer %s.", pszLayerName);

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */

    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
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
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                CPLFree( pszLayerName );
                return NULL;
            }
        }
    }

    pszGeomColumnName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if (!pszGeomColumnName)
        pszGeomColumnName="SHAPE";

    pszExpectedFIDName = CSLFetchNameValue( papszOptions, "FID" );
    if (!pszExpectedFIDName)
        pszExpectedFIDName = CSLFetchNameValue( papszOptions, "MYSQL_FID" );
    if (!pszExpectedFIDName)
        pszExpectedFIDName="OGR_FID";

    const bool bFID64 = CPLFetchBool(papszOptions, "FID64", false);
    const char* pszFIDType = bFID64 ? "BIGINT": "INT";

    CPLDebug("MYSQL","Geometry Column Name %s.", pszGeomColumnName);
    CPLDebug("MYSQL","FID Column Name %s.", pszExpectedFIDName);

    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    const bool bHasSI = ( eType != wkbNone && (pszSI == NULL || CPLTestBool(pszSI)) );

    if( wkbFlatten(eType) == wkbNone )
    {
        osCommand.Printf(
                 "CREATE TABLE `%s` ( "
                 "   %s %s UNIQUE NOT NULL AUTO_INCREMENT )",
                 pszLayerName, pszExpectedFIDName, pszFIDType );
    }
    else
    {
        osCommand.Printf(
                 "CREATE TABLE `%s` ( "
                 "   %s %s UNIQUE NOT NULL AUTO_INCREMENT, "
                 "   %s GEOMETRY %s)",
                 pszLayerName, pszExpectedFIDName, pszFIDType, pszGeomColumnName,
                 bHasSI ? "NOT NULL" : "");
    }

    if( CSLFetchNameValue( papszOptions, "ENGINE" ) != NULL )
    {
        osCommand += " ENGINE = ";
        osCommand += CSLFetchNameValue( papszOptions, "ENGINE" );
    }

    if( !mysql_query(GetConn(), osCommand ) )
    {
        if( mysql_field_count( GetConn() ) == 0 )
            CPLDebug("MYSQL","Created table %s.", pszLayerName);
        else
        {
            ReportError( osCommand );
            return NULL;
        }
    }
    else
    {
        ReportError( osCommand );
        return NULL;
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

    // Calling this does no harm
    InitializeMetadataTables();

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    int nSRSId = -1;

    if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Sometimes there is an old crufty entry in the geometry_columns  */
/*      table if things were not properly cleaned up before.  We make   */
/*      an effort to clean out such cruft.                              */
/*                                                                      */
/* -------------------------------------------------------------------- */
    osCommand.Printf(
             "DELETE FROM geometry_columns WHERE f_table_name = '%s'",
             pszLayerName );

    if( mysql_query(GetConn(), osCommand ) )
    {
        ReportError( osCommand );
        return NULL;
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;

/* -------------------------------------------------------------------- */
/*      Attempt to add this table to the geometry_columns table, if     */
/*      it is a spatial layer.                                          */
/* -------------------------------------------------------------------- */
    if( eType != wkbNone )
    {
        const int nCoordDimension = eType == wkbFlatten(eType) ? 2 : 3;

        pszGeometryType = OGRToOGCGeomType(eType);

        if( nSRSId == -1 )
            osCommand.Printf(
                     "INSERT INTO geometry_columns "
                     " (F_TABLE_NAME, "
                     "  F_GEOMETRY_COLUMN, "
                     "  COORD_DIMENSION, "
                     "  TYPE) values "
                     "  ('%s', '%s', %d, '%s')",
                     pszLayerName,
                     pszGeomColumnName,
                     nCoordDimension,
                     pszGeometryType );
        else
            osCommand.Printf(
                     "INSERT INTO geometry_columns "
                     " (F_TABLE_NAME, "
                     "  F_GEOMETRY_COLUMN, "
                     "  COORD_DIMENSION, "
                     "  SRID, "
                     "  TYPE) values "
                     "  ('%s', '%s', %d, %d, '%s')",
                     pszLayerName,
                     pszGeomColumnName,
                     nCoordDimension,
                     nSRSId,
                     pszGeometryType );

        if( mysql_query(GetConn(), osCommand ) )
        {
            ReportError( osCommand );
            return NULL;
        }

        // make sure to attempt to free results of successful queries
        hResult = mysql_store_result( GetConn() );
        if( hResult != NULL )
            mysql_free_result( hResult );
        hResult = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the spatial index.                                       */
/*                                                                      */
/*      We're doing this before we add geometry and record to the table */
/*      so this may not be exactly the best way to do it.               */
/* -------------------------------------------------------------------- */
    if( bHasSI )
    {
        osCommand.Printf(
                 "ALTER TABLE `%s` ADD SPATIAL INDEX(`%s`) ",
                 pszLayerName,
                 pszGeomColumnName);

        if( mysql_query(GetConn(), osCommand ) )
        {
            ReportError( osCommand );
            return NULL;
        }

        // make sure to attempt to free results of successful queries
        hResult = mysql_store_result( GetConn() );
        if( hResult != NULL )
            mysql_free_result( hResult );
        hResult = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMySQLTableLayer     *poLayer;
    OGRErr                  eErr;

    poLayer = new OGRMySQLTableLayer( this, pszLayerName, TRUE, nSRSId );
    eErr = poLayer->Initialize(pszLayerName);
    if (eErr == OGRERR_FAILURE)
        return NULL;
    if( eType != wkbNone )
        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetNullable(FALSE);

    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    poLayer->SetPrecisionFlag( CPLFetchBool(papszOptions, "PRECISION", true));

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRMySQLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRMySQLLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszLayerName );

    return poLayer;
}
