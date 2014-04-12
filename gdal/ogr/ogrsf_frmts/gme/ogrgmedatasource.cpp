/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMEDataSource Implementation.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gme.h"
#include "ogrgmejson.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

#define GDAL_API_KEY "AIzaSyA_2h1_wXMOLHNSVeo-jf1ACME-M1XMgP0"
#define GME_TABLE_SCOPE_RO "https://www.googleapis.com/auth/mapsengine.readonly"
#define GME_TABLE_SCOPE "https://www.googleapis.com/auth/mapsengine"

/************************************************************************/
/*                          OGRGMEDataSource()                          */
/************************************************************************/

OGRGMEDataSource::OGRGMEDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bReadWrite = FALSE;
    bUseHTTPS = FALSE;

    bMustCleanPersistant = FALSE;
    nRetries = 0;
}

/************************************************************************/
/*                         ~OGRGMEDataSource()                          */
/************************************************************************/

OGRGMEDataSource::~OGRGMEDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (bMustCleanPersistant)
    {
        char** papszOptions = CSLAddString(NULL, CPLSPrintf("CLOSE_PERSISTENT=GME:%p", this));
        CPLHTTPFetch( GetAPIURL(), papszOptions);
        CSLDestroy(papszOptions);
    }

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMEDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    //else if( bReadWrite && EQUAL(pszCap,ODsCDeleteLayer) )
    //    return TRUE;
    //else
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGMEDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                      OGRGMEGetOptionValue()                          */
/************************************************************************/

CPLString OGRGMEGetOptionValue(const char* pszFilename,
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

int OGRGMEDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (!EQUALN(pszFilename, "GME:", 4))
        return FALSE;

    bReadWrite = bUpdateIn;

    pszName = CPLStrdup( pszFilename );

    osAuth = OGRGMEGetOptionValue(pszFilename, "auth");
    if (osAuth.size() == 0)
        osAuth = CPLGetConfigOption("GME_AUTH", "");

    osRefreshToken = OGRGMEGetOptionValue(pszFilename, "refresh");
    if (osRefreshToken.size() == 0)
        osRefreshToken = CPLGetConfigOption("GME_REFRESH_TOKEN", "");

    osAPIKey = CPLGetConfigOption("GME_APIKEY", GDAL_API_KEY);

    CPLString osTables = OGRGMEGetOptionValue(pszFilename, "tables");

    osProjectId = OGRGMEGetOptionValue(pszFilename, "project");

    osSelect = OGRGMEGetOptionValue(pszFilename, "select");
    osWhere = OGRGMEGetOptionValue(pszFilename, "where");

    CPLString osBatchPatchSize;
    osBatchPatchSize = OGRGMEGetOptionValue(pszFilename, "batchpatchsize");
    if (osBatchPatchSize.size() == 0) {
        osBatchPatchSize = CPLGetConfigOption("GME_BATCH_PATCH_SIZE","50");
    }
    int iBatchPatchSize = atoi( osBatchPatchSize.c_str() );

    bUseHTTPS = TRUE;

    osAccessToken = OGRGMEGetOptionValue(pszFilename, "access");
    if (osAccessToken.size() == 0)
        osAccessToken = CPLGetConfigOption("GME_ACCESS_TOKEN","");
    if (osAccessToken.size() == 0 && osRefreshToken.size() > 0)
    {
        osAccessToken.Seize(GOA2GetAccessToken(osRefreshToken,
                                               GME_TABLE_SCOPE)); // TODO
        if (osAccessToken.size() == 0) {
            CPLDebug( "GME", "Cannot get access token");
            return FALSE;
        }
    }

    if (osAccessToken.size() == 0 && osAuth.size() > 0)
    {
        osRefreshToken.Seize(GOA2GetRefreshToken(osAuth, GME_TABLE_SCOPE)); // TODO
        if (osRefreshToken.size() == 0)
            CPLDebug( "GME", "Cannot get refresh token");
            return FALSE;
    }

    if ((osAccessToken.size() ==0) && (osTables.size() == 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unauthenticated access requires explicit tables= parameter");
        return FALSE;
    }

    osTraceToken = OGRGMEGetOptionValue(pszFilename, "trace");
    if (osTraceToken.size() == 0) {
        CPLDebug("GME", "Looking for GME_TRACE_TOKEN");
        osTraceToken = CPLGetConfigOption("GME_TRACE_TOKEN", "");
    }
    if (osTraceToken.size() != 0) {
      CPLDebug("GME", "Found trace token %s", osTraceToken.c_str());
    }

    if (osTables.size() != 0)
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
	    OGRGMELayer *poGMELayer = new OGRGMELayer(this, papszTables[i]);
            poGMELayer->SetBatchPatchSize(iBatchPatchSize);
	    if (poGMELayer->GetLayerDefn()) {
                papoLayers[nLayers ++] = poGMELayer;
            }
        }
        CSLDestroy(papszTables);
	if ( nLayers == 0 ) {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not find any tables.");
	    return FALSE;
        }
        CPLDebug("GME", "Found %d layers", nLayers);
        return TRUE;
    }
    else if (osProjectId.size() != 0) {
        CPLDebug("GME", "We have a projectId: %s. Use CreateLayer to create tables.",
                 osProjectId.c_str());
        return TRUE;
    }
    CPLDebug("GME", "No table no project, giving up!");
    return FALSE;
}

