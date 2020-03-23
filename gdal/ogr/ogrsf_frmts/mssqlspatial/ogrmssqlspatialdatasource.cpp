/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLSpatialDataSource class..
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRMSSQLSpatialDataSource()                 */
/************************************************************************/

OGRMSSQLSpatialDataSource::OGRMSSQLSpatialDataSource() :
    bDSUpdate(false)
{
    pszName = nullptr;
    pszCatalog = nullptr;
    papoLayers = nullptr;
    nLayers = 0;

    nKnownSRID = 0;
    panSRID = nullptr;
    papoSRS = nullptr;

    poLayerInCopyMode = nullptr;

    nGeometryFormat = MSSQLGEOMETRY_NATIVE;
    pszConnection = nullptr;

    sMSSQLVersion.nMajor = 0;
    sMSSQLVersion.nMinor = 0;
    sMSSQLVersion.nBuild = 0;
    sMSSQLVersion.nRevision = 0;

    bUseGeometryColumns = CPLTestBool(CPLGetConfigOption("MSSQLSPATIAL_USE_GEOMETRY_COLUMNS", "YES"));
    bAlwaysOutputFid = CPLTestBool(CPLGetConfigOption("MSSQLSPATIAL_ALWAYS_OUTPUT_FID", "NO"));
    bListAllTables = CPLTestBool(CPLGetConfigOption("MSSQLSPATIAL_LIST_ALL_TABLES", "NO"));

    const char* nBCPSizeParam = CPLGetConfigOption("MSSQLSPATIAL_BCP_SIZE", nullptr);
    if( nBCPSizeParam != nullptr )
        nBCPSize = atoi(nBCPSizeParam);
    else
        nBCPSize = 1000;
#ifdef MSSQL_BCP_SUPPORTED
    bUseCopy = CPLTestBool(CPLGetConfigOption("MSSQLSPATIAL_USE_BCP", "TRUE"));
#else
    bUseCopy = FALSE;
#endif
    CPLDebug( "MSSQLSpatial", "Use COPY/BCP: %d", bUseCopy );
}

/************************************************************************/
/*                         ~OGRMSSQLSpatialDataSource()                 */
/************************************************************************/

OGRMSSQLSpatialDataSource::~OGRMSSQLSpatialDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    CPLFree( pszName );
    CPLFree( pszCatalog );

    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != nullptr )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
    CPLFree( pszConnection );
}
/************************************************************************/
/*                      OGRMSSQLDecodeVersionString()                   */
/************************************************************************/

