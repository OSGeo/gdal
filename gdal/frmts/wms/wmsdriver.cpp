/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "wmsdriver.h"
#include "wmsmetadataset.h"

#include "minidriver_wms.h"
#include "minidriver_tileservice.h"
#include "minidriver_worldwind.h"
#include "minidriver_tms.h"
#include "minidriver_tiled_wms.h"
#include "minidriver_virtualearth.h"
#include "minidriver_arcgis_server.h"
#include "minidriver_iip.h"
#include "minidriver_mrf.h"

#include <limits>
#include <utility>

CPL_CVSID("$Id$")

//
// A static map holding seen server GetTileService responses, per process
// It makes opening and reopening rasters from the same server faster
//
GDALWMSDataset::StringMap_t GDALWMSDataset::cfg;
CPLMutex *GDALWMSDataset::cfgmtx = NULL;


/************************************************************************/
/*              GDALWMSDatasetGetConfigFromURL()                        */
/************************************************************************/

static
CPLXMLNode * GDALWMSDatasetGetConfigFromURL(GDALOpenInfo *poOpenInfo)
{
    const char* pszBaseURL = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszBaseURL, "WMS:"))
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

    if (!osBaseURL.empty() && osBaseURL.back() == '&')
        osBaseURL.resize(osBaseURL.size() - 1);

    if (osVersion.empty())
        osVersion = "1.1.1";

    CPLString osSRSTag;
    CPLString osSRSValue;
    if(VersionStringToInt(osVersion.c_str())>= VersionStringToInt("1.3.0"))
    {
        if (!osSRS.empty() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "WMS version 1.3 and above expects CRS however SRS was set instead.");
        }
        osSRSValue = osCRS;
        osSRSTag = "CRS";
    }
    else
    {
        if (!osCRS.empty() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "WMS version 1.1.1 and below expects SRS however CRS was set instead.");
        }
        osSRSValue = osSRS;
        osSRSTag = "SRS";
    }

    if (osSRSValue.empty())
        osSRSValue = "EPSG:4326";

    if (osBBOX.empty())
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
    double dXSize, dYSize;

    int nOverviewCount = (osOverviewCount.size()) ? atoi(osOverviewCount) : 20;

    if (!osMinResolution.empty())
    {
        double dfMinResolution = CPLAtofM(osMinResolution);

        while (nOverviewCount > 20)
        {
            nOverviewCount --;
            dfMinResolution *= 2;
        }

        // Determine a suitable size that doesn't overflow max int.
        dXSize = ((dfMaxX - dfMinX) / dfMinResolution + 0.5);
        dYSize = ((dfMaxY - dfMinY) / dfMinResolution + 0.5);

        while (dXSize > (std::numeric_limits<int>::max)() ||
               dYSize > (std::numeric_limits<int>::max)())
        {
            dfMinResolution *= 2;

            dXSize = ((dfMaxX - dfMinX) / dfMinResolution + 0.5);
            dYSize = ((dfMaxY - dfMinY) / dfMinResolution + 0.5);
        }
    }
    else
    {
        double dfRatio = (dfMaxX - dfMinX) / (dfMaxY - dfMinY);
        if (dfRatio > 1)
        {
            dXSize = nTileSize;
            dYSize = dXSize / dfRatio;
        }
        else
        {
            dYSize = nTileSize;
            dXSize = dYSize * dfRatio;
        }

        if (nOverviewCount < 0 || nOverviewCount > 20)
            nOverviewCount = 20;

        dXSize = dXSize * (1 << nOverviewCount);
        dYSize = dYSize * (1 << nOverviewCount);

        // Determine a suitable size that doesn't overflow max int.
        while (dXSize > (std::numeric_limits<int>::max)() ||
               dYSize > (std::numeric_limits<int>::max)())
        {
            dXSize /= 2;
            dYSize /= 2;
        }
    }

    nXSize = (int) dXSize;
    nYSize = (int) dYSize;

    bool bTransparent = !osTransparent.empty() && CPLTestBool(osTransparent);

    if (osFormat.empty())
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
    char* pszEscapedLayerXML = CPLEscapeString(osLayer.c_str(), -1, CPLES_XML);

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
            STARTS_WITH(pszURL, "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/") &&
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

    if (nLevelCount == 0 || osURL.empty())
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
/*                             GetJSonValue()                           */
/************************************************************************/

