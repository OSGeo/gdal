/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLSpatialDataSource class..
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRMSSQLSpatialDataSource()                 */
/************************************************************************/

OGRMSSQLSpatialDataSource::OGRMSSQLSpatialDataSource()

{
    pszName = NULL;
    pszCatalog = NULL;
    papoLayers = NULL;
    nLayers = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    nGeometryFormat = MSSQLGEOMETRY_NATIVE;
}

/************************************************************************/
/*                         ~OGRMSSQLSpatialDataSource()                 */
/************************************************************************/

OGRMSSQLSpatialDataSource::~OGRMSSQLSpatialDataSource()

{
    int         i;

    CPLFree( pszName );
    CPLFree( pszCatalog );

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
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMSSQLSpatialDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRMSSQLSpatialDataSource::DeleteLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    const char* pszLayerName = papoLayers[iLayer]->GetTableName();
    const char* pszSchemaName = papoLayers[iLayer]->GetSchemaName();

    CPLODBCStatement oStmt( &oSession );
    oStmt.Appendf( "DELETE FROM geometry_columns WHERE f_table_schema = '%s' AND f_table_name = '%s'\n", 
        pszSchemaName, pszLayerName );
    oStmt.Appendf("DROP TABLE [%s].[%s]", pszSchemaName, pszLayerName );

    CPLDebug( "MSSQLSpatial", "DeleteLayer(%s)", pszLayerName );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if ( strlen(pszLayerName) == 0 )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */

    oSession.BeginTransaction();
    
    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Error deleting layer: %s", GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    oSession.CommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRMSSQLSpatialDataSource::CreateLayer( const char * pszLayerName,
                                          OGRSpatialReference *poSRS,
                                          OGRwkbGeometryType eType,
                                          char ** papszOptions )

{
    char                *pszTableName = NULL;
    char                *pszSchemaName = NULL;
    const char          *pszGeomType = NULL;
    const char          *pszGeomColumn = NULL;
    int                 nCoordDimension = 3;

    /* determine the dimension */
    if( eType == wkbFlatten(eType) )
        nCoordDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != NULL )
        nCoordDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));
        
    /* MSSQL Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema is not
       specified
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != NULL )
    {
      int length = pszDotPos - pszLayerName;
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';
      
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszDotPos + 1 ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = NULL;
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszLayerName ); //skip "."
      else
          pszTableName = CPLStrdup( pszLayerName ); //skip "."
    }

    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != NULL )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue( papszOptions, "SCHEMA" ));
    }

    if (pszSchemaName == NULL)
        pszSchemaName = CPLStrdup("dbo");

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetTableName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                if (!pszSchemaName)
                    pszSchemaName = CPLStrdup(papoLayers[iLayer]->GetSchemaName());

                DeleteLayer( iLayer );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );

                CPLFree( pszSchemaName );
                CPLFree( pszTableName );
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    pszGeomType = CSLFetchNameValue( papszOptions, "GEOM_TYPE" );

    if( !pszGeomType )
        pszGeomType = "geometry";
    
    if( !EQUAL(pszGeomType, "geometry")
        && !EQUAL(pszGeomType, "geography"))
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "FORMAT=%s not recognised or supported.", 
                  pszGeomType );

        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        return NULL;
    }

    /* determine the geometry column name */
    pszGeomColumn =  CSLFetchNameValue( papszOptions, "GEOM_NAME");
    if (!pszGeomColumn)
        pszGeomColumn = "ogr_geometry";

