/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
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

#include "stdinc.h"
#include "wmsmetadataset.h"

/************************************************************************/
/*              GDALWMSDatasetGetConfigFromURL()                        */
/************************************************************************/

static
CPLXMLNode * GDALWMSDatasetGetConfigFromURL(GDALOpenInfo *poOpenInfo)
{
    const char* pszBaseURL = poOpenInfo->pszFilename;
    if (EQUALN(pszBaseURL, "WMS:", 4))
        pszBaseURL += 4;

    CPLString osLayer = CPLURLGetValue(pszBaseURL, "LAYERS");
    CPLString osVersion = CPLURLGetValue(pszBaseURL, "VERSION");
    CPLString osSRS = CPLURLGetValue(pszBaseURL, "SRS");
    CPLString osCRS = CPLURLGetValue(pszBaseURL, "CRS");
    CPLString osBBOX = CPLURLGetValue(pszBaseURL, "BBOX");
    CPLString osFormat = CPLURLGetValue(pszBaseURL, "FORMAT");
    CPLString osTransparent = CPLURLGetValue(pszBaseURL, "TRANSPARENT");

    /* GDAL specific extensions to alter the default settings */
    CPLString osOverviewCount = CPLURLGetValue(pszBaseURL, "OVERVIEWCOUNT");
    CPLString osTileSize = CPLURLGetValue(pszBaseURL, "TILESIZE");
    CPLString osMinResolution = CPLURLGetValue(pszBaseURL, "MINRESOLUTION");
    CPLString osBBOXOrder = CPLURLGetValue(pszBaseURL, "BBOXORDER");

    CPLString osBaseURL = pszBaseURL;
    /* Remove all keywords to get base URL */

    osBaseURL = CPLURLAddKVP(osBaseURL, "VERSION", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "REQUEST", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "LAYERS", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "SRS", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "CRS", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "BBOX", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "FORMAT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "TRANSPARENT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "STYLES", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "WIDTH", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "HEIGHT", NULL);

    osBaseURL = CPLURLAddKVP(osBaseURL, "OVERVIEWCOUNT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "TILESIZE", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "MINRESOLUTION", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "BBOXORDER", NULL);

    if (osBaseURL.size() > 0 && osBaseURL[osBaseURL.size() - 1] == '&')
        osBaseURL.resize(osBaseURL.size() - 1);

    if (osVersion.size() == 0)
        osVersion = "1.1.1";

    CPLString osSRSTag;
    CPLString osSRSValue;
    if(VersionStringToInt(osVersion.c_str())>= VersionStringToInt("1.3.0"))
    {
        if (osSRS.size())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "WMS version 1.3 and above expects CRS however SRS was set instead.");
        }
        osSRSValue = osCRS;
        osSRSTag = "CRS";
    }
    else
    {
        if (osCRS.size())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "WMS version 1.1.1 and below expects SRS however CRS was set instead.");
        }
        osSRSValue = osSRS;
        osSRSTag = "SRS";
    }

    if (osSRSValue.size() == 0)
        osSRSValue = "EPSG:4326";
    
    if (osBBOX.size() == 0)
    {
        if (osBBOXOrder.compare("yxYX") == 0)
            osBBOX = "-90,-180,90,180";
        else
            osBBOX = "-180,-90,180,90";
    }

    char** papszTokens = CSLTokenizeStringComplex(osBBOX, ",", 0, 0);
    if (CSLCount(papszTokens) != 4)
    {
        CSLDestroy(papszTokens);
        return NULL;
    }
    const char* pszMinX = papszTokens[0];
    const char* pszMinY = papszTokens[1];
    const char* pszMaxX = papszTokens[2];
    const char* pszMaxY = papszTokens[3];

    if (osBBOXOrder.compare("yxYX") == 0)
    {
        std::swap(pszMinX, pszMinY);
        std::swap(pszMaxX, pszMaxY);
    }

    double dfMinX = CPLAtofM(pszMinX);
    double dfMinY = CPLAtofM(pszMinY);
    double dfMaxX = CPLAtofM(pszMaxX);
    double dfMaxY = CPLAtofM(pszMaxY);

    if (dfMaxY <= dfMinY || dfMaxX <= dfMinX)
    {
        CSLDestroy(papszTokens);
        return NULL;
    }

    int nTileSize = atoi(osTileSize);
    if (nTileSize <= 128 || nTileSize > 2048)
        nTileSize = 1024;

    int nXSize, nYSize;

    int nOverviewCount = (osOverviewCount.size()) ? atoi(osOverviewCount) : 20;

    if (osMinResolution.size() != 0)
    {
        double dfMinResolution = CPLAtofM(osMinResolution);

        while (nOverviewCount > 20)
        {
            nOverviewCount --;
            dfMinResolution *= 2;
        }

        nXSize = (int) ((dfMaxX - dfMinX) / dfMinResolution + 0.5);
        nYSize = (int) ((dfMaxY - dfMinY) / dfMinResolution + 0.5);
    }
    else
    {
        double dfRatio = (dfMaxX - dfMinX) / (dfMaxY - dfMinY);
        if (dfRatio > 1)
        {
            nXSize = nTileSize;
            nYSize = (int) (nXSize / dfRatio);
        }
        else
        {
            nYSize = nTileSize;
            nXSize = (int) (nYSize * dfRatio);
        }

        if (nOverviewCount < 0 || nOverviewCount > 20)
            nOverviewCount = 20;

        nXSize = nXSize * (1 << nOverviewCount);
        nYSize = nYSize * (1 << nOverviewCount);
    }

    int bTransparent = osTransparent.size() ? CSLTestBoolean(osTransparent) : FALSE;

    if (osFormat.size() == 0)
    {
        if (!bTransparent)
        {
            osFormat = "image/jpeg";
        }
        else
        {
            osFormat = "image/png";
        }
    }

    char* pszEscapedURL = CPLEscapeString(osBaseURL.c_str(), -1, CPLES_XML);
    char* pszEscapedLayerURL = CPLEscapeString(osLayer.c_str(), -1, CPLES_URL);
    char* pszEscapedLayerXML = CPLEscapeString(pszEscapedLayerURL, -1, CPLES_XML);

    CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>\n"
            "  <Service name=\"WMS\">\n"
            "    <Version>%s</Version>\n"
            "    <ServerUrl>%s</ServerUrl>\n"
            "    <Layers>%s</Layers>\n"
            "    <%s>%s</%s>\n"
            "    <ImageFormat>%s</ImageFormat>\n"
            "    <Transparent>%s</Transparent>\n"
            "    <BBoxOrder>%s</BBoxOrder>\n"
            "  </Service>\n"
            "  <DataWindow>\n"
            "    <UpperLeftX>%s</UpperLeftX>\n"
            "    <UpperLeftY>%s</UpperLeftY>\n"
            "    <LowerRightX>%s</LowerRightX>\n"
            "    <LowerRightY>%s</LowerRightY>\n"
            "    <SizeX>%d</SizeX>\n"
            "    <SizeY>%d</SizeY>\n"
            "  </DataWindow>\n"
            "  <BandsCount>%d</BandsCount>\n"
            "  <BlockSizeX>%d</BlockSizeX>\n"
            "  <BlockSizeY>%d</BlockSizeY>\n"
            "  <OverviewCount>%d</OverviewCount>\n"
            "</GDAL_WMS>\n",
            osVersion.c_str(),
            pszEscapedURL,
            pszEscapedLayerXML,
            osSRSTag.c_str(),
            osSRSValue.c_str(),
            osSRSTag.c_str(),
            osFormat.c_str(),
            (bTransparent) ? "TRUE" : "FALSE",
            (osBBOXOrder.size()) ? osBBOXOrder.c_str() : "xyXY",
            pszMinX, pszMaxY, pszMaxX, pszMinY,
            nXSize, nYSize,
            (bTransparent) ? 4 : 3,
            nTileSize, nTileSize,
            nOverviewCount);

    CPLFree(pszEscapedURL);
    CPLFree(pszEscapedLayerURL);
    CPLFree(pszEscapedLayerXML);

    CSLDestroy(papszTokens);

    CPLDebug("WMS", "Opening WMS :\n%s", osXML.c_str());

    return CPLParseXMLString(osXML);
}

