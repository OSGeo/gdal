/******************************************************************************
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
#include "ogrgeojsonreader.h"
#include <time.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPLScenesV1Dataset()                       */
/************************************************************************/

OGRPLScenesV1Dataset::OGRPLScenesV1Dataset() :
    m_bLayerListInitialized(false),
    m_bMustCleanPersistent(false),
    m_nLayers(0),
    m_papoLayers(NULL),
    m_bFollowLinks(false)
{}

/************************************************************************/
/*                         ~OGRPLScenesV1Dataset()                      */
/************************************************************************/

OGRPLScenesV1Dataset::~OGRPLScenesV1Dataset()
{
    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);

    if( m_bMustCleanPersistent )
    {
        char **papszOptions =
            CSLSetNameValue(
                NULL, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
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
    json_object* poId = CPL_json_object_object_get(poCatalog, "id");
    if( poId == NULL || json_object_get_type(poId) != json_type_string )
        return NULL;
    json_object* poLinks = CPL_json_object_object_get(poCatalog, "_links");
    if( poLinks == NULL || json_object_get_type(poLinks) != json_type_object )
        return NULL;
    json_object* poSpec = CPL_json_object_object_get(poLinks, "spec");
    if( poSpec == NULL || json_object_get_type(poSpec) != json_type_string )
        return NULL;
    json_object* poItems = CPL_json_object_object_get(poLinks, "items");
    if( poItems == NULL || json_object_get_type(poItems) != json_type_string )
        return NULL;
    json_object* poCount = CPL_json_object_object_get(poCatalog, "item_count");
    GIntBig nCount = -1;
    if( poCount != NULL && json_object_get_type(poCount) == json_type_int )
    {
        nCount = json_object_get_int64(poCount);
    }
    CPLString osDisplayDescription;
    json_object* poDisplayDescription = CPL_json_object_object_get(poCatalog, "display_description");
    if( poDisplayDescription != NULL && json_object_get_type(poDisplayDescription) == json_type_string )
        osDisplayDescription = json_object_get_string(poDisplayDescription);
    CPLString osDisplayName;
    json_object* poDisplayName = CPL_json_object_object_get(poCatalog, "display_name");
    if( poDisplayName != NULL && json_object_get_type(poDisplayName) == json_type_string )
        osDisplayName = json_object_get_string(poDisplayName);

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
    if( !osDisplayName.empty() )
        poPLLayer->SetMetadataItem("SHORT_DESCRIPTION", osDisplayName.c_str());
    if( !osDisplayDescription.empty() )
        poPLLayer->SetMetadataItem("DESCRIPTION", osDisplayDescription.c_str());
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
    json_object* poCatalogs = CPL_json_object_object_get(poObj, "catalogs");
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
    json_object* poLinks = CPL_json_object_object_get(poObj, "_links");
    if( poLinks && json_object_get_type(poLinks) == json_type_object )
    {
        json_object* poNext = CPL_json_object_object_get(poLinks, "_next");
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

    while( !osURL.empty() )
    {
        json_object* poObj = RunRequest(osURL);
        if( poObj == NULL )
            break;
        if( !ParseCatalogsPage( poObj, osURL ) )
        {
            json_object_put(poObj);
            break;
        }
        json_object_put(poObj);
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
    papszOptions =
        CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=PLSCENES:%p", this));
    papszOptions =
        CSLAddString(papszOptions,
                     CPLSPrintf("HEADERS=Authorization: api-key %s",
                                m_osAPIKey.c_str()));
    return papszOptions;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRPLScenesV1Dataset::RunRequest(const char* pszURL,
                                              int bQuiet404Error,
                                              const char* pszHTTPVerb,
                                              bool bExpectJSonReturn,
                                              const char* pszPostContent)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), NULL);
    // We need to set it each time as CURL would reuse the previous value
    // if reusing the same connection
    papszOptions = CSLSetNameValue(papszOptions, "CUSTOMREQUEST", pszHTTPVerb);
    if( pszPostContent != NULL )
    {
        CPLString osHeaders = "Content-Type: application/json";
        //osHeaders += "\r\n";
        //osHeaders += CPLSPrintf("Authorization: api-key %s", m_osAPIKey.c_str());
        papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders);
        papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS", pszPostContent);
    }
    papszOptions = CSLSetNameValue(papszOptions, "MAX_RETRY", "3");
    CPLHTTPResult *psResult = NULL;
    if( STARTS_WITH(m_osBaseURL, "/vsimem/") &&
        STARTS_WITH(pszURL, "/vsimem/") )
    {
        psResult = (CPLHTTPResult*) CPLCalloc(1, sizeof(CPLHTTPResult));
        vsi_l_offset nDataLengthLarge = 0;
        CPLString osURL(pszURL);
        if( osURL[osURL.size()-1 ] == '/' )
            osURL.resize(osURL.size()-1);
        if( pszPostContent != NULL )
        {
            osURL += "&POSTFIELDS=";
            osURL += pszPostContent;
        }
        CPLDebug("PLSCENES", "Fetching %s", osURL.c_str());
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
                CPLStrdup(CPLSPrintf("Error 404. Cannot find %s", osURL.c_str()));
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

    if( pszPostContent != NULL && m_bMustCleanPersistent )
    {
        papszOptions = CSLSetNameValue(NULL, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
        m_bMustCleanPersistent = false;
    }

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

    if( !bExpectJSonReturn && (psResult->pabyData == NULL || psResult->nDataLen == 0) )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if( psResult->pabyData == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
#ifdef DEBUG_VERBOSE
    CPLDebug("PLScenes", "%s", pszText);
#endif

    json_object* poObj = NULL;
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

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
/*                           InsertAPIKeyInURL()                        */
/************************************************************************/

CPLString OGRPLScenesV1Dataset::InsertAPIKeyInURL(CPLString osURL)
{
    if( STARTS_WITH(osURL, "http://") )
    {
        osURL = "http://" + m_osAPIKey + ":@" + osURL.substr(strlen("http://"));
    }
    else if( STARTS_WITH(osURL, "https://") )
    {
        osURL = "https://" + m_osAPIKey + ":@" + osURL.substr(strlen("https://"));
    }
    return osURL;
}

/************************************************************************/
/*                            OpenRasterScene()                         */
/************************************************************************/

GDALDataset* OGRPLScenesV1Dataset::OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                                 CPLString osScene,
                                                 char** papszOptions)
{
    if( !(poOpenInfo->nOpenFlags & GDAL_OF_RASTER) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The scene option must only be used with vector access");
        return NULL;
    }

    int nActivationTimeout = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                      "ACTIVATION_TIMEOUT", "3600"));

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "scene") &&
                !EQUAL(pszKey, "product_type") &&
                !EQUAL(pszKey, "catalog") &&
                !EQUAL(pszKey, "version") &&
                !EQUAL(pszKey, "follow_links"))
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option %s", pszKey);
                CPLFree(pszKey);
                return NULL;
            }
            CPLFree(pszKey);
        }
    }

    const char* pszCatalog = CSLFetchNameValueDef(papszOptions, "catalog",
                CSLFetchNameValue(poOpenInfo->papszOpenOptions, "CATALOG"));
    if( pszCatalog == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing catalog");
        return NULL;
    }

    const char* pszProductType = CSLFetchNameValueDef(papszOptions, "product_type",
                CSLFetchNameValue(poOpenInfo->papszOpenOptions, "PRODUCT_TYPE"));

    CPLString osRasterURL;
    osRasterURL = m_osBaseURL;
    osRasterURL += pszCatalog;
    osRasterURL += "/items/";
    osRasterURL += osScene;
    osRasterURL += "/assets/";

    time_t nStartTime = time(NULL);
