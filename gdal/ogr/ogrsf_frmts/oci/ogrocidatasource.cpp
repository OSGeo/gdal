/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

constexpr int anEPSGOracleMapping[] =
{
    /* Oracle SRID, EPSG GCS/PCS Code */

    8192, 4326, // WGS84
    8306, 4322, // WGS72
    8267, 4269, // NAD83
    8274, 4277, // OSGB 36
         // NAD27 isn't easily mapped since there are many Oracle NAD27 codes.

    81989, 27700, // UK National Grid

    0, 0 // end marker
};

/************************************************************************/
/*                          OGROCIDataSource()                          */
/************************************************************************/

OGROCIDataSource::OGROCIDataSource()

{
    pszName = nullptr;
    pszDBName = nullptr;
    papoLayers = nullptr;
    nLayers = 0;
    bDSUpdate = FALSE;
    bNoLogging = FALSE;
    poSession = nullptr;
    papoSRS = nullptr;
    panSRID = nullptr;
    nKnownSRID = 0;
}

/************************************************************************/
/*                         ~OGROCIDataSource()                          */
/************************************************************************/

OGROCIDataSource::~OGROCIDataSource()

{
    int         i;

    CPLFree( pszName );
    CPLFree( pszDBName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    for( i = 0; i < nKnownSRID; i++ )
    {
        papoSRS[i]->Release();
    }
    CPLFree( papoSRS );
    CPLFree( panSRID );

    if( poSession != nullptr )
        delete poSession;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROCIDataSource::Open( const char * pszNewName,
                            char** papszOpenOptionsIn,
                            int bUpdate,
                            int bTestOpen )

{
    CPLAssert( nLayers == 0 && poSession == nullptr );

/* -------------------------------------------------------------------- */
/*      Verify Oracle prefix.                                           */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(pszNewName,"OCI:") )
    {
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to Oracle OCI driver naming convention,"
                      " OCI:*\n", pszNewName );
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to parse out name, password and database name.              */
/* -------------------------------------------------------------------- */
    char *pszUserid;
    const char *pszPassword = "";
    const char *pszDatabase = "";
    char **papszTableList = nullptr;
    const char *pszWorkspace = "";

    int   i;

    if( pszNewName[4] == '\0' )
    {
        pszUserid = CPLStrdup(CSLFetchNameValueDef(papszOpenOptionsIn, "USER", ""));
        pszPassword = CSLFetchNameValueDef(papszOpenOptionsIn, "PASSWORD", "");
        pszDatabase = CSLFetchNameValueDef(papszOpenOptionsIn, "DBNAME", "");
        const char* pszTables = CSLFetchNameValue(papszOpenOptionsIn, "TABLES");
        if( pszTables )
            papszTableList = CSLTokenizeStringComplex(pszTables, ",", TRUE, FALSE );
        pszWorkspace = CSLFetchNameValueDef(papszOpenOptionsIn, "WORKSPACE", "");
    }
    else
    {
        pszUserid = CPLStrdup( pszNewName + 4 );

        // Is there a table list?
        for( i = static_cast<int>(strlen(pszUserid))-1; i > 1; i-- )
        {
            if( pszUserid[i] == ':' )
            {
                papszTableList = CSLTokenizeStringComplex( pszUserid+i+1, ",",
                                                        TRUE, FALSE );
                pszUserid[i] = '\0';
                break;
            }

            if( pszUserid[i] == '/' || pszUserid[i] == '@' )
                break;
        }

        for( i = 0;
            pszUserid[i] != '\0' && pszUserid[i] != '/' && pszUserid[i] != '@';
            i++ ) {}

        if( pszUserid[i] == '/' )
        {
            pszUserid[i++] = '\0';
            pszPassword = pszUserid + i;
            for( ; pszUserid[i] != '\0' && pszUserid[i] != '@'; i++ ) {}
        }

        if( pszUserid[i] == '@' )
        {
            pszUserid[i++] = '\0';
            pszDatabase = pszUserid + i;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */

    poSession = OGRGetOCISession( pszUserid, pszPassword, pszDatabase );

    if( poSession == nullptr )
    {
        CPLFree(pszUserid);
        CSLDestroy(papszTableList);
        return FALSE;
    }

    if( ! EQUAL(pszWorkspace, "") )
    {
        OGROCIStringBuf oValidateCmd;
        OGROCIStatement oValidateStmt( GetSession() );

        oValidateCmd.Append( "call DBMS_WM.GotoWorkspace('" );
        oValidateCmd.Append( pszWorkspace );
        oValidateCmd.Append( "')" );

        oValidateStmt.Execute( oValidateCmd.GetString() );
    }

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      If no list of target tables was provided, collect a list of     */
/*      spatial tables now.                                             */
/* -------------------------------------------------------------------- */
    if( papszTableList == nullptr )
    {
        OGROCIStatement oGetTables( poSession );

        if( oGetTables.Execute(
            "SELECT TABLE_NAME, OWNER FROM ALL_SDO_GEOM_METADATA" )
            == CE_None )
        {
            char **papszRow;

            while( (papszRow = oGetTables.SimpleFetchRow()) != nullptr )
            {
                char szFullTableName[100];

                if( EQUAL(papszRow[1],pszUserid) )
                    strcpy( szFullTableName, papszRow[0] );
                else
                    snprintf( szFullTableName, sizeof(szFullTableName), "%s.%s",
                             papszRow[1], papszRow[0] );

                if( CSLFindString( papszTableList, szFullTableName ) == -1 )
                    papszTableList = CSLAddString( papszTableList,
                                                   szFullTableName );
            }
        }
    }
    CPLFree( pszUserid );

/* -------------------------------------------------------------------- */
/*      Open all the selected tables or views.                          */
/* -------------------------------------------------------------------- */
    for( i = 0; papszTableList != nullptr && papszTableList[i] != nullptr; i++ )
    {
        OpenTable( papszTableList[i], -1, bUpdate, FALSE, papszOpenOptionsIn );
    }

    CSLDestroy( papszTableList );

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGROCIDataSource::OpenTable( const char *pszNewName,
                                 int nSRID, int bUpdate, CPL_UNUSED int bTestOpen,
                                 char** papszOpenOptionsIn )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGROCITableLayer    *poLayer;

    poLayer = new OGROCITableLayer( this, pszNewName, wkbUnknown, nSRID,
                                    bUpdate, FALSE );

    if( !poLayer->IsValid() )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGROCILayer **)
        CPLRealloc( papoLayers,  sizeof(OGROCILayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    poLayer->SetOptions( papszOpenOptionsIn );

    return TRUE;
}

/************************************************************************/
/*                           ValidateLayer()                            */
/************************************************************************/

void OGROCIDataSource::ValidateLayer( const char *pszLayerName )

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
                  "ValidateLayer(): %s is not a recognised layer.",
                  pszLayerName );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Verify we have an FID and geometry column for this table.       */
/* -------------------------------------------------------------------- */
    OGROCITableLayer *poLayer = (OGROCITableLayer *) papoLayers[iLayer];

    if( strlen(poLayer->GetFIDColumn()) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ValidateLayer(): %s lacks a fid column.",
                  pszLayerName );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Prepare and execute the geometry validation.                    */
/* -------------------------------------------------------------------- */

    if( strlen(poLayer->GetGeometryColumn()) != 0 )
    {
        OGROCIStringBuf oValidateCmd;
        OGROCIStatement oValidateStmt( GetSession() );

        oValidateCmd.Append( "SELECT c." );
        oValidateCmd.Append( poLayer->GetFIDColumn() );
        oValidateCmd.Append( ", SDO_GEOM.VALIDATE_GEOMETRY(c." );
        oValidateCmd.Append( poLayer->GetGeometryColumn() );
        oValidateCmd.Append( ", m.diminfo) from " );
        oValidateCmd.Append( poLayer->GetLayerDefn()->GetName() );
        oValidateCmd.Append( " c, user_sdo_geom_metadata m WHERE m.table_name= '");
        oValidateCmd.Append( poLayer->GetLayerDefn()->GetName() );
        oValidateCmd.Append( "' AND m.column_name = '" );
        oValidateCmd.Append( poLayer->GetGeometryColumn() );
        oValidateCmd.Append( "' AND SDO_GEOM.VALIDATE_GEOMETRY(c." );
        oValidateCmd.Append( poLayer->GetGeometryColumn() );
        oValidateCmd.Append( ", m.diminfo ) <> 'TRUE'" );

        oValidateStmt.Execute( oValidateCmd.GetString() );

/* -------------------------------------------------------------------- */
/*      Report results to debug stream.                                 */
/* -------------------------------------------------------------------- */
        char **papszRow;

        while( (papszRow = oValidateStmt.SimpleFetchRow()) != nullptr )
        {
            const char *pszReason = papszRow[1];

            if( EQUAL(pszReason,"13011") )
                pszReason = "13011: value is out of range";
            else if( EQUAL(pszReason,"13050") )
                pszReason = "13050: unable to construct spatial object";
            else if( EQUAL(pszReason,"13349") )
                pszReason = "13349: polygon boundary crosses itself";

            CPLDebug( "OCI", "Validation failure for FID=%s: %s",
                  papszRow[0], pszReason );
        }
    }
}

/************************************************************************/
/*                           DeleteLayer(int)                           */
/************************************************************************/

OGRErr OGROCIDataSource::DeleteLayer( int iLayer )

{
/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName =
        papoLayers[iLayer]->GetLayerDefn()->GetName();

    CPLDebug( "OCI", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    OGROCIStatement oCommand( poSession );
    CPLString       osCommand;
    int             nFailures = 0;

    osCommand.Printf( "DROP TABLE \"%s\"", osLayerName.c_str() );
    if( oCommand.Execute( osCommand ) != CE_None )
        nFailures++;

    osCommand.Printf(
        "DELETE FROM USER_SDO_GEOM_METADATA WHERE TABLE_NAME = UPPER('%s')",
        osLayerName.c_str() );

    if( oCommand.Execute( osCommand ) != CE_None )
        nFailures++;

    if( nFailures == 0 )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                      DeleteLayer(const char *)                       */
/************************************************************************/

void OGROCIDataSource::DeleteLayer( const char *pszLayerName )

{
    int iLayer;

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            break;
        }
    }

    if( iLayer == nLayers )
    {
        CPLDebug( "OCI", "DeleteLayer: %s not found in layer list." \
                  "  Layer *not* deleted.", pszLayerName );
        return;
    }

    DeleteLayer( iLayer );
}

/************************************************************************/
/*                           TruncateLayer()                            */
/************************************************************************/

void OGROCIDataSource::TruncateLayer( const char *pszLayerName )

{

/* -------------------------------------------------------------------- */
/*      Set OGR Debug statement explaining what is happening            */
/* -------------------------------------------------------------------- */
    CPLDebug( "OCI", "Truncate TABLE %s", pszLayerName );

/* -------------------------------------------------------------------- */
/*      Truncate the layer in the database.                             */
/* -------------------------------------------------------------------- */
    OGROCIStatement oCommand( poSession );
    CPLString       osCommand;

    osCommand.Printf( "TRUNCATE TABLE \"%s\"", pszLayerName );
    oCommand.Execute( osCommand );
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGROCIDataSource::ICreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )

{
    char               *pszSafeLayerName = CPLStrdup(pszLayerName);

    poSession->CleanName( pszSafeLayerName );
    CPLDebug( "OCI", "In Create Layer ..." );

    bNoLogging = CPLFetchBool( papszOptions, "NO_LOGGING", false );

/* -------------------------------------------------------------------- */
/*      Get the default string size                                     */
/* -------------------------------------------------------------------- */

    int nDefaultStringSize = DEFAULT_STRING_SIZE;

    if (CSLFetchNameValue( papszOptions, "DEFAULT_STRING_SIZE" ) != nullptr)
    {
        nDefaultStringSize = atoi( 
            CSLFetchNameValue( papszOptions, "DEFAULT_STRING_SIZE" ) );
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    if( CPLFetchBool( papszOptions, "TRUNCATE", false ) )
    {
        CPLDebug( "OCI", "Calling TruncateLayer for %s", pszLayerName );
        TruncateLayer( pszSafeLayerName );
    }
    else
    {
        for( iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( EQUAL(pszSafeLayerName,
                      papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            {
                if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != nullptr
                    && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
                {
                    DeleteLayer( pszSafeLayerName );
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Layer %s already exists, CreateLayer failed.\n"
                              "Use the layer creation option OVERWRITE=YES to "
                              "replace it.",
                              pszSafeLayerName );
                    CPLFree( pszSafeLayerName );
                    return nullptr;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    char szSRSId[100];

    const char* pszSRID = CSLFetchNameValue( papszOptions, "SRID" );
    if( pszSRID != nullptr )
        snprintf( szSRSId, sizeof(szSRSId), "%s", pszSRID );
    else if( poSRS != nullptr )
        snprintf( szSRSId, sizeof(szSRSId), "%d", FetchSRSId( poSRS ) );
    else
        strcpy( szSRSId, "NULL" );

/* -------------------------------------------------------------------- */
/*      Determine name of geometry column to use.                       */
/* -------------------------------------------------------------------- */
    const char *pszGeometryName =
        CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if( pszGeometryName == nullptr )
        pszGeometryName = "ORA_GEOMETRY";
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);

/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    const char *pszExpectedFIDName =
        CPLGetConfigOption( "OCI_FID", "OGR_FID" );

    OGROCIStatement oStatement( poSession );

/* -------------------------------------------------------------------- */
/*      If geometry type is wkbNone, do not create a geometry column.   */
/* -------------------------------------------------------------------- */

    CPLString osCommand;
    if ( CSLFetchNameValue( papszOptions, "TRUNCATE" ) == nullptr  )
    {
        if (eType == wkbNone)
        {
            osCommand.Printf(
                     "CREATE TABLE \"%s\" ( "
                     "%s INTEGER PRIMARY KEY)",
                     pszSafeLayerName, pszExpectedFIDName);
        }
        else
        {
            osCommand.Printf(
                     "CREATE TABLE \"%s\" ( "
                     "%s INTEGER PRIMARY KEY, "
                     "%s %s%s )",
                     pszSafeLayerName, pszExpectedFIDName,
                     pszGeometryName, SDO_GEOMETRY,
                     (!bGeomNullable) ? " NOT NULL":"");
        }

        if (bNoLogging)
        {
            osCommand += CPLSPrintf(" NOLOGGING "
              "VARRAY %s.SDO_ELEM_INFO STORE AS SECUREFILE LOB (NOCACHE NOLOGGING) "
              "VARRAY %s.SDO_ORDINATES STORE AS SECUREFILE LOB (NOCACHE NOLOGGING) ",
              pszGeometryName, pszGeometryName);
        }

        if( oStatement.Execute( osCommand ) != CE_None )
        {
            CPLFree( pszSafeLayerName );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    const char *pszLoaderFile = CSLFetchNameValue(papszOptions,"LOADER_FILE");
    OGROCIWritableLayer *poLayer;

    if( pszLoaderFile == nullptr )
        poLayer = new OGROCITableLayer( this, pszSafeLayerName, eType,
                                        EQUAL(szSRSId,"NULL") ? -1 : atoi(szSRSId),
                                        TRUE, TRUE );
    else
        poLayer =
            new OGROCILoaderLayer( this, pszSafeLayerName,
                                   pszGeometryName,
                                   EQUAL(szSRSId,"NULL") ? -1 : atoi(szSRSId),
                                   pszLoaderFile );

/* -------------------------------------------------------------------- */
/*      Set various options on the layer.                               */
/* -------------------------------------------------------------------- */
    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", false) );
    poLayer->SetPrecisionFlag( CPLFetchBool(papszOptions, "PRECISION", true));
    poLayer->SetDefaultStringSize( nDefaultStringSize );

    const char* pszDIM = CSLFetchNameValue(papszOptions,"DIM");
    if( pszDIM != nullptr )
        poLayer->SetDimension( atoi(pszDIM) );
    else if( eType != wkbNone )
        poLayer->SetDimension( (wkbFlatten(eType) == eType) ? 2 : 3 );

    poLayer->SetOptions( papszOptions );
    if( eType != wkbNone && !bGeomNullable )
        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetNullable(FALSE);

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGROCILayer **)
        CPLRealloc( papoLayers,  sizeof(OGROCILayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszSafeLayerName );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCIDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) && bDSUpdate )
        return TRUE;
    else if( EQUAL(pszCap,ODsCDeleteLayer) && bDSUpdate )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bDSUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROCIDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGROCIDataSource::ExecuteSQL( const char *pszSQLCommand,
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
/*      Ensure any pending stuff is flushed to the database.            */
/* -------------------------------------------------------------------- */
    FlushCache();

    CPLDebug( "OCI", "ExecuteSQL(%s)", pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        DeleteLayer( pszLayerName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case VALLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "VALLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        ValidateLayer( pszLayerName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Just execute simple command.                                    */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(pszSQLCommand, "SELECT") )
    {
        OGROCIStatement oCommand( poSession );

        oCommand.Execute( pszSQLCommand, OCI_COMMIT_ON_SUCCESS );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise instantiate a layer.                                  */
/* -------------------------------------------------------------------- */
    else
    {
        OGROCIStatement oCommand( poSession );

        if( oCommand.Execute( pszSQLCommand, OCI_DESCRIBE_ONLY ) == CE_None )
            return new OGROCISelectLayer( this, pszSQLCommand, &oCommand );
        else
            return nullptr;
    }
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGROCIDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGROCIDataSource::FetchSRS( int nId )

{
    if( nId < 0 )
        return nullptr;

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
/*      Try looking up in MDSYS.CS_SRS table.                           */
/* -------------------------------------------------------------------- */
    OGROCIStatement oStatement( GetSession() );
    char            szSelect[200], **papszResult;

    snprintf( szSelect, sizeof(szSelect),
             "SELECT WKTEXT, AUTH_SRID, AUTH_NAME FROM MDSYS.CS_SRS "
             "WHERE SRID = %d AND WKTEXT IS NOT NULL", nId );

    if( oStatement.Execute( szSelect ) != CE_None )
        return nullptr;

    papszResult = oStatement.SimpleFetchRow();
    if( CSLCount(papszResult) < 1 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Turn into a spatial reference.                                  */
/* -------------------------------------------------------------------- */
    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( poSRS->importFromWkt( papszResult[0] ) != OGRERR_NONE )
    {
        delete poSRS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If we have a corresponding EPSG code for this SRID, use that    */
/*      authority.                                                      */
/* -------------------------------------------------------------------- */
    int bGotEPSGMapping = FALSE;
    for( i = 0; anEPSGOracleMapping[i] != 0; i += 2 )
    {
        if( anEPSGOracleMapping[i] == nId )
        {
            poSRS->SetAuthority( poSRS->GetRoot()->GetValue(), "EPSG",
                                 anEPSGOracleMapping[i+1] );
            bGotEPSGMapping = TRUE;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Insert authority information, if it is available.               */
/* -------------------------------------------------------------------- */
    if( papszResult[1] != nullptr && atoi(papszResult[1]) != 0
        && papszResult[2] != nullptr && strlen(papszResult[1]) != 0
        && poSRS->GetRoot() != nullptr
        && !bGotEPSGMapping )
    {
        poSRS->SetAuthority( poSRS->GetRoot()->GetValue(),
                             papszResult[2], atoi(papszResult[1]) );
    }

/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    panSRID = (int *) CPLRealloc(panSRID,sizeof(int) * (nKnownSRID+1) );
    papoSRS = (OGRSpatialReference **)
        CPLRealloc(papoSRS, sizeof(void*) * (nKnownSRID + 1) );
    panSRID[nKnownSRID] = nId;
    papoSRS[nKnownSRID] = poSRS;

    nKnownSRID++;

    return poSRS;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGROCIDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    char                *pszWKT = nullptr;
    int                 nSRSId;

    if( poSRS == nullptr )
        return -1;

    if( !poSRS->IsProjected() && !poSRS->IsGeographic() )
        return -1;

/* ==================================================================== */
/*      The first strategy is to see if we can identify it by           */
/*      authority information within the SRS.  Either using ORACLE      */
/*      authority values directly, or check if there is a known         */
/*      translation for an EPSG authority code.                         */
/* ==================================================================== */
    const char *pszAuthName = nullptr, *pszAuthCode = nullptr;

    if( poSRS->IsGeographic() )
    {
        pszAuthName = poSRS->GetAuthorityName( "GEOGCS" );
        pszAuthCode = poSRS->GetAuthorityCode( "GEOGCS" );
    }
    else if( poSRS->IsProjected() )
    {
        pszAuthName = poSRS->GetAuthorityName( "PROJCS" );
        pszAuthCode = poSRS->GetAuthorityCode( "PROJCS" );
    }

    if( pszAuthName != nullptr && pszAuthCode != nullptr )
    {
        if( EQUAL(pszAuthName,"Oracle")
            && atoi(pszAuthCode) != 0 )
            return atoi(pszAuthCode);

        if( EQUAL(pszAuthName,"EPSG") )
        {
            int i, nEPSGCode = atoi(pszAuthCode);

            for( i = 0; anEPSGOracleMapping[i] != 0; i += 2 )
            {
                if( nEPSGCode == anEPSGOracleMapping[i+1] )
                    return anEPSGOracleMapping[i];
            }
        }
    }

/* ==================================================================== */
/*      We need to lookup the SRS in the existing Oracle CS_SRS         */
/*      table.                                                          */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Convert SRS into old style format (SF-SQL 1.0).                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS2 = poSRS->Clone();

/* -------------------------------------------------------------------- */
/*      Convert any degree type unit names to "Decimal Degree".         */
/* -------------------------------------------------------------------- */
    double dfAngularUnits = poSRS2->GetAngularUnits( nullptr );
    if( fabs(dfAngularUnits - 0.0174532925199433) < 0.0000000000000010 )
        poSRS2->SetAngularUnits( "Decimal Degree", 0.0174532925199433 );

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    const char* const apszOptions[] = { "FORMAT=SFSQL", nullptr };
    if( poSRS2->exportToWkt( &pszWKT, apszOptions ) != OGRERR_NONE )
    {
        delete poSRS2;
        CPLFree(pszWKT);
        return -1;
    }

    delete poSRS2;

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf     oCmdText;
    OGROCIStatement     oCmdStatement( GetSession() );
    char                **papszResult = nullptr;

    oCmdText.Append( "SELECT SRID FROM MDSYS.CS_SRS WHERE WKTEXT = '" );
    oCmdText.Append( pszWKT );
    oCmdText.Append( "'" );

    if( oCmdStatement.Execute( oCmdText.GetString() ) == CE_None )
        papszResult = oCmdStatement.SimpleFetchRow() ;

/* -------------------------------------------------------------------- */
/*      We got it!  Return it.                                          */
/* -------------------------------------------------------------------- */
    if( papszResult != nullptr && papszResult[0] != nullptr && papszResult[1] == nullptr )
    {
        CPLFree( pszWKT );
        return atoi( papszResult[0] );
    }

/* ==================================================================== */
/*      We didn't find it, so we need to define it as a new SRID at     */
/*      the end of the list of known values.                            */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    if( oCmdStatement.Execute("SELECT MAX(SRID) FROM MDSYS.CS_SRS") == CE_None )
        papszResult = oCmdStatement.SimpleFetchRow();
    else
        papszResult = nullptr;

    if( papszResult != nullptr && papszResult[0] != nullptr && papszResult[1] == nullptr )
        nSRSId = atoi(papszResult[0]) + 1;
    else
        nSRSId = 1;

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    oCmdText.Clear();
    oCmdText.Append( "INSERT INTO MDSYS.CS_SRS (SRID, WKTEXT, CS_NAME) " );
    oCmdText.Appendf( 100, " VALUES (%d,'", nSRSId );
    oCmdText.Append( pszWKT );
    oCmdText.Append( "', '" );
    oCmdText.Append( poSRS->GetRoot()->GetChild(0)->GetValue() );
    oCmdText.Append( "' )" );

    CPLFree( pszWKT );

    if( oCmdStatement.Execute( oCmdText.GetString() ) != CE_None )
        return -1;
    else
        return nSRSId;
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGROCIDataSource::GetLayerByName( const char *pszNameIn )

{
    OGROCILayer *poLayer = nullptr;
    int  i, count;

    if ( !pszNameIn )
        return nullptr;

    count = GetLayerCount();

    /* first a case sensitive check */
    for( i = 0; i < count; i++ )
    {
        poLayer = papoLayers[i];

        if( strcmp( pszNameIn, poLayer->GetName() ) == 0 )
        {
            return poLayer;
        }
    }

    char *pszSafeLayerName = CPLStrdup( pszNameIn );
    poSession->CleanName( pszSafeLayerName );

    /* then case insensitive and laundered */
    for( i = 0; i < count; i++ )
    {
        poLayer = papoLayers[i];

        if( EQUAL( pszSafeLayerName, poLayer->GetName() ) )
        {
            break;
        }
    }

    CPLFree( pszSafeLayerName );

    return i < count ? poLayer : nullptr;
}
