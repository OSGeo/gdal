/******************************************************************************
 *
 * Project:  PLMosaic driver
 * Purpose:  PLMosaic driver
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015-2018, Planet Labs
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
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "../vrt/gdal_vrt.h"

#include "ogrgeojsonreader.h"

#include <algorithm>

CPL_CVSID("$Id$")

#define SPHERICAL_RADIUS        6378137.0
#define GM_ORIGIN  -20037508.340
#define GM_ZOOM_0  ((2 * -(GM_ORIGIN)) / 256)

/************************************************************************/
/* ==================================================================== */
/*                           PLMosaicDataset                            */
/* ==================================================================== */
/************************************************************************/

class PLLinkedDataset;
class PLLinkedDataset
{
public:
    CPLString            osKey;
    GDALDataset         *poDS;
    PLLinkedDataset       *psPrev;
    PLLinkedDataset       *psNext;

                        PLLinkedDataset() : poDS(nullptr), psPrev(nullptr), psNext(nullptr) {}
};

class PLMosaicRasterBand;

class PLMosaicDataset final: public GDALPamDataset
{
    friend class PLMosaicRasterBand;

        int                     bMustCleanPersistent;
        CPLString               osCachePathRoot;
        int                     bTrustCache;
        CPLString               osBaseURL;
        CPLString               osAPIKey;
        CPLString               osMosaic;
        char                   *pszWKT;
        int                     nQuadSize;
        CPLString               osQuadsURL;
        int                     bHasGeoTransform;
        double                  adfGeoTransform[6];
        int                     nZoomLevelMax;
        int                     bUseTMSForMain;
        std::vector<GDALDataset*> apoTMSDS;
        int                     nMetaTileXShift = 0;
        int                     nMetaTileYShift = 0;
        bool                    bQuadDownload = false;

        int                     nCacheMaxSize;
        std::map<CPLString, PLLinkedDataset*> oMapLinkedDatasets;
        PLLinkedDataset        *psHead;
        PLLinkedDataset        *psTail;
        void                    FlushDatasetsCache();
        CPLString               GetMosaicCachePath();
        void                    CreateMosaicCachePathIfNecessary();

        int                     nLastMetaTileX;
        int                     nLastMetaTileY;
        json_object            *poLastItemsInformation = nullptr;
        CPLString               osLastRetGetLocationInfo;
        const char             *GetLocationInfo(int nPixel, int nLine);

        char                  **GetBaseHTTPOptions();
        CPLHTTPResult          *Download(const char* pszURL,
                                         int bQuiet404Error = FALSE);
        json_object            *RunRequest(const char* pszURL,
                                           int bQuiet404Error = FALSE);
        int                     OpenMosaic();
        std::vector<CPLString>  ListSubdatasets();

        static CPLString        formatTileName(int tile_x, int tile_y);
        void                    InsertNewDataset(CPLString osKey, GDALDataset* poDS);
        GDALDataset*            OpenAndInsertNewDataset(CPLString osTmpFilename,
                                                        CPLString osTilename);

  public:
                PLMosaicDataset();
    virtual  ~PLMosaicDataset();

    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset  *Open( GDALOpenInfo * );

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

    virtual void FlushCache(bool bAtClosing) override;

    virtual const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual CPLErr      GetGeoTransform(double* padfGeoTransform) override;

    GDALDataset        *GetMetaTile(int tile_x, int tile_y);
};

/************************************************************************/
/* ==================================================================== */
/*                         PLMosaicRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class PLMosaicRasterBand final: public GDALRasterBand
{
    friend class PLMosaicDataset;

  public:

                PLMosaicRasterBand( PLMosaicDataset * poDS, int nBand,
                                    GDALDataType eDataType );

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual CPLErr          IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg) override;

    virtual const char     *GetMetadataItem( const char* pszName,
                                             const char * pszDomain = "" ) override;

    virtual GDALColorInterp GetColorInterpretation() override;

    virtual int             GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int iOvrLevel) override;
};

/************************************************************************/
/*                        PLMosaicRasterBand()                          */
/************************************************************************/

PLMosaicRasterBand::PLMosaicRasterBand( PLMosaicDataset *poDSIn, int nBandIn,
                                        GDALDataType eDataTypeIn )

