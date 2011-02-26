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
    CPLString osBBOX = CPLURLGetValue(pszBaseURL, "BBOX");
    CPLString osFormat = CPLURLGetValue(pszBaseURL, "FORMAT");
    CPLString osTransparent = CPLURLGetValue(pszBaseURL, "TRANSPARENT");
    CPLString osOverviewCount = CPLURLGetValue(pszBaseURL, "OVERVIEWCOUNT");

    CPLString osBaseURL = pszBaseURL;
    /* Remove all keywords to get base URL */
    osBaseURL = CPLURLAddKVP(osBaseURL, "SERVICE", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "VERSION", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "REQUEST", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "LAYERS", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "SRS", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "BBOX", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "FORMAT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "TRANSPARENT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "STYLES", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "WIDTH", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "HEIGHT", NULL);
    osBaseURL = CPLURLAddKVP(osBaseURL, "OVERVIEWCOUNT", NULL);

    if (osVersion.size() == 0)
        osVersion = "1.1.1";
    if (osSRS.size() == 0)
        osSRS = "EPSG:4326";
    if (osBBOX.size() == 0)
        osBBOX = "-180,-90,180,90";

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

    double dfMinX = CPLAtofM(pszMinX);
    double dfMinY = CPLAtofM(pszMinY);
    double dfMaxX = CPLAtofM(pszMaxX);
    double dfMaxY = CPLAtofM(pszMaxY);

    if (dfMaxY <= dfMinY || dfMaxX <= dfMinX)
    {
        CSLDestroy(papszTokens);
        return NULL;
    }

    double dfRatio = (dfMaxX - dfMinX) / (dfMaxY - dfMinY);
    int nXSize, nYSize;
    if (dfRatio > 1)
    {
        nXSize = 1024;
        nYSize = nXSize / dfRatio;
    }
    else
    {
        nYSize = 1024;
        nXSize = nYSize * dfRatio;
    }

    int nOverviewCount = (osOverviewCount.size()) ? atoi(osOverviewCount) : 20;
    if (nOverviewCount < 0 || nOverviewCount > 20)
        nOverviewCount = 20;
    nXSize = nXSize * (1 << nOverviewCount);
    nYSize = nYSize * (1 << nOverviewCount);

    int bTransparent = CSLTestBoolean(osTransparent);

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

    CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>"
            "  <Service name=\"WMS\">"
            "    <Version>%s</Version>"
            "    <ServerURL>%s</ServerUrl>"
            "    <Layers>%s</Layers>"
            "    <SRS>%s</SRS>"
            "    <ImageFormat>%s</ImageFormat>"
            "    <Transparent>%s</Transparent>"
            "  </Service>"
            "  <DataWindow>"
            "    <UpperLeftX>%s</UpperLeftX>"
            "    <UpperLeftY>%s</UpperLeftY>"
            "    <LowerRightX>%s</LowerRightX>"
            "    <LowerRightY>%s</LowerRightY>"
            "    <SizeX>%d</SizeX>"
            "    <SizeY>%d</SizeY>"
            "  </DataWindow>"
            "  <BandsCount>%d</BandsCount>"
            "  <OverviewCount>%d</OverviewCount>"
            "</GDAL_WMS>",
            osVersion.c_str(),
            osBaseURL.c_str(),
            osLayer.c_str(),
            osSRS.c_str(),
            osFormat.c_str(),
            (bTransparent) ? "TRUE" : "FALSE",
            pszMinX, pszMaxY, pszMaxX, pszMinY,
            nXSize, nYSize,
            (bTransparent) ? 4 : 3,
            nOverviewCount);

    CSLDestroy(papszTokens);

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
    if (pszURL == NULL)
        return NULL;
    CPLString osURL = pszURL;
    if (strlen(pszURL) > 10 &&
        strcmp(pszURL + strlen(pszURL) - strlen("1.0.0/"), "1.0.0/") == 0)
        osURL.resize(strlen(pszURL) - strlen("1.0.0/"));
    osURL += "${z}/${x}/${y}.${format}";

    const char* pszSRS = CPLGetXMLValue(psRoot, "SRS", NULL);
    if (pszSRS == NULL)
        return NULL;

    CPLXMLNode* psSRS = CPLGetXMLNode( psRoot, "BoundingBox" );
    if (psSRS == NULL)
        return NULL;

    const char* pszMinX = CPLGetXMLValue(psSRS, "minx", NULL);
    const char* pszMinY = CPLGetXMLValue(psSRS, "miny", NULL);
    const char* pszMaxX = CPLGetXMLValue(psSRS, "maxx", NULL);
    const char* pszMaxY = CPLGetXMLValue(psSRS, "maxy", NULL);
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
            if (nLevelCount == 0)
            {
                const char* pszUnitsPerPixel =
                    CPLGetXMLValue(psIter, "units-per-pixel", NULL);
                if (pszUnitsPerPixel == NULL)
                    return NULL;
                dfPixelSize = CPLAtofM(pszUnitsPerPixel);
            }
            nLevelCount++;
        }
    }

    if (nLevelCount == 0)
        return NULL;

    int nTileCountX = (int)((dfMaxX - dfMinX) / dfPixelSize / nTileWidth + 0.1);
    int nTileCountY = (int)((dfMaxY - dfMinY) / dfPixelSize / nTileHeight + 0.1);

    CPLString osXML = CPLSPrintf(
            "<GDAL_WMS>"
            "  <Service name=\"TMS\">"
            "    <ServerURL>%s</ServerUrl>"
            "    <Format>%s</Format>"
            "  </Service>"
            "  <DataWindow>"
            "    <UpperLeftX>%s</UpperLeftX>"
            "    <UpperLeftY>%s</UpperLeftY>"
            "    <LowerRightX>%s</LowerRightX>"
            "    <LowerRightY>%s</LowerRightY>"
            "    <TileLevel>%d</TileLevel>"
            "    <TileCountX>%d</TileCountX>"
            "    <TileCountY>%d</TileCountY>"
            "  </DataWindow>"
            "  <Projection>%s</Projection>"
            "  <BlockSizeX>%d</BlockSizeX>"
            "  <BlockSizeY>%d</BlockSizeY>"
            "  <BandsCount>%d</BandsCount>"
            "</GDAL_WMS>",
            osURL.c_str(),
            pszTileFormat,
            pszMinX, pszMaxY, pszMaxX, pszMinY,
            nLevelCount - 1,
            nTileCountX, nTileCountY,
            pszSRS,
            nTileWidth, nTileHeight, 3);

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
              strstr(pabyHeader, "<WMS_Capabilities") != NULL))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<WMS_Tile_Service") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMap version=\"1.0.0\" tilemapservice=") != NULL)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMapService version=\"1.0.0\">") != NULL)
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
              strstr(pabyHeader, "<WMS_Capabilities") != NULL))
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
             strstr(pabyHeader, "<TileMap version=\"1.0.0\" tilemapservice=") != NULL)
    {
        CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
        if (psXML == NULL)
            return NULL;
        config = GDALWMSDatasetGetConfigFromTileMap(psXML);
        CPLDestroyXMLNode( psXML );
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMapService version=\"1.0.0\">") != NULL)
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
        driver->pfnOpen = GDALWMSDataset::Open;
        driver->pfnIdentify = GDALWMSDataset::Identify;
        driver->pfnUnloadDriver = GDALDeregister_WMS;
        GetGDALDriverManager()->RegisterDriver(driver);

        GDALWMSMiniDriverManager *const mdm = GetGDALWMSMiniDriverManager();
        mdm->Register(new GDALWMSMiniDriverFactory_WMS());
        mdm->Register(new GDALWMSMiniDriverFactory_TileService());
        mdm->Register(new GDALWMSMiniDriverFactory_WorldWind());
        mdm->Register(new GDALWMSMiniDriverFactory_TMS());
	mdm->Register(new GDALWMSMiniDriverFactory_TiledWMS());
    }
}