retry:
    time_t nCurrentTime = time(NULL);
    if( nCurrentTime - nStartTime > nActivationTimeout )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Activation timeout reached");
        return NULL;
    }
    json_object* poObj = RunRequest( osRasterURL );
    if( poObj == NULL )
        return NULL;

    json_object* poSubObj = NULL;
    if( pszProductType != NULL &&
        (poSubObj = CPL_json_object_object_get(poObj, pszProductType)) != NULL )
    {
       /* do nothing */
    }
    else if( pszProductType != NULL && !EQUAL(pszProductType, "LIST") &&
        (poSubObj = CPL_json_object_object_get(poObj, pszProductType)) == NULL )
    {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot find asset %s", pszProductType);
       json_object_put(poObj);
       return NULL;
    }
    else if( pszProductType == NULL &&
             (poSubObj = CPL_json_object_object_get(poObj, "visual")) != NULL )
    {
        /* do nothing */
    }
    else
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        char** papszSubdatasets = NULL;
        int nSubDataset = 0;
        json_object_object_foreachC( poObj, it )
        {
            ++nSubDataset;
            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                    CPLSPrintf("SUBDATASET_%d_NAME", nSubDataset),
                    CPLSPrintf("Scene=%s of catalog %s, type %s",
                               osScene.c_str(), pszCatalog, it.key));
            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                    CPLSPrintf("SUBDATASET_%d_DESC", nSubDataset),
                    CPLSPrintf("PLScenes:version=v1,catalog=%s,scene=%s,product_type=%s",
                               pszCatalog, osScene.c_str(), it.key));
        }
        json_object_put(poObj);
        if( nSubDataset != 0 )
        {
            GDALDataset* poDS = new OGRPLScenesV1Dataset();
            poDS->SetMetadata(papszSubdatasets, "SUBDATASETS");
            CSLDestroy(papszSubdatasets);
            return poDS;
        }
        return NULL;
    }
    if( json_object_get_type(poSubObj) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link");
        json_object_put(poObj);
        return NULL;
    }

    json_object* poPermissions = CPL_json_object_object_get(poSubObj, "_permissions");
    if( poPermissions != NULL )
    {
        const char* pszPermissions = json_object_to_json_string_ext( poPermissions, 0 );
        if( pszPermissions && strstr(pszPermissions, "download") == NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "You don't have download permissions for this product");
        }
    }

    json_object* poHTTP = json_ex_get_object_by_path(poSubObj, "files.http");
    if( poHTTP == NULL || json_object_get_type(poHTTP) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link");
        json_object_put(poObj);
        return NULL;
    }
    json_object* poLocation = CPL_json_object_object_get(poHTTP, "location");
    json_object* poStatus = CPL_json_object_object_get(poHTTP, "status");
    bool bActive = false;
    if( poStatus != NULL && json_object_get_type(poStatus) == json_type_string )
    {
        const char* pszStatus = json_object_get_string(poStatus);
        if( EQUAL( pszStatus, "activating" ) )
        {
            CPLDebug("PLScenes", "The product is in activation. Retrying...");
            CPLSleep( nActivationTimeout == 1 ? 0.5 : 1.0);
            poLocation = NULL;
            json_object_put(poObj);
            goto retry;
        }
        bActive = EQUAL( pszStatus, "active" );
    }
    if( poLocation == NULL || json_object_get_type(poLocation) != json_type_string ||
        !bActive )
    {
        CPLDebug("PLScenes", "The product isn't activated yet. Activating it");
        json_object* poActivate = json_ex_get_object_by_path(poHTTP, "_links.activate");
        if( poActivate == NULL || json_object_get_type(poActivate) != json_type_string )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link to activate scene %s",
                      osScene.c_str());
            json_object_put(poObj);
            return NULL;
        }
        CPLString osActivate = json_object_get_string(poActivate);
        poLocation = NULL;
        json_object_put(poObj);
        poObj = RunRequest( osActivate, FALSE, "POST", false );
        if( poObj != NULL )
            json_object_put(poObj);
        poObj = NULL;
        CPLSleep(nActivationTimeout == 1 ? 0.5 : 1.0);
        goto retry;
    }

    const char* pszLink = json_object_get_string(poLocation);

    osRasterURL = pszLink ? pszLink : "";
    json_object_put(poObj);
    if( osRasterURL.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link to scene %s",
                 osScene.c_str());
        return NULL;
    }

    osRasterURL = InsertAPIKeyInURL(osRasterURL);

    CPLString osOldHead(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD", ""));
    CPLString osOldAllowedFilename(CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME", ""));

    const bool bUseVSICURL =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "RANDOM_ACCESS", true);
    if( bUseVSICURL && !(STARTS_WITH(m_osBaseURL, "/vsimem/")) )
    {
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_USE_HEAD", "NO");
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME",
                                      ("/vsicurl/" + osRasterURL).c_str());

        VSIStatBufL sStat;
        if( VSIStatL(("/vsicurl/" + osRasterURL).c_str(), &sStat) == 0 &&
            sStat.st_size > 0 )
        {
            osRasterURL = "/vsicurl/" + osRasterURL;
        }
        else
        {
            CPLDebug("PLSCENES", "Cannot use random access for that file");
        }
    }

    char** papszAllowedDrivers = NULL;
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "HTTP");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "GTiff");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "PNG");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "JPEG");
    GDALDataset* poOutDS = (GDALDataset*) GDALOpenEx(osRasterURL, GDAL_OF_RASTER,
                                                     papszAllowedDrivers, NULL, NULL);
    CSLDestroy(papszAllowedDrivers);
    if( poOutDS )
    {
        OGRLayer* poLayer = GetLayerByName(pszCatalog);
        if( poLayer != NULL )
        {
            // Set a dummy name so that PAM goes here
            CPLPushErrorHandler(CPLQuietErrorHandler);
            poOutDS->SetDescription("/vsimem/tmp/ogrplscenesv1");

            /* Attach scene metadata. */
            poLayer->SetAttributeFilter(CPLSPrintf("id = '%s'", osScene.c_str()));
            OGRFeature* poFeat = poLayer->GetNextFeature();
            if( poFeat )
            {
                for(int i=0;i<poFeat->GetFieldCount();i++)
                {
                    if( poFeat->IsFieldSetAndNotNull(i) )
                    {
                        const char* pszKey = poFeat->GetFieldDefnRef(i)->GetNameRef();
                        const char* pszVal = poFeat->GetFieldAsString(i);
                        if( strncmp(pszKey, "asset_", strlen("asset_")) == 0 ||
                            strstr(pszVal, "https://") != NULL )
                        {
                            continue;
                        }
                        poOutDS->SetMetadataItem(pszKey, pszVal);
                    }
                }
            }
            delete poFeat;

            poOutDS->FlushCache();
            VSIUnlink("/vsimem/tmp/ogrplscenesv1");
            VSIUnlink("/vsimem/tmp/ogrplscenesv1.aux.xml");
            CPLPopErrorHandler();
        }

        CPLErrorReset();
        poOutDS->SetDescription(poOpenInfo->pszFilename);
        CSLDestroy(poOutDS->GetFileList()); /* so as to probe all auxiliary files before resetting the allowed extensions */
    }
    else if( CPLGetLastErrorType() == CE_None )
    {
        poObj = RunRequest( osRasterURL );
        if( poObj == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "The generation of the product is in progress. Retry later");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s", json_object_to_json_string_ext( poObj, JSON_C_TO_STRING_PRETTY ));
            json_object_put(poObj);
        }
    }

    if( bUseVSICURL )
    {
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_USE_HEAD",
                                    !osOldHead.empty() ? osOldHead.c_str(): NULL);
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME",
                                    !osOldAllowedFilename.empty() ? osOldAllowedFilename.c_str(): NULL);
    }

    return poOutDS;
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
    if( poDS->m_osAPIKey.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing PL_API_KEY configuration option or API_KEY open option");
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

    poDS->m_bFollowLinks = CPLTestBool( CSLFetchNameValueDef(papszOptions, "follow_links",
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "FOLLOW_LINKS", "FALSE")) );

    poDS->m_osFilter = CSLFetchNameValueDef(papszOptions, "filter",
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "FILTER", ""));
    poDS->m_osFilter.Trim();

    const char* pszScene = CSLFetchNameValueDef(papszOptions, "scene",
                CSLFetchNameValue(poOpenInfo->papszOpenOptions, "SCENE"));
    if( pszScene )
    {
        GDALDataset* poRasterDS = poDS->OpenRasterScene(poOpenInfo, pszScene,
                                                        papszOptions);
        delete poDS;
        CSLDestroy(papszOptions);
        return poRasterDS;
    }
    else if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) &&
             !(poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing scene");
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "version") &&
                !EQUAL(pszKey, "catalog") &&
                !EQUAL(pszKey, "follow_links") &&
                !EQUAL(pszKey, "filter") )
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

    const char* pszCatalog = CSLFetchNameValueDef(papszOptions, "catalog",
                CSLFetchNameValue(poOpenInfo->papszOpenOptions, "CATALOG"));
    if( pszCatalog == NULL )
    {
        // Establish (partial if there are other pages) layer list.
        if( !poDS->ParseCatalogsPage( poObj, poDS->m_osNextCatalogPageURL) )
        {
            delete poDS;
            poDS = NULL;
        }
    }
    else
    {
        if( poDS->GetLayerByName( pszCatalog ) == NULL )
        {
            delete poDS;
            poDS = NULL;
        }
    }

    json_object_put(poObj);

    CSLDestroy(papszOptions);

    if( !(poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}
