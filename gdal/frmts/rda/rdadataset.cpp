/******************************************************************************
 *
 * Project:  RDA driver
 * Purpose:  RDA driver
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2017, DigitalGlobe
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
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "gdal_mdreader.h"
#include "ogr_spatialref.h"
#include "cpl_mem_cache.h"

#include "ogrgeojsonreader.h"

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

using TileCacheType = lru11::Cache<std::string,
                                   std::shared_ptr<GDALDataset>, std::mutex>;

static TileCacheType* gpoTileCache = nullptr;

/************************************************************************/
/*                          GetTileCache()                              */
/************************************************************************/

static TileCacheType* GetTileCache()
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> guard(mutex);
    if( gpoTileCache == nullptr )
        gpoTileCache = new TileCacheType(8,0);
    return gpoTileCache;
}

/************************************************************************/
/*                        GDALRDADriverUnload()                         */
/************************************************************************/

static void GDALRDADriverUnload(GDALDriver*)
{
    if( gpoTileCache )
        delete gpoTileCache;
    gpoTileCache = nullptr;
}

/************************************************************************/
/*                          GDALRDADataset                              */
/************************************************************************/

class GDALRDADataset: public GDALDataset
{
        friend class GDALRDARasterBand;

        CPLString m_osAuthURL;
        CPLString m_osRDAAPIURL;
        CPLString m_osClientId;
        CPLString m_osClientSecret;
        CPLString m_osUserName;
        CPLString m_osUserPassword;

        CPLString m_osAccessToken;
        int       m_nExpiresIn = 0;

        CPLString m_osGraphId;
        CPLString m_osNodeId;
        bool      m_bDeleteOnClose = true;

        CPLString m_osImageId;
        CPLString m_osProfileName;
        CPLString m_osNativeTileFileFormat;
        int64_t   m_nTileXOffset = 0;
        int64_t   m_nTileYOffset = 0;
        int64_t   m_nNumXTiles = 0;
        int64_t   m_nNumYTiles = 0;
        int       m_nTileXSize = 0;
        int       m_nTileYSize = 0;
        int64_t   m_nMinX = 0;
        int64_t   m_nMinY = 0;
        int64_t   m_nMaxX = 0;
        int64_t   m_nMaxY = 0;
        int64_t   m_nMinTileX = 0;
        int64_t   m_nMinTileY = 0;
        int64_t   m_nMaxTileX = 0;
        int64_t   m_nMaxTileY = 0;
        CPLString m_osColorInterpretation;
        GDALDataType m_eDT = GDT_Unknown;

        CPLString m_osTileCacheDir;

        bool      m_bTriedReadGeoreferencing = false;
        CPLString m_osWKT;
        bool      m_bGotGeoTransform = false;
        std::array<double,6> m_adfGeoTransform{{ 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 }};
        bool      m_bTriedReadRPC = false;

        int       m_nXOffAdvise = 0;
        int       m_nYOffAdvise = 0;
        int       m_nXSizeAdvise = 0;
        int       m_nYSizeAdvise = 0;

        int       m_nXOffFetched = 0;
        int       m_nYOffFetched = 0;
        int       m_nXSizeFetched = 0;
        int       m_nYSizeFetched = 0;

        bool      ReadConfiguration();
        bool      GetAuthorization();
        bool      ParseAuthorizationResponse(const CPLString& osAuth);
        bool      ParseConnectionString( GDALOpenInfo* poOpenInfo );
        json_object* ReadJSonFile(const char* pszFilename);
        bool      ReadImageMetadata();
        bool      ReadRPCs();
        bool      ReadGeoreferencing();
        std::vector<std::shared_ptr<GDALDataset>>
                GetTiles(
                    const std::vector<std::pair<int64_t,int64_t>>& aTileIdx);
        CPLString GetDatasetCacheDir();
        static void  CacheFile( const CPLString& osCachedFilename,
                             const void* pData, size_t nDataLen );

        char**    GetHTTPOptions();
        GByte*    Download(const CPLString& osURL);
        bool      Open( GDALOpenInfo* poOpenInfo );

        std::string MakeKeyCache(int64_t nTileX, int64_t nTileY);
        void      BatchFetch(int nXOff, int nYOff, int nXSize, int nYSize);

    public:
        GDALRDADataset();
        ~GDALRDADataset();

        static int Identify( GDALOpenInfo* poOpenInfo );
        static GDALDataset* OpenStatic( GDALOpenInfo* poOpenInfo );

        CPLErr          GetGeoTransform(double *padfTransform) override;
        const char*     GetProjectionRef() override;
        CPLErr          IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      int nBandCount, int* panBands,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GSpacing nBandSpace,
                                      GDALRasterIOExtraArg *psExtraArg) override;
        CPLErr          AdviseRead (int nXOff, int nYOff,
                                        int nXSize, int nYSize, 
                                        int /* nBufXSize */,
                                        int /* nBufYSize */,
                                        GDALDataType /* eBufType */,
                                        int /*nBands*/, int* /*panBands*/,
                                        char ** /* papszOptions */) override;
        char**          GetMetadata( const char* pszDomain = "" ) override;
};


/************************************************************************/
/*                         GDALRDARasterBand                            */
/************************************************************************/

class GDALRDARasterBand: public GDALRasterBand
{
    public:
        GDALRDARasterBand( GDALRDADataset* poDS, int nBand);

        CPLErr          IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void* pData) override;
        CPLErr          IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg) override;
        CPLErr          AdviseRead (int nXOff, int nYOff,
                                        int nXSize, int nYSize, 
                                        int /* nBufXSize */,
                                        int /* nBufYSize */,
                                        GDALDataType /* eBufType */, 
                                        char ** /* papszOptions */) override;
        GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                             GetCacheDir()                            */
/************************************************************************/

static CPLString GetCacheDir()
{
    CPLString osDir = CPLGetConfigOption("RDA_CACHE_DIR", "");
    if( osDir.empty() )
    {
        const char* pszHome = CPLGetHomeDir();
        osDir = CPLFormFilename(pszHome, ".gdal", nullptr);
        osDir = CPLFormFilename(osDir, "rda_cache", nullptr);
    }
    if( !osDir.empty() )
    {
        VSIMkdirRecursive(osDir, 0755);
    }
    return osDir;
}

/************************************************************************/
/*                          GDALRDADataset()                            */
/************************************************************************/

GDALRDADataset::GDALRDADataset() :
    m_osAuthURL(CPLGetConfigOption("GBDX_AUTH_URL",
                        "https://geobigdata.io/auth/v1/oauth/token/")),
    m_osRDAAPIURL(CPLGetConfigOption("GBDX_RDA_API_URL",
                        "https://idahoapi.geobigdata.io/v1")),
    m_osClientId(CPLGetConfigOption("GBDX_CLIENT_ID", "")),
    m_osClientSecret(CPLGetConfigOption("GBDX_CLIENT_SECRET", "")),
    m_osUserName(CPLGetConfigOption("GBDX_USERNAME", "")),
    m_osUserPassword(CPLGetConfigOption("GBDX_PASSWORD", ""))
{
}

/************************************************************************/
/*                         ~GDALRDADataset()                            */
/************************************************************************/

GDALRDADataset::~GDALRDADataset()
{
    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT",
                                   CPLSPrintf("%p", this));
    CPLHTTPMultiFetch( nullptr, 0, 0, papszOptions);
    CSLDestroy(papszOptions);

    if( m_bDeleteOnClose && !m_osTileCacheDir.empty() )
    {
        VSIRmdirRecursive(m_osTileCacheDir);
        char** papszContent = VSIReadDir(CPLGetPath(m_osTileCacheDir));
        int nCount = 0;
        for( char** papszIter = papszContent;
                                papszIter && *papszIter; ++papszIter )
        {
            if( strcmp(*papszIter, ".") == 0 ||
                strcmp(*papszIter, "..") == 0 )
            {
                continue;
            }
            nCount ++;
        }
        if( nCount == 0 )
        {
            VSIRmdir(CPLGetPath(m_osTileCacheDir));
        }
        CSLDestroy(papszContent);
    }

    // We could just evict the tiles of our dataset
    if( gpoTileCache )
        GetTileCache()->clear();
}