/************************************************************************/
/*              GDALWMSDatasetGetConfigFromTileMap()                    */
/************************************************************************/

static
CPLXMLNode * GDALWMSDatasetGetConfigFromTileMap(CPLXMLNode* psXML)
{
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TileMap" );
    if (psRoot == NULL)
        return NULL;

    CPLXMLNode* psTileSets = CPLGetXMLNode(psRoot, "TileSets");
    if (psTileSets == NULL)
        return NULL;

    const char* pszURL = CPLGetXMLValue(psRoot, "tilemapservice", NULL);

    int bCanChangeURL = TRUE;

    CPLString osURL;
    if (pszURL)
    {
        osURL = pszURL;
        /* Special hack for http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/basic/ */
        if (strlen(pszURL) > 10 &&
            strncmp(pszURL, "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/",
                            strlen("http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/")) == 0 &&
            strcmp(pszURL + strlen(pszURL) - strlen("1.0.0/"), "1.0.0/") == 0)
        {
            osURL.resize(strlen(pszURL) - strlen("1.0.0/"));
            bCanChangeURL = FALSE;
        }
        osURL += "${z}/${x}/${y}.${format}";
    }

    const char* pszSRS = CPLGetXMLValue(psRoot, "SRS", NULL);
    if (pszSRS == NULL)
        return NULL;

    CPLXMLNode* psBoundingBox = CPLGetXMLNode( psRoot, "BoundingBox" );
    if (psBoundingBox == NULL)
        return NULL;

    const char* pszMinX = CPLGetXMLValue(psBoundingBox, "minx", NULL);
    const char* pszMinY = CPLGetXMLValue(psBoundingBox, "miny", NULL);
    const char* pszMaxX = CPLGetXMLValue(psBoundingBox, "maxx", NULL);
    const char* pszMaxY = CPLGetXMLValue(psBoundingBox, "maxy", NULL);
    if (pszMinX == NULL || pszMinY == NULL || pszMaxX == NULL || pszMaxY == NULL)
        return NULL;

    double dfMinX = CPLAtofM(pszMinX);
    double dfMinY = CPLAtofM(pszMinY);
    double dfMaxX = CPLAtofM(pszMaxX);
    double dfMaxY = CPLAtofM(pszMaxY);
    if (dfMaxY <= dfMinY || dfMaxX <= dfMinX)
        return NULL;

    CPLXMLNode* psTileFormat = CPLGetXMLNode( psRoot, "TileFormat" );
    if (psTileFormat == NULL)
        return NULL;

    const char* pszTileWidth = CPLGetXMLValue(psTileFormat, "width", NULL);
    const char* pszTileHeight = CPLGetXMLValue(psTileFormat, "height", NULL);
    const char* pszTileFormat = CPLGetXMLValue(psTileFormat, "extension", NULL);
    if (pszTileWidth == NULL || pszTileHeight == NULL || pszTileFormat == NULL)
        return NULL;

    int nTileWidth = atoi(pszTileWidth);
    int nTileHeight = atoi(pszTileHeight);
    if (nTileWidth < 128 || nTileHeight < 128)
        return NULL;

    CPLXMLNode* psIter = psTileSets->psChild;
    int nLevelCount = 0;
    double dfPixelSize = 0;
    for(; psIter != NULL; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "TileSet"))
        {
            const char* pszOrder =
                CPLGetXMLValue(psIter, "order", NULL);
            if (pszOrder == NULL)
            {
                CPLDebug("WMS", "Cannot find order attribute");
                return NULL;
            }
            if (atoi(pszOrder) != nLevelCount)
            {
                CPLDebug("WMS", "Expected order=%d, got %s", nLevelCount, pszOrder);
                return NULL;
            }

            const char* pszHref =
                CPLGetXMLValue(psIter, "href", NULL);
            if (nLevelCount == 0 && pszHref != NULL)
            {
                if (bCanChangeURL && strlen(pszHref) > 10 &&
                    strcmp(pszHref + strlen(pszHref) - strlen("/0"), "/0") == 0)
                {
                    osURL = pszHref;
                    osURL.resize(strlen(pszHref) - strlen("/0"));
                    osURL += "/${z}/${x}/${y}.${format}";
                }
            }
            const char* pszUnitsPerPixel =
                CPLGetXMLValue(psIter, "units-per-pixel", NULL);
            if (pszUnitsPerPixel == NULL)
                return NULL;
            dfPixelSize = CPLAtofM(pszUnitsPerPixel);

            nLevelCount++;
        }
    }

    if (nLevelCount == 0 || osURL.size() == 0)
        return NULL;

    int nXSize = 0;
    int nYSize = 0;

    while(nLevelCount > 0)
    {
        GIntBig nXSizeBig = (GIntBig)((dfMaxX - dfMinX) / dfPixelSize + 0.5);
        GIntBig nYSizeBig = (GIntBig)((dfMaxY - dfMinY) / dfPixelSize + 0.5);
        if (nXSizeBig < INT_MAX && nYSizeBig < INT_MAX)
        {
            nXSize = (int)nXSizeBig;
            nYSize = (int)nYSizeBig;
            break;
        }
        CPLDebug("WMS", "Dropping one overview level so raster size fits into 32bit...");
        dfPixelSize *= 2;
        nLevelCount --;
    }

    char* pszEscapedURL = CPLEscapeString(osURL.c_str(), -1, CPLES_XML);
    
    CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>\n"
            "  <Service name=\"TMS\">\n"
            "    <ServerUrl>%s</ServerUrl>\n"
            "    <Format>%s</Format>\n"
            "  </Service>\n"
            "  <DataWindow>\n"
            "    <UpperLeftX>%s</UpperLeftX>\n"
            "    <UpperLeftY>%s</UpperLeftY>\n"
            "    <LowerRightX>%s</LowerRightX>\n"
            "    <LowerRightY>%s</LowerRightY>\n"
            "    <TileLevel>%d</TileLevel>\n"
            "    <SizeX>%d</SizeX>\n"
            "    <SizeY>%d</SizeY>\n"
            "  </DataWindow>\n"
            "  <Projection>%s</Projection>\n"
            "  <BlockSizeX>%d</BlockSizeX>\n"
            "  <BlockSizeY>%d</BlockSizeY>\n"
            "  <BandsCount>%d</BandsCount>\n"
            "</GDAL_WMS>\n",
            pszEscapedURL,
            pszTileFormat,
            pszMinX, pszMaxY, pszMaxX, pszMinY,
            nLevelCount - 1,
            nXSize, nYSize,
            pszSRS,
            nTileWidth, nTileHeight, 3);
    CPLDebug("WMS", "Opening TMS :\n%s", osXML.c_str());

    CPLFree(pszEscapedURL);

    return CPLParseXMLString(osXML);
}