{
    eDataType = eDataTypeIn;
    nBlockXSize = 256;
    nBlockYSize = 256;

    poDS = poDSIn;
    nBand = nBandIn;

    if( eDataType == GDT_UInt16 )
    {
        if( nBand <= 3 )
            SetMetadataItem("NBITS", "12", "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PLMosaicRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                       void *pImage )
{
    PLMosaicDataset* poMOSDS = reinterpret_cast<PLMosaicDataset *>( poDS );

#ifdef DEBUG_VERBOSE
    CPLDebug("PLMOSAIC", "IReadBlock(band=%d, x=%d, y=%d)",
             nBand, nBlockYOff, nBlockYOff);
#endif

    if( poMOSDS->bUseTMSForMain && !poMOSDS->apoTMSDS.empty() )
        return poMOSDS->apoTMSDS[0]->GetRasterBand(nBand)->ReadBlock(nBlockXOff, nBlockYOff,
                                                                pImage);

    const int bottom_yblock = (nRasterYSize - nBlockYOff * nBlockYSize) / nBlockYSize - 1;

    const int meta_tile_x = poMOSDS->nMetaTileXShift +
                            (nBlockXOff * nBlockXSize) / poMOSDS->nQuadSize;
    const int meta_tile_y = poMOSDS->nMetaTileYShift +
                            (bottom_yblock * nBlockYSize) / poMOSDS->nQuadSize;
    const int sub_tile_x = nBlockXOff % (poMOSDS->nQuadSize / nBlockXSize);
    const int sub_tile_y = nBlockYOff % (poMOSDS->nQuadSize / nBlockYSize);

    GDALDataset *poMetaTileDS = poMOSDS->GetMetaTile(meta_tile_x, meta_tile_y);
    if( poMetaTileDS == nullptr )
    {
        memset(pImage, 0,
               nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType)/8));
        return CE_None;
    }

    return poMetaTileDS->GetRasterBand(nBand)->
                RasterIO( GF_Read,
                        sub_tile_x * nBlockXSize,
                        sub_tile_y * nBlockYSize,
                        nBlockXSize,
                        nBlockYSize,
                        pImage, nBlockXSize, nBlockYSize,
                        eDataType, 0, 0, nullptr);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PLMosaicRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff, int nXSize, int nYSize,
                                         void * pData, int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         GSpacing nPixelSpace, GSpacing nLineSpace,
                                         GDALRasterIOExtraArg* psExtraArg )
{
    PLMosaicDataset* poMOSDS = reinterpret_cast<PLMosaicDataset *>( poDS );
    if( poMOSDS->bUseTMSForMain && !poMOSDS->apoTMSDS.empty() )
        return poMOSDS->apoTMSDS[0]->GetRasterBand(nBand)->RasterIO(
                                         eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );

    return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char* PLMosaicRasterBand::GetMetadataItem( const char* pszName,
                                                 const char* pszDomain )
{
    PLMosaicDataset* poMOSDS = reinterpret_cast<PLMosaicDataset *>( poDS );
    int nPixel, nLine;
    if( poMOSDS->bQuadDownload &&
        pszName != nullptr && pszDomain != nullptr &&
        EQUAL(pszDomain, "LocationInfo") &&
        sscanf(pszName, "Pixel_%d_%d", &nPixel, &nLine) == 2 )
    {
        return poMOSDS->GetLocationInfo(nPixel, nLine);
    }

    return GDALRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int PLMosaicRasterBand::GetOverviewCount()
{
    PLMosaicDataset *poGDS = reinterpret_cast<PLMosaicDataset *>( poDS );
    return std::max(0, static_cast<int>(poGDS->apoTMSDS.size()) - 1);
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* PLMosaicRasterBand::GetOverview(int iOvrLevel)
{
    PLMosaicDataset *poGDS = reinterpret_cast<PLMosaicDataset *>( poDS );
    if (iOvrLevel < 0 ||
        iOvrLevel >= static_cast<int>(poGDS->apoTMSDS.size()) - 1)
        return nullptr;

    poGDS->CreateMosaicCachePathIfNecessary();

    return poGDS->apoTMSDS[iOvrLevel+1]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PLMosaicRasterBand::GetColorInterpretation()
{
    switch( nBand )
    {
        case 1:
            return GCI_RedBand;
        case 2:
            return GCI_GreenBand;
        case 3:
            return GCI_BlueBand;
        case 4:
            return GCI_AlphaBand;
        default:
            CPLAssert(false);
            return GCI_GrayIndex;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                           PLMosaicDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        PLMosaicDataset()                            */
/************************************************************************/

PLMosaicDataset::PLMosaicDataset() :
    bMustCleanPersistent(FALSE),
    bTrustCache(FALSE),
    pszWKT(nullptr),
    nQuadSize(0),
    bHasGeoTransform(FALSE),
    nZoomLevelMax(0),
    bUseTMSForMain(FALSE),
    nCacheMaxSize(10),
    psHead(nullptr),
    psTail(nullptr),
    nLastMetaTileX(-1),
    nLastMetaTileY(-1)
{
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;

    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    osCachePathRoot = CPLGetPath(CPLGenerateTempFilename(""));
}

/************************************************************************/
/*                         ~PLMosaicDataset()                           */
/************************************************************************/

PLMosaicDataset::~PLMosaicDataset()

{
    PLMosaicDataset::FlushCache(true);
    CPLFree(pszWKT);
    for( auto& poDS: apoTMSDS )
        delete poDS;
    if( poLastItemsInformation )
        json_object_put(poLastItemsInformation);
    if (bMustCleanPersistent)
    {
        char** papszOptions
            = CSLSetNameValue(nullptr, "CLOSE_PERSISTENT",
                              CPLSPrintf("PLMOSAIC:%p", this));
        CPLHTTPDestroyResult( CPLHTTPFetch( osBaseURL, papszOptions) );
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                      FlushDatasetsCache()                            */
/************************************************************************/

void PLMosaicDataset::FlushDatasetsCache()
{
    for( PLLinkedDataset* psIter = psHead; psIter != nullptr;  )
    {
        PLLinkedDataset* psNext = psIter->psNext;
        if( psIter->poDS )
            GDALClose(psIter->poDS);
        delete psIter;
        psIter = psNext;
    }
    psHead = nullptr;
    psTail = nullptr;
    oMapLinkedDatasets.clear();
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

void PLMosaicDataset::FlushCache(bool bAtClosing)
{
    FlushDatasetsCache();

    nLastMetaTileX = -1;
    nLastMetaTileY = -1;
    if( poLastItemsInformation )
        json_object_put(poLastItemsInformation);
    poLastItemsInformation = nullptr;
    osLastRetGetLocationInfo.clear();

    GDALDataset::FlushCache(bAtClosing);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int PLMosaicDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "PLMOSAIC:");
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                         */
/************************************************************************/

char** PLMosaicDataset::GetBaseHTTPOptions()
{
    bMustCleanPersistent = TRUE;

    char** papszOptions
        = CSLAddString(nullptr, CPLSPrintf("PERSISTENT=PLMOSAIC:%p", this));
    /* Use basic auth, rather than Authorization headers since curl would forward it to S3 */
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("USERPWD=%s:", osAPIKey.c_str()));

    return papszOptions;
}

/************************************************************************/
/*                               Download()                             */
/************************************************************************/

CPLHTTPResult* PLMosaicDataset::Download(const char* pszURL,
                                         int bQuiet404Error)
{
    char** papszOptions = CSLAddString(GetBaseHTTPOptions(), nullptr);
    CPLHTTPResult *psResult = nullptr;
    if( STARTS_WITH(osBaseURL, "/vsimem/") &&
        STARTS_WITH(pszURL, "/vsimem/") )
    {
        CPLDebug("PLSCENES", "Fetching %s", pszURL);
        psResult = reinterpret_cast<CPLHTTPResult *>(
            CPLCalloc( 1, sizeof( CPLHTTPResult ) ) );
        vsi_l_offset nDataLength = 0;
        CPLString osURL(pszURL);
        if( osURL.back() == '/' )
            osURL.resize(osURL.size()-1);
        GByte* pabyBuf = VSIGetMemFileBuffer(osURL, &nDataLength, FALSE);
        if( pabyBuf )
        {
            psResult->pabyData = reinterpret_cast<GByte *>(
                VSIMalloc(1 + static_cast<size_t>( nDataLength ) ) );
            if( psResult->pabyData )
            {
                memcpy(psResult->pabyData, pabyBuf, static_cast<size_t>( nDataLength ) );
                psResult->pabyData[nDataLength] = 0;
                psResult->nDataLen = static_cast<int>( nDataLength );
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

    if( psResult->pszErrBuf != nullptr )
    {
        if( !(bQuiet404Error && strstr(psResult->pszErrBuf, "404")) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "%s",
                      psResult->pabyData ? reinterpret_cast<const char*>(
                          psResult->pabyData ) :
                      psResult->pszErrBuf );
        }
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    return psResult;
}

/************************************************************************/
/*                               RunRequest()                           */
/************************************************************************/

json_object* PLMosaicDataset::RunRequest(const char* pszURL,
                                         int bQuiet404Error)
{
    CPLHTTPResult * psResult = Download(pszURL, bQuiet404Error);
    if( psResult == nullptr )
    {
        return nullptr;
    }

    json_object* poObj = nullptr;
    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
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
/*                           PLMosaicGetParameter()                     */
/************************************************************************/

static CPLString PLMosaicGetParameter( GDALOpenInfo * poOpenInfo,
                                       char** papszOptions,
                                       const char* pszName,
                                       const char* pszDefaultVal )
{
    return CSLFetchNameValueDef( papszOptions, pszName,
        CSLFetchNameValueDef( poOpenInfo->papszOpenOptions, pszName,
                              pszDefaultVal ));
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PLMosaicDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo) )
        return nullptr;

    PLMosaicDataset* poDS = new PLMosaicDataset();

    poDS->osBaseURL = CPLGetConfigOption("PL_URL", "https://api.planet.com/basemaps/v1/mosaics");

    char** papszOptions = CSLTokenizeStringComplex(
            poOpenInfo->pszFilename+strlen("PLMosaic:"), ",", TRUE, FALSE );
    for( char** papszIter = papszOptions; papszIter && *papszIter; papszIter ++ )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszValue != nullptr )
        {
            if( !EQUAL(pszKey, "api_key") &&
                !EQUAL(pszKey, "mosaic") &&
                !EQUAL(pszKey, "cache_path") &&
                !EQUAL(pszKey, "trust_cache") &&
                !EQUAL(pszKey, "use_tiles") )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unsupported option %s", pszKey);
                CPLFree(pszKey);
                delete poDS;
                CSLDestroy(papszOptions);
                return nullptr;
            }
            CPLFree(pszKey);
        }
    }

    poDS->osAPIKey = PLMosaicGetParameter(poOpenInfo, papszOptions, "api_key",
                                          CPLGetConfigOption("PL_API_KEY",""));

    if( poDS->osAPIKey.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing PL_API_KEY configuration option or API_KEY open option");
        delete poDS;
        CSLDestroy(papszOptions);
        return nullptr;
    }

    poDS->osMosaic = PLMosaicGetParameter(poOpenInfo, papszOptions, "mosaic", "");

    poDS->osCachePathRoot = PLMosaicGetParameter(poOpenInfo, papszOptions, "cache_path",
                                          CPLGetConfigOption("PL_CACHE_PATH",""));

    poDS->bTrustCache = CPLTestBool(PLMosaicGetParameter(
                        poOpenInfo, papszOptions, "trust_cache", "FALSE"));

    poDS->bUseTMSForMain = CPLTestBool(PLMosaicGetParameter(
                        poOpenInfo, papszOptions, "use_tiles", "FALSE"));

    CSLDestroy(papszOptions);
    papszOptions = nullptr;

    if( !poDS->osMosaic.empty() )
    {
        if( !poDS->OpenMosaic() )
        {
            delete poDS;
            poDS = nullptr;
        }
    }
    else
    {
        auto aosNameList = poDS->ListSubdatasets();
        if( aosNameList.empty() )
        {
            delete poDS;
            poDS = nullptr;
        }
        else if( aosNameList.size() == 1 )
        {
            const CPLString osOldFilename(poOpenInfo->pszFilename);
            const CPLString osMosaicConnectionString
                = CPLSPrintf("PLMOSAIC:mosaic=%s", aosNameList[0].c_str());
            delete poDS;
            GDALOpenInfo oOpenInfo(osMosaicConnectionString.c_str(), GA_ReadOnly);
            oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
            poDS = reinterpret_cast<PLMosaicDataset *>( Open(&oOpenInfo) );
            if( poDS )
                poDS->SetDescription(osOldFilename);
        }
        else
        {
            CPLStringList aosSubdatasets;
            for( const auto& osName: aosNameList )
            {
                const int nDatasetIdx = aosSubdatasets.Count() / 2 + 1;
                aosSubdatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nDatasetIdx),
                    CPLSPrintf("PLMOSAIC:mosaic=%s", osName.c_str()));
                aosSubdatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nDatasetIdx),
                    CPLSPrintf("Mosaic %s", osName.c_str()));
            }
            poDS->SetMetadata(aosSubdatasets.List(), "SUBDATASETS");
        }
    }

    if( poDS )
        poDS->SetPamFlags(0);

    return poDS;
}

/************************************************************************/
/*                           ReplaceSubString()                         */
/************************************************************************/

static void ReplaceSubString(CPLString &osTarget,
                             CPLString osPattern,
                             CPLString osReplacement)

{
    // Assumes only one occurrence of osPattern.
    size_t pos = osTarget.find(osPattern);
    if( pos == CPLString::npos )
        return;

    osTarget.replace(pos, osPattern.size(), osReplacement);
}

/************************************************************************/
/*                            GetMosaicCachePath()                      */
/************************************************************************/

CPLString PLMosaicDataset::GetMosaicCachePath()
{
    if( !osCachePathRoot.empty() )
    {
        const CPLString osCachePath(
            CPLFormFilename(osCachePathRoot, "plmosaic_cache", nullptr));
        const CPLString osMosaicPath(
            CPLFormFilename(osCachePath, osMosaic, nullptr));

        return osMosaicPath;
    }
    return "";
}

/************************************************************************/
/*                     CreateMosaicCachePathIfNecessary()               */
/************************************************************************/

void PLMosaicDataset::CreateMosaicCachePathIfNecessary()
{
    if( !osCachePathRoot.empty() )
    {
        const CPLString osCachePath(
            CPLFormFilename(osCachePathRoot, "plmosaic_cache", nullptr));
        const CPLString osMosaicPath(
            CPLFormFilename(osCachePath, osMosaic, nullptr));

        VSIStatBufL sStatBuf;
        if( VSIStatL(osMosaicPath, &sStatBuf) != 0 )
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            VSIMkdir(osCachePathRoot, 0755);
            VSIMkdir(osCachePath, 0755);
            VSIMkdir(osMosaicPath, 0755);
            CPLPopErrorHandler();
        }
    }
}

/************************************************************************/
/*                     LongLatToSphericalMercator()                     */
/************************************************************************/

static void LongLatToSphericalMercator(double* x, double* y)
{
  double X = SPHERICAL_RADIUS * (*x) / 180 * M_PI;
  double Y = SPHERICAL_RADIUS * log( tan(M_PI / 4 + 0.5 * (*y) / 180 * M_PI) );
  *x = X;
  *y = Y;
}

/************************************************************************/
/*                               OpenMosaic()                           */
/************************************************************************/

int PLMosaicDataset::OpenMosaic()
{
    CPLString osURL;

    osURL = osBaseURL;
    if( osURL.back() != '/' )
        osURL += '/';
    char* pszEscaped = CPLEscapeString(osMosaic, -1, CPLES_URL);
    osURL += "?name__is=" + CPLString(pszEscaped);
    CPLFree(pszEscaped);

    json_object* poObj = RunRequest(osURL);
    if( poObj == nullptr )
    {
        return FALSE;
    }

    json_object* poMosaics = CPL_json_object_object_get(poObj, "mosaics");
    json_object* poMosaic = nullptr;
    if( poMosaics == nullptr ||
        json_object_get_type(poMosaics) != json_type_array ||
        json_object_array_length(poMosaics) != 1 ||
        (poMosaic = json_object_array_get_idx(poMosaics, 0)) == nullptr ||
        json_object_get_type(poMosaic) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No mosaic %s", osMosaic.c_str());
        json_object_put(poObj);
        return FALSE;
    }

    json_object* poId = CPL_json_object_object_get(poMosaic, "id");
    json_object* poCoordinateSystem = CPL_json_object_object_get(poMosaic, "coordinate_system");
    json_object* poDataType = CPL_json_object_object_get(poMosaic, "datatype");
    json_object* poQuadSize = json_ex_get_object_by_path(poMosaic, "grid.quad_size");
    json_object* poResolution = json_ex_get_object_by_path(poMosaic, "grid.resolution");
    json_object* poLinks = CPL_json_object_object_get(poMosaic, "_links");
    json_object* poLinksTiles = nullptr;
    json_object* poBBox = CPL_json_object_object_get(poMosaic, "bbox");
    if( poLinks != nullptr && json_object_get_type(poLinks) == json_type_object )
    {
        poLinksTiles = CPL_json_object_object_get(poLinks, "tiles");
    }
    if( poId == nullptr || json_object_get_type(poId) != json_type_string ||
        poCoordinateSystem == nullptr || json_object_get_type(poCoordinateSystem) != json_type_string ||
        poDataType == nullptr || json_object_get_type(poDataType) != json_type_string ||
        poQuadSize == nullptr || json_object_get_type(poQuadSize) != json_type_int ||
        poResolution == nullptr || (json_object_get_type(poResolution) != json_type_int &&
                                 json_object_get_type(poResolution) != json_type_double) )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Missing required parameter");
        json_object_put(poObj);
        return FALSE;
    }

    CPLString osId(json_object_get_string(poId));

    const char* pszSRS = json_object_get_string(poCoordinateSystem);
    if( !EQUAL(pszSRS, "EPSG:3857") )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported coordinate_system = %s",
                 pszSRS);
        json_object_put(poObj);
        return FALSE;
    }

    OGRSpatialReference oSRS;
    oSRS.SetFromUserInput(pszSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get());
    oSRS.exportToWkt(&pszWKT);

    json_object* poQuadDownload = CPL_json_object_object_get(
                                        poMosaic, "quad_download");
    bQuadDownload = CPL_TO_BOOL(json_object_get_boolean(poQuadDownload));

    GDALDataType eDT = GDT_Unknown;
    const char* pszDataType = json_object_get_string(poDataType);
    if( EQUAL(pszDataType, "byte") )
        eDT = GDT_Byte;
    else if( EQUAL(pszDataType, "uint16") )
        eDT = GDT_UInt16;
    else if( EQUAL(pszDataType, "int16") )
        eDT = GDT_Int16;
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data_type = %s",
                 pszDataType);
        json_object_put(poObj);
        return FALSE;
    }

    if( eDT == GDT_Byte && !bQuadDownload )
        bUseTMSForMain = true;

    if( bUseTMSForMain && eDT != GDT_Byte )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot use tile API for full resolution data on non Byte mosaic");
        bUseTMSForMain = FALSE;
    }

    nQuadSize = json_object_get_int(poQuadSize);
    if( nQuadSize <= 0 || (nQuadSize % 256) != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported quad_size = %d",
                 nQuadSize);
        json_object_put(poObj);
        return FALSE;
    }

    const double dfResolution = json_object_get_double(poResolution);
    if( EQUAL(pszSRS, "EPSG:3857") )
    {
        double dfZoomLevel = log(GM_ZOOM_0 / dfResolution)/log(2.0);
        nZoomLevelMax = static_cast<int>( dfZoomLevel + 0.1 );
        if( fabs(dfZoomLevel - nZoomLevelMax) > 1e-5 )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported resolution = %.12g",
                    dfResolution);
            json_object_put(poObj);
            return FALSE;
        }

        bHasGeoTransform = TRUE;
        adfGeoTransform[0] = GM_ORIGIN;
        adfGeoTransform[1] = dfResolution;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = -GM_ORIGIN;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = -dfResolution;
        nRasterXSize = static_cast<int>( 2 * -GM_ORIGIN / dfResolution + 0.5 );
        nRasterYSize = nRasterXSize;

        if( poBBox != nullptr &&
            json_object_get_type(poBBox) == json_type_array &&
            json_object_array_length(poBBox) == 4 )
        {
            double xmin =
                json_object_get_double(json_object_array_get_idx(poBBox, 0));
            double ymin =
                json_object_get_double(json_object_array_get_idx(poBBox, 1));
            double xmax =
                json_object_get_double(json_object_array_get_idx(poBBox, 2));
            double ymax =
                json_object_get_double(json_object_array_get_idx(poBBox, 3));
            LongLatToSphericalMercator(&xmin, &ymin);
            LongLatToSphericalMercator(&xmax, &ymax);
            xmin = std::max(xmin, GM_ORIGIN);
            ymin = std::max(ymin, GM_ORIGIN);
            xmax = std::min(xmax, -GM_ORIGIN);
            ymax = std::min(ymax, -GM_ORIGIN);

            double dfTileSize = dfResolution * nQuadSize;
            xmin = floor(xmin / dfTileSize) * dfTileSize;
            ymin = floor(ymin / dfTileSize) * dfTileSize;
            xmax = ceil(xmax / dfTileSize) * dfTileSize;
            ymax = ceil(ymax / dfTileSize) * dfTileSize;
            adfGeoTransform[0] = xmin;
            adfGeoTransform[3] = ymax;
            nRasterXSize = static_cast<int>((xmax - xmin) / dfResolution + 0.5);
            nRasterYSize = static_cast<int>((ymax - ymin) / dfResolution + 0.5);
            nMetaTileXShift = static_cast<int>((xmin - GM_ORIGIN) / dfTileSize + 0.5);
            nMetaTileYShift = static_cast<int>((ymin - GM_ORIGIN) / dfTileSize + 0.5);
        }
    }

    osQuadsURL = osBaseURL;
    if( osQuadsURL.back() != '/' )
        osQuadsURL += '/';
    osQuadsURL += osId + "/quads/";

    // Use WMS/TMS driver for overviews (only for byte)
    if( eDT == GDT_Byte && EQUAL(pszSRS, "EPSG:3857") &&
        poLinksTiles != nullptr &&
        json_object_get_type(poLinksTiles) == json_type_string )
    {
        const char* pszLinksTiles = json_object_get_string(poLinksTiles);
        if( strstr(pszLinksTiles, "{x}") == nullptr ||
            strstr(pszLinksTiles, "{y}") == nullptr ||
            strstr(pszLinksTiles, "{z}") == nullptr )
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Invalid _links.tiles = %s",
                     pszLinksTiles);
        }
        else
        {
            CPLString osCacheStr;
            if( !osCachePathRoot.empty() )
            {
                osCacheStr = "    <Cache><Path>";
                osCacheStr += GetMosaicCachePath();
                osCacheStr += "</Path><Unique>False</Unique></Cache>\n";
            }

            CPLString osTMSURL(pszLinksTiles);
            ReplaceSubString(osTMSURL, "{x}", "${x}");
            ReplaceSubString(osTMSURL, "{y}", "${y}");
            ReplaceSubString(osTMSURL, "{z}", "${z}");
            ReplaceSubString(osTMSURL, "{0-3}", "0");

            for( int nZoomLevel = nZoomLevelMax; nZoomLevel >= 0;
                     nZoomLevel -- )
            {
                const int nZShift = nZoomLevelMax - nZoomLevel;
                int nOvrXSize = nRasterXSize >> nZShift;
                int nOvrYSize = nRasterYSize >> nZShift;
                if( nOvrXSize == 0 || nOvrYSize == 0 )
                    break;

                CPLString osTMS = CPLSPrintf(
    "<GDAL_WMS>\n"
    "    <Service name=\"TMS\">\n"
    "        <ServerUrl>%s</ServerUrl>\n"
    "    </Service>\n"
    "    <DataWindow>\n"
    "        <UpperLeftX>%.16g</UpperLeftX>\n"
    "        <UpperLeftY>%.16g</UpperLeftY>\n"
    "        <LowerRightX>%.16g</LowerRightX>\n"
    "        <LowerRightY>%.16g</LowerRightY>\n"
    "        <SizeX>%d</SizeX>\n"
    "        <SizeY>%d</SizeY>\n"
    "        <TileLevel>%d</TileLevel>\n"
    "        <YOrigin>top</YOrigin>\n"
    "    </DataWindow>\n"
    "    <Projection>%s</Projection>\n"
    "    <BlockSizeX>256</BlockSizeX>\n"
    "    <BlockSizeY>256</BlockSizeY>\n"
    "    <BandsCount>4</BandsCount>\n"
    "%s"
    "</GDAL_WMS>",
                    osTMSURL.c_str(),
                    GM_ORIGIN,
                    -GM_ORIGIN,
                    -GM_ORIGIN,
                    GM_ORIGIN,
                    256 << nZoomLevel,
                    256 << nZoomLevel,
                    nZoomLevel,
                    pszSRS,
                    osCacheStr.c_str());

                GDALDataset* poTMSDS = reinterpret_cast<GDALDataset *>(
                        GDALOpenEx( osTMS, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                nullptr, nullptr, nullptr ) );
                if( poTMSDS )
                {
                    double dfThisResolution = dfResolution * (1 << nZShift);

                    VRTDatasetH hVRTDS = VRTCreate(nOvrXSize, nOvrYSize);
                    for(int iBand=1;iBand<=4;iBand++)
                    {
                        VRTAddBand( hVRTDS, GDT_Byte, nullptr );
                    }

                    int nSrcXOff, nSrcYOff, nDstXOff, nDstYOff;

                    nSrcXOff = static_cast<int>(0.5 +
                        (adfGeoTransform[0] - GM_ORIGIN) / dfThisResolution);
                    nDstXOff = 0;

                    nSrcYOff = static_cast<int>(0.5 +
                        (-GM_ORIGIN - adfGeoTransform[3]) / dfThisResolution);
                    nDstYOff = 0;

                    for(int iBand=1;iBand<=4;iBand++)
                    {
                        VRTSourcedRasterBandH hVRTBand =
                            reinterpret_cast<VRTSourcedRasterBandH>(
                                GDALGetRasterBand(hVRTDS, iBand));
                        VRTAddSimpleSource(
                            hVRTBand, GDALGetRasterBand(poTMSDS, iBand),
                            nSrcXOff, nSrcYOff, nOvrXSize, nOvrYSize,
                            nDstXOff, nDstYOff, nOvrXSize, nOvrYSize,
                            "NEAR", VRT_NODATA_UNSET);
                    }
                    poTMSDS->Dereference();

                    apoTMSDS.push_back( reinterpret_cast<GDALDataset*>(hVRTDS) );
                }

                if( nOvrXSize < 256 && nOvrYSize < 256 )
                    break;
            }
        }
    }

    if( bUseTMSForMain && apoTMSDS.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot find tile definition, so use_tiles will be ignored");
        bUseTMSForMain = FALSE;
    }

    for(int i=0;i<4;i++)
        SetBand(i + 1, new PLMosaicRasterBand(this, i + 1, eDT));

    json_object* poFirstAcquired = CPL_json_object_object_get(poMosaic, "first_acquired");
    if( poFirstAcquired != nullptr && json_object_get_type(poFirstAcquired) == json_type_string )
    {
        SetMetadataItem("FIRST_ACQUIRED",
                                 json_object_get_string(poFirstAcquired));
    }
    json_object* poLastAcquired = CPL_json_object_object_get(poMosaic, "last_acquired");
    if( poLastAcquired != nullptr && json_object_get_type(poLastAcquired) == json_type_string )
    {
        SetMetadataItem("LAST_ACQUIRED",
                                 json_object_get_string(poLastAcquired));
    }
    json_object* poName = CPL_json_object_object_get(poMosaic, "name");
    if( poName != nullptr && json_object_get_type(poName) == json_type_string )
    {
        SetMetadataItem("NAME", json_object_get_string(poName));
    }

    json_object_put(poObj);
    return TRUE;
}