/************************************************************************/
/*                        GetDatasetCacheDir()                          */
/************************************************************************/

CPLString GDALRDADataset::GetDatasetCacheDir()
{
    if( m_osTileCacheDir.empty() )
    {
        m_osTileCacheDir = CPLFormFilename(GetCacheDir(),
                                           m_osGraphId, nullptr);
        m_osTileCacheDir = CPLFormFilename(m_osTileCacheDir,
                                           m_osNodeId, nullptr);
    }
    return m_osTileCacheDir;
}

/************************************************************************/
/*                              CacheFile()                             */
/************************************************************************/

void GDALRDADataset::CacheFile( const CPLString& osCachedFilename,
                                  const void* pData, size_t nDataLen )
{
    CPLString osCacheTmpFilename(osCachedFilename + ".tmp");
    VSIMkdirRecursive( CPLGetPath(osCachedFilename), 0755 );
    VSILFILE* fp = VSIFOpenL(osCacheTmpFilename, "wb");
    if( fp )
    {
        VSIFWriteL(pData, 1, nDataLen, fp);
        VSIFCloseL(fp);
        VSIUnlink(osCachedFilename);
        VSIRename(osCacheTmpFilename, osCachedFilename);
    }
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GDALRDADataset::Identify( GDALOpenInfo* poOpenInfo )
{
    return strstr(poOpenInfo->pszFilename, "graph-id") != nullptr &&
           strstr(poOpenInfo->pszFilename, "node-id") != nullptr;
}

/************************************************************************/
/*                           ReadConfiguration()                        */
/************************************************************************/

bool GDALRDADataset::ReadConfiguration()
{
    const char* pszHome = CPLGetHomeDir();
    CPLString osConfigFile(
        CPLGetConfigOption("GDBX_CONFIG_FILE",
            CPLFormFilename(pszHome ? pszHome: "", ".gbdx-config", nullptr)));
    if( !osConfigFile.empty() )
    {
        CSLUniquePtr papszContent(CSLLoad2( osConfigFile, -1, -1, nullptr));
        char** papszIter = papszContent.get();
        bool bInGbdxSection = false;
        while( papszIter && *papszIter )
        {
            const char* pszLine= *papszIter;
            if( pszLine[0] == '[' )
            {
                bInGbdxSection = strcmp(pszLine, "[gbdx]")  == 0;
            }
            else if( bInGbdxSection )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( strcmp(pszKey, "auth_url") == 0 )
                        m_osAuthURL = pszValue;
                    else if( strcmp(pszKey, "rda_api_url") == 0 ||
                             strcmp(pszKey, "idaho_api_url") == 0 )
                        m_osRDAAPIURL = pszValue;
                    else if( strcmp(pszKey, "client_id") == 0 )
                        m_osClientId = pszValue;
                    else if( strcmp(pszKey, "client_secret") == 0 )
                        m_osClientSecret = pszValue;
                    else if( strcmp(pszKey, "user_name") == 0 )
                        m_osUserName = pszValue;
                    else if( strcmp(pszKey, "user_password") == 0 )
                        m_osUserPassword = pszValue;
                }
                CPLFree(pszKey);
            }
            papszIter ++;
        }
    }

    bool bOK = true;
    if( m_osUserName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing GBDX_USERNAME / user_name");
        bOK = false;
    }
    if( m_osUserPassword.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing GBDX_PASSWORD / user_password");
        bOK = false;
    }
    if( m_osClientId.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing GBDX_CLIENT_ID / client_id");
        bOK = false;
    }
    if( m_osClientSecret.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing GBDX_CLIENT_SECRET / client_secret");
        bOK = false;
    }
    if( !bOK )
        return false;

    if( m_osAuthURL.find('\\') != std::string::npos )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GBDX_AUTH_URL / auth_url contains an unexpected escape "
                 "character '\\'");
    }
    if( m_osRDAAPIURL.find('\\') != std::string::npos )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GBDX_RDA_API_URL / rda_url contains an unexpected escape "
                 "character '\\'");
    }
    if( m_osClientId.find('\\') != std::string::npos )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GBDX_CLIENT_ID / client_id contains an unexpected escape "
                 "character '\\'");
    }
    if( m_osClientSecret.find('\\') != std::string::npos )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GBDX_CLIENT_SECRET / client_secret contains an "
                 "unexpected escape "
                 "character '\\'");
    }
#ifdef DEBUG_VERBOSE
    CPLDebug("RDA",
             "Using\n"
             "      GBDX_AUTH_URL=%s\n"
             "      GBDX_RDA_API_URL=%s\n"
             "      GBDX_USERNAME=%s\n"
             "      GBDX_PASSWORD=%s\n"
             "      GBDX_CLIENT_ID=%s\n"
             "      GBDX_CLIENT_SECRET=%s",
             m_osAuthURL.c_str(),
             m_osRDAAPIURL.c_str(),
             m_osUserName.c_str(),
             m_osUserPassword.c_str(),
             m_osClientId.c_str(),
             m_osClientSecret.c_str());
#endif

    return true;
}

/************************************************************************/
/*                             URLEscape()                              */
/************************************************************************/

static CPLString URLEscape(const CPLString& osStr)
{
    char* pszEscaped = CPLEscapeString(osStr.c_str(), -1, CPLES_URL);
    CPLString osRet(pszEscaped);
    CPLFree(pszEscaped);
    return osRet;
}

/************************************************************************/
/*                           GetAuthorization()                         */
/************************************************************************/

bool GDALRDADataset::GetAuthorization()
{
    CPLString osAuthCachedFile =
        CPLFormFilename(GetCacheDir(), "authorization.json", nullptr);
    VSIStatBufL sStat;
    if( VSIStatL(osAuthCachedFile, &sStat) == 0 && sStat.st_size < 10000 &&
        CPLTestBool(CPLGetConfigOption("RDA_USE_CACHED_AUTH", "YES")) )
    {
        char* pszAuthContent = static_cast<char*>(
            CPLCalloc(1, static_cast<size_t>(sStat.st_size) + 1));
        VSILFILE* fp = VSIFOpenL(osAuthCachedFile, "rb");
        if( fp )
        {
            VSIFReadL(pszAuthContent, 1,
                      static_cast<size_t>(sStat.st_size), fp);
            VSIFCloseL(fp);
        }
        if( ParseAuthorizationResponse(pszAuthContent) )
        {
            if( m_nExpiresIn <= 0 ||
                time(nullptr) + 60 > sStat.st_mtime + m_nExpiresIn )
            {
                m_osAccessToken.clear();
                VSIUnlink(osAuthCachedFile);
            }
            else
            {
                CPLDebug("RDA", "Reusing cached authorization");
            }
        }
        CPLFree(pszAuthContent);
        if( !m_osAccessToken.empty() )
            return true;
    }

    CPLString osPostContent;
    osPostContent += "grant_type=password&username=" + URLEscape(m_osUserName);
    osPostContent += "&password=" + URLEscape(m_osUserPassword);

    CPLString osKeyStr( m_osClientId + ":" + m_osClientSecret );
    char* pszB64 = CPLBase64Encode( static_cast<int>(osKeyStr.size()),
                        reinterpret_cast<const GByte*>(osKeyStr.c_str()));
    CPLString osAuthorization( "Authorization: Basic " );
    osAuthorization += pszB64;
    CPLFree(pszB64);

    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS",
                                   osPostContent.c_str());
    CPLString osHeaders("Content-Type: application/x-www-form-urlencoded");
    osHeaders += "\n";
    osHeaders += osAuthorization;
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders.c_str());
    CPLHTTPResult* psResult = CPLHTTPFetch( m_osAuthURL, papszOptions);
    CSLDestroy(papszOptions);

    if( psResult->pszErrBuf != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Authorization request failed: %s",
                    psResult->pabyData ? reinterpret_cast<const char*>(
                        psResult->pabyData ) :
                    psResult->pszErrBuf );
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Authorization request failed: "
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return false;
    }
    //CPLDebug("RDA", "%s", psResult->pabyData);
    CPLString osAuthorizationResponse(
                        reinterpret_cast<char*>(psResult->pabyData));
    CPLHTTPDestroyResult(psResult);
    if( !ParseAuthorizationResponse(osAuthorizationResponse) )
        return false;

    if( m_nExpiresIn > 0 )
    {
        VSILFILE* fp = VSIFOpenL(osAuthCachedFile, "wb");
        if( fp )
        {
            VSIFWriteL(osAuthorizationResponse,
                       1, osAuthorizationResponse.size(), fp);
            VSIFCloseL(fp);
        }
    }

    return true;
}

