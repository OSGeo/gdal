/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Definition of GDALWMSMetaDataset class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

int VersionStringToInt(const char *version);

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
    CPLString osVersion = CPLURLGetValue(pszURL, "VERSION");
    CPLString osPreferredSRS = CPLURLGetValue(pszURL, "SRS");
    if( osPreferredSRS.size() == 0 )
        osPreferredSRS = CPLURLGetValue(pszURL, "CRS");

    if (osVersion.size() == 0)
        osVersion = "1.1.1";

    CPLString osURL(pszURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WMS");
    osURL = CPLURLAddKVP(osURL, "VERSION", osVersion);
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetCapabilities");
    /* Remove all other keywords */
    osURL = CPLURLAddKVP(osURL, "LAYERS", NULL);
    osURL = CPLURLAddKVP(osURL, "SRS", NULL);
    osURL = CPLURLAddKVP(osURL, "CRS", NULL);
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

    GDALDataset* poRet = AnalyzeGetCapabilities(psXML, osFormat, osTransparent, osPreferredSRS);

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
    osURL = CPLURLAddKVP(osURL, "CRS", NULL);
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
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GDALWMSMetaDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
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
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "VERSION", osVersion);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "REQUEST", "GetMap");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "LAYERS", pszLayerName);
    if(VersionStringToInt(osVersion.c_str())>= VersionStringToInt("1.3.0"))
    {
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "CRS", pszSRS);
        /* FIXME: this should apply to all SRS that need axis inversion */
        if (strcmp(pszSRS, "EPSG:4326") == 0)
            osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "BBOXORDER", "yxYX");
    }
    else
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
/*                         AddWMSCSubDataset()                          */
/************************************************************************/

void GDALWMSMetaDataset::AddWMSCSubDataset(WMSCTileSetDesc& oWMSCTileSetDesc,
                                          const char* pszTitle,
                                          CPLString osTransparent)
{
    CPLString osSubdatasetName = "WMS:";
    osSubdatasetName += osGetURL;
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "SERVICE", "WMS");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "VERSION", osVersion);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "REQUEST", "GetMap");
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "LAYERS", oWMSCTileSetDesc.osLayers);
    if(VersionStringToInt(osVersion.c_str())>= VersionStringToInt("1.3.0"))
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "CRS", oWMSCTileSetDesc.osSRS);
    else
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "SRS", oWMSCTileSetDesc.osSRS);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "BBOX",
             CPLSPrintf("%s,%s,%s,%s", oWMSCTileSetDesc.osMinX.c_str(),
                                       oWMSCTileSetDesc.osMinY.c_str(),
                                       oWMSCTileSetDesc.osMaxX.c_str(),
                                       oWMSCTileSetDesc.osMaxY.c_str()));

    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "FORMAT", oWMSCTileSetDesc.osFormat);
    if (osTransparent.size() != 0)
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "TRANSPARENT",
                                        osTransparent);
    if (oWMSCTileSetDesc.nTileWidth != oWMSCTileSetDesc.nTileHeight)
        CPLDebug("WMS", "Weird: nTileWidth != nTileHeight for %s",
                 oWMSCTileSetDesc.osLayers.c_str());
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "TILESIZE",
                                    CPLSPrintf("%d", oWMSCTileSetDesc.nTileWidth));
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "OVERVIEWCOUNT",
                                    CPLSPrintf("%d", oWMSCTileSetDesc.nResolutions - 1));
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "MINRESOLUTION",
                                    CPLSPrintf("%.16f", oWMSCTileSetDesc.dfMinResolution));
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "TILED", "true");

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
        AddSubDataset(osSubdatasetName, oWMSCTileSetDesc.osLayers);
    }
}

/************************************************************************/
/*                             ExploreLayer()                           */
/************************************************************************/

