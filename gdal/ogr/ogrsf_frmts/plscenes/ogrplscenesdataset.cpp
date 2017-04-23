/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesDataset
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Planet Labs
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPLScenesDataset()                         */
/************************************************************************/

OGRPLScenesDataset::OGRPLScenesDataset() :
    bMustCleanPersistent(false),
    nLayers(0),
    papoLayers(NULL)
{}

/************************************************************************/
/*                         ~OGRPLScenesDataset()                        */
/************************************************************************/

OGRPLScenesDataset::~OGRPLScenesDataset()
{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree(papoLayers);

    if( bMustCleanPersistent )
    {
        char **papszOptions =
            CSLSetNameValue(NULL, "CLOSE_PERSISTENT",
                            CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPLScenesDataset::GetLayer( int idx )
{
    if( idx < 0 || idx >= nLayers )
        return NULL;
    return papoLayers[idx];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRPLScenesDataset::GetLayerByName(const char* pszName)
{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszName);
    if( poLayer != NULL )
        return poLayer;

    CPLString osURL;
    osURL = osBaseURL;
    osURL += pszName;
    osURL += "/";
    json_object* poObj = RunRequest( (osURL + CPLString("?count=10")).c_str() );
    if( poObj == NULL )
        return NULL;

    OGRPLScenesLayer* poPLLayer = new OGRPLScenesLayer(this, pszName, osURL, poObj);
    papoLayers = (OGRPLScenesLayer**) CPLRealloc(papoLayers,
                                sizeof(OGRPLScenesLayer*) * (nLayers + 1));
    papoLayers[nLayers ++] = poPLLayer;

    json_object_put(poObj);

    return poPLLayer;
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer* OGRPLScenesDataset::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )
{
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
    {
        swq_select oSelect;
        CPLString osSQLCommand(pszSQLCommand);
        size_t nLimitPos = osSQLCommand.ifind(" limit ");
        if( nLimitPos != std::string::npos )
            osSQLCommand.resize(nLimitPos);

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLErr eErr = oSelect.preparse(osSQLCommand);
        CPLPopErrorHandler();
        if( eErr != CE_None )
            return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);

/* -------------------------------------------------------------------- */
/*      ORDER BY optimization on acquired field                         */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == NULL &&
            oSelect.table_count == 1 && oSelect.order_specs == 1 &&
            strcmp(oSelect.order_defs[0].field_name, "acquired") == 0 )
        {
            OGRPLScenesLayer* poLayer = NULL;
            for( int idx = 0; idx < nLayers; idx++ )
            {
                if( strcmp( papoLayers[idx]->GetName(),
                            oSelect.table_defs[0].table_name) == 0 )
                {
                    poLayer = papoLayers[idx];
                    break;
                }
            }
            if( poLayer != NULL )
            {
                poLayer->SetAcquiredOrderingFlag(
                                        oSelect.order_defs[0].ascending_flag);
                OGRLayer* poRet = GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
                if( poRet )
                    oMapResultSetToSourceLayer[poRet] = poLayer;
                return poRet;
            }
        }
    }
    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/***********************************************************************/
/*                           ReleaseResultSet()                        */
/***********************************************************************/

void OGRPLScenesDataset::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if( poResultsSet )
    {
        OGRPLScenesLayer* poSrcLayer = oMapResultSetToSourceLayer[poResultsSet];
        // Reset the acquired ordering to its default
        if( poSrcLayer )
        {
            poSrcLayer->SetAcquiredOrderingFlag(-1);
            oMapResultSetToSourceLayer.erase(poResultsSet);
        }
        delete poResultsSet;
    }
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                         */
/************************************************************************/

char** OGRPLScenesDataset::GetBaseHTTPOptions()
{
    bMustCleanPersistent = true;

    char** papszOptions = NULL;
    papszOptions =
        CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=PLSCENES:%p", this));
    papszOptions =
        CSLAddString(papszOptions,
                     CPLSPrintf("HEADERS=Authorization: api-key %s",
                                osAPIKey.c_str()));
    return papszOptions;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRPLScenesDataset::RunRequest(const char* pszURL,
                                            int bQuiet404Error)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), NULL);
    papszOptions = CSLSetNameValue(papszOptions, "MAX_RETRY", "3");
    CPLHTTPResult * psResult;
    if( STARTS_WITH(osBaseURL, "/vsimem/") &&
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

    json_object* poObj = NULL;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
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
/*                            OpenRasterScene()                         */
/************************************************************************/

GDALDataset* OGRPLScenesDataset::OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                                 CPLString osScene,
                                                 char** papszOptions)
{
    if( !(poOpenInfo->nOpenFlags & GDAL_OF_RASTER) )
    {
        return NULL;
    }

    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "scene") &&
                !EQUAL(pszKey, "product_type") &&
                !EQUAL(pszKey, "version") )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option %s", pszKey);
                CPLFree(pszKey);
                return NULL;
            }
            CPLFree(pszKey);
        }
    }

    const char* pszProductType = CSLFetchNameValueDef(papszOptions, "product_type",
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "PRODUCT_TYPE", "visual"));

    CPLString osRasterURL;
    osRasterURL = osBaseURL;
    osRasterURL += CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CATALOG", "ortho");
    osRasterURL += "/";
    osRasterURL += osScene;
    json_object* poObj = RunRequest( osRasterURL );
    if( poObj == NULL )
        return NULL;
    json_object* poProperties = CPL_json_object_object_get(poObj, "properties");
    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find properties object");
        json_object_put(poObj);
        return NULL;
    }

    const char* pszLink = NULL;
    if( EQUAL(pszProductType, "thumb") )
    {
        json_object* poLinks = CPL_json_object_object_get(poProperties, "links");
        if( poLinks != NULL && json_object_get_type(poLinks) == json_type_object )
        {
            json_object* poThumbnail = CPL_json_object_object_get(poLinks, "thumbnail");
            if( poThumbnail && json_object_get_type(poThumbnail) == json_type_string )
                pszLink = json_object_get_string(poThumbnail);
        }
    }
    else
    {
        json_object* poData = CPL_json_object_object_get(poProperties, "data");
        if( poData != NULL && json_object_get_type(poData) == json_type_object )
        {
            json_object* poProducts = CPL_json_object_object_get(poData, "products");
            if( poProducts != NULL && json_object_get_type(poProducts) == json_type_object )
            {
                json_object* poProduct = CPL_json_object_object_get(poProducts, pszProductType);
                if( poProduct != NULL && json_object_get_type(poProduct) == json_type_object )
                {
                    json_object* poFull = CPL_json_object_object_get(poProduct, "full");
                    if( poFull && json_object_get_type(poFull) == json_type_string )
                        pszLink = json_object_get_string(poFull);
                }
            }
        }
    }
    osRasterURL = pszLink ? pszLink : "";
    json_object_put(poObj);
    if( osRasterURL.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find link to scene %s",
                 osScene.c_str());
        return NULL;
    }

    if( STARTS_WITH(osRasterURL, "http://") )
    {
        osRasterURL = "http://" + osAPIKey + ":@" + osRasterURL.substr(strlen("http://"));
    }
    else if( STARTS_WITH(osRasterURL, "https://") )
    {
        osRasterURL = "https://" + osAPIKey + ":@" + osRasterURL.substr(strlen("https://"));
    }

    CPLString osOldHead(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD", ""));
    CPLString osOldAllowedFilename(CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME", ""));

    const bool bUseVSICURL =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "RANDOM_ACCESS", true);
    if( bUseVSICURL && !(STARTS_WITH(osBaseURL, "/vsimem/")) )
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
        if( !EQUAL(pszProductType, "thumb") )
        {
            OGRPLScenesLayer* poLayer = new OGRPLScenesLayer(this, "ortho",
                                            (osBaseURL + "ortho/").c_str());
            papoLayers = (OGRPLScenesLayer**) CPLRealloc(papoLayers,
                                        sizeof(OGRPLScenesLayer*) * (nLayers + 1));
            papoLayers[nLayers ++] = poLayer;

            // Set a dummy name so that PAM goes here
            CPLPushErrorHandler(CPLQuietErrorHandler);
            poOutDS->SetDescription("/vsimem/tmp/ogrplscenesv0");

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
                        if( strstr(pszKey, "file_size") == NULL &&
                            strstr(pszVal, "https://") == NULL )
                        {
                            poOutDS->SetMetadataItem(pszKey, pszVal);
                        }
                    }
                }
            }
            delete poFeat;

            poOutDS->FlushCache();
            VSIUnlink("/vsimem/tmp/ogrplscenesv0");
            VSIUnlink("/vsimem/tmp/ogrplscenesv0.aux.xml");
            CPLPopErrorHandler();
        }

        CPLErrorReset();
        poOutDS->SetDescription(poOpenInfo->pszFilename);
        CSLDestroy(poOutDS->GetFileList()); /* so as to probe all auxiliary files before resetting the allowed extensions */
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

