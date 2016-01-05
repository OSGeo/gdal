/****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Implements OGRDB2DataSource class
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 ****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#include "ogr_db2.h"

/* layer status */
#define DB2LAYERSTATUS_ORIGINAL 0
#define DB2LAYERSTATUS_INITIAL  1
#define DB2LAYERSTATUS_CREATED  2
#define DB2LAYERSTATUS_DISABLED 3

/************************************************************************/
/*                          OGRDB2DataSource()                 */
/************************************************************************/

OGRDB2DataSource::OGRDB2DataSource()

{
    pszName = NULL;
    pszCatalog = NULL;
    papoLayers = NULL;
    nLayers = 0;

    nKnownSRID = 0;
    panSRID = NULL;
    papoSRS = NULL;

    bUseGeometryColumns = CSLTestBoolean(CPLGetConfigOption(
            "DB2SPATIAL_USE_GEOMETRY_COLUMNS",
            "YES"));
    bListAllTables = CSLTestBoolean(CPLGetConfigOption(
                                        "DB2SPATIAL_LIST_ALL_TABLES", "NO"));
}

/************************************************************************/
/*                         ~OGRDB2DataSource()                 */
/************************************************************************/

OGRDB2DataSource::~OGRDB2DataSource()

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

int OGRDB2DataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) || EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDB2DataSource::GetLayer( int iLayer )

{
    CPLDebug("OGR_DB2DataSource::GetLayer", "pszLayer %d", iLayer);
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/
// Layer names are always uppercased - for now
OGRLayer *OGRDB2DataSource::GetLayerByName( const char* pszLayerName )

{
    if (!pszLayerName)
        return NULL;
    char *pszTableName = NULL;
    char *pszSchemaName = NULL;
    OGRLayer *poLayer = NULL;
    char *pszLayerNameUpper = NULL;
    CPLDebug("OGR_DB2DataSource::GetLayerByName", "pszLayerName: '%s'",
             pszLayerName);
    pszLayerNameUpper = ToUpper(pszLayerName);
    const char* pszDotPos = strstr(pszLayerNameUpper,".");
    if ( pszDotPos != NULL )
    {
        int length = static_cast<int>(pszDotPos - pszLayerNameUpper);
        pszSchemaName = (char*)CPLMalloc(length+1);
        strncpy(pszSchemaName, pszLayerNameUpper, length);
        pszSchemaName[length] = '\0';
        pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
        pszTableName = CPLStrdup( pszLayerNameUpper );
    }

    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszTableName,papoLayers[iLayer]->GetTableName()) &&
                (pszSchemaName == NULL ||
                 EQUAL(pszSchemaName,papoLayers[iLayer]->GetSchemaName())))
        {
            CPLDebug("OGR_DB2DataSource::GetLayerByName",
                     "found layer: %d; schema: '%s'; table: '%s'",
                     iLayer,papoLayers[iLayer]->GetSchemaName(),
                     papoLayers[iLayer]->GetTableName());

            poLayer = papoLayers[iLayer];
        }
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );
    CPLFree( pszLayerNameUpper );

    return poLayer;
}


/************************************************************************/
/*                    DeleteLayer(OGRDB2TableLayer * poLayer)           */
/************************************************************************/

int OGRDB2DataSource::DeleteLayer( OGRDB2TableLayer * poLayer )
{
    int iLayer = 0;
    if( poLayer == NULL )
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Blow away our OGR structures related to the layer.  This is     */
    /*      pretty dangerous if anything has a reference to this layer!     */
    /* -------------------------------------------------------------------- */
    const char* pszTableName = poLayer->GetTableName();
    const char* pszSchemaName = poLayer->GetSchemaName();

    CPLODBCStatement oStmt( &oSession );

    oStmt.Appendf("DROP TABLE %s.%s", pszSchemaName, pszTableName );

    CPLDebug( "OGR_DB2DataSource::DeleteLayer", "Drop stmt: '%s'",
              oStmt.GetCommand());

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if (poLayer == papoLayers[iLayer]) break;
    }
    delete papoLayers[iLayer]; // free the layer object
    // move remaining layers down
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

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
/*                            DeleteLayer(int iLayer)                   */
/************************************************************************/