/************************************************************************/
/*                           CreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRGMEDataSource::CreateLayer( const char *pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!bReadWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

    if (osAccessToken.size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return NULL;
    }

    if ((CSLFetchNameValue( papszOptions, "projectId" ) == NULL) && (osProjectId.size() != 0)) {
        papszOptions = CSLAddNameValue( papszOptions, "projectId", osProjectId.c_str() );
    }

    osTraceToken = OGRGMEGetOptionValue(pszName, "trace");
    if (osTraceToken.size() == 0) {
      osTraceToken = CPLGetConfigOption("GME_TRACE_TOKEN", "");
    }
    if (osTraceToken.size() != 0) {
      CPLDebug("GME", "Found trace token %s", osTraceToken.c_str());
    }

    OGRGMELayer* poLayer = new OGRGMELayer(this, pszName, papszOptions);
    poLayer->SetGeometryType(eGType);
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            GetAPIURL()                               */
/************************************************************************/

const char*  OGRGMEDataSource::GetAPIURL() const
{
    const char* pszAPIURL = CPLGetConfigOption("GME_API_URL", NULL);
    if (pszAPIURL)
        return pszAPIURL;
    else if (bUseHTTPS)
        return "https://www.googleapis.com/mapsengine/v1";
    else
        return "http://www.googleapis.com/mapsengine/v1";
}

/************************************************************************/
/*                          AddHTTPOptions()                            */
/************************************************************************/

void OGRGMEDataSource::AddHTTPOptions(CPLStringList &oOptions)
{
    bMustCleanPersistant = TRUE;

    if (strlen(osAccessToken) > 0)
        oOptions.AddString(
            CPLSPrintf("HEADERS=Authorization: Bearer %s",
                       osAccessToken.c_str()));

    oOptions.AddString(CPLSPrintf("PERSISTENT=GME:%p", this));
}

/************************************************************************/
/*                            MakeRequest()                             */
/************************************************************************/

