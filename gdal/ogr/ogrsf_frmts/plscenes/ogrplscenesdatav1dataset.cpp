/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesDataV1Dataset
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRPLScenesDataV1Dataset()                     */
/************************************************************************/

OGRPLScenesDataV1Dataset::OGRPLScenesDataV1Dataset() :
    m_bLayerListInitialized(false),
    m_bMustCleanPersistent(false),
    m_nLayers(0),
    m_papoLayers(nullptr),
    m_bFollowLinks(false)
{}

/************************************************************************/
/*                       ~OGRPLScenesDataV1Dataset()                    */
/************************************************************************/

OGRPLScenesDataV1Dataset::~OGRPLScenesDataV1Dataset()
{
    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);

    if( m_bMustCleanPersistent )
    {
        char **papszOptions =
            CSLSetNameValue(
                nullptr, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPLScenesDataV1Dataset::GetLayer(int idx)
{
    if( idx < 0 || idx >= GetLayerCount() )
        return nullptr;
    return m_papoLayers[idx];
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRPLScenesDataV1Dataset::GetLayerCount()
{
    if( !m_bLayerListInitialized )
    {
        m_bLayerListInitialized = true;
        EstablishLayerList();
    }
    return m_nLayers;
}

/************************************************************************/
/*                          ParseItemType()                             */
/************************************************************************/

OGRLayer* OGRPLScenesDataV1Dataset::ParseItemType(json_object* poItemType)
{
    if( poItemType == nullptr || json_object_get_type(poItemType) != json_type_object )
        return nullptr;
    json_object* poId = CPL_json_object_object_get(poItemType, "id");
    if( poId == nullptr || json_object_get_type(poId) != json_type_string )
        return nullptr;

    CPLString osDisplayDescription;
    json_object* poDisplayDescription = CPL_json_object_object_get(poItemType, "display_description");
    if( poDisplayDescription != nullptr && json_object_get_type(poDisplayDescription) == json_type_string )
        osDisplayDescription = json_object_get_string(poDisplayDescription);
    CPLString osDisplayName;
    json_object* poDisplayName = CPL_json_object_object_get(poItemType, "display_name");
    if( poDisplayName != nullptr && json_object_get_type(poDisplayName) == json_type_string )
        osDisplayName = json_object_get_string(poDisplayName);

    const char* pszId = json_object_get_string(poId);

    // The layer might already exist if GetLayerByName() is called before
    // GetLayer()/GetLayerCount() is

    // Prevent GetLayerCount() from calling EstablishLayerList()
    bool bLayerListInitializedBackup = m_bLayerListInitialized;
    m_bLayerListInitialized = true;
    OGRLayer* poExistingLayer = GDALDataset::GetLayerByName(pszId);
    m_bLayerListInitialized = bLayerListInitializedBackup;
    if( poExistingLayer != nullptr )
        return poExistingLayer;

    OGRPLScenesDataV1Layer* poPLLayer = new OGRPLScenesDataV1Layer(
                                                                this, pszId);
    if( !osDisplayName.empty() )
        poPLLayer->SetMetadataItem("SHORT_DESCRIPTION", osDisplayName.c_str());
    if( !osDisplayDescription.empty() )
        poPLLayer->SetMetadataItem("DESCRIPTION", osDisplayDescription.c_str());
    m_papoLayers = (OGRPLScenesDataV1Layer**) CPLRealloc(m_papoLayers,
                                sizeof(OGRPLScenesDataV1Layer*) * (m_nLayers + 1));
    m_papoLayers[m_nLayers ++] = poPLLayer;
    return poPLLayer;
}

/************************************************************************/
/*                          ParseItemTypes()                         */
/************************************************************************/

bool OGRPLScenesDataV1Dataset::ParseItemTypes(json_object* poObj,
                                             CPLString& osNext)
{
    json_object* poItemTypes = CPL_json_object_object_get(poObj, "item_types");
    if( poItemTypes == nullptr || json_object_get_type(poItemTypes) != json_type_array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing item_types object, or not of type array");
        return false;
    }
    const auto nCatalogsLength = json_object_array_length(poItemTypes);
    for( auto i=decltype(nCatalogsLength){0}; i<nCatalogsLength; i++ )
    {
        json_object* poItemType = json_object_array_get_idx(poItemTypes, i);
        ParseItemType(poItemType);
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

void OGRPLScenesDataV1Dataset::EstablishLayerList()
{
    CPLString osURL(m_osNextItemTypesPageURL);
    m_osNextItemTypesPageURL = "";

    while( !osURL.empty() )
    {
        json_object* poObj = RunRequest(osURL);
        if( poObj == nullptr )
            break;
        if( !ParseItemTypes( poObj, osURL ) )
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

OGRLayer *OGRPLScenesDataV1Dataset::GetLayerByName(const char* pszName)
{
    // Prevent GetLayerCount() from calling EstablishLayerList()
    bool bLayerListInitializedBackup = m_bLayerListInitialized;
    m_bLayerListInitialized = true;
    OGRLayer* poRet = GDALDataset::GetLayerByName(pszName);
    m_bLayerListInitialized = bLayerListInitializedBackup;
    if( poRet != nullptr )
        return poRet;

    CPLString osURL(m_osBaseURL + "item-types/" + pszName);
    json_object* poObj = RunRequest(osURL);
    if( poObj == nullptr )
        return nullptr;
    poRet = ParseItemType(poObj);
    json_object_put(poObj);
    return poRet;
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                         */
/************************************************************************/

char** OGRPLScenesDataV1Dataset::GetBaseHTTPOptions()
{
    m_bMustCleanPersistent = true;

    char** papszOptions = nullptr;
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

json_object* OGRPLScenesDataV1Dataset::RunRequest(const char* pszURL,
                                              int bQuiet404Error,
                                              const char* pszHTTPVerb,
                                              bool bExpectJSonReturn,
                                              const char* pszPostContent)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), nullptr);
    // We need to set it each time as CURL would reuse the previous value
    // if reusing the same connection
    papszOptions = CSLSetNameValue(papszOptions, "CUSTOMREQUEST", pszHTTPVerb);
    if( pszPostContent != nullptr )
    {
        CPLString osHeaders = CSLFetchNameValueDef(papszOptions, "HEADERS", "");
        if( !osHeaders.empty() )
            osHeaders += "\r\n";
        osHeaders += "Content-Type: application/json";
        papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders);
        papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS", pszPostContent);
    }
    papszOptions = CSLSetNameValue(papszOptions, "MAX_RETRY", "3");
    CPLHTTPResult *psResult = nullptr;
    if( STARTS_WITH(m_osBaseURL, "/vsimem/") &&
        STARTS_WITH(pszURL, "/vsimem/") )
    {
        psResult = (CPLHTTPResult*) CPLCalloc(1, sizeof(CPLHTTPResult));
        vsi_l_offset nDataLengthLarge = 0;
        CPLString osURL(pszURL);
        if( osURL[osURL.size()-1 ] == '/' )
            osURL.resize(osURL.size()-1);
        if( pszPostContent != nullptr )
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

    if( pszPostContent != nullptr && m_bMustCleanPersistent )
    {
        papszOptions = CSLSetNameValue(nullptr, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
        m_bMustCleanPersistent = false;
    }

    if( psResult->pszErrBuf != nullptr )
    {
        if( !(bQuiet404Error && strstr(psResult->pszErrBuf, "404")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                    psResult->pabyData ? (const char*) psResult->pabyData :
                    psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( !bExpectJSonReturn && (psResult->pabyData == nullptr || psResult->nDataLen == 0) )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
#ifdef DEBUG_VERBOSE
    CPLDebug("PLScenes", "%s", pszText);
#endif

    json_object* poObj = nullptr;
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLHTTPDestroyResult(psResult);

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Return is not a JSON dictionary");
        json_object_put(poObj);
        poObj = nullptr;
    }

    return poObj;
}

/************************************************************************/
/*                           InsertAPIKeyInURL()                        */
/************************************************************************/

CPLString OGRPLScenesDataV1Dataset::InsertAPIKeyInURL(CPLString osURL)
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

GDALDataset* OGRPLScenesDataV1Dataset::OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                                 CPLString osScene,
                                                 char** papszOptions)
{
    if( !(poOpenInfo->nOpenFlags & GDAL_OF_RASTER) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The scene option must only be used with vector access");
        return nullptr;
    }

    int nActivationTimeout = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                      "ACTIVATION_TIMEOUT", "3600"));

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != nullptr )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "scene") &&
                !EQUAL(pszKey, "product_type") &&
                !EQUAL(pszKey, "asset") &&
                !EQUAL(pszKey, "catalog") &&
                !EQUAL(pszKey, "itemtypes") &&
                !EQUAL(pszKey, "version") &&
                !EQUAL(pszKey, "follow_links") &&
                !EQUAL(pszKey, "metadata"))
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option %s", pszKey);
                CPLFree(pszKey);
                return nullptr;
            }
            CPLFree(pszKey);
        }
    }

    const char* pszCatalog =
        CSLFetchNameValueDef(papszOptions, "itemtypes",
        CSLFetchNameValueDef(papszOptions, "catalog",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ITEMTYPES",
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "CATALOG"))));
    if( pszCatalog == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing catalog");
        return nullptr;
    }

    const char* pszProductType =
        CSLFetchNameValueDef(papszOptions, "asset",
        CSLFetchNameValueDef(papszOptions, "product_type",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ASSET",
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "PRODUCT_TYPE"))));

    CPLString osRasterURL;
    osRasterURL = m_osBaseURL;
    osRasterURL += "item-types/";
    osRasterURL += pszCatalog;
    osRasterURL += "/items/";
    osRasterURL += osScene;
    osRasterURL += "/assets/";

    time_t nStartTime = time(nullptr);