int OGRDB2DataSource::DeleteLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;

    return DeleteLayer(papoLayers[iLayer]);

}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRDB2DataSource::ICreateLayer( const char * pszLayerName,
        OGRSpatialReference *poSRS,
        OGRwkbGeometryType eType,
        char ** papszOptions )

{
    char                *pszTableName = NULL;
    char                *pszSchemaName = NULL;
    const char          *pszGeomColumn = NULL;
    int                 nCoordDimension = 3;
    CPLDebug("OGR_DB2DataSource::ICreateLayer","layer name: %s",pszLayerName);
    /* determine the dimension */
    if( eType == wkbFlatten(eType) )
        nCoordDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != NULL )
        nCoordDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));

    /* DB2 Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema is not
       specified
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != NULL )
    {
        int length = static_cast<int>(pszDotPos - pszLayerName);
        pszSchemaName = (char*)CPLMalloc(length+1);
        strncpy(pszSchemaName, pszLayerName, length);
        pszSchemaName[length] = '\0';

        /* For now, always convert layer name to uppercase table name*/
        pszTableName = ToUpper( pszDotPos + 1 );

    }
    else
    {
        pszSchemaName = NULL;
        /* For now, always convert layer name to uppercase table name*/
        pszTableName = ToUpper( pszLayerName );
    }

    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != NULL )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue(papszOptions, "SCHEMA"));
    }


    /* -------------------------------------------------------------------- */
    /*      Do we already have this layer?  If so, should we blow it        */
    /*      away?                                                           */
    /* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        CPLDebug("OGR_DB2DataSource::ICreateLayer",
                 "schema: '%s'; table: '%s'",
                 papoLayers[iLayer]->GetSchemaName(),
                 papoLayers[iLayer]->GetTableName());

        if( EQUAL(pszTableName,papoLayers[iLayer]->GetTableName()) &&
                (pszSchemaName == NULL ||
                 EQUAL(pszSchemaName,papoLayers[iLayer]->GetSchemaName())) )
        {
            CPLDebug("OGR_DB2DataSource::ICreateLayer",
                     "Found match, schema: '%s'; table: '%s'"	,
                     pszSchemaName, pszTableName);
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                    && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),
                              "NO"))
            {
                if (!pszSchemaName)
                    pszSchemaName = CPLStrdup(papoLayers[iLayer]->
                                              GetSchemaName());

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

    /* determine the geometry column name */
    pszGeomColumn =  CSLFetchNameValue( papszOptions, "GEOM_NAME");
    if (!pszGeomColumn)
        pszGeomColumn = "OGR_geometry";


    /* -------------------------------------------------------------------- */
    /*      Try to get the SRS Id of this spatial reference system,         */
    /*      adding to the srs table if needed.                              */
    /* -------------------------------------------------------------------- */
    int nSRSId = 0;

    if( CSLFetchNameValue( papszOptions, "SRID") != NULL )
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));

    if( nSRSId == 0 && poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

    CPLODBCStatement oStmt( &oSession );

    if (pszSchemaName != NULL)
        oStmt.Appendf("CREATE TABLE %s.%s ",
                      pszSchemaName, pszTableName);
    else
        oStmt.Appendf("CREATE TABLE %s" ,
                      pszTableName);
    oStmt.Appendf(" (ogr_fid int not null primary key GENERATED BY DEFAULT "
                  "AS IDENTITY, "
                  "%s db2gse.st_%s )",
                  pszGeomColumn, OGRToOGCGeomType(eType));

    oSession.BeginTransaction();
    CPLDebug("OGR_DB2DataSource::ICreateLayer", "stmt: '%s'",
             oStmt.GetCommand());
    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating layer: %s", GetSession()->GetLastError() );
        CPLDebug("OGR_DB2DataSource::ICreateLayer", "create failed");

        return NULL;
    }

    oSession.CommitTransaction();

    // If we didn't have a schema name when the table was created,
    // get the schema that was created by implicit
    if (pszSchemaName == NULL) {
        oStmt.Clear();
        oStmt.Appendf( "SELECT table_schema FROM db2gse.st_geometry_columns "
                       "WHERE table_name = '%s'",
                       pszTableName);

        CPLDebug("OGR_DB2DataSource::ICreateLayer", "SQL: %s",
                 oStmt.GetCommand());
        if( oStmt.ExecuteSQL() && oStmt.Fetch())
        {
            CPLDebug("OGR_DB2DataSource::ICreateLayer", "col 0: %s",
                     oStmt.GetColData(0));
            pszSchemaName = CPLStrdup( oStmt.GetColData(0) );
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDB2TableLayer   *poLayer;

    poLayer = new OGRDB2TableLayer( this );

    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",
                               TRUE));

    char *pszWKT = NULL;
    if( poSRS && poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        pszWKT = NULL;
    }
    CPLDebug("OGR_DB2DataSource::ICreateLayer", "srs wkt: %s",pszWKT);
    if (poLayer->Initialize(pszSchemaName, pszTableName, pszGeomColumn,
                            nCoordDimension, nSRSId, pszWKT, eType)
            == OGRERR_FAILURE)
    {
        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        CPLFree( pszWKT );
        return NULL;
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );
    CPLFree( pszWKT );

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = (OGRDB2TableLayer **)
                 CPLRealloc( papoLayers, sizeof(OGRDB2TableLayer *)
                             * (nLayers+1));

    papoLayers[nLayers++] = poLayer;


    return poLayer;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRDB2DataSource::OpenTable( const char *pszSchemaName,
                                 const char *pszTableName,
                                 const char *pszGeomCol, int nCoordDimension,
                                 int nSRID, const char *pszSRText,
                                 OGRwkbGeometryType eType,
                                 CPL_UNUSED int bUpdate )
{
    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDB2TableLayer  *poLayer = new OGRDB2TableLayer( this );
    CPLDebug( "OGR_DB2DataSource::OpenTable",
              "pszSchemaName: '%s'; pszTableName: '%s'; pszGeomCol: '%s'",
              pszSchemaName , pszTableName, pszGeomCol);
    if( poLayer->Initialize( pszSchemaName, pszTableName, pszGeomCol,
                             nCoordDimension, nSRID, pszSRText, eType ) )
    {
        delete poLayer;
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = (OGRDB2TableLayer **)
                 CPLRealloc( papoLayers,  sizeof(OGRDB2TableLayer *)
                             * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}


/************************************************************************/
/*                       GetLayerCount()                                */
/************************************************************************/

int OGRDB2DataSource::GetLayerCount()
{
    return nLayers;
}

/************************************************************************/
/*                       ParseValue()                                   */
/************************************************************************/

int OGRDB2DataSource::ParseValue(char** pszValue, char* pszSource,
                                 const char* pszKey, int nStart, int nNext,
                                 int nTerm, int bRemove)
{
    int nLen = static_cast<int>(strlen(pszKey));
    if ((*pszValue) == NULL && nStart + nLen < nNext &&
            EQUALN(pszSource + nStart, pszKey, nLen))
    {
        *pszValue = (char*)CPLMalloc( sizeof(char)
                                      * (nNext - nStart - nLen + 1) );
        if (*pszValue)
            strncpy(*pszValue, pszSource + nStart + nLen,
                    nNext - nStart - nLen);
        (*pszValue)[nNext - nStart - nLen] = 0;

        if (bRemove)
        {
            // remove the value from the source string
            if (pszSource[nNext] == ';')
                memmove( pszSource + nStart, pszSource + nNext + 1,
                         nTerm - nNext);
            else
                memmove( pszSource + nStart, pszSource + nNext,
                         nTerm - nNext + 1);
        }
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int OGRDB2DataSource::Create( const char * pszFilename,
                              int nXSize,
                              int nYSize,
                              int nRasterBands,
                              GDALDataType eDT,
                              char **papszOptions )
{
    CPLDebug( "OGR_DB2DataSource::Create", "pszFileName: '%s'", pszFilename);
    return Open(pszFilename, 0, 0);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDB2DataSource::Open( GDALOpenInfo* poOpenInfo )
{

    SetDescription( poOpenInfo->pszFilename );
    CPLDebug( "OGR_DB2DataSource::OpenNew", "pszNewName: '%s'",
              poOpenInfo->pszFilename);

    return Open(poOpenInfo->pszFilename, 0, 0);
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDB2DataSource::Open( const char * pszNewName, int bUpdate,
                            int bTestOpen )

{

    CPLAssert( nLayers == 0 );

    CPLDebug( "OGR_DB2DataSource::Open", "pszNewName: '%s'", pszNewName);

    if( !STARTS_WITH_CI(pszNewName, DB2ODBC_PREFIX) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to DB2 naming convention,"
                      " DB2:*\n", pszNewName );
        return FALSE;
    }

    /* Determine if the connection string contains specific values */
    char* pszTableSpec = NULL;
    char* pszConnectionName = CPLStrdup(pszNewName + strlen(DB2ODBC_PREFIX));
    char* pszDriver = NULL;
    int nCurrent, nNext, nTerm;
    nCurrent = nNext = nTerm = static_cast<int>(strlen(pszConnectionName));

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
    }
    CPLDebug( "OGR_DB2DataSource::Open", "pszCatalog: '%s'", pszCatalog);
    CPLDebug( "OGR_DB2DataSource::Open", "pszTableSpec: '%s'", pszTableSpec);
    CPLDebug( "OGR_DB2DataSource::Open", "pszDriver: '%s'", pszDriver);
    CPLDebug( "OGR_DB2DataSource::Open", "pszConnectionName: '%s'",
              pszConnectionName);

    /* Determine if the connection string contains the database portion */
    if( pszCatalog == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "'%s' does not contain the 'database' portion\n",
                  pszNewName );
        CPLFree(pszTableSpec);
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
    char  **papszSRTexts=NULL;

    // if the table parameter was specified, pull out the table names
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
                char* pos = strchr(papszQualifiedParts[
                                       CSLCount( papszQualifiedParts ) - 1], '(');
                if (pos != NULL)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos+1;
                    int len = static_cast<int>(strlen(pszGeomColumnName));
                    if (len > 0)
                        pszGeomColumnName[len - 1] = '\0';
                }
                papszGeomColumnNames = CSLAddString( papszGeomColumnNames,
                                                     pszGeomColumnName ?
                                                     pszGeomColumnName : "");
            }

            if( CSLCount( papszQualifiedParts ) == 2 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames,
                                                 ToUpper(papszQualifiedParts[0]));

                papszTableNames = CSLAddString( papszTableNames,
                                                ToUpper(papszQualifiedParts[1]));
            }
            else if( CSLCount( papszQualifiedParts ) == 1 )
            {
                papszSchemaNames = CSLAddString( papszSchemaNames, "NULL");
                papszTableNames = CSLAddString( papszTableNames,
                                                ToUpper(papszQualifiedParts[0]));
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
    }

    CPLFree(pszTableSpec);

    /* Initialize the DB2 connection. */
    int nResult;

    CPLDebug( "OGR_DB2DataSource::Open", "EstablishSession with: '%s'",
              pszConnectionName);
    nResult = oSession.EstablishSession( pszConnectionName, "", "" );
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
        CSLDestroy( papszSRTexts );
        CPLFree(pszConnectionName);
        return FALSE;
    }

    char** papszTypes = NULL;

    /* read metadata for the specified tables */
    if (papszTableNames != NULL)
    {

        for( int iTable = 0;
                papszTableNames != NULL && papszTableNames[iTable] != NULL;
                iTable++ )
        {
            int found = FALSE;
            CPLODBCStatement oStmt( &oSession );
// If a table name was specified, get the information from ST_Geometry_Columns
// for this table
            oStmt.Appendf( "SELECT table_schema, column_name, 2, srs_id, "
                           "srs_name, type_name "
                           "FROM db2gse.st_geometry_columns "
                           "WHERE table_name = '%s'",
                           papszTableNames[iTable]);

// If the schema was specified, add it to the SELECT statement
            if (strcmp(papszSchemaNames[iTable], "NULL"))
                oStmt.Appendf("  AND table_schema = '%s' ",
                              papszSchemaNames[iTable]);
            CPLDebug("OGR_DB2DataSource::Open", "SQL: %s",oStmt.GetCommand());
            if( oStmt.ExecuteSQL() )
            {
                while( oStmt.Fetch() )
                {
                    found = TRUE;

                    /* set schema for table if it was not specified */
                    if (!strcmp(papszSchemaNames[iTable], "NULL")) {
                        CPLFree(papszSchemaNames[iTable]);
                        papszSchemaNames[iTable] = CPLStrdup(
                                                       oStmt.GetColData(0) );
                    }
                    if (papszGeomColumnNames == NULL)
                        papszGeomColumnNames = CSLAddString(
                                                   papszGeomColumnNames,
                                                   oStmt.GetColData(1) );
                    else if (*papszGeomColumnNames[iTable] == 0)
                    {
                        CPLFree(papszGeomColumnNames[iTable]);
                        papszGeomColumnNames[iTable] = CPLStrdup(
                                                           oStmt.GetColData(1) );
                    }

                    papszCoordDimensions =
                        CSLAddString( papszCoordDimensions,
                                      oStmt.GetColData(2, "2") );
                    papszSRIds =
                        CSLAddString( papszSRIds, oStmt.GetColData(3, "-1") );
                    papszSRTexts =
                        CSLAddString( papszSRTexts, oStmt.GetColData(4, "") );
                    // Convert the DB2 spatial type to the OGC spatial type
                    // which just entails stripping off the "ST_" at the
                    // beginning of the DB2 type name
                    char DB2SpatialType[20], OGCSpatialType [20];
                    strcpy(DB2SpatialType, oStmt.GetColData(5));
                    strcpy(OGCSpatialType, DB2SpatialType+3);
                    papszTypes = CSLAddString( papszTypes, OGCSpatialType );
                }
            }

            if (!found) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Table %s.%s not found in "
                          "db2gse.st_geometry_columns"
                          , papszSchemaNames[iTable],papszTableNames[iTable]);

                CSLDestroy( papszTableNames );
                CSLDestroy( papszSchemaNames );
                CSLDestroy( papszGeomColumnNames );
                CSLDestroy( papszCoordDimensions );
                CSLDestroy( papszSRIds );
                CSLDestroy( papszSRTexts );
                CPLFree(pszConnectionName);
                return FALSE;
            }
        }
    }


    /* Determine the available tables if not specified. */
    if (papszTableNames == NULL)
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT table_schema, table_name, column_name, 2, "
                      "srs_id, srs_name, type_name "
                      "FROM db2gse.st_geometry_columns");

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
                    CSLAddString( papszSRIds, oStmt.GetColData(4,"-1") );
                papszSRTexts =
                    CSLAddString( papszSRTexts, oStmt.GetColData(5,"") );
                char DB2SpatialType[20], OGCSpatialType [20];
                strcpy(DB2SpatialType, oStmt.GetColData(6));
                strcpy(OGCSpatialType, DB2SpatialType+3);
                papszTypes =
                    CSLAddString( papszTypes, OGCSpatialType );
            }
        }
    }
    /*
    CPLDebug("OGR_DB2DataSource::Open", "papszSchemaNames");
    CSLPrint(papszSchemaNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszTableNames");
    CSLPrint(papszTableNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszGeomColumnNames");
    CSLPrint(papszGeomColumnNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszSRIds");
    CSLPrint(papszSRIds, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszSRTexts");
    CSLPrint(papszSRTexts, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszTypes");
    CSLPrint(papszTypes, stderr);
    */

    int nSRId, nCoordDimension;
    char * pszSRText = NULL;
    OGRwkbGeometryType eType;

    for( int iTable = 0;
            papszTableNames != NULL && papszTableNames[iTable] != NULL;
            iTable++ )
    {
        pszSRText = NULL;
        nSRId = -1;
        if (papszSRIds != NULL) {
            nSRId = atoi(papszSRIds[iTable]);
            CPLDebug("OGR_DB2DataSource::Open", "iTable: %d; schema: %s; "
                     "table: %s; geomCol: %s; geomType: %s; srid: '%s'",
                     iTable, papszSchemaNames[iTable],
                     papszTableNames[iTable],
                     papszGeomColumnNames[iTable], papszTypes[iTable],
                     papszSRIds[iTable]);
            // If srid is not defined it was probably because the table
            // was not registered.
            // In that case, try to get it from the actual data table.
            if (nSRId < 0) {
                CPLODBCStatement oStmt( &oSession );
                oStmt.Appendf( "select db2gse.st_srsid(%s) from %s.%s "
                               "fetch first row only",
                               papszGeomColumnNames[iTable],
                               papszSchemaNames[iTable],
                               papszTableNames[iTable] );
                CPLDebug("OGR_DB2DataSource::Open",
                         "SQL: %s", oStmt.GetCommand());

                if( oStmt.ExecuteSQL())
                {
                    if ( oStmt.Fetch()) {
                        nSRId = atoi( oStmt.GetColData( 0 ) );
                        CPLODBCStatement oStmt2( &oSession );
                        oStmt2.Appendf( "select definition from "
                                        "db2gse.st_spatial_reference_systems "
                                        "where srs_id = %d",
                                        nSRId);
                        if( oStmt2.ExecuteSQL() && oStmt2.Fetch() )
                        {
                            if ( oStmt2.GetColData( 0 ) )
                                pszSRText = CPLStrdup(oStmt2.GetColData(0));
                        }
                        CPLDebug("OGR_DB2DataSource::Open",
                                 "nSRId: %d; srText: %s",
                                 nSRId, pszSRText);
                    } else CPLDebug("OGR_DB2DataSource::Open",
                                        "Last error: '%s'", oSession.GetLastError());

                }
            } else {
                pszSRText = CPLStrdup(papszSRTexts[iTable]);
            }
        }
        if (papszCoordDimensions != NULL)
            nCoordDimension = atoi(papszCoordDimensions[iTable]);
        else
            nCoordDimension = 2;

        if (papszTypes != NULL)
            eType = OGRFromOGCGeomType(papszTypes[iTable]);
        else
            eType = wkbUnknown;

        if( strlen(papszGeomColumnNames[iTable]) > 0 )
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable],
                       papszGeomColumnNames[iTable],
                       nCoordDimension, nSRId, pszSRText, eType, bUpdate );
        else
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable],
                       NULL,
                       nCoordDimension, nSRId, pszSRText, wkbNone, bUpdate );
    }

    CSLDestroy( papszTableNames );
    CSLDestroy( papszSchemaNames );
    CSLDestroy( papszGeomColumnNames );
    CSLDestroy( papszCoordDimensions );
    CSLDestroy( papszSRIds );
    CSLDestroy( papszSRTexts );
    CSLDestroy( papszTypes );

    CPLFree(pszConnectionName);
    CPLFree(pszSRText);

    bDSUpdate = bUpdate;

    return TRUE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRDB2DataSource::ExecuteSQL( const char *pszSQLCommand,
        OGRGeometry *poSpatialFilter,
        const char *pszDialect )