void OGRMSSQLSpatialDataSource::OGRMSSQLDecodeVersionString(MSSQLVer* psVersion, const char* pszVer)
{
    while (*pszVer == ' ') pszVer++;

    const char* ptr = pszVer;
    // get Version string
    while (*ptr && *ptr != ' ') ptr++;
    GUInt32 iLen = static_cast<int>(ptr - pszVer);
    char szVer[20] = {};
    if (iLen > sizeof(szVer) - 1) iLen = sizeof(szVer) - 1;
    strncpy(szVer, pszVer, iLen);
    szVer[iLen] = '\0';

    ptr = pszVer = szVer;

    // get Major number
    while (*ptr && *ptr != '.') ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    char szNum[20] = {};
    if (iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nMajor = atoi(szNum);

    if (*ptr == 0)
        return;
    pszVer = ++ptr;

    // get Minor number
    while (*ptr && *ptr != '.') ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    if (iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nMinor = atoi(szNum);

    if (*ptr == 0)
        return;
    pszVer = ++ptr;

    // get Build number
    while (*ptr && *ptr != '.') ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    if (iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nBuild = atoi(szNum);

    if (*ptr == 0)
        return;
    pszVer = ++ptr;

    // get Revision number
    while (*ptr && *ptr != '.') ptr++;
    iLen = static_cast<int>(ptr - pszVer);
    if (iLen > sizeof(szNum) - 1) iLen = sizeof(szNum) - 1;
    strncpy(szNum, pszVer, iLen);
    szNum[iLen] = '\0';
    psVersion->nRevision = atoi(szNum);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialDataSource::TestCapability( const char * pszCap )

{
#if (ODBCVER >= 0x0300)
    if ( EQUAL(pszCap,ODsCTransactions) )
        return TRUE;
#endif
    if( EQUAL(pszCap,ODsCCreateLayer) || EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return TRUE;
    if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
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
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRMSSQLSpatialDataSource::GetLayerByName( const char* pszLayerName )

{
    if (!pszLayerName)
        return nullptr;

    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;

    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != nullptr )
    {
      int length = static_cast<int>(pszDotPos - pszLayerName);
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';
      pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = CPLStrdup("dbo");
      pszTableName = CPLStrdup( pszLayerName );
    }

    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszTableName,papoLayers[iLayer]->GetTableName()) &&
            EQUAL(pszSchemaName,papoLayers[iLayer]->GetSchemaName()) )
        {
            CPLFree( pszSchemaName );
            CPLFree( pszTableName );
            return papoLayers[iLayer];
        }
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );

    return nullptr;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::DeleteLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return OGRERR_FAILURE;

    EndCopy();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    const char* pszTableName = papoLayers[iLayer]->GetTableName();
    const char* pszSchemaName = papoLayers[iLayer]->GetSchemaName();

    CPLODBCStatement oStmt( &oSession );
    if (bUseGeometryColumns)
        oStmt.Appendf( "DELETE FROM geometry_columns WHERE f_table_schema = '%s' AND f_table_name = '%s'\n",
            pszSchemaName, pszTableName );
    oStmt.Appendf("DROP TABLE [%s].[%s]", pszSchemaName, pszTableName );

    CPLDebug( "MSSQLSpatial", "DeleteLayer(%s)", pszTableName );

    papoLayers[iLayer]->SetSpatialIndexFlag(FALSE);

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if ( strlen(pszTableName) == 0 )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */

    int bInTransaction = oSession.IsInTransaction();
    if (!bInTransaction)
        oSession.BeginTransaction();

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Error deleting layer: %s", GetSession()->GetLastError() );

        if (!bInTransaction)
            oSession.RollbackTransaction();

        return OGRERR_FAILURE;
    }

    if (!bInTransaction)
        oSession.CommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRMSSQLSpatialDataSource::ICreateLayer( const char * pszLayerName,
                                          OGRSpatialReference *poSRS,
                                          OGRwkbGeometryType eType,
                                          char ** papszOptions )

{
    char                *pszTableName = nullptr;
    char                *pszSchemaName = nullptr;
    const char          *pszGeomType = nullptr;
    const char          *pszGeomColumn = nullptr;
    int                 nCoordDimension = 3;
    char                *pszFIDColumnName = nullptr;

    EndCopy();

    /* determine the dimension */
    if( eType == wkbFlatten(eType) )
        nCoordDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != nullptr )
        nCoordDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));

    int bExtractSchemaFromLayerName = CPLTestBool(CSLFetchNameValueDef(
                                    papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    /* MSSQL Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema is not
       specified
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != nullptr && bExtractSchemaFromLayerName )
    {
      int length = static_cast<int>(pszDotPos - pszLayerName);
      pszSchemaName = (char*)CPLMalloc(length+1);
      CPLAssert(pszSchemaName != nullptr); /* to make Coverity happy and not believe a REVERSE_INULL is possible */
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';

      if( CPLFetchBool(papszOptions, "LAUNDER", true) )
          pszTableName = LaunderName( pszDotPos + 1 ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      if( CPLFetchBool(papszOptions, "LAUNDER", TRUE) )
          pszTableName = LaunderName( pszLayerName ); //skip "."
      else
          pszTableName = CPLStrdup( pszLayerName ); //skip "."
    }

    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != nullptr )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue( papszOptions, "SCHEMA" ));
    }

    if (pszSchemaName == nullptr)
        pszSchemaName = CPLStrdup("dbo");

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszTableName,papoLayers[iLayer]->GetTableName()) &&
            EQUAL(pszSchemaName,papoLayers[iLayer]->GetSchemaName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != nullptr
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                CPLFree(pszSchemaName);
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
                return nullptr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    if ( eType != wkbNone )
    {
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
            return nullptr;
        }

        /* determine the geometry column name */
        pszGeomColumn =  CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        if (!pszGeomColumn)
            pszGeomColumn =  CSLFetchNameValue( papszOptions, "GEOM_NAME");
        if (!pszGeomColumn)
            pszGeomColumn = "ogr_geometry";
    }
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);

