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
        if( papoSRS[i] != NULL && papoSRS[i]->Dereference() == 0 )
            delete papoSRS[i];
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
/*      Verify postgresql prefix.                                       */
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

#ifdef notdef
/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRMySQLDataSource::DeleteLayer( const char *pszLayerName )

{
    int iLayer;

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            break;
    }

    if( iLayer == nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete layer '%s', but this layer is not known to OGR.", 
                  pszLayerName );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_MYSQL", "DeleteLayer(%s)", pszLayerName );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1, 
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    PGresult            *hResult;
    char                szCommand[1024];

    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    if( bHavePostGIS )
    {
        sprintf( szCommand, 
                 "SELECT DropGeometryColumn('%s','%s','wkb_geometry')",
                 pszDBName, pszLayerName );

        CPLDebug( "OGR_MYSQL", "PGexec(%s)", szCommand );

        hResult = PQexec( hPGConn, szCommand );
        PQclear( hResult );
    }

    sprintf( szCommand, "DROP TABLE %s", pszLayerName );
    CPLDebug( "OGR_MYSQL", "PGexec(%s)", szCommand );
    hResult = PQexec( hPGConn, szCommand );
    PQclear( hResult );
    
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    sprintf( szCommand, "DROP SEQUENCE %s_ogc_fid_seq", pszLayerName );
    CPLDebug( "OGR_MYSQL", "PGexec(%s)", szCommand );
    hResult = PQexec( hPGConn, szCommand );
    PQclear( hResult );
    
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
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
    PGresult            *hResult;
    char                szCommand[1024];
    const char          *pszGeomType;
    char                *pszLayerName;

    if( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

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
                DeleteLayer( pszLayerName );
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

/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    pszGeomType = CSLFetchNameValue( papszOptions, "GEOM_TYPE" );
    if( pszGeomType == NULL )
    {
        if( bHavePostGIS )
            pszGeomType = "geometry";
        else
            pszGeomType = "bytea";
    }

    if( bHavePostGIS && !EQUAL(pszGeomType,"geometry") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't override GEOM_TYPE in PostGIS enabled databases.\n"
                  "Creation of layer %s with GEOM_TYPE %s has failed.",
                  pszLayerName, pszGeomType );
        CPLFree( pszLayerName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    int nSRSId = -1;

    if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    if( !bHavePostGIS )
        sprintf( szCommand, 
                 "CREATE TABLE \"%s\" ( "
                 "   OGC_FID SERIAL, "
                 "   WKB_GEOMETRY %s )",
                 pszLayerName, pszGeomType );
    else
        sprintf( szCommand, 
                 "CREATE TABLE \"%s\" ( OGC_FID SERIAL )", 
                 pszLayerName );

    CPLDebug( "OGR_MYSQL", "PQexec(%s)", szCommand );
    hResult = PQexec(hPGConn, szCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s\n%s", szCommand, PQerrorMessage(hPGConn) );

        CPLFree( pszLayerName );

        PQclear( hResult );
        hResult = PQexec( hPGConn, "ROLLBACK" );
        PQclear( hResult );

        return NULL;
    }
    
    PQclear( hResult );

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( bHavePostGIS )
    {
        const char *pszGeometryType;

        /* Sometimes there is an old cruft entry in the geometry_columns
         * table if things were not properly cleaned up before.  We make
         * an effort to clean out such cruft.
         */
        sprintf( szCommand, 
                 "DELETE FROM geometry_columns WHERE f_table_name = '%s'", 
                 pszLayerName );
                 
        CPLDebug( "OGR_MYSQL", "PQexec(%s)", szCommand );
        hResult = PQexec(hPGConn, szCommand);
        PQclear( hResult );

        switch( wkbFlatten(eType) )
        {
          case wkbPoint:
            pszGeometryType = "POINT";
            break;

          case wkbLineString:
            pszGeometryType = "LINESTRING";
            break;

          case wkbPolygon:
            pszGeometryType = "POLYGON";
            break;

          case wkbMultiPoint:
            pszGeometryType = "MULTIPOINT";
            break;

          case wkbMultiLineString:
            pszGeometryType = "MULTILINESTRING";
            break;

          case wkbMultiPolygon:
            pszGeometryType = "MULTIPOLYGON";
            break;

          case wkbGeometryCollection:
            pszGeometryType = "GEOMETRYCOLLECTION";
            break;

          default:
            pszGeometryType = "GEOMETRY";
            break;

        }

        sprintf( szCommand, 
                 "select AddGeometryColumn('%s','%s','wkb_geometry',%d,'%s',%d)",
                 pszDBName, pszLayerName, nSRSId, pszGeometryType, 3 );

        CPLDebug( "OGR_MYSQL", "PQexec(%s)", szCommand );
        hResult = PQexec(hPGConn, szCommand);

        if( !hResult 
            || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "AddGeometryColumn failed for layer %s, layer creation has failed.",
                      pszLayerName );
            
            CPLFree( pszLayerName );

            PQclear( hResult );

            hResult = PQexec(hPGConn, "ROLLBACK");
            PQclear( hResult );

            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Complete, and commit the transaction.                           */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMySQLTableLayer     *poLayer;

    poLayer = new OGRMySQLTableLayer( this, pszLayerName, TRUE, nSRSId );

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRMySQLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRMySQLLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    CPLFree( pszLayerName );

    return poLayer;
}
#endif
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMySQLDataSource::TestCapability( const char * pszCap )

{
#ifdef notdef
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
#endif
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