GDALDataset* OGRPLScenesDataset::Open(GDALOpenInfo* poOpenInfo)
{
    OGRPLScenesDataset* poDS = new OGRPLScenesDataset();

    poDS->osBaseURL = CPLGetConfigOption("PL_URL", "https://api.planet.com/v0/scenes/");

    char** papszOptions = CSLTokenizeStringComplex(
        poOpenInfo->pszFilename+strlen("PLScenes:"), ",", TRUE, FALSE );

    poDS->osAPIKey = CSLFetchNameValueDef(papszOptions, "api_key",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "API_KEY",
                                CPLGetConfigOption("PL_API_KEY","")) );
    if( poDS->osAPIKey.empty() )
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
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "spat") &&
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

    json_object* poObj = poDS->RunRequest(poDS->osBaseURL);
    if( poObj == NULL )
    {
        delete poDS;
        CSLDestroy(papszOptions);
        return NULL;
    }

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poObj, it )
    {
        if( it.val != NULL && json_object_get_type(it.val) == json_type_string )
        {
            const char* pszSceneType = it.key;
            const char* pszSceneTypeURL = json_object_get_string(it.val);
            json_object* poObj2 = NULL;
            if( !EQUAL(pszSceneType, "ortho") )
                poObj2 = poDS->RunRequest( (CPLString(pszSceneTypeURL) + CPLString("?count=10")).c_str() );

            OGRPLScenesLayer* poLayer = new OGRPLScenesLayer(poDS, pszSceneType, pszSceneTypeURL, poObj2);

            if( poObj2 )
                json_object_put(poObj2);

            poDS->papoLayers = (OGRPLScenesLayer**) CPLRealloc(poDS->papoLayers,
                                        sizeof(OGRPLScenesLayer*) * (poDS->nLayers + 1));
            poDS->papoLayers[poDS->nLayers ++] = poLayer;

            const char* pszSpat = CSLFetchNameValue(papszOptions, "spat");
            if( pszSpat )
            {
                char** papszTokens = CSLTokenizeString2(pszSpat, " ", 0);
                if( CSLCount(papszTokens) >= 4 )
                {
                    poLayer->SetMainFilterRect(CPLAtof(papszTokens[0]),
                                               CPLAtof(papszTokens[1]),
                                               CPLAtof(papszTokens[2]),
                                               CPLAtof(papszTokens[3]));
                }
                CSLDestroy(papszTokens);
            }
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
