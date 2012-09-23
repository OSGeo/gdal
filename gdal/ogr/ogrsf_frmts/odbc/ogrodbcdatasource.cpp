/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_odbc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                         OGRODBCDataSource()                          */
/************************************************************************/

OGRODBCDataSource::OGRODBCDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;
}

/************************************************************************/
/*                         ~OGRODBCDataSource()                         */
/************************************************************************/

OGRODBCDataSource::~OGRODBCDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    for( i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
}

/************************************************************************/
/*                              OpenMDB()                               */
/************************************************************************/

int OGRODBCDataSource::OpenMDB( const char * pszNewName, int bUpdate )
{
    const char *pszDSNStringTemplate =
        "DRIVER=Microsoft Access Driver (*.mdb);DBQ=%s";
    char* pszDSN = (char *) CPLMalloc(strlen(pszNewName)+strlen(pszDSNStringTemplate)+100);
    sprintf( pszDSN, pszDSNStringTemplate,  pszNewName );

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "ODBC", "EstablishSession(%s)", pszDSN );

    if( !oSession.EstablishSession( pszDSN, NULL, NULL ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to initialize ODBC connection to DSN for %s,\n"
                  "%s", pszDSN, oSession.GetLastError() );
        CPLFree( pszDSN );
        return FALSE;
    }

    CPLFree( pszDSN );

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Check if it is a PGeo MDB.                                      */
/* -------------------------------------------------------------------- */
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT TableName, FieldName, ShapeType, ExtentLeft, ExtentRight, ExtentBottom, ExtentTop, SRID, HasZ FROM GDB_GeomColumns" );

        if( oStmt.ExecuteSQL() )
        {
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if it is a Geomedia MDB.                                  */
/* -------------------------------------------------------------------- */
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT TableName FROM GAliasTable WHERE TableType = 'INGRFeatures'" );

        if( oStmt.ExecuteSQL() )
        {
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return all tables as  non-spatial tables.                       */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oTableList( &oSession );

    if( oTableList.GetTables() )
    {
        while( oTableList.Fetch() )
        {
            const char *pszSchema = oTableList.GetColData(1);
            CPLString osLayerName;

            if( pszSchema != NULL && strlen(pszSchema) > 0 )
            {
                osLayerName = pszSchema;
                osLayerName += ".";
            }

            osLayerName += oTableList.GetColData(2);

            OpenTable( osLayerName, NULL, bUpdate );
        }

        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRODBCDataSource::Open( const char * pszNewName, int bUpdate,
                             int bTestOpen )

{
    CPLAssert( nLayers == 0 );

    if( !EQUALN(pszNewName, "ODBC:",5) && EQUAL(CPLGetExtension(pszNewName), "MDB") )
        return OpenMDB(pszNewName, bUpdate);

/* -------------------------------------------------------------------- */
/*      Start parsing dataset name from the end of string, fetching     */
/*      the name of spatial reference table and names for SRID and      */
/*      SRTEXT columns first.                                           */
/* -------------------------------------------------------------------- */
    char *pszWrkName = CPLStrdup( pszNewName + 5 ); // Skip the 'ODBC:' part
    char **papszTables = NULL;
    char **papszGeomCol = NULL;
    char *pszSRSTableName = NULL;
    char *pszSRIDCol = NULL, *pszSRTextCol = NULL;
    char *pszDelimiter;

    if ( (pszDelimiter = strrchr( pszWrkName, ':' )) != NULL )
    {
        char *pszOBracket = strchr( pszDelimiter + 1, '(' );

        if( strchr(pszDelimiter,'\\') != NULL
            || strchr(pszDelimiter,'/') != NULL )
        {
            /*
            ** if there are special tokens then this isn't really
            ** the srs table name, so avoid further processing.
            */
        }
        else if( pszOBracket == NULL )
        {
            pszSRSTableName = CPLStrdup( pszDelimiter + 1 );
            *pszDelimiter = '\0';
        }
        else
        {
            char *pszCBracket = strchr( pszOBracket, ')' );
            if( pszCBracket != NULL )
                *pszCBracket = '\0';

            char *pszComma = strchr( pszOBracket, ',' );
            if( pszComma != NULL )
            {
                *pszComma = '\0';
                pszSRIDCol = CPLStrdup( pszComma + 1 );
            }
            
            *pszOBracket = '\0';
            pszSRSTableName = CPLStrdup( pszDelimiter + 1 );
            pszSRTextCol = CPLStrdup( pszOBracket + 1 );

            *pszDelimiter = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      Strip off any comma delimeted set of tables names to access     */
/*      from the end of the string first.  Also allow an optional       */
/*      bracketed geometry column name after the table name.            */
/* -------------------------------------------------------------------- */
    while( (pszDelimiter = strrchr( pszWrkName, ',' )) != NULL )
    {
        char *pszOBracket = strstr( pszDelimiter + 1, "(" );
        if( pszOBracket == NULL )
        {
            papszTables = CSLAddString( papszTables, pszDelimiter + 1 );
            papszGeomCol = CSLAddString( papszGeomCol, "" );
        }
        else
        {
            char *pszCBracket = strstr(pszOBracket,")");
            
            if( pszCBracket != NULL )
                *pszCBracket = '\0';
            
            *pszOBracket = '\0';
            papszTables = CSLAddString( papszTables, pszDelimiter + 1 );
            papszGeomCol = CSLAddString( papszGeomCol, pszOBracket+1 );
        }
        *pszDelimiter = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Split out userid, password and DSN.  The general form is        */
/*      user/password@dsn.  But if there are no @ characters the        */
/*      whole thing is assumed to be a DSN.                             */
/* -------------------------------------------------------------------- */
    char *pszUserid = NULL;
    char *pszPassword = NULL;
    char *pszDSN = NULL;

    if( strstr(pszWrkName,"@") == NULL )
    {
        pszDSN = CPLStrdup( pszWrkName );
    }
    else
    {
        char *pszTarget;

        pszDSN = CPLStrdup(strstr(pszWrkName, "@") + 1);
        if( *pszWrkName == '/' )
        {
            pszPassword = CPLStrdup(pszWrkName + 1);
            pszTarget = strstr(pszPassword,"@");
            *pszTarget = '\0';
        }
        else
        {
            pszUserid = CPLStrdup(pszWrkName);
            pszTarget = strstr(pszUserid,"@");
            *pszTarget = '\0';

            pszTarget = strstr(pszUserid,"/");
            if( pszTarget != NULL )
            {
                *pszTarget = '\0';
                pszPassword = CPLStrdup(pszTarget+1);
            }
        }
    }

    CPLFree( pszWrkName );

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_ODBC",
              "EstablishSession(DSN:\"%s\", userid:\"%s\", password:\"%s\")", 
              pszDSN, pszUserid ? pszUserid : "",
              pszPassword ? pszPassword : "" );

    if( !oSession.EstablishSession( pszDSN, pszUserid, pszPassword ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to initialize ODBC connection to DSN for %s,\n"
                  "%s", 
                  pszNewName+5, oSession.GetLastError() );
        CSLDestroy( papszTables );
        CSLDestroy( papszGeomCol );
        CPLFree( pszDSN );
        CPLFree( pszUserid );
        CPLFree( pszPassword );
        return FALSE;
    }

    CPLFree( pszDSN );
    CPLFree( pszUserid );
    CPLFree( pszPassword );

    pszName = CPLStrdup( pszNewName );
    
    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      If no explicit list of tables was given, check for a list in    */
/*      a geometry_columns table.                                       */
/* -------------------------------------------------------------------- */
    if( papszTables == NULL )
    {
        CPLODBCStatement oStmt( &oSession );
        
        oStmt.Append( "SELECT f_table_name, f_geometry_column, geometry_type"
                      " FROM geometry_columns" );
        if( oStmt.ExecuteSQL() )
        {
            while( oStmt.Fetch() )
            {
                papszTables = 
                    CSLAddString( papszTables, oStmt.GetColData(0) );
                papszGeomCol = 
                    CSLAddString( papszGeomCol, oStmt.GetColData(1) );
            }
        }
    }
            
/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables as           */
/*      non-spatial tables.                                             */
/* -------------------------------------------------------------------- */
    if( papszTables == NULL )
    {
        CPLODBCStatement oTableList( &oSession );
        
        if( oTableList.GetTables() )
        {
            while( oTableList.Fetch() )
            {
                const char *pszSchema = oTableList.GetColData(1);
                CPLString osLayerName;

                if( pszSchema != NULL && strlen(pszSchema) > 0 )
                {
                    osLayerName = pszSchema;
                    osLayerName += ".";
                }

                osLayerName += oTableList.GetColData(2);

                papszTables = CSLAddString( papszTables, osLayerName );

                papszGeomCol = CSLAddString(papszGeomCol,"");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have an explicit list of requested tables, use them       */
/*      (non-spatial).                                                  */
/* -------------------------------------------------------------------- */
    for( int iTable = 0; 
         papszTables != NULL && papszTables[iTable] != NULL; 
         iTable++ )
    {
        if( strlen(papszGeomCol[iTable]) > 0 )
            OpenTable( papszTables[iTable], papszGeomCol[iTable], bUpdate );
        else
            OpenTable( papszTables[iTable], NULL, bUpdate );
    }

    CSLDestroy( papszTables );
    CSLDestroy( papszGeomCol );

/* -------------------------------------------------------------------- */
/*      If no explicit list of tables was given, check for a list in    */
/*      a geometry_columns table.                                       */
/* -------------------------------------------------------------------- */
    if ( pszSRSTableName )
    {
        CPLODBCStatement oSRSList( &oSession );

        if ( !pszSRTextCol )
            pszSRTextCol = CPLStrdup( "srtext" );
        if ( !pszSRIDCol )
            pszSRIDCol = CPLStrdup( "srid" );

        oSRSList.Append( "SELECT " );
        oSRSList.Append( pszSRIDCol );
        oSRSList.Append( "," );
        oSRSList.Append( pszSRTextCol );
        oSRSList.Append( " FROM " );
        oSRSList.Append( pszSRSTableName );

        CPLDebug( "OGR_ODBC", "ExecuteSQL(%s) to read SRS table",
                  oSRSList.GetCommand() );
        if ( oSRSList.ExecuteSQL() )
        {
            int nRows = 256;     // A reasonable number of SRIDs to start from
            panSRID = (int *)CPLMalloc( nRows * sizeof(int) );
            papoSRS = (OGRSpatialReference **)
                CPLMalloc( nRows * sizeof(OGRSpatialReference*) );

            while ( oSRSList.Fetch() )
            {
                char *pszSRID = (char *) oSRSList.GetColData( pszSRIDCol );
                if ( !pszSRID )
                    continue;

                char *pszSRText = (char *) oSRSList.GetColData( pszSRTextCol );

                if ( pszSRText )
                {
                    if ( nKnownSRID > nRows )
                    {
                        nRows *= 2;
                        panSRID = (int *)CPLRealloc( panSRID,
                                                     nRows * sizeof(int) );
                        papoSRS = (OGRSpatialReference **)
                            CPLRealloc( papoSRS,
                            nRows * sizeof(OGRSpatialReference*) );
                    }
                    panSRID[nKnownSRID] = atoi( pszSRID );
                    papoSRS[nKnownSRID] = new OGRSpatialReference();
                    if ( papoSRS[nKnownSRID]->importFromWkt( &pszSRText )
                         != OGRERR_NONE )
                    {
                        delete papoSRS[nKnownSRID];
                        continue;
                    }
                    nKnownSRID++;
                }
            }
        }
    }

    if ( pszSRIDCol )
        CPLFree( pszSRIDCol );
    if ( pszSRTextCol )
        CPLFree( pszSRTextCol );
    if ( pszSRSTableName )
        CPLFree( pszSRSTableName );

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRODBCDataSource::OpenTable( const char *pszNewName, 
                                  const char *pszGeomCol,
                                  int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRODBCTableLayer  *poLayer;

    poLayer = new OGRODBCTableLayer( this );

    if( poLayer->Initialize( pszNewName, pszGeomCol ) )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRODBCLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRODBCLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRODBCDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}
/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRODBCDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic imlplementation for OGRSQL dialect.                 */
/* -------------------------------------------------------------------- */
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Execute statement.                                              */
/* -------------------------------------------------------------------- */
    CPLODBCStatement *poStmt = new CPLODBCStatement( &oSession );

    CPLDebug( "ODBC", "ExecuteSQL(%s) called.", pszSQLCommand );
    poStmt->Append( pszSQLCommand );
    if( !poStmt->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", oSession.GetLastError() );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poStmt->GetColCount() == 0 )
    {
        delete poStmt;
        CPLErrorReset();
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */
    OGRODBCSelectLayer *poLayer = NULL;
        
    poLayer = new OGRODBCSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRODBCDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
