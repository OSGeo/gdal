/******************************************************************************
 * $Id$
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

// g++ -g -Wall -fPIC -shared -o ogr_PLSCENES.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/plscene ogr/ogrsf_frmts/plscenes/*.c* -L. -lgdal -Iogr/ogrsf_frmts/geojson -Iogr/ogrsf_frmts/geojson/libjson 

CPL_CVSID("$Id$");

extern "C" void RegisterOGRPLSCENES();

/************************************************************************/
/*                         OGRPLScenesDataset()                         */
/************************************************************************/

OGRPLScenesDataset::OGRPLScenesDataset()
{
    bMustCleanPersistant = FALSE;
    nLayers = 0;
    papoLayers = NULL;
}

/************************************************************************/
/*                         ~OGRPLScenesDataset()                        */
/************************************************************************/

OGRPLScenesDataset::~OGRPLScenesDataset()
{
    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    if (bMustCleanPersistant)
    {
        char** papszOptions = NULL;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT", CPLSPrintf("PLSCENES:%p", this));
        CPLHTTPFetch( osBaseURL, papszOptions);
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPLScenesDataset::GetLayer(int idx)
{
    if( idx < 0 || idx >= nLayers )
        return NULL;
    return papoLayers[idx];
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer* OGRPLScenesDataset::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )
{
    if( EQUALN(pszSQLCommand, "SELECT ", strlen("SELECT ")) )
    {
        swq_select oSelect;
        CPLString osSQLCommand(pszSQLCommand);
        size_t nLimitPos = osSQLCommand.ifind(" limit ");
        if( nLimitPos != std::string::npos )
            osSQLCommand.resize(nLimitPos);

        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRErr eErr = oSelect.preparse(osSQLCommand);
        CPLPopErrorHandler();
        if( eErr != OGRERR_NONE )
            return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);

/* -------------------------------------------------------------------- */
/*      ORDER BY optimization on acquired field                         */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == NULL &&
            oSelect.table_count == 1 && oSelect.order_specs == 1 &&
            strcmp(oSelect.order_defs[0].field_name, "acquired") == 0 )
        {
            int idx;
            OGRPLScenesLayer* poLayer = NULL;
            for(idx = 0; idx < nLayers; idx ++ )
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
/*                             Identify()                               */
/************************************************************************/

int OGRPLScenesDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    return EQUALN(poOpenInfo->pszFilename, "PLSCENES:", strlen("PLSCENES:"));
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                         */
/************************************************************************/

char** OGRPLScenesDataset::GetBaseHTTPOptions()
{
    bMustCleanPersistant = TRUE;

    char** papszOptions = NULL;
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=PLSCENES:%p", this));
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: api-key %s", osAPIKey.c_str()));
    return papszOptions;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* OGRPLScenesDataset::RunRequest(const char* pszURL,
                                            int bQuiet404Error)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), NULL);
    CPLHTTPResult * psResult;
    if( strncmp(osBaseURL, "/vsimem/", strlen("/vsimem/")) == 0 &&
        strncmp(pszURL, "/vsimem/", strlen("/vsimem/")) == 0 )
    {
        CPLDebug("PLSCENES", "Fetching %s", pszURL);
        psResult = (CPLHTTPResult*) CPLCalloc(1, sizeof(CPLHTTPResult));
        vsi_l_offset nDataLength = 0;
        CPLString osURL(pszURL);
        if( osURL[osURL.size()-1 ] == '/' )
            osURL.resize(osURL.size()-1);
        GByte* pabyBuf = VSIGetMemFileBuffer(osURL, &nDataLength, FALSE); 
        if( pabyBuf )
        {
            psResult->pabyData = (GByte*) VSIMalloc(1 + nDataLength);
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
        char* pszKey;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != NULL )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "scene") &&
                !EQUAL(pszKey, "product_type") )
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
    if( strncmp(osBaseURL, "/vsimem/", strlen("/vsimem/")) == 0 )
        osRasterURL = osBaseURL;
    else
        osRasterURL.Printf("https://%s:@api.planet.com/v0/scenes/",
                           osAPIKey.c_str());
    osRasterURL += "ortho/";
    osRasterURL += osScene;
    osRasterURL += "/";
    if( EQUAL(pszProductType, "thumb") )
        osRasterURL += "thumb";
    else
        osRasterURL += CPLSPrintf("full?product=%s", pszProductType);

    CPLString osOldHead(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD", ""));
    CPLString osOldExt(CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", ""));

    int bUseVSICURL = CSLFetchBoolean(poOpenInfo->papszOpenOptions, "RANDOM_ACCESS", TRUE);
    if( bUseVSICURL && !(strncmp(osBaseURL, "/vsimem/", strlen("/vsimem/")) == 0) )
    {
        osRasterURL = "/vsicurl/" + osRasterURL;
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_USE_HEAD", "NO");
        CPLSetThreadLocalConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", "{noext}");
    }

    GDALDataset* poOutDS = (GDALDataset*) GDALOpen(osRasterURL, GA_ReadOnly);
    if( poOutDS )
    {
        poOutDS->SetDescription(poOpenInfo->pszFilename);
        poOutDS->GetFileList(); /* so as to probe all auxiliary files before reseting the allowed extensions */

        if( !EQUAL(pszProductType, "thumb") )
        {
            OGRPLScenesLayer* poLayer = new OGRPLScenesLayer(this, "ortho",
                                            (osBaseURL + "ortho/").c_str());
            papoLayers = (OGRPLScenesLayer**) CPLRealloc(papoLayers,
                                        sizeof(OGRPLScenesLayer*) * (nLayers + 1));
            papoLayers[nLayers ++] = poLayer;

            /* Attach scene matadata */
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
                        if( strstr(pszKey, "file_size") == NULL &&
                            strstr(pszVal, "https://") == NULL )
                        {
                            poOutDS->SetMetadataItem(pszKey, pszVal);
                        }
                    }
                }
            }
            delete poFeat;
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

GDALDataset* OGRPLScenesDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return NULL;

    OGRPLScenesDataset* poDS = new OGRPLScenesDataset();

    poDS->osBaseURL = CPLGetConfigOption("PL_URL", "https://api.planet.com/v0/scenes/");

    char** papszOptions = CSLTokenizeStringComplex(
            poOpenInfo->pszFilename+strlen("PLScenes:"), ",", TRUE, FALSE );

    poDS->osAPIKey = CSLFetchNameValueDef(papszOptions, "api_key",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "API_KEY",
                                CPLGetConfigOption("PL_API_KEY","")) );
    if( poDS->osAPIKey.size() == 0 )
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
                !EQUAL(pszKey, "spat") )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option %s", pszKey);
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
            OGRPLScenesLayer* poLayer = new OGRPLScenesLayer(poDS, it.key,
                                                     json_object_get_string(it.val));
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

/************************************************************************/
/*                        RegisterOGRPLSCENES()                         */
/************************************************************************/

void RegisterOGRPLSCENES()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PLSCENES" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PLSCENES" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Planet Labs Scenes API" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "drv_plscenes.html" );
        
        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='API_KEY' type='string' description='Account API key'/>"
"  <Option name='SCENE' type='string' description='Scene id (for raster fetching)'/>"
"  <Option name='PRODUCT_TYPE' type='string' description='Product type: visual, analytic or thumb (for raster fetching)' default='visual'/>"
"  <Option name='RANDOM_ACCESS' type='boolean' description='Whether raster should be accessed in random access mode (but with potentially not optimal throughput). If no, in-memory ingestion is done' default='YES'/>"
"</OpenOptionList>");

        poDriver->pfnOpen = OGRPLScenesDataset::Open;
        poDriver->pfnIdentify = OGRPLScenesDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

