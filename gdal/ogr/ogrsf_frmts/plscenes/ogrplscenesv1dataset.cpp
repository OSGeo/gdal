/******************************************************************************
 * $Id$
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesV1Dataset
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Planet Labs
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

#include "ogr_plscenes.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPLScenesV1Dataset()                       */
/************************************************************************/

OGRPLScenesV1Dataset::OGRPLScenesV1Dataset()
{
    m_bLayerListInitialized = false;
    m_bMustCleanPersistent = false;
    m_nLayers = 0;
    m_papoLayers = NULL;
}

/************************************************************************/
/*                         ~OGRPLScenesV1Dataset()                      */
/************************************************************************/

OGRPLScenesV1Dataset::~OGRPLScenesV1Dataset()
{
    for(int i=0;i<m_nLayers;i++)
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);

    if (m_bMustCleanPersistent)
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPLScenesV1Dataset::GetLayer(int idx)
{
    if( idx < 0 || idx >= GetLayerCount() )
        return NULL;
    return m_papoLayers[idx];
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRPLScenesV1Dataset::GetLayerCount()
{
    if( !m_bLayerListInitialized )
    {
        m_bLayerListInitialized = true;
        EstablishLayerList();
    }
    return m_nLayers;
}

/************************************************************************/
/*                           ParseCatalog()                             */
/************************************************************************/

OGRLayer* OGRPLScenesV1Dataset::ParseCatalog(json_object* poCatalog)
{
    if( poCatalog == NULL || json_object_get_type(poCatalog) != json_type_object )
        return NULL;
    json_object* poId = json_object_object_get(poCatalog, "id");
    if( poId == NULL || json_object_get_type(poId) != json_type_string )
        return NULL;
    json_object* poLinks = json_object_object_get(poCatalog, "_links");
    if( poLinks == NULL || json_object_get_type(poLinks) != json_type_object )
        return NULL;
    json_object* poSpec = json_object_object_get(poLinks, "spec");
    if( poSpec == NULL || json_object_get_type(poSpec) != json_type_string )
        return NULL;
    json_object* poItems = json_object_object_get(poLinks, "items");
    if( poItems == NULL || json_object_get_type(poItems) != json_type_string )
        return NULL;
    json_object* poCount = json_object_object_get(poCatalog, "count");
    GIntBig nCount = -1;
    if( poCount != NULL && json_object_get_type(poCount) == json_type_int )
    {
        nCount = json_object_get_int64(poCount);
    }
    const char* pszId = json_object_get_string(poId);
    const char* pszSpecURL = json_object_get_string(poSpec);
    const char* pszItemsURL = json_object_get_string(poItems);

    // The layer might already exist if GetLayerByName() is called before
    // GetLayer()/GetLayerCount() is

    // Prevent GetLayerCount() from calling EstablishLayerList()
    bool bLayerListInitializedBackup = m_bLayerListInitialized;
    m_bLayerListInitialized = true;
    OGRLayer* poExistingLayer = GDALDataset::GetLayerByName(pszId);
    m_bLayerListInitialized = bLayerListInitializedBackup;
    if( poExistingLayer != NULL )
        return poExistingLayer;

    OGRPLScenesV1Layer* poPLLayer = new OGRPLScenesV1Layer(
                            this, pszId, pszSpecURL, pszItemsURL, nCount);
    m_papoLayers = (OGRPLScenesV1Layer**) CPLRealloc(m_papoLayers,
                                sizeof(OGRPLScenesV1Layer*) * (m_nLayers + 1));
    m_papoLayers[m_nLayers ++] = poPLLayer;
    return poPLLayer;
}


/************************************************************************/
/*                          ParseCatalogsPage()                         */
/************************************************************************/

bool OGRPLScenesV1Dataset::ParseCatalogsPage(json_object* poObj,
                                             CPLString& osNext)
{
    json_object* poCatalogs = json_object_object_get(poObj, "catalogs");
    if( poCatalogs == NULL || json_object_get_type(poCatalogs) != json_type_array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing catalogs object, or not of type array");
        return false;
    }
    const int nCatalogsLength = json_object_array_length(poCatalogs);
    for( int i=0; i<nCatalogsLength; i++ ) 
    {
        json_object* poCatalog = json_object_array_get_idx(poCatalogs, i);
        ParseCatalog(poCatalog);
    }

    // Is there a next page ?
    osNext = "";
    json_object* poLinks = json_object_object_get(poObj, "_links");
    if( poLinks && json_object_get_type(poLinks) == json_type_object )
    {
        json_object* poNext = json_object_object_get(poLinks, "_next");
        if( poNext && json_object_get_type(poNext) == json_type_string )
        {
            osNext = json_object_get_string(poNext);
        }
    }

    return true;
}

/************************************************************************/
/*                          EstablishLayerList()                        */
/************************************************************************/

void OGRPLScenesV1Dataset::EstablishLayerList()
{
    CPLString osURL(m_osNextCatalogPageURL);
    m_osNextCatalogPageURL = "";

    while( osURL.size() != 0 )
    {
        json_object* poObj = RunRequest(osURL);
        if( poObj == NULL )
            break;
        if( !ParseCatalogsPage( poObj, osURL ) )
        {
            json_object_put(poObj);
            break;
        }
    }
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRPLScenesV1Dataset::GetLayerByName(const char* pszName)
{
    // Prevent GetLayerCount() from calling EstablishLayerList()
    bool bLayerListInitializedBackup = m_bLayerListInitialized;
    m_bLayerListInitialized = true;
    OGRLayer* poRet = GDALDataset::GetLayerByName(pszName);
    m_bLayerListInitialized = bLayerListInitializedBackup;
    if( poRet != NULL )
        return poRet;

    CPLString osURL(m_osBaseURL + pszName);
    json_object* poObj = RunRequest(osURL);
    if( poObj == NULL )
        return NULL;
    poRet = ParseCatalog(poObj);
    json_object_put(poObj);
    return poRet;
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                         */
/************************************************************************/

char** OGRPLScenesV1Dataset::GetBaseHTTPOptions()
{
    m_bMustCleanPersistent = true;

    char** papszOptions = NULL;
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=PLSCENES:%p", this));
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: api-key %s", m_osAPIKey.c_str()));
    return papszOptions;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRPLScenesV1Dataset::RunRequest(const char* pszURL,
                                            int bQuiet404Error)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), NULL);
    CPLHTTPResult * psResult;
    if( STARTS_WITH(m_osBaseURL, "/vsimem/") &&
        STARTS_WITH(pszURL, "/vsimem/") )
    {
        CPLDebug("PLSCENES", "Fetching %s", pszURL);
        psResult = (CPLHTTPResult*) CPLCalloc(1, sizeof(CPLHTTPResult));
        vsi_l_offset nDataLengthLarge = 0;
        CPLString osURL(pszURL);
        if( osURL[osURL.size()-1 ] == '/' )
            osURL.resize(osURL.size()-1);
        GByte* pabyBuf = VSIGetMemFileBuffer(osURL, &nDataLengthLarge, FALSE); 
        size_t nDataLength = static_cast<size_t>(nDataLengthLarge);
        if( pabyBuf )
        {
            psResult->pabyData = (GByte*) VSI_MALLOC_VERBOSE(1 + nDataLength);
            if( psResult->pabyData )
            {
                memcpy(psResult->pabyData, pabyBuf, nDataLength);
                psResult->pabyData[nDataLength] = 0;
            }
        }
        else
        {
            psResult->pszErrBuf =
                CPLStrdup(CPLSPrintf("Error 404. Cannot find %s", pszURL));
        }
    }
    else
    {
        if( bQuiet404Error )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        psResult = CPLHTTPFetch( pszURL, papszOptions);
        if( bQuiet404Error )
            CPLPopErrorHandler();
    }
    CSLDestroy(papszOptions);

    if( psResult->pszErrBuf != NULL )
    {
        if( !(bQuiet404Error && strstr(psResult->pszErrBuf, "404")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                    psResult->pabyData ? (const char*) psResult->pabyData :
                    psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( psResult->pabyData == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    json_tokener* jstok = NULL;
    json_object* poObj = NULL;

#ifdef DEBUG_VERBOSE
    CPLDebug("PLScenes", "%s", (const char*) psResult->pabyData);
#endif

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

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Return is not a JSON dictionary");
        json_object_put(poObj);
        poObj = NULL;
    }

    return poObj;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* OGRPLScenesV1Dataset::Open(GDALOpenInfo* poOpenInfo)
{
    OGRPLScenesV1Dataset* poDS = new OGRPLScenesV1Dataset();

    poDS->m_osBaseURL = CPLGetConfigOption("PL_URL", "https://api.planet.com/v1/catalogs/");

    char** papszOptions = CSLTokenizeStringComplex(
            poOpenInfo->pszFilename+strlen("PLScenes:"), ",", TRUE, FALSE );

    poDS->m_osAPIKey = CSLFetchNameValueDef(papszOptions, "api_key",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "API_KEY",
                                CPLGetConfigOption("PL_API_KEY","")) );
    if( poDS->m_osAPIKey.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing PL_API_KEY configuration option or API_KEY open option");
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "version") )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option '%s'", pszKey);
                CPLFree(pszKey);
                delete poDS;
                CSLDestroy(papszOptions);
                return NULL;
            }
            CPLFree(pszKey);
        }
    }

    json_object* poObj = poDS->RunRequest(poDS->m_osBaseURL);
    if( poObj == NULL )
    {
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

    // Establish (partial if there are other pages) layer list.
    if( !poDS->ParseCatalogsPage( poObj, poDS->m_osNextCatalogPageURL) )
    {
        delete poDS;
        poDS = NULL;
    }

    json_object_put(poObj);

    CSLDestroy(papszOptions);

    return poDS;
}
