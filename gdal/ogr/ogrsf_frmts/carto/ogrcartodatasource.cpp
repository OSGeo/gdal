/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTODataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_carto.h"
#include "ogr_pgdump.h"
#include "ogrgeojsonreader.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRCARTODataSource()                        */
/************************************************************************/

OGRCARTODataSource::OGRCARTODataSource() :
    pszName(NULL),
    pszAccount(NULL),
    papoLayers(NULL),
    nLayers(0),
    bReadWrite(false),
    bBatchInsert(true),
    bUseHTTPS(false),
    bMustCleanPersistent(false),
    bHasOGRMetadataFunction(-1),
    nPostGISMajor(2),
    nPostGISMinor(0)
{}

/************************************************************************/
/*                       ~OGRCARTODataSource()                        */
/************************************************************************/

OGRCARTODataSource::~OGRCARTODataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if( bMustCleanPersistent )
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("CARTO:%p", this));
        CPLHTTPDestroyResult( CPLHTTPFetch( GetAPIURL(), papszOptions) );
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
    CPLFree( pszAccount );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODataSource::TestCapability( const char * pszCap )

{
    if( bReadWrite && EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( bReadWrite && EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bReadWrite;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCARTODataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRCARTODataSource::GetLayerByName(const char * pszLayerName)
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszLayerName);
    return poLayer;
}

/************************************************************************/
/*                     OGRCARTOGetOptionValue()                       */
/************************************************************************/

static CPLString OGRCARTOGetOptionValue(const char* pszFilename,
                               const char* pszOptionName)
{
    CPLString osOptionName(pszOptionName);
    osOptionName += "=";
    const char* pszOptionValue = strstr(pszFilename, osOptionName);
    if (!pszOptionValue)
        return "";

    CPLString osOptionValue(pszOptionValue + osOptionName.size());
    const char* pszSpace = strchr(osOptionValue.c_str(), ' ');
    if (pszSpace)
        osOptionValue.resize(pszSpace - osOptionValue.c_str());
    return osOptionValue;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCARTODataSource::Open( const char * pszFilename,
                                char** papszOpenOptionsIn,
                                int bUpdateIn )

{
    bReadWrite = CPL_TO_BOOL(bUpdateIn);
    bBatchInsert = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptionsIn, "BATCH_INSERT", "YES"));

    pszName = CPLStrdup( pszFilename );
    if( CSLFetchNameValue(papszOpenOptionsIn, "ACCOUNT") )
        pszAccount = CPLStrdup(CSLFetchNameValue(papszOpenOptionsIn, "ACCOUNT"));
    else
    {
        if( STARTS_WITH_CI(pszFilename, "CARTODB:") )
            pszAccount = CPLStrdup(pszFilename + strlen("CARTODB:"));
        else
            pszAccount = CPLStrdup(pszFilename + strlen("CARTO:"));
        char* pchSpace = strchr(pszAccount, ' ');
        if( pchSpace )
            *pchSpace = '\0';
        if( pszAccount[0] == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing account name");
            return FALSE;
        }
    }

    osAPIKey = CSLFetchNameValueDef(
        papszOpenOptionsIn, "API_KEY",
        CPLGetConfigOption("CARTO_API_KEY",
                           CPLGetConfigOption("CARTODB_API_KEY", "")));

    CPLString osTables = OGRCARTOGetOptionValue(pszFilename, "tables");

    /*if( osTables.empty() && osAPIKey.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "When not specifying tables option, CARTO_API_KEY must be defined");
        return FALSE;
    }*/

    bUseHTTPS = CPLTestBool(
        CPLGetConfigOption("CARTO_HTTPS",
                           CPLGetConfigOption("CARTODB_HTTPS", "YES")));

    OGRLayer* poSchemaLayer = ExecuteSQLInternal("SELECT current_schema()");
    if( poSchemaLayer )
    {
        OGRFeature* poFeat = poSchemaLayer->GetNextFeature();
        if( poFeat )
        {
            if( poFeat->GetFieldCount() == 1 )
            {
                osCurrentSchema = poFeat->GetFieldAsString(0);
            }
            delete poFeat;
        }
        ReleaseResultSet(poSchemaLayer);
    }
    if( osCurrentSchema.empty() )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Find out PostGIS version                                        */
/* -------------------------------------------------------------------- */
    if( bReadWrite )
    {
        OGRLayer* poPostGISVersionLayer = ExecuteSQLInternal("SELECT postgis_version()");
        if( poPostGISVersionLayer )
        {
            OGRFeature* poFeat = poPostGISVersionLayer->GetNextFeature();
            if( poFeat )
            {
                if( poFeat->GetFieldCount() == 1 )
                {
                    const char* pszVersion = poFeat->GetFieldAsString(0);
                    nPostGISMajor = atoi(pszVersion);
                    const char* pszDot = strchr(pszVersion, '.');
                    nPostGISMinor = 0;
                    if( pszDot )
                        nPostGISMinor = atoi(pszDot + 1);
                }
                delete poFeat;
            }
            ReleaseResultSet(poPostGISVersionLayer);
        }
    }

    if( !osAPIKey.empty() && bUpdateIn )
    {
        ExecuteSQLInternal(
                "DROP FUNCTION IF EXISTS ogr_table_metadata(TEXT,TEXT); "
                "CREATE OR REPLACE FUNCTION ogr_table_metadata(schema_name TEXT, table_name TEXT) RETURNS TABLE "
                "(attname TEXT, typname TEXT, attlen INT, format_type TEXT, "
                "attnum INT, attnotnull BOOLEAN, indisprimary BOOLEAN, "
                "defaultexpr TEXT, dim INT, srid INT, geomtyp TEXT, srtext TEXT) AS $$ "
                "SELECT a.attname::text, t.typname::text, a.attlen::int, "
                        "format_type(a.atttypid,a.atttypmod)::text, "
                        "a.attnum::int, "
                        "a.attnotnull::boolean, "
                        "i.indisprimary::boolean, "
                        "pg_get_expr(def.adbin, c.oid)::text AS defaultexpr, "
                        "(CASE WHEN t.typname = 'geometry' THEN postgis_typmod_dims(a.atttypmod) ELSE NULL END)::int dim, "
                        "(CASE WHEN t.typname = 'geometry' THEN postgis_typmod_srid(a.atttypmod) ELSE NULL END)::int srid, "
                        "(CASE WHEN t.typname = 'geometry' THEN postgis_typmod_type(a.atttypmod) ELSE NULL END)::text geomtyp, "
                        "srtext "
                "FROM pg_class c "
                "JOIN pg_attribute a ON a.attnum > 0 AND "
                                        "a.attrelid = c.oid AND c.relname = $2 "
                                        "AND c.relname IN (SELECT CDB_UserTables())"
                "JOIN pg_type t ON a.atttypid = t.oid "
                "JOIN pg_namespace n ON c.relnamespace=n.oid AND n.nspname = $1 "
                "LEFT JOIN pg_index i ON c.oid = i.indrelid AND "
                                        "i.indisprimary = 't' AND a.attnum = ANY(i.indkey) "
                "LEFT JOIN pg_attrdef def ON def.adrelid = c.oid AND "
                                            "def.adnum = a.attnum "
                "LEFT JOIN spatial_ref_sys srs ON srs.srid = postgis_typmod_srid(a.atttypmod) "
                "ORDER BY a.attnum "
                "$$ LANGUAGE SQL");
    }

    if (!osTables.empty())
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRCARTOTableLayer**) CPLRealloc(
                papoLayers, (nLayers + 1) * sizeof(OGRCARTOTableLayer*));
            papoLayers[nLayers ++] = new OGRCARTOTableLayer(this, papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }

    OGRLayer* poTableListLayer = ExecuteSQLInternal("SELECT CDB_UserTables()");
    if( poTableListLayer )
    {
        OGRFeature* poFeat;
        while( (poFeat = poTableListLayer->GetNextFeature()) != NULL )
        {
            if( poFeat->GetFieldCount() == 1 )
            {
                papoLayers = (OGRCARTOTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRCARTOTableLayer*));
                papoLayers[nLayers ++] = new OGRCARTOTableLayer(
                            this, poFeat->GetFieldAsString(0));
            }
            delete poFeat;
        }
        ReleaseResultSet(poTableListLayer);
    }
    else if( osCurrentSchema == "public" )
        return FALSE;

    /* There's currently a bug with CDB_UserTables() on multi-user accounts */
    if( nLayers == 0 && osCurrentSchema != "public" )
    {
        CPLString osSQL;
        osSQL.Printf("SELECT c.relname FROM pg_class c, pg_namespace n "
                     "WHERE c.relkind in ('r', 'v') AND c.relname !~ '^pg_' AND c.relnamespace=n.oid AND n.nspname = '%s'",
                     OGRCARTOEscapeLiteral(osCurrentSchema).c_str());
        poTableListLayer = ExecuteSQLInternal(osSQL);
        if( poTableListLayer )
        {
            OGRFeature* poFeat;
            while( (poFeat = poTableListLayer->GetNextFeature()) != NULL )
            {
                if( poFeat->GetFieldCount() == 1 )
                {
                    papoLayers = (OGRCARTOTableLayer**) CPLRealloc(
                        papoLayers, (nLayers + 1) * sizeof(OGRCARTOTableLayer*));
                    papoLayers[nLayers ++] = new OGRCARTOTableLayer(
                                this, poFeat->GetFieldAsString(0));
                }
                delete poFeat;
            }
            ReleaseResultSet(poTableListLayer);
        }
        else
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char* OGRCARTODataSource::GetAPIURL() const
{
    const char* pszAPIURL = CPLGetConfigOption("CARTO_API_URL",
                                CPLGetConfigOption("CARTODB_API_URL", NULL));
    if (pszAPIURL)
        return pszAPIURL;
    else if( bUseHTTPS )
        return CPLSPrintf("https://%s.carto.com/api/v2/sql", pszAccount);
    else
        return CPLSPrintf("http://%s.carto.com/api/v2/sql", pszAccount);
}

/************************************************************************/
/*                             FetchSRSId()                             */
/************************************************************************/

int OGRCARTODataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    const char*         pszAuthorityName;

    if( poSRS == NULL )
        return 0;

    OGRSpatialReference oSRS(*poSRS);
    // cppcheck-suppress uselessAssignmentPtrArg
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
    if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        const int nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );

        return nAuthorityCode;
    }

    return 0;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRCARTODataSource::ICreateLayer( const char *pszNameIn,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if( !bReadWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszNameIn,papoLayers[iLayer]->GetName()) )
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
                          pszNameIn );
                return NULL;
            }
        }
    }

    CPLString osName(pszNameIn);
    if( CPLFetchBool(papszOptions, "LAUNDER", true) )
    {
        char* pszTmp = OGRPGCommonLaunderName(pszNameIn);
        osName = pszTmp;
        CPLFree(pszTmp);
    }

    OGRCARTOTableLayer* poLayer = new OGRCARTOTableLayer(this, osName);
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);
    int nSRID = (poSpatialRef && eGType != wkbNone) ? FetchSRSId( poSpatialRef ) : 0;
    bool bCartoify = CPLFetchBool(papszOptions, "CARTODBFY",
                                  CPLFetchBool(papszOptions, "CARTODBIFY",
                                               true));
    if( bCartoify )
    {
        if( nSRID != 4326 )
        {
            if( eGType != wkbNone )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Cannot register table in dashboard with "
                        "cdb_cartodbfytable() since its SRS is not EPSG:4326");
            }
            bCartoify = false;
        }
    }

    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    poLayer->SetDeferredCreation(eGType, poSpatialRef,
                                 bGeomNullable, bCartoify);
    papoLayers = (OGRCARTOTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRCARTOTableLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCARTODataSource::DeleteLayer(int iLayer)
{
    if( !bReadWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = papoLayers[iLayer]->GetLayerDefn()->GetName();

    CPLDebug( "CARTO", "DeleteLayer(%s)", osLayerName.c_str() );

    int bDeferredCreation = papoLayers[iLayer]->GetDeferredCreation();
    papoLayers[iLayer]->CancelDeferredCreation();
    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if (osLayerName.empty())
        return OGRERR_NONE;

    if( !bDeferredCreation )
    {
        CPLString osSQL;
        osSQL.Printf("DROP TABLE %s",
                    OGRCARTOEscapeIdentifier(osLayerName).c_str());

        json_object* poObj = RunSQL(osSQL);
        if( poObj == NULL )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

char** OGRCARTODataSource::AddHTTPOptions()
{
    bMustCleanPersistent = true;

    return CSLAddString(NULL, CPLSPrintf("PERSISTENT=CARTO:%p", this));
}

/************************************************************************/
/*                               RunSQL()                               */
/************************************************************************/

json_object* OGRCARTODataSource::RunSQL(const char* pszUnescapedSQL)
{
    CPLString osSQL("POSTFIELDS=q=");
    /* Do post escaping */
    for(int i=0;pszUnescapedSQL[i] != 0;i++)
    {
        const int ch = ((unsigned char*)pszUnescapedSQL)[i];
        if (ch != '&' && ch >= 32 && ch < 128)
            osSQL += (char)ch;
        else
            osSQL += CPLSPrintf("%%%02X", ch);
    }

/* -------------------------------------------------------------------- */
/*      Provide the API Key                                             */
/* -------------------------------------------------------------------- */
    if( !osAPIKey.empty() )
    {
        osSQL += "&api_key=";
        osSQL += osAPIKey;
    }

/* -------------------------------------------------------------------- */
/*      Collection the header options and execute request.              */
/* -------------------------------------------------------------------- */
    const char* pszAPIURL = GetAPIURL();
    char** papszOptions = CSLAddString(
        !STARTS_WITH(pszAPIURL, "/vsimem/") ? AddHTTPOptions(): NULL, osSQL);
    CPLHTTPResult * psResult = CPLHTTPFetch( GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult->pszContentType &&
        STARTS_WITH(psResult->pszContentType, "text/html"))
    {
        CPLDebug( "CARTO", "RunSQL HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RunSQL Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RunSQL Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( strlen((const char*)psResult->pabyData) < 1000 )
        CPLDebug( "CARTO", "RunSQL Response:%s", psResult->pabyData );

    json_object* poObj = NULL;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLHTTPDestroyResult(psResult);

    if( poObj != NULL )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = CPL_json_object_object_get(poObj, "error");
            if( poError != NULL && json_object_get_type(poError) == json_type_array &&
                json_object_array_length(poError) > 0 )
            {
                poError = json_object_array_get_idx(poError, 0);
                if( poError != NULL && json_object_get_type(poError) == json_type_string )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Error returned by server : %s", json_object_get_string(poError));
                    json_object_put(poObj);
                    return NULL;
                }
            }
        }
        else
        {
            json_object_put(poObj);
            return NULL;
        }
    }

    return poObj;
}

/************************************************************************/
/*                        OGRCARTOGetSingleRow()                      */
/************************************************************************/

json_object* OGRCARTOGetSingleRow(json_object* poObj)
{
    if( poObj == NULL )
    {
        return NULL;
    }

    json_object* poRows = CPL_json_object_object_get(poObj, "rows");
    if( poRows == NULL ||
        json_object_get_type(poRows) != json_type_array ||
        json_object_array_length(poRows) != 1 )
    {
        return NULL;
    }

    json_object* poRowObj = json_object_array_get_idx(poRows, 0);
    if( poRowObj == NULL || json_object_get_type(poRowObj) != json_type_object )
    {
        return NULL;
    }

    return poRowObj;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRCARTODataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    return ExecuteSQLInternal(pszSQLCommand, poSpatialFilter, pszDialect,
                              true);
}

OGRLayer * OGRCARTODataSource::ExecuteSQLInternal( const char *pszSQLCommand,
                                                   OGRGeometry *poSpatialFilter,
                                                   const char *pszDialect,
                                                   bool bRunDeferredActions )

{
    if( bRunDeferredActions )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoLayers[iLayer]->RunDeferredCreationIfNecessary();
            CPL_IGNORE_RET_VAL(papoLayers[iLayer]->FlushDeferredInsert());
            papoLayers[iLayer]->RunDeferredCartofy();
        }
    }

    /* Skip leading spaces */
    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

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

    if( !STARTS_WITH_CI(pszSQLCommand, "SELECT") &&
        !STARTS_WITH_CI(pszSQLCommand, "EXPLAIN") &&
        !STARTS_WITH_CI(pszSQLCommand, "WITH") )
    {
        RunSQL(pszSQLCommand);
        return NULL;
    }

    OGRCARTOResultLayer* poLayer = new OGRCARTOResultLayer( this, pszSQLCommand );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    if( !poLayer->IsOK() )
    {
        delete poLayer;
        return NULL;
    }

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRCARTODataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
