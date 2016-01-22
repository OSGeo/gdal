/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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


#include "ogr_ingres.h"

#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            SetConnParam()                            */
/*                                                                      */
/*      Set Connection Parameters                                       */
/************************************************************************/
IIAPI_STATUS
SetConnParam(II_PTR *connHandle,
             II_LONG paramID,
             const II_PTR paramValue)
{
    IIAPI_SETCONPRMPARM	setconnParm;
    IIAPI_WAITPARM	waitParm = { -1 };

    setconnParm.sc_genParm.gp_callback = NULL;
    setconnParm.sc_genParm.gp_closure = NULL;
    setconnParm.sc_connHandle = *connHandle;
    setconnParm.sc_paramID = paramID;
    setconnParm.sc_paramValue = paramValue;
	
    IIapi_setConnectParam(&setconnParm);
	
    while( setconnParm.sc_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );
	
    if (setconnParm.sc_genParm.gp_errorHandle)
    {
        OGRIngresStatement::ReportError( &(setconnParm.sc_genParm), 
                                         "Failed to set OpenAPI connection para." );
        return IIAPI_ST_FAILURE;
    }
    else
    {
        /* save the handle   */
        *connHandle = setconnParm.sc_connHandle;
    }
   
    return (setconnParm.sc_genParm.gp_status);
}

/************************************************************************/
/*                         OGRIngresDataSource()                        */
/************************************************************************/

OGRIngresDataSource::OGRIngresDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    hConn = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;
    poActiveLayer = NULL;
}

/************************************************************************/
/*                        ~OGRIngresDataSource()                        */
/************************************************************************/

OGRIngresDataSource::~OGRIngresDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

#ifdef notdef
    if( hConn != NULL )
        ingres_close( hConn );
#endif

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRIngresDataSource::Open( const char *pszFullName, 
                               char **papszOptions, int bUpdate )


{
    CPLAssert( nLayers == 0 );
  
 #define MAX_TARGET_STRING_LENGTH 512
    char pszDBTarget[MAX_TARGET_STRING_LENGTH];

/* -------------------------------------------------------------------- */
/*      Verify we have a dbname, this parameter is required.            */
/* -------------------------------------------------------------------- */
    const char *pszDBName = CSLFetchNameValue(papszOptions,"dbname");

    if( pszDBName == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "No DBNAME item provided in INGRES datasource name." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a table list?                                        */
/* -------------------------------------------------------------------- */
    char **papszTableNames = NULL;
    const char *pszTables = CSLFetchNameValue(papszOptions,"tables");

    if( pszTables != NULL )
        papszTableNames = CSLTokenizeStringComplex(pszTables,"/",TRUE,FALSE);
   
/* -------------------------------------------------------------------- */
/*      Add support to dynamic vnode if passed                          */
/* -------------------------------------------------------------------- */
    const char *pszHost = CSLFetchNameValue(papszOptions,"host");
    if (pszHost)
    {
        const char *pszInstance = CSLFetchNameValue(papszOptions,"instance");
        if (pszInstance == NULL || strlen(pszInstance) != 2)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                  "instance name must be specified with host." );
            return FALSE;
        }
        
        /* 
        ** make sure the user name and password are passed too,
        ** note it could not be zero length.
        */ 
        const char *pszUsername = CSLFetchNameValue(papszOptions,"username");
        const char *pszPassword = CSLFetchNameValue(papszOptions,"password");
        
        if (pszUsername == NULL || strlen(pszUsername) == 0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                  "user name must be specified in dynamic vnode." );            
            return FALSE;
        }
        
        if (pszPassword == NULL || strlen(pszPassword) == 0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                  "password must be specified in dynamic vnode." );            
            return FALSE;
        }
        
        /* 
        ** construct the vnode string, like : 
        ** @host,protocol,port[;attribute=value{;attribute=value}][[user,password]], 
        ** visit for detail 
        ** http://docs.actian.com/ingres/10.0/command-reference-guide/1207-dynamic-vnode-specificationconnect-to-remote-node
        */
        sprintf(pszDBTarget, "@%s,%s,%s;%s[%s,%s]::%s ", 
            pszHost,        /* host, compute name or IP address */
            "TCP_IP",       /* protocal, default with TCP/IP */
            pszInstance,    /* instance Name */
            "" ,            /* option, Null */
            pszUsername,    /* user name, could not be empty */
            pszPassword,    /* pwd */
            pszDBName       /* database name */
            );
        
       CPLDebug("INGRES", pszDBTarget);
    }
    else
    {
        /* Remain the database name */
        strcpy(pszDBTarget, pszDBName);
    }
    