void GDALWMSMetaDataset::ExploreLayer(CPLXMLNode* psXML,
                                      CPLString osFormat,
                                      CPLString osTransparent,
                                      CPLString osPreferredSRS,
                                      const char* pszSRS,
                                      const char* pszMinX,
                                      const char* pszMinY,
                                      const char* pszMaxX,
                                      const char* pszMaxY)
{
    const char* pszName = CPLGetXMLValue(psXML, "Name", NULL);
    const char* pszTitle = CPLGetXMLValue(psXML, "Title", NULL);
    const char* pszAbstract = CPLGetXMLValue(psXML, "Abstract", NULL);

    CPLXMLNode* psSRS = NULL;
    const char* pszSRSLocal = NULL;
    const char* pszMinXLocal = NULL;
    const char* pszMinYLocal = NULL;
    const char* pszMaxXLocal = NULL;
    const char* pszMaxYLocal = NULL;

    const char* pszSRSTagName =
        VersionStringToInt(osVersion.c_str()) >= VersionStringToInt("1.3.0") ? "CRS" : "SRS";

    /* Use local bounding box if available, otherwise use the one */
    /* that comes from an upper layer */
    /* such as in http://neowms.sci.gsfc.nasa.gov/wms/wms */
    CPLXMLNode* psIter = psXML->psChild;
    while( psIter != NULL )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "BoundingBox") == 0 )
        {
            psSRS = psIter;
            pszSRSLocal = CPLGetXMLValue(psSRS, pszSRSTagName, NULL);
            if( osPreferredSRS.size() == 0 || pszSRSLocal == NULL )
                break;
            if( EQUAL(osPreferredSRS, pszSRSLocal) )
                break;
            psSRS = NULL;
            pszSRSLocal = NULL;
        }
        psIter = psIter->psNext;
    }

    if (psSRS == NULL)
    {
        psSRS = CPLGetXMLNode( psXML, "LatLonBoundingBox" );
        pszSRSLocal = CPLGetXMLValue(psXML, pszSRSTagName, NULL);
        if (pszSRSLocal == NULL)
            pszSRSLocal = "EPSG:4326";
    }


    if (pszSRSLocal != NULL && psSRS != NULL)
    {
        pszMinXLocal = CPLGetXMLValue(psSRS, "minx", NULL);
        pszMinYLocal = CPLGetXMLValue(psSRS, "miny", NULL);
        pszMaxXLocal = CPLGetXMLValue(psSRS, "maxx", NULL);
        pszMaxYLocal = CPLGetXMLValue(psSRS, "maxy", NULL);

        if (pszMinXLocal && pszMinYLocal && pszMaxXLocal && pszMaxYLocal)
        {
            pszSRS = pszSRSLocal;
            pszMinX = pszMinXLocal;
            pszMinY = pszMinYLocal;
            pszMaxX = pszMaxXLocal;
            pszMaxY = pszMaxYLocal;
        }
    }

    if (pszName != NULL && pszSRS && pszMinX && pszMinY && pszMaxX && pszMaxY)
    {
        CPLString osLocalTransparent(osTransparent);
        if (osLocalTransparent.size() == 0)
        {
            const char* pszOpaque = CPLGetXMLValue(psXML, "opaque", "0");
            if (EQUAL(pszOpaque, "1"))
                osLocalTransparent = "FALSE";
        }

        WMSCKeyType oWMSCKey(pszName, pszSRS);
        std::map<WMSCKeyType, WMSCTileSetDesc>::iterator oIter = osMapWMSCTileSet.find(oWMSCKey);
        if (oIter != osMapWMSCTileSet.end())
        {
            AddWMSCSubDataset(oIter->second, pszTitle, osLocalTransparent);
        }
        else
        {
            AddSubDataset(pszName, pszTitle, pszAbstract,
                          pszSRS, pszMinX, pszMinY,
                          pszMaxX, pszMaxY, osFormat, osLocalTransparent);
        }
    }

    psIter = psXML->psChild;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element)
        {
            if (EQUAL(psIter->pszValue, "Layer"))
                ExploreLayer(psIter, osFormat, osTransparent, osPreferredSRS,
                             pszSRS, pszMinX, pszMinY, pszMaxX, pszMaxY);
        }
    }
}

/************************************************************************/
/*                         ParseWMSCTileSets()                          */
/************************************************************************/