/************************************************************************/
/*             GDALWMSDatasetGetConfigFromArcGISJSON()                  */
/************************************************************************/

static CPLXMLNode* GDALWMSDatasetGetConfigFromArcGISJSON(const char* pszURL,
                                                         const char* pszContent)
{
    /* TODO : use JSONC library to parse. But we don't really need it */
    CPLString osTmpFilename(CPLSPrintf("/vsimem/WMSArcGISJSON%p", pszURL));
    VSILFILE* fp = VSIFileFromMemBuffer( osTmpFilename,
                                         (GByte*)pszContent,
                                         strlen(pszContent),
                                         FALSE);
    const char* pszLine;
    int nTileWidth = -1, nTileHeight = -1;
    int nWKID = -1;
    double dfMinX = 0, dfMaxY = 0;
    int bHasMinX = FALSE, bHasMaxY = FALSE;
    int nExpectedLevel = 0;
    double dfBaseResolution = 0;
    while((pszLine = CPLReadLine2L(fp, 4096, NULL)) != NULL)
    {
        const char* pszPtr;
        if ((pszPtr = strstr(pszLine, "\"rows\" : ")) != NULL)
            nTileHeight = atoi(pszPtr + strlen("\"rows\" : "));
        else if ((pszPtr = strstr(pszLine, "\"cols\" : ")) != NULL)
            nTileWidth = atoi(pszPtr + strlen("\"cols\" : "));
        else if ((pszPtr = strstr(pszLine, "\"wkid\" : ")) != NULL)
        {
            int nVal = atoi(pszPtr + strlen("\"wkid\" : "));
            if (nWKID < 0)
                nWKID = nVal;
            else if (nWKID != nVal)
            {
                CPLDebug("WMS", "Inconsisant WKID values : %d, %d", nVal, nWKID);
                VSIFCloseL(fp);
                return NULL;
            }
        }
        else if ((pszPtr = strstr(pszLine, "\"x\" : ")) != NULL)
        {
            bHasMinX = TRUE;
            dfMinX = CPLAtofM(pszPtr + strlen("\"x\" : "));
        }
        else if ((pszPtr = strstr(pszLine, "\"y\" : ")) != NULL)
        {
            bHasMaxY = TRUE;
            dfMaxY = CPLAtofM(pszPtr + strlen("\"y\" : "));
        }
        else if ((pszPtr = strstr(pszLine, "\"level\" : ")) != NULL)
        {
            int nLevel = atoi(pszPtr + strlen("\"level\" : "));
            if (nLevel != nExpectedLevel)
            {
                CPLDebug("WMS", "Expected level : %d, got : %d", nExpectedLevel, nLevel);
                VSIFCloseL(fp);
                return NULL;
            }

            if ((pszPtr = strstr(pszLine, "\"resolution\" : ")) != NULL)
            {
                double dfResolution = CPLAtofM(pszPtr + strlen("\"resolution\" : "));
                if (nLevel == 0)
                    dfBaseResolution = dfResolution;
            }
            else
            {
                CPLDebug("WMS", "Did not get resolution");
                VSIFCloseL(fp);
                return NULL;
            }
            nExpectedLevel ++;
        }
    }
    VSIFCloseL(fp);

    int nLevelCount = nExpectedLevel - 1;
    if (nLevelCount < 1)
    {
        CPLDebug("WMS", "Did not get levels");
        return NULL;
    }

    if (nTileWidth <= 0)
    {
        CPLDebug("WMS", "Did not get tile width");
        return NULL;
    }
    if (nTileHeight <= 0)
    {
        CPLDebug("WMS", "Did not get tile height");
        return NULL;
    }
    if (nWKID <= 0)
    {
        CPLDebug("WMS", "Did not get WKID");
        return NULL;
    }
    if (!bHasMinX)
    {
        CPLDebug("WMS", "Did not get min x");
        return NULL;
    }
    if (!bHasMaxY)
    {
        CPLDebug("WMS", "Did not get max y");
        return NULL;
    }
    
    if (nWKID == 102100)
        nWKID = 3857;

    const char* pszEndURL = strstr(pszURL, "/MapServer?f=json");
    CPLAssert(pszEndURL);
    CPLString osURL(pszURL);
    osURL.resize(pszEndURL - pszURL);

    double dfMaxX = dfMinX + dfBaseResolution * nTileWidth;
    double dfMinY = dfMaxY - dfBaseResolution * nTileHeight;

    int nTileCountX = 1;
    if (fabs(dfMinX - -180) < 1e-4 && fabs(dfMaxY - 90) < 1e-4 &&
        fabs(dfMinY - -90) < 1e-4)
    {
        nTileCountX = 2;
        dfMaxX = 180;
    }

    CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>\n"
            "  <Service name=\"TMS\">\n"
            "    <ServerUrl>%s/MapServer/tile/${z}/${y}/${x}</ServerUrl>\n"
            "  </Service>\n"
            "  <DataWindow>\n"
            "    <UpperLeftX>%.8f</UpperLeftX>\n"
            "    <UpperLeftY>%.8f</UpperLeftY>\n"
            "    <LowerRightX>%.8f</LowerRightX>\n"
            "    <LowerRightY>%.8f</LowerRightY>\n"
            "    <TileLevel>%d</TileLevel>\n"
            "    <TileCountX>%d</TileCountX>\n"
            "    <YOrigin>top</YOrigin>\n"
            "  </DataWindow>\n"
            "  <Projection>EPSG:%d</Projection>\n"
            "  <BlockSizeX>%d</BlockSizeX>\n"
            "  <BlockSizeY>%d</BlockSizeY>\n"
            "  <Cache/>\n"
            "</GDAL_WMS>\n",
            osURL.c_str(),
            dfMinX, dfMaxY, dfMaxX, dfMinY,
            nLevelCount,
            nTileCountX,
            nWKID,
            nTileWidth, nTileHeight);
    CPLDebug("WMS", "Opening TMS :\n%s", osXML.c_str());

    return CPLParseXMLString(osXML);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int GDALWMSDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    const char* pszFilename = poOpenInfo->pszFilename;
    const char* pabyHeader = (const char *) poOpenInfo->pabyHeader;
    if (poOpenInfo->nHeaderBytes == 0 &&
         EQUALN(pszFilename, "<GDAL_WMS>", 10))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes >= 10 &&
             EQUALN(pabyHeader, "<GDAL_WMS>", 10))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             (EQUALN(pszFilename, "WMS:", 4) ||
             CPLString(pszFilename).ifind("SERVICE=WMS") != std::string::npos) )
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             (strstr(pabyHeader, "<WMT_MS_Capabilities") != NULL ||
              strstr(pabyHeader, "<WMS_Capabilities") != NULL ||
              strstr(pabyHeader, "<!DOCTYPE WMT_MS_Capabilities") != NULL))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<WMS_Tile_Service") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMap version=\"1.0.0\"") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<Services") != NULL &&
             strstr(pabyHeader, "<TileMapService version=\"1.0") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMapService version=\"1.0.0\"") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             EQUALN(pszFilename, "http", 4) &&
             strstr(pszFilename, "/MapServer?f=json") != NULL)
    {
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

GDALDataset *GDALWMSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    CPLXMLNode *config = NULL;
    CPLErr ret = CE_None;

    const char* pszFilename = poOpenInfo->pszFilename;
    const char* pabyHeader = (const char *) poOpenInfo->pabyHeader;

    if (poOpenInfo->nHeaderBytes == 0 &&
        EQUALN(pszFilename, "<GDAL_WMS>", 10))
    {
        config = CPLParseXMLString(pszFilename);
    }
    else if (poOpenInfo->nHeaderBytes >= 10 &&
             EQUALN(pabyHeader, "<GDAL_WMS>", 10))
    {
        config = CPLParseXMLFile(pszFilename);
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             (EQUALN(pszFilename, "WMS:http", 8) ||
              EQUALN(pszFilename, "http", 4)) &&
             strstr(pszFilename, "/MapServer?f=json") != NULL)
    {
        if (EQUALN(pszFilename, "WMS:http", 8))
            pszFilename += 4;
        CPLString osURL(pszFilename);
        if (strstr(pszFilename, "&pretty=true") == NULL)
            osURL += "&pretty=true";
        CPLHTTPResult *psResult = CPLHTTPFetch(osURL.c_str(), NULL);
        if (psResult == NULL)
            return NULL;
        if (psResult->pabyData == NULL)
        {
            CPLHTTPDestroyResult(psResult);
            return NULL;
        }
        config = GDALWMSDatasetGetConfigFromArcGISJSON(osURL,
                                                       (const char*)psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
    }

    else if (poOpenInfo->nHeaderBytes == 0 &&
             (EQUALN(pszFilename, "WMS:", 4) ||
              CPLString(pszFilename).ifind("SERVICE=WMS") != std::string::npos))
    {
        CPLString osLayers = CPLURLGetValue(pszFilename, "LAYERS");
        CPLString osRequest = CPLURLGetValue(pszFilename, "REQUEST");
        if (osLayers.size() != 0)
            config = GDALWMSDatasetGetConfigFromURL(poOpenInfo);
        else if (EQUAL(osRequest, "GetTileService"))
            return GDALWMSMetaDataset::DownloadGetTileService(poOpenInfo);
        else
            return GDALWMSMetaDataset::DownloadGetCapabilities(poOpenInfo);
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             (strstr(pabyHeader, "<WMT_MS_Capabilities") != NULL ||
              strstr(pabyHeader, "<WMS_Capabilities") != NULL ||
              strstr(pabyHeader, "<!DOCTYPE WMT_MS_Capabilities") != NULL))
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        GDALDataset* poRet = GDALWMSMetaDataset::AnalyzeGetCapabilities(psXML);
        CPLDestroyXMLNode( psXML );
        return poRet;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<WMS_Tile_Service") != NULL)
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        GDALDataset* poRet = GDALWMSMetaDataset::AnalyzeGetTileService(psXML);
        CPLDestroyXMLNode( psXML );
        return poRet;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMap version=\"1.0.0\"") != NULL)
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        config = GDALWMSDatasetGetConfigFromTileMap(psXML);
        CPLDestroyXMLNode( psXML );
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<Services") != NULL &&
             strstr(pabyHeader, "<TileMapService version=\"1.0") != NULL)
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=Services" );
        GDALDataset* poRet = NULL;
        if (psRoot)
        {
            CPLXMLNode* psTileMapService = CPLGetXMLNode(psRoot, "TileMapService");
            if (psTileMapService)
            {
                const char* pszHref = CPLGetXMLValue(psTileMapService, "href", NULL);
                if (pszHref)
                {
                    poRet = (GDALDataset*) GDALOpen(pszHref, GA_ReadOnly);
                }
            }
        }
        CPLDestroyXMLNode( psXML );
        return poRet;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMapService version=\"1.0.0\"") != NULL)
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        GDALDataset* poRet = GDALWMSMetaDataset::AnalyzeTileMapService(psXML);
        CPLDestroyXMLNode( psXML );
        return poRet;
    }
    else
        return NULL;
    if (config == NULL) return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLDestroyXMLNode(config);
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The WMS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    GDALWMSDataset *ds = new GDALWMSDataset();
    ret = ds->Initialize(config);
    if (ret != CE_None) {
        delete ds;
        ds = NULL;
    }
    CPLDestroyXMLNode(config);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    if (ds != NULL)
    {
        ds->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
        ds->SetDescription( poOpenInfo->pszFilename );
        ds->TryLoadXML();
    }

    return ds;
}
/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GDALWMSDataset::CreateCopy( const char * pszFilename,
                                         GDALDataset *poSrcDS,
                                         int bStrict, char ** papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void * pProgressData )
{
    if (poSrcDS->GetDriver() == NULL ||
        !EQUAL(poSrcDS->GetDriver()->GetDescription(), "WMS"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset must be a WMS dataset");
        return NULL;
    }

    const char* pszXML = poSrcDS->GetMetadataItem("XML", "WMS");
    if (pszXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot get XML definition of source WMS dataset");
        return NULL;
    }

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
        return NULL;

    VSIFWriteL(pszXML, 1, strlen(pszXML), fp);
    VSIFCloseL(fp);

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                         GDALDeregister_WMS()                         */
/************************************************************************/

static void GDALDeregister_WMS( GDALDriver * )

{
    DestroyWMSMiniDriverManager();
}

/************************************************************************/
/*                          GDALRegister_WMS()                          */
/************************************************************************/

void GDALRegister_WMS() {
    GDALDriver *driver;
    if (GDALGetDriverByName("WMS") == NULL) {
        driver = new GDALDriver();

        driver->SetDescription("WMS");
        driver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Map Service");
        driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_wms.html");
        driver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        driver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

        driver->pfnOpen = GDALWMSDataset::Open;
        driver->pfnIdentify = GDALWMSDataset::Identify;
        driver->pfnUnloadDriver = GDALDeregister_WMS;
        driver->pfnCreateCopy = GDALWMSDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver(driver);

        GDALWMSMiniDriverManager *const mdm = GetGDALWMSMiniDriverManager();
        mdm->Register(new GDALWMSMiniDriverFactory_WMS());
        mdm->Register(new GDALWMSMiniDriverFactory_TileService());
        mdm->Register(new GDALWMSMiniDriverFactory_WorldWind());
        mdm->Register(new GDALWMSMiniDriverFactory_TMS());
        mdm->Register(new GDALWMSMiniDriverFactory_TiledWMS());
        mdm->Register(new GDALWMSMiniDriverFactory_VirtualEarth());
    }
}