CPLHTTPResult * OGRGMEDataSource::MakeRequest(const char *pszRequest,
                                              const char *pszMoreOptions)
{
/* -------------------------------------------------------------------- */
/*      Provide the API Key - used to rate limit access (see            */
/*      GME_APIKEY config)                                              */
/* -------------------------------------------------------------------- */
    CPLString osQueryFields;

    osQueryFields += "key=";
    osQueryFields += osAPIKey;

    if (pszMoreOptions)
        osQueryFields += pszMoreOptions;

/* -------------------------------------------------------------------- */
/*      Collect the header options.                                     */
/* -------------------------------------------------------------------- */
    CPLStringList oOptions;
    AddHTTPOptions(oOptions);

/* -------------------------------------------------------------------- */
/*      Build URL                                                       */
/* -------------------------------------------------------------------- */
    CPLString osURL = GetAPIURL();
    osURL += "/";
    osURL += pszRequest;

    if (osURL.find("?") == std::string::npos) {
        osURL += "?";
    } else {
        osURL += "?";
    }
    osURL += osQueryFields;

    // Trace the request if we have a tracing token
    if (osTraceToken.size() != 0) {
      CPLDebug("GME", "Using trace token %s", osTraceToken.c_str());
      osURL += "&trace=";
      osURL += osTraceToken;
    }

    CPLDebug( "GME", "Sleep for 1s to try and avoid qps limiting errors.");
    CPLSleep( 1.0 );

    CPLHTTPResult * psResult = CPLHTTPFetch(osURL, oOptions);

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "GME", "MakeRequest HTML Response: %s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server");
        if (nRetries < 2) {
            CPLDebug("GME", "Sleeping 5s and retrying");
            nRetries ++;
            CPLSleep( 5.0 );
            psResult = MakeRequest(pszRequest, pszMoreOptions);
            if (psResult)
                CPLDebug( "GME", "Got a result after %d retries", nRetries );
            else
                CPLDebug( "GME", "Didn't get a result after %d retries", nRetries );
            nRetries--;
        } else {
            CPLDebug("GME", "I've waited too long on GME. Giving up!");
            CPLHTTPDestroyResult(psResult);
            psResult = NULL;
        }
        return psResult;
    }
    if (psResult && psResult->pszErrBuf != NULL)
    {
        CPLDebug( "GME", "MakeRequest Error Message: %s", psResult->pszErrBuf );
        CPLDebug( "GME", "error doc:\n%s\n", psResult->pabyData);
        json_object *error_response = OGRGMEParseJSON((const char *) psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
        json_object *error_doc = json_object_object_get(error_response, "error");
        json_object *errors_doc = json_object_object_get(error_doc, "errors");
        array_list *errors_array = json_object_get_array(errors_doc);
        int nErrors = array_list_length(errors_array);
        for (int i = 0; i < nErrors; i++) {
            json_object *error_obj = (json_object *)array_list_get_idx(errors_array, i);
            const char* reason = OGRGMEGetJSONString(error_obj, "reason");
            const char* domain = OGRGMEGetJSONString(error_obj, "domain");
            const char* message = OGRGMEGetJSONString(error_obj, "message");
            const char* locationType = OGRGMEGetJSONString(error_obj, "locationType");
            const char* location = OGRGMEGetJSONString(error_obj, "location");
            if ((nRetries < 10) && EQUAL(reason, "rateLimitExceeded")) {
                // Sleep nRetries * 1.0s and retry
                nRetries ++;
                CPLDebug( "GME", "Got a %s (%d) times.", reason, nRetries );
                CPLDebug( "GME", "Sleep for %2.2f to try and avoid qps limiting errors.", 1.0 * nRetries );
                CPLSleep( 1.0 * nRetries );
                psResult = MakeRequest(pszRequest, pszMoreOptions);
                if (psResult)
                    CPLDebug( "GME", "Got a result after %d retries", nRetries );
                else
                    CPLDebug( "GME", "Didn't get a result after %d retries", nRetries );
                nRetries = 0;
            }
	    else if (EQUAL(reason, "authError")) {
                CPLDebug( "GME", "Failed to GET %s: %s", pszRequest, message );
                CPLError( CE_Failure, CPLE_OpenFailed, "GME: %s", message);
	    }
	    else if (EQUAL(reason, "backendError")) {
                CPLDebug( "GME", "Backend error retrying: GET %s: %s", pszRequest, message );
                psResult = MakeRequest(pszRequest, pszMoreOptions);
	    }
            else {
                int code = 444;
                json_object *code_child = json_object_object_get(error_doc, "code");
                if (code_child != NULL )
                    code = json_object_get_int(code_child);

                CPLDebug( "GME", "MakeRequest Error for %s: %s:%d", pszRequest, reason, code);
                CPLError( CE_Failure, CPLE_AppDefined, "GME: %s %s %s: %s - %s",
                          domain, reason, locationType, location, message );
            }
        }
        return psResult;
    }
    else if (psResult && psResult->nStatus != 0)
    {
        CPLDebug( "GME", "MakeRequest Error Status:%d", psResult->nStatus );
    }
    return psResult;
}

/************************************************************************/
/*                        AddHTTPPostOptions()                          */
/************************************************************************/

void OGRGMEDataSource::AddHTTPPostOptions(CPLStringList &oOptions)
{
    bMustCleanPersistant = TRUE;

    if (strlen(osAccessToken) > 0)
        oOptions.AddString(
            CPLSPrintf("HEADERS=Content-type: application/json\n"
                       "Authorization: Bearer %s",
                       osAccessToken.c_str()));

    oOptions.AddString(CPLSPrintf("PERSISTENT=GME:%p", this));
}

/************************************************************************/
/*                            PostRequest()                             */
/************************************************************************/