void GDALWMSMetaDataset::ParseWMSCTileSets(CPLXMLNode* psXML)
{
    CPLXMLNode* psIter = psXML->psChild;
    for(;psIter;psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element && EQUAL(psIter->pszValue, "TileSet"))
        {
            const char* pszSRS = CPLGetXMLValue(psIter, "SRS", NULL);
            if (pszSRS == NULL)
                continue;

            CPLXMLNode* psBoundingBox = CPLGetXMLNode( psIter, "BoundingBox" );
            if (psBoundingBox == NULL)
                continue;

            const char* pszMinX = CPLGetXMLValue(psBoundingBox, "minx", NULL);
            const char* pszMinY = CPLGetXMLValue(psBoundingBox, "miny", NULL);
            const char* pszMaxX = CPLGetXMLValue(psBoundingBox, "maxx", NULL);
            const char* pszMaxY = CPLGetXMLValue(psBoundingBox, "maxy", NULL);
            if (pszMinX == NULL || pszMinY == NULL || pszMaxX == NULL || pszMaxY == NULL)
                continue;

            double dfMinX = CPLAtofM(pszMinX);
            double dfMinY = CPLAtofM(pszMinY);
            double dfMaxX = CPLAtofM(pszMaxX);
            double dfMaxY = CPLAtofM(pszMaxY);
            if (dfMaxY <= dfMinY || dfMaxX <= dfMinX)
                continue;

            const char* pszFormat = CPLGetXMLValue( psIter, "Format", NULL );
            if (pszFormat == NULL)
                continue;
            if (strstr(pszFormat, "kml"))
                continue;

            const char* pszTileWidth = CPLGetXMLValue(psIter, "Width", NULL);
            const char* pszTileHeight = CPLGetXMLValue(psIter, "Height", NULL);
            if (pszTileWidth == NULL || pszTileHeight == NULL)
                continue;

            int nTileWidth = atoi(pszTileWidth);
            int nTileHeight = atoi(pszTileHeight);
            if (nTileWidth < 128 || nTileHeight < 128)
                continue;

            const char* pszLayers = CPLGetXMLValue(psIter, "Layers", NULL);
            if (pszLayers == NULL)
                continue;

            const char* pszResolutions = CPLGetXMLValue(psIter, "Resolutions", NULL);
            if (pszResolutions == NULL)
                continue;
            char** papszTokens = CSLTokenizeStringComplex(pszResolutions, " ", 0, 0);
            double dfMinResolution = 0;
            int i;
            for(i=0; papszTokens && papszTokens[i]; i++)
            {
                double dfResolution = CPLAtofM(papszTokens[i]);
                if (i==0 || dfResolution < dfMinResolution)
                    dfMinResolution = dfResolution;
            }
            CSLDestroy(papszTokens);
            int nResolutions = i;
            if (nResolutions == 0)
                continue;

            const char* pszStyles = CPLGetXMLValue(psIter, "Styles", "");

            /* http://demo.opengeo.org/geoserver/gwc/service/wms?tiled=TRUE&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetCapabilities */
            /* has different variations of formats for the same (formats, SRS) tuple, so just */
            /* keep the first one which is a png format */
            WMSCKeyType oWMSCKey(pszLayers, pszSRS);
            std::map<WMSCKeyType, WMSCTileSetDesc>::iterator oIter = osMapWMSCTileSet.find(oWMSCKey);
            if (oIter != osMapWMSCTileSet.end())
                continue;

            WMSCTileSetDesc oWMSCTileSet;
            oWMSCTileSet.osLayers = pszLayers;
            oWMSCTileSet.osSRS = pszSRS;
            oWMSCTileSet.osMinX = pszMinX;
            oWMSCTileSet.osMinY = pszMinY;
            oWMSCTileSet.osMaxX = pszMaxX;
            oWMSCTileSet.osMaxY = pszMaxY;
            oWMSCTileSet.dfMinX = dfMinX;
            oWMSCTileSet.dfMinY = dfMinY;
            oWMSCTileSet.dfMaxX = dfMaxX;
            oWMSCTileSet.dfMaxY = dfMaxY;
            oWMSCTileSet.nResolutions = nResolutions;
            oWMSCTileSet.dfMinResolution = dfMinResolution;
            oWMSCTileSet.osFormat = pszFormat;
            oWMSCTileSet.osStyle = pszStyles;
            oWMSCTileSet.nTileWidth = nTileWidth;
            oWMSCTileSet.nTileHeight = nTileHeight;

            osMapWMSCTileSet[oWMSCKey] = oWMSCTileSet;
        }
    }
}

/************************************************************************/
/*                        AnalyzeGetCapabilities()                      */
/************************************************************************/

GDALDataset* GDALWMSMetaDataset::AnalyzeGetCapabilities(CPLXMLNode* psXML,
                                                          CPLString osFormat,
                                                          CPLString osTransparent,
                                                          CPLString osPreferredSRS)
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

    CPLXMLNode* psVendorSpecificCapabilities =
        CPLGetXMLNode(psCapability, "VendorSpecificCapabilities");

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();
    const char* pszVersion = CPLGetXMLValue(psRoot, "version", NULL);
    if (pszVersion)
        poDS->osVersion = pszVersion;
    else
        poDS->osVersion = "1.1.1";
    poDS->osGetURL = pszGetURL;
    poDS->osXMLEncoding = pszEncoding ? pszEncoding : "";
    if (psVendorSpecificCapabilities)
        poDS->ParseWMSCTileSets(psVendorSpecificCapabilities);
    poDS->ExploreLayer(psLayer, osFormat, osTransparent, osPreferredSRS);

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
/*                     AnalyzeGetTileServiceRecurse()                   */
/************************************************************************/

void GDALWMSMetaDataset::AnalyzeGetTileServiceRecurse(CPLXMLNode* psXML)
{
    CPLXMLNode* psIter = psXML->psChild;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TiledGroup"))
        {
            const char* pszName = CPLGetXMLValue(psIter, "Name", NULL);
            const char* pszTitle = CPLGetXMLValue(psIter, "Title", NULL);
            if (pszName)
                AddTiledSubDataset(pszName, pszTitle);
        }
        else if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TiledGroups"))
        {
            AnalyzeGetTileServiceRecurse(psIter);
        }
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

    poDS->AnalyzeGetTileServiceRecurse(psTiledPatterns);

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

