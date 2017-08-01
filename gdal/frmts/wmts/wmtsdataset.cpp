/******************************************************************************
 *
 * Project:  GDAL WMTS driver
 * Purpose:  Implement GDAL WMTS support
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Funded by Land Information New Zealand (LINZ)
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_http.h"
#include "cpl_minixml.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "../vrt/gdal_vrt.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <limits>

extern "C" void GDALRegister_WMTS();

// g++ -g -Wall -fPIC frmts/wmts/wmtsdataset.cpp -shared -o gdal_WMTS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -L. -lgdal

/* Set in stone by WMTS spec. In pixel/meter */
#define WMTS_PITCH                      0.00028

#define WMTS_WGS84_DEG_PER_METER    (180 / M_PI / SRS_WGS84_SEMIMAJOR)

CPL_CVSID("$Id$")

typedef enum
{
    AUTO,
    LAYER_BBOX,
    TILE_MATRIX_SET,
    MOST_PRECISE_TILE_MATRIX
} ExtentMethod;

/************************************************************************/
/* ==================================================================== */
/*                            WMTSTileMatrix                            */
/* ==================================================================== */
/************************************************************************/

class WMTSTileMatrix
{
    public:
        CPLString osIdentifier;
        double    dfScaleDenominator;
        double    dfPixelSize;
        double    dfTLX;
        double    dfTLY;
        int       nTileWidth;
        int       nTileHeight;
        int       nMatrixWidth;
        int       nMatrixHeight;
};

/************************************************************************/
/* ==================================================================== */
/*                          WMTSTileMatrixLimits                        */
/* ==================================================================== */
/************************************************************************/

class WMTSTileMatrixLimits
{
    public:
        CPLString osIdentifier;
        int nMinTileRow;
        int nMaxTileRow;
        int nMinTileCol;
        int nMaxTileCol;
};

/************************************************************************/
/* ==================================================================== */
/*                          WMTSTileMatrixSet                           */
/* ==================================================================== */
/************************************************************************/

class WMTSTileMatrixSet
{
    public:
        OGRSpatialReference         oSRS;
        CPLString                   osSRS;
        bool                        bBoundingBoxValid;
        OGREnvelope                 sBoundingBox; /* expressed in TMS SRS */
        std::vector<WMTSTileMatrix> aoTM;

        WMTSTileMatrixSet() :
            oSRS( OGRSpatialReference() ),
            bBoundingBoxValid(false)
        {
        }
};

/************************************************************************/
/* ==================================================================== */
/*                              WMTSDataset                             */
/* ==================================================================== */
/************************************************************************/

class WMTSDataset : public GDALPamDataset
{
  friend class WMTSBand;

    CPLString                 osLayer;
    CPLString                 osTMS;
    CPLString                 osXML;
    CPLString                 osURLFeatureInfoTemplate;
    WMTSTileMatrixSet         oTMS;

    char                    **papszHTTPOptions;

    std::vector<GDALDataset*> apoDatasets;
    CPLString                 osProjection;
    double                    adfGT[6];

    CPLString                 osLastGetFeatureInfoURL;
    CPLString                 osMetadataItemGetFeatureInfo;

    static char**       BuildHTTPRequestOpts(CPLString osOtherXML);
    static CPLXMLNode*  GetCapabilitiesResponse(const CPLString& osFilename,
                                                char** papszHTTPOptions);
    static CPLString    FixCRSName(const char* pszCRS);
    static CPLString    Replace(const CPLString& osStr, const char* pszOld, const char* pszNew);
    static CPLString    GetOperationKVPURL(CPLXMLNode* psXML,
                                           const char* pszOperation);
    static int          ReadTMS(CPLXMLNode* psContents,
                                const CPLString& osIdentifier,
                                const CPLString& osMaxTileMatrixIdentifier,
                                int nMaxZoomLevel,
                                WMTSTileMatrixSet& oTMS);
    static int          ReadTMLimits(CPLXMLNode* psTMSLimits,
                                     std::map<CPLString, WMTSTileMatrixLimits>& aoMapTileMatrixLimits);

  public:
                 WMTSDataset();
    virtual     ~WMTSDataset();

    virtual CPLErr GetGeoTransform(double* padfGT) override;
    virtual const char* GetProjectionRef() override;
    virtual const char* GetMetadataItem(const char* pszName,
                                        const char* pszDomain) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                         GDALDataset *poSrcDS,
                                         CPL_UNUSED int bStrict,
                                         CPL_UNUSED char ** papszOptions,
                                         CPL_UNUSED GDALProgressFunc pfnProgress,
                                         CPL_UNUSED void * pProgressData );

  protected:
    virtual int         CloseDependentDatasets() override;

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;
};

/************************************************************************/
/* ==================================================================== */
/*                               WMTSBand                               */
/* ==================================================================== */
/************************************************************************/

class WMTSBand : public GDALPamRasterBand
{
  public:
                  WMTSBand(WMTSDataset* poDS, int nBand);

    virtual GDALRasterBand* GetOverview(int nLevel) override;
    virtual int GetOverviewCount() override;
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual const char* GetMetadataItem(const char* pszName,
                                        const char* pszDomain) override;

  protected:
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage) override;
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing,
                              GDALRasterIOExtraArg* psExtraArg ) override;
};

/************************************************************************/
/*                            WMTSBand()                                */
/************************************************************************/