/************************************************************************/
/*                          ListSubdatasets()                           */
/************************************************************************/

std::vector<CPLString> PLMosaicDataset::ListSubdatasets()
{
    std::vector<CPLString> aosNameList;
    CPLString osURL(osBaseURL);
    while(osURL.size())
    {
        json_object* poObj = RunRequest(osURL);
        if( poObj == nullptr )
        {
            return aosNameList;
        }

        osURL = "";
        json_object* poLinks = CPL_json_object_object_get(poObj, "_links");
        if( poLinks != nullptr && json_object_get_type(poLinks) == json_type_object )
        {
            json_object* poNext = CPL_json_object_object_get(poLinks, "_next");
            if( poNext != nullptr && json_object_get_type(poNext) == json_type_string )
            {
                osURL = json_object_get_string(poNext);
            }
        }

        json_object* poMosaics = CPL_json_object_object_get(poObj, "mosaics");
        if( poMosaics == nullptr || json_object_get_type(poMosaics) != json_type_array )
        {
            json_object_put(poObj);
            return aosNameList;
        }

        const auto nMosaics = json_object_array_length(poMosaics);
        for(auto i=decltype(nMosaics){0};i< nMosaics;i++)
        {
            const char* pszName = nullptr;
            const char* pszCoordinateSystem = nullptr;
            json_object* poMosaic = json_object_array_get_idx(poMosaics, i);
            bool bAccessible = false;
            if( poMosaic && json_object_get_type(poMosaic) == json_type_object )
            {
                json_object* poName = CPL_json_object_object_get(poMosaic, "name");
                if( poName != nullptr && json_object_get_type(poName) == json_type_string )
                {
                    pszName = json_object_get_string(poName);
                }

                json_object* poCoordinateSystem = CPL_json_object_object_get(poMosaic, "coordinate_system");
                if( poCoordinateSystem && json_object_get_type(poCoordinateSystem) == json_type_string )
                {
                    pszCoordinateSystem = json_object_get_string(poCoordinateSystem);
                }

                json_object* poDataType = CPL_json_object_object_get(poMosaic, "datatype");
                if( poDataType && json_object_get_type(poDataType) == json_type_string &&
                    EQUAL(json_object_get_string(poDataType), "byte") &&
                    !CSLTestBoolean(CPLGetConfigOption("PL_MOSAIC_LIST_QUAD_DOWNLOAD_ONLY", "NO")) )
                {
                    bAccessible = true; // through tile API
                }
                else
                {
                    json_object* poQuadDownload = CPL_json_object_object_get(
                                                    poMosaic, "quad_download");
                    bAccessible = CPL_TO_BOOL(
                        json_object_get_boolean(poQuadDownload));
                }
            }

            if( bAccessible && pszName && pszCoordinateSystem &&
                EQUAL(pszCoordinateSystem, "EPSG:3857") )
            {
                aosNameList.push_back(pszName);
            }
        }

        json_object_put(poObj);
    }
    return aosNameList;
}