/************************************************************************/
/*                    ParseAuthorizationResponse()                      */
/************************************************************************/

bool GDALRDADataset::ParseAuthorizationResponse(const CPLString& osAuth)
{
    json_object* poObj = nullptr;
    if( !OGRJSonParse(osAuth, &poObj, true) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Authorization response is invalid JSon: %s",
                 osAuth.c_str());
        return false;
    }
    JsonObjectUniquePtr oObj(poObj);

    json_object* poAccessToken =
        json_ex_get_object_by_path(poObj, "access_token");
    if( poAccessToken == nullptr ||
        json_object_get_type(poAccessToken) != json_type_string )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find access_token");
        return false;
    }

    m_osAccessToken = json_object_get_string(poAccessToken);

    json_object* poExpiresIn =
        json_ex_get_object_by_path(poObj, "expires_in");
    if( poExpiresIn != nullptr &&
        json_object_get_type(poExpiresIn) == json_type_int )
    {
        m_nExpiresIn = json_object_get_int(poExpiresIn);
    }

    //  refresh_token ?

    return true;
}

/************************************************************************/
/*                      ParseConnectionString()                         */
/************************************************************************/

bool GDALRDADataset::ParseConnectionString( GDALOpenInfo* poOpenInfo )
{
    json_object* poObj = nullptr;
    if( !OGRJSonParse(poOpenInfo->pszFilename, &poObj, true) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid JSon document as dataset name");
        return false;
    }
    JsonObjectUniquePtr oObj(poObj);

    json_object* poGraphId =
        CPL_json_object_object_get(poObj, "graph-id");
    if( poGraphId == nullptr ||
        json_object_get_type(poGraphId) != json_type_string )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing graph-id");
        return false;
    }
    m_osGraphId = json_object_get_string(poGraphId);

    json_object* poNodeId =
        CPL_json_object_object_get(poObj, "node-id");
    if( poNodeId == nullptr ||
        json_object_get_type(poNodeId) != json_type_string )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing node-id");
        return false;
    }
    m_osNodeId = json_object_get_string(poNodeId);

    json_object* poDeleteOnClose = json_ex_get_object_by_path(poObj,
                                                "options.delete-on-close");
    if( poDeleteOnClose &&
        json_object_get_type(poDeleteOnClose) == json_type_boolean )
    {
        m_bDeleteOnClose = CPL_TO_BOOL(
            json_object_get_boolean(poDeleteOnClose));
    }

    return true;
}

/************************************************************************/
/*                             GetHTTPOptions()                         */
/************************************************************************/

char** GDALRDADataset::GetHTTPOptions()
{
    CPLString osAuthorization( "Authorization: Bearer " );
    osAuthorization += m_osAccessToken;

    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS",
                                   osAuthorization.c_str());
    papszOptions = CSLSetNameValue(papszOptions, "PERSISTENT",
                                   CPLSPrintf("%p", this));
    return papszOptions;
}

/************************************************************************/
/*                            Download()                                */
/************************************************************************/

GByte* GDALRDADataset::Download(const CPLString& osURL)
{
    char** papszOptions = GetHTTPOptions();
    const char* pszURL = osURL.c_str();
    CPLHTTPResult** pasResult = CPLHTTPMultiFetch(&pszURL, 1, 0, papszOptions);
    CSLDestroy(papszOptions);
    if( pasResult == nullptr )
        return nullptr;

    CPLHTTPResult* psResult = pasResult[0];
    if( psResult->pszErrBuf != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Get request %s failed: %s",
                   osURL.c_str(),
                    psResult->pabyData ? reinterpret_cast<const char*>(
                        psResult->pabyData ) :
                    psResult->pszErrBuf );
        CPLHTTPDestroyResult(psResult);
        CPLFree(pasResult);
        return nullptr;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Get request %s failed: "
                 "Empty content returned by server",
                 osURL.c_str());
        CPLHTTPDestroyResult(psResult);
        CPLFree(pasResult);
        return nullptr;
    }
    CPLDebug("RDA", "%s", psResult->pabyData);
    GByte* pabyRes = psResult->pabyData;
    psResult->pabyData = nullptr;
    CPLHTTPDestroyResult(psResult);
    CPLFree(pasResult);
    return pabyRes;
}

/************************************************************************/
/*                          GetJsonString()                             */
/************************************************************************/

static CPLString GetJsonString(json_object* poObj, const char* pszPath,
                               bool bVerboseError, bool& bError)
{
    json_object* poVal = json_ex_get_object_by_path(poObj, pszPath);
    if( poVal == nullptr || json_object_get_type(poVal) != json_type_string )
    {
        if( bVerboseError ) 
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find %s of type string", pszPath);
        }
        bError = true;
        return CPLString();
    }
    return json_object_get_string(poVal);
}

/************************************************************************/
/*                          GetJsonInt64()                             */
/************************************************************************/

static GIntBig GetJsonInt64(json_object* poObj, const char* pszPath,
                               bool bVerboseError, bool& bError)
{
    json_object* poVal = json_ex_get_object_by_path(poObj, pszPath);
    if( poVal == nullptr || json_object_get_type(poVal) != json_type_int )
    {
        if( bVerboseError ) 
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find %s of type integer", pszPath);
        }
        bError = true;
        return 0;
    }
    return json_object_get_int64(poVal);
}

/************************************************************************/
/*                          GetJsonDouble()                             */
/************************************************************************/

static double GetJsonDouble(json_object* poObj, const char* pszPath,
                               bool bVerboseError, bool& bError)
{
    json_object* poVal = json_ex_get_object_by_path(poObj, pszPath);
    if( poVal == nullptr ||
        (json_object_get_type(poVal) != json_type_double &&
         json_object_get_type(poVal) != json_type_int) )
    {
        if( bVerboseError ) 
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find %s of type double", pszPath);
        }
        bError = true;
        return 0.0;
    }
    return json_object_get_double(poVal);
}

/************************************************************************/
/*                           ReadJSonFile()                             */
/************************************************************************/

