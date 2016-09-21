/******************************************************************************
 * $Id$
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAmigoCloudDataSource class
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

CPLString OGRAMIGOCLOUDGetOptionValue(const char* pszFilename, const char* pszOptionName);

/************************************************************************/
/*                        OGRAmigoCloudDataSource()                        */
/************************************************************************/

OGRAmigoCloudDataSource::OGRAmigoCloudDataSource() :
    pszName(NULL),
    pszProjetctId(NULL),
    papoLayers(NULL),
    nLayers(0),
    bReadWrite(FALSE),
    bUseHTTPS(FALSE),
    bMustCleanPersistent(FALSE),
    bHasOGRMetadataFunction(-1)
{}

/************************************************************************/
/*                       ~OGRAmigoCloudDataSource()                        */
/************************************************************************/

OGRAmigoCloudDataSource::~OGRAmigoCloudDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistent)
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("AMIGOCLOUD:%p", this));
        CPLHTTPDestroyResult( CPLHTTPFetch( GetAPIURL(), papszOptions) );
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
    CPLFree(pszProjetctId);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAmigoCloudDataSource::TestCapability( const char * pszCap )

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

OGRLayer *OGRAmigoCloudDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRAmigoCloudDataSource::GetLayerByName(const char * pszLayerName)
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

int OGRAmigoCloudDataSource::Open( const char * pszFilename,
                                char** papszOpenOptionsIn,
                                int bUpdateIn )