/************************************************************************/
/*                            GetProjectionRef()                       */
/************************************************************************/

const char* PLMosaicDataset::_GetProjectionRef()
{
    return (pszWKT) ? pszWKT : "";
}

/************************************************************************/
/*                            GetGeoTransform()                         */
/************************************************************************/

CPLErr PLMosaicDataset::GetGeoTransform(double* padfGeoTransform)
{
    memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
    return ( bHasGeoTransform ) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                          formatTileName()                            */
/************************************************************************/

CPLString PLMosaicDataset::formatTileName(int tile_x, int tile_y)

{
    return CPLSPrintf("%d-%d", tile_x, tile_y);
}

/************************************************************************/
/*                          InsertNewDataset()                          */
/************************************************************************/

void PLMosaicDataset::InsertNewDataset(CPLString osKey, GDALDataset* poDS)
{
    if( static_cast<int>( oMapLinkedDatasets.size() ) == nCacheMaxSize )
    {
        CPLDebug("PLMOSAIC", "Discarding older entry %s from cache",
                 psTail->osKey.c_str());
        oMapLinkedDatasets.erase(psTail->osKey);
        PLLinkedDataset* psNewTail = psTail->psPrev;
        psNewTail->psNext = nullptr;
        if( psTail->poDS )
            GDALClose( psTail->poDS );
        delete psTail;
        psTail = psNewTail;
    }

    PLLinkedDataset* psLinkedDataset = new PLLinkedDataset();
    if( psHead )
        psHead->psPrev = psLinkedDataset;
    psLinkedDataset->osKey = osKey;
    psLinkedDataset->psNext = psHead;
    psLinkedDataset->poDS = poDS;
    psHead = psLinkedDataset;
    if( psTail == nullptr )
        psTail = psHead;
    oMapLinkedDatasets[osKey] = psLinkedDataset;
}

/************************************************************************/
/*                         OpenAndInsertNewDataset()                    */
/************************************************************************/

GDALDataset* PLMosaicDataset::OpenAndInsertNewDataset(CPLString osTmpFilename,
                                                      CPLString osTilename)
{
    const char* const apszAllowedDrivers[2] = { "GTiff", nullptr };
    GDALDataset* poDS = reinterpret_cast<GDALDataset *>(
        GDALOpenEx( osTmpFilename, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszAllowedDrivers, nullptr, nullptr ) );
    if( poDS != nullptr )
    {
        if( poDS->GetRasterXSize() != nQuadSize ||
            poDS->GetRasterYSize() != nQuadSize ||
            poDS->GetRasterCount() != 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent metatile characteristics");
            GDALClose(poDS);
            poDS = nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid GTiff dataset: %s",
                 osTilename.c_str());
    }

    InsertNewDataset(osTilename, poDS);
    return poDS;
}

/************************************************************************/
/*                            GetMetaTile()                             */
/************************************************************************/

GDALDataset* PLMosaicDataset::GetMetaTile(int tile_x, int tile_y)
{
    const CPLString osTilename = formatTileName(tile_x, tile_y);
    std::map<CPLString,PLLinkedDataset*>::const_iterator it =
                                                    oMapLinkedDatasets.find(osTilename);
    if( it == oMapLinkedDatasets.end() )
    {
        CPLString osTmpFilename;

        const CPLString osMosaicPath(GetMosaicCachePath());
        osTmpFilename = CPLFormFilename(osMosaicPath,
                CPLSPrintf("%s_%s.tif", osMosaic.c_str(), CPLGetFilename(osTilename)), nullptr);
        VSIStatBufL sStatBuf;

        CPLString osURL = osQuadsURL;
        osURL += osTilename;
        osURL += "/full";

        if( !osCachePathRoot.empty() && VSIStatL(osTmpFilename, &sStatBuf) == 0 )
        {
            if( bTrustCache )
            {
                return OpenAndInsertNewDataset(osTmpFilename, osTilename);
            }

            CPLDebug("PLMOSAIC", "File %s exists. Checking if it is up-to-date...",
                     osTmpFilename.c_str());
            // Currently we only check by file size, which should be good enough
            // as the metatiles are compressed, so a change in content is likely
            // to cause a change in filesize. Use of a signature would be better
            // though if available in the metadata
            VSIStatBufL sRemoteTileStatBuf;
            char* pszEscapedURL = CPLEscapeString(
                (osURL + "?api_key=" + osAPIKey).c_str(), -1, CPLES_URL );
            CPLString osVSICURLUrl(
                STARTS_WITH(osURL, "/vsimem/") ? osURL :
                    "/vsicurl?use_head=no&url=" + CPLString(pszEscapedURL));
            CPLFree(pszEscapedURL);
            if( VSIStatL(osVSICURLUrl, &sRemoteTileStatBuf) == 0 &&
                sRemoteTileStatBuf.st_size == sStatBuf.st_size )
            {
                CPLDebug("PLMOSAIC", "Cached tile is up-to-date");
                return OpenAndInsertNewDataset(osTmpFilename, osTilename);
            }
            else
            {
                CPLDebug("PLMOSAIC", "Cached tile is not up-to-date");
                VSIUnlink(osTmpFilename);
            }
        }

        // Fetch the GeoTIFF now

        CPLHTTPResult* psResult = Download(osURL, TRUE);
        if( psResult == nullptr )
        {
            InsertNewDataset(osTilename, nullptr);
            return nullptr;
        }

        CreateMosaicCachePathIfNecessary();

        VSILFILE* fp = osCachePathRoot.size() ? VSIFOpenL(osTmpFilename, "wb") : nullptr;
        if( fp )
        {
            VSIFWriteL(psResult->pabyData, 1, psResult->nDataLen, fp);
            VSIFCloseL(fp);
        }
        else
        {
            // In case there's no temporary path or it is not writable
            // use a in-memory dataset, and limit the cache to only one
            if( !osCachePathRoot.empty() && nCacheMaxSize > 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot write into %s. Using /vsimem and reduce cache to 1 entry",
                         osCachePathRoot.c_str());
                FlushDatasetsCache();
                nCacheMaxSize = 1;
            }
            osTmpFilename =
                CPLSPrintf("/vsimem/single_tile_plmosaic_cache/%s/%d_%d.tif",
                           osMosaic.c_str(), tile_x, tile_y);
            fp = VSIFOpenL(osTmpFilename, "wb");
            if( fp )
            {
                VSIFWriteL(psResult->pabyData, 1, psResult->nDataLen, fp);
                VSIFCloseL(fp);
            }
        }
        CPLHTTPDestroyResult(psResult);
        GDALDataset* poDS = OpenAndInsertNewDataset(osTmpFilename, osTilename);

        if( STARTS_WITH(osTmpFilename, "/vsimem/single_tile_plmosaic_cache/") )
            VSIUnlink(osTilename);

        return poDS;
    }

    // Move link to head of MRU list
    PLLinkedDataset* psLinkedDataset = it->second;
    GDALDataset* poDS = psLinkedDataset->poDS;
    if( psLinkedDataset != psHead )
    {
        if( psLinkedDataset == psTail )
            psTail = psLinkedDataset->psPrev;
        if( psLinkedDataset->psPrev )
            psLinkedDataset->psPrev->psNext = psLinkedDataset->psNext;
        if( psLinkedDataset->psNext )
            psLinkedDataset->psNext->psPrev = psLinkedDataset->psPrev;
        psLinkedDataset->psNext = psHead;
        psLinkedDataset->psPrev = nullptr;
        psHead->psPrev = psLinkedDataset;
        psHead = psLinkedDataset;
    }

    return poDS;
}