/* -------------------------------------------------------------------- */
/*      Initialize the Ingres API. Should we only do this once per      */
/*      program run?  Really we should also try to terminate the api    */
/*      on program exit.                                                */
/* -------------------------------------------------------------------- */
    IIAPI_INITPARM  initParm;

    initParm.in_version = IIAPI_VERSION_1; 
    initParm.in_timeout = -1;
    IIapi_initialize( &initParm );

/* -------------------------------------------------------------------- */
/*      check effective user and db password                            */
/* -------------------------------------------------------------------- */
    hConn = NULL;
    const char *pszEffuser = CSLFetchNameValue(papszOptions,"effuser");
    const char *pszDBpwd = CSLFetchNameValue(papszOptions,"dbpwd");
    if ( pszEffuser 
        && strlen(pszEffuser) > 0 
        && pszDBpwd 
        && strlen(pszDBpwd) > 0 )
    { 
        if (SetConnParam(&hConn, IIAPI_CP_EFFECTIVE_USER,
			(II_PTR)pszEffuser) != IIAPI_ST_SUCCESS 
            || SetConnParam(&hConn, IIAPI_CP_DBMS_PASSWORD,
			(II_PTR)pszDBpwd) != IIAPI_ST_SUCCESS )
        {
            return FALSE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Try to connect to the database.                                 */
/* -------------------------------------------------------------------- */
    IIAPI_CONNPARM	connParm;
    IIAPI_WAITPARM	waitParm = { -1 };
    
    memset( &connParm, 0, sizeof(connParm) );
    connParm.co_genParm.gp_callback = NULL;
    connParm.co_genParm.gp_closure = NULL;
    connParm.co_target = (II_CHAR *) pszDBTarget;
    connParm.co_connHandle = hConn;
    connParm.co_tranHandle = NULL;
    connParm.co_username = 
        (II_CHAR*) CSLFetchNameValue(papszOptions,"username");
    connParm.co_password = 
        (II_CHAR*)CSLFetchNameValue(papszOptions,"password");
    connParm.co_timeout = -1;

    if( CSLFetchNameValue(papszOptions,"timeout") != NULL )
        connParm.co_timeout = atoi(CSLFetchNameValue(papszOptions,"timeout"));

    IIapi_connect( &connParm );
       
    while( connParm.co_genParm.gp_completed == FALSE )
	IIapi_wait( &waitParm );

    hConn = connParm.co_connHandle;

    if( connParm.co_genParm.gp_status != IIAPI_ST_SUCCESS 
        || hConn == NULL )
    {
        OGRIngresStatement::ReportError( &(connParm.co_genParm), 
                                    "Failed to connect to Ingres database." );
        return FALSE;
    }

    pszName = CPLStrdup( pszFullName );
    
    bDSUpdate = bUpdate;

    // Check for new or old Ingres spatial library
    {
    	OGRIngresStatement oStmt( hConn );

    	if( oStmt.ExecuteSQL("SELECT COUNT(*) FROM iicolumns WHERE table_name = 'iiattribute' AND column_name = 'attgeomtype'" ) )
    	{
    		char **papszFields;
    		while( (papszFields = oStmt.GetRow()) )
    		{
    			CPLString osCount = papszFields[0];
    			if( osCount[0] == '0' )
    			{
    				bNewIngres = FALSE;
    			}
    			else
    			{
    				bNewIngres = TRUE;
    			}
    		}
    	}
    }

/* -------------------------------------------------------------------- */
/*      Get a list of available tables.                                 */
/* -------------------------------------------------------------------- */
    if( papszTableNames == NULL )
    {
        OGRIngresStatement oStmt( hConn );
        
        if( oStmt.ExecuteSQL( "select table_name from iitables where system_use = 'U' and table_name not like 'iietab_%'" ) )
        {
            char **papszFields;
            while( (papszFields = oStmt.GetRow()) )
            {
                CPLString osTableName = papszFields[0];
                osTableName.Trim();
                papszTableNames = CSLAddString( papszTableNames, 
                                                osTableName );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the schema of the available tables.                         */
/* -------------------------------------------------------------------- */
    int iRecord;

    for( iRecord = 0; 
         papszTableNames != NULL && papszTableNames[iRecord] != NULL;
         iRecord++ )
    {
        OpenTable( papszTableNames[iRecord], bUpdate );
    }

    CSLDestroy( papszTableNames );

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRIngresDataSource::OpenTable( const char *pszNewName, int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRIngresTableLayer  *poLayer;
    OGRErr eErr;

    poLayer = new OGRIngresTableLayer( this, pszNewName, bUpdate );
    eErr = poLayer->Initialize(pszNewName);
    if (eErr == OGRERR_FAILURE)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRIngresLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRIngresLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresDataSource::TestCapability( const char * pszCap )

{
	
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCDeleteLayer))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRIngresDataSource::GetLayer( int iLayer )

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

OGRErr OGRIngresDataSource::InitializeMetadataTables()

{
#ifdef notdef
    char            szCommand[1024];
    INGRES_RES       *hResult;
    OGRErr	    eErr = OGRERR_NONE;
 
    sprintf( szCommand, "DESCRIBE geometry_columns" );
    if( ingres_query(GetConn(), szCommand ) )
    {
        sprintf(szCommand,
                "CREATE TABLE geometry_columns "
                "( F_TABLE_CATALOG VARCHAR(256), "
                "F_TABLE_SCHEMA VARCHAR(256), "
                "F_TABLE_NAME VARCHAR(256) NOT NULL," 
                "F_GEOMETRY_COLUMN VARCHAR(256) NOT NULL, "
                "COORD_DIMENSION INT, "
                "SRID INT,"
                "TYPE VARCHAR(256) NOT NULL)");
        if( ingres_query(GetConn(), szCommand ) )
        {
            ReportError( szCommand );
            eErr = OGRERR_FAILURE;
        }
        else
            CPLDebug("INGRES","Creating geometry_columns metadata table");
 
    }
 
    // make sure to attempt to free results of successful queries
    hResult = ingres_store_result( GetConn() );
    if( hResult != NULL )
    {
        ingres_free_result( hResult );
        hResult = NULL;   
    }
 
    sprintf( szCommand, "DESCRIBE spatial_ref_sys" );
    if( ingres_query(GetConn(), szCommand ) )
    {
        sprintf(szCommand,
                "CREATE TABLE spatial_ref_sys "
                "(SRID INT NOT NULL, "
                "AUTH_NAME VARCHAR(256), "
                "AUTH_SRID INT, "
                "SRTEXT VARCHAR(2048))");
        if( ingres_query(GetConn(), szCommand ) )
        {
            ReportError( szCommand );
            eErr = OGRERR_FAILURE;
        }
        else
            CPLDebug("INGRES","Creating spatial_ref_sys metadata table");
 
    }    
 
    // make sure to attempt to free results of successful queries
    hResult = ingres_store_result( GetConn() );
    if( hResult != NULL )
    {
        ingres_free_result( hResult );
        hResult = NULL;
    }
 
    return eErr;
#endif
    return OGRERR_NONE;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRIngresDataSource::FetchSRS( int nId )
{
    char         szCommand[1024];
    char           **papszRow;
    OGRIngresStatement oStatement(GetConn());
            
    if( nId < 0 )
        return NULL;

    /*
     * Only the new Ingres Geospatial library
     */
    if(IsNewIngres() == FALSE)
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

    OGRSpatialReference *poSRS = NULL;

    sprintf( szCommand,
         "SELECT srtext FROM spatial_ref_sys WHERE srid = %d",
         nId );

    oStatement.ExecuteSQL(szCommand);
        
    char    *pszWKT = NULL;
    papszRow = NULL;
    

    papszRow = oStatement.GetRow();

    if( papszRow != NULL)
    {
        if(papszRow[0] != NULL )
        {
            //VARCHAR uses the first two bytes for length
            pszWKT = &papszRow[0][2];
        }
    }

     poSRS = new OGRSpatialReference();
     if( pszWKT == NULL || poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
     {
         delete poSRS;
         poSRS = NULL;
     }

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

int OGRIngresDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    char            **papszRow;
    char            szCommand[10000];
    char            *pszWKT = NULL;
    char            *pszProj4 = NULL;
    const char      *pszAuthName;
    const char      *pszAuthID;
    int             nSRSId;

    if( poSRS == NULL )
        return -1;

    /* -------------------------------------------------------------------- */
    /*  If it is a EPSG	Spatial Reference, search with special type			*/
    /* -------------------------------------------------------------------- */
    pszAuthName = poSRS->GetAuthorityName(NULL);
    pszAuthID = poSRS->GetAuthorityCode(NULL);

    if (pszAuthName && pszAuthID && EQUAL(pszAuthName, "EPSG"))
    {
         sprintf( szCommand, 
             "SELECT srid FROM spatial_ref_sys WHERE auth_name = 'EPSG' and auth_srid= %s",
             pszAuthID );


        OGRIngresStatement  oStateSRID(GetConn());
        oStateSRID.ExecuteSQL(szCommand);

        papszRow = oStateSRID.GetRow();
        if (papszRow == NULL)
        {
            CPLDebug("INGRES", "No rows exists matching EPSG:%s in spatial_ref_sys", pszAuthID );
        }
        else if( papszRow != NULL && papszRow[0] != NULL )
        {
            nSRSId = *((II_INT4 *)papszRow[0]);
            return nSRSId;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate SRS to WKT.                                           */
    /* -------------------------------------------------------------------- */
    if( poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
        return -1;

    /* -------------------------------------------------------------------- */
    /*      Translate SRS to Proj4.                                         */
    /* -------------------------------------------------------------------- */
    if( poSRS->exportToProj4( &pszProj4 ) != OGRERR_NONE )
        return -1;

    CPLAssert( strlen(pszProj4) + strlen(pszWKT) < sizeof(szCommand) - 500 );

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    sprintf( szCommand, 
             "SELECT srid FROM spatial_ref_sys WHERE srtext = '%s'",
             pszWKT );

    {
        OGRIngresStatement  oStateSRID(GetConn());
        oStateSRID.ExecuteSQL(szCommand);

        papszRow = oStateSRID.GetRow();
        if (papszRow == NULL)
        {
            CPLDebug("INGRES", "No rows exist currently exist in spatial_ref_sys");
        }
        else if( papszRow != NULL && papszRow[0] != NULL )
        {
            nSRSId = *((II_INT4 *)papszRow[0]);
            return nSRSId;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    sprintf( szCommand, 
             "SELECT MAX(srid) FROM spatial_ref_sys");    

    {
        OGRIngresStatement  oStateMaxSRID(GetConn());
        oStateMaxSRID.ExecuteSQL(szCommand);
        papszRow = oStateMaxSRID.GetRow();

        // The spatial reference created by user must be greater than
        // the 10000. The below are system maintained.
#define USER_DEFINED_SR_START   10000
        if( papszRow != NULL && papszRow[0] != NULL )
        {
            // if there is no row in spatial reference, a random value 
            // will be return, how to judge?
            nSRSId = *((II_INT4 *)papszRow[0]) ;
            if (nSRSId <= 0)
            {
                nSRSId = USER_DEFINED_SR_START+1; 
            }
            else
            {
                nSRSId = *((II_INT4 *)papszRow[0]) + 1;
            }
             
        }
        else
            nSRSId = USER_DEFINED_SR_START+1;

    }  
    
    if(pszAuthName == NULL || strlen(pszAuthName) == 0)
    {
        poSRS->AutoIdentifyEPSG();
        pszAuthName = poSRS->GetAuthorityName(NULL);
        if (pszAuthName != NULL && EQUAL(pszAuthName, "EPSG"))
        {
            const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                poSRS->importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthName = poSRS->GetAuthorityName(NULL);
                pszAuthID = poSRS->GetAuthorityCode(NULL);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    if(pszAuthName != NULL)
    {
        sprintf( szCommand,
                 "INSERT INTO spatial_ref_sys (srid,auth_name,auth_srid,"
                 "srtext,proj4text) VALUES (%d,'%s',%s,'%s','%s')",
                 nSRSId, pszAuthName, pszAuthID, pszWKT, pszProj4 );
    }
    else
    {
        sprintf( szCommand,
                 "INSERT INTO spatial_ref_sys (srid,auth_name,auth_srid,"
                 "srtext,proj4text) VALUES (%d,NULL,NULL,'%s','%s')",
                 nSRSId, pszWKT, pszProj4 );
    }

    {
        OGRIngresStatement  oStateNewSRID(GetConn());
        if(!oStateNewSRID.ExecuteSQL(szCommand))
        {
            CPLDebug("INGRES", "Failed to create new spatial reference system");
        }
    }

    return nSRSId;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRIngresDataSource::ExecuteSQL( const char *pszSQLCommand,
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

    if( poSpatialFilter != NULL )
    {
        CPLDebug( "OGR_INGRES", 
          "Spatial filter ignored for now in OGRIngresDataSource::ExecuteSQL()" );
    }

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    EstablishActiveLayer( NULL );

    OGRIngresStatement *poStatement = new OGRIngresStatement( hConn );

    if( !poStatement->ExecuteSQL( pszSQLCommand ) )
    {
        delete poStatement;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a tuple result? If so, instantiate a results         */
/*      layer for it.                                                   */
/* -------------------------------------------------------------------- */

    OGRIngresResultLayer *poLayer = NULL;

    poLayer = new OGRIngresResultLayer( this, pszSQLCommand, poStatement );
    EstablishActiveLayer( poLayer );
        
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRIngresDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    if( poActiveLayer == poLayer )
        poActiveLayer = NULL;

    delete poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRIngresDataSource::LaunderName( const char *pszSrcName )

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
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRIngresDataSource::DeleteLayer( int iLayer)

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;
        
/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = papoLayers[iLayer]->GetLayerDefn()->GetName();
    
    CPLDebug( "INGRES", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    char        	szCommand[1024];
    OGRIngresStatement  oStmt( hConn );

    sprintf( szCommand,
             "DROP TABLE %s ",
             osLayerName.c_str() );
    
    if( oStmt.ExecuteSQL( szCommand ) )
    {
        CPLDebug("INGRES","Dropped table %s.", osLayerName.c_str());
        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}



/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRIngresDataSource::CreateLayer( const char * pszLayerNameIn,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    const char          *pszGeometryType = NULL;
    const char		*pszGeomColumnName;
    const char 		*pszExpectedFIDName; 
	
    char                *pszLayerName;
    int                 nDimension = 3; // Ingres only supports 2d currently


    if( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

    if( wkbFlatten(eType) == eType )
        nDimension = 2;

    CPLDebug("INGRES","Creating layer %s.", pszLayerName);

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

/* -------------------------------------------------------------------- */
/*      What do we want to use for geometry and FID columns?            */
/* -------------------------------------------------------------------- */
    pszGeomColumnName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if (!pszGeomColumnName)
        pszGeomColumnName="SHAPE";

    pszExpectedFIDName = CSLFetchNameValue( papszOptions, "INGRES_FID" );
    if (!pszExpectedFIDName)
        pszExpectedFIDName="OGR_FID";

    CPLDebug("INGRES","Geometry Column Name %s.", pszGeomColumnName);
    CPLDebug("INGRES","FID Column Name %s.", pszExpectedFIDName);

/* -------------------------------------------------------------------- */
/*      What sort of geometry column do we want to create?              */
/* -------------------------------------------------------------------- */
    pszGeometryType = CSLFetchNameValue( papszOptions, "GEOMETRY_TYPE" );

    if( pszGeometryType != NULL )
        /* user selected type */;
    
    else if( wkbFlatten(eType) == wkbPoint )
        pszGeometryType = "POINT";

    else if( wkbFlatten(eType) == wkbLineString)
    {
    	if( IsNewIngres() )
    	{
            pszGeometryType = "LINESTRING";
    	}
    	else
    	{
            pszGeometryType = "LONG LINE";
    	}
    }

    else if( wkbFlatten(eType) == wkbPolygon )
    {
    	if( IsNewIngres() )
    	{
            pszGeometryType = "POLYGON";
    	}
    	else
    	{
            pszGeometryType = "LONG POLYGON";
    	}
    }

    else if( wkbFlatten(eType) == wkbMultiPolygon )
    {
    	if( IsNewIngres() )
            pszGeometryType = "MULTIPOLYGON";
    }

    else if( wkbFlatten(eType) == wkbMultiLineString )
    {
    	if( IsNewIngres() )
            pszGeometryType = "MULTILINESTRING";
    }

    else if( wkbFlatten(eType) == wkbMultiPoint )
    {
    	if( IsNewIngres() )
            pszGeometryType = "MULTIPOINT";
    }

    else if( wkbFlatten(eType) == wkbGeometryCollection )
    {
    	if( IsNewIngres() )
            pszGeometryType = "GEOMETRYCOLLECTION";
    }

    else if( wkbFlatten(eType) == wkbUnknown )
    {
    	if( IsNewIngres() )
            // this is also used as the generic geometry type.
            pszGeometryType = "GEOMETRYCOLLECTION";
    }

    /* -------------------------------------------------------------------- */
    /*      Try to get the SRS Id of this spatial reference system,         */
    /*      adding tot the srs table if needed.                             */
    /* -------------------------------------------------------------------- */
    int nSRSId = -1;
    
    if( poSRS != NULL && IsNewIngres() == TRUE )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Form table creation command.                                    */
/* -------------------------------------------------------------------- */
    CPLString osCommand;

    if( pszGeometryType == NULL )
    {
        osCommand.Printf( "CREATE TABLE %s ( "
                          "   %s INTEGER )",
                          pszLayerName, pszExpectedFIDName );
    }
    else
    {
        if(nSRSId != -1)
        {
            osCommand.Printf( "CREATE TABLE %s ("
                              " %s INTEGER NOT NULL PRIMARY KEY GENERATED BY DEFAULT AS seq_%s IDENTITY (START WITH 1 INCREMENT BY 1),"  
                              " %s %s SRID %d ) ",
                              pszLayerName,                              
                              pszExpectedFIDName,
                              pszLayerName,
                              pszGeomColumnName,
                              pszGeometryType,
                              nSRSId);
        }
        else
        {
            osCommand.Printf( "CREATE TABLE %s ("
                              " %s INTEGER NOT NULL PRIMARY KEY GENERATED BY DEFAULT AS seq_%s IDENTITY (START WITH 1 INCREMENT BY 1),"
                              " %s %s )",
                              pszLayerName,                              
                              pszExpectedFIDName,
                              pszLayerName,
                              pszGeomColumnName,
                              pszGeometryType);
        }
    }

/* -------------------------------------------------------------------- */
/*      Execute the create table command.                               */
/* -------------------------------------------------------------------- */
    {
        OGRIngresStatement  oStmt( hConn );

        if( !oStmt.ExecuteSQL( osCommand ) )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRIngresTableLayer     *poLayer;
    OGRErr                  eErr;

    poLayer = new OGRIngresTableLayer( this, pszLayerName, TRUE, nSRSId );
    eErr = poLayer->Initialize(pszLayerName);
    if (eErr == OGRERR_FAILURE)
    {
        delete poLayer;
        return NULL;
    }

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRIngresLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRIngresLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszLayerName );

    return poLayer;
}

/************************************************************************/
/*                        EstablishActiveLayer()                        */
/************************************************************************/

void OGRIngresDataSource::EstablishActiveLayer( OGRIngresLayer *poNewLayer )

{
    if( poActiveLayer != poNewLayer && poActiveLayer != NULL )
        poActiveLayer->ResetReading();

    poActiveLayer = poNewLayer;
}

/************************************************************************/
/*                        IsNewIngres()                                 */
/************************************************************************/

int OGRIngresDataSource::IsNewIngres()
{
	return bNewIngres;
}
