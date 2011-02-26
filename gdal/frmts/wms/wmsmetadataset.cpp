/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Definition of GDALWMSMetaDataset class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "wmsmetadataset.h"

/************************************************************************/
/*                          GDALWMSMetaDataset()                        */
/************************************************************************/

GDALWMSMetaDataset::GDALWMSMetaDataset() : papszSubDatasets(NULL)
{
}

/************************************************************************/
/*                         ~GDALWMSMetaDataset()                        */
/************************************************************************/

GDALWMSMetaDataset::~GDALWMSMetaDataset()
{
    CSLDestroy(papszSubDatasets);
}

/************************************************************************/
/*                            AddSubDataset()                           */
/************************************************************************/

void GDALWMSMetaDataset::AddSubDataset(const char* pszName,
                                       const char* pszDesc)
{
    char    szName[80];
    int     nCount = CSLCount(papszSubDatasets ) / 2;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszName );

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszDesc);
}

/************************************************************************/
/*                        DownloadGetCapabilities()                     */
/************************************************************************/

GDALDataset *GDALWMSMetaDataset::DownloadGetCapabilities(GDALOpenInfo *poOpenInfo)
{
    const char* pszURL = poOpenInfo->pszFilename;
    if (EQUALN(pszURL, "WMS:", 4))
        pszURL += 4;

    CPLString osFormat = CPLURLGetValue(pszURL, "FORMAT");
    CPLString osTransparent = CPLURLGetValue(pszURL, "TRANSPARENT");

    CPLString osURL(pszURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WMS");
    osURL = CPLURLAddKVP(osURL, "VERSION", "1.1.1");
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetCapabilities");
    /* Remove all other keywords */
    osURL = CPLURLAddKVP(osURL, "LAYERS", NULL);
    osURL = CPLURLAddKVP(osURL, "SRS", NULL);
    osURL = CPLURLAddKVP(osURL, "BBOX", NULL);
    osURL = CPLURLAddKVP(osURL, "FORMAT", NULL);
    osURL = CPLURLAddKVP(osURL, "TRANSPARENT", NULL);
    osURL = CPLURLAddKVP(osURL, "STYLES", NULL);
    osURL = CPLURLAddKVP(osURL, "WIDTH", NULL);
    osURL = CPLURLAddKVP(osURL, "HEIGHT", NULL);

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL );
    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown",
                 psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pabyData == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    GDALDataset* poRet = AnalyzeGetCapabilities(psXML, osFormat, osTransparent);

    CPLHTTPDestroyResult(psResult);
    CPLDestroyXMLNode( psXML );

    return poRet;
}


/************************************************************************/
/*                         DownloadGetTileService()                     */
/************************************************************************/

GDALDataset *GDALWMSMetaDataset::DownloadGetTileService(GDALOpenInfo *poOpenInfo)
{
    const char* pszURL = poOpenInfo->pszFilename;
    if (EQUALN(pszURL, "WMS:", 4))
        pszURL += 4;

    CPLString osURL(pszURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WMS");
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetTileService");
    /* Remove all other keywords */
    osURL = CPLURLAddKVP(osURL, "VERSION", NULL);
    osURL = CPLURLAddKVP(osURL, "LAYERS", NULL);
    osURL = CPLURLAddKVP(osURL, "SRS", NULL);
    osURL = CPLURLAddKVP(osURL, "BBOX", NULL);
    osURL = CPLURLAddKVP(osURL, "FORMAT", NULL);
    osURL = CPLURLAddKVP(osURL, "TRANSPARENT", NULL);
    osURL = CPLURLAddKVP(osURL, "STYLES", NULL);
    osURL = CPLURLAddKVP(osURL, "WIDTH", NULL);
    osURL = CPLURLAddKVP(osURL, "HEIGHT", NULL);

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL );
    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown",
                 psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pabyData == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    GDALDataset* poRet = AnalyzeGetTileService(psXML);

    CPLHTTPDestroyResult(psResult);
    CPLDestroyXMLNode( psXML );

    return poRet;
}
/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALWMSMetaDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void GDALWMSMetaDataset::AddSubDataset( const char* pszLayerName,
                                          const char* pszTitle,
                                          const char* pszAbstract,
                                          const char* pszSRS,
                                          const char* pszMinX,
                                          const char* pszMinY,
                                          const char* pszMaxX,
                                          const char* pszMaxY,
                                          CPLString osFormat,
                                          CPLString osTransparent)