json_object* GDALRDADataset::ReadJSonFile(const char* pszFilename)
{
    CPLString osCachedFilename(
        CPLFormFilename(GetDatasetCacheDir(), pszFilename, nullptr));
    VSIStatBufL sStat;
    char* pszRes = nullptr;
    bool bToCache = false;
    if( VSIStatL(osCachedFilename, &sStat) == 0 && sStat.st_size < 100000 )
    {
        pszRes = static_cast<char*>(CPLCalloc(1,
                                    static_cast<size_t>(sStat.st_size) + 1));
        VSILFILE* fp = VSIFOpenL(osCachedFilename, "rb");
        if( fp )
        {
            VSIFReadL( pszRes, 1, static_cast<size_t>(sStat.st_size), fp );
            VSIFCloseL(fp);
        }
        else
        {
            VSIFree(pszRes);
            pszRes = nullptr;
        }
    }
    if( pszRes == nullptr )
    {
        CPLString osURL(m_osRDAAPIURL);
        osURL += "/metadata/" + m_osGraphId + "/" +
                 m_osNodeId + "/" + pszFilename;
        pszRes = reinterpret_cast<char*>(Download(osURL));
        bToCache = true;
    }
    if( pszRes == nullptr )
        return nullptr;
    json_object* poObj = nullptr;
    if( !OGRJSonParse(pszRes, &poObj, true) )
    {
        CPLFree(pszRes);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid JSon document");
        return nullptr;
    }
    if( CPL_json_object_object_get(poObj, "error") )
    {
        json_object_put(poObj);
        poObj = nullptr;
        // In case we don't get image.json, don't cache anything
        if( strcmp(pszFilename, "image.json") == 0 )
            bToCache = false;
    }
    if( bToCache )
    {
        CacheFile( osCachedFilename, pszRes, strlen(pszRes) );
    }
    CPLFree(pszRes);
    return poObj;
}

/************************************************************************/
/*                       ReadImageMetadata()                            */
/************************************************************************/