{
    /* -------------------------------------------------------------------- */
    /*      Use generic implementation for recognized dialects              */
    /* -------------------------------------------------------------------- */
    CPLDebug("OGRDB2DataSource::ExecuteSQL", "SQL: '%s'; dialect: '%s'",
             pszSQLCommand, pszDialect);
    if( IsGenericSQLDialect(pszDialect) )
        return GDALDataset::ExecuteSQL( pszSQLCommand, poSpatialFilter,
                                        pszDialect );

    /* -------------------------------------------------------------------- */
    /*      Special case DELLAYER: command.                                 */
    /* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        OGRLayer* poLayer = GetLayerByName(pszLayerName);

        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer] == poLayer )
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return NULL;
    }

    CPLDebug( "OGRDB2DataSource::ExecuteSQL", "ExecuteSQL(%s) called.",
              pszSQLCommand );


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

    OGRDB2SelectLayer *poLayer = NULL;

    poLayer = new OGRDB2SelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRDB2DataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            ToUpper()                                 */
/************************************************************************/

char *OGRDB2DataSource::ToUpper( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );
    int     i;

    for( i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) toupper( pszSafeName[i] );
    }

    return pszSafeName;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRDB2DataSource::LaunderName( const char *pszSrcName )

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

OGRErr OGRDB2DataSource::InitializeMetadataTables()