{
    CPLString osSubdatasetName = "WMS:";
    osSubdatasetName += osGetURL;
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "SERVICE", "WMS");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "VERSION", "1.1.1");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "REQUEST", "GetMap");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "LAYERS", pszLayerName);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "SRS", pszSRS);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "BBOX",
             CPLSPrintf("%s,%s,%s,%s", pszMinX, pszMinY, pszMaxX, pszMaxY));
    if (osFormat.size() != 0)
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "FORMAT",
                                        osFormat);
    if (osTransparent.size() != 0)
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "TRANSPARENT",
                                        osTransparent);

    if (pszTitle)
    {
        if (osXMLEncoding.size() != 0 &&
            osXMLEncoding != "utf-8" &&
            osXMLEncoding != "UTF-8")
        {
            char* pszRecodedTitle = CPLRecode(pszTitle, osXMLEncoding.c_str(),
                                              CPL_ENC_UTF8);
            if (pszRecodedTitle)
                AddSubDataset(osSubdatasetName, pszRecodedTitle);
            else
                AddSubDataset(osSubdatasetName, pszTitle);
            CPLFree(pszRecodedTitle);
        }
        else
        {
            AddSubDataset(osSubdatasetName, pszTitle);
        }
    }
    else
    {
        AddSubDataset(osSubdatasetName, pszLayerName);
    }
}

/************************************************************************/
/*                             ExploreLayer()                           */
/************************************************************************/

void GDALWMSMetaDataset::ExploreLayer(CPLXMLNode* psXML,
                                        CPLString osFormat,
                                        CPLString osTransparent)
{
    const char* pszName = CPLGetXMLValue(psXML, "Name", NULL);
    const char* pszTitle = CPLGetXMLValue(psXML, "Title", NULL);
    const char* pszAbstract = CPLGetXMLValue(psXML, "Abstract", NULL);

    if (pszName != NULL)
    {
        const char* pszSRS;
        CPLXMLNode* psSRS = CPLGetXMLNode( psXML, "BoundingBox" );
        if (psSRS == NULL)
        {
            psSRS = CPLGetXMLNode( psXML, "LatLonBoundingBox" );
            if (psSRS)
                pszSRS = "EPSG:4326";
        }
        else
            pszSRS = CPLGetXMLValue(psSRS, "SRS", NULL);

        if (psSRS)
        {
            const char* pszMinX = CPLGetXMLValue(psSRS, "minx", NULL);
            const char* pszMinY = CPLGetXMLValue(psSRS, "miny", NULL);
            const char* pszMaxX = CPLGetXMLValue(psSRS, "maxx", NULL);
            const char* pszMaxY = CPLGetXMLValue(psSRS, "maxy", NULL);

            CPLString osLocalTransparent(osTransparent);
            if (osLocalTransparent.size() == 0)
            {
                const char* pszOpaque = CPLGetXMLValue(psXML, "opaque", "0");
                if (EQUAL(pszOpaque, "1"))
                    osLocalTransparent = "FALSE";
            }

            if (pszSRS && pszMinX && pszMinY && pszMaxX && pszMaxY)
                AddSubDataset(pszName, pszTitle, pszAbstract,
                              pszSRS, pszMinX, pszMinY,
                              pszMaxX, pszMaxY, osFormat, osLocalTransparent);
        }
    }

    CPLXMLNode* psIter = psXML->psChild;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element)
        {
            if (EQUAL(psIter->pszValue, "Layer"))
                ExploreLayer(psIter, osFormat, osTransparent);
        }
    }
}

/************************************************************************/
/*                        AnalyzeGetCapabilities()                      */
/************************************************************************/

GDALDataset* GDALWMSMetaDataset::AnalyzeGetCapabilities(CPLXMLNode* psXML,
                                                          CPLString osFormat,
                                                          CPLString osTransparent)
{
    const char* pszEncoding = NULL;
    if (psXML->eType == CXT_Element && strcmp(psXML->pszValue, "?xml") == 0)
        pszEncoding = CPLGetXMLValue(psXML, "encoding", NULL);

    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=WMT_MS_Capabilities" );
    if (psRoot == NULL)
        psRoot = CPLGetXMLNode( psXML, "=WMS_Capabilities" );
    if (psRoot == NULL)
        return NULL;
    CPLXMLNode* psCapability = CPLGetXMLNode(psRoot, "Capability");
    if (psCapability == NULL)
        return NULL;

    CPLXMLNode* psOnlineResource = CPLGetXMLNode(psCapability,
                             "Request.GetMap.DCPType.HTTP.Get.OnlineResource");
    if (psOnlineResource == NULL)
        return NULL;
    const char* pszGetURL =
        CPLGetXMLValue(psOnlineResource, "xlink:href", NULL);
    if (pszGetURL == NULL)
        return NULL;

    CPLXMLNode* psLayer = CPLGetXMLNode(psCapability, "Layer");
    if (psLayer == NULL)
        return NULL;

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();
    poDS->osGetURL = pszGetURL;
    poDS->osXMLEncoding = pszEncoding ? pszEncoding : "";
    poDS->ExploreLayer(psLayer, osFormat, osTransparent);

    return poDS;
}