bool GDALRDADataset::ReadImageMetadata()
{
    json_object* poObj = ReadJSonFile("image.json");
    if( poObj == nullptr )
        return false;

    JsonObjectUniquePtr oObj(poObj);

    bool bError = false;
    bool bNonFatalError = false;
    m_osImageId = GetJsonString(poObj, "imageId", true, bError);
    m_osProfileName = GetJsonString(poObj, "profileName", false,
                                    bNonFatalError);
    m_osNativeTileFileFormat = GetJsonString(poObj, "nativeTileFileFormat",
                                            false, bNonFatalError);
    m_osNativeTileFileFormat = m_osNativeTileFileFormat.tolower();
    if( m_osNativeTileFileFormat.empty() )
        m_osNativeTileFileFormat = "tif";
    m_nTileXOffset = GetJsonInt64(poObj, "tileXOffset", true, bError);
    m_nTileYOffset = GetJsonInt64(poObj, "tileYOffset", true, bError);
    m_nNumXTiles = std::max<GIntBig>(0,
                                GetJsonInt64(poObj, "numXTiles", true, bError));
    m_nNumYTiles = std::max<GIntBig>(0,
                                GetJsonInt64(poObj, "numYTiles", true, bError));
    m_nTileXSize = static_cast<int>(std::max<GIntBig>(0,
        std::min<GIntBig>(
            GetJsonInt64(poObj, "tileXSize", true, bError), INT_MAX)));
    m_nTileYSize = static_cast<int>(std::max<GIntBig>(0,
        std::min<GIntBig>(
            GetJsonInt64(poObj, "tileYSize", true, bError), INT_MAX)));
    nBands = static_cast<int>(std::max<GIntBig>(0, 
        std::min<GIntBig>(
            GetJsonInt64(poObj, "numBands", true, bError), INT_MAX)));
    if( !bError && !GDALCheckBandCount(nBands, false) )
        return false;
    CPLString osDataType(GetJsonString(poObj, "dataType", true, bError));
    nRasterYSize = static_cast<int>(std::max<GIntBig>(0,
        std::min<GIntBig>(
            GetJsonInt64(poObj, "imageHeight", true, bError), INT_MAX)));
    nRasterXSize = static_cast<int>(std::max<GIntBig>(0,
        std::min<GIntBig>(
            GetJsonInt64(poObj, "imageWidth", true, bError), INT_MAX)));
    if( !bError && !GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) )
        return false;
    m_nMinX = GetJsonInt64(poObj, "minX", true, bError);
    m_nMinY = GetJsonInt64(poObj, "minY", true, bError);
    m_nMaxX = GetJsonInt64(poObj, "maxX", true, bError);
    m_nMaxY = GetJsonInt64(poObj, "maxY", true, bError);
    m_nMinTileX = GetJsonInt64(poObj, "minTileX", true, bError);
    m_nMinTileY = GetJsonInt64(poObj, "minTileY", true, bError);
    m_nMaxTileX = GetJsonInt64(poObj, "maxTileX", true, bError);
    m_nMaxTileY = GetJsonInt64(poObj, "maxTileY", true, bError);
    m_osColorInterpretation = GetJsonString(poObj,
                                        "colorInterpretation", true, bError);
    int64_t nXStart = m_nMinX - m_nMinTileX * m_nTileXSize;
    if( nXStart < 0 || nXStart >= m_nTileXSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent values of minX, minTileX and tileXSize");
        bError = true;
    }
    int64_t nYStart = m_nMinY - m_nMinTileY * m_nTileYSize;
    if( nYStart < 0 || nYStart >= m_nTileYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent values of minY, minTileY and tileYSize");
        bError = true;
    }

    CPLString osSensorName = GetJsonString(
        poObj, "sensorName", false, bNonFatalError);
    if( !osSensorName.empty() )
        SetMetadataItem("SENSOR_NAME", osSensorName);

    CPLString osSensorPlatformName = GetJsonString(
            poObj, "sensorPlatformName", false, bNonFatalError);
    if( !osSensorPlatformName.empty() )
        SetMetadataItem("SENSOR_PLATFORM_NAME", osSensorPlatformName);

    CPLString osAcquisitionDate = GetJsonString(
            poObj, "acquisitionDate", false, bNonFatalError);
    if( !osAcquisitionDate.empty() )
        SetMetadataItem("ACQUISITION_DATE", osAcquisitionDate);

    bNonFatalError = false;
    double dfGSD = GetJsonDouble(poObj, "groundSampleDistanceMeters", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("GSD", CPLSPrintf("%.3f m", dfGSD));

    bNonFatalError = false;
    double dfCloudCover = GetJsonDouble(poObj, "cloudCover", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("CLOUD_COVER", CPLSPrintf("%.1f", dfCloudCover));

    bNonFatalError = false;
    double dfSunAzimuth = GetJsonDouble(poObj, "sunAzimuth", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("SUN_AZIMUTH", CPLSPrintf("%.1f", dfSunAzimuth));

    bNonFatalError = false;
    double dfSunElevation = GetJsonDouble(poObj, "sunElevation", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("SUN_ELEVATION", CPLSPrintf("%.1f", dfSunElevation));

    bNonFatalError = false;
    double dfSatAzimuth = GetJsonDouble(poObj, "satAzimuth", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("SAT_AZIMUTH", CPLSPrintf("%.1f", dfSatAzimuth));

    bNonFatalError = false;
    double dfSatElevation = GetJsonDouble(poObj, "satElevation", false,
                                 bNonFatalError);
    if( !bNonFatalError )
        SetMetadataItem("SAT_ELEVATION", CPLSPrintf("%.1f", dfSatElevation));

    if( m_nNumXTiles <= 0 || m_nNumYTiles <= 0 || m_nTileXSize <= 0 ||
        m_nTileYSize <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad metadata values");
        bError = true;
    }

    static const struct
    {
        const char* pszName;
        GDALDataType eDT;
    }
    asDataTypes[] = {
        { "byte", GDT_Byte },
        { "short", GDT_Int16 },
        { "unsigned_short", GDT_UInt16 },
        { "integer", GDT_Int32 },
        { "unsigned_integer", GDT_UInt32 },
        // { "long", GDT_Int64 }, // Not supported
        // { "unsigned_long", GDT_UInt64 }, // Not supported
        { "float", GDT_Float32 },
        { "double", GDT_Float64 }
    };
    for( size_t i=0; i<CPL_ARRAYSIZE(asDataTypes); ++i )
    {
        if( EQUAL(asDataTypes[i].pszName, osDataType) )
        {
            m_eDT = asDataTypes[i].eDT;
            break;
        }
    }
    if( m_eDT == GDT_Unknown )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unhandled data type: %s",
                 osDataType.c_str());
        bError = true;
    }

    return !bError;
}

/************************************************************************/
/*                      ReadGeoreferencing()                            */
/************************************************************************/

bool GDALRDADataset::ReadGeoreferencing()
{
    m_bTriedReadGeoreferencing = true;

    json_object* poObj = ReadJSonFile("georeferencing.json");
    if( poObj == nullptr )
        return false;

    JsonObjectUniquePtr oObj(poObj);

    bool bError = false;
    CPLString osSRS =
        GetJsonString(poObj, "spatialReferenceSystemCode", true, bError);
    OGRSpatialReference oSRS;
    if( !osSRS.empty() && oSRS.SetFromUserInput(osSRS) == OGRERR_NONE )
    {
        char* pszWKT = nullptr;
        oSRS.exportToWkt(&pszWKT);
        if( pszWKT )
            m_osWKT = pszWKT;
        CPLFree(pszWKT);
    }

    bError = false;
    double dfScaleX = GetJsonDouble(poObj, "scaleX", true, bError);
    double dfScaleY = GetJsonDouble(poObj, "scaleY", true, bError);
    double dfTranslateX = GetJsonDouble(poObj, "translateX", true, bError);
    double dfTranslateY = GetJsonDouble(poObj, "translateY", true, bError);
    double dfShearX = GetJsonDouble(poObj, "shearX", true, bError);
    double dfShearY = GetJsonDouble(poObj, "shearY", true, bError);

    double adfPixelToPixelTranslate[6] = {
        static_cast<double>(m_nMinX),
        1.0,
        0.0,
        static_cast<double>(m_nMinY),
        0.0,
        1.0 };
    double adfPixelToMap[6] = { dfTranslateX, dfScaleX, dfShearX,
                                dfTranslateY, dfShearY, dfScaleY };

    //set the composed transform as the dataset transform
    if( !bError )
    {
        m_bGotGeoTransform = true;
        GDALComposeGeoTransforms(adfPixelToPixelTranslate,
                                 adfPixelToMap,
                                 m_adfGeoTransform.data());
    }

    return true;
}

/************************************************************************/
/*                            Get20Coeffs()                             */
/************************************************************************/

static CPLString Get20Coeffs(json_object* poObj, const char* pszPath,
                             bool bVerboseError, bool& bError)
{
    json_object* poCoeffs = CPL_json_object_object_get(poObj, pszPath);
    if( poCoeffs == nullptr ||
        json_object_get_type(poCoeffs) != json_type_array ||
        json_object_array_length(poCoeffs) != 20 )
    {
        if( bVerboseError ) 
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find %s of type array of 20 double", pszPath);
        }
        bError = true;
        return CPLString();
    }
    CPLString osRet;
    for( int i = 0; i < 20; i++ )
    {
        if( i != 0 )
            osRet += " ";
        osRet += CPLSPrintf("%.18g",
            json_object_get_double(json_object_array_get_idx(poCoeffs, i)));
    }
    return osRet;
}

/************************************************************************/
/*                              ReadRPCs()                              */
/************************************************************************/

bool GDALRDADataset::ReadRPCs()
{
    json_object* poObj = ReadJSonFile("rpcs.json");
    if( poObj == nullptr )
        return false;

    JsonObjectUniquePtr oObj(poObj);

    bool bError = false;

    // Not sure how to deal with those, so error out if they are != 1
    json_object* poScale = CPL_json_object_object_get(poObj, "postScaleFactorX");
    if( poScale != nullptr && json_object_get_double(poScale) != 1.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "postScaleFactorX != 1 in rpcs.json not supported");
        bError = true;
    }
    poScale = CPL_json_object_object_get(poObj, "postScaleFactorY");
    if( poScale != nullptr && json_object_get_double(poScale) != 1.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "postScaleFactorY != 1 in rpcs.json not supported");
        bError = true;
    }

    char** papszMD = nullptr;

    bool bMinMaxLongLatError = false;
    const double dfX0 =
        GetJsonDouble(poObj, "upperLeftCorner.x", false, bMinMaxLongLatError);
    const double dfX1 =
        GetJsonDouble(poObj, "upperRightCorner.x", false, bMinMaxLongLatError);
    const double dfX2 =
        GetJsonDouble(poObj, "upperLeftCorner.x", false, bMinMaxLongLatError);
    const double dfX3 =
        GetJsonDouble(poObj, "lowerRightCorner.x", false, bMinMaxLongLatError);
    const double dfY0 =
        GetJsonDouble(poObj, "upperLeftCorner.y", false, bMinMaxLongLatError);
    const double dfY1 =
        GetJsonDouble(poObj, "upperRightCorner.y", false, bMinMaxLongLatError);
    const double dfY2 =
        GetJsonDouble(poObj, "upperLeftCorner.y", false, bMinMaxLongLatError);
    const double dfY3 =
        GetJsonDouble(poObj, "lowerRightCorner.y", false, bMinMaxLongLatError);
    const double dfMinX = std::min(std::min(dfX0, dfX1), std::min(dfX2, dfX3));
    const double dfMinY = std::min(std::min(dfY0, dfY1), std::min(dfY2, dfY3));
    const double dfMaxX = std::max(std::max(dfX0, dfX1), std::max(dfX2, dfX3));
    const double dfMaxY = std::max(std::max(dfY0, dfY1), std::max(dfY2, dfY3));
    if( !bMinMaxLongLatError )
    {
        papszMD = CSLSetNameValue(papszMD, RPC_MIN_LONG,
                                  CPLSPrintf("%.18g", dfMinX));
        papszMD = CSLSetNameValue(papszMD, RPC_MIN_LAT,
                                  CPLSPrintf("%.18g", dfMinY));
        papszMD = CSLSetNameValue(papszMD, RPC_MAX_LONG,
                                  CPLSPrintf("%.18g", dfMaxX));
        papszMD = CSLSetNameValue(papszMD, RPC_MAX_LAT,
                                  CPLSPrintf("%.18g", dfMaxY));
    }

    papszMD = CSLSetNameValue(papszMD, RPC_LINE_OFF,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "lineOffset", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_SAMP_OFF,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "sampleOffset", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LAT_OFF,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "latOffset", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LONG_OFF,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "lonOffset", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_HEIGHT_OFF,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "heightOffset", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LINE_SCALE,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "lineScale", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_SAMP_SCALE,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "sampleScale", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LAT_SCALE,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "latScale", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LONG_SCALE,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "lonScale", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_HEIGHT_SCALE,
        CPLSPrintf("%.18g", GetJsonDouble(poObj, "heightScale", true, bError)));
    papszMD = CSLSetNameValue(papszMD, RPC_LINE_NUM_COEFF,
        Get20Coeffs(poObj, "lineNumCoefs", true, bError));
    papszMD = CSLSetNameValue(papszMD, RPC_LINE_DEN_COEFF,
        Get20Coeffs(poObj, "lineDenCoefs", true, bError));
    papszMD = CSLSetNameValue(papszMD, RPC_SAMP_NUM_COEFF,
        Get20Coeffs(poObj, "sampleNumCoefs", true, bError));
    papszMD = CSLSetNameValue(papszMD, RPC_SAMP_DEN_COEFF,
        Get20Coeffs(poObj, "sampleDenCoefs", true, bError));
    if( !bError )
        SetMetadata(papszMD, "RPC");
    CSLDestroy(papszMD);
    return !bError;
}