{
    CPLDebug( "OGR_DB2DataSource::InitializeMetadataTables", "Not supported");

    CPLError( CE_Failure, CPLE_AppDefined,
              "Dynamically creating DB2 spatial metadata tables is "
              "not supported" );
    return OGRERR_FAILURE;

}


/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRDB2DataSource::FetchSRS( int nId )

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

    OGRSpatialReference *poSRS = NULL;

    /* -------------------------------------------------------------------- */
    /*      Try looking up in spatial_ref_sys table                         */
    /* -------------------------------------------------------------------- */
    if (bUseGeometryColumns)
    {
        CPLODBCStatement oStmt( GetSession() );
        oStmt.Appendf( "SELECT definition FROM "
                       "db2gse.st_spatial_reference_systems "
                       "WHERE srs_id = %d", nId );

        if( oStmt.ExecuteSQL() && oStmt.Fetch() )
        {
            if ( oStmt.GetColData( 0 ) )
            {
                poSRS = new OGRSpatialReference();
                char* pszWKT = (char*)oStmt.GetColData( 0 );
                CPLDebug("OGR_DB2DataSource::FetchSRS", "SRS = %s", pszWKT);
                if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
                {
                    delete poSRS;
                    poSRS = NULL;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try looking up the EPSG list                                    */
    /* -------------------------------------------------------------------- */
    if (!poSRS)
    {
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromEPSG( nId ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Add to the cache.                                               */
    /* -------------------------------------------------------------------- */
    if (poSRS)
    {
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

int OGRDB2DataSource::FetchSRSId( OGRSpatialReference * poSRS)

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
        /* -----------------------------------------------------------------*/
        /*      Try to identify an EPSG code                                */
        /* -----------------------------------------------------------------*/
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
        oStmt.Appendf("SELECT srs_id "
                      "FROM db2gse.st_spatial_reference_systems WHERE "
                      "organization = '%s' AND organization_coordsys_id = %d",
                      pszAuthorityName,
                      nAuthorityCode );

        if( oStmt.ExecuteSQL() && oStmt.Fetch() && oStmt.GetColData( 0 ) )
        {
            nSRSId = atoi(oStmt.GetColData( 0 ));
            CPLDebug("OGR_DB2DataSource::FetchSRSId", "nSRSId = %d", nSRSId);
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

    oStmt.Append( "SELECT srs_id FROM db2gse.st_spatial_reference_systems "
                  "WHERE description = ");
    OGRDB2AppendEscaped(&oStmt, pszWKT);

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
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Didn't find srs_id for %s", pszWKT );
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
        oStmt.Appendf("SELECT srid FROM spatial_ref_sys where srid = %d",
                      nAuthorityCode);
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
        oStmt.Append("SELECT COALESCE(MAX(srid) + 1, 32768) "
                     "FROM spatial_ref_sys "
                     "where srid between 32768 and 65536");

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
            "INSERT INTO spatial_ref_sys (srid, auth_srid, auth_name, "
            "srtext, proj4text) "
            "VALUES (%d, %d, ", nSRSId, nAuthorityCode );
        OGRDB2AppendEscaped(&oStmt, pszAuthorityName);
        oStmt.Append(", ");
        OGRDB2AppendEscaped(&oStmt, pszWKT);
        oStmt.Append(", ");
        OGRDB2AppendEscaped(&oStmt, pszProj4);
        oStmt.Append(")");
    }
    else
    {
        oStmt.Appendf(
            "INSERT INTO spatial_ref_sys (srid,srtext,proj4text) "
            "VALUES (%d, ", nSRSId);
        OGRDB2AppendEscaped(&oStmt, pszWKT);
        oStmt.Append(", ");
        OGRDB2AppendEscaped(&oStmt, pszProj4);
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

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if (!oSession.BeginTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to start transaction: %s", oSession.GetLastError());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::CommitTransaction()
{
    if (!oSession.CommitTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to commit transaction: %s",
                  oSession.GetLastError() );

        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer]->GetLayerStatus()
                    == DB2LAYERSTATUS_INITIAL )
                papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_DISABLED);
        }
        return OGRERR_FAILURE;
    }

    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->GetLayerStatus() == DB2LAYERSTATUS_INITIAL )
            papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_CREATED);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::RollbackTransaction()
{
    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->GetLayerStatus() == DB2LAYERSTATUS_INITIAL )
            papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_DISABLED);
    }

    if (!oSession.RollbackTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to roll back transaction: %s",
                  oSession.GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