/************************************************************************/
/*                          AddTiledSubDataset()                        */
/************************************************************************/

void GDALWMSMetaDataset::AddTiledSubDataset(const char* pszTiledGroupName,
                                            const char* pszTitle)
{
    CPLString osSubdatasetName = "<GDAL_WMS><Service name=\"TiledWMS\"><ServerUrl>";
    osSubdatasetName += osGetURL;
    osSubdatasetName += "</ServerUrl><TiledGroupName>";
    osSubdatasetName += pszTiledGroupName;
    osSubdatasetName += "</TiledGroupName></Service></GDAL_WMS>";

    if (pszTitle)
    {
        if (osXMLEncoding.size() != 0 &&
            osXMLEncoding != "utf-8" &&
            osXMLEncoding != "UTF-8")
        {
            char* pszRecodedTitle = CPLRecode(pszTitle, osXMLEncoding.c_str(),
                                              CPL_ENC_UTF8);
            if (pszRecodedTitle)
                AddSubDataset(osSubdatasetName, pszRecodedTitle);
            else
                AddSubDataset(osSubdatasetName, pszTitle);
            CPLFree(pszRecodedTitle);
        }
        else
        {
            AddSubDataset(osSubdatasetName, pszTitle);
        }
    }
    else
    {
        AddSubDataset(osSubdatasetName, pszTiledGroupName);
    }
}

/************************************************************************/
/*                        AnalyzeGetTileService()                       */
/************************************************************************/

GDALDataset* GDALWMSMetaDataset::AnalyzeGetTileService(CPLXMLNode* psXML)
{
    const char* pszEncoding = NULL;
    if (psXML->eType == CXT_Element && strcmp(psXML->pszValue, "?xml") == 0)
        pszEncoding = CPLGetXMLValue(psXML, "encoding", NULL);

    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=WMS_Tile_Service" );
    if (psRoot == NULL)
        return NULL;
    CPLXMLNode* psTiledPatterns = CPLGetXMLNode(psRoot, "TiledPatterns");
    if (psTiledPatterns == NULL)
        return NULL;

    const char* pszURL = CPLGetXMLValue(psTiledPatterns,
                                        "OnlineResource.xlink:href", NULL);
    if (pszURL == NULL)
        return NULL;

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();
    poDS->osGetURL = pszURL;
    poDS->osXMLEncoding = pszEncoding ? pszEncoding : "";

    CPLXMLNode* psIter = psTiledPatterns->psChild;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TiledGroup"))
        {
            const char* pszName = CPLGetXMLValue(psIter, "Name", NULL);
            const char* pszTitle = CPLGetXMLValue(psIter, "Title", NULL);
            if (pszName)
                poDS->AddTiledSubDataset(pszName, pszTitle);
        }
    }

    return poDS;
}

/************************************************************************/
/*                        AnalyzeTileMapService()                       */
/************************************************************************/

GDALDataset* GDALWMSMetaDataset::AnalyzeTileMapService(CPLXMLNode* psXML)
{
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TileMapService" );
    if (psRoot == NULL)
        return NULL;
    CPLXMLNode* psTileMaps = CPLGetXMLNode(psRoot, "TileMaps");
    if (psTileMaps == NULL)
        return NULL;

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();

    CPLXMLNode* psIter = psTileMaps->psChild;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TileMap"))
        {
            const char* pszHref = CPLGetXMLValue(psIter, "href", NULL);
            const char* pszTitle = CPLGetXMLValue(psIter, "title", NULL);
            if (pszHref && pszTitle)
            {
                CPLString osHref(pszHref);
                const char* pszDup100 = strstr(pszHref, "1.0.0/1.0.0/");
                if (pszDup100)
                {
                    osHref.resize(pszDup100 - pszHref);
                    osHref += pszDup100 + strlen("1.0.0/");
                }
                poDS->AddSubDataset(osHref, pszTitle);
            }
        }
    }

    return poDS;
}