WMTSBand::WMTSBand( WMTSDataset* poDSIn, int nBandIn )
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;
    poDSIn->apoDatasets[0]->GetRasterBand(1)->
        GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr WMTSBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage)
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;
    return poGDS->apoDatasets[0]->GetRasterBand(nBand)->ReadBlock(nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WMTSBand::IRasterIO( GDALRWFlag eRWFlag,
                            int nXOff, int nYOff, int nXSize, int nYSize,
                            void * pData, int nBufXSize, int nBufYSize,
                            GDALDataType eBufType,
                            GSpacing nPixelSpace, GSpacing nLineSpace,
                            GDALRasterIOExtraArg* psExtraArg )
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;

    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && poGDS->apoDatasets.size() > 1 && eRWFlag == GF_Read )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nPixelSpace, nLineSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    return poGDS->apoDatasets[0]->GetRasterBand(nBand)->RasterIO(
                                         eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int WMTSBand::GetOverviewCount()
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;

    if( poGDS->apoDatasets.size() > 1 )
        return (int)poGDS->apoDatasets.size() - 1;
    else
        return 0;
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* WMTSBand::GetOverview(int nLevel)
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;

    if (nLevel < 0 || nLevel >= GetOverviewCount())
        return NULL;

    GDALDataset* poOvrDS = poGDS->apoDatasets[nLevel+1];
    if (poOvrDS)
        return poOvrDS->GetRasterBand(nBand);
    else
        return NULL;
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp WMTSBand::GetColorInterpretation()
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;
    if (poGDS->nBands == 1)
    {
        return GCI_GrayIndex;
    }
    else if (poGDS->nBands == 3 || poGDS->nBands == 4)
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else if (nBand == 3)
            return GCI_BlueBand;
        else if (nBand == 4)
            return GCI_AlphaBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *WMTSBand::GetMetadataItem( const char * pszName,
                                       const char * pszDomain )
{
    WMTSDataset* poGDS = (WMTSDataset*) poDS;

/* ==================================================================== */
/*      LocationInfo handling.                                          */
/* ==================================================================== */
    if( pszDomain != NULL && EQUAL(pszDomain,"LocationInfo") &&
        pszName != NULL && STARTS_WITH_CI(pszName, "Pixel_") &&
        !poGDS->oTMS.aoTM.empty() &&
        !poGDS->osURLFeatureInfoTemplate.empty() )
    {
        int iPixel, iLine;

/* -------------------------------------------------------------------- */
/*      What pixel are we aiming at?                                    */
/* -------------------------------------------------------------------- */
        if( sscanf( pszName+6, "%d_%d", &iPixel, &iLine ) != 2 )
            return NULL;

        const WMTSTileMatrix& oTM = poGDS->oTMS.aoTM.back();

        iPixel += (int)floor(0.5 + (poGDS->adfGT[0] - oTM.dfTLX) / oTM.dfPixelSize);
        iLine += (int)floor(0.5 + (oTM.dfTLY - poGDS->adfGT[3]) / oTM.dfPixelSize);

        CPLString osURL(poGDS->osURLFeatureInfoTemplate);
        osURL = WMTSDataset::Replace(osURL, "{TileMatrixSet}", poGDS->osTMS);
        osURL = WMTSDataset::Replace(osURL, "{TileMatrix}", oTM.osIdentifier);
        osURL = WMTSDataset::Replace(osURL, "{TileCol}",
                                     CPLSPrintf("%d", iPixel / oTM.nTileWidth));
        osURL = WMTSDataset::Replace(osURL, "{TileRow}",
                                     CPLSPrintf("%d", iLine / oTM.nTileHeight));
        osURL = WMTSDataset::Replace(osURL, "{I}",
                                     CPLSPrintf("%d", iPixel % oTM.nTileWidth));
        osURL = WMTSDataset::Replace(osURL, "{J}",
                                     CPLSPrintf("%d", iLine % oTM.nTileHeight));

        if( poGDS->osLastGetFeatureInfoURL.compare(osURL) != 0 )
        {
            poGDS->osLastGetFeatureInfoURL = osURL;
            poGDS->osMetadataItemGetFeatureInfo = "";
            char* pszRes = NULL;
            CPLHTTPResult* psResult = CPLHTTPFetch( osURL, poGDS->papszHTTPOptions);
            if( psResult && psResult->nStatus == 0 && psResult->pabyData )
                pszRes = CPLStrdup((const char*) psResult->pabyData);
            CPLHTTPDestroyResult(psResult);

            if (pszRes)
            {
                poGDS->osMetadataItemGetFeatureInfo = "<LocationInfo>";
                CPLPushErrorHandler(CPLQuietErrorHandler);
                CPLXMLNode* psXML = CPLParseXMLString(pszRes);
                CPLPopErrorHandler();
                if (psXML != NULL && psXML->eType == CXT_Element)
                {
                    if (strcmp(psXML->pszValue, "?xml") == 0)
                    {
                        if (psXML->psNext)
                        {
                            char* pszXML = CPLSerializeXMLTree(psXML->psNext);
                            poGDS->osMetadataItemGetFeatureInfo += pszXML;
                            CPLFree(pszXML);
                        }
                    }
                    else
                    {
                        poGDS->osMetadataItemGetFeatureInfo += pszRes;
                    }
                }
                else
                {
                    char* pszEscapedXML = CPLEscapeString(pszRes, -1, CPLES_XML_BUT_QUOTES);
                    poGDS->osMetadataItemGetFeatureInfo += pszEscapedXML;
                    CPLFree(pszEscapedXML);
                }
                if (psXML != NULL)
                    CPLDestroyXMLNode(psXML);

                poGDS->osMetadataItemGetFeatureInfo += "</LocationInfo>";
                CPLFree(pszRes);
            }
        }
        return poGDS->osMetadataItemGetFeatureInfo.c_str();
    }

    return GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          WMTSDataset()                               */
/************************************************************************/

WMTSDataset::WMTSDataset() :
    papszHTTPOptions(NULL)
{
    adfGT[0] = 0;
    adfGT[1] = 1;
    adfGT[2] = 0;
    adfGT[3] = 0;
    adfGT[4] = 0;
    adfGT[5] = 1;
}

/************************************************************************/
/*                        ~WMTSDataset()                                */
/************************************************************************/

WMTSDataset::~WMTSDataset()
{
    CloseDependentDatasets();
    CSLDestroy(papszHTTPOptions);
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int WMTSDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();
    if( !apoDatasets.empty() )
    {
        for(size_t i=0;i<apoDatasets.size();i++)
            delete apoDatasets[i];
        apoDatasets.resize(0);
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr  WMTSDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && apoDatasets.size() > 1 && eRWFlag == GF_Read )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace,
                                    nBandSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    return apoDatasets[0]->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType, nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr WMTSDataset::GetGeoTransform(double* padfGT)
{
    memcpy(padfGT, adfGT, 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* WMTSDataset::GetProjectionRef()
{
    return osProjection.c_str();
}

/************************************************************************/
/*                          WMTSEscapeXML()                             */
/************************************************************************/

static CPLString WMTSEscapeXML(const char* pszUnescapedXML)
{
    CPLString osRet;
    char* pszTmp = CPLEscapeString(pszUnescapedXML, -1, CPLES_XML);
    osRet = pszTmp;
    CPLFree(pszTmp);
    return osRet;
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char* WMTSDataset::GetMetadataItem(const char* pszName,
                                         const char* pszDomain)
{
    if( pszName != NULL && EQUAL(pszName, "XML") &&
        pszDomain != NULL && EQUAL(pszDomain, "WMTS") )
    {
        return osXML.c_str();
    }

    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int WMTSDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "WMTS:") )
        return TRUE;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "<GDAL_WMTS") )
        return TRUE;

    if( poOpenInfo->nHeaderBytes == 0 )
        return FALSE;

    if( strstr((const char*)poOpenInfo->pabyHeader, "<GDAL_WMTS") )
        return TRUE;

    return (strstr((const char*)poOpenInfo->pabyHeader,
                  "<Capabilities") != NULL ||
            strstr((const char*)poOpenInfo->pabyHeader,
                  "<wmts:Capabilities") != NULL) &&
            strstr((const char*)poOpenInfo->pabyHeader,
                    "http://www.opengis.net/wmts/1.0") != NULL;
}

/************************************************************************/
/*                          QuoteIfNecessary()                          */
/************************************************************************/

static CPLString QuoteIfNecessary(const char* pszVal)
{
    if( strchr(pszVal, ' ') || strchr(pszVal, ',') || strchr(pszVal, '=') )
    {
        CPLString osVal;
        osVal += "\"";
        osVal += pszVal;
        osVal += "\"";
        return osVal;
    }
    else
        return pszVal;
}

/************************************************************************/
/*                             FixCRSName()                             */
/************************************************************************/

CPLString WMTSDataset::FixCRSName(const char* pszCRS)
{
    while( *pszCRS == ' ' || *pszCRS == '\r' || *pszCRS == '\n' )
        pszCRS ++;

    /* http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml uses urn:ogc:def:crs:EPSG:6.18:3:3857 */
    /* instead of urn:ogc:def:crs:EPSG:6.18.3:3857. Coming from an incorrect example of URN in WMTS spec */
    /* https://portal.opengeospatial.org/files/?artifact_id=50398 */
    if( STARTS_WITH_CI(pszCRS, "urn:ogc:def:crs:EPSG:6.18:3:") )    {
        return CPLSPrintf("urn:ogc:def:crs:EPSG::%s",
                          pszCRS + strlen("urn:ogc:def:crs:EPSG:6.18:3:"));
    }

    if( EQUAL(pszCRS, "urn:ogc:def:crs:EPSG::102100") )
        return "EPSG:3857";

    CPLString osRet(pszCRS);
    while( osRet.size() &&
           (osRet.back() == ' ' || osRet.back() == '\r' || osRet.back() == '\n') )
    {
        osRet.resize(osRet.size() - 1);
    }
    return osRet;
}

/************************************************************************/
/*                              ReadTMS()                               */
/************************************************************************/

int WMTSDataset::ReadTMS(CPLXMLNode* psContents,
                         const CPLString& osIdentifier,
                         const CPLString& osMaxTileMatrixIdentifier,
                         int nMaxZoomLevel,
                         WMTSTileMatrixSet& oTMS)
{
    for(CPLXMLNode* psIter = psContents->psChild; psIter != NULL; psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element || strcmp(psIter->pszValue, "TileMatrixSet") != 0 )
            continue;
        const char* pszIdentifier = CPLGetXMLValue(psIter, "Identifier", "");
        if( !EQUAL(osIdentifier, pszIdentifier) )
            continue;
        const char* pszSupportedCRS = CPLGetXMLValue(psIter, "SupportedCRS", NULL);
        if( pszSupportedCRS == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing SupportedCRS");
            return FALSE;
        }
        oTMS.osSRS = pszSupportedCRS;
        if( oTMS.oSRS.SetFromUserInput(FixCRSName(pszSupportedCRS)) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse CRS '%s'",
                     pszSupportedCRS);
            return FALSE;
        }
        int bSwap = oTMS.oSRS.EPSGTreatsAsLatLong() || oTMS.oSRS.EPSGTreatsAsNorthingEasting();
        CPLXMLNode* psBB = CPLGetXMLNode(psIter, "BoundingBox");
        oTMS.bBoundingBoxValid = false;
        if( psBB != NULL )
        {
            CPLString osCRS = CPLGetXMLValue(psBB, "crs", "");
            if( EQUAL(osCRS, "") || EQUAL(osCRS, pszSupportedCRS) )
            {
                CPLString osLowerCorner = CPLGetXMLValue(psBB, "LowerCorner", "");
                CPLString osUpperCorner = CPLGetXMLValue(psBB, "UpperCorner", "");
                if( !osLowerCorner.empty() && !osUpperCorner.empty() )
                {
                    char** papszLC = CSLTokenizeString(osLowerCorner);
                    char** papszUC = CSLTokenizeString(osUpperCorner);
                    if( CSLCount(papszLC) == 2 && CSLCount(papszUC) == 2 )
                    {
                        oTMS.sBoundingBox.MinX = CPLAtof(papszLC[(bSwap)? 1 : 0]);
                        oTMS.sBoundingBox.MinY = CPLAtof(papszLC[(bSwap)? 0 : 1]);
                        oTMS.sBoundingBox.MaxX = CPLAtof(papszUC[(bSwap)? 1 : 0]);
                        oTMS.sBoundingBox.MaxY = CPLAtof(papszUC[(bSwap)? 0 : 1]);
                        oTMS.bBoundingBoxValid = true;
                    }
                    CSLDestroy(papszLC);
                    CSLDestroy(papszUC);
                }
            }
        }
        else
        {
            const char* pszWellKnownScaleSet = CPLGetXMLValue(psIter, "WellKnownScaleSet", "");
            if( EQUAL(pszIdentifier, "GoogleCRS84Quad") ||
                EQUAL(pszWellKnownScaleSet, "urn:ogc:def:wkss:OGC:1.0:GoogleCRS84Quad") ||
                EQUAL(pszIdentifier, "GlobalCRS84Scale") ||
                EQUAL(pszWellKnownScaleSet, "urn:ogc:def:wkss:OGC:1.0:GlobalCRS84Scale") )
            {
                oTMS.sBoundingBox.MinX = -180;
                oTMS.sBoundingBox.MinY = -90;
                oTMS.sBoundingBox.MaxX = 180;
                oTMS.sBoundingBox.MaxY = 90;
                oTMS.bBoundingBoxValid = true;
            }
        }

        bool bFoundTileMatrix = false;
        for(CPLXMLNode* psSubIter = psIter->psChild; psSubIter != NULL; psSubIter = psSubIter->psNext )
        {
            if( psSubIter->eType != CXT_Element || strcmp(psSubIter->pszValue, "TileMatrix") != 0 )
                continue;
            const char* l_pszIdentifier = CPLGetXMLValue(psSubIter, "Identifier", NULL);
            const char* pszScaleDenominator = CPLGetXMLValue(psSubIter, "ScaleDenominator", NULL);
            const char* pszTopLeftCorner = CPLGetXMLValue(psSubIter, "TopLeftCorner", NULL);
            const char* pszTileWidth = CPLGetXMLValue(psSubIter, "TileWidth", NULL);
            const char* pszTileHeight = CPLGetXMLValue(psSubIter, "TileHeight", NULL);
            const char* pszMatrixWidth = CPLGetXMLValue(psSubIter, "MatrixWidth", NULL);
            const char* pszMatrixHeight = CPLGetXMLValue(psSubIter, "MatrixHeight", NULL);
            if( l_pszIdentifier == NULL || pszScaleDenominator == NULL ||
                pszTopLeftCorner == NULL || strchr(pszTopLeftCorner, ' ') == NULL ||
                pszTileWidth == NULL || pszTileHeight == NULL ||
                pszMatrixWidth == NULL || pszMatrixHeight == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing required element in TileMatrix element");
                return FALSE;
            }
            WMTSTileMatrix oTM;
            oTM.osIdentifier = l_pszIdentifier;
            oTM.dfScaleDenominator = CPLAtof(pszScaleDenominator);
            oTM.dfPixelSize = oTM.dfScaleDenominator * WMTS_PITCH;
            if( oTM.dfPixelSize <= 0.0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid ScaleDenominator");
                return FALSE;
            }
            if( oTMS.oSRS.IsGeographic() )
                oTM.dfPixelSize *= WMTS_WGS84_DEG_PER_METER;
            double dfVal1 = CPLAtof(pszTopLeftCorner);
            double dfVal2 = CPLAtof(strchr(pszTopLeftCorner, ' ')+1);
            if( !bSwap ||
                /* Hack for http://osm.geobretagne.fr/gwc01/service/wmts?request=getcapabilities */
                ( STARTS_WITH_CI(l_pszIdentifier, "EPSG:4326:") &&
                  dfVal1 == -180.0 ) )
            {
                oTM.dfTLX = dfVal1;
                oTM.dfTLY = dfVal2;
            }
            else
            {
                oTM.dfTLX = dfVal2;
                oTM.dfTLY = dfVal1;
            }
            oTM.nTileWidth = atoi(pszTileWidth);
            oTM.nTileHeight = atoi(pszTileHeight);
            if( oTM.nTileWidth <= 0 || oTM.nTileWidth > 4096 ||
                oTM.nTileHeight <= 0 || oTM.nTileHeight > 4096 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid TileWidth/TileHeight element");
                return FALSE;
            }
            oTM.nMatrixWidth = atoi(pszMatrixWidth);
            oTM.nMatrixHeight = atoi(pszMatrixHeight);
            // http://datacarto.geonormandie.fr/mapcache/wmts?SERVICE=WMTS&REQUEST=GetCapabilities
            // has a TileMatrix 0 with MatrixWidth = MatrixHeight = 0
            if( oTM.nMatrixWidth < 1 || oTM.nMatrixHeight < 1 )
                continue;
            oTMS.aoTM.push_back(oTM);
            if( (nMaxZoomLevel >= 0 && static_cast<int>(oTMS.aoTM.size())-1
                                                        == nMaxZoomLevel) ||
                (!osMaxTileMatrixIdentifier.empty() &&
                 EQUAL(osMaxTileMatrixIdentifier, l_pszIdentifier)) )
            {
                bFoundTileMatrix = true;
                break;
            }
        }
        if( nMaxZoomLevel >= 0 && !bFoundTileMatrix )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find TileMatrix of zoom level %d in TileMatrixSet '%s'",
                     nMaxZoomLevel,
                     osIdentifier.c_str());
            return FALSE;
        }
        if( !osMaxTileMatrixIdentifier.empty() && !bFoundTileMatrix )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find TileMatrix '%s' in TileMatrixSet '%s'",
                     osMaxTileMatrixIdentifier.c_str(),
                     osIdentifier.c_str());
            return FALSE;
        }
        if( oTMS.aoTM.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find TileMatrix in TileMatrixSet '%s'",
                     osIdentifier.c_str());
            return FALSE;
        }
        return TRUE;
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find TileMatrixSet '%s'",
             osIdentifier.c_str());
    return FALSE;
}