{

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );
    if( CSLFetchNameValue(papszOpenOptionsIn, "PROJECTID") )
        pszProjetctId = CPLStrdup(CSLFetchNameValue(papszOpenOptionsIn, "PROJECTID"));
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

    osAPIKey = CSLFetchNameValueDef(papszOpenOptionsIn, "API_KEY",
                                    CPLGetConfigOption("AMIGOCLOUD_API_KEY", ""));

    CPLString osDatasets = OGRAMIGOCLOUDGetOptionValue(pszFilename, "datasets");

    bUseHTTPS = CPLTestBool(CPLGetConfigOption("AMIGOCLOUD_HTTPS", "YES"));

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

    if (osDatasets.size() != 0)
    {
        char** papszTables = CSLTokenizeString2(osDatasets, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRAmigoCloudTableLayer**) CPLRealloc(
                papoLayers, (nLayers + 1) * sizeof(OGRAmigoCloudTableLayer*));
            papoLayers[nLayers ++] = new OGRAmigoCloudTableLayer(this, papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char* OGRAmigoCloudDataSource::GetAPIURL() const
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

int OGRAmigoCloudDataSource::FetchSRSId( OGRSpatialReference * poSRS )

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

OGRLayer   *OGRAmigoCloudDataSource::ICreateLayer( const char *pszNameIn,
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
    CPLString osName(pszNameIn);

    OGRAmigoCloudTableLayer* poLayer = new OGRAmigoCloudTableLayer(this, osName);
    int bGeomNullable = CSLFetchBoolean(papszOptions, "GEOMETRY_NULLABLE", TRUE);
    poLayer->SetDeferredCreation(eGType, poSpatialRef, bGeomNullable);
    papoLayers = (OGRAmigoCloudTableLayer**) CPLRealloc(
                    papoLayers, (nLayers + 1) * sizeof(OGRAmigoCloudTableLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRAmigoCloudDataSource::DeleteLayer(int iLayer)
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
    CPLString osDatasetId = papoLayers[iLayer]->GetDatasetId();

    CPLDebug( "AMIGOCLOUD", "DeleteLayer(%s)", osDatasetId.c_str() );

    int bDeferredCreation = papoLayers[iLayer]->GetDeferredCreation();
    papoLayers[iLayer]->CancelDeferredCreation();
    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    if (osDatasetId.size() == 0)
        return OGRERR_NONE;

    if( !bDeferredCreation )
    {
        std::stringstream url;
        url << std::string(GetAPIURL()) << "/users/0/projects/" + std::string(GetProjetcId()) + "/datasets/"+ osDatasetId.c_str();
        json_object *poObj = RunDELETE(url.str().c_str());

//        json_object* poObj = RunSQL(osSQL);
        if( poObj == NULL )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

char** OGRAmigoCloudDataSource::AddHTTPOptions()
{
    bMustCleanPersistent = TRUE;

    return CSLAddString(NULL, CPLSPrintf("PERSISTENT=AMIGOCLOUD:%p", this));
}

/************************************************************************/
/*                               RunPOST()                               */
/************************************************************************/

json_object* OGRAmigoCloudDataSource::RunPOST(const char*pszURL, const char *pszPostData, const char *pszHeaders)
{
    CPLString osURL(pszURL);

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
    CSLDestroy(papszOptions);
    if( psResult == NULL )
        return NULL;

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunPOST Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

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
/*                               RunDELETE()                               */
/************************************************************************/

json_object* OGRAmigoCloudDataSource::RunDELETE(const char*pszURL)
{
    CPLString osURL(pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( osAPIKey.size() > 0 )
    {
        osURL += "?token=";
        osURL += osAPIKey;
    }
    char** papszOptions=NULL;
    CPLString osPOSTFIELDS("CUSTOMREQUEST=DELETE");
    papszOptions = CSLAddString(papszOptions, osPOSTFIELDS);

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == NULL )
        return NULL;

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunDELETE HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunDELETE Error Message:%s", psResult->pszErrBuf );
    }
    else if ( psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunDELETE Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLDebug( "AMIGOCLOUD", "RunDELETE Response:%s", psResult->pabyData );

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

json_object* OGRAmigoCloudDataSource::RunGET(const char*pszURL)
{
    CPLString osURL(pszURL);

    /* -------------------------------------------------------------------- */
    /*      Provide the API Key                                             */
    /* -------------------------------------------------------------------- */
    if( osAPIKey.size() > 0 )
    {
        osURL += "?token=";
        osURL += osAPIKey;
    }

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL.c_str(), NULL);
    if( psResult == NULL )
        return NULL;

    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server:%s", psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if ( psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunGET Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

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

json_object* OGRAmigoCloudDataSource::RunSQL(const char* pszUnescapedSQL)
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

    char * pszEscaped = CPLEscapeString( pszUnescapedSQL, -1, CPLES_URL );
    std::string escaped = pszEscaped;
    CPLFree( pszEscaped );
    osSQL += escaped;

/* -------------------------------------------------------------------- */
/*      Collection the header options and execute request.              */
/* -------------------------------------------------------------------- */

    std::string pszAPIURL = GetAPIURL();
    char** papszOptions = NULL;

    pszAPIURL += osSQL;

    CPLHTTPResult * psResult = CPLHTTPFetch( pszAPIURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pszErrBuf != NULL)
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL Error Message:%s", psResult->pszErrBuf );
    }
    else if (psResult->nStatus != 0)
    {
        CPLDebug( "AMIGOCLOUD", "RunSQL Error Status:%d", psResult->nStatus );
    }

    if( psResult->pabyData == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

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

OGRLayer * OGRAmigoCloudDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
    return ExecuteSQLInternal(pszSQLCommand, poSpatialFilter, pszDialect,
                              TRUE);
}

OGRLayer * OGRAmigoCloudDataSource::ExecuteSQLInternal( const char *pszSQLCommand,
                                                     OGRGeometry *poSpatialFilter,
                                                     const char *,
                                                     int bRunDeferredActions )

{
    if( bRunDeferredActions )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoLayers[iLayer]->RunDeferredCreationIfNecessary();
            papoLayers[iLayer]->FlushDeferredInsert();
        }
    }

    /* Skip leading spaces */
    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;


    if( !EQUALN(pszSQLCommand, "SELECT", strlen("SELECT")) &&
        !EQUALN(pszSQLCommand, "EXPLAIN", strlen("EXPLAIN")) &&
        !EQUALN(pszSQLCommand, "WITH", strlen("WITH")) )
    {
        RunSQL(pszSQLCommand);
        return NULL;
    }

    OGRAmigoCloudResultLayer* poLayer = new OGRAmigoCloudResultLayer( this, pszSQLCommand );

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

void OGRAmigoCloudDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