/* -------------------------------------------------------------------- */
/*      Initialize the metadata tables                                  */
/* -------------------------------------------------------------------- */

    if (InitializeMetadataTables() != OGRERR_NONE)
    {
        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = 0;

    if( CSLFetchNameValue( papszOptions, "SRID") != nullptr )
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));

    if( nSRSId == 0 && poSRS != nullptr )
        nSRSId = FetchSRSId( poSRS );

/* -------------------------------------------------------------------- */
/*      Create a new table and create a new entry in the geometry,      */
/*      geometry_columns metadata table.                                */
/* -------------------------------------------------------------------- */

    CPLODBCStatement oStmt( &oSession );

    if( eType != wkbNone && bUseGeometryColumns)
    {
        const char *pszGeometryType = OGRToOGCGeomType(eType);

        oStmt.Appendf( "DELETE FROM geometry_columns WHERE f_table_schema = '%s' "
            "AND f_table_name = '%s'\n", pszSchemaName, pszTableName );

        oStmt.Appendf("INSERT INTO [geometry_columns] ([f_table_catalog], [f_table_schema] ,[f_table_name], "
            "[f_geometry_column],[coord_dimension],[srid],[geometry_type]) VALUES ('%s', '%s', '%s', '%s', %d, %d, '%s')\n",
            pszCatalog, pszSchemaName, pszTableName, pszGeomColumn, nCoordDimension, nSRSId, pszGeometryType );
    }

    if (!EQUAL(pszSchemaName,"dbo"))
    {
        // creating the schema if not exists
        oStmt.Appendf("IF NOT EXISTS (SELECT name from sys.schemas WHERE name = '%s') EXEC sp_executesql N'CREATE SCHEMA [%s]'\n", pszSchemaName, pszSchemaName);
    }

     /* determine the FID column name */
    const char* pszFIDColumnNameIn = CSLFetchNameValueDef(papszOptions, "FID", "ogr_fid");
    if( CPLFetchBool(papszOptions, "LAUNDER", TRUE) )
        pszFIDColumnName = LaunderName( pszFIDColumnNameIn );
    else
        pszFIDColumnName = CPLStrdup( pszFIDColumnNameIn );

    const bool bFID64 = CPLFetchBool(papszOptions, "FID64", FALSE);
    const char* pszFIDType = bFID64 ? "bigint": "int";

    if( eType == wkbNone )
    {
        oStmt.Appendf("CREATE TABLE [%s].[%s] ([%s] [%s] IDENTITY(1,1) NOT NULL, "
            "CONSTRAINT [PK_%s] PRIMARY KEY CLUSTERED ([%s] ASC))",
            pszSchemaName, pszTableName, pszFIDColumnName, pszFIDType, pszTableName, pszFIDColumnName);
    }
    else
    {
        oStmt.Appendf("CREATE TABLE [%s].[%s] ([%s] [%s] IDENTITY(1,1) NOT NULL, "
            "[%s] [%s] %s, CONSTRAINT [PK_%s] PRIMARY KEY CLUSTERED ([%s] ASC))",
            pszSchemaName, pszTableName, pszFIDColumnName, pszFIDType, pszGeomColumn, pszGeomType,
            bGeomNullable ? "NULL":"NOT NULL", pszTableName, pszFIDColumnName);
    }

    CPLFree( pszFIDColumnName );

    int bInTransaction = oSession.IsInTransaction();
    if (!bInTransaction)
        oSession.BeginTransaction();

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Error creating layer: %s When using the overwrite option and the layer doesn't contain geometry column, you might require to use the MSSQLSPATIAL_LIST_ALL_TABLES config option to get the previous layer deleted before creating the new one.", GetSession()->GetLastError() );

        if (!bInTransaction)
            oSession.RollbackTransaction();

        return nullptr;
    }

    if (!bInTransaction)
        oSession.CommitTransaction();

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMSSQLSpatialTableLayer   *poLayer;

    poLayer = new OGRMSSQLSpatialTableLayer( this );

    if (bInTransaction)
        poLayer->SetLayerStatus(MSSQLLAYERSTATUS_INITIAL);
    else
        poLayer->SetLayerStatus(MSSQLLAYERSTATUS_CREATED);

    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    poLayer->SetPrecisionFlag( CPLFetchBool(papszOptions, "PRECISION", true));

    if( bUseCopy )
        poLayer->SetUseCopy(nBCPSize);

    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    int bCreateSpatialIndex = ( pszSI == nullptr || CPLTestBool(pszSI) );
    if (pszGeomColumn == nullptr)
        bCreateSpatialIndex = FALSE;

    poLayer->SetSpatialIndexFlag( bCreateSpatialIndex );

    const char *pszUploadGeometryFormat = CSLFetchNameValue( papszOptions, "UPLOAD_GEOM_FORMAT" );
    if (pszUploadGeometryFormat)
    {
        if (STARTS_WITH_CI(pszUploadGeometryFormat,"wkb"))
            poLayer->SetUploadGeometryFormat(MSSQLGEOMETRY_WKB);
        else if (STARTS_WITH_CI(pszUploadGeometryFormat, "wkt"))
            poLayer->SetUploadGeometryFormat(MSSQLGEOMETRY_WKT);
    }

    char *pszWKT = nullptr;
    if( poSRS && poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        pszWKT = nullptr;
    }

    if( bFID64 )
        poLayer->SetMetadataItem(OLMD_FID64, "YES");

    if (poLayer->Initialize(pszSchemaName, pszTableName, pszGeomColumn, nCoordDimension, nSRSId, pszWKT, eType) == CE_Failure)
    {
        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        CPLFree( pszWKT );
        return nullptr;
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );
    CPLFree( pszWKT );

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
                                          int nSRID, const char *pszSRText, OGRwkbGeometryType eType,
                                          bool bUpdate )
{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

    if( poLayer->Initialize( pszSchemaName, pszTableName, pszGeomCol, nCoordDimension, nSRID, pszSRText, eType ) )
    {
        delete poLayer;
        return FALSE;
    }
    poLayer->SetUpdate(bUpdate);

    if (bUseCopy)
        poLayer->SetUseCopy(nBCPSize);

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
    int nLen = static_cast<int>(strlen(pszKey));
    if ((*pszValue) == nullptr && nStart + nLen < nNext &&
            EQUALN(pszSource + nStart, pszKey, nLen))
    {
        *pszValue = (char*)CPLMalloc( sizeof(char) * (nNext - nStart - nLen + 1) );
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

int OGRMSSQLSpatialDataSource::Open( const char * pszNewName, bool bUpdate,
                             int bTestOpen )

{
    CPLAssert( nLayers == 0 );

    if( !STARTS_WITH_CI(pszNewName, "MSSQL:") )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to MSSSQLSpatial naming convention,"
                      " MSSQL:*\n", pszNewName );
        return FALSE;
    }

    /* Determine if the connection string contains specific values */
    char* pszTableSpec = nullptr;
    char* pszGeometryFormat = nullptr;
    char* pszConnectionName = CPLStrdup(pszNewName + 6);
    char* pszDriver = nullptr;
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

        if (ParseValue(&pszGeometryFormat, pszConnectionName,
            "geometryformat=", nCurrent, nNext, nTerm, TRUE))
        {
            if (STARTS_WITH_CI(pszGeometryFormat, "wkbzm"))
                nGeometryFormat = MSSQLGEOMETRY_WKBZM;
            else if (STARTS_WITH_CI(pszGeometryFormat, "wkb"))
                nGeometryFormat = MSSQLGEOMETRY_WKB;
            else if (STARTS_WITH_CI(pszGeometryFormat, "wkt"))
                nGeometryFormat = MSSQLGEOMETRY_WKT;
            else if (STARTS_WITH_CI(pszGeometryFormat, "native"))
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
            pszGeometryFormat = nullptr;
            continue;
        }
    }

    /* Determine if the connection string contains the catalog portion */
    if( pszCatalog == nullptr )
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

    char  **papszTableNames=nullptr;
    char  **papszSchemaNames=nullptr;
    char  **papszGeomColumnNames=nullptr;
    char  **papszCoordDimensions=nullptr;
    char  **papszSRIds=nullptr;
    char  **papszSRTexts=nullptr;

    /* Determine if the connection string contains the TABLES portion */
    if( pszTableSpec != nullptr )
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
                char* pszGeomColumnName = nullptr;
                char* pos = strchr(papszQualifiedParts[CSLCount( papszQualifiedParts ) - 1], '(');
                if (pos != nullptr)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos+1;
                    int len = static_cast<int>(strlen(pszGeomColumnName));
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

    if ( pszDriver == nullptr )
    {
        char* pszConnectionName2 = pszConnectionName;
#if SQLNCLI_VERSION == 11
        pszDriver = CPLStrdup("{SQL Server Native Client 11.0}");
#elif SQLNCLI_VERSION == 10
        pszDriver = CPLStrdup("{SQL Server Native Client 10.0}");
#elif MSODBCSQL_VERSION == 13
        pszDriver = CPLStrdup("{ODBC Driver 13 for SQL Server}");
#elif MSODBCSQL_VERSION == 17
        pszDriver = CPLStrdup("{ODBC Driver 17 for SQL Server}");
#else
        pszDriver = CPLStrdup("{SQL Server}");
#endif
        pszConnectionName = CPLStrdup(CPLSPrintf("DRIVER=%s;%s", pszDriver, pszConnectionName2));
        CPLFree(pszConnectionName2);
    }

    CPLFree(pszDriver);

    /* Initialize the SQL Server connection. */
    if( !oSession.EstablishSession( pszConnectionName, "", "" ) )
    {
        /* Get a list of the available drivers */
        HENV hEnv;
        if ( SQL_SUCCEEDED(SQLAllocEnv( &hEnv ) ) )
        {
            CPLString osDriverList;
            SQLUSMALLINT direction = SQL_FETCH_FIRST;
            SQLSMALLINT driver_ret;
            SQLSMALLINT attr_ret;
            SQLCHAR attr[256];
            SQLCHAR driver[256];
            while(SQL_SUCCEEDED(SQLDrivers(hEnv, direction,
                driver, sizeof(driver), &driver_ret, attr, sizeof(attr), &attr_ret)))
            {
                direction = SQL_FETCH_NEXT;
                osDriverList += CPLSPrintf("%s\n", driver);
            }

            CPLError( CE_Failure, CPLE_AppDefined,
                "Unable to initialize connection to the server for %s,\n"
                "%s\n"
                "Try specifying the driver in the connection string from the list of available drivers:\n"
                "%s", pszNewName, oSession.GetLastError(), osDriverList.c_str() );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "Unable to initialize connection to the server for %s,\n"
                "%s\n", pszNewName, oSession.GetLastError() );
        }

        if( hEnv != nullptr )
            SQLFreeEnv( hEnv );

        CSLDestroy( papszTableNames );
        CSLDestroy( papszSchemaNames );
        CSLDestroy( papszGeomColumnNames );
        CSLDestroy( papszCoordDimensions );
        CSLDestroy( papszSRIds );
        CSLDestroy( papszSRTexts );
        CPLFree(pszGeometryFormat);
        CPLFree(pszConnectionName);
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Find out SQLServer version                                      */
    /* -------------------------------------------------------------------- */
    if (true) {
        sMSSQLVersion.nMajor = -1;
        sMSSQLVersion.nMinor = -1;
        sMSSQLVersion.nBuild = -1;
        sMSSQLVersion.nRevision = -1;

        CPLODBCStatement oStmt(&oSession);

        /* Use join to make sure the existence of the referred column/table */
        oStmt.Append("SELECT SERVERPROPERTY('ProductVersion') AS ProductVersion;");

        if (oStmt.ExecuteSQL())
        {
            while (oStmt.Fetch())
            {
                OGRMSSQLDecodeVersionString(&sMSSQLVersion, oStmt.GetColData(0));
            }
        }
    }

    char** papszTypes = nullptr;

    /* read metadata for the specified tables */
    if (papszTableNames != nullptr && bUseGeometryColumns)
    {
        for( int iTable = 0;
            papszTableNames[iTable] != nullptr;
            iTable++ )
        {
            CPLODBCStatement oStmt( &oSession );

            /* Use join to make sure the existence of the referred column/table */
            oStmt.Appendf( "SELECT f_geometry_column, coord_dimension, g.srid, srtext, geometry_type FROM dbo.geometry_columns g JOIN INFORMATION_SCHEMA.COLUMNS ON f_table_schema = TABLE_SCHEMA and f_table_name = TABLE_NAME and f_geometry_column = COLUMN_NAME left outer join dbo.spatial_ref_sys s on g.srid = s.srid WHERE f_table_schema = '%s' AND f_table_name = '%s'", papszSchemaNames[iTable], papszTableNames[iTable]);

            if( oStmt.ExecuteSQL() )
            {
                while( oStmt.Fetch() )
                {
                    if (papszGeomColumnNames == nullptr)
                            papszGeomColumnNames = CSLAddString( papszGeomColumnNames, oStmt.GetColData(0) );
                    else if (*papszGeomColumnNames[iTable] == 0)
                    {
                        CPLFree(papszGeomColumnNames[iTable]);
                        papszGeomColumnNames[iTable] = CPLStrdup( oStmt.GetColData(0) );
                    }

                    papszCoordDimensions =
                            CSLAddString( papszCoordDimensions, oStmt.GetColData(1, "2") );
                    papszSRIds =
                            CSLAddString( papszSRIds, oStmt.GetColData(2, "0") );
                    papszSRTexts =
                        CSLAddString( papszSRTexts, oStmt.GetColData(3, "") );
                    papszTypes =
                            CSLAddString( papszTypes, oStmt.GetColData(4, "GEOMETRY") );
                }
            }
            else
            {
                /* probably the table is missing at all */
                InitializeMetadataTables();
            }
        }
    }

    /* if requesting all user database table then this takes priority */
    if (papszTableNames == nullptr && bListAllTables)
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "select sys.schemas.name, sys.schemas.name + '.' + sys.objects.name, sys.columns.name from sys.columns join sys.types on sys.columns.system_type_id = sys.types.system_type_id and sys.columns.user_type_id = sys.types.user_type_id join sys.objects on sys.objects.object_id = sys.columns.object_id join sys.schemas on sys.objects.schema_id = sys.schemas.schema_id where (sys.types.name = 'geometry' or sys.types.name = 'geography') and (sys.objects.type = 'U' or sys.objects.type = 'V') union all select sys.schemas.name, sys.schemas.name + '.' + sys.objects.name, '' from sys.objects join sys.schemas on sys.objects.schema_id = sys.schemas.schema_id where not exists (select * from sys.columns sc1 join sys.types on sc1.system_type_id = sys.types.system_type_id where (sys.types.name = 'geometry' or sys.types.name = 'geography') and sys.objects.object_id = sc1.object_id) and (sys.objects.type = 'U' or sys.objects.type = 'V')" );

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
            }
        }
    }

    /* Determine the available tables if not specified. */
    if (papszTableNames == nullptr && bUseGeometryColumns)
    {
        CPLODBCStatement oStmt( &oSession );

        /* Use join to make sure the existence of the referred column/table */
        oStmt.Append( "SELECT f_table_schema, f_table_name, f_geometry_column, coord_dimension, g.srid, srtext, geometry_type FROM dbo.geometry_columns g JOIN INFORMATION_SCHEMA.COLUMNS ON f_table_schema = TABLE_SCHEMA and f_table_name = TABLE_NAME and f_geometry_column = COLUMN_NAME left outer join dbo.spatial_ref_sys s on g.srid = s.srid");

        if( oStmt.ExecuteSQL() )
        {
            while( oStmt.Fetch() )
            {
                papszSchemaNames =
                        CSLAddString( papszSchemaNames, oStmt.GetColData(0, "dbo") );
                papszTableNames =
                        CSLAddString( papszTableNames, oStmt.GetColData(1) );
                papszGeomColumnNames =
                        CSLAddString( papszGeomColumnNames, oStmt.GetColData(2) );
                papszCoordDimensions =
                        CSLAddString( papszCoordDimensions, oStmt.GetColData(3, "2") );
                papszSRIds =
                        CSLAddString( papszSRIds, oStmt.GetColData(4, "0") );
                papszSRTexts =
                    CSLAddString( papszSRTexts, oStmt.GetColData(5, "") );
                papszTypes =
                        CSLAddString( papszTypes, oStmt.GetColData(6, "GEOMETRY") );
            }
        }
        else
        {
            /* probably the table is missing at all */
            InitializeMetadataTables();
        }
    }

    /* Query catalog for tables having geometry columns */
    if (papszTableNames == nullptr)
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT sys.schemas.name, sys.schemas.name + '.' + sys.objects.name, sys.columns.name from sys.columns join sys.types on sys.columns.system_type_id = sys.types.system_type_id and sys.columns.user_type_id = sys.types.user_type_id join sys.objects on sys.objects.object_id = sys.columns.object_id join sys.schemas on sys.objects.schema_id = sys.schemas.schema_id where (sys.types.name = 'geometry' or sys.types.name = 'geography') and (sys.objects.type = 'U' or sys.objects.type = 'V')");

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
            }
        }
    }

    int nSRId, nCoordDimension;
    OGRwkbGeometryType eType;

    for( int iTable = 0;
         papszTableNames != nullptr && papszTableNames[iTable] != nullptr;
         iTable++ )
    {
        if (papszSRIds != nullptr)
            nSRId = atoi(papszSRIds[iTable]);
        else
            nSRId = 0;

        if (papszCoordDimensions != nullptr)
            nCoordDimension = atoi(papszCoordDimensions[iTable]);
        else
            nCoordDimension = 2;

        if (papszTypes != nullptr)
            eType = OGRFromOGCGeomType(papszTypes[iTable]);
        else
            eType = wkbUnknown;

        CPLAssert(papszGeomColumnNames && papszGeomColumnNames[iTable]);
        if( strlen(papszGeomColumnNames[iTable]) > 0 )
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable], papszGeomColumnNames[iTable],
                    nCoordDimension, nSRId, papszSRTexts? papszSRTexts[iTable] : nullptr, eType, bUpdate );
        else
            OpenTable( papszSchemaNames[iTable], papszTableNames[iTable], nullptr,
                    nCoordDimension, nSRId, papszSRTexts? papszSRTexts[iTable] : nullptr, wkbNone, bUpdate );
    }

    CSLDestroy( papszTableNames );
    CSLDestroy( papszSchemaNames );
    CSLDestroy( papszGeomColumnNames );
    CSLDestroy( papszCoordDimensions );
    CSLDestroy( papszSRIds );
    CSLDestroy( papszSRTexts );
    CSLDestroy( papszTypes );

    CPLFree(pszGeometryFormat);

    CPLFree(pszConnection);
    pszConnection = pszConnectionName;

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
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
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
        return nullptr;
    }

    CPLDebug( "MSSQLSpatial", "ExecuteSQL(%s) called.", pszSQLCommand );

    if( STARTS_WITH_CI(pszSQLCommand, "DROP SPATIAL INDEX ON ") )
    {
        /* Handle command to drop a spatial index. */
        OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

        if (poLayer)
        {
            if( poLayer->Initialize( nullptr, pszSQLCommand + 22, nullptr, 0, 0, nullptr, wkbUnknown ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to initialize layer '%s'", pszSQLCommand + 22 );
            }
            poLayer->DropSpatialIndex();
            delete poLayer;
        }
        return nullptr;
    }
    else if( STARTS_WITH_CI(pszSQLCommand, "CREATE SPATIAL INDEX ON ") )
    {
        /* Handle command to create a spatial index. */
        OGRMSSQLSpatialTableLayer  *poLayer = new OGRMSSQLSpatialTableLayer( this );

        if (poLayer)
        {
            if( poLayer->Initialize( nullptr, pszSQLCommand + 24, nullptr, 0, 0, nullptr, wkbUnknown ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to initialize layer '%s'", pszSQLCommand + 24 );
            }
            poLayer->CreateSpatialIndex();
            delete poLayer;
        }
        return nullptr;
    }

    /* Execute the command natively */
    CPLODBCStatement *poStmt = new CPLODBCStatement( &oSession );
    poStmt->Append( pszSQLCommand );

    if( !poStmt->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", oSession.GetLastError() );
        delete poStmt;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poStmt->GetColCount() == 0 )
    {
        delete poStmt;
        CPLErrorReset();
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */

    OGRMSSQLSpatialSelectLayer* poLayer =
        new OGRMSSQLSpatialSelectLayer( this, poStmt );

    if( poSpatialFilter != nullptr )
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
    if (bUseGeometryColumns)
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

        int bInTransaction = oSession.IsInTransaction();
        if (!bInTransaction)
            oSession.BeginTransaction();

        if( !oStmt.ExecuteSQL() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "Error initializing the metadata tables : %s", GetSession()->GetLastError() );

            if (!bInTransaction)
                oSession.RollbackTransaction();

            return OGRERR_FAILURE;
        }

        if (!bInTransaction)
            oSession.CommitTransaction();
    }

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

    EndCopy();

    OGRSpatialReference *poSRS = nullptr;

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table                         */
/* -------------------------------------------------------------------- */
    if (bUseGeometryColumns)
    {
        CPLODBCStatement oStmt( GetSession() );
        oStmt.Appendf( "SELECT srtext FROM spatial_ref_sys WHERE srid = %d", nId );

        if( oStmt.ExecuteSQL() && oStmt.Fetch() )
        {
            if ( oStmt.GetColData( 0 ) )
            {
                poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                const char* pszWKT = oStmt.GetColData( 0 );
                if( poSRS->importFromWkt( pszWKT ) != OGRERR_NONE )
                {
                    delete poSRS;
                    poSRS = nullptr;
                }
                else
                {
                    const char* pszAuthorityName = poSRS->GetAuthorityName(nullptr);
                    const char* pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);
                    if( pszAuthorityName && pszAuthorityCode &&
                        EQUAL(pszAuthorityName, "EPSG") )
                    {
                        const int nCode = atoi(pszAuthorityCode);
                        poSRS->Clear();
                        poSRS->importFromEPSG(nCode);
                    }
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
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSRS->importFromEPSG( nId ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
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

int OGRMSSQLSpatialDataSource::FetchSRSId( OGRSpatialReference * poSRS)

{
    char                *pszWKT = nullptr;
    int                 nSRSId = 0;
    const char*         pszAuthorityName;

    if( poSRS == nullptr )
        return 0;

    OGRSpatialReference oSRS(*poSRS);
    // cppcheck-suppress uselessAssignmentPtrArg
    poSRS = nullptr;

    pszAuthorityName = oSRS.GetAuthorityName(nullptr);

    if( pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(nullptr);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    int  nAuthorityCode = 0;
    if( pszAuthorityName != nullptr && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        nAuthorityCode = atoi( oSRS.GetAuthorityCode(nullptr) );

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
    char    *pszProj4 = nullptr;
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

    int bInTransaction = oSession.IsInTransaction();
    if (!bInTransaction)
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
        if (!bInTransaction)
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
    {
        if (!bInTransaction)
            oSession.CommitTransaction();
    }
    else
    {
        if (!bInTransaction)
            oSession.RollbackTransaction();
    }

    return nSRSId;
}

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if (!oSession.BeginTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to start transaction: %s", oSession.GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::CommitTransaction()
{
    if (!oSession.CommitTransaction())
     {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to commit transaction: %s", oSession.GetLastError() );

        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer]->GetLayerStatus() == MSSQLLAYERSTATUS_INITIAL )
                papoLayers[iLayer]->SetLayerStatus(MSSQLLAYERSTATUS_DISABLED);
        }
        return OGRERR_FAILURE;
    }

    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->GetLayerStatus() == MSSQLLAYERSTATUS_INITIAL )
            papoLayers[iLayer]->SetLayerStatus(MSSQLLAYERSTATUS_CREATED);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::RollbackTransaction()
{
    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->GetLayerStatus() == MSSQLLAYERSTATUS_INITIAL )
            papoLayers[iLayer]->SetLayerStatus(MSSQLLAYERSTATUS_DISABLED);
    }

    if (!oSession.RollbackTransaction())
     {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to roll back transaction: %s", oSession.GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

void OGRMSSQLSpatialDataSource::StartCopy(OGRMSSQLSpatialTableLayer *poMSSQLSpatialLayer)
{
    if (poLayerInCopyMode == poMSSQLSpatialLayer)
        return;
    EndCopy();
    poLayerInCopyMode = poMSSQLSpatialLayer;
    poLayerInCopyMode->StartCopy();
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRMSSQLSpatialDataSource::EndCopy()
{
    if (poLayerInCopyMode != nullptr)
    {
        OGRErr result = poLayerInCopyMode->EndCopy();
        poLayerInCopyMode = nullptr;

        return result;
    }
    else
        return OGRERR_NONE;
}