/************************************************************************/
/*                              ReadTMLimits()                          */
/************************************************************************/

int WMTSDataset::ReadTMLimits(CPLXMLNode* psTMSLimits,
                              std::map<CPLString, WMTSTileMatrixLimits>& aoMapTileMatrixLimits)
{
    for(CPLXMLNode* psIter = psTMSLimits->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType != CXT_Element || strcmp(psIter->pszValue, "TileMatrixLimits") != 0 )
            continue;
        WMTSTileMatrixLimits oTMLimits;
        const char* pszTileMatrix = CPLGetXMLValue(psIter, "TileMatrix", NULL);
        const char* pszMinTileRow = CPLGetXMLValue(psIter, "MinTileRow", NULL);
        const char* pszMaxTileRow = CPLGetXMLValue(psIter, "MaxTileRow", NULL);
        const char* pszMinTileCol = CPLGetXMLValue(psIter, "MinTileCol", NULL);
        const char* pszMaxTileCol = CPLGetXMLValue(psIter, "MaxTileCol", NULL);
        if( pszTileMatrix == NULL ||
            pszMinTileRow == NULL || pszMaxTileRow == NULL ||
            pszMinTileCol == NULL || pszMaxTileCol == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing required element in TileMatrixLimits element");
            return FALSE;
        }
        oTMLimits.osIdentifier = pszTileMatrix;
        oTMLimits.nMinTileRow = atoi(pszMinTileRow);
        oTMLimits.nMaxTileRow = atoi(pszMaxTileRow);
        oTMLimits.nMinTileCol = atoi(pszMinTileCol);
        oTMLimits.nMaxTileCol = atoi(pszMaxTileCol);
        aoMapTileMatrixLimits[pszTileMatrix] = oTMLimits;
    }
    return TRUE;
}

/************************************************************************/
/*                               Replace()                              */
/************************************************************************/

CPLString WMTSDataset::Replace(const CPLString& osStr, const char* pszOld,
                               const char* pszNew)
{
    size_t nPos = osStr.ifind(pszOld);
    if( nPos == std::string::npos )
        return osStr;
    CPLString osRet(osStr.substr(0, nPos));
    osRet += pszNew;
    osRet += osStr.substr(nPos + strlen(pszOld));
    return osRet;
}

/************************************************************************/
/*                       GetCapabilitiesResponse()                      */
/************************************************************************/