/* -------------------------------------------------------------------- */
/*      Initialize the metadata tables                                  */
/* -------------------------------------------------------------------- */

    if (InitializeMetadataTables() != OGRERR_NONE)
    {
        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = 0;

    if( CSLFetchNameValue( papszOptions, "SRID") != NULL )
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));

    if( nSRSId == 0 && poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Create a new table and create a new entry in the geometry,      */
/*      geometry_columns metadata table.                                */
/* -------------------------------------------------------------------- */

    if( eType != wkbNone )
    {
        const char *pszGeometryType = OGRToOGCGeomType(eType);

        CPLODBCStatement oStmt( &oSession );
        
        oStmt.Appendf( "DELETE FROM geometry_columns WHERE f_table_schema = '%s' "
            "AND f_table_name = '%s'\n", pszSchemaName, pszTableName );
        
        oStmt.Appendf("INSERT INTO [geometry_columns] ([f_table_catalog], [f_table_schema] ,[f_table_name], "
            "[f_geometry_column],[coord_dimension],[srid],[geometry_type]) VALUES ('%s', '%s', '%s', '%s', %d, %d, '%s')\n", 
            pszCatalog, pszSchemaName, pszTableName, pszGeomColumn, nCoordDimension, nSRSId, pszGeometryType );

        oStmt.Appendf("CREATE TABLE [%s].[%s] ([ogr_fid] [int] IDENTITY(1,1) NOT NULL, "
            "[%s] [%s] NULL, CONSTRAINT [PK_%s] PRIMARY KEY CLUSTERED ([ogr_fid] ASC))",
            pszSchemaName, pszTableName, pszGeomColumn, pszGeomType, pszTableName);

        oSession.BeginTransaction();
        
        if( !oStmt.ExecuteSQL() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "Error creating layer: %s", GetSession()->GetLastError() );

            return NULL;
        }

        oSession.CommitTransaction();
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMSSQLSpatialTableLayer   *poLayer;

    poLayer = new OGRMSSQLSpatialTableLayer( this );

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));
    
    if (poLayer->Initialize("dbo", pszLayerName, pszGeomColumn, nCoordDimension, nSRSId, eType) == OGRERR_FAILURE)
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRMSSQLSpatialTableLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRMSSQLSpatialTableLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;


    return poLayer;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRMSSQLSpatialDataSource::OpenTable( const char *pszSchemaName, const char *pszTableName, 
                    const char *pszGeomCol, int nCoordDimension,
                    int nSRID, OGRwkbGeometryType eType, int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

    if( poLayer->Initialize( pszSchemaName, pszTableName, pszGeomCol, nCoordDimension, nSRID, eType ) )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRMSSQLSpatialTableLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRMSSQLSpatialTableLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}


/************************************************************************/
/*                       GetLayerCount()                                */
/************************************************************************/

int OGRMSSQLSpatialDataSource::GetLayerCount() 
{ 
    return nLayers; 
}

/************************************************************************/
/*                       ParseValue()                                   */
/************************************************************************/

