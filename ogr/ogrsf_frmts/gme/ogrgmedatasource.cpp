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
            papoLayers[nLayers ++] = new OGRGMELayer(this, papszTables[i]);
        }
        CSLDestroy(papszTables);
        return TRUE;
    }

#ifdef notdef
    /* Get list of tables */
    CPLHTTPResult * psResult = RunSQL("SHOW TABLES");

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        psResult->pszErrBuf != NULL ||
        strncmp(pszLine, "table id,name", strlen("table id,name")) != 0)
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    pszLine = OGRGMEGotoNextLine(pszLine);
    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGMEGotoNextLine(pszLine);
        if (pszNextLine)
            pszNextLine[-1] = 0;

        char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
        if (CSLCount(papszTokens) == 2)
        {
            CPLString osTableId(papszTokens[0]);
            CPLString osLayerName(papszTokens[1]);
            for(int i=0;i<nLayers;i++)
            {
                if (strcmp(papoLayers[i]->GetName(), osLayerName) == 0)
                {
                    osLayerName += " (";
                    osLayerName += osTableId;
                    osLayerName += ")";
                    break;
                }
            }
            papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers ++] = new OGRGMETableLayer(this, osLayerName, osTableId);
        }
        CSLDestroy(papszTokens);

        pszLine = pszNextLine;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
#endif
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
/*      GFT_APIKEY config)                                              */
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

    CPLDebug( "GME", "Sleep for 1.1s to try and avoid qps limiting errors.");
    CPLSleep( 1.1 );

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
        CPLDebug( "GME", "MakeRequest Error Message:%s", psResult->pszErrBuf );
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