/************************************************************************/
/*                          GetMetadata()                               */
/************************************************************************/

char** GDALRDADataset::GetMetadata( const char* pszDomain )
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "RPC") && !m_bTriedReadRPC )
    {
        m_bTriedReadRPC = true;
        if( m_bTriedReadGeoreferencing )
            ReadGeoreferencing();
        // RPCs are only valid if there's no valid geotransform
        if( !m_bGotGeoTransform )
        {
            ReadRPCs();
        }
    }
    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr GDALRDADataset::GetGeoTransform(double *padfTransform)
{
    if( !m_bTriedReadGeoreferencing )
        ReadGeoreferencing();
    std::copy_n(m_adfGeoTransform.begin(), m_adfGeoTransform.size(),
                padfTransform);
    return (m_bGotGeoTransform) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char* GDALRDADataset::GetProjectionRef()
{
    if( !m_bTriedReadGeoreferencing )
        ReadGeoreferencing();
    return m_osWKT.c_str();
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

bool GDALRDADataset::Open( GDALOpenInfo* poOpenInfo )
{
    if( !ParseConnectionString(poOpenInfo) )
        return false;

    if( !ReadConfiguration() )
        return false;

    if( !GetAuthorization() )
        return false;

    if( !ReadImageMetadata() )
        return false;

    for( int i = 0; i < nBands; i++ )
    {
        SetBand( i+1 , new GDALRDARasterBand(this, i+1) );
    }

    // Hint for users of the driver to process by block, and then by band
    GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    return true;
}

GDALDataset* GDALRDADataset::OpenStatic( GDALOpenInfo* poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    std::unique_ptr<GDALRDADataset> poDS =
        std::unique_ptr<GDALRDADataset>(new GDALRDADataset());
    if( !poDS->Open(poOpenInfo) )
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                       GDALRDARasterBand()                          */
/************************************************************************/

GDALRDARasterBand::GDALRDARasterBand( GDALRDADataset* poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDSIn->m_nTileXSize;
    nBlockYSize = poDSIn->m_nTileYSize;
    eDataType = poDSIn->m_eDT;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALRDARasterBand::GetColorInterpretation()
{
    GDALRDADataset* poGDS = reinterpret_cast<GDALRDADataset*>(poDS);

    static const struct
    {
        const char* pszName;
        GDALColorInterp aeInter[5];
    }
    asColorInterpretations[] = 
    {
        { "PAN", { GCI_GrayIndex, GCI_Undefined, GCI_Undefined,
                   GCI_Undefined, GCI_Undefined } },
        { "PAN_WITH_ALPHA", { GCI_GrayIndex, GCI_AlphaBand, GCI_Undefined,
                              GCI_Undefined, GCI_Undefined } },
        { "RGB", { GCI_RedBand, GCI_GreenBand, GCI_BlueBand,
                   GCI_Undefined, GCI_Undefined } },
        { "RGBN", { GCI_RedBand, GCI_GreenBand, GCI_BlueBand,
                    GCI_Undefined, GCI_Undefined } },
        { "RGBA",  { GCI_RedBand, GCI_GreenBand, GCI_BlueBand,
                     GCI_AlphaBand, GCI_Undefined } },
        { "BGR",  { GCI_BlueBand, GCI_GreenBand, GCI_RedBand,
                    GCI_Undefined, GCI_Undefined } },
        { "BGRN",  { GCI_BlueBand, GCI_GreenBand, GCI_RedBand,
                     GCI_Undefined, GCI_Undefined } },
        { "LANDSAT_7_30M",  { GCI_BlueBand, GCI_GreenBand, GCI_RedBand,
                              GCI_Undefined, GCI_Undefined } },
        { "BGRA",  { GCI_BlueBand, GCI_GreenBand, GCI_RedBand,
                     GCI_AlphaBand, GCI_Undefined } },
        { "WORLDVIEW_8_BAND",  { GCI_Undefined, GCI_BlueBand, GCI_GreenBand,
                                 GCI_YellowBand, GCI_RedBand } },
        { "LANDSAT_8_30M",  { GCI_Undefined, GCI_BlueBand, GCI_GreenBand,
                              GCI_RedBand, GCI_Undefined } }
    };

    if( nBand <= 5 )
    {
        for( size_t i = 0; i < CPL_ARRAYSIZE(asColorInterpretations); i++ )
        {
            if( EQUAL(poGDS->m_osColorInterpretation,
                      asColorInterpretations[i].pszName) )
            {
                return asColorInterpretations[i].aeInter[nBand-1];
            }
        }
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                           MakeKeyCache()                             */
/************************************************************************/

std::string GDALRDADataset::MakeKeyCache(int64_t nTileX, int64_t nTileY)
{
    return std::string(CPLSPrintf("%p_" CPL_FRMT_GIB "_" CPL_FRMT_GIB,
                                  this, static_cast<GIntBig>(nTileX),
                                  static_cast<GIntBig>(nTileY)));
}

/************************************************************************/
/*                          BatchFetch()                                */
/************************************************************************/

void GDALRDADataset::BatchFetch(int nXOff, int nYOff, int nXSize, int nYSize)
{
    if( m_nXOffFetched == nXOff &&
        m_nYOffFetched == nYOff &&
        m_nXSizeFetched == nXSize &&
        m_nYSizeFetched == nYSize )
    {
        return;
    }
    m_nXOffFetched = nXOff;
    m_nYOffFetched = nYOff;
    m_nXSizeFetched = nXSize;
    m_nYSizeFetched = nYSize;

    if( m_nXSizeAdvise != 0 && m_nYSizeAdvise != 0 )
    {
        nXOff = m_nXOffAdvise;
        nYOff = m_nYOffAdvise;
        nXSize = m_nXSizeAdvise;
        nYSize = m_nYSizeAdvise;
        m_nXOffAdvise = 0;
        m_nYOffAdvise = 0;
        m_nXSizeAdvise = 0;
        m_nYSizeAdvise = 0;
    }

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    int nXBlock1 = nXOff / nBlockXSize;
    int nXBlock2 = (nXOff + nXSize - 1) / nBlockXSize;
    int nYBlock1 = nYOff / nBlockYSize;
    int nYBlock2 = (nYOff + nYSize - 1) / nBlockYSize;

    bool extendX = m_nMinTileX * m_nTileXSize != m_nMinX;
    bool extendY = m_nMinTileY * m_nTileYSize != m_nMinY;
    if( extendX && m_nMinTileX + nXBlock2 + 1 <= m_nMaxTileX )
        nXBlock2 ++;
    if( extendY && m_nMinTileY + nYBlock2 + 1 <= m_nMaxTileY )
        nYBlock2 ++;

    int64_t requestedBlockX = m_nMinTileX + nXBlock1;
    int64_t requestedBlockY = m_nMinTileY + nYBlock1;

    int nXBlocks = nXBlock2 - nXBlock1 + 1;
    int nYBlocks = nYBlock2 - nYBlock1 + 1;

    std::vector<char*> apszURLLists;
    std::vector<std::pair<int64_t,int64_t>> aTileIdx;
    for( int iY = 0; iY < nYBlocks; iY++ )
    {
        for( int iX = 0; iX < nXBlocks; iX++ )
        {
            const int64_t nTileX = requestedBlockX + iX;
            const int64_t nTileY = requestedBlockY + iY;
            const auto nKey = MakeKeyCache(nTileX, nTileY);
            std::shared_ptr<GDALDataset> ds;
            if( GetTileCache()->tryGet(nKey, ds) )
            {
                continue;
            }

            CPLString osSubPath;
            osSubPath += CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nTileX));
            osSubPath += "/";
            osSubPath += CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nTileY));
            osSubPath += ".";
            osSubPath += m_osNativeTileFileFormat;
            CPLString osCachedFilename(GetDatasetCacheDir() + "/" + osSubPath);
            VSIStatBufL sStat;
            if( VSIStatL(osCachedFilename, &sStat) == 0 )
            {
                continue;
            }

            CPLString osURL(m_osRDAAPIURL);
            osURL += "/tile/" + m_osGraphId + "/" + m_osNodeId + "/";
            osURL += osSubPath;
            apszURLLists.push_back(CPLStrdup(osURL));
            aTileIdx.push_back(std::pair<int64_t,int64_t>(nTileX, nTileY));
        }
    }

    const int nMaxDownloads = 8;
    for(size_t i=0;i<apszURLLists.size();i+=nMaxDownloads)
    {
        char** papszOptions = GetHTTPOptions();
        int nToDownload = std::min(nMaxDownloads,
                                static_cast<int>(apszURLLists.size() - i));
        CPLHTTPResult ** pasResults =
            CPLHTTPMultiFetch( &apszURLLists[i],
                                nToDownload,
                                nMaxDownloads,
                                papszOptions);
        CSLDestroy(papszOptions);

        for( int j = 0; j < nToDownload; j++ )
        {
            if( pasResults[j]->pabyData )
            {
                const int64_t nTileX = aTileIdx[i+j].first;
                const int64_t nTileY = aTileIdx[i+j].second;

                CPLString osSubPath;
                osSubPath += CPLSPrintf(CPL_FRMT_GIB,
                                        static_cast<GIntBig>(nTileX));
                osSubPath += "/";
                osSubPath += CPLSPrintf(CPL_FRMT_GIB,
                                        static_cast<GIntBig>(nTileY));
                osSubPath += ".";
                osSubPath += m_osNativeTileFileFormat;
                CPLString osCachedFilename(
                    GetDatasetCacheDir() + "/" + osSubPath);
                CacheFile( osCachedFilename,
                        pasResults[j]->pabyData, pasResults[j]->nDataLen);
            }
            CPLFree(apszURLLists[i+j]);
            CPLHTTPDestroyResult(pasResults[j]);
        }
        CPLFree(pasResults);
    }
}

/************************************************************************/
/*                            GetTiles()                                */
/************************************************************************/

std::vector<std::shared_ptr<GDALDataset>>
GDALRDADataset::GetTiles(
                    const std::vector<std::pair<int64_t,int64_t>>& aTileIdx)
{
    std::vector<std::shared_ptr<GDALDataset>> oResult;
    std::vector<size_t> anOutIndex;
    std::vector<char*> apszURLLists;
    for(size_t i=0;i<aTileIdx.size();i++)
    {
        const int64_t nTileX = aTileIdx[i].first;
        const int64_t nTileY = aTileIdx[i].second;

        const auto nKey = MakeKeyCache(nTileX, nTileY);
        std::shared_ptr<GDALDataset> ds;
        if( GetTileCache()->tryGet(nKey, ds) )
        {
            oResult.push_back(ds);
            continue;
        }

        CPLString osSubPath;
        osSubPath += CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nTileX));
        osSubPath += "/";
        osSubPath += CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nTileY));
        osSubPath += ".";
        osSubPath += m_osNativeTileFileFormat;
        CPLString osCachedFilename(GetDatasetCacheDir() + "/" + osSubPath);
        VSIStatBufL sStat;
        if( VSIStatL(osCachedFilename, &sStat) == 0 )
        {
            GDALDataset* poTileDS = reinterpret_cast<GDALDataset*>(
                GDALOpenEx(osCachedFilename, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                           nullptr, nullptr, nullptr));
            if( poTileDS == nullptr ||
                poTileDS->GetRasterCount() != GetRasterCount() ||
                poTileDS->GetRasterXSize() != m_nTileXSize ||
                poTileDS->GetRasterYSize() != m_nTileYSize )
            {
                delete poTileDS;
            }
            else
            {
                ds = std::shared_ptr<GDALDataset>(poTileDS);
                oResult.push_back(ds);
                GetTileCache()->insert(nKey, ds);
                continue;
            }
        }

        CPLString osURL(m_osRDAAPIURL);
        osURL += "/tile/" + m_osGraphId + "/" + m_osNodeId + "/";
        osURL += osSubPath;
        apszURLLists.push_back(CPLStrdup(osURL));
        anOutIndex.push_back(i);
        oResult.push_back( nullptr );
    }

    if( !apszURLLists.empty() )
    {
        char** papszOptions = GetHTTPOptions();
        CPLHTTPResult ** pasResults =
            CPLHTTPMultiFetch( apszURLLists.data(),
                            static_cast<int>(apszURLLists.size()),
                            0,
                            papszOptions);
        CSLDestroy(papszOptions);
        for(size_t i=0;i<apszURLLists.size();i++)
        {
            CPLFree(apszURLLists[i]);
        }
        if( pasResults == nullptr )
            return oResult;

        for(size_t i=0;i<anOutIndex.size();i++)
        {
            const size_t nOutIdx = anOutIndex[i];
            const int64_t nTileX = aTileIdx[nOutIdx].first;
            const int64_t nTileY = aTileIdx[nOutIdx].second;
            if( pasResults[i]->pabyData )
            {
                CPLString osTmpMemFile(CPLSPrintf("/vsimem/rda_%p_%d_%d.",
                                                this,
                                                static_cast<int>(nTileX),
                                                static_cast<int>(nTileY)));
                osTmpMemFile += m_osNativeTileFileFormat;
                GByte* pabyData = pasResults[i]->pabyData;
                pasResults[i]->pabyData = nullptr;
                VSIFCloseL(VSIFileFromMemBuffer(osTmpMemFile, pabyData,
                                                pasResults[i]->nDataLen, true));
                GDALDataset* poTileDS = reinterpret_cast<GDALDataset*>(
                    GDALOpenEx(osTmpMemFile, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                           nullptr, nullptr, nullptr));
                std::shared_ptr<GDALDataset> ds =
                    std::shared_ptr<GDALDataset>(poTileDS);
                if( poTileDS == nullptr )
                {
                    VSIUnlink(osTmpMemFile);
                    continue;
                }
                poTileDS->MarkSuppressOnClose();
                if( poTileDS->GetRasterCount() != GetRasterCount() ||
                    poTileDS->GetRasterXSize() != m_nTileXSize ||
                    poTileDS->GetRasterYSize() != m_nTileYSize )
                {
                    continue;
                }
                oResult[nOutIdx] = ds;
                const auto nKey = MakeKeyCache(nTileX, nTileY);
                GetTileCache()->insert(nKey, ds);

                CPLString osSubPath;
                osSubPath += CPLSPrintf(CPL_FRMT_GIB,
                                        static_cast<GIntBig>(nTileX));
                osSubPath += "/";
                osSubPath += CPLSPrintf(CPL_FRMT_GIB,
                                        static_cast<GIntBig>(nTileY));
                osSubPath += ".";
                osSubPath += m_osNativeTileFileFormat;
                CPLString osCachedFilename(
                    GetDatasetCacheDir() + "/" + osSubPath);
                CacheFile( osCachedFilename, pabyData, pasResults[i]->nDataLen);
            }
            CPLHTTPDestroyResult(pasResults[i]);
        }
        CPLFree(pasResults);
    }

    return oResult;
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr GDALRDADataset::IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      int nBandCount, int* panBands,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GSpacing nBandSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    BatchFetch(nXOff, nYOff, nXSize, nYSize);
    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nBandCount, panBands,
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                          AdviseRead()                                */
/************************************************************************/