retry:
    time_t nCurrentTime = time(nullptr);
    if( nCurrentTime - nStartTime > nActivationTimeout )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Activation timeout reached");
        return nullptr;
    }
    json_object* poObj = RunRequest( osRasterURL );
    if( poObj == nullptr )
        return nullptr;

    json_object* poSubObj = CPL_json_object_object_get(poObj,
                               pszProductType ? pszProductType : "visual");
    if( poSubObj == nullptr )
    {
        if( pszProductType != nullptr && !EQUAL(pszProductType, "LIST") )
        {
           CPLError(CE_Failure, CPLE_AppDefined, "Cannot find asset %s", pszProductType);
           json_object_put(poObj);
        }
        else
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            char** papszSubdatasets = nullptr;
            int nSubDataset = 0;
            json_object_object_foreachC( poObj, it )
            {
                ++nSubDataset;
                papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                        CPLSPrintf("SUBDATASET_%d_NAME", nSubDataset),
                        CPLSPrintf("Scene=%s of item types %s, asset %s",
                                   osScene.c_str(), pszCatalog, it.key));
                papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                        CPLSPrintf("SUBDATASET_%d_DESC", nSubDataset),
                        CPLSPrintf("PLScenes:version=Data_V1,itemtypes=%s,scene=%s,asset=%s",
                                   pszCatalog, osScene.c_str(), it.key));
            }
            json_object_put(poObj);
            if( nSubDataset != 0 )
            {
                GDALDataset* poDS = new OGRPLScenesDataV1Dataset();
                poDS->SetMetadata(papszSubdatasets, "SUBDATASETS");
                CSLDestroy(papszSubdatasets);
                return poDS;
            }
        }
        return nullptr;
    }

    if( json_object_get_type(poSubObj) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link");
        json_object_put(poObj);
        return nullptr;
    }

    json_object* poPermissions = CPL_json_object_object_get(poSubObj, "_permissions");
    if( poPermissions != nullptr )
    {
        const char* pszPermissions = json_object_to_json_string_ext( poPermissions, 0 );
        if( pszPermissions && strstr(pszPermissions, "download") == nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "You don't have download permissions for this product");
        }
    }

    json_object* poLocation = CPL_json_object_object_get(poSubObj, "location");
    json_object* poStatus = CPL_json_object_object_get(poSubObj, "status");
    bool bActive = false;
    if( poStatus != nullptr && json_object_get_type(poStatus) == json_type_string )
    {
        const char* pszStatus = json_object_get_string(poStatus);
        if( EQUAL( pszStatus, "activating" ) )
        {
            CPLDebug("PLScenes", "The product is in activation. Retrying...");
            CPLSleep( nActivationTimeout == 1 ? 0.5 : 1.0);
            poLocation = nullptr;
            json_object_put(poObj);
            goto retry;
        }
        bActive = EQUAL( pszStatus, "active" );
    }
    if( poLocation == nullptr || json_object_get_type(poLocation) != json_type_string ||
        !bActive )
    {
        CPLDebug("PLScenes", "The product isn't activated yet. Activating it");
        json_object* poActivate = json_ex_get_object_by_path(poSubObj, "_links.activate");
        if( poActivate == nullptr || json_object_get_type(poActivate) != json_type_string )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link to activate scene %s",
                      osScene.c_str());
            json_object_put(poObj);
            return nullptr;
        }
        CPLString osActivate = json_object_get_string(poActivate);
        poLocation = nullptr;
        json_object_put(poObj);
        poObj = RunRequest( osActivate, FALSE, "GET", false );
        if( poObj != nullptr )
            json_object_put(poObj);
        poObj = nullptr;
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
        return nullptr;
    }

    osRasterURL = InsertAPIKeyInURL(osRasterURL);

    const bool bUseVSICURL =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "RANDOM_ACCESS", true);
    if( bUseVSICURL && !(STARTS_WITH(m_osBaseURL, "/vsimem/")) )
    {
        char* pszEscapedURL = CPLEscapeString(osRasterURL, -1, CPLES_URL);
        CPLString osTmpURL("/vsicurl?use_head=no&max_retry=3&empty_dir=yes&url=");
        osTmpURL += pszEscapedURL;
        CPLFree(pszEscapedURL);
        CPLDebug("PLSCENES", "URL = %s", osTmpURL.c_str());

        VSIStatBufL sStat;
        if( VSIStatL(osTmpURL, &sStat) == 0 &&
            sStat.st_size > 0 )
        {
            osRasterURL = osTmpURL;
        }
        else
        {
            CPLDebug("PLSCENES", "Cannot use random access for that file");
        }
    }

    char** papszAllowedDrivers = nullptr;
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "HTTP");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "GTiff");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "PNG");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "JPEG");
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "NITF");
    GDALDataset* poOutDS = (GDALDataset*) GDALOpenEx(osRasterURL, GDAL_OF_RASTER,
                                                     papszAllowedDrivers, nullptr, nullptr);
    CSLDestroy(papszAllowedDrivers);
    if( poOutDS )
    {
        if( CPLFetchBool(papszOptions, "metadata",
                CPLFetchBool(poOpenInfo->papszOpenOptions, "METADATA", true)) )
        {
            OGRLayer* poLayer = GetLayerByName(pszCatalog);
            if( poLayer != nullptr )
            {
                // Set a dummy name so that PAM goes here
                CPLPushErrorHandler(CPLQuietErrorHandler);
                poOutDS->SetDescription("/vsimem/tmp/ogrplscenesDataV1");

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
                                strstr(pszVal, "https://") != nullptr ||
                                strcmp(pszKey, "columns") == 0 ||
                                strcmp(pszKey, "rows") == 0 ||
                                strcmp(pszKey, "epsg_code") == 0 ||
                                strcmp(pszKey, "origin_x") == 0 ||
                                strcmp(pszKey, "origin_y") == 0 ||
                                strcmp(pszKey, "permissions") == 0 ||
                                strcmp(pszKey, "acquired") == 0 // Redundant with TIFFTAG_DATETIME
                            )
                            {
                                continue;
                            }
                            poOutDS->SetMetadataItem(pszKey, pszVal);
                        }
                    }
                }
                delete poFeat;

                poOutDS->FlushCache();
                VSIUnlink("/vsimem/tmp/ogrplscenesDataV1");
                VSIUnlink("/vsimem/tmp/ogrplscenesDataV1.aux.xml");
                CPLPopErrorHandler();
            }
        }

        CPLErrorReset();
        poOutDS->SetDescription(poOpenInfo->pszFilename);
    }
    else if( CPLGetLastErrorType() == CE_None )
    {
        poObj = RunRequest( osRasterURL );
        if( poObj == nullptr )
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

    return poOutDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* OGRPLScenesDataV1Dataset::Open(GDALOpenInfo* poOpenInfo)
{
    OGRPLScenesDataV1Dataset* poDS = new OGRPLScenesDataV1Dataset();

    poDS->m_osBaseURL = CPLGetConfigOption("PL_URL", "https://api.planet.com/data/v1/");

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
        return nullptr;
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
        return nullptr;
    }

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != nullptr )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "version") &&
                !EQUAL(pszKey, "catalog") &&
                !EQUAL(pszKey, "itemtypes") &&
                !EQUAL(pszKey, "follow_links") &&
                !EQUAL(pszKey, "filter") )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option '%s'", pszKey);
                CPLFree(pszKey);
                delete poDS;
                CSLDestroy(papszOptions);
                return nullptr;
            }
            CPLFree(pszKey);
        }
    }

    json_object* poObj = poDS->RunRequest((poDS->m_osBaseURL + "item-types/").c_str());
    if( poObj == nullptr )
    {
        delete poDS;
        CSLDestroy(papszOptions);
        return nullptr;
    }

    const char* pszCatalog =
        CSLFetchNameValueDef(papszOptions, "itemtypes",
        CSLFetchNameValueDef(papszOptions, "catalog",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ITEMTYPES",
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "CATALOG"))));
    if( pszCatalog == nullptr )
    {
        // Establish (partial if there are other pages) layer list.
        if( !poDS->ParseItemTypes( poObj, poDS->m_osNextItemTypesPageURL) )
        {
            delete poDS;
            poDS = nullptr;
        }
    }
    else
    {
        if( poDS->GetLayerByName( pszCatalog ) == nullptr )
        {
            delete poDS;
            poDS = nullptr;
        }
    }

    json_object_put(poObj);

    CSLDestroy(papszOptions);

    if( !(poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}