CPLHTTPResult * OGRGMEDataSource::PostRequest(const char *pszRequest,
                                              const char *pszBody)
{
/* -------------------------------------------------------------------- */
/*      Provide the API Key - used to rate limit access (see            */
/*      GME_APIKEY config)                                              */
/* -------------------------------------------------------------------- */
    CPLString osQueryFields;

    osQueryFields += "key=";
    osQueryFields += osAPIKey;

/* -------------------------------------------------------------------- */
/*      Collect the header options.                                     */
/* -------------------------------------------------------------------- */
    CPLStringList oOptions;
    oOptions.AddString("CUSTOMREQUEST=POST");
    CPLString osPostFields = "POSTFIELDS=";
    osPostFields += pszBody;
    oOptions.AddString(osPostFields);

    AddHTTPPostOptions(oOptions);

/* -------------------------------------------------------------------- */
/*      Build URL                                                       */
/* -------------------------------------------------------------------- */
    CPLString osURL = GetAPIURL();
    osURL += "/";
    osURL += pszRequest;

    if (osURL.find("?") == std::string::npos) {
        osURL += "?";
    } else {
        osURL += "?";
    }
    osURL += osQueryFields;

    // Trace the request if we have a tracing token
    if (osTraceToken.size() != 0) {
      CPLDebug("GME", "Using trace token %s", osTraceToken.c_str());
      osURL += "&trace=";
      osURL += osTraceToken;
    }

    CPLDebug( "GME", "Sleep for 1s to try and avoid qps limiting errors.");
    CPLSleep( 1.0 );

    CPLDebug( "GME", "Posting to %s.", osURL.c_str());
    CPLHTTPResult * psResult = CPLHTTPFetch(osURL, oOptions);

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "GME", "PostRequest HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL)
    {
        CPLDebug( "GME", "PostRequest Error Message: %s", psResult->pszErrBuf );
        CPLDebug( "GME", "error doc:\n%s\n", psResult->pabyData);
        json_object *error_response = OGRGMEParseJSON((const char *) psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
        json_object *error_doc = json_object_object_get(error_response, "error");
        json_object *errors_doc = json_object_object_get(error_doc, "errors");
        array_list *errors_array = json_object_get_array(errors_doc);
        int nErrors = array_list_length(errors_array);
        for (int i = 0; i < nErrors; i++) {
            json_object *error_obj = (json_object *)array_list_get_idx(errors_array, i);
            const char* reason = OGRGMEGetJSONString(error_obj, "reason");
            const char* domain = OGRGMEGetJSONString(error_obj, "domain");
            const char* message = OGRGMEGetJSONString(error_obj, "message");
            const char* locationType = OGRGMEGetJSONString(error_obj, "locationType");
            const char* location = OGRGMEGetJSONString(error_obj, "location");
            if ((nRetries < 10) && EQUAL(reason, "rateLimitExceeded")) {
                // Sleep nRetries * 1.0s and retry
                nRetries ++;
                CPLDebug( "GME", "Got a %s (%d) times.", reason, nRetries );
                CPLDebug( "GME", "Sleep for %2.2f to try and avoid qps limiting errors.", 1.0 * nRetries );
                CPLSleep( 1.0 * nRetries );
                psResult = PostRequest(pszRequest, pszBody);
                if (psResult)
                    CPLDebug( "GME", "Got a result after %d retries", nRetries );
                else
                    CPLDebug( "GME", "Didn't get a result after %d retries", nRetries );
                nRetries = 0;
            }
	    else if (EQUAL(reason, "authError")) {
                CPLDebug( "GME", "Failed to GET %s: %s", pszRequest, message );
                CPLError( CE_Failure, CPLE_OpenFailed, "GME: %s", message);
	    }
	    else if (EQUAL(reason, "backendError")) {
                CPLDebug( "GME", "Backend error retrying: GET %s: %s", pszRequest, message );
                psResult = PostRequest(pszRequest, pszBody);
	    }
            else {
                int code = 444;
                json_object *code_child = json_object_object_get(error_doc, "code");
                if (code_child != NULL )
                    code = json_object_get_int(code_child);

                CPLError( CE_Failure, CPLE_AppDefined, "GME: %s %s %s: %s - %s",
                          domain, reason, locationType, location, message );
                if ((code == 400) && (EQUAL(reason, "invalid")) && (EQUAL(location, "id"))) {
                  CPLDebug("GME", "Got the notorious 400 - invalid id, retrying in 5s");
                  CPLSleep( 5.0 );
                  psResult = PostRequest(pszRequest, pszBody);
                }
                else {
                    CPLDebug( "GME", "PostRequest Error for %s: %s:%d", pszRequest, reason, code);
                }
            }
        }
        return psResult;
    }
    else if (psResult && psResult->nStatus != 0)
    {
        CPLDebug( "GME", "PostRequest Error Status:%d", psResult->nStatus );
    }
    return psResult;
}