CPLErr GDALRDADataset::AdviseRead (int nXOff, int nYOff,
                                   int nXSize, int nYSize, 
                                   int /* nBufXSize */,
                                   int /* nBufYSize */,
                                   GDALDataType /* eBufType */, 
                                   int /*nBands*/, int* /*panBands*/,
                                   char ** /* papszOptions */)
{
    m_nXOffAdvise = nXOff;
    m_nYOffAdvise = nYOff;
    m_nXSizeAdvise = nXSize;
    m_nYSizeAdvise = nYSize;
    return CE_None;
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr GDALRDARasterBand::IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    GDALRDADataset* poGDS = reinterpret_cast<GDALRDADataset*>(poDS);
    poGDS->BatchFetch(nXOff, nYOff, nXSize, nYSize);
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                          AdviseRead()                                */
/************************************************************************/

CPLErr GDALRDARasterBand::AdviseRead (int nXOff, int nYOff,
                                        int nXSize, int nYSize, 
                                        int /* nBufXSize */,
                                        int /* nBufYSize */,
                                        GDALDataType /* eBufType */, 
                                        char ** /* papszOptions */)
{
    GDALRDADataset* poGDS = reinterpret_cast<GDALRDADataset*>(poDS);
    poGDS->m_nXOffAdvise = nXOff;
    poGDS->m_nYOffAdvise = nYOff;
    poGDS->m_nXSizeAdvise = nXSize;
    poGDS->m_nYSizeAdvise = nYSize;
    return CE_None;
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GDALRDARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void* pImage)
{
    GDALRDADataset* poGDS = reinterpret_cast<GDALRDADataset*>(poDS);
    int64_t nTileX = nBlockXOff + poGDS->m_nMinTileX;
    int64_t nTileY = nBlockYOff + poGDS->m_nMinTileY;

    int nXStart = static_cast<int>(
        poGDS->m_nMinX - poGDS->m_nMinTileX * nBlockXSize);
    int nYStart = static_cast<int>(
        poGDS->m_nMinY - poGDS->m_nMinTileY * nBlockYSize);
    int nXBlocks = 1;
    int nYBlocks = 1;
    if( nXStart != 0 && nTileX + 1 <= poGDS->m_nMaxTileX )
        nXBlocks ++;
    if( nYStart != 0 && nTileY + 1 <= poGDS->m_nMaxTileY )
        nYBlocks ++;

    std::vector<std::pair<int64_t,int64_t>> aTileIdx;
    for( int iY = 0; iY < nYBlocks; iY++ )
    {
        for( int iX = 0; iX < nXBlocks; iX++ )
        {
            aTileIdx.push_back(std::pair<int64_t,int64_t>(nTileX + iX,
                                                          nTileY + iY));
        }
    }
    auto oResult = poGDS->GetTiles(aTileIdx);

    GByte* pabyTempBuffer = nullptr;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    if( nXStart != 0 || nYStart != 0 )
    {
        pabyTempBuffer = static_cast<GByte*>(CPLCalloc(
                                 nBlockXSize* nBlockYSize, nDTSize));
    }

    CPLErr eErr = CE_None;

    for( int i = 1; eErr == CE_None && i <= poGDS->GetRasterCount(); i++ )
    {
        GByte* pabyDstBuffer;
        GDALRasterBlock* poBlock = nullptr;
        if( i == nBand )
            pabyDstBuffer = static_cast<GByte*>(pImage);
        else
        {
            // Check if the same block in other bands is already in the GDAL
            // block cache
            poBlock = reinterpret_cast<GDALRDARasterBand*>(
                poGDS->GetRasterBand(i))->TryGetLockedBlockRef(
                    nBlockXOff, nBlockYOff);
            if( poBlock != nullptr )
            {
                // Yes, no need to do further work
                poBlock->DropLock();
                continue;
            }
            // Instanciate the block
            poBlock = poGDS->GetRasterBand(i)->GetLockedBlockRef(
                        nBlockXOff, nBlockYOff, TRUE);
            if (poBlock == nullptr)
            {
                continue;
            }
            pabyDstBuffer = static_cast<GByte*>(poBlock->GetDataRef());
        }

        for( int iY = 0; eErr == CE_None && iY < nYBlocks; iY++ )
        {
            for( int iX = 0; eErr == CE_None && iX < nXBlocks; iX++ )
            {
                GDALDataset* poTileDS = oResult[iY * nXBlocks + iX].get();
                if( poTileDS == nullptr )
                {
                    eErr = CE_Failure;
                    break;
                }

                eErr = poTileDS->GetRasterBand(i)->RasterIO(GF_Read,
                            0, 0,
                            nBlockXSize, nBlockYSize,
                            pabyTempBuffer ? pabyTempBuffer : pabyDstBuffer,
                            nBlockXSize, nBlockYSize,
                            eDataType, 0, 0, nullptr);
                if( pabyTempBuffer )
                {
                    int nSrcXOffset;
                    int nSrcYOffset;
                    int nDstXOffset;
                    int nDstYOffset;
                    int nCopyXSize;
                    int nCopyYSize;
                    if( iX == 0 )
                    {
                        nSrcXOffset = nXStart;
                        nDstXOffset = 0;
                        nCopyXSize = std::min(nBlockXSize - nXStart,
                                    nRasterXSize - nBlockXOff * nBlockXSize);
                    }
                    else
                    {
                        nSrcXOffset = 0;
                        nDstXOffset = nBlockXSize - nXStart;
                        nCopyXSize = std::max(0, std::min(nXStart,
                            nRasterXSize - nBlockXOff * nBlockXSize -
                                                    (nBlockXSize - nXStart)));
                    }
                    if( iY == 0 )
                    {
                        nSrcYOffset = nYStart;
                        nDstYOffset = 0;
                        nCopyYSize = std::min(nBlockYSize - nYStart,
                                    nRasterYSize - nBlockYOff * nBlockYSize);
                    }
                    else
                    {
                        nSrcYOffset = 0;
                        nDstYOffset = nBlockYSize - nYStart;
                        nCopyYSize = std::max(0, std::min(nYStart,
                            nRasterYSize - nBlockYOff * nBlockYSize -
                                                    (nBlockYSize - nYStart)));
                    }
                    for( int iCopyY = 0; nCopyXSize > 0 &&
                                        iCopyY < nCopyYSize; iCopyY ++ )
                    {
                        GDALCopyWords(
                            pabyTempBuffer +
                                ((nSrcYOffset + iCopyY) * nBlockXSize +
                                                        nSrcXOffset) * nDTSize,
                            eDataType,
                            nDTSize,
                            pabyDstBuffer +
                                ((nDstYOffset + iCopyY) * nBlockXSize +
                                                        nDstXOffset) * nDTSize,
                            eDataType,
                            nDTSize,
                            nCopyXSize);
                    }
                }
            }
        }

        if( poBlock )
            poBlock->DropLock();
    }

    CPLFree(pabyTempBuffer);

    return eErr;
}

/************************************************************************/
/*                       GDALRegister_RDA()                             */
/************************************************************************/

void GDALRegister_RDA()

{
    if( GDALGetDriverByName( "RDA" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "RDA" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "DigitalGlobe Raster Data Access driver" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_rda.html" );

    poDriver->pfnIdentify = GDALRDADataset::Identify;
    poDriver->pfnOpen = GDALRDADataset::OpenStatic;
    poDriver->pfnUnloadDriver = GDALRDADriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
