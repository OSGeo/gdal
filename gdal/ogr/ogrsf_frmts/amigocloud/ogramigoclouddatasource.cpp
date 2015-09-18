/******************************************************************************
 * $Id$
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAMIGOCLOUDDataSource class
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#include "ogr_amigocloud.h"
#include "ogr_pgdump.h"
#include <sstream>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRAMIGOCLOUDDataSource()                        */
/************************************************************************/

OGRAMIGOCLOUDDataSource::OGRAMIGOCLOUDDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
    pszProjetctId = NULL;

    bReadWrite = FALSE;
    bBatchInsert = TRUE;
    bUseHTTPS = FALSE;

    bMustCleanPersistant = FALSE;
    bHasOGRMetadataFunction = -1;
}

/************************************************************************/
/*                       ~OGRAMIGOCLOUDDataSource()                        */
/************************************************************************/

OGRAMIGOCLOUDDataSource::~OGRAMIGOCLOUDDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistant)
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("AMIGOCLOUD:%p", this));
        CPLHTTPFetch( GetAPIURL(), papszOptions);
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
    CPLFree(pszProjetctId);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAMIGOCLOUDDataSource::TestCapability( const char * pszCap )

{
    if( bReadWrite && EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( bReadWrite && EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAMIGOCLOUDDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRAMIGOCLOUDDataSource::GetLayerByName(const char * pszLayerName)
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszLayerName);
    return poLayer;
}

/************************************************************************/
/*                     OGRAMIGOCLOUDGetOptionValue()                       */
/************************************************************************/

CPLString OGRAMIGOCLOUDGetOptionValue(const char* pszFilename,
                               const char* pszOptionName)
{
    CPLString osOptionName(pszOptionName);
    osOptionName += "=";
    const char* pszOptionValue = strstr(pszFilename, osOptionName);
    if (!pszOptionValue)
        return "";

    CPLString osOptionValue(pszOptionValue + strlen(osOptionName));
    const char* pszSpace = strchr(osOptionValue.c_str(), ' ');
    if (pszSpace)
        osOptionValue.resize(pszSpace - osOptionValue.c_str());
    return osOptionValue;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAMIGOCLOUDDataSource::Open( const char * pszFilename,
                                char** papszOpenOptions,
                                int bUpdateIn )

{
    bReadWrite = bUpdateIn;
    bBatchInsert = CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions, "BATCH_INSERT", "YES"));

    pszName = CPLStrdup( pszFilename );
    if( CSLFetchNameValue(papszOpenOptions, "PROJECTID") )
        pszProjetctId = CPLStrdup(CSLFetchNameValue(papszOpenOptions, "PROJECTID"));
    else
    {
        pszProjetctId = CPLStrdup(pszFilename + strlen("AMIGOCLOUD:"));
        char* pchSpace = strchr(pszProjetctId, ' ');
        if( pchSpace )
            *pchSpace = '\0';
        if( pszProjetctId[0] == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing projetc id");
            return FALSE;
        }
    }

    osAPIKey = CSLFetchNameValueDef(papszOpenOptions, "API_KEY",
                                    CPLGetConfigOption("AMIGOCLOUD_API_KEY", ""));

    CPLString osTables = OGRAMIGOCLOUDGetOptionValue(pszFilename, "tables");
    
    /*if( osTables.size() == 0 && osAPIKey.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "When not specifying tables option, AMIGOCLOUD_API_KEY must be defined");
        return FALSE;
    }*/

    bUseHTTPS = CSLTestBoolean(CPLGetConfigOption("AMIGOCLOUD_HTTPS", "YES"));

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
    if( osCurrentSchema.size() == 0 )
        return FALSE;

    if(false && osAPIKey.size() && bUpdateIn )
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
    
    if (osTables.size() != 0)
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRAMIGOCLOUDTableLayer**) CPLRealloc(
                papoLayers, (nLayers + 1) * sizeof(OGRAMIGOCLOUDTableLayer*));
            papoLayers[nLayers ++] = new OGRAMIGOCLOUDTableLayer(this, papszTables[i]);
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
                papoLayers = (OGRAMIGOCLOUDTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRAMIGOCLOUDTableLayer*));
                papoLayers[nLayers ++] = new OGRAMIGOCLOUDTableLayer(
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
                     OGRAMIGOCLOUDEscapeLiteral(osCurrentSchema).c_str());
        poTableListLayer = ExecuteSQLInternal(osSQL);
        if( poTableListLayer )
        {
            OGRFeature* poFeat;
            while( (poFeat = poTableListLayer->GetNextFeature()) != NULL )
            {
                if( poFeat->GetFieldCount() == 1 )
                {
                    papoLayers = (OGRAMIGOCLOUDTableLayer**) CPLRealloc(
                        papoLayers, (nLayers + 1) * sizeof(OGRAMIGOCLOUDTableLayer*));
                    papoLayers[nLayers ++] = new OGRAMIGOCLOUDTableLayer(
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

const char* OGRAMIGOCLOUDDataSource::GetAPIURL() const
{
    const char* pszAPIURL = CPLGetConfigOption("AMIGOCLOUD_API_URL", NULL);
    if (pszAPIURL)
        return pszAPIURL;

    else if (bUseHTTPS)
        return CPLSPrintf("https://www.amigocloud.com/api/v1");
    else
        return CPLSPrintf("http://www.amigocloud.com/api/v1");
}

/************************************************************************/
/*                             FetchSRSId()                             */
/************************************************************************/

int OGRAMIGOCLOUDDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
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
    if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        int             nAuthorityCode;

        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        nAuthorityCode = atoi( oSRS.GetAuthorityCode(NULL) );

        return nAuthorityCode;
    }

    return 0;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRAMIGOCLOUDDataSource::ICreateLayer( const char *pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszName,papoLayers[iLayer]->GetName()) )
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
                          pszName );
                return NULL;
            }
        }
    }
    
    CPLString osName(pszName);
    if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
    {
        char* pszTmp = OGRPGCommonLaunderName(pszName);
        osName = pszTmp;
        CPLFree(pszTmp);
    }

    OGRAMIGOCLOUDTableLayer* poLayer = new OGRAMIGOCLOUDTableLayer(this, osName);
    int bGeomNullable = CSLFetchBoolean(papszOptions, "GEOMETRY_NULLABLE", TRUE);
    int bAmigoCloudify = CSLFetchBoolean(papszOptions, "AMIGOCLOUDIFY",
                                      CSLFetchBoolean(papszOptions, "AMIGOCLOUDIFY", TRUE));
    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetDeferedCreation(eGType, poSpatialRef, bGeomNullable, bAmigoCloudify);
    papoLayers = (OGRAMIGOCLOUDTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRAMIGOCLOUDTableLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRAMIGOCLOUDDataSource::DeleteLayer(int iLayer)
{
    if (!bReadWrite)
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

    CPLDebug( "AMIGOCLOUD", "DeleteLayer(%s)", osLayerName.c_str() );

    int bDeferedCreation = papoLayers[iLayer]->GetDeferedCreation();
    papoLayers[iLayer]->CancelDeferedCreation();
    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if (osLayerName.size() == 0)
        return OGRERR_NONE;

    if( !bDeferedCreation )
    {
        CPLString osSQL;
        osSQL.Printf("DROP TABLE %s",
                    OGRAMIGOCLOUDEscapeIdentifier(osLayerName).c_str());

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

char** OGRAMIGOCLOUDDataSource::AddHTTPOptions()
{
    bMustCleanPersistant = TRUE;

    return CSLAddString(NULL, CPLSPrintf("PERSISTENT=AMIGOCLOUD:%p", this));
}

static std::string url_encode(const std::string &value) {
    std::stringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << '%' << int((unsigned char) c);
    }

    return escaped.str();
}


/************************************************************************/
/*                               RunPOST()                               */
/************************************************************************/

json_object* OGRAMIGOCLOUDDataSource::RunPOST(const char*pszURL, const char *pszPostData, const char *pszHeaders)
{
    CPLString osURL(pszURL);
    printf("RunPOST 1 %s\n", pszURL);
    printf("RunPOST 2 %s\n", pszPostData);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( osAPIKey.size() > 0 )
    {
        osURL += "?token=";
        osURL += osAPIKey;
    }
    char** papszOptions=NULL;
    CPLString osPOSTFIELDS("POSTFIELDS=");
    if (pszPostData)
        osPOSTFIELDS += pszPostData;
    papszOptions = CSLAddString(papszOptions, osPOSTFIELDS);
    papszOptions = CSLAddString(papszOptions, pszHeaders);

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), papszOptions);

    printf("RunPOST result: %s\n", psResult->pabyData);

    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult && psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( strlen((const char*)psResult->pabyData) < 1000 )
        CPLDebug( "AMIGOCLOUD", "RunPOST Response:%s", psResult->pabyData );

    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

    jstok = json_tokener_new();
    poObj = json_tokener_parse_ex(jstok, (const char*) psResult->pabyData, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JSON parsing error: %s (at offset %d)",
                  json_tokener_error_desc(jstok->err), jstok->char_offset);
        json_tokener_free(jstok);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    json_tokener_free(jstok);

    CPLHTTPDestroyResult(psResult);

    if( poObj != NULL )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = json_object_object_get(poObj, "error");
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
/*                               RunGET()                               */
/************************************************************************/

json_object* OGRAMIGOCLOUDDataSource::RunGET(const char*pszURL)
{
    CPLString osURL(pszURL);
    printf("RunGET %s\n", pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( osAPIKey.size() > 0 )
    {
        osURL += "?token=";
        osURL += osAPIKey;
    }

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), NULL);

    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult && psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( strlen((const char*)psResult->pabyData) < 1000 )
        CPLDebug( "AMIGOCLOUD", "RunGET Response:%s", psResult->pabyData );

    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

    jstok = json_tokener_new();
    poObj = json_tokener_parse_ex(jstok, (const char*) psResult->pabyData, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JSON parsing error: %s (at offset %d)",
                  json_tokener_error_desc(jstok->err), jstok->char_offset);
        json_tokener_free(jstok);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    json_tokener_free(jstok);

    CPLHTTPDestroyResult(psResult);

    if( poObj != NULL )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = json_object_object_get(poObj, "error");
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
/*                               RunSQL()                               */
/************************************************************************/

json_object* OGRAMIGOCLOUDDataSource::RunSQL(const char* pszUnescapedSQL)
{
    CPLString osSQL;
    osSQL = "/users/0/projects/" + CPLString(pszProjetctId) + "/sql";

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( osAPIKey.size() > 0 )
    {
        osSQL += "?token=";
        osSQL += osAPIKey;
    }

    osSQL += "&query=";

    std::string escaped = url_encode(pszUnescapedSQL);
    osSQL += escaped;

/* -------------------------------------------------------------------- */
/*      Collection the header options and execute request.              */
/* -------------------------------------------------------------------- */

    std::string pszAPIURL = GetAPIURL();
    char** papszOptions = NULL;

    pszAPIURL += osSQL;

    printf("RunSQL url %s\n", pszAPIURL.c_str());
    printf("RunSQL sql %s\n", pszUnescapedSQL);

    CPLHTTPResult * psResult = CPLHTTPFetch( pszAPIURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);

    printf("RunSQL result: %s\n", psResult->pabyData);
/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL) 
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult && psResult->nStatus != 0) 
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    
    if( strlen((const char*)psResult->pabyData) < 1000 )
        CPLDebug( "AMIGOCLOUD", "RunSQL Response:%s", psResult->pabyData );
    
    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

    jstok = json_tokener_new();
    poObj = json_tokener_parse_ex(jstok, (const char*) psResult->pabyData, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "JSON parsing error: %s (at offset %d)",
                    json_tokener_error_desc(jstok->err), jstok->char_offset);
        json_tokener_free(jstok);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    json_tokener_free(jstok);

    CPLHTTPDestroyResult(psResult);

    if( poObj != NULL )
    {
        if( json_object_get_type(poObj) == json_type_object )
        {
            json_object* poError = json_object_object_get(poObj, "error");
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
/*                        OGRAMIGOCLOUDGetSingleRow()                      */
/************************************************************************/

json_object* OGRAMIGOCLOUDGetSingleRow(json_object* poObj)
{
    if( poObj == NULL )
    {
        return NULL;
    }

    json_object* poRows = json_object_object_get(poObj, "data");
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

OGRLayer * OGRAMIGOCLOUDDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    return ExecuteSQLInternal(pszSQLCommand, poSpatialFilter, pszDialect,
                              TRUE);
}

OGRLayer * OGRAMIGOCLOUDDataSource::ExecuteSQLInternal( const char *pszSQLCommand,
                                                     OGRGeometry *poSpatialFilter,
                                                     const char *pszDialect,
                                                     int bRunDeferedActions )

{
    if( bRunDeferedActions )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoLayers[iLayer]->RunDeferedCreationIfNecessary();
            papoLayers[iLayer]->FlushDeferedInsert();
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
    
    if( !EQUALN(pszSQLCommand, "SELECT", strlen("SELECT")) &&
        !EQUALN(pszSQLCommand, "EXPLAIN", strlen("EXPLAIN")) &&
        !EQUALN(pszSQLCommand, "WITH", strlen("WITH")) )
    {
        RunSQL(pszSQLCommand);
        return NULL;
    }

    OGRAMIGOCLOUDResultLayer* poLayer = new OGRAMIGOCLOUDResultLayer( this, pszSQLCommand );

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

void OGRAMIGOCLOUDDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