/************************************************************************/
/*                         GetLocationInfo()                            */
/************************************************************************/

const char* PLMosaicDataset::GetLocationInfo(int nPixel, int nLine)
{
    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    const int nBlockXOff = nPixel / nBlockXSize;
    const int nBlockYOff = nLine / nBlockYSize;
    const int bottom_yblock = (nRasterYSize - nBlockYOff * nBlockYSize) / nBlockYSize - 1;

    const int meta_tile_x = nMetaTileXShift + (nBlockXOff * nBlockXSize) / nQuadSize;
    const int meta_tile_y = nMetaTileYShift + (bottom_yblock * nBlockYSize) / nQuadSize;

    CPLString osQuadURL = osQuadsURL;
    CPLString osTilename = formatTileName(meta_tile_x, meta_tile_y);
    osQuadURL += osTilename;

    if( meta_tile_x != nLastMetaTileX || meta_tile_y != nLastMetaTileY )
    {
        const CPLString osQuadScenesURL = osQuadURL + "/items";

        json_object_put(poLastItemsInformation);
        poLastItemsInformation = RunRequest(osQuadScenesURL, TRUE);

        nLastMetaTileX = meta_tile_x;
        nLastMetaTileY = meta_tile_y;
    }

    osLastRetGetLocationInfo.clear();

    CPLXMLNode* psRoot = CPLCreateXMLNode(nullptr, CXT_Element, "LocationInfo");

    if( poLastItemsInformation )
    {
        json_object* poItems = CPL_json_object_object_get(poLastItemsInformation, "items");
        if( poItems && json_object_get_type(poItems) == json_type_array &&
            json_object_array_length(poItems) != 0 )
        {
            CPLXMLNode* psScenes =
                CPLCreateXMLNode(psRoot, CXT_Element, "Scenes");
            const auto nItemsLength = json_object_array_length(poItems);
            for(auto i = decltype(nItemsLength){0}; i < nItemsLength; i++ )
            {
                json_object* poObj = json_object_array_get_idx(poItems, i);
                if ( poObj && json_object_get_type(poObj) == json_type_object )
                {
                    json_object* poLink = CPL_json_object_object_get(poObj, "link");
                    if( poLink )
                    {
                        CPLXMLNode* psScene = CPLCreateXMLNode(psScenes, CXT_Element, "Scene");
                        CPLXMLNode* psItem = CPLCreateXMLNode(psScene,
                                CXT_Element, "link");
                        CPLCreateXMLNode(psItem, CXT_Text, json_object_get_string(poLink));
                    }
                }
            }
        }
    }

    char* pszXML = CPLSerializeXMLTree(psRoot);
    CPLDestroyXMLNode(psRoot);
    osLastRetGetLocationInfo = pszXML;
    CPLFree(pszXML);

    return osLastRetGetLocationInfo.c_str();
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr  PLMosaicDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( bUseTMSForMain && !apoTMSDS.empty() )
        return apoTMSDS[0]->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType, nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg );

    return BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                               pData, nBufXSize, nBufYSize,
                               eBufType, nBandCount, panBandMap,
                               nPixelSpace, nLineSpace, nBandSpace,
                               psExtraArg );
}

/************************************************************************/
/*                      GDALRegister_PLMOSAIC()                         */
/************************************************************************/

void GDALRegister_PLMOSAIC()

{
    if( GDALGetDriverByName( "PLMOSAIC" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PLMOSAIC" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Planet Labs Mosaics API" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/plmosaic.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "PLMOSAIC:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='API_KEY' type='string' description='Account API key' required='true'/>"
"  <Option name='MOSAIC' type='string' description='Mosaic name'/>"
"  <Option name='CACHE_PATH' type='string' description='Directory where to put cached quads'/>"
"  <Option name='TRUST_CACHE' type='boolean' description='Whether already cached quads should be trusted as the most recent version' default='NO'/>"
"  <Option name='USE_TILES' type='boolean' description='Whether to use the tile API even for full resolution data (only for Byte mosaics)' default='NO'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = PLMosaicDataset::Identify;
    poDriver->pfnOpen = PLMosaicDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
