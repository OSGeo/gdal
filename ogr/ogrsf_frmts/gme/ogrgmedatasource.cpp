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
    //if( bReadWrite && EQUAL(pszCap,ODsCCreateLayer) )
    //    return TRUE;
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

    osSelect = OGRGMEGetOptionValue(pszFilename, "select");
    osWhere = OGRGMEGetOptionValue(pszFilename, "where");

    bUseHTTPS = TRUE;

    osAccessToken = OGRGMEGetOptionValue(pszFilename, "access");
    if (osAccessToken.size() == 0)
        osAccessToken = CPLGetConfigOption("GME_ACCESS_TOKEN","");
    if (osAccessToken.size() == 0 && osRefreshToken.size() > 0) 
    {
        osAccessToken.Seize(GOA2GetAccessToken(osRefreshToken,
                                               GME_TABLE_SCOPE_RO)); // TODO
        if (osAccessToken.size() == 0)
            return FALSE;
    }

    if (osAccessToken.size() == 0 && osAuth.size() > 0)
    {
        osRefreshToken.Seize(GOA2GetRefreshToken(osAuth, GME_TABLE_SCOPE_RO)); // TODO
        if (osRefreshToken.size() == 0)
            return FALSE;
    }

    if (osAccessToken.size() == 0)
    {
        if (osTables.size() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Unauthenticated access requires explicit tables= parameter");
            return FALSE;
        }
    }

    if (osTables.size() != 0)
    {
        char** papszTables = CSLTokenizeString2(osTables, ",", 0);
        for(int i=0;papszTables && papszTables[i];i++)
        {
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
	    OGRGMELayer *poGMELayer = new OGRGMELayer(this, papszTables[i]);
	    if (poGMELayer->GetLayerDefn()) {
                papoLayers[nLayers ++] = poGMELayer;
            }
        }
        CSLDestroy(papszTables);
	if ( nLayers == 0 )
	    return FALSE;
        return TRUE;
    }

    return FALSE;
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
    int bUsePost = 
        CSLTestBoolean(
            CPLGetConfigOption("GME_USE_POST", "FALSE"));
/* -------------------------------------------------------------------- */
/*      Provide the API Key - used to rate limit access (see            */
/*      GME_APIKEY config)                                              */
/* -------------------------------------------------------------------- */
    CPLString osQueryFields;

    osQueryFields += "key=";
    osQueryFields += osAPIKey;

    if (!osSelect.empty()) {
        CPLDebug( "GME", "found select=%s", osSelect.c_str());
        osQueryFields += "&select=";
        osQueryFields += osSelect;
    }
    if (!osWhere.empty()) {
        CPLDebug( "GME", "found where=%s", osWhere.c_str());
        osQueryFields += "&where=";
        osQueryFields += osWhere;
    }

    if (pszMoreOptions)
        osQueryFields += pszMoreOptions;

/* -------------------------------------------------------------------- */
/*      Collect the header options.                                     */
/* -------------------------------------------------------------------- */
    CPLStringList oOptions;
    if (bUsePost) {
        CPLString osPostFields = "POSTFIELDS=";
        osPostFields += osQueryFields;
        oOptions.AddString(osPostFields);
    }
    AddHTTPOptions(oOptions);

/* -------------------------------------------------------------------- */
/*      Build URL                                                       */
/* -------------------------------------------------------------------- */
    CPLString osURL = GetAPIURL();
    osURL += "/";
    osURL += pszRequest;

    if (!bUsePost) {
        if (osURL.find("?") == std::string::npos) {
            osURL += "?";
        } else {
            osURL += "?";
        }
        osURL += osQueryFields;
    }

    CPLDebug( "GME", "Sleep for 0.1s to try and avoid qps limiting errors.");
    CPLSleep( 1.0 );

    CPLHTTPResult * psResult = CPLHTTPFetch(osURL, oOptions);

/* -------------------------------------------------------------------- */
/*      Check for some error conditions and report.  HTML Messages      */
/*      are transformed info failure.                                   */
/* -------------------------------------------------------------------- */
    if (psResult && psResult->pszContentType &&
        strncmp(psResult->pszContentType, "text/html", 9) == 0)
    {
        CPLDebug( "GME", "MakeRequest HTML Response:%s", psResult->pabyData );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "HTML error page returned by server");
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
    }
    if (psResult && psResult->pszErrBuf != NULL) 
    {
        CPLDebug( "GME", "MakeRequest Error Message: %s", psResult->pszErrBuf );
        CPLDebug( "GME", "error doc:\n%s\n", psResult->pabyData);
        json_object *error_response = Parse((const char *) psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        psResult = NULL;
        json_object *error_doc = json_object_object_get(error_response, "error");
        json_object *errors_doc = json_object_object_get(error_doc, "errors");
        array_list *errors_array = json_object_get_array(errors_doc);
        int nErrors = array_list_length(errors_array);
        for (int i = 0; i < nErrors; i++) {
            json_object *error_obj = (json_object *)array_list_get_idx(errors_array, i);
            const char* reason = GetJSONString(error_obj, "reason");
            const char* domain = GetJSONString(error_obj, "domain");
            const char* message = GetJSONString(error_obj, "message");
            const char* locationType = GetJSONString(error_obj, "locationType");
            const char* location = GetJSONString(error_obj, "location");
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
                CPLError( CE_Failure, CPLE_AppDefined, "GME: %s (%s) in %s/%s: %s",
                          reason, domain, locationType, location, message );
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
/*                           Parse()                                    */
/************************************************************************/

json_object *OGRGMEDataSource::Parse( const char* pszText )
{
    if( NULL != pszText )
    {
        json_tokener* jstok = NULL;
        json_object* jsobj = NULL;

        jstok = json_tokener_new();
        jsobj = json_tokener_parse_ex(jstok, pszText, -1);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "ESRIJSON parsing error: %s (at offset %d)",
                          json_tokener_errors[jstok->err], jstok->char_offset);
            
            json_tokener_free(jstok);
            return NULL;
        }
        json_tokener_free(jstok);

        /* JSON tree is shared for while lifetime of the reader object
         * and will be released in the destructor.
         */
        return jsobj;
    }

    return NULL;
}

/************************************************************************/
/*                           GetJSONString()                            */
/*                                                                      */
/*      Fetch a string field from a json_object (only an immediate      */
/*      child).                                                         */
/************************************************************************/

const char *OGRGMEDataSource::GetJSONString(json_object *parent,
                                            const char *field,
                                            const char *default_value)
{
    json_object *child = json_object_object_get(parent, field);
    if (child == NULL )
        return default_value;

    return json_object_get_string(child);
}
