/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Definition of GDALWMSMetaDataset class
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

GDALWMSMetaDataset::GDALWMSMetaDataset() : papszSubDatasets(nullptr) {}

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

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszName );

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszDesc);
}

/************************************************************************/
/*                        DownloadGetCapabilities()                     */
/************************************************************************/

GDALDataset *GDALWMSMetaDataset::DownloadGetCapabilities(GDALOpenInfo *poOpenInfo)
{
    const char* pszURL = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszURL, "WMS:"))
        pszURL += 4;

    CPLString osFormat = CPLURLGetValue(pszURL, "FORMAT");
    CPLString osTransparent = CPLURLGetValue(pszURL, "TRANSPARENT");
    CPLString osVersion = CPLURLGetValue(pszURL, "VERSION");
    CPLString osPreferredSRS = CPLURLGetValue(pszURL, "SRS");
    if( osPreferredSRS.empty() )
        osPreferredSRS = CPLURLGetValue(pszURL, "CRS");

    if (osVersion.empty())
        osVersion = "1.1.1";

    CPLString osURL(pszURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WMS");
    osURL = CPLURLAddKVP(osURL, "VERSION", osVersion);
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetCapabilities");
    /* Remove all other keywords */
    osURL = CPLURLAddKVP(osURL, "LAYERS", nullptr);
    osURL = CPLURLAddKVP(osURL, "SRS", nullptr);
    osURL = CPLURLAddKVP(osURL, "CRS", nullptr);
    osURL = CPLURLAddKVP(osURL, "BBOX", nullptr);
    osURL = CPLURLAddKVP(osURL, "FORMAT", nullptr);
    osURL = CPLURLAddKVP(osURL, "TRANSPARENT", nullptr);
    osURL = CPLURLAddKVP(osURL, "STYLES", nullptr);
    osURL = CPLURLAddKVP(osURL, "WIDTH", nullptr);
    osURL = CPLURLAddKVP(osURL, "HEIGHT", nullptr);

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, nullptr );
    if (psResult == nullptr)
    {
        return nullptr;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown",
                 psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    if (psResult->pabyData == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
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
    if (STARTS_WITH_CI(pszURL, "WMS:"))
        pszURL += 4;

    CPLString osURL(pszURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WMS");
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetTileService");
    /* Remove all other keywords */
    osURL = CPLURLAddKVP(osURL, "VERSION", nullptr);
    osURL = CPLURLAddKVP(osURL, "LAYERS", nullptr);
    osURL = CPLURLAddKVP(osURL, "SRS", nullptr);
    osURL = CPLURLAddKVP(osURL, "CRS", nullptr);
    osURL = CPLURLAddKVP(osURL, "BBOX", nullptr);
    osURL = CPLURLAddKVP(osURL, "FORMAT", nullptr);
    osURL = CPLURLAddKVP(osURL, "TRANSPARENT", nullptr);
    osURL = CPLURLAddKVP(osURL, "STYLES", nullptr);
    osURL = CPLURLAddKVP(osURL, "WIDTH", nullptr);
    osURL = CPLURLAddKVP(osURL, "HEIGHT", nullptr);

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, nullptr );
    if (psResult == nullptr)
    {
        return nullptr;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown",
                 psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    if (psResult->pabyData == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    GDALDataset* poRet = AnalyzeGetTileService(psXML, poOpenInfo);

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
                                   "SUBDATASETS", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALWMSMetaDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void GDALWMSMetaDataset::AddSubDataset( const char* pszLayerName,
                                        const char* pszTitle,
                                        CPL_UNUSED const char* pszAbstract,
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
    char* pszEscapedLayerName = CPLEscapeString(pszLayerName, -1, CPLES_URL);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "LAYERS", pszEscapedLayerName);
    CPLFree(pszEscapedLayerName);
    if(VersionStringToInt(osVersion.c_str())>= VersionStringToInt("1.3.0"))
    {
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "CRS", pszSRS);
    }
    else
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "SRS", pszSRS);
    osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "BBOX",
             CPLSPrintf("%s,%s,%s,%s", pszMinX, pszMinY, pszMaxX, pszMaxY));
    if (!osFormat.empty())
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "FORMAT",
                                        osFormat);
    if (!osTransparent.empty())
        osSubdatasetName = CPLURLAddKVP(osSubdatasetName, "TRANSPARENT",
                                        osTransparent);

    if (pszTitle)
    {
        if (!osXMLEncoding.empty() &&
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
    if (!osTransparent.empty())
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
        if (!osXMLEncoding.empty() &&
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
    const char* pszName = CPLGetXMLValue(psXML, "Name", nullptr);
    const char* pszTitle = CPLGetXMLValue(psXML, "Title", nullptr);
    const char* pszAbstract = CPLGetXMLValue(psXML, "Abstract", nullptr);

    CPLXMLNode* psSRS = nullptr;
    const char* pszSRSLocal = nullptr;
    const char* pszMinXLocal = nullptr;
    const char* pszMinYLocal = nullptr;
    const char* pszMaxXLocal = nullptr;
    const char* pszMaxYLocal = nullptr;

    const char* pszSRSTagName =
        VersionStringToInt(osVersion.c_str()) >= VersionStringToInt("1.3.0") ? "CRS" : "SRS";

    /* Use local bounding box if available, otherwise use the one */
    /* that comes from an upper layer */
    /* such as in http://neowms.sci.gsfc.nasa.gov/wms/wms */
    CPLXMLNode* psIter = psXML->psChild;
    while( psIter != nullptr )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "BoundingBox") == 0 )
        {
            psSRS = psIter;
            pszSRSLocal = CPLGetXMLValue(psSRS, pszSRSTagName, nullptr);
            if( osPreferredSRS.empty() || pszSRSLocal == nullptr )
                break;
            if( EQUAL(osPreferredSRS, pszSRSLocal) )
                break;
            psSRS = nullptr;
            pszSRSLocal = nullptr;
        }
        psIter = psIter->psNext;
    }

    if (psSRS == nullptr)
    {
        psSRS = CPLGetXMLNode( psXML, "LatLonBoundingBox" );
        pszSRSLocal = CPLGetXMLValue(psXML, pszSRSTagName, nullptr);
        if (pszSRSLocal == nullptr)
            pszSRSLocal = "EPSG:4326";
    }

    if (pszSRSLocal != nullptr && psSRS != nullptr)
    {
        pszMinXLocal = CPLGetXMLValue(psSRS, "minx", nullptr);
        pszMinYLocal = CPLGetXMLValue(psSRS, "miny", nullptr);
        pszMaxXLocal = CPLGetXMLValue(psSRS, "maxx", nullptr);
        pszMaxYLocal = CPLGetXMLValue(psSRS, "maxy", nullptr);

        if (pszMinXLocal && pszMinYLocal && pszMaxXLocal && pszMaxYLocal)
        {
            pszSRS = pszSRSLocal;
            pszMinX = pszMinXLocal;
            pszMinY = pszMinYLocal;
            pszMaxX = pszMaxXLocal;
            pszMaxY = pszMaxYLocal;
        }
    }

    if (pszName != nullptr && pszSRS && pszMinX && pszMinY && pszMaxX && pszMaxY)
    {
        CPLString osLocalTransparent(osTransparent);
        if (osLocalTransparent.empty())
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
    for(; psIter != nullptr; psIter = psIter->psNext)
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
            const char* pszSRS = CPLGetXMLValue(psIter, "SRS", nullptr);
            if (pszSRS == nullptr)
                continue;

            CPLXMLNode* psBoundingBox = CPLGetXMLNode( psIter, "BoundingBox" );
            if (psBoundingBox == nullptr)
                continue;

            const char* pszMinX = CPLGetXMLValue(psBoundingBox, "minx", nullptr);
            const char* pszMinY = CPLGetXMLValue(psBoundingBox, "miny", nullptr);
            const char* pszMaxX = CPLGetXMLValue(psBoundingBox, "maxx", nullptr);
            const char* pszMaxY = CPLGetXMLValue(psBoundingBox, "maxy", nullptr);
            if (pszMinX == nullptr || pszMinY == nullptr || pszMaxX == nullptr || pszMaxY == nullptr)
                continue;

            double dfMinX = CPLAtofM(pszMinX);
            double dfMinY = CPLAtofM(pszMinY);
            double dfMaxX = CPLAtofM(pszMaxX);
            double dfMaxY = CPLAtofM(pszMaxY);
            if (dfMaxY <= dfMinY || dfMaxX <= dfMinX)
                continue;

            const char* pszFormat = CPLGetXMLValue( psIter, "Format", nullptr );
            if (pszFormat == nullptr)
                continue;
            if (strstr(pszFormat, "kml"))
                continue;

            const char* pszTileWidth = CPLGetXMLValue(psIter, "Width", nullptr);
            const char* pszTileHeight = CPLGetXMLValue(psIter, "Height", nullptr);
            if (pszTileWidth == nullptr || pszTileHeight == nullptr)
                continue;

            int nTileWidth = atoi(pszTileWidth);
            int nTileHeight = atoi(pszTileHeight);
            if (nTileWidth < 128 || nTileHeight < 128)
                continue;

            const char* pszLayers = CPLGetXMLValue(psIter, "Layers", nullptr);
            if (pszLayers == nullptr)
                continue;

            const char* pszResolutions = CPLGetXMLValue(psIter, "Resolutions", nullptr);
            if (pszResolutions == nullptr)
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
    const char* pszEncoding = nullptr;
    if (psXML->eType == CXT_Element && strcmp(psXML->pszValue, "?xml") == 0)
        pszEncoding = CPLGetXMLValue(psXML, "encoding", nullptr);

    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=WMT_MS_Capabilities" );
    if (psRoot == nullptr)
        psRoot = CPLGetXMLNode( psXML, "=WMS_Capabilities" );
    if (psRoot == nullptr)
        return nullptr;
    CPLXMLNode* psCapability = CPLGetXMLNode(psRoot, "Capability");
    if (psCapability == nullptr)
        return nullptr;

    CPLXMLNode* psOnlineResource = CPLGetXMLNode(psCapability,
                             "Request.GetMap.DCPType.HTTP.Get.OnlineResource");
    if (psOnlineResource == nullptr)
        return nullptr;
    const char* pszGetURL =
        CPLGetXMLValue(psOnlineResource, "xlink:href", nullptr);
    if (pszGetURL == nullptr)
        return nullptr;

    CPLXMLNode* psLayer = CPLGetXMLNode(psCapability, "Layer");
    if (psLayer == nullptr)
        return nullptr;

    CPLXMLNode* psVendorSpecificCapabilities =
        CPLGetXMLNode(psCapability, "VendorSpecificCapabilities");

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();
    const char* pszVersion = CPLGetXMLValue(psRoot, "version", nullptr);
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

// tiledWMS only
void GDALWMSMetaDataset::AddTiledSubDataset(const char* pszTiledGroupName,
                                            const char* pszTitle,
                                            const char* const* papszChanges)
{
    CPLString osSubdatasetName = "<GDAL_WMS><Service name=\"TiledWMS\"><ServerUrl>";
    osSubdatasetName += osGetURL;
    osSubdatasetName += "</ServerUrl><TiledGroupName>";
    osSubdatasetName += pszTiledGroupName;
    osSubdatasetName += "</TiledGroupName>";

    for (int i = 0; papszChanges != nullptr && papszChanges[i] != nullptr; i++)
    {
        char* key = nullptr;
        const char* value = CPLParseNameValue(papszChanges[i], &key);
        if (value != nullptr && key != nullptr)
            osSubdatasetName += CPLSPrintf("<Change key=\"${%s}\">%s</Change>", key, value);
        CPLFree(key);
    }

    osSubdatasetName += "</Service></GDAL_WMS>";

    if (pszTitle)
    {
        if (!osXMLEncoding.empty() &&
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
// tiledWMS only
void GDALWMSMetaDataset::AnalyzeGetTileServiceRecurse(CPLXMLNode* psXML, GDALOpenInfo * poOpenInfo)
{
    // Only list tiled groups that contain the string in the open option TiledGroupName, if given
    char **papszLocalOpenOptions = poOpenInfo ? poOpenInfo->papszOpenOptions : nullptr;
    CPLString osMatch(CSLFetchNameValueDef(papszLocalOpenOptions, "TiledGroupName",""));
    osMatch.toupper();
    // Also pass the change patterns, if provided
    char **papszChanges = CSLFetchNameValueMultiple(papszLocalOpenOptions, "Change");

    CPLXMLNode* psIter = psXML->psChild;
    for(; psIter != nullptr; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element && EQUAL(psIter->pszValue, "TiledGroup"))
        {
            const char *pszName = CPLGetXMLValue(psIter, "Name", nullptr);
            if (pszName)
            {
                const char* pszTitle = CPLGetXMLValue(psIter, "Title", nullptr);
                if (osMatch.empty())
                {
                    AddTiledSubDataset(pszName, pszTitle, papszChanges);
                }
                else
                {
                    CPLString osNameUpper(pszName);
                    osNameUpper.toupper();
                    if (std::string::npos != osNameUpper.find(osMatch))
                        AddTiledSubDataset(pszName, pszTitle, papszChanges);
                }
            }
        }
        else if (psIter->eType == CXT_Element && EQUAL(psIter->pszValue, "TiledGroups"))
        {
            AnalyzeGetTileServiceRecurse(psIter, poOpenInfo);
        }
    }
    CPLFree(papszChanges);
}

/************************************************************************/
/*                        AnalyzeGetTileService()                       */
/************************************************************************/
// tiledWMS only
GDALDataset* GDALWMSMetaDataset::AnalyzeGetTileService(CPLXMLNode* psXML, GDALOpenInfo * poOpenInfo)
{
    const char* pszEncoding = nullptr;
    if (psXML->eType == CXT_Element && strcmp(psXML->pszValue, "?xml") == 0)
        pszEncoding = CPLGetXMLValue(psXML, "encoding", nullptr);

    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=WMS_Tile_Service" );
    if (psRoot == nullptr)
        return nullptr;
    CPLXMLNode* psTiledPatterns = CPLGetXMLNode(psRoot, "TiledPatterns");
    if (psTiledPatterns == nullptr)
        return nullptr;

    const char* pszURL = CPLGetXMLValue(psTiledPatterns,
                                        "OnlineResource.xlink:href", nullptr);
    if (pszURL == nullptr)
        return nullptr;

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();
    poDS->osGetURL = pszURL;
    poDS->osXMLEncoding = pszEncoding ? pszEncoding : "";

    poDS->AnalyzeGetTileServiceRecurse(psTiledPatterns, poOpenInfo);

    return poDS;
}

/************************************************************************/
/*                        AnalyzeTileMapService()                       */
/************************************************************************/

GDALDataset* GDALWMSMetaDataset::AnalyzeTileMapService(CPLXMLNode* psXML)
{
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TileMapService" );
    if (psRoot == nullptr)
        return nullptr;
    CPLXMLNode* psTileMaps = CPLGetXMLNode(psRoot, "TileMaps");
    if (psTileMaps == nullptr)
        return nullptr;

    GDALWMSMetaDataset* poDS = new GDALWMSMetaDataset();

    CPLXMLNode* psIter = psTileMaps->psChild;
    for(; psIter != nullptr; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TileMap"))
        {
            const char* pszHref = CPLGetXMLValue(psIter, "href", nullptr);
            const char* pszTitle = CPLGetXMLValue(psIter, "title", nullptr);
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