int OGRMSSQLSpatialDataSource::ParseValue(char** pszValue, char* pszSource, const char* pszKey, int nStart, int nNext, int nTerm, int bRemove)
{
    int nLen = strlen(pszKey);
    if ((*pszValue) == NULL && nStart + nLen < nNext && 
            EQUALN(pszSource + nStart, pszKey, nLen))
    {
        *pszValue = (char*)CPLMalloc( sizeof(char) * (nNext - nStart - nLen + 1) );
        if (*pszValue)
            strncpy(*pszValue, pszSource + nStart + nLen, nNext - nStart - nLen);
        (*pszValue)[nNext - nStart - nLen] = 0;

        if (bRemove)
        {
            // remove the value from the source string
            if (pszSource[nNext] == ';')
                memmove( pszSource + nStart, pszSource + nNext + 1, nTerm - nNext);
            else
                memmove( pszSource + nStart, pszSource + nNext, nTerm - nNext + 1);
        }
        return TRUE;
    }
    return FALSE;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMSSQLSpatialDataSource::Open( const char * pszNewName, int bUpdate,
                             int bTestOpen )

{
    CPLAssert( nLayers == 0 );

    if( !EQUALN(pszNewName,"MSSQL:",6) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to MSSSQLSpatial naming convention,"
                      " MSSQL:*\n", pszNewName );
        return FALSE;
    }

    /* Determine if the connection string contains specific values */
    char* pszTableSpec = NULL;
    char* pszGeometryFormat = NULL;
    char* pszConnectionName = CPLStrdup(pszNewName + 6);
    char* pszDriver = NULL;
    int nCurrent, nNext, nTerm;
    nCurrent = nNext = nTerm = strlen(pszConnectionName);

    while (nCurrent > 0)
    {
        --nCurrent;
        if (pszConnectionName[nCurrent] == ';')
        {
            nNext = nCurrent;
            continue;
        }

        if (ParseValue(&pszCatalog, pszConnectionName, "database=", 
            nCurrent, nNext, nTerm, FALSE))
            continue;

        if (ParseValue(&pszTableSpec, pszConnectionName, "tables=", 
            nCurrent, nNext, nTerm, TRUE))
            continue;

        if (ParseValue(&pszDriver, pszConnectionName, "driver=", 
            nCurrent, nNext, nTerm, FALSE))
            continue;

        if (ParseValue(&pszGeometryFormat, pszConnectionName, 
            "geometryformat=", nCurrent, nNext, nTerm, TRUE))
        {
            if (EQUALN(pszGeometryFormat, "wkb",3))
                nGeometryFormat = MSSQLGEOMETRY_WKB;
            else if (EQUALN(pszGeometryFormat,"wkt",3))
                nGeometryFormat = MSSQLGEOMETRY_WKT;
            else if (EQUALN(pszGeometryFormat,"native",3))
                nGeometryFormat = MSSQLGEOMETRY_NATIVE;
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid geometry type specified: %s,"
                      " MSSQL:*\n", pszGeometryFormat );
                
                CPLFree(pszTableSpec);
                CPLFree(pszGeometryFormat);
                CPLFree(pszConnectionName);
                CPLFree(pszDriver);
                return FALSE;
            }

            CPLFree(pszGeometryFormat);
            pszGeometryFormat = NULL;
            continue;
        }
    }

    /* Determine if the connection string contains the catalog portion */
    if( pszCatalog == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                      "'%s' does not contain the 'database' portion\n", pszNewName );
        
        CPLFree(pszTableSpec);
        CPLFree(pszGeometryFormat);
        CPLFree(pszConnectionName);
        CPLFree(pszDriver);
        return FALSE;
    }
    
    pszName = CPLStrdup(pszNewName);

    char  **papszTableNames=NULL;
    char  **papszSchemaNames=NULL;
    char  **papszGeomColumnNames=NULL;
    char  **papszCoordDimensions=NULL;
    char  **papszSRIds=NULL;

    /* Determine if the connection string contains the TABLES portion */
    if( pszTableSpec != NULL )
    {
        char          **papszTableList;
        int             i;

        papszTableList = CSLTokenizeString2( pszTableSpec, ",", 0 );

        for( i = 0; i < CSLCount(papszTableList); i++ )
        {
            char      **papszQualifiedParts;

            // Get schema and table name
            papszQualifiedParts = CSLTokenizeString2( papszTableList[i],
                                                      ".", 0 );

            /* Find the geometry column name if specified */
            if( CSLCount( papszQualifiedParts ) >= 1 )
            {
                char* pszGeomColumnName = NULL;
                char* pos = strchr(papszQualifiedParts[CSLCount( papszQualifiedParts ) - 1], '(');
                if (pos != NULL)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos+1;
                    int len = strlen(pszGeomColumnName);
                    if (len > 0)
                        pszGeomColumnName[len - 1] = '\0';
                }
                papszGeomColumnNames = CSLAddString( papszGeomColumnNames,
                        pszGeomColumnName ? pszGeomColumnName : "");
            }

            if( CSLCount( papszQualifiedParts ) == 2 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames, 
                                                papszQualifiedParts[0] );
                papszTableNames = CSLAddString( papszTableNames,
                                                papszQualifiedParts[1] );
            }
            else if( CSLCount( papszQualifiedParts ) == 1 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames, "dbo");
                papszTableNames = CSLAddString( papszTableNames,
                                                papszQualifiedParts[0] );
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
    }

    CPLFree(pszTableSpec);

    /* Initialize the SQL Server connection. */
    int nResult;
    if ( pszDriver != NULL )
    {
        /* driver has been specified */
        CPLDebug( "OGR_MSSQLSpatial", "EstablishSession(Connection:\"%s\")", pszConnectionName);
        nResult = oSession.EstablishSession( pszConnectionName, "", "" );
    }
    else
    {
        /* no driver has been specified, defautls to SQL Server */
        CPLDebug( "OGR_MSSQLSpatial", "EstablishSession(Connection:\"%s\")", pszConnectionName);
        nResult = oSession.EstablishSession( CPLSPrintf("DRIVER=SQL Server;%s", pszConnectionName), "", "" );
    }

    CPLFree(pszDriver);

    if( !nResult )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to initialize connection to the server for %s,\n"
                  "%s", pszNewName, oSession.GetLastError() );
        
        CSLDestroy( papszTableNames );
        CSLDestroy( papszSchemaNames );
        CSLDestroy( papszGeomColumnNames );
        CSLDestroy( papszCoordDimensions );
        CSLDestroy( papszSRIds );
        CPLFree(pszGeometryFormat);
        CPLFree(pszConnectionName);
        return FALSE;
    }

    char** papszTypes = NULL;

    /* Determine the available tables if not specified. */
    if (papszTableNames == NULL)
    {
        CPLODBCStatement oStmt( &oSession );
            
        oStmt.Append( "SELECT f_table_schema, f_table_name, f_geometry_column, coord_dimension, srid, geometry_type FROM dbo.geometry_columns");

        if( oStmt.ExecuteSQL() )
        {
            while( oStmt.Fetch() )
            {
                papszSchemaNames = 
                        CSLAddString( papszSchemaNames, oStmt.GetColData(0) );
                papszTableNames = 
                        CSLAddString( papszTableNames, oStmt.GetColData(1) );
                papszGeomColumnNames = 
                        CSLAddString( papszGeomColumnNames, oStmt.GetColData(2) );
                papszCoordDimensions = 
                        CSLAddString( papszCoordDimensions, oStmt.GetColData(3) );
                papszSRIds = 
                        CSLAddString( papszSRIds, oStmt.GetColData(4) );
                papszTypes = 
                        CSLAddString( papszTypes, oStmt.GetColData(5) );
            }
        }
    }

    int nSRId, nCoordDimension;
    OGRwkbGeometryType eType;
        
    for( int iTable = 0; 
         papszTableNames != NULL && papszTableNames[iTable] != NULL; 
         iTable++ )
    {
        if (papszSRIds != NULL)
            nSRId = atoi(papszSRIds[iTable]);
        else
            nSRId = -1;

        if (papszCoordDimensions != NULL)
            nCoordDimension = atoi(papszCoordDimensions[iTable]);
        else
            nCoordDimension = 2;

        if (papszTypes != NULL)
            eType = OGRFromOGCGeomType(papszTypes[iTable]);
        else
            eType = wkbUnknown;

        if( strlen(papszGeomColumnNames[iTable]) > 0 )
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable], papszGeomColumnNames[iTable], 
                    nCoordDimension, nSRId, eType, bUpdate );
        else
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable], NULL, 
                    nCoordDimension, nSRId, eType, bUpdate );
    }

    CSLDestroy( papszTableNames );
    CSLDestroy( papszSchemaNames );
    CSLDestroy( papszGeomColumnNames );
    CSLDestroy( papszCoordDimensions );
    CSLDestroy( papszSRIds );
    CSLDestroy( papszTypes );

    CPLFree(pszGeometryFormat);
    CPLFree(pszConnectionName);
    
    bDSUpdate = bUpdate;

    return TRUE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRMSSQLSpatialDataSource::ExecuteSQL( const char *pszSQLCommand,
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
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;
        
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( EQUAL(papoLayers[iLayer]->GetName(), 
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return NULL;
    }

    CPLDebug( "MSSQLSpatial", "ExecuteSQL(%s) called.", pszSQLCommand );

    if( EQUALN(pszSQLCommand, "DROP SPATIAL INDEX ON ", 22) )
    {
        /* Handle command to drop a spatial index. */
        OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

        if (poLayer)
        {
            if( poLayer->Initialize( "dbo", pszSQLCommand + 22, NULL, 0, 0, wkbUnknown ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to initialize layer '%s'", pszSQLCommand + 22 );   
            }
            poLayer->DropSpatialIndex();
            delete poLayer;
        }
        return NULL;
    }
    else if( EQUALN(pszSQLCommand, "CREATE SPATIAL INDEX ON ", 24) )
    {
        /* Handle command to create a spatial index. */
        OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

        if (poLayer)
        {
            if( poLayer->Initialize( "dbo", pszSQLCommand + 24, NULL, 0, 0, wkbUnknown ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to initialize layer '%s'", pszSQLCommand + 24 );    
            }
            poLayer->CreateSpatialIndex();
            delete poLayer;
        }
        return NULL;
    }
    
    /* Execute the command natively */
    CPLODBCStatement *poStmt = new CPLODBCStatement( &oSession );
    poStmt->Append( pszSQLCommand );

    if( !poStmt->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", oSession.GetLastError() );
        delete poStmt;
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
    
    OGRMSSQLSpatialSelectLayer *poLayer = NULL;
        
    poLayer = new OGRMSSQLSpatialSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRMSSQLSpatialDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRMSSQLSpatialDataSource::LaunderName( const char *pszSrcName )

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
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS).                                              */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::InitializeMetadataTables()

{
    CPLODBCStatement oStmt( &oSession );

    oStmt.Append( "IF NOT EXISTS (SELECT * FROM sys.objects WHERE "
        "object_id = OBJECT_ID(N'[dbo].[geometry_columns]') AND type in (N'U')) "
        "CREATE TABLE geometry_columns (f_table_catalog varchar(128) not null, "
        "f_table_schema varchar(128) not null, f_table_name varchar(256) not null, "
        "f_geometry_column varchar(256) not null, coord_dimension integer not null, "
        "srid integer not null, geometry_type varchar(30) not null, "
        "CONSTRAINT geometry_columns_pk PRIMARY KEY (f_table_catalog, "
        "f_table_schema, f_table_name, f_geometry_column));\n" );

    oStmt.Append( "IF NOT EXISTS (SELECT * FROM sys.objects "
        "WHERE object_id = OBJECT_ID(N'[dbo].[spatial_ref_sys]') AND type in (N'U')) "
        "CREATE TABLE spatial_ref_sys (srid integer not null "
        "PRIMARY KEY, auth_name varchar(256), auth_srid integer, srtext varchar(2048), proj4text varchar(2048))" );

    oSession.BeginTransaction();
    
    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Error initializing the metadata tables : %s", GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }

    oSession.CommitTransaction();

    return OGRERR_NONE;
}


/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRMSSQLSpatialDataSource::FetchSRS( int nId )

{
    if( nId <= 0 )
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
/*      Try looking up in spatial_ref_sys table                         */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = NULL;

    CPLODBCStatement oStmt( GetSession() );
    oStmt.Appendf( "SELECT srtext FROM spatial_ref_sys WHERE srid = %d", nId );

    if( !oStmt.ExecuteSQL() || !oStmt.Fetch() )
    {
        return NULL;
    }
    
    if ( oStmt.GetColData( 0 ) )
    {
        poSRS = new OGRSpatialReference();
        char* pszWKT = (char*)oStmt.GetColData( 0 );
        if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
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
        nKnownSRID++;
    }

    return poSRS;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRMSSQLSpatialDataSource::FetchSRSId( OGRSpatialReference * poSRS)

{
    char                *pszWKT = NULL;
    int                 nSRSId = 0;
    const char*         pszAuthorityName;

    if( poSRS == NULL )
        return 0;

    OGRSpatialReference oSRS(*poSRS);
    poSRS = NULL;

    pszAuthorityName = oSRS.GetAuthorityName(NULL);

    if( pszAuthorityName == NULL || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(NULL);
        if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(NULL);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    int  nAuthorityCode = 0;
    if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );

        CPLODBCStatement oStmt( &oSession );
        oStmt.Appendf("SELECT srid FROM spatial_ref_sys WHERE "
                         "auth_name = '%s' AND auth_srid = %d",
                         pszAuthorityName,
                         nAuthorityCode );

        if( oStmt.ExecuteSQL() && oStmt.Fetch() && oStmt.GetColData( 0 ) )
        {
            nSRSId = atoi(oStmt.GetColData( 0 ));
            return nSRSId;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Try to find in the existing table.                              */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oStmt( &oSession );

    oStmt.Append( "SELECT srid FROM spatial_ref_sys WHERE srtext = ");
    OGRMSSQLAppendEscaped(&oStmt, pszWKT);

/* -------------------------------------------------------------------- */
/*      We got it!  Return it.                                          */
/* -------------------------------------------------------------------- */
    if( oStmt.ExecuteSQL() )
    {
        if ( oStmt.Fetch() && oStmt.GetColData( 0 ) )
        {
            nSRSId = atoi(oStmt.GetColData( 0 ));
            CPLFree(pszWKT);
            return nSRSId;
        }
    }
    else
    {
        /* probably the table is missing at all */
        if( InitializeMetadataTables() != OGRERR_NONE )
        {
            CPLFree(pszWKT);
            return 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */
    char    *pszProj4 = NULL;
    if( oSRS.exportToProj4( &pszProj4 ) != OGRERR_NONE )
    {
        CPLFree( pszProj4 );
        CPLFree(pszWKT);
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Check whether the auth_code can be used as srid.                */
/* -------------------------------------------------------------------- */
    nSRSId = nAuthorityCode;

    oStmt.Clear();
    oSession.BeginTransaction();
    if (nAuthorityCode > 0)
    {
        oStmt.Appendf("SELECT srid FROM spatial_ref_sys where srid = %d", nAuthorityCode);
        if ( oStmt.ExecuteSQL() && oStmt.Fetch())
        {
            nSRSId = 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the current maximum srid in the srs table.                  */
/* -------------------------------------------------------------------- */
    
    if (nSRSId == 0)
    {
        oStmt.Clear();
        oStmt.Append("SELECT COALESCE(MAX(srid) + 1, 32768) FROM spatial_ref_sys where srid between 32768 and 65536");

        if ( oStmt.ExecuteSQL() && oStmt.Fetch() && oStmt.GetColData( 0 ) )
        {
            nSRSId = atoi(oStmt.GetColData( 0 ));
        }
    }

    if (nSRSId == 0)
    {
        /* unable to allocate srid */
        oSession.RollbackTransaction();
        CPLFree( pszProj4 );
        CPLFree(pszWKT);
        return 0;
    }
    
    oStmt.Clear();
    if( nAuthorityCode > 0 )
    {
        oStmt.Appendf(
                 "INSERT INTO spatial_ref_sys (srid, auth_srid, auth_name, srtext, proj4text) "
                 "VALUES (%d, %d, ", nSRSId, nAuthorityCode );
        OGRMSSQLAppendEscaped(&oStmt, pszAuthorityName);
        oStmt.Append(", ");
        OGRMSSQLAppendEscaped(&oStmt, pszWKT);
        oStmt.Append(", ");
        OGRMSSQLAppendEscaped(&oStmt, pszProj4);
        oStmt.Append(")");
    }
    else
    {
        oStmt.Appendf(
                 "INSERT INTO spatial_ref_sys (srid,srtext,proj4text) VALUES (%d, ", nSRSId);
        OGRMSSQLAppendEscaped(&oStmt, pszWKT);
        oStmt.Append(", ");
        OGRMSSQLAppendEscaped(&oStmt, pszProj4);
        oStmt.Append(")");
    }

    /* Free everything that was allocated. */
    CPLFree( pszProj4 );
    CPLFree( pszWKT);

    if ( oStmt.ExecuteSQL() )
        oSession.CommitTransaction();
    else
        oSession.RollbackTransaction();

    return nSRSId;
}