static const char* GetJSonValue(const char* pszLine, const char* pszKey)
{
    const char* pszJSonKey = CPLSPrintf("\"%s\" : ", pszKey);
    const char* pszPtr;
    if( (pszPtr = strstr(pszLine, pszJSonKey)) != NULL )
        return pszPtr + strlen(pszJSonKey);
    pszJSonKey = CPLSPrintf("\"%s\": ", pszKey);
    if( (pszPtr = strstr(pszLine, pszJSonKey)) != NULL )
        return pszPtr + strlen(pszJSonKey);
    return NULL;
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
    int nTileWidth = -1;
    int nTileHeight = -1;
    int nWKID = -1;
    double dfMinX = 0.0;
    double dfMaxY = 0.0;
    int bHasMinX = FALSE;
    int bHasMaxY = FALSE;
    int nExpectedLevel = 0;
    double dfBaseResolution = 0.0;
    while((pszLine = CPLReadLine2L(fp, 4096, NULL)) != NULL)
    {
        const char* pszVal;
        if ((pszVal = GetJSonValue(pszLine, "rows")) != NULL)
            nTileHeight = atoi(pszVal);
        else if ((pszVal = GetJSonValue(pszLine, "cols")) != NULL)
            nTileWidth = atoi(pszVal);
        else if ((pszVal = GetJSonValue(pszLine, "wkid")) != NULL)
        {
            int nVal = atoi(pszVal);
            if (nWKID < 0)
                nWKID = nVal;
            else if (nWKID != nVal)
            {
                CPLDebug("WMS", "Inconsisant WKID values : %d, %d", nVal, nWKID);
                VSIFCloseL(fp);
                return NULL;
            }
        }
        else if ((pszVal = GetJSonValue(pszLine, "x")) != NULL)
        {
            bHasMinX = TRUE;
            dfMinX = CPLAtofM(pszVal);
        }
        else if ((pszVal = GetJSonValue(pszLine, "y")) != NULL)
        {
            bHasMaxY = TRUE;
            dfMaxY = CPLAtofM(pszVal);
        }
        else if ((pszVal = GetJSonValue(pszLine, "level")) != NULL)
        {
            int nLevel = atoi(pszVal);
            if (nLevel != nExpectedLevel)
            {
                CPLDebug("WMS", "Expected level : %d, got : %d", nExpectedLevel, nLevel);
                VSIFCloseL(fp);
                return NULL;
            }

            pszVal = GetJSonValue(pszLine, "resolution");
            if( pszVal == NULL )
            {
                pszLine = CPLReadLine2L(fp, 4096, NULL);
                if( pszLine == NULL )
                    break;
                pszVal = GetJSonValue(pszLine, "resolution");
            }
            if (pszVal != NULL)
            {
                double dfResolution = CPLAtofM(pszVal);
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

    const int nLevelCountOri = nLevelCount;
    while( (double)nTileCountX * nTileWidth * (1 << nLevelCount) > INT_MAX )
        nLevelCount --;
    while( (double)nTileHeight * (1 << nLevelCount) > INT_MAX )
        nLevelCount --;
    if( nLevelCount != nLevelCountOri )
        CPLDebug("WMS", "Had to limit level count to %d instead of %d to stay within GDAL raster size limits",
                 nLevelCount, nLevelCountOri);

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
         STARTS_WITH_CI(pszFilename, "<GDAL_WMS>"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes >= 10 &&
             STARTS_WITH_CI(pabyHeader, "<GDAL_WMS>"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             (STARTS_WITH_CI(pszFilename, "WMS:") ||
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
             STARTS_WITH_CI(pszFilename, "http") &&
             strstr(pszFilename, "/MapServer?f=json") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
              STARTS_WITH_CI(pszFilename, "AGS:"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
              STARTS_WITH_CI(pszFilename, "IIP:"))
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
        STARTS_WITH_CI(pszFilename, "<GDAL_WMS>"))
    {
        config = CPLParseXMLString(pszFilename);
    }
    else if (poOpenInfo->nHeaderBytes >= 10 &&
             STARTS_WITH_CI(pabyHeader, "<GDAL_WMS>"))
    {
        config = CPLParseXMLFile(pszFilename);
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             (STARTS_WITH_CI(pszFilename, "WMS:http") ||
              STARTS_WITH_CI(pszFilename, "http")) &&
             strstr(pszFilename, "/MapServer?f=json") != NULL)
    {
        if (STARTS_WITH_CI(pszFilename, "WMS:http"))
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
             (STARTS_WITH_CI(pszFilename, "WMS:") ||
              CPLString(pszFilename).ifind("SERVICE=WMS") != std::string::npos))
    {
        CPLString osLayers = CPLURLGetValue(pszFilename, "LAYERS");
        CPLString osRequest = CPLURLGetValue(pszFilename, "REQUEST");
        if (!osLayers.empty())
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
    else if (poOpenInfo->nHeaderBytes == 0 &&
              STARTS_WITH_CI(pszFilename, "AGS:"))
    {
        return NULL;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
              STARTS_WITH_CI(pszFilename, "IIP:"))
    {
        CPLString osURL(pszFilename + 4);
        osURL += "&obj=Basic-Info";
        CPLHTTPResult *psResult = CPLHTTPFetch(osURL.c_str(), NULL);
        if (psResult == NULL)
            return NULL;
        if (psResult->pabyData == NULL)
        {
            CPLHTTPDestroyResult(psResult);
            return NULL;
        }
        int nXSize, nYSize;
        const char* pszMaxSize = strstr((const char*)psResult->pabyData, "Max-size:");
        const char* pszResolutionNumber = strstr((const char*)psResult->pabyData, "Resolution-number:");
        if( pszMaxSize &&
            sscanf(pszMaxSize + strlen("Max-size:"), "%d %d", &nXSize, &nYSize) == 2 &&
            pszResolutionNumber )
        {
            int nResolutions = atoi(pszResolutionNumber + strlen("Resolution-number:"));
            char* pszEscapedURL = CPLEscapeString(pszFilename + 4, -1, CPLES_XML);
            CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>"
            "    <Service name=\"IIP\">"
            "        <ServerUrl>%s</ServerUrl>"
            "    </Service>"
            "    <DataWindow>"
            "        <SizeX>%d</SizeX>"
            "        <SizeY>%d</SizeY>"
            "        <TileLevel>%d</TileLevel>"
            "    </DataWindow>"
            "    <BlockSizeX>256</BlockSizeX>"
            "    <BlockSizeY>256</BlockSizeY>"
            "    <BandsCount>3</BandsCount>"
            "    <Cache />"
            "</GDAL_WMS>",
                pszEscapedURL,
                nXSize, nYSize, nResolutions - 1);
            config = CPLParseXMLString(osXML);
            CPLFree(pszEscapedURL);
        }
        CPLHTTPDestroyResult(psResult);
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
                  "The WMS poDriver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    GDALWMSDataset *ds = new GDALWMSDataset();
    ret = ds->Initialize(config, poOpenInfo->papszOpenOptions);
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
/*                             GetServerConfig()                        */
/************************************************************************/

const char *GDALWMSDataset::GetServerConfig(const char *URI, char **papszHTTPOptions)
{
    CPLMutexHolder oHolder(&cfgmtx);

    // Might have it cached already
    if (cfg.end() != cfg.find(URI))
        return cfg.find(URI)->second;

    CPLHTTPResult *psResult = CPLHTTPFetch(URI, papszHTTPOptions);

    if (NULL == psResult)
        return NULL;

    // Capture the result in buffer, get rid of http result
    if ((psResult->nStatus == 0) && (NULL != psResult->pabyData) && ('\0' != psResult->pabyData[0]))
        cfg.insert(make_pair(URI, static_cast<CPLString>(reinterpret_cast<const char *>(psResult->pabyData))));

    CPLHTTPDestroyResult(psResult);

    if (cfg.end() != cfg.find(URI))
        return cfg.find(URI)->second;
    else
        return NULL;
}

// Empties the server configuration cache and removes the mutex
void GDALWMSDataset::ClearConfigCache() {
    // Obviously not thread safe, should only be called when no WMS files are being opened
    cfg.clear();
    DestroyCfgMutex();
}

void GDALWMSDataset::DestroyCfgMutex() {
    if (cfgmtx)
        CPLDestroyMutex(cfgmtx);
    cfgmtx = NULL;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GDALWMSDataset::CreateCopy( const char * pszFilename,
                                         GDALDataset *poSrcDS,
                                         CPL_UNUSED int bStrict,
                                         CPL_UNUSED char ** papszOptions,
                                         CPL_UNUSED GDALProgressFunc pfnProgress,
                                         CPL_UNUSED void * pProgressData )
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

void WMSDeregister(CPL_UNUSED GDALDriver *d) {
    GDALWMSDataset::DestroyCfgMutex();
}

// Define a minidriver factory type, create one and register it
#define RegisterMinidriver(name) \
    class WMSMiniDriverFactory_##name : public WMSMiniDriverFactory { \
    public: \
        WMSMiniDriverFactory_##name() { m_name = CPLString(#name); }\
        virtual ~WMSMiniDriverFactory_##name() {}\
        virtual WMSMiniDriver* New() const override { return new WMSMiniDriver_##name;} \
    }; \
    WMSRegisterMiniDriverFactory(new WMSMiniDriverFactory_##name());

/************************************************************************/
/*                          GDALRegister_WMS()                          */
/************************************************************************/

void GDALRegister_WMS()

{
    if( GDALGetDriverByName( "WMS" ) != NULL )
        return;

    // Register all minidrivers here
    RegisterMinidriver(WMS);
    RegisterMinidriver(TileService);
    RegisterMinidriver(WorldWind);
    RegisterMinidriver(TMS);
    RegisterMinidriver(TiledWMS);
    RegisterMinidriver(VirtualEarth);
    RegisterMinidriver(AGS);
    RegisterMinidriver(IIP);
    RegisterMinidriver(MRF);

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("WMS");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OGC Web Map Service" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_wms.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->pfnOpen = GDALWMSDataset::Open;
    poDriver->pfnIdentify = GDALWMSDataset::Identify;
    poDriver->pfnUnloadDriver = WMSDeregister;
    poDriver->pfnCreateCopy = GDALWMSDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
