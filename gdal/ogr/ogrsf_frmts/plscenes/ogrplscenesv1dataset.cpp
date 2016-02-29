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
    json_object* poAssetCategories = json_object_object_get(poCatalog, "asset_categories");
    std::vector<CPLString> aoAssetCategories;
    if( poAssetCategories != NULL && json_object_get_type(poAssetCategories) == json_type_object )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poAssetCategories, it )
        {
            aoAssetCategories.push_back(it.key);
        }
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
                            this, pszId, pszSpecURL, pszItemsURL, nCount, aoAssetCategories);
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

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "scene") &&
                !EQUAL(pszKey, "product_type") &&
                !EQUAL(pszKey, "catalog") &&
                !EQUAL(pszKey, "version") )
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

    json_object* poObj = RunRequest( osRasterURL );
    if( poObj == NULL )
        return NULL;

    json_object* poSubObj = NULL;
    if( pszProductType != NULL &&
        (poSubObj = json_object_object_get(poObj, pszProductType)) != NULL )
    {
       /* do nothing */
    }
    else if( pszProductType != NULL && !EQUAL(pszProductType, "LIST") &&
        (poSubObj = json_object_object_get(poObj, pszProductType)) == NULL )
    {
       CPLError(CE_Failure, CPLE_AppDefined, "Cannot find asset %s", pszProductType);
       json_object_put(poObj);
       return NULL;
    }
    else if( pszProductType == NULL &&
             (poSubObj = json_object_object_get(poObj, "visual")) != NULL )
    {
        pszProductType = "visual";
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
    json_object* poFile = json_object_object_get(poSubObj, "file");
    if( poFile == NULL || json_object_get_type(poFile) != json_type_string )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link");
        json_object_put(poObj);
        return NULL;
    }
    const char* pszLink = json_object_get_string(poFile);

    osRasterURL = pszLink ? pszLink : "";
    json_object_put(poObj);
    if( osRasterURL.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link to scene %s",
                 osScene.c_str());
        return NULL;
    }

    if( STARTS_WITH(osRasterURL, "http://") )
    {
        osRasterURL = "http://" + m_osAPIKey + ":@" + osRasterURL.substr(strlen("http://"));
    }
    else if( STARTS_WITH(osRasterURL, "https://") )
    {
        osRasterURL = "https://" + m_osAPIKey + ":@" + osRasterURL.substr(strlen("https://"));
    }

    CPLString osOldHead(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD", ""));
    CPLString osOldExt(CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", ""));

    int bUseVSICURL = CSLFetchBoolean(poOpenInfo->papszOpenOptions, "RANDOM_ACCESS", TRUE);
    if( bUseVSICURL && !(STARTS_WITH(m_osBaseURL, "/vsimem/")) )
    {
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_USE_HEAD", "NO");
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", "{noext}");

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

    GDALDataset* poOutDS = (GDALDataset*) GDALOpenEx(osRasterURL, GDAL_OF_RASTER, NULL, NULL, NULL);
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
                    if( poFeat->IsFieldSet(i) )
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
        CSLDestroy(poOutDS->GetFileList()); /* so as to probe all auxiliary files before reseting the allowed extensions */
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
                                    osOldHead.size() ? osOldHead.c_str(): NULL);
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS",
                                    osOldExt.size() ? osOldExt.c_str(): NULL);
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
    if( poDS->m_osAPIKey.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing PL_API_KEY configuration option or API_KEY open option");
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

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

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "version") &&
                !EQUAL(pszKey, "catalog") )
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