CPLXMLNode* WMTSDataset::GetCapabilitiesResponse(const CPLString& osFilename,
                                                 char** papszHTTPOptions)
{
    CPLXMLNode* psXML;
    VSIStatBufL sStat;
    if( VSIStatL(osFilename, &sStat) == 0 )
        psXML = CPLParseXMLFile(osFilename);
    else
    {
        CPLHTTPResult* psResult = CPLHTTPFetch(osFilename, papszHTTPOptions);
        if( psResult == NULL )
            return NULL;
        if( psResult->pabyData == NULL )
        {
            CPLHTTPDestroyResult(psResult);
            return NULL;
        }
        psXML = CPLParseXMLString((const char*)psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
    }
    return psXML;
}

/************************************************************************/
/*                          WMTSAddOtherXML()                           */
/************************************************************************/

static void WMTSAddOtherXML(CPLXMLNode* psRoot, const char* pszElement,
                            CPLString& osOtherXML)
{
    CPLXMLNode* psElement = CPLGetXMLNode(psRoot, pszElement);
    if( psElement )
    {
        CPLXMLNode* psNext = psElement->psNext;
        psElement->psNext = NULL;
        char* pszTmp = CPLSerializeXMLTree(psElement);
        osOtherXML += pszTmp;
        CPLFree(pszTmp);
        psElement->psNext = psNext;
    }
}

/************************************************************************/
/*                          GetOperationKVPURL()                        */
/************************************************************************/

CPLString WMTSDataset::GetOperationKVPURL(CPLXMLNode* psXML,
                                          const char* pszOperation)
{
    CPLString osRet;
    CPLXMLNode* psOM = CPLGetXMLNode(psXML, "=Capabilities.OperationsMetadata");
    for(CPLXMLNode* psIter = psOM ? psOM->psChild : NULL;
        psIter != NULL; psIter = psIter->psNext)
    {
        if( psIter->eType != CXT_Element ||
            strcmp(psIter->pszValue, "Operation") != 0 ||
            !EQUAL(CPLGetXMLValue(psIter, "name", ""), pszOperation) )
        {
            continue;
        }
        CPLXMLNode* psHTTP = CPLGetXMLNode(psIter, "DCP.HTTP");
        for(CPLXMLNode* psGet = psHTTP ? psHTTP->psChild : NULL;
                psGet != NULL; psGet = psGet->psNext)
        {
            if( psGet->eType != CXT_Element ||
                strcmp(psGet->pszValue, "Get") != 0 )
            {
                continue;
            }
            if( !EQUAL(CPLGetXMLValue(psGet, "Constraint.AllowedValues.Value", "KVP"), "KVP") )
                continue;
            osRet = CPLGetXMLValue(psGet, "href", "");
        }
    }
    return osRet;
}

/************************************************************************/
/*                           BuildHTTPRequestOpts()                     */
/************************************************************************/

char** WMTSDataset::BuildHTTPRequestOpts(CPLString osOtherXML)
{
    osOtherXML = "<Root>" + osOtherXML + "</Root>";
    CPLXMLNode* psXML = CPLParseXMLString(osOtherXML);
    char **http_request_opts = NULL;
    if (CPLGetXMLValue(psXML, "Timeout", NULL)) {
        CPLString optstr;
        optstr.Printf("TIMEOUT=%s", CPLGetXMLValue(psXML, "Timeout", NULL));
        http_request_opts = CSLAddString(http_request_opts, optstr.c_str());
    }
    if (CPLGetXMLValue(psXML, "UserAgent", NULL)) {
        CPLString optstr;
        optstr.Printf("USERAGENT=%s", CPLGetXMLValue(psXML, "UserAgent", NULL));
        http_request_opts = CSLAddString(http_request_opts, optstr.c_str());
    }
    if (CPLGetXMLValue(psXML, "Referer", NULL)) {
        CPLString optstr;
        optstr.Printf("REFERER=%s", CPLGetXMLValue(psXML, "Referer", NULL));
        http_request_opts = CSLAddString(http_request_opts, optstr.c_str());
    }
    if (CPLTestBool(CPLGetXMLValue(psXML, "UnsafeSSL", "false"))) {
        http_request_opts = CSLAddString(http_request_opts, "UNSAFESSL=1");
    }
    if (CPLGetXMLValue(psXML, "UserPwd", NULL)) {
        CPLString optstr;
        optstr.Printf("USERPWD=%s", CPLGetXMLValue(psXML, "UserPwd", NULL));
        http_request_opts = CSLAddString(http_request_opts, optstr.c_str());
    }
    CPLDestroyXMLNode(psXML);
    return http_request_opts;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* WMTSDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return NULL;

    CPLXMLNode* psXML = NULL;
    CPLString osTileFormat;
    CPLString osInfoFormat;

    CPLString osGetCapabilitiesURL = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                "URL", "");
    CPLString osLayer = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                    "LAYER", "");
    CPLString osTMS = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                    "TILEMATRIXSET", "");
    CPLString osMaxTileMatrixIdentifier = CSLFetchNameValueDef(
                                    poOpenInfo->papszOpenOptions,
                                    "TILEMATRIX", "");
    int nUserMaxZoomLevel = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                    "ZOOM_LEVEL",
                                    CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                    "ZOOMLEVEL", "-1")));
    CPLString osStyle = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                    "STYLE", "");

    int bExtendBeyondDateLine =
        CPLFetchBool(poOpenInfo->papszOpenOptions,
                     "EXTENDBEYONDDATELINE", false);

    CPLString osOtherXML = "<Cache />"
                     "<UnsafeSSL>true</UnsafeSSL>"
                     "<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>"
                     "<ZeroBlockOnServerException>true</ZeroBlockOnServerException>";

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "WMTS:") )
    {
        char** papszTokens = CSLTokenizeString2( poOpenInfo->pszFilename + 5,
                                                 ",", CSLT_HONOURSTRINGS );
        if( papszTokens && papszTokens[0] )
        {
            osGetCapabilitiesURL = papszTokens[0];
            for(char** papszIter = papszTokens+1; *papszIter; papszIter++)
            {
                char* pszKey = NULL;
                const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "layer") )
                        osLayer = pszValue;
                    else if( EQUAL(pszKey, "tilematrixset") )
                        osTMS = pszValue;
                    else if( EQUAL(pszKey, "tilematrix") )
                        osMaxTileMatrixIdentifier = pszValue;
                    else if( EQUAL(pszKey, "zoom_level") ||
                             EQUAL(pszKey, "zoomlevel") )
                        nUserMaxZoomLevel = atoi(pszValue);
                    else if( EQUAL(pszKey, "style") )
                        osStyle = pszValue;
                    else if( EQUAL(pszKey, "extendbeyonddateline") )
                        bExtendBeyondDateLine = CPLTestBool(pszValue);
                    else
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unknown parameter: %s'", pszKey);
                }
                CPLFree(pszKey);
            }
        }
        CSLDestroy(papszTokens);

        char** papszHTTPOptions = BuildHTTPRequestOpts(osOtherXML);
        psXML = GetCapabilitiesResponse(osGetCapabilitiesURL, papszHTTPOptions);
        CSLDestroy(papszHTTPOptions);
    }

    int bHasAOI = FALSE;
    OGREnvelope sAOI;
    int nBands = 4;
    CPLString osProjection;

    if( (psXML != NULL && CPLGetXMLNode(psXML, "=GDAL_WMTS") != NULL ) ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "<GDAL_WMTS") ||
        (poOpenInfo->nHeaderBytes > 0 &&
         strstr((const char*)poOpenInfo->pabyHeader, "<GDAL_WMTS")) )
    {
        CPLXMLNode* psGDALWMTS;
        if( psXML != NULL && CPLGetXMLNode(psXML, "=GDAL_WMTS") != NULL )
            psGDALWMTS = CPLCloneXMLTree(psXML);
        else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "<GDAL_WMTS") )
            psGDALWMTS = CPLParseXMLString(poOpenInfo->pszFilename);
        else
            psGDALWMTS = CPLParseXMLFile(poOpenInfo->pszFilename);
        if( psGDALWMTS == NULL )
            return NULL;
        CPLXMLNode* psRoot = CPLGetXMLNode(psGDALWMTS, "=GDAL_WMTS");
        if( psRoot == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find root <GDAL_WMTS>");
            CPLDestroyXMLNode(psGDALWMTS);
            return NULL;
        }
        osGetCapabilitiesURL = CPLGetXMLValue(psRoot, "GetCapabilitiesUrl", "");
        if( osGetCapabilitiesURL.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing <GetCapabilitiesUrl>");
            CPLDestroyXMLNode(psGDALWMTS);
            return NULL;
        }

        osLayer = CPLGetXMLValue(psRoot, "Layer", osLayer);
        osTMS = CPLGetXMLValue(psRoot, "TileMatrixSet", osTMS);
        osMaxTileMatrixIdentifier = CPLGetXMLValue(psRoot, "TileMatrix",
                                                   osMaxTileMatrixIdentifier);
        nUserMaxZoomLevel = atoi(CPLGetXMLValue(psRoot, "ZoomLevel",
                                       CPLSPrintf("%d", nUserMaxZoomLevel)));
        osStyle = CPLGetXMLValue(psRoot, "Style", osStyle);
        osTileFormat = CPLGetXMLValue(psRoot, "Format", osTileFormat);
        osInfoFormat = CPLGetXMLValue(psRoot, "InfoFormat", osInfoFormat);
        osProjection = CPLGetXMLValue(psRoot, "Projection", osProjection);
        bExtendBeyondDateLine = CPLTestBool(CPLGetXMLValue(psRoot, "ExtendBeyondDateLine",
                                            (bExtendBeyondDateLine) ? "true": "false"));

        osOtherXML = "";
        WMTSAddOtherXML(psRoot, "Cache", osOtherXML);
        WMTSAddOtherXML(psRoot, "MaxConnections", osOtherXML);
        WMTSAddOtherXML(psRoot, "Timeout", osOtherXML);
        WMTSAddOtherXML(psRoot, "OfflineMode", osOtherXML);
        WMTSAddOtherXML(psRoot, "MaxConnections", osOtherXML);
        WMTSAddOtherXML(psRoot, "UserAgent", osOtherXML);
        WMTSAddOtherXML(psRoot, "UserPwd", osOtherXML);
        WMTSAddOtherXML(psRoot, "UnsafeSSL", osOtherXML);
        WMTSAddOtherXML(psRoot, "Referer", osOtherXML);
        WMTSAddOtherXML(psRoot, "ZeroBlockHttpCodes", osOtherXML);
        WMTSAddOtherXML(psRoot, "ZeroBlockOnServerException", osOtherXML);

        nBands = atoi(CPLGetXMLValue(psRoot, "BandsCount", "4"));

        const char* pszULX = CPLGetXMLValue(psRoot, "DataWindow.UpperLeftX", NULL);
        const char* pszULY = CPLGetXMLValue(psRoot, "DataWindow.UpperLeftY", NULL);
        const char* pszLRX = CPLGetXMLValue(psRoot, "DataWindow.LowerRightX", NULL);
        const char* pszLRY = CPLGetXMLValue(psRoot, "DataWindow.LowerRightY", NULL);
        if( pszULX && pszULY && pszLRX && pszLRY )
        {
            sAOI.MinX = CPLAtof(pszULX);
            sAOI.MaxY = CPLAtof(pszULY);
            sAOI.MaxX = CPLAtof(pszLRX);
            sAOI.MinY = CPLAtof(pszLRY);
            bHasAOI = TRUE;
        }

        CPLDestroyXMLNode(psGDALWMTS);

        CPLDestroyXMLNode(psXML);
        char** papszHTTPOptions = BuildHTTPRequestOpts(osOtherXML);
        psXML = GetCapabilitiesResponse(osGetCapabilitiesURL, papszHTTPOptions);
        CSLDestroy(papszHTTPOptions);
    }
    else if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "WMTS:") )
    {
        osGetCapabilitiesURL = poOpenInfo->pszFilename;
        psXML = CPLParseXMLFile(poOpenInfo->pszFilename);
    }
    if( psXML == NULL )
        return NULL;
    CPLStripXMLNamespace(psXML, NULL, TRUE);

    CPLXMLNode* psContents = CPLGetXMLNode(psXML, "=Capabilities.Contents");
    if( psContents == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing Capabilities.Contents element");
        CPLDestroyXMLNode(psXML);
        return NULL;
    }

    if( STARTS_WITH(osGetCapabilitiesURL, "/vsimem/") )
    {
        const char* pszHref = CPLGetXMLValue(psXML,
                            "=Capabilities.ServiceMetadataURL.href", NULL);
        if( pszHref )
            osGetCapabilitiesURL = pszHref;
        else
        {
            osGetCapabilitiesURL = GetOperationKVPURL(psXML, "GetCapabilities");
            if( !osGetCapabilitiesURL.empty() )
            {
                osGetCapabilitiesURL = CPLURLAddKVP(osGetCapabilitiesURL, "service", "WMTS");
                osGetCapabilitiesURL = CPLURLAddKVP(osGetCapabilitiesURL, "request", "GetCapabilities");
            }
        }
    }
    CPLString osCapabilitiesFilename(osGetCapabilitiesURL);
    if( !STARTS_WITH_CI(osCapabilitiesFilename, "WMTS:") )
        osCapabilitiesFilename = "WMTS:" + osGetCapabilitiesURL;

    int nLayerCount = 0;
    CPLStringList aosSubDatasets;
    CPLString osSelectLayer(osLayer), osSelectTMS(osTMS), osSelectStyle(osStyle);
    CPLString osSelectLayerTitle, osSelectLayerAbstract;
    CPLString osSelectTileFormat(osTileFormat), osSelectInfoFormat(osInfoFormat);
    int nCountTileFormat = 0;
    int nCountInfoFormat = 0;
    CPLString osURLTileTemplate;
    CPLString osURLFeatureInfoTemplate;
    std::set<CPLString> aoSetLayers;
    std::map<CPLString, OGREnvelope> aoMapBoundingBox;
    std::map<CPLString, WMTSTileMatrixLimits> aoMapTileMatrixLimits;
    std::map<CPLString, CPLString> aoMapDimensions;

    for(CPLXMLNode* psIter = psContents->psChild; psIter != NULL; psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element || strcmp(psIter->pszValue, "Layer") != 0 )
            continue;
        const char* pszIdentifier = CPLGetXMLValue(psIter, "Identifier", "");
        if( aoSetLayers.find(pszIdentifier) != aoSetLayers.end() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Several layers with identifier '%s'. Only first one kept",
                     pszIdentifier);
        }
        aoSetLayers.insert(pszIdentifier);
        if( !osLayer.empty() && strcmp(osLayer, pszIdentifier) != 0 )
            continue;
        const char* pszTitle = CPLGetXMLValue(psIter, "Title", NULL);
        if( osSelectLayer.empty() )
        {
            osSelectLayer = pszIdentifier;
        }
        if( strcmp(osSelectLayer, pszIdentifier) == 0 )
        {
            if( pszTitle != NULL )
                osSelectLayerTitle = pszTitle;
            const char* pszAbstract = CPLGetXMLValue(psIter, "Abstract", NULL);
            if( pszAbstract != NULL )
                osSelectLayerAbstract = pszAbstract;
        }

        std::vector<CPLString> aosTMS;
        std::vector<CPLString> aosStylesIdentifier;
        std::vector<CPLString> aosStylesTitle;

        CPLXMLNode* psSubIter = psIter->psChild;
        for(; psSubIter != NULL; psSubIter = psSubIter->psNext )
        {
            if( psSubIter->eType != CXT_Element )
                continue;
            if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                strcmp(psSubIter->pszValue, "Format") == 0 )
            {
                const char* pszValue = CPLGetXMLValue(psSubIter, "", "");
                if( !osTileFormat.empty() && strcmp(osTileFormat, pszValue) != 0 )
                    continue;
                nCountTileFormat ++;
                if( osSelectTileFormat.empty() ||
                    EQUAL(pszValue, "image/png") )
                {
                    osSelectTileFormat = pszValue;
                }
            }
            else if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                     strcmp(psSubIter->pszValue, "InfoFormat") == 0 )
            {
                const char* pszValue = CPLGetXMLValue(psSubIter, "", "");
                if( !osInfoFormat.empty() && strcmp(osInfoFormat, pszValue) != 0 )
                    continue;
                nCountInfoFormat ++;
                if( osSelectInfoFormat.empty() ||
                    (EQUAL(pszValue, "application/vnd.ogc.gml") &&
                     !EQUAL(osSelectInfoFormat, "application/vnd.ogc.gml/3.1.1")) ||
                    EQUAL(pszValue, "application/vnd.ogc.gml/3.1.1") )
                {
                    osSelectInfoFormat = pszValue;
                }
            }
            else if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                     strcmp(psSubIter->pszValue, "Dimension") == 0 )
            {
                /* Cf http://wmts.geo.admin.ch/1.0.0/WMTSCapabilities.xml */
                const char* pszDimensionIdentifier = CPLGetXMLValue(psSubIter, "Identifier", NULL);
                const char* pszDefault = CPLGetXMLValue(psSubIter, "Default", "");
                if( pszDimensionIdentifier != NULL )
                    aoMapDimensions[pszDimensionIdentifier] = pszDefault;
            }
            else if( strcmp(psSubIter->pszValue, "TileMatrixSetLink") == 0 )
            {
                const char* pszTMS = CPLGetXMLValue(
                                            psSubIter, "TileMatrixSet", "");
                if( !osTMS.empty() && strcmp(osTMS, pszTMS) != 0 )
                    continue;
                if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                    osSelectTMS.empty() )
                {
                    osSelectTMS = pszTMS;
                }
                if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                    strcmp(osSelectTMS, pszTMS) == 0 )
                {
                    CPLXMLNode* psTMSLimits = CPLGetXMLNode(
                                        psSubIter, "TileMatrixSetLimits");
                    if( psTMSLimits )
                        ReadTMLimits(psTMSLimits, aoMapTileMatrixLimits);
                }
                aosTMS.push_back(pszTMS);
            }
            else if( strcmp(psSubIter->pszValue, "Style") == 0 )
            {
                int bIsDefault = CPLTestBool(CPLGetXMLValue(
                                            psSubIter, "isDefault", "false"));
                const char* l_pszIdentifier = CPLGetXMLValue(
                                            psSubIter, "Identifier", "");
                if( !osStyle.empty() && strcmp(osStyle, l_pszIdentifier) != 0 )
                    continue;
                const char* pszStyleTitle = CPLGetXMLValue(
                                        psSubIter, "Title", l_pszIdentifier);
                if( bIsDefault )
                {
                    aosStylesIdentifier.insert(aosStylesIdentifier.begin(),
                                                CPLString(l_pszIdentifier));
                    aosStylesTitle.insert(aosStylesTitle.begin(),
                                            CPLString(pszStyleTitle));
                    if( strcmp(osSelectLayer, l_pszIdentifier) == 0 &&
                        osSelectStyle.empty() )
                    {
                        osSelectStyle = l_pszIdentifier;
                    }
                }
                else
                {
                    aosStylesIdentifier.push_back(l_pszIdentifier);
                    aosStylesTitle.push_back(pszStyleTitle);
                }
            }
            else if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                     (strcmp(psSubIter->pszValue, "BoundingBox") == 0 ||
                      strcmp(psSubIter->pszValue, "WGS84BoundingBox") == 0) )
            {
                CPLString osCRS = CPLGetXMLValue(psSubIter, "crs", "");
                if( osCRS.empty() )
                {
                    if( strcmp(psSubIter->pszValue, "WGS84BoundingBox") == 0 )
                    {
                        osCRS = "EPSG:4326";
                    }
                    else
                    {
                        int nCountTileMatrixSet = 0;
                        CPLString osSingleTileMatrixSet;
                        for(CPLXMLNode* psIter3 = psContents->psChild; psIter3 != NULL; psIter3 = psIter3->psNext )
                        {
                            if( psIter3->eType != CXT_Element || strcmp(psIter3->pszValue, "TileMatrixSet") != 0 )
                                continue;
                            nCountTileMatrixSet ++;
                            if( nCountTileMatrixSet == 1 )
                                osSingleTileMatrixSet = CPLGetXMLValue(psIter3, "Identifier", "");
                        }
                        if( nCountTileMatrixSet == 1 )
                        {
                            // For 13-082_WMTS_Simple_Profile/schemas/wmts/1.0/profiles/WMTSSimple/examples/wmtsGetCapabilities_response_OSM.xml
                            WMTSTileMatrixSet oTMS;
                            if( ReadTMS(psContents, osSingleTileMatrixSet,
                                        CPLString(), -1, oTMS) )
                            {
                                osCRS = oTMS.osSRS;
                            }
                        }
                    }
                }
                CPLString osLowerCorner = CPLGetXMLValue(psSubIter, "LowerCorner", "");
                CPLString osUpperCorner = CPLGetXMLValue(psSubIter, "UpperCorner", "");
                OGRSpatialReference oSRS;
                if( !osCRS.empty() && !osLowerCorner.empty() && !osUpperCorner.empty() &&
                    oSRS.SetFromUserInput(FixCRSName(osCRS)) == OGRERR_NONE )
                {
                    int bSwap = oSRS.EPSGTreatsAsLatLong() ||
                                oSRS.EPSGTreatsAsNorthingEasting();
                    char** papszLC = CSLTokenizeString(osLowerCorner);
                    char** papszUC = CSLTokenizeString(osUpperCorner);
                    if( CSLCount(papszLC) == 2 && CSLCount(papszUC) == 2 )
                    {
                        OGREnvelope sEnvelope;
                        sEnvelope.MinX = CPLAtof(papszLC[(bSwap)? 1 : 0]);
                        sEnvelope.MinY = CPLAtof(papszLC[(bSwap)? 0 : 1]);
                        sEnvelope.MaxX = CPLAtof(papszUC[(bSwap)? 1 : 0]);
                        sEnvelope.MaxY = CPLAtof(papszUC[(bSwap)? 0 : 1]);
                        aoMapBoundingBox[osCRS] = sEnvelope;
                    }
                    CSLDestroy(papszLC);
                    CSLDestroy(papszUC);
                }
            }
            else if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
                     strcmp(psSubIter->pszValue, "ResourceURL") == 0 )
            {
                if( EQUAL(CPLGetXMLValue(psSubIter, "resourceType", ""), "tile") )
                {
                    const char* pszFormat = CPLGetXMLValue(psSubIter, "format", "");
                    if( !osTileFormat.empty() && strcmp(osTileFormat, pszFormat) != 0 )
                        continue;
                    if( osURLTileTemplate.empty() )
                        osURLTileTemplate = CPLGetXMLValue(psSubIter, "template", "");
                }
                else if( EQUAL(CPLGetXMLValue(psSubIter, "resourceType", ""), "FeatureInfo") )
                {
                    const char* pszFormat = CPLGetXMLValue(psSubIter, "format", "");
                    if( !osInfoFormat.empty() && strcmp(osInfoFormat, pszFormat) != 0 )
                        continue;
                    if( osURLFeatureInfoTemplate.empty() )
                        osURLFeatureInfoTemplate = CPLGetXMLValue(psSubIter, "template", "");
                }
            }
        }
        if( strcmp(osSelectLayer, pszIdentifier) == 0 &&
            osSelectStyle.empty() && !aosStylesIdentifier.empty() )
        {
            osSelectStyle = aosStylesIdentifier[0];
        }
        for(size_t i=0;i<aosTMS.size();i++)
        {
            for(size_t j=0;j<aosStylesIdentifier.size();j++)
            {
                int nIdx = 1 + aosSubDatasets.size() / 2;
                CPLString osName(osCapabilitiesFilename);
                osName += ",layer=";
                osName += QuoteIfNecessary(pszIdentifier);
                if( aosTMS.size() > 1 )
                {
                    osName += ",tilematrixset=";
                    osName += QuoteIfNecessary(aosTMS[i]);
                }
                if( aosStylesIdentifier.size() > 1 )
                {
                    osName += ",style=";
                    osName += QuoteIfNecessary(aosStylesIdentifier[j]);
                }
                aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nIdx), osName);

                CPLString osDesc("Layer ");
                osDesc += pszTitle ? pszTitle : pszIdentifier;
                if( aosTMS.size() > 1 )
                {
                    osDesc += ", tile matrix set ";
                    osDesc += aosTMS[i];
                }
                if( aosStylesIdentifier.size() > 1 )
                {
                    osDesc += ", style ";
                    osDesc += QuoteIfNecessary(aosStylesTitle[j]);
                }
                aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nIdx), osDesc);
            }
        }
        if( !aosTMS.empty() && !aosStylesIdentifier.empty() )
            nLayerCount ++;
        else
            CPLError(CE_Failure, CPLE_AppDefined, "Missing TileMatrixSetLink and/or Style");
    }

    if( nLayerCount == 0 )
    {
        CPLDestroyXMLNode(psXML);
        return NULL;
    }

    WMTSDataset* poDS = new WMTSDataset();

    if( aosSubDatasets.size() > 2 )
        poDS->SetMetadata(aosSubDatasets.List(), "SUBDATASETS");

    if( nLayerCount == 1 )
    {
        if( !osSelectLayerTitle.empty() )
            poDS->SetMetadataItem("TITLE", osSelectLayerTitle);
        if( !osSelectLayerAbstract.empty() )
            poDS->SetMetadataItem("ABSTRACT", osSelectLayerAbstract);

        poDS->papszHTTPOptions = BuildHTTPRequestOpts(osOtherXML);
        poDS->osLayer = osSelectLayer;
        poDS->osTMS = osSelectTMS;

        WMTSTileMatrixSet oTMS;
        if( !ReadTMS(psContents, osSelectTMS, osMaxTileMatrixIdentifier,
                     nUserMaxZoomLevel, oTMS) )
        {
            CPLDestroyXMLNode(psXML);
            delete poDS;
            return NULL;
        }

        const char* pszExtentMethod = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "EXTENT_METHOD", "AUTO");
        ExtentMethod eExtentMethod = AUTO;
        if( EQUAL(pszExtentMethod, "LAYER_BBOX") )
            eExtentMethod = LAYER_BBOX;
        else if( EQUAL(pszExtentMethod, "TILE_MATRIX_SET") )
            eExtentMethod = TILE_MATRIX_SET;
        else if( EQUAL(pszExtentMethod, "MOST_PRECISE_TILE_MATRIX") )
            eExtentMethod = MOST_PRECISE_TILE_MATRIX;

        // Use in priority layer bounding box expressed in the SRS of the TMS
        if( (!bHasAOI || bExtendBeyondDateLine) &&
            (eExtentMethod == AUTO || eExtentMethod == LAYER_BBOX) &&
            aoMapBoundingBox.find(oTMS.osSRS) != aoMapBoundingBox.end() )
        {
            if( !bHasAOI )
            {
                sAOI = aoMapBoundingBox[oTMS.osSRS];
                bHasAOI = TRUE;
            }

            int bRecomputeAOI = FALSE;
            if( bExtendBeyondDateLine )
            {
                bExtendBeyondDateLine = FALSE;

                OGRSpatialReference oWGS84;
                    oWGS84.SetFromUserInput(SRS_WKT_WGS84);
                OGRCoordinateTransformation* poCT =
                    OGRCreateCoordinateTransformation(&oTMS.oSRS, &oWGS84);
                if( poCT != NULL )
                {
                    double dfX1 = sAOI.MinX;
                    double dfY1 = sAOI.MinY;
                    double dfX2 = sAOI.MaxX;
                    double dfY2 = sAOI.MaxY;
                    if( poCT->Transform(1, &dfX1, &dfY1) &&
                        poCT->Transform(1, &dfX2, &dfY2) )
                    {
                        if( fabs(dfX1 + 180) < 1e-8 &&
                            fabs(dfX2 - 180) < 1e-8 )
                        {
                            bExtendBeyondDateLine = TRUE;
                            bRecomputeAOI = TRUE;
                        }
                        else if( dfX2 < dfX1 )
                        {
                            bExtendBeyondDateLine = TRUE;
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "ExtendBeyondDateLine disabled, since longitudes of %s "
                                    "BoundingBox do not span from -180 to 180 but from %.16g to %.16g, "
                                    "or longitude of upper right corner is not lesser than the one of lower left corner",
                                    oTMS.osSRS.c_str(), dfX1, dfX2);
                        }
                    }
                    delete poCT;
                }
            }
            if( bExtendBeyondDateLine && bRecomputeAOI )
            {
                bExtendBeyondDateLine = FALSE;

                std::map<CPLString, OGREnvelope>::iterator oIter = aoMapBoundingBox.begin();
                for(; oIter != aoMapBoundingBox.end(); ++oIter )
                {
                    OGRSpatialReference oSRS;
                    if( oSRS.SetFromUserInput(FixCRSName(oIter->first)) == OGRERR_NONE )
                    {
                        OGRSpatialReference oWGS84;
                        oWGS84.SetFromUserInput(SRS_WKT_WGS84);
                        OGRCoordinateTransformation* poCT =
                            OGRCreateCoordinateTransformation(&oSRS, &oWGS84);
                        double dfX1 = oIter->second.MinX;
                        double dfY1 = oIter->second.MinY;
                        double dfX2 = oIter->second.MaxX;
                        double dfY2 = oIter->second.MaxY;
                        if( poCT != NULL &&
                            poCT->Transform(1, &dfX1, &dfY1) &&
                            poCT->Transform(1, &dfX2, &dfY2) &&
                            dfX2 < dfX1 )
                        {
                            delete poCT;
                            dfX2 += 360;
                            char* pszProj4 = NULL;
                            oTMS.oSRS.exportToProj4(&pszProj4);
                            oSRS.SetFromUserInput(CPLSPrintf("%s +over +wktext", pszProj4));
                            CPLFree(pszProj4);
                            poCT = OGRCreateCoordinateTransformation(&oWGS84, &oSRS);
                            if( poCT &&
                                poCT->Transform(1, &dfX1, &dfY1) &&
                                poCT->Transform(1, &dfX2, &dfY2) )
                            {
                                bExtendBeyondDateLine = TRUE;
                                sAOI.MinX = std::min(dfX1, dfX2);
                                sAOI.MinY = std::min(dfY1, dfY2);
                                sAOI.MaxX = std::max(dfX1, dfX2);
                                sAOI.MaxY = std::max(dfY1, dfY2);
                                CPLDebug("WMTS",
                                         "ExtendBeyondDateLine using %s bounding box",
                                         oIter->first.c_str());
                            }
                            delete poCT;
                            break;
                        }
                        delete poCT;
                    }
                }
            }
        }
        else
        {
            if( bExtendBeyondDateLine )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "ExtendBeyondDateLine disabled, since BoundingBox of %s is missing",
                         oTMS.osSRS.c_str());
                bExtendBeyondDateLine = FALSE;
            }
        }

        // Otherwise default to reproject a layer bounding box expressed in
        // another SRS
        if( !bHasAOI && !aoMapBoundingBox.empty() &&
            (eExtentMethod == AUTO || eExtentMethod == LAYER_BBOX) )
        {
            std::map<CPLString, OGREnvelope>::iterator oIter = aoMapBoundingBox.begin();
            for(; oIter != aoMapBoundingBox.end(); ++oIter )
            {
                OGRSpatialReference oSRS;
                if( oSRS.SetFromUserInput(FixCRSName(oIter->first)) == OGRERR_NONE )
                {
                    // Check if this doesn't match the most precise tile matrix
                    // by densifying its contour
                    const WMTSTileMatrix& oTM = oTMS.aoTM.back();

                    bool bMatchFound = false;
                    const char *pszProjectionTMS = oTMS.oSRS.GetAttrValue("PROJECTION");
                    const char *pszProjectionBBOX = oSRS.GetAttrValue("PROJECTION");
                    const bool bIsTMerc = (pszProjectionTMS != NULL &&
                        EQUAL(pszProjectionTMS, SRS_PT_TRANSVERSE_MERCATOR)) ||
                        (pszProjectionBBOX != NULL &&
                        EQUAL(pszProjectionBBOX, SRS_PT_TRANSVERSE_MERCATOR));
                    // If one of the 2 SRS is a TMerc, try with classical tmerc
                    // or etmerc.
                    for( int j = 0; j < (bIsTMerc ? 2 : 1); j++ )
                    {
                        CPLString osOldVal =
                            CPLGetThreadLocalConfigOption("OSR_USE_ETMERC", "");
                        if( bIsTMerc )
                        {
                            CPLSetThreadLocalConfigOption("OSR_USE_ETMERC",
                                                      (j==0) ? "NO" : "YES");
                        }
                        OGRCoordinateTransformation* poRevCT =
                            OGRCreateCoordinateTransformation(&oTMS.oSRS, &oSRS);
                        if( bIsTMerc )
                        {
                            CPLSetThreadLocalConfigOption("OSR_USE_ETMERC",
                                osOldVal.empty() ? NULL : osOldVal.c_str());
                        }
                        if( poRevCT != NULL )
                        {
                            const double dfX0 = oTM.dfTLX;
                            const double dfY1 = oTM.dfTLY;
                            const double dfX1 = oTM.dfTLX +
                            oTM.nMatrixWidth  * oTM.dfPixelSize * oTM.nTileWidth;
                            const double dfY0 = oTM.dfTLY -
                            oTM.nMatrixHeight * oTM.dfPixelSize * oTM.nTileHeight;
                            double dfXMin = std::numeric_limits<double>::infinity();
                            double dfYMin = std::numeric_limits<double>::infinity();
                            double dfXMax = -std::numeric_limits<double>::infinity();
                            double dfYMax = -std::numeric_limits<double>::infinity();

                            const int NSTEPS = 20;
                            for(int i=0;i<=NSTEPS;i++)
                            {
                                double dfX = dfX0 + (dfX1 - dfX0) * i / NSTEPS;
                                double dfY = dfY0;
                                if( poRevCT->Transform(1, &dfX, &dfY) )
                                {
                                    dfXMin = std::min(dfXMin, dfX);
                                    dfYMin = std::min(dfYMin, dfY);
                                    dfXMax = std::max(dfXMax, dfX);
                                    dfYMax = std::max(dfYMax, dfY);
                                }

                                dfX = dfX0 + (dfX1 - dfX0) * i / NSTEPS;
                                dfY = dfY1;
                                if( poRevCT->Transform(1, &dfX, &dfY) )
                                {
                                    dfXMin = std::min(dfXMin, dfX);
                                    dfYMin = std::min(dfYMin, dfY);
                                    dfXMax = std::max(dfXMax, dfX);
                                    dfYMax = std::max(dfYMax, dfY);
                                }

                                dfX = dfX0;
                                dfY = dfY0 + (dfY1 - dfY0) * i / NSTEPS;
                                if( poRevCT->Transform(1, &dfX, &dfY) )
                                {
                                    dfXMin = std::min(dfXMin, dfX);
                                    dfYMin = std::min(dfYMin, dfY);
                                    dfXMax = std::max(dfXMax, dfX);
                                    dfYMax = std::max(dfYMax, dfY);
                                }

                                dfX = dfX1;
                                dfY = dfY0 + (dfY1 - dfY0) * i / NSTEPS;
                                if( poRevCT->Transform(1, &dfX, &dfY) )
                                {
                                    dfXMin = std::min(dfXMin, dfX);
                                    dfYMin = std::min(dfYMin, dfY);
                                    dfXMax = std::max(dfXMax, dfX);
                                    dfYMax = std::max(dfYMax, dfY);
                                }
                            }

                            delete poRevCT;
#ifdef DEBUG_VERBOSE
                            CPLDebug("WMTS", "Reprojected densified bbox of most "
                                    "precise tile matrix in %s: %.8g %8g %8g %8g",
                                    oIter->first.c_str(),
                                    dfXMin, dfYMin, dfXMax, dfYMax);
#endif
                            if( fabs(oIter->second.MinX - dfXMin) < 1e-5 *
                                std::max(fabs(oIter->second.MinX),fabs(dfXMin)) &&
                                fabs(oIter->second.MinY - dfYMin) < 1e-5 *
                                std::max(fabs(oIter->second.MinY),fabs(dfYMin)) &&
                                fabs(oIter->second.MaxX - dfXMax) < 1e-5 *
                                std::max(fabs(oIter->second.MaxX),fabs(dfXMax)) &&
                                fabs(oIter->second.MaxY - dfYMax) < 1e-5 *
                                std::max(fabs(oIter->second.MaxY),fabs(dfYMax)) )
                            {
                                bMatchFound = true;
#ifdef DEBUG_VERBOSE
                                CPLDebug("WMTS", "Matches layer bounding box, so "
                                        "that one is not significant");
#endif
                                break;
                            }
                        }
                    }

                    if( bMatchFound )
                    {
                        if( eExtentMethod == LAYER_BBOX )
                            eExtentMethod = MOST_PRECISE_TILE_MATRIX;
                        break;
                    }

                    OGRCoordinateTransformation* poCT =
                        OGRCreateCoordinateTransformation(&oSRS, &oTMS.oSRS);
                    if( poCT != NULL )
                    {
                        double dfX1 = oIter->second.MinX;
                        double dfY1 = oIter->second.MinY;
                        double dfX2 = oIter->second.MaxX;
                        double dfY2 = oIter->second.MinY;
                        double dfX3 = oIter->second.MaxX;
                        double dfY3 = oIter->second.MaxY;
                        double dfX4 = oIter->second.MinX;
                        double dfY4 = oIter->second.MaxY;
                        if( poCT->Transform(1, &dfX1, &dfY1) &&
                            poCT->Transform(1, &dfX2, &dfY2) &&
                            poCT->Transform(1, &dfX3, &dfY3) &&
                            poCT->Transform(1, &dfX4, &dfY4) )
                        {
                            sAOI.MinX = std::min(std::min(dfX1, dfX2),
                                                 std::min(dfX3, dfX4));
                            sAOI.MinY = std::min(std::min(dfY1, dfY2),
                                                 std::min(dfY3, dfY4));
                            sAOI.MaxX = std::max(std::max(dfX1, dfX2),
                                                 std::max(dfX3, dfX4));
                            sAOI.MaxY = std::max(std::max(dfY1, dfY2),
                                                 std::max(dfY3, dfY4));
                            bHasAOI = TRUE;
                        }
                        delete poCT;
                    }
                    break;
                }
            }
        }

        // Otherwise default to BoundingBox of the TMS
        if( !bHasAOI && oTMS.bBoundingBoxValid &&
            (eExtentMethod == AUTO || eExtentMethod == TILE_MATRIX_SET) )
        {
            CPLDebug("WMTS", "Using TMS bounding box");
            sAOI = oTMS.sBoundingBox;
            bHasAOI = TRUE;
        }

        // Otherwise default to implied BoundingBox of the most precise TM
        if( !bHasAOI &&
            (eExtentMethod == AUTO || eExtentMethod == MOST_PRECISE_TILE_MATRIX) )
        {
            const WMTSTileMatrix& oTM = oTMS.aoTM.back();
            CPLDebug("WMTS", "Using TM level %s bounding box", oTM.osIdentifier.c_str() );

            sAOI.MinX = oTM.dfTLX;
            sAOI.MaxY = oTM.dfTLY;
            sAOI.MaxX = oTM.dfTLX + oTM.nMatrixWidth  * oTM.dfPixelSize * oTM.nTileWidth;
            sAOI.MinY = oTM.dfTLY - oTM.nMatrixHeight * oTM.dfPixelSize * oTM.nTileHeight;
            bHasAOI = TRUE;
        }

        if( !bHasAOI )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not determine raster extent");
            CPLDestroyXMLNode(psXML);
            delete poDS;
            return NULL;
        }

        {
            // Clip with implied BoundingBox of the most precise TM
            // Useful for http://tileserver.maptiler.com/wmts
            const WMTSTileMatrix& oTM = oTMS.aoTM.back();

            // For https://data.linz.govt.nz/services;key=XXXXXXXX/wmts/1.0.0/set/69/WMTSCapabilities.xml
            // only clip in Y since there's a warp over dateline.
            // Update: it sems that the content of the server has changed since
            // initial coding. So do X clipping in default mode.
            if( !bExtendBeyondDateLine )
            {
                sAOI.MinX = std::max(sAOI.MinX, oTM.dfTLX);
                sAOI.MaxX = std::min(sAOI.MaxX,
                    oTM.dfTLX +
                    oTM.nMatrixWidth  * oTM.dfPixelSize * oTM.nTileWidth);
            }
            sAOI.MaxY = std::min(sAOI.MaxY, oTM.dfTLY);
            sAOI.MinY =
                std::max(sAOI.MinY,
                         oTM.dfTLY -
                         oTM.nMatrixHeight * oTM.dfPixelSize * oTM.nTileHeight);
        }

        // Clip with limits of most precise TM when available
        {
            const WMTSTileMatrix& oTM = oTMS.aoTM.back();
            if( aoMapTileMatrixLimits.find(oTM.osIdentifier) != aoMapTileMatrixLimits.end() )
            {
                const WMTSTileMatrixLimits& oTMLimits = aoMapTileMatrixLimits[oTM.osIdentifier];
                double dfTileWidthUnits = oTM.dfPixelSize * oTM.nTileWidth;
                double dfTileHeightUnits = oTM.dfPixelSize * oTM.nTileHeight;
                sAOI.MinX = std::max(sAOI.MinX, oTM.dfTLX + oTMLimits.nMinTileCol * dfTileWidthUnits);
                sAOI.MaxY = std::min(sAOI.MaxY, oTM.dfTLY - oTMLimits.nMinTileRow * dfTileHeightUnits);
                sAOI.MaxX = std::min(sAOI.MaxX, oTM.dfTLX + (oTMLimits.nMaxTileCol + 1) * dfTileWidthUnits);
                sAOI.MinY = std::max(sAOI.MinY, oTM.dfTLY - (oTMLimits.nMaxTileRow + 1) * dfTileHeightUnits);
            }
        }

        // Establish raster dimension and extent
        int nMaxZoomLevel = (int)oTMS.aoTM.size()-1;
        while(nMaxZoomLevel >= 0)
        {
            const WMTSTileMatrix& oTM = oTMS.aoTM[nMaxZoomLevel];
            double dfRasterXSize = (sAOI.MaxX - sAOI.MinX) / oTM.dfPixelSize;
            double dfRasterYSize = (sAOI.MaxY - sAOI.MinY) / oTM.dfPixelSize;
            if( dfRasterXSize < INT_MAX && dfRasterYSize < INT_MAX )
            {
                if( nMaxZoomLevel != (int)oTMS.aoTM.size()-1 )
                {
                    CPLDebug("WMTS", "Using zoom level %s instead of %s to avoid int overflow",
                             oTMS.aoTM[nMaxZoomLevel].osIdentifier.c_str(),
                             oTMS.aoTM.back().osIdentifier.c_str());
                }

                // Align AOI on pixel boundaries with respect to TopLeftCorner of
                // this tile matrix
                poDS->adfGT[0] = oTM.dfTLX + floor((sAOI.MinX - oTM.dfTLX) / oTM.dfPixelSize+1e-10) * oTM.dfPixelSize;
                poDS->adfGT[1] = oTM.dfPixelSize;
                poDS->adfGT[2] = 0.0;
                poDS->adfGT[3] = oTM.dfTLY + ceil((sAOI.MaxY - oTM.dfTLY) / oTM.dfPixelSize-1e-10) * oTM.dfPixelSize;
                poDS->adfGT[4] = 0.0;
                poDS->adfGT[5] = -oTM.dfPixelSize;
                poDS->nRasterXSize = int(0.5 + (sAOI.MaxX - poDS->adfGT[0]) / oTM.dfPixelSize);
                poDS->nRasterYSize = int(0.5 + (poDS->adfGT[3] - sAOI.MinY) / oTM.dfPixelSize);
                break;
            }
            nMaxZoomLevel --;
        }
        if( nMaxZoomLevel < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No zoom level in tile matrix set found");
            CPLDestroyXMLNode(psXML);
            delete poDS;
            return NULL;
        }
        CPLDebug("WMTS", "Using tilematrix=%s (zoom level %d)",
                 oTMS.aoTM[nMaxZoomLevel].osIdentifier.c_str(), nMaxZoomLevel);
        oTMS.aoTM.resize(1 + nMaxZoomLevel);
        poDS->oTMS = oTMS;

        if( !osProjection.empty() )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput(osProjection) == OGRERR_NONE )
            {
                char* pszWKT = NULL;
                oSRS.exportToWkt(&pszWKT);
                poDS->osProjection = pszWKT;
                CPLFree(pszWKT);
            }
        }
        if( poDS->osProjection.empty() )
        {
            // Strip AXIS
            OGR_SRSNode *poGEOGCS = oTMS.oSRS.GetAttrNode( "GEOGCS" );
            if( poGEOGCS != NULL )
                poGEOGCS->StripNodes( "AXIS" );

            OGR_SRSNode *poPROJCS = oTMS.oSRS.GetAttrNode( "PROJCS" );
            if (poPROJCS != NULL && oTMS.oSRS.EPSGTreatsAsNorthingEasting())
                poPROJCS->StripNodes( "AXIS" );

            char* pszWKT = NULL;
            oTMS.oSRS.exportToWkt(&pszWKT);
            poDS->osProjection = pszWKT;
            CPLFree(pszWKT);
        }

        if( osURLTileTemplate.empty() )
        {
            osURLTileTemplate = GetOperationKVPURL(psXML, "GetTile");
            if( osURLTileTemplate.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "No RESTful nor KVP GetTile operation found");
                CPLDestroyXMLNode(psXML);
                delete poDS;
                return NULL;
            }
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "service", "WMTS");
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "request", "GetTile");
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "version", "1.0.0");
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "layer", osSelectLayer);
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "style", osSelectStyle);
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "format", osSelectTileFormat);
            osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate, "TileMatrixSet", osSelectTMS);
            osURLTileTemplate += "&TileMatrix={TileMatrix}";
            osURLTileTemplate += "&TileRow=${y}";
            osURLTileTemplate += "&TileCol=${x}";

            std::map<CPLString,CPLString>::iterator oIter = aoMapDimensions.begin();
            for(; oIter != aoMapDimensions.end(); ++oIter )
            {
                osURLTileTemplate = CPLURLAddKVP(osURLTileTemplate,
                                                 oIter->first,
                                                 oIter->second);
            }
            //CPLDebug("WMTS", "osURLTileTemplate = %s", osURLTileTemplate.c_str());
        }
        else
        {
            osURLTileTemplate = Replace(osURLTileTemplate, "{Style}", osSelectStyle);
            osURLTileTemplate = Replace(osURLTileTemplate, "{TileMatrixSet}", osSelectTMS);
            osURLTileTemplate = Replace(osURLTileTemplate, "{TileCol}", "${x}");
            osURLTileTemplate = Replace(osURLTileTemplate, "{TileRow}", "${y}");

            std::map<CPLString,CPLString>::iterator oIter = aoMapDimensions.begin();
            for(; oIter != aoMapDimensions.end(); ++oIter )
            {
                osURLTileTemplate = Replace(osURLTileTemplate,
                                            CPLSPrintf("{%s}", oIter->first.c_str()),
                                            oIter->second);
            }
        }

        if( osURLFeatureInfoTemplate.empty() && !osSelectInfoFormat.empty() )
        {
            osURLFeatureInfoTemplate = GetOperationKVPURL(psXML, "GetFeatureInfo");
            if( !osURLFeatureInfoTemplate.empty() )
            {
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "service", "WMTS");
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "request", "GetFeatureInfo");
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "version", "1.0.0");
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "layer", osSelectLayer);
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "style", osSelectStyle);
                //osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "format", osSelectTileFormat);
                osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate, "InfoFormat", osSelectInfoFormat);
                osURLFeatureInfoTemplate += "&TileMatrixSet={TileMatrixSet}";
                osURLFeatureInfoTemplate += "&TileMatrix={TileMatrix}";
                osURLFeatureInfoTemplate += "&TileRow={TileRow}";
                osURLFeatureInfoTemplate += "&TileCol={TileCol}";
                osURLFeatureInfoTemplate += "&J={J}";
                osURLFeatureInfoTemplate += "&I={I}";

                std::map<CPLString,CPLString>::iterator oIter = aoMapDimensions.begin();
                for(; oIter != aoMapDimensions.end(); ++oIter )
                {
                    osURLFeatureInfoTemplate = CPLURLAddKVP(osURLFeatureInfoTemplate,
                                                            oIter->first,
                                                            oIter->second);
                }
                //CPLDebug("WMTS", "osURLFeatureInfoTemplate = %s", osURLFeatureInfoTemplate.c_str());
            }
        }
        else
        {
             osURLFeatureInfoTemplate = Replace(osURLFeatureInfoTemplate, "{Style}", osSelectStyle);

            std::map<CPLString,CPLString>::iterator oIter = aoMapDimensions.begin();
            for(; oIter != aoMapDimensions.end(); ++oIter )
            {
                osURLFeatureInfoTemplate = Replace(osURLFeatureInfoTemplate,
                                            CPLSPrintf("{%s}", oIter->first.c_str()),
                                            oIter->second);
            }
        }
        poDS->osURLFeatureInfoTemplate = osURLFeatureInfoTemplate;

        // Build all TMS datasets, wrapped in VRT datasets
        for(int i=nMaxZoomLevel;i>=0;i--)
        {
            const WMTSTileMatrix& oTM = oTMS.aoTM[i];
            int nRasterXSize = int(0.5 + poDS->nRasterXSize / oTM.dfPixelSize * poDS->adfGT[1]);
            int nRasterYSize = int(0.5 + poDS->nRasterYSize / oTM.dfPixelSize * poDS->adfGT[1]);
            if( !poDS->apoDatasets.empty() &&
                (nRasterXSize < 128 || nRasterYSize < 128) )
            {
                break;
            }
            CPLString osURL(Replace(osURLTileTemplate, "{TileMatrix}", oTM.osIdentifier));

            double dfTileWidthUnits = oTM.dfPixelSize * oTM.nTileWidth;
            double dfTileHeightUnits = oTM.dfPixelSize * oTM.nTileHeight;

            // Compute the shift in terms of tiles between AOI and TM origin
            int nTileX = (int)(floor(poDS->adfGT[0] - oTM.dfTLX + 1e-10) / dfTileWidthUnits);
            int nTileY = (int)(floor(oTM.dfTLY - poDS->adfGT[3] + 1e-10) / dfTileHeightUnits);

            // Compute extent of this zoom level slightly larger than the AOI and
            // aligned on tile boundaries at this TM
            double dfULX = oTM.dfTLX + nTileX * dfTileWidthUnits;
            double dfULY = oTM.dfTLY - nTileY * dfTileHeightUnits;
            double dfLRX = poDS->adfGT[0] + poDS->nRasterXSize * poDS->adfGT[1];
            double dfLRY = poDS->adfGT[3] + poDS->nRasterYSize * poDS->adfGT[5];
            dfLRX = dfULX + ceil((dfLRX - dfULX) / dfTileWidthUnits - 1e-10) * dfTileWidthUnits;
            dfLRY = dfULY + floor((dfLRY - dfULY) / dfTileHeightUnits + 1e-10) * dfTileHeightUnits;

            int nSizeX = int(0.5+(dfLRX - dfULX) / oTM.dfPixelSize);
            int nSizeY = int(0.5+(dfULY - dfLRY) / oTM.dfPixelSize);

            double dfDateLineX = oTM.dfTLX + oTM.nMatrixWidth * dfTileWidthUnits;
            int nSizeX1 = int(0.5+(dfDateLineX - dfULX) / oTM.dfPixelSize);
            int nSizeX2 = int(0.5+(dfLRX - dfDateLineX) / oTM.dfPixelSize);
            if( bExtendBeyondDateLine && dfDateLineX > dfLRX )
            {
                CPLDebug("WMTS", "ExtendBeyondDateLine ignored in that case");
                bExtendBeyondDateLine = FALSE;
            }

#define WMS_TMS_TEMPLATE \
    "<GDAL_WMS>" \
    "<Service name=\"TMS\">" \
    "    <ServerUrl>%s</ServerUrl>" \
    "</Service>" \
    "<DataWindow>" \
    "    <UpperLeftX>%.16g</UpperLeftX>" \
    "    <UpperLeftY>%.16g</UpperLeftY>" \
    "    <LowerRightX>%.16g</LowerRightX>" \
    "    <LowerRightY>%.16g</LowerRightY>" \
    "    <TileLevel>0</TileLevel>" \
    "    <TileX>%d</TileX>" \
    "    <TileY>%d</TileY>" \
    "    <SizeX>%d</SizeX>" \
    "    <SizeY>%d</SizeY>" \
    "    <YOrigin>top</YOrigin>" \
    "</DataWindow>" \
    "<BlockSizeX>%d</BlockSizeX>" \
    "<BlockSizeY>%d</BlockSizeY>" \
    "<BandsCount>%d</BandsCount>" \
    "%s" \
"</GDAL_WMS>"

            CPLString osStr(CPLSPrintf( WMS_TMS_TEMPLATE,
                WMTSEscapeXML(osURL).c_str(),
                dfULX, dfULY, (bExtendBeyondDateLine) ? dfDateLineX : dfLRX, dfLRY,
                nTileX, nTileY, (bExtendBeyondDateLine) ? nSizeX1 : nSizeX, nSizeY,
                oTM.nTileWidth, oTM.nTileHeight, nBands,
                osOtherXML.c_str()));
            GDALDataset* poWMSDS = (GDALDataset*)GDALOpenEx(
                osStr, GDAL_OF_RASTER | GDAL_OF_SHARED | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
            if( poWMSDS == NULL )
            {
                CPLDestroyXMLNode(psXML);
                delete poDS;
                return NULL;
            }

            VRTDatasetH hVRTDS = VRTCreate( nRasterXSize, nRasterYSize );
            for(int iBand=1;iBand<=nBands;iBand++)
            {
                VRTAddBand( hVRTDS, GDT_Byte, NULL );
            }

            int nSrcXOff, nSrcYOff, nDstXOff, nDstYOff;

            nSrcXOff = 0;
            nDstXOff = (int)(0.5 + (dfULX - poDS->adfGT[0]) / oTM.dfPixelSize);

            nSrcYOff = 0;
            nDstYOff = (int)(0.5 + (poDS->adfGT[3] - dfULY) / oTM.dfPixelSize);

            if( bExtendBeyondDateLine )
            {
                int nSrcXOff2, nDstXOff2;

                nSrcXOff2 = 0;
                nDstXOff2 = (int)(0.5 + (dfDateLineX - poDS->adfGT[0]) / oTM.dfPixelSize);

                osStr = CPLSPrintf( WMS_TMS_TEMPLATE,
                    WMTSEscapeXML(osURL).c_str(),
                    -dfDateLineX, dfULY, dfLRX - 2 * dfDateLineX, dfLRY,
                    0, nTileY, nSizeX2, nSizeY,
                    oTM.nTileWidth, oTM.nTileHeight, nBands,
                    osOtherXML.c_str());

                GDALDataset* poWMSDS2 = (GDALDataset*)GDALOpenEx(
                    osStr, GDAL_OF_RASTER | GDAL_OF_SHARED, NULL, NULL, NULL);
                CPLAssert(poWMSDS2);

                for(int iBand=1;iBand<=nBands;iBand++)
                {
                    VRTSourcedRasterBandH hVRTBand =
                        (VRTSourcedRasterBandH) GDALGetRasterBand(hVRTDS, iBand);
                    VRTAddSimpleSource( hVRTBand, GDALGetRasterBand(poWMSDS, iBand),
                                        nSrcXOff, nSrcYOff, nSizeX1, nSizeY,
                                        nDstXOff, nDstYOff, nSizeX1, nSizeY,
                                        "NEAR", VRT_NODATA_UNSET);
                    VRTAddSimpleSource( hVRTBand, GDALGetRasterBand(poWMSDS2, iBand),
                                        nSrcXOff2, nSrcYOff, nSizeX2, nSizeY,
                                        nDstXOff2, nDstYOff, nSizeX2, nSizeY,
                                        "NEAR", VRT_NODATA_UNSET);
                }

                poWMSDS2->Dereference();
            }
            else
            {
                for(int iBand=1;iBand<=nBands;iBand++)
                {
                    VRTSourcedRasterBandH hVRTBand =
                        (VRTSourcedRasterBandH) GDALGetRasterBand(hVRTDS, iBand);
                    VRTAddSimpleSource( hVRTBand, GDALGetRasterBand(poWMSDS, iBand),
                                        nSrcXOff, nSrcYOff, nSizeX, nSizeY,
                                        nDstXOff, nDstYOff, nSizeX, nSizeY,
                                        "NEAR", VRT_NODATA_UNSET);
                }
            }

            poWMSDS->Dereference();

            poDS->apoDatasets.push_back((GDALDataset*)hVRTDS);
        }

        if( poDS->apoDatasets.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No zoom level found");
            CPLDestroyXMLNode(psXML);
            delete poDS;
            return NULL;
        }

        poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        for(int i=0;i<nBands;i++)
            poDS->SetBand(i+1, new WMTSBand(poDS,i+1));

        poDS->osXML = "<GDAL_WMTS>\n";
        poDS->osXML += "  <GetCapabilitiesUrl>" +
                     WMTSEscapeXML(osGetCapabilitiesURL) +
                     "</GetCapabilitiesUrl>\n";
        if( !osSelectLayer.empty() )
            poDS->osXML += "  <Layer>" + WMTSEscapeXML(osSelectLayer) + "</Layer>\n";
        if( !osSelectStyle.empty() )
            poDS->osXML += "  <Style>" + WMTSEscapeXML(osSelectStyle) + "</Style>\n";
        if( !osSelectTMS.empty() )
            poDS->osXML += "  <TileMatrixSet>" + WMTSEscapeXML(osSelectTMS) + "</TileMatrixSet>\n";
        if( !osMaxTileMatrixIdentifier.empty() )
            poDS->osXML += "  <TileMatrix>" + WMTSEscapeXML(osMaxTileMatrixIdentifier) + "</TileMatrix>\n";
        if( nUserMaxZoomLevel >= 0 )
            poDS->osXML += "  <ZoomLevel>" + CPLString().Printf("%d", nUserMaxZoomLevel) + "</ZoomLevel>\n";
        if( nCountTileFormat > 1 && !osSelectTileFormat.empty() )
            poDS->osXML += "  <Format>" + WMTSEscapeXML(osSelectTileFormat) + "</Format>\n";
        if( nCountInfoFormat > 1 && !osSelectInfoFormat.empty() )
            poDS->osXML += "  <InfoFormat>" + WMTSEscapeXML(osSelectInfoFormat) + "</InfoFormat>\n";
        poDS->osXML += "  <DataWindow>\n";
        poDS->osXML += CPLSPrintf("    <UpperLeftX>%.16g</UpperLeftX>\n",
                             poDS->adfGT[0]);
        poDS->osXML += CPLSPrintf("    <UpperLeftY>%.16g</UpperLeftY>\n",
                             poDS->adfGT[3]);
        poDS->osXML += CPLSPrintf("    <LowerRightX>%.16g</LowerRightX>\n",
                             poDS->adfGT[0] +  poDS->adfGT[1] *  poDS->nRasterXSize);
        poDS->osXML += CPLSPrintf("    <LowerRightY>%.16g</LowerRightY>\n",
                             poDS->adfGT[3] +  poDS->adfGT[5] *  poDS->nRasterYSize);
        poDS->osXML += "  </DataWindow>\n";
        if( bExtendBeyondDateLine )
            poDS->osXML += "  <ExtendBeyondDateLine>true</ExtendBeyondDateLine>\n";
        poDS->osXML += CPLSPrintf("  <BandsCount>%d</BandsCount>\n", nBands);
        poDS->osXML += "  <Cache />\n";
        poDS->osXML += "  <UnsafeSSL>true</UnsafeSSL>\n";
        poDS->osXML += "  <ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>\n";
        poDS->osXML += "  <ZeroBlockOnServerException>true</ZeroBlockOnServerException>\n";
        poDS->osXML += "</GDAL_WMTS>\n";
    }

    CPLDestroyXMLNode(psXML);

    poDS->SetPamFlags(0);
    return poDS;
}
/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *WMTSDataset::CreateCopy( const char * pszFilename,
                                         GDALDataset *poSrcDS,
                                         CPL_UNUSED int bStrict,
                                         CPL_UNUSED char ** papszOptions,
                                         CPL_UNUSED GDALProgressFunc pfnProgress,
                                         CPL_UNUSED void * pProgressData )
{
    if( poSrcDS->GetDriver() == NULL ||
        poSrcDS->GetDriver() != GDALGetDriverByName("WMTS") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset must be a WMTS dataset");
        return NULL;
    }

    const char* pszXML = poSrcDS->GetMetadataItem("XML", "WMTS");
    if (pszXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot get XML definition of source WMTS dataset");
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
/*                       GDALRegister_WMTS()                            */
/************************************************************************/

void GDALRegister_WMTS()

{
    if( !GDAL_CHECK_VERSION( "WMTS driver" ) )
        return;

    if( GDALGetDriverByName( "WMTS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "WMTS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OGC Web Map Tile Service" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_wmts.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "WMTS:" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URL' type='string' description='URL that points to GetCapabilities response' required='YES'/>"
"  <Option name='LAYER' type='string' description='Layer identifier'/>"
"  <Option name='TILEMATRIXSET' alias='TMS' type='string' description='Tile matrix set identifier'/>"
"  <Option name='TILEMATRIX' type='string' description='Tile matrix identifier of maximum zoom level. Exclusive with ZOOM_LEVEL.'/>"
"  <Option name='ZOOM_LEVEL' alias='ZOOMLEVEL' type='int' description='Maximum zoom level. Exclusive with TILEMATRIX.'/>"
"  <Option name='STYLE' type='string' description='Style identifier'/>"
"  <Option name='EXTENDBEYONDDATELINE' type='boolean' description='Whether to enable extend-beyond-dateline behaviour' default='NO'/>"
"  <Option name='EXTENT_METHOD' type='string-select' description='How the raster extent is computed' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>LAYER_BBOX</Value>"
"       <Value>TILE_MATRIX_SET</Value>"
"       <Value>MOST_PRECISE_TILE_MATRIX</Value>"
"  </Option>"
"</OpenOptionList>");

    poDriver->pfnOpen = WMTSDataset::Open;
    poDriver->pfnIdentify = WMTSDataset::Identify;
    poDriver->pfnCreateCopy = WMTSDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
