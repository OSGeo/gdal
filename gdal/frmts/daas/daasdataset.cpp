/******************************************************************************
 *
 * Project:  DAAS driver
 * Purpose:  DAAS driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Airbus DS Intelligence
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
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "gdal_mdreader.h"
#include "../mem/memdataset.h"

#include "cpl_json.h"

#include <algorithm>
#include <array>
#include <memory>

constexpr int knMIN_BLOCKSIZE = 64;
constexpr int knDEFAULT_BLOCKSIZE = 512;
constexpr int knMAX_BLOCKSIZE = 8192;

constexpr GUInt32 RETRY_PER_BAND = 1;
constexpr GUInt32 RETRY_SPATIAL_SPLIT = 2;

// Let's limit to 100 MB uncompressed per request
constexpr int knDEFAULT_SERVER_BYTE_LIMIT = 100 * 1024 * 1024;

constexpr int MAIN_MASK_BAND_NUMBER = 0;

/************************************************************************/
/*                          GDALDAASBandDesc                            */
/************************************************************************/

class GDALDAASBandDesc
{
    public:
        int       nIndex = 0;
        GDALDataType eDT = GDT_Unknown; // as declared in the GetMetadata response bands[]
        CPLString osName;
        CPLString osDescription;
        CPLString osColorInterp;
        bool      bIsMask = false;
};

/************************************************************************/
/*                          GDALDAASDataset                             */
/************************************************************************/

class GDALDAASRasterBand;

class GDALDAASDataset final: public GDALDataset
{
    public:
        enum class Format
        {
            RAW,
            PNG,
            JPEG,
            JPEG2000,
        };

    private:
        friend class GDALDAASRasterBand;

        CPLString m_osGetMetadataURL;

        CPLString m_osAuthURL;
        CPLString m_osAccessToken;
        time_t    m_nExpirationTime = 0;
        CPLString m_osXForwardUser;

        GDALDAASDataset* m_poParentDS = nullptr;
        //int         m_iOvrLevel = 0;

        CPLString m_osWKT;
        CPLString m_osSRSType;
        CPLString m_osSRSValue;
        bool      m_bGotGeoTransform = false;
        std::array<double,6> m_adfGeoTransform{{ 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 }};
        bool      m_bRequestInGeoreferencedCoordinates = false;
        GDALDataType m_eDT = GDT_Unknown;
        int          m_nActualBitDepth = 0;
        bool         m_bHasNoData = false;
        double       m_dfNoDataValue = 0.0;
        CPLString    m_osGetBufferURL;
        int          m_nBlockSize = knDEFAULT_BLOCKSIZE;
        Format       m_eFormat = Format::RAW;
        GIntBig      m_nServerByteLimit = knDEFAULT_SERVER_BYTE_LIMIT;
        GDALRIOResampleAlg m_eCurrentResampleAlg = GRIORA_NearestNeighbour;

        int          m_nMainMaskBandIndex = 0;
        CPLString    m_osMainMaskName;
        GDALDAASRasterBand* m_poMaskBand = nullptr;
        std::vector<GDALDAASBandDesc> m_aoBandDesc;

        int       m_nXOffAdvise = 0;
        int       m_nYOffAdvise = 0;
        int       m_nXSizeAdvise = 0;
        int       m_nYSizeAdvise = 0;

        int       m_nXOffFetched = 0;
        int       m_nYOffFetched = 0;
        int       m_nXSizeFetched = 0;
        int       m_nYSizeFetched = 0;

        std::vector<std::unique_ptr<GDALDAASDataset>> m_apoOverviewDS;

        char    **m_papszOpenOptions = nullptr;

        // Methods
        GDALDAASDataset(GDALDAASDataset* poParentDS, int iOvrLevel);

        bool      Open( GDALOpenInfo* poOpenInfo );
        bool      GetAuthorization();
        bool      GetImageMetadata();
        char**    GetHTTPOptions();
        void      ReadSRS(const CPLJSONObject& oProperties);
        void      ReadRPCs(const CPLJSONObject& oProperties);
        bool      SetupServerSideReprojection(const char* pszTargetSRS);
        void      InstantiateBands();

    public:
        GDALDAASDataset();
        ~GDALDAASDataset();

        static int Identify( GDALOpenInfo* poOpenInfo );
        static GDALDataset* OpenStatic( GDALOpenInfo* poOpenInfo );

        CPLErr          GetGeoTransform(double *padfTransform) override;
        const OGRSpatialReference* GetSpatialRef() const override {
            return GetSpatialRefFromOldGetProjectionRef();
        }
        const char*     _GetProjectionRef() override;
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
        void            FlushCache() override;
};


/************************************************************************/
/*                         GDALDAASRasterBand                            */
/************************************************************************/

class GDALDAASRasterBand final: public GDALRasterBand
{
        friend class GDALDAASDataset;

        int                 m_nSrcIndex = 0;
        GDALColorInterp     m_eColorInterp = GCI_Undefined;

        CPLErr          GetBlocks    (int nBlockXOff, int nBlockYOff,
                                      int nXBlocks, int nYBlocks,
                                      const std::vector<int>& anRequestedBands,
                                      void* pBuffer);

        GUInt32          PrefetchBlocks(int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        const std::vector<int>& anRequestedBands);

    public:
        GDALDAASRasterBand( GDALDAASDataset* poDS, int nBand,
                            const GDALDAASBandDesc& oBandDesc );

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
        double          GetNoDataValue(int* pbHasNoData) override;
        GDALColorInterp GetColorInterpretation() override;
        GDALRasterBand *GetMaskBand() override;
        int             GetMaskFlags() override;
        int             GetOverviewCount() override;
        GDALRasterBand* GetOverview(int) override;
};

/************************************************************************/
/*                          GDALDAASDataset()                           */
/************************************************************************/

GDALDAASDataset::GDALDAASDataset() :
    m_osAuthURL(CPLGetConfigOption("GDAL_DAAS_AUTH_URL",
        "https://authenticate.geoapi-airbusds.com/auth/realms/IDP/protocol/openid-connect/token"))
{
}

/************************************************************************/
/*                          GDALDAASDataset()                           */
/************************************************************************/

GDALDAASDataset::GDALDAASDataset(GDALDAASDataset* poParentDS,
                                 int iOvrLevel) :
    m_osGetMetadataURL(poParentDS->m_osGetMetadataURL),
    m_osAuthURL(poParentDS->m_osAuthURL),
    m_osAccessToken(CPLString()), // only used by parent
    m_nExpirationTime(0), // only used by parent
    m_osXForwardUser(CPLString()), // only used by parent
    m_poParentDS(poParentDS),
    //m_iOvrLevel(iOvrLevel),
    m_osWKT(poParentDS->m_osWKT),
    m_osSRSType(poParentDS->m_osSRSType),
    m_osSRSValue(poParentDS->m_osSRSValue),
    m_bGotGeoTransform(poParentDS->m_bGotGeoTransform),
    m_bRequestInGeoreferencedCoordinates(poParentDS->m_bRequestInGeoreferencedCoordinates),
    m_eDT(poParentDS->m_eDT),
    m_nActualBitDepth(poParentDS->m_nActualBitDepth),
    m_bHasNoData(poParentDS->m_bHasNoData),
    m_dfNoDataValue(poParentDS->m_dfNoDataValue),
    m_osGetBufferURL(poParentDS->m_osGetBufferURL),
    m_eFormat(poParentDS->m_eFormat),
    m_nServerByteLimit(poParentDS->m_nServerByteLimit),
    m_nMainMaskBandIndex(poParentDS->m_nMainMaskBandIndex),
    m_osMainMaskName(poParentDS->m_osMainMaskName),
    m_poMaskBand(nullptr),
    m_aoBandDesc(poParentDS->m_aoBandDesc)
{
    nRasterXSize = m_poParentDS->nRasterXSize >> iOvrLevel;
    nRasterYSize = m_poParentDS->nRasterYSize >> iOvrLevel;
    m_adfGeoTransform[0] = m_poParentDS->m_adfGeoTransform[0];
    m_adfGeoTransform[1] = m_poParentDS->m_adfGeoTransform[1] *
                                    m_poParentDS->nRasterXSize / nRasterXSize;
    m_adfGeoTransform[2] = m_poParentDS->m_adfGeoTransform[2];
    m_adfGeoTransform[3] = m_poParentDS->m_adfGeoTransform[3];
    m_adfGeoTransform[4] = m_poParentDS->m_adfGeoTransform[4];
    m_adfGeoTransform[5] = m_poParentDS->m_adfGeoTransform[5] *
                                    m_poParentDS->nRasterYSize / nRasterYSize;

    InstantiateBands();

    SetMetadata( m_poParentDS->GetMetadata() );
    SetMetadata( m_poParentDS->GetMetadata("RPC"), "RPC" );
}

/************************************************************************/
/*                         ~GDALDAASDataset()                            */
/************************************************************************/

GDALDAASDataset::~GDALDAASDataset()
{
    if( m_poParentDS == nullptr )
    {
        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "CLOSE_PERSISTENT",
                                    CPLSPrintf("%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch( "", papszOptions));
        CSLDestroy(papszOptions);
    }

    delete m_poMaskBand;
    CSLDestroy(m_papszOpenOptions);
}

/************************************************************************/
/*                          InstantiateBands()                          */
/************************************************************************/

void GDALDAASDataset::InstantiateBands()
{
    for( int i = 0; i < static_cast<int>(m_aoBandDesc.size()); i++ )
    {
        GDALRasterBand* poBand = new GDALDAASRasterBand(this, i+1,
                                                        m_aoBandDesc[i]);
        SetBand( i+1 , poBand );
    }

    if( !m_osMainMaskName.empty() )
    {
        GDALDAASBandDesc oDesc;
        oDesc.nIndex = m_nMainMaskBandIndex;
        oDesc.osName = m_osMainMaskName;
        m_poMaskBand = new GDALDAASRasterBand(this, 0, oDesc);
    }

    if( nBands > 1 )
    {
        // Hint for users of the driver
        GDALDataset::SetMetadataItem(
            "INTERLEAVE",
            "PIXEL",
            "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GDALDAASDataset::Identify( GDALOpenInfo* poOpenInfo )
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "DAAS:");
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr GDALDAASDataset::GetGeoTransform(double *padfTransform)
{
    std::copy_n(m_adfGeoTransform.begin(), m_adfGeoTransform.size(),
                padfTransform);
    return (m_bGotGeoTransform) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char* GDALDAASDataset::_GetProjectionRef()
{
    return m_osWKT.c_str();
}

/********************-****************************************************/
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
/*                             GetHTTPOptions()                         */
/************************************************************************/

char** GDALDAASDataset::GetHTTPOptions()
{
    if( m_poParentDS )
        return m_poParentDS->GetHTTPOptions();

    char** papszOptions = nullptr;
    CPLString osHeaders;
    if( !m_osAccessToken.empty() )
    {
        // Renew token if needed
        if( m_nExpirationTime != 0 && time(nullptr) >= m_nExpirationTime )
        {
            GetAuthorization();
        }
        osHeaders += "Authorization: Bearer "  + m_osAccessToken;
    }
    else
    {
        const char* pszAuthorization =
            CPLGetConfigOption("GDAL_DAAS_AUTHORIZATION", nullptr);
        if( pszAuthorization )
            osHeaders += pszAuthorization;
    }
    if( !m_osXForwardUser.empty() )
    {
        if( !osHeaders.empty() )
            osHeaders += "\r\n";
        osHeaders += "X-Forwarded-User: " + m_osXForwardUser;
    }
    if( !osHeaders.empty() )
    {
        papszOptions = CSLSetNameValue(papszOptions, "HEADERS",
                                    osHeaders.c_str());
    }
    papszOptions = CSLSetNameValue(papszOptions, "PERSISTENT",
                                   CPLSPrintf("%p", this));
    // 30 minutes
    papszOptions = CSLSetNameValue(papszOptions, "TIMEOUT", "1800");
    return papszOptions;
}



/************************************************************************/
/*                          DAASBackoffFactor()                         */
/************************************************************************/

/* Add a small amount of random jitter to avoid cyclic server stampedes */
static double DAASBackoffFactor(double base)
{
    // We don't need cryptographic quality randomness...
    // coverity[dont_call]
    return base + rand() * 0.5 / RAND_MAX;
}

/************************************************************************/
/*                          DAAS_CPLHTTPFetch()                         */
/************************************************************************/

static
CPLHTTPResult* DAAS_CPLHTTPFetch(const char* pszURL, char** papszOptions)
{
    CPLHTTPResult* psResult;
    const int RETRY_COUNT = 4;
    // coverity[tainted_data]
    double dfRetryDelay = CPLAtof(CPLGetConfigOption(
        "GDAL_DAAS_INITIAL_RETRY_DELAY", "1.0"));
    for(int i=0; i <= RETRY_COUNT; i++)
    {
        psResult = CPLHTTPFetch(pszURL, papszOptions);
        if( psResult == nullptr )
            break;

        if (psResult->nDataLen != 0
            && psResult->nStatus == 0
            && psResult->pszErrBuf == nullptr)
        {
            /* got a valid response */
            CPLErrorReset();
            break;
        }
        else
        {
            const char* pszErrorText = psResult->pszErrBuf ?
                                    psResult->pszErrBuf : "(null)";

            /* Get HTTP status code */
            int nHTTPStatus = -1;
            if( psResult->pszErrBuf != nullptr &&
                EQUALN(psResult->pszErrBuf, "HTTP error code : ",
                       strlen("HTTP error code : ")) )
            {
                nHTTPStatus = atoi(psResult->pszErrBuf +
                                        strlen("HTTP error code : "));
                if( psResult->pabyData )
                    pszErrorText = (const char*)psResult->pabyData;
            }

            if( (nHTTPStatus == 500 ||
                 (nHTTPStatus >= 502 && nHTTPStatus <= 504)) &&
                 i < RETRY_COUNT )
            {
                CPLError( CE_Warning, CPLE_FileIO,
                          "Error when downloading %s,"
                          "HTTP status=%d, retrying in %.2fs : %s",
                          pszURL, nHTTPStatus, dfRetryDelay, pszErrorText);
                CPLHTTPDestroyResult(psResult);
                psResult = nullptr;

                CPLSleep( dfRetryDelay );
                dfRetryDelay *= DAASBackoffFactor(4);
            }
            else
            {
                break;
            }
        }
    }

    return psResult;
}

/************************************************************************/
/*                           GetAuthorization()                         */
/************************************************************************/

bool GDALDAASDataset::GetAuthorization()
{
    CPLString osClientId =
        CSLFetchNameValueDef(m_papszOpenOptions, "CLIENT_ID",
                             CPLGetConfigOption("GDAL_DAAS_CLIENT_ID", ""));
    CPLString osAPIKey =
        CSLFetchNameValueDef(m_papszOpenOptions, "API_KEY",
                             CPLGetConfigOption("GDAL_DAAS_API_KEY", ""));
    CPLString osAuthorization =
        CSLFetchNameValueDef(m_papszOpenOptions, "ACCESS_TOKEN",
                             CPLGetConfigOption("GDAL_DAAS_ACCESS_TOKEN", ""));
    m_osXForwardUser =
        CSLFetchNameValueDef(m_papszOpenOptions, "X_FORWARDED_USER",
                        CPLGetConfigOption("GDAL_DAAS_X_FORWARDED_USER", ""));

    if( !osAuthorization.empty() )
    {
        if( !osClientId.empty() && !osAPIKey.empty() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "GDAL_DAAS_CLIENT_ID + GDAL_DAAS_API_KEY and "
                    "GDAL_DAAS_ACCESS_TOKEN defined. Only the later taken into "
                    "account");
        }
        m_osAccessToken = osAuthorization;
        return true;
    }

    if( osClientId.empty() && osAPIKey.empty() )
    {
        CPLDebug("DAAS", "Neither GDAL_DAAS_CLIENT_ID, GDAL_DAAS_API_KEY "
                 "nor GDAL_DAAS_ACCESS_TOKEN is defined. Trying without "
                 "authorization");
        return true;
    }

    if( osClientId.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDAL_DAAS_API_KEY defined, but GDAL_DAAS_CLIENT_ID missing.");
        return false;
    }

    if( osAPIKey.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDAL_DAAS_CLIENT_ID defined, but GDAL_DAAS_API_KEY missing.");
        return false;
    }

    CPLString osPostContent;
    osPostContent += "client_id=" + URLEscape(osClientId);
    osPostContent += "&apikey=" + URLEscape(osAPIKey);
    osPostContent += "&grant_type=api_key";

    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS",
                                   osPostContent.c_str());
    CPLString osHeaders("Content-Type: application/x-www-form-urlencoded");
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders.c_str());
    // FIXME for server side: make sure certificates are valid
    papszOptions = CSLSetNameValue(papszOptions, "UNSAFESSL", "YES");
    CPLHTTPResult* psResult = DAAS_CPLHTTPFetch( m_osAuthURL, papszOptions);
    CSLDestroy(papszOptions);

    if( psResult->pszErrBuf != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Get request %s failed: %s",
                  m_osAuthURL.c_str(),
                  psResult->pabyData ? CPLSPrintf("%s: %s",
                    psResult->pszErrBuf,
                    reinterpret_cast<const char*>(psResult->pabyData )) :
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

    CPLString osAuthorizationResponse(
                        reinterpret_cast<char*>(psResult->pabyData));
    CPLHTTPDestroyResult(psResult);

    CPLJSONDocument oDoc;
    if( !oDoc.LoadMemory(osAuthorizationResponse) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannont parse GetAuthorization response");
        return false;
    }

    m_osAccessToken = oDoc.GetRoot().GetString("access_token");
    if( m_osAccessToken.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot retrieve access_token");
        return false;
    }

    int nExpiresIn = oDoc.GetRoot().GetInteger("expires_in");
    if( nExpiresIn > 0 )
    {
        m_nExpirationTime = time(nullptr) + nExpiresIn - 60;
    }

    return true;
}

/************************************************************************/
/*                           GetObject()                                */
/************************************************************************/

static CPLJSONObject GetObject(CPLJSONObject& oContainer, const char* pszPath,
                               CPLJSONObject::Type eExpectedType,
                               const char* pszExpectedType,
                               bool bVerboseError, bool& bError)
{
    CPLJSONObject oObj = oContainer.GetObj(pszPath);
    if( !oObj.IsValid() )
    {
        if( bVerboseError )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s missing", pszPath);
        }
        bError = true;
        oObj.Deinit();
        return oObj;
    }
    if( oObj.GetType() != eExpectedType)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "%s not %s", pszPath, pszExpectedType);
        bError = true;
        oObj.Deinit();
        return oObj;
    }
    return oObj;
}

/************************************************************************/
/*                          GetInteger()                                */
/************************************************************************/

static int GetInteger(CPLJSONObject& oContainer, const char* pszPath,
                      bool bVerboseError, bool& bError)
{
    CPLJSONObject oObj = GetObject(oContainer, pszPath,
                                   CPLJSONObject::Type::Integer, "an integer",
                                   bVerboseError, bError);
    if( !oObj.IsValid() )
    {
        return 0;
    }
    return oObj.ToInteger();
}

/************************************************************************/
/*                          GetDouble()                                */
/************************************************************************/

static double GetDouble(CPLJSONObject& oContainer, const char* pszPath,
                        bool bVerboseError, bool& bError)
{
    CPLJSONObject oObj = oContainer.GetObj(pszPath);
    if( !oObj.IsValid() )
    {
        if( bVerboseError )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s missing", pszPath);
        }
        bError = true;
        return 0.0;
    }
    if( oObj.GetType() != CPLJSONObject::Type::Integer &&
        oObj.GetType() != CPLJSONObject::Type::Double)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "%s not a double", pszPath);
        bError = true;
        return 0.0;
    }
    return oObj.ToDouble();
}

/************************************************************************/
/*                          GetString()                                 */
/************************************************************************/

static CPLString GetString(CPLJSONObject& oContainer, const char* pszPath,
                           bool bVerboseError, bool& bError)
{
    CPLJSONObject oObj = GetObject(oContainer, pszPath,
                                   CPLJSONObject::Type::String, "a string",
                                   bVerboseError, bError);
    if( !oObj.IsValid() )
    {
        return CPLString();
    }
    return oObj.ToString();
}

/************************************************************************/
/*                     GetGDALDataTypeFromDAASPixelType()               */
/************************************************************************/

static GDALDataType GetGDALDataTypeFromDAASPixelType(
                                                const CPLString& osPixelType)
{
    const struct {
        const char* pszName;
        GDALDataType eDT;
    } asDataTypes[] = {
        { "Byte",    GDT_Byte },
        { "UInt16",  GDT_UInt16 },
        { "Int16",   GDT_Int16 },
        { "UInt32",  GDT_UInt32 },
        { "Int32",   GDT_Int32 },
        { "Float32", GDT_Float32 },
        { "Float64", GDT_Float64 },
    };
    for( size_t i = 0; i < CPL_ARRAYSIZE(asDataTypes); ++i )
    {
        if( osPixelType == asDataTypes[i].pszName )
        {
            return asDataTypes[i].eDT;
        }
    }
    return GDT_Unknown;
}

/************************************************************************/
/*                         GetImageMetadata()                           */
/************************************************************************/

bool GDALDAASDataset::GetImageMetadata()
{
    char** papszOptions = GetHTTPOptions();
    CPLHTTPResult* psResult = DAAS_CPLHTTPFetch(m_osGetMetadataURL,
                                                papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return false;

    if( psResult->pszErrBuf != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Get request %s failed: %s",
                  m_osGetMetadataURL.c_str(),
                  psResult->pabyData ? CPLSPrintf("%s: %s",
                    psResult->pszErrBuf,
                    reinterpret_cast<const char*>(psResult->pabyData )) :
                  psResult->pszErrBuf );
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Get request %s failed: "
                 "Empty content returned by server",
                 m_osGetMetadataURL.c_str());
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    CPLString osResponse(reinterpret_cast<char*>(psResult->pabyData));
    CPLHTTPDestroyResult(psResult);

    CPLJSONDocument oDoc;
    CPLDebug("DAAS", "%s", osResponse.c_str());
    if( !oDoc.LoadMemory(osResponse) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannont parse GetImageMetadata response");
        return false;
    }

    CPLJSONObject oProperties = oDoc.GetRoot().GetObj(
        "response/payload/payload/imageMetadata/properties");
    if( !oProperties.IsValid() )
    {
        oProperties = oDoc.GetRoot().GetObj("properties");
        if( !oProperties.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannont find response/payload/payload/imageMetadata/"
                    "properties nor properties in GetImageMetadata response");
            return false;
        }
    }

    bool bError = false;
    nRasterXSize = GetInteger(oProperties, "width", true, bError);
    nRasterYSize = GetInteger(oProperties, "height", true, bError);
    if( !bError && !GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) )
    {
        bError = true;
    }

    bool bIgnoredError = false;

    m_nActualBitDepth = GetInteger(oProperties, "actualBitDepth", false,
                                   bIgnoredError);

    bool bNoDataError = false;
    m_dfNoDataValue = GetDouble(oProperties, "noDataValue", false,
                                bNoDataError);
    m_bHasNoData = !bNoDataError;

    CPLJSONObject oGetBufferObj = oProperties.GetObj("_links/getBuffer");
    if( !oGetBufferObj.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing",
                    "_links/getBuffer");
        bError = true;
    }
    CPLJSONObject oGetBufferDict;
    oGetBufferDict.Deinit();
    if( oGetBufferObj.GetType() == CPLJSONObject::Type::Array )
    {
        auto array = oGetBufferObj.ToArray();
        if( array.Size() > 0 )
        {
            oGetBufferDict = array[0];
        }
    }
    else if( oGetBufferObj.GetType() == CPLJSONObject::Type::Object )
    {
        oGetBufferDict = oGetBufferObj;
    }
    if( !oGetBufferDict.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing",
                    "_links/getBuffer/href");
        bError = true;
    }
    else
    {
        m_osGetBufferURL = GetString(oGetBufferDict, "href", true,
                                        bError);
    }

#ifndef REMOVE_THAT_LEGACY_CODE
    if( !STARTS_WITH_CI(m_osGetMetadataURL, "https://192.168.") &&
        !STARTS_WITH_CI(m_osGetMetadataURL, "http://192.168.") &&
        STARTS_WITH_CI(m_osGetBufferURL, "http://192.168.") )
    {
        size_t nPosDaas = m_osGetMetadataURL.find("/daas/");
        size_t nPosImages = m_osGetMetadataURL.find("/images/");
        if( nPosDaas != std::string::npos && nPosImages != std::string::npos )
        {
            m_osGetBufferURL = m_osGetMetadataURL.substr(0, nPosDaas) +
                "/daas/images/" +
                m_osGetMetadataURL.substr(nPosImages + strlen("/images/")) +
                "/buffer";
        }
    }
#endif

    CPLJSONArray oGTArray = oProperties.GetArray("geotransform");
    if( oGTArray.IsValid() && oGTArray.Size() == 6 )
    {
        m_bGotGeoTransform = true;
        for( int i = 0; i < 6; i++ )
        {
            m_adfGeoTransform[i] = oGTArray[i].ToDouble();
        }
    }

    CPLJSONArray oBandArray = oProperties.GetArray("bands");
    if( !oBandArray.IsValid() || oBandArray.Size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing or empty bands array");
        bError = true;
    }
    else
    {
        for( int i = 0; i < oBandArray.Size(); ++i )
        {
            CPLJSONObject oBandObj = oBandArray[i];
            if( oBandObj.GetType() == CPLJSONObject::Type::Object )
            {
                GDALDAASBandDesc oDesc;
                oDesc.nIndex = i + 1;
                oDesc.osName = GetString(oBandObj, "name", true, bError);
                oDesc.osDescription = GetString(oBandObj, "description",
                                                false, bIgnoredError);
                oDesc.osColorInterp = GetString(oBandObj, "colorInterpretation",
                                                false, bIgnoredError);
                oDesc.bIsMask = oBandObj.GetBool("isMask");

                const CPLString osPixelType(GetString(oBandObj, "pixelType", true, bError));
                oDesc.eDT = GetGDALDataTypeFromDAASPixelType(osPixelType);
                if( oDesc.eDT == GDT_Unknown )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                            "Unsupported value pixelType = '%s'", osPixelType.c_str());
                    bError = true;
                }
                if( i == 0 )
                {
                    m_eDT = oDesc.eDT;
                }

                if( !CPLFetchBool( m_papszOpenOptions, "MASKS", true ) &&
                    oDesc.bIsMask )
                {
                    continue;
                }
                if( oDesc.osColorInterp == "MAIN_MASK" &&
                    m_osMainMaskName.empty() )
                {
                    m_nMainMaskBandIndex = i + 1;
                    m_osMainMaskName = oDesc.osName;
                }
                else
                {
                    m_aoBandDesc.push_back(oDesc);
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Invalid bands[] element");
                bError = true;
            }
        }
    }

    ReadSRS(oProperties);

    ReadRPCs(oProperties);

    // Collect other metadata
    for( const auto& oObj: oProperties.GetChildren() )
    {
        const CPLString& osName(oObj.GetName());
        const auto& oType(oObj.GetType());
        if( osName != "aoiFactor" &&
            osName != "crsCode" &&
            osName != "nbBands" &&
            osName != "nbBits" &&
            osName != "nBits" &&
            osName != "actualBitDepth" &&
            osName != "width" &&
            osName != "height" &&
            osName != "noDataValue" &&
            osName != "step" &&
            osName != "pixelType" &&
            oObj.IsValid() &&
            oType != CPLJSONObject::Type::Null &&
            oType != CPLJSONObject::Type::Array &&
            oType != CPLJSONObject::Type::Object )
        {
            SetMetadataItem( osName.c_str(), oObj.ToString().c_str() );
        }
    }

    // Metadata for IMAGERY domain
    CPLString osAcquisitionDate(
        GetString(oProperties, "acquisitionDate", false, bIgnoredError));
    if( !osAcquisitionDate.empty() )
    {
        int iYear = 0;
        int iMonth = 0;
        int iDay = 0;
        int iHours = 0;
        int iMin = 0;
        int iSec = 0;
        const int r = sscanf (osAcquisitionDate.c_str(),
                              "%d-%d-%dT%d:%d:%d.%*dZ",
                              &iYear, &iMonth, &iDay, &iHours, &iMin, &iSec);
        if( r == 6 )
        {
            SetMetadataItem( MD_NAME_ACQDATETIME,
                             CPLSPrintf("%04d-%02d-%02d %02d:%02d:%02d",
                                        iYear, iMonth, iDay,
                                        iHours, iMin, iSec),
                             MD_DOMAIN_IMAGERY );
        }
    }

    bIgnoredError = false;
    double dfCloudCover = GetDouble(
        oProperties, "cloudCover", false, bIgnoredError);
    if( !bIgnoredError )
    {
        SetMetadataItem( MD_NAME_CLOUDCOVER,
                         CPLSPrintf("%.2f", dfCloudCover),
                         MD_DOMAIN_IMAGERY );
    }

    CPLString osSatellite(
        GetString(oProperties, "satellite", false, bIgnoredError));
    if( !osSatellite.empty() )
    {
        SetMetadataItem( MD_NAME_SATELLITE, osSatellite.c_str(),
                         MD_DOMAIN_IMAGERY );
    }

    return !bError;
}


/************************************************************************/
/*                            ReadSRS()                                 */
/************************************************************************/

void GDALDAASDataset::ReadSRS(const CPLJSONObject& oProperties)
{
    CPLJSONArray oSRSArray = oProperties.GetArray("srsExpression/names");
    if( oSRSArray.IsValid() )
    {
        for( int i = 0; i < oSRSArray.Size(); ++i )
        {
            CPLJSONObject oSRSObj = oSRSArray[i];
            if( oSRSObj.GetType() == CPLJSONObject::Type::Object )
            {
                bool bError = false;
                CPLString osType( GetString(oSRSObj, "type", true, bError) );
                CPLString osValue( GetString(oSRSObj, "value", true, bError) );
                // Use urn in priority
                if( osType == "urn" && !osValue.empty() )
                {
                    m_osSRSType = osType;
                    m_osSRSValue = osValue;
                }
                // Use proj4 if urn not already set
                else if( osType == "proj4" && !osValue.empty() &&
                         m_osSRSType != "urn" )
                {
                    m_osSRSType = osType;
                    m_osSRSValue = osValue;
                }
                // If no SRS set, take the first one
                else if( m_osSRSValue.empty() && !osType.empty() &&
                         !osValue.empty() )
                {
                    m_osSRSType = osType;
                    m_osSRSValue = osValue;
                }
            }
        }
    }
    else
    {
        auto osCrsCode = oProperties.GetString("crsCode");
        if( !osCrsCode.empty() )
        {
            m_osSRSType = "urn";
            m_osSRSValue = osCrsCode;
        }
    }

    if( m_osSRSType == "urn" || m_osSRSType == "proj4" )
    {
        OGRSpatialReference oSRS;
        if( oSRS.SetFromUserInput(m_osSRSValue) == OGRERR_NONE )
        {
            OGR_SRSNode *poGEOGCS = oSRS.GetAttrNode("GEOGCS");
            if( poGEOGCS != nullptr )
                poGEOGCS->StripNodes("AXIS");

            OGR_SRSNode *poPROJCS = oSRS.GetAttrNode("PROJCS");
            if (poPROJCS != nullptr && oSRS.EPSGTreatsAsNorthingEasting())
                poPROJCS->StripNodes("AXIS");

            char* pszWKT = nullptr;
            oSRS.exportToWkt(&pszWKT);
            if( pszWKT )
                m_osWKT = pszWKT;
            CPLFree(pszWKT);
        }
    }
}

/************************************************************************/
/*                            ReadRPCs()                                */
/************************************************************************/

void GDALDAASDataset::ReadRPCs(const CPLJSONObject& oProperties)
{
    CPLJSONObject oRPC = oProperties.GetObj("rpc");
    if( oRPC.IsValid() )
    {
        bool bRPCError = false;
        CPLStringList aoRPC;
        const struct {
            const char* pszJsonName;
            const char* pszGDALName;
        } asRPCSingleValues[] = {
            { "errBias", RPC_ERR_BIAS},
            { "errRand", RPC_ERR_RAND},
            { "sampOff", RPC_SAMP_OFF },
            { "lineOff", RPC_LINE_OFF },
            { "latOff",  RPC_LAT_OFF },
            { "longOff", RPC_LONG_OFF },
            { "heightOff", RPC_HEIGHT_OFF },
            { "lineScale", RPC_LINE_SCALE },
            { "sampScale", RPC_SAMP_SCALE },
            { "latScale", RPC_LAT_SCALE },
            { "longScale", RPC_LONG_SCALE },
            { "heightScale", RPC_HEIGHT_SCALE },
        };
        for( size_t i = 0; i < CPL_ARRAYSIZE(asRPCSingleValues); ++i )
        {
            bool bRPCErrorTmp = false;
            const bool bVerboseError =
                !(strcmp(asRPCSingleValues[i].pszGDALName, RPC_ERR_BIAS) == 0 ||
                  strcmp(asRPCSingleValues[i].pszGDALName, RPC_ERR_RAND) == 0);
            double dfRPCVal = GetDouble(oRPC, asRPCSingleValues[i].pszJsonName, bVerboseError, bRPCErrorTmp);
            if (bRPCErrorTmp)
            {
                if (bVerboseError)
                {
                    bRPCError = true;
                }
                continue;
            }
            aoRPC.SetNameValue(asRPCSingleValues[i].pszGDALName, CPLSPrintf("%.18g", dfRPCVal));
        }

        const struct {
            const char* pszJsonName;
            const char* pszGDALName;
        } asRPCArrayValues[] = {
            { "lineNumCoeff", RPC_LINE_NUM_COEFF },
            { "lineDenCoeff", RPC_LINE_DEN_COEFF },
            { "sampNumCoeff", RPC_SAMP_NUM_COEFF },
            { "sampDenCoeff", RPC_SAMP_DEN_COEFF },
        };
        for( size_t i = 0; i < CPL_ARRAYSIZE(asRPCArrayValues); ++i )
        {
            CPLJSONArray oRPCArray =
                oRPC.GetArray(asRPCArrayValues[i].pszJsonName);
            if( oRPCArray.IsValid() && oRPCArray.Size() == 20 )
            {
                CPLString osVal;
                for( int j = 0; j < 20; j++ )
                {
                    if( j > 0 )
                        osVal += " ";
                    osVal += CPLSPrintf("%.18g", oRPCArray[j].ToDouble());
                }
                aoRPC.SetNameValue(asRPCArrayValues[i].pszGDALName,
                                   osVal.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find %s", asRPCArrayValues[i].pszJsonName);
            }
        }
        if( !bRPCError )
        {
            SetMetadata(aoRPC.List(), "RPC");
        }
    }
}

/************************************************************************/
/*                      SetupServerSideReprojection()                   */
/************************************************************************/

bool GDALDAASDataset::SetupServerSideReprojection(const char* pszTargetSRS)
{
    if( m_osWKT.empty() || !m_bGotGeoTransform )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "TARGET_SRS is specified, but projection and/or "
                    "geotransform are missing in image metadata");
        return false;
    }

    OGRSpatialReference oSRS;
    if( oSRS.SetFromUserInput(pszTargetSRS) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid TARGET_SRS value");
        return false;
    }

    // Check that we can find the EPSG code as we will need to
    // provide as a urn to getBuffer
    const char* pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
    const char* pszAuthorityName = oSRS.GetAuthorityName(nullptr);
    if( pszAuthorityName == nullptr || !EQUAL(pszAuthorityName, "EPSG") ||
        pszAuthorityCode == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "TARGET_SRS cannot be identified to a EPSG code");
        return false;
    }

    CPLString osTargetEPSGCode = CPLString("epsg:") + pszAuthorityCode;

    char* pszWKT = nullptr;
    oSRS.exportToWkt(&pszWKT);
    char** papszTO = CSLSetNameValue( nullptr, "DST_SRS", pszWKT );
    CPLString osTargetWKT = pszWKT;
    CPLFree(pszWKT);

    void* hTransformArg =
            GDALCreateGenImgProjTransformer2( this, nullptr, papszTO );
    if( hTransformArg == nullptr )
    {
        CSLDestroy(papszTO);
        return false;
    }

    GDALTransformerInfo* psInfo = (GDALTransformerInfo*)hTransformArg;
    double adfGeoTransform[6];
    double adfExtent[4];
    int    nXSize, nYSize;

    if ( GDALSuggestedWarpOutput2( this,
                                psInfo->pfnTransform, hTransformArg,
                                adfGeoTransform,
                                &nXSize, &nYSize,
                                adfExtent, 0 ) != CE_None )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find extent in specified TARGET_SRS");
        CSLDestroy(papszTO);
        GDALDestroyGenImgProjTransformer( hTransformArg );
        return false;
    }

    GDALDestroyGenImgProjTransformer( hTransformArg );

    std::copy_n(adfGeoTransform, 6, m_adfGeoTransform.begin());
    m_bRequestInGeoreferencedCoordinates = true;
    m_osSRSType = "epsg";
    m_osSRSValue = osTargetEPSGCode;
    m_osWKT = osTargetWKT;
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    return true;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

bool GDALDAASDataset::Open( GDALOpenInfo* poOpenInfo )
{
    m_papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    m_osGetMetadataURL =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "GET_METADATA_URL",
                             poOpenInfo->pszFilename + strlen("DAAS:"));
    if( m_osGetMetadataURL.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GET_METADATA_URL is missing");
        return false;
    }
    m_nBlockSize = std::max(knMIN_BLOCKSIZE, std::min(knMAX_BLOCKSIZE,
        atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                  "BLOCK_SIZE",
                                  CPLSPrintf("%d", m_nBlockSize)))));
    m_nServerByteLimit = atoi(CPLGetConfigOption("GDAL_DAAS_SERVER_BYTE_LIMIT",
                            CPLSPrintf("%d", knDEFAULT_SERVER_BYTE_LIMIT)));

    if( CPLTestBool(CPLGetConfigOption("GDAL_DAAS_PERFORM_AUTH", "YES")) &&
        !GetAuthorization() )
        return false;
    if( !GetImageMetadata() )
        return false;

    const char* pszFormat = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                 "PIXEL_ENCODING",
                                                 "AUTO");
    if( EQUAL(pszFormat, "AUTO") )
    {
        if( (m_aoBandDesc.size() == 1 || m_aoBandDesc.size() == 3 ||
             m_aoBandDesc.size() == 4) && m_eDT == GDT_Byte )
        {
            m_eFormat = Format::PNG;
        }
        else
        {
            m_eFormat = Format::RAW;
        }
    }
    else if( EQUAL(pszFormat, "RAW") )
    {
        m_eFormat = Format::RAW;
    }
    else if( EQUAL(pszFormat, "PNG") )
    {
        if( (m_aoBandDesc.size() == 1 || m_aoBandDesc.size() == 3 ||
             m_aoBandDesc.size() == 4) && m_eDT == GDT_Byte )
        {
            m_eFormat = Format::PNG;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "PNG only supported for 1, 3 or 4-band Byte dataset. "
                     "Falling back to RAW");
            m_eFormat = Format::RAW;
        }
    }
    else if( EQUAL(pszFormat, "JPEG") )
    {
        if( (m_aoBandDesc.size() == 1 || m_aoBandDesc.size() == 3) &&
            m_eDT == GDT_Byte )
        {
            m_eFormat = Format::JPEG;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "JPEG only supported for 1 or 3-band Byte dataset. "
                     "Falling back to RAW");
            m_eFormat = Format::RAW;
        }
    }
    else if( EQUAL(pszFormat, "JPEG2000") )
    {
        if( m_eDT != GDT_Float32 && m_eDT != GDT_Float64 )
        {
            m_eFormat = Format::JPEG2000;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "JPEG2000 only supported for integer datatype dataset. "
                     "Falling back to RAW");
            m_eFormat = Format::RAW;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported PIXEL_ENCODING=%s", pszFormat);
        return false;
    }

    const char* pszTargetSRS = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                                 "TARGET_SRS");
    if( pszTargetSRS )
    {
        if( !SetupServerSideReprojection(pszTargetSRS) )
        {
            return false;
        }
    }

    InstantiateBands();

    // Instantiate overviews
    int iOvr = 0;
    while( (nRasterXSize >> iOvr) > 256 || (nRasterYSize >> iOvr) > 256 )
    {
        iOvr ++;
        if( (nRasterXSize >> iOvr) == 0 ||
            (nRasterYSize >> iOvr) == 0 )
        {
            break;
        }
        m_apoOverviewDS.push_back(
            std::unique_ptr<GDALDAASDataset>(new GDALDAASDataset(this, iOvr)));
    }

    return true;
}

GDALDataset* GDALDAASDataset::OpenStatic( GDALOpenInfo* poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    std::unique_ptr<GDALDAASDataset> poDS =
        std::unique_ptr<GDALDAASDataset>(new GDALDAASDataset());
    if( !poDS->Open(poOpenInfo) )
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                       GDALDAASRasterBand()                          */
/************************************************************************/

GDALDAASRasterBand::GDALDAASRasterBand( GDALDAASDataset* poDSIn, int nBandIn,
                                        const GDALDAASBandDesc& oBandDesc )
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poDSIn->m_eDT;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();
    nBlockXSize = poDSIn->m_nBlockSize;
    nBlockYSize = poDSIn->m_nBlockSize;
    m_nSrcIndex = oBandDesc.nIndex;

    SetDescription( oBandDesc.osName );
    if( !oBandDesc.osDescription.empty() )
    {
        SetMetadataItem("DESCRIPTION", oBandDesc.osDescription );
    }

    const struct {
        const char* pszName;
        GDALColorInterp eColorInterp;
    } asColorInterpretations[] = {
        { "RED",         GCI_RedBand },
        { "GREEN",       GCI_GreenBand },
        { "BLUE",        GCI_BlueBand },
        { "GRAY",        GCI_GrayIndex },
        { "ALPHA",       GCI_AlphaBand },
        { "UNDEFINED",   GCI_Undefined },
    };
    for( size_t i = 0; i < CPL_ARRAYSIZE(asColorInterpretations); ++i )
    {
        if( EQUAL(oBandDesc.osColorInterp, asColorInterpretations[i].pszName ) )
        {
            m_eColorInterp = asColorInterpretations[i].eColorInterp;
            break;
        }
    }
    if( !oBandDesc.osColorInterp.empty() &&
        !EQUAL(oBandDesc.osColorInterp, "UNDEFINED") &&
        m_eColorInterp != GCI_Undefined )
    {
        SetMetadataItem("COLOR_INTERPRETATION", oBandDesc.osColorInterp );
    }

    if( poDSIn->m_nActualBitDepth != 0 && poDSIn->m_nActualBitDepth != 8 &&
        poDSIn->m_nActualBitDepth != 16 && poDSIn->m_nActualBitDepth != 32 &&
        poDSIn->m_nActualBitDepth != 64 )
    {
        SetMetadataItem("NBITS",
                        CPLSPrintf("%d", poDSIn->m_nActualBitDepth),
                        "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                         GetNoDataValue()                             */
/************************************************************************/

double GDALDAASRasterBand::GetNoDataValue(int* pbHasNoData)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    if( poGDS->m_bHasNoData )
    {
        if( pbHasNoData )
            *pbHasNoData = true;
        return poGDS->m_dfNoDataValue;
    }
    if( pbHasNoData )
        *pbHasNoData = false;
    return 0.0;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALDAASRasterBand::GetColorInterpretation()
{
    return m_eColorInterp;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GDALDAASRasterBand::GetMaskBand()
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    if( poGDS->m_poMaskBand )
        return poGDS->m_poMaskBand;
    return GDALRasterBand::GetMaskBand();
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GDALDAASRasterBand::GetMaskFlags()
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    if( poGDS->m_poMaskBand )
        return GMF_PER_DATASET;
    return GDALRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                      CanSpatiallySplit()                             */
/************************************************************************/

static bool CanSpatiallySplit(GUInt32 nRetryFlags,
                              int nXOff, int nYOff,
                              int nXSize, int nYSize,
                              int nBufXSize, int nBufYSize,
                              int nBlockXSize, int nBlockYSize,
                              GSpacing nPixelSpace,
                              GSpacing nLineSpace,
                              int& nXOff1, int& nYOff1,
                              int& nXSize1, int& nYSize1,
                              int& nXOff2, int& nYOff2,
                              int& nXSize2, int& nYSize2,
                              GSpacing& nDataShift2)
{
    if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nYSize > nBlockYSize )
    {
        int nHalf = std::max(nBlockYSize,
                             ((nYSize / 2 ) / nBlockYSize) * nBlockYSize);
        nXOff1 = nXOff;
        nYOff1 = nYOff;
        nXSize1 = nXSize;
        nYSize1 = nHalf;
        nXOff2 = nXOff;
        nYOff2 = nYOff + nHalf;
        nXSize2 = nXSize;
        nYSize2 = nYSize - nHalf;
        nDataShift2 = nHalf * nLineSpace;
        return true;
    }
    else if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nXSize > nBlockXSize )
    {
        int nHalf = std::max(nBlockXSize,
                             ((nXSize / 2 ) / nBlockXSize) * nBlockXSize);
        nXOff1 = nXOff;
        nYOff1 = nYOff;
        nXSize1 = nHalf;
        nYSize1 = nYSize;
        nXOff2 = nXOff + nHalf;
        nYOff2 = nYOff;
        nXSize2 = nXSize - nHalf;
        nYSize2 = nYSize;
        nDataShift2 = nHalf * nPixelSpace;
        return true;
    }
    return false;
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr GDALDAASDataset::IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      int nBandCount, int* panBandMap,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GSpacing nBandSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    m_eCurrentResampleAlg = psExtraArg->eResampleAlg;

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetRasterBand(1)->GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        GDALRasterIOExtraArg sExtraArg;
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        const int nOverview =
            GDALBandGetBestOverviewLevel2( GetRasterBand(1),
                                            nXOff, nYOff, nXSize, nYSize,
                                           nBufXSize, nBufYSize, &sExtraArg );
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand =
                        GetRasterBand(1)->GetOverview(nOverview);
            if (poOverviewBand == nullptr ||
                poOverviewBand->GetDataset() == nullptr)
            {
                return CE_Failure;
            }

            return poOverviewBand->GetDataset()->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, &sExtraArg );
        }
    }

    GDALDAASRasterBand* poBand =
        dynamic_cast<GDALDAASRasterBand*>(GetRasterBand(1));
    if( poBand )
    {
        std::vector<int> anRequestedBands;
        if( m_poMaskBand)
            anRequestedBands.push_back(0);
        for( int i = 1; i <= GetRasterCount(); i++ )
            anRequestedBands.push_back(i);
        GUInt32 nRetryFlags = poBand->PrefetchBlocks(
                                    nXOff, nYOff, nXSize, nYSize,
                                    anRequestedBands);
        int nBlockXSize, nBlockYSize;
        poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        int nXOff1 = 0;
        int nYOff1 = 0;
        int nXSize1 = 0;
        int nYSize1 = 0;
        int nXOff2 = 0;
        int nYOff2 = 0;
        int nXSize2 = 0;
        int nYSize2 = 0;
        GSpacing nDataShift2 = 0;
        if( CanSpatiallySplit(nRetryFlags, nXOff, nYOff, nXSize, nYSize,
                            nBufXSize, nBufYSize,
                            nBlockXSize, nBlockYSize,
                            nPixelSpace, nLineSpace,
                            nXOff1, nYOff1,
                            nXSize1, nYSize1,
                            nXOff2, nYOff2,
                            nXSize2, nYSize2,
                            nDataShift2) )
        {
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);

            CPLErr eErr = IRasterIO(eRWFlag, nXOff1, nYOff1,
                                    nXSize1, nYSize1,
                                    pData,
                                    nXSize1, nYSize1,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace,
                                    &sExtraArg);
            if( eErr == CE_None )
            {
                eErr = IRasterIO(eRWFlag,
                                    nXOff2, nYOff2,
                                    nXSize2, nYSize2,
                                    static_cast<GByte*>(pData) + nDataShift2,
                                    nXSize2, nYSize2,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace,
                                    &sExtraArg);
            }
            return eErr;
        }
        else if( (nRetryFlags & RETRY_PER_BAND) && nBands > 1 )
        {
            for( int iBand = 1; iBand <= nBands; iBand++ )
            {
                poBand =
                    dynamic_cast<GDALDAASRasterBand*>(GetRasterBand(iBand));
                if( poBand )
                {
                    CPL_IGNORE_RET_VAL(poBand->PrefetchBlocks(
                                            nXOff, nYOff, nXSize, nYSize,
                                            std::vector<int>{iBand}));
                }
            }
        }
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType,
                                  nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg);
}

/************************************************************************/
/*                          AdviseRead()                                */
/************************************************************************/

CPLErr GDALDAASDataset::AdviseRead (int nXOff, int nYOff,
                                   int nXSize, int nYSize,
                                   int nBufXSize,
                                   int nBufYSize,
                                   GDALDataType /* eBufType */,
                                   int /*nBands*/, int* /*panBands*/,
                                   char ** /* papszOptions */)
{
    if( nXSize == nBufXSize && nYSize == nBufYSize )
    {
        m_nXOffAdvise = nXOff;
        m_nYOffAdvise = nYOff;
        m_nXSizeAdvise = nXSize;
        m_nYSizeAdvise = nYSize;
    }
    return CE_None;
}

/************************************************************************/
/*                          FlushCache()                                */
/************************************************************************/

void GDALDAASDataset::FlushCache ()
{
    GDALDataset::FlushCache();
    m_nXOffFetched = 0;
    m_nYOffFetched = 0;
    m_nXSizeFetched = 0;
    m_nYSizeFetched = 0;
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int GDALDAASRasterBand::GetOverviewCount()
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewDS.size());
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* GDALDAASRasterBand::GetOverview(int iIndex)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    if( iIndex >= 0 &&
        iIndex < static_cast<int>(poGDS->m_apoOverviewDS.size()) )
    {
        return poGDS->m_apoOverviewDS[iIndex]->GetRasterBand(nBand);
    }
    return nullptr;
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GDALDAASRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void* pImage)
{
    return GetBlocks(nBlockXOff, nBlockYOff, 1, 1,
                     std::vector<int>{nBand}, pImage);
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr GDALDAASRasterBand::IRasterIO(GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);

    poGDS->m_eCurrentResampleAlg = psExtraArg->eResampleAlg;

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        GDALRasterIOExtraArg sExtraArg;
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        const int nOverview =
            GDALBandGetBestOverviewLevel2( this, nXOff, nYOff, nXSize, nYSize,
                                           nBufXSize, nBufYSize, &sExtraArg );
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == nullptr)
                return CE_Failure;

            return poOverviewBand->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nPixelSpace, nLineSpace, &sExtraArg );
        }
    }

    std::vector<int> anRequestedBands;
    if( poGDS->m_poMaskBand)
        anRequestedBands.push_back(0);
    for( int i = 1; i <= poGDS->GetRasterCount(); i++ )
        anRequestedBands.push_back(i);
    GUInt32 nRetryFlags = PrefetchBlocks(
        nXOff, nYOff, nXSize, nYSize, anRequestedBands);
    int nXOff1 = 0;
    int nYOff1 = 0;
    int nXSize1 = 0;
    int nYSize1 = 0;
    int nXOff2 = 0;
    int nYOff2 = 0;
    int nXSize2 = 0;
    int nYSize2 = 0;
    GSpacing nDataShift2 = 0;
    if( CanSpatiallySplit(nRetryFlags, nXOff, nYOff, nXSize, nYSize,
                          nBufXSize, nBufYSize,
                          nBlockXSize, nBlockYSize,
                          nPixelSpace, nLineSpace,
                          nXOff1, nYOff1,
                          nXSize1, nYSize1,
                          nXOff2, nYOff2,
                          nXSize2, nYSize2,
                          nDataShift2) )
    {
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        CPLErr eErr = IRasterIO(eRWFlag, nXOff1, nYOff1,
                                nXSize1, nYSize1,
                                pData,
                                nXSize1, nYSize1,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        if( eErr == CE_None )
        {
            eErr = IRasterIO(eRWFlag,
                                nXOff2, nYOff2,
                                nXSize2, nYSize2,
                                static_cast<GByte*>(pData) + nDataShift2,
                                nXSize2, nYSize2,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        }
        return eErr;
    }
    else if( (nRetryFlags & RETRY_PER_BAND) && poGDS->nBands > 1 )
    {
        CPL_IGNORE_RET_VAL(PrefetchBlocks(
            nXOff, nYOff, nXSize, nYSize, std::vector<int>{nBand}));
    }

    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                          AdviseRead()                                */
/************************************************************************/

CPLErr GDALDAASRasterBand::AdviseRead (int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nBufXSize,
                                        int nBufYSize,
                                        GDALDataType /* eBufType */,
                                        char ** /* papszOptions */)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);
    if( nXSize == nBufXSize && nYSize == nBufYSize )
    {
        poGDS->m_nXOffAdvise = nXOff;
        poGDS->m_nYOffAdvise = nYOff;
        poGDS->m_nXSizeAdvise = nXSize;
        poGDS->m_nYSizeAdvise = nYSize;
    }
    return CE_None;
}

/************************************************************************/
/*                          PrefetchBlocks()                            */
/************************************************************************/

// Return or'ed flags among 0, RETRY_PER_BAND, RETRY_SPATIAL_SPLIT if the user
// should try to split the request in smaller chunks

GUInt32 GDALDAASRasterBand::PrefetchBlocks(int nXOff, int nYOff,
                                           int nXSize, int nYSize,
                                           const std::vector<int>& anRequestedBands)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);

    if( anRequestedBands.size() > 1 )
    {
        if( poGDS->m_nXOffFetched == nXOff &&
            poGDS->m_nYOffFetched == nYOff &&
            poGDS->m_nXSizeFetched == nXSize &&
            poGDS->m_nYSizeFetched == nYSize )
        {
            return 0;
        }
        poGDS->m_nXOffFetched = nXOff;
        poGDS->m_nYOffFetched = nYOff;
        poGDS->m_nXSizeFetched = nXSize;
        poGDS->m_nYSizeFetched = nYSize;
    }

    int nBlockXOff = nXOff / nBlockXSize;
    int nBlockYOff = nYOff / nBlockYSize;
    int nXBlocks = (nXOff + nXSize - 1) / nBlockXSize - nBlockXOff + 1;
    int nYBlocks = (nYOff + nYSize - 1) / nBlockYSize - nBlockYOff + 1;

    int nTotalDataTypeSize = 0;
    const int nQueriedBands = static_cast<int>(anRequestedBands.size());
    for( int i = 0; i < nQueriedBands; i++ )
    {
        const int iBand = anRequestedBands[i];
        if( iBand > 0 && iBand <= poGDS->GetRasterCount() )
        {
            nTotalDataTypeSize += GDALGetDataTypeSizeBytes(
                    poGDS->GetRasterBand(iBand)->GetRasterDataType());
        }
        else
        {
            nTotalDataTypeSize += GDALGetDataTypeSizeBytes(
                    poGDS->m_poMaskBand->GetRasterDataType());
        }
    }

    // If AdviseRead() was called before, and the current requested area is
    // in it, check if we can prefetch the whole advised area
    const GIntBig nCacheMax = GDALGetCacheMax64()/2;
    if( poGDS->m_nXSizeAdvise > 0 &&
        nXOff >= poGDS->m_nXOffAdvise &&
        nYOff >= poGDS->m_nYOffAdvise &&
        nXOff + nXSize <= poGDS->m_nXOffAdvise + poGDS->m_nXSizeAdvise &&
        nYOff + nYSize <= poGDS->m_nYOffAdvise + poGDS->m_nYSizeAdvise )
    {
        int nBlockXOffAdvise = poGDS->m_nXOffAdvise / nBlockXSize;
        int nBlockYOffAdvise = poGDS->m_nYOffAdvise / nBlockYSize;
        int nXBlocksAdvise = (poGDS->m_nXOffAdvise +
            poGDS->m_nXSizeAdvise - 1) / nBlockXSize - nBlockXOffAdvise + 1;
        int nYBlocksAdvise = (poGDS->m_nYOffAdvise +
            poGDS->m_nYSizeAdvise - 1) / nBlockYSize - nBlockYOffAdvise + 1;
        const GIntBig nUncompressedSize =
            static_cast<GIntBig>(nXBlocksAdvise) * nYBlocksAdvise *
                        nBlockXSize * nBlockYSize * nTotalDataTypeSize;
        if( nUncompressedSize <= nCacheMax &&
            nUncompressedSize <= poGDS->m_nServerByteLimit )
        {
            CPLDebug("DAAS", "Using advise read");
            nBlockXOff = nBlockXOffAdvise;
            nBlockYOff = nBlockYOffAdvise;
            nXBlocks = nXBlocksAdvise;
            nYBlocks = nYBlocksAdvise;
            if( anRequestedBands.size() > 1 )
            {
                poGDS->m_nXOffAdvise = 0;
                poGDS->m_nYOffAdvise = 0;
                poGDS->m_nXSizeAdvise = 0;
                poGDS->m_nYSizeAdvise = 0;
            }
        }
    }

    // Check the number of already cached blocks, and remove fully
    // cached lines at the top of the area of interest from the queried
    // blocks
    int nBlocksCached = 0;
    int nBlocksCachedForThisBand = 0;
    bool bAllLineCached = true;
    for( int iYBlock = 0; iYBlock < nYBlocks; )
    {
        for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
        {
            for( int i = 0; i < nQueriedBands; i++ )
            {
                const int iBand = anRequestedBands[i];
                GDALRasterBlock* poBlock = nullptr;
                GDALDAASRasterBand* poIterBand;
                if( iBand > 0 && iBand <= poGDS->GetRasterCount() )
                    poIterBand = reinterpret_cast<GDALDAASRasterBand*>(
                                                    poGDS->GetRasterBand(iBand));
                else
                    poIterBand = poGDS->m_poMaskBand;

                poBlock = poIterBand->TryGetLockedBlockRef(
                    nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                if (poBlock != nullptr)
                {
                    nBlocksCached ++;
                    if( iBand == nBand )
                        nBlocksCachedForThisBand ++;
                    poBlock->DropLock();
                    continue;
                }
                else
                {
                    bAllLineCached = false;
                }
            }
        }

        if( bAllLineCached )
        {
            nBlocksCached -= nXBlocks * nQueriedBands;
            nBlocksCachedForThisBand -= nXBlocks;
            nBlockYOff ++;
            nYBlocks --;
        }
        else
        {
            iYBlock ++;
        }
    }

    if( nXBlocks > 0 && nYBlocks > 0 )
    {
        bool bMustReturn = false;
        GUInt32 nRetryFlags = 0;

        // Get the blocks if the number of already cached blocks is lesser
        // than 25% of the to be queried blocks
        if( nBlocksCached > (nQueriedBands * nXBlocks * nYBlocks) / 4 )
        {
            if( nBlocksCachedForThisBand <= (nXBlocks * nYBlocks) / 4 )
            {
                nRetryFlags |= RETRY_PER_BAND;
            }
            else
            {
                bMustReturn = true;
            }
        }

        // Make sure that we have enough cache (with a margin of 50%)
        // and the number of queried pixels isn't too big w.r.t server
        // limit
        const GIntBig nUncompressedSize =
            static_cast<GIntBig>(nXBlocks) * nYBlocks *
                        nBlockXSize * nBlockYSize * nTotalDataTypeSize;
        if( nUncompressedSize > nCacheMax ||
            nUncompressedSize > poGDS->m_nServerByteLimit )
        {
            if( anRequestedBands.size() > 1 && poGDS->GetRasterCount() > 1 )
            {
                const int nThisDTSize = GDALGetDataTypeSizeBytes(eDataType);
                const GIntBig nUncompressedSizeThisBand =
                    static_cast<GIntBig>(nXBlocks) * nYBlocks *
                            nBlockXSize * nBlockYSize * nThisDTSize;
                if( nUncompressedSizeThisBand <= poGDS->m_nServerByteLimit &&
                    nUncompressedSizeThisBand <= nCacheMax )
                {
                    nRetryFlags |= RETRY_PER_BAND;
                }
            }
            if( nXBlocks > 1 || nYBlocks > 1 )
            {
                nRetryFlags |= RETRY_SPATIAL_SPLIT;
            }
            return nRetryFlags;
        }
        if( bMustReturn )
            return nRetryFlags;

        GetBlocks(nBlockXOff, nBlockYOff, nXBlocks, nYBlocks,
                  anRequestedBands, nullptr);
    }

    return 0;
}

/************************************************************************/
/*                           GetBlocks()                                */
/************************************************************************/

CPLErr GDALDAASRasterBand::GetBlocks(int nBlockXOff, int nBlockYOff,
                                     int nXBlocks, int nYBlocks,
                                     const std::vector<int>& anRequestedBands,
                                     void* pDstBuffer)
{
    GDALDAASDataset* poGDS = reinterpret_cast<GDALDAASDataset*>(poDS);

    CPLAssert( !anRequestedBands.empty() );
    if( pDstBuffer )
    {
        CPLAssert( nXBlocks == 1 && nYBlocks == 1 && anRequestedBands.size() == 1 );
    }

    // Detect if there is a mix of non-mask and mask bands
    if( anRequestedBands.size() > 1 )
    {
        std::vector<int> anNonMasks;
        std::vector<int> anMasks;
        for( auto& iBand: anRequestedBands )
        {
            if( iBand == MAIN_MASK_BAND_NUMBER || poGDS->m_aoBandDesc[iBand-1].bIsMask )
                anMasks.push_back(iBand);
            else
                anNonMasks.push_back(iBand);
        }
        if( !anNonMasks.empty() && !anMasks.empty() )
        {
            return
                GetBlocks(nBlockXOff, nBlockYOff, nXBlocks, nYBlocks,
                          anNonMasks, nullptr) == CE_None &&
                GetBlocks(nBlockXOff, nBlockYOff, nXBlocks, nYBlocks,
                          anMasks, nullptr) == CE_None ? CE_None : CE_Failure;
        }
    }

    char** papszOptions = poGDS->GetHTTPOptions();

    CPLString osHeaders = CSLFetchNameValueDef(papszOptions, "HEADERS", "");
    if( !osHeaders.empty() )
        osHeaders += "\r\n";
    osHeaders += "Content-Type: application/json";
    osHeaders += "\r\n";
    CPLString osDataContentType("application/octet-stream");
    GDALDAASDataset::Format eRequestFormat(GDALDAASDataset::Format::RAW);
    if( poGDS->m_eFormat == GDALDAASDataset::Format::PNG &&
        (anRequestedBands.size() == 1 || anRequestedBands.size() == 3 ||
         anRequestedBands.size() == 4 ) )
    {
        eRequestFormat = poGDS->m_eFormat;
        osDataContentType = "image/png";
    }
    else if( poGDS->m_eFormat == GDALDAASDataset::Format::JPEG &&
            (anRequestedBands.size() == 1 || anRequestedBands.size() == 3) )
    {
        eRequestFormat = poGDS->m_eFormat;
        osDataContentType = "image/jpeg";
    }
    else if( poGDS->m_eFormat == GDALDAASDataset::Format::JPEG2000 )
    {
        eRequestFormat = poGDS->m_eFormat;
        osDataContentType = "image/jp2";
    }
    osHeaders += "Accept: " + osDataContentType;
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders);

    // Build request JSon document
    CPLJSONDocument oDoc;
    CPLJSONObject oBBox;

    if( poGDS->m_bRequestInGeoreferencedCoordinates )
    {
        CPLJSONObject oSRS;
        oSRS.Add("type", poGDS->m_osSRSType);
        oSRS.Add("value", poGDS->m_osSRSValue);
        oBBox.Add("srs", oSRS);
    }
    else
    {
        CPLJSONObject oSRS;
        oSRS.Add("type", "image");
        oBBox.Add("srs", oSRS);
    }

    const int nMainXSize = poGDS->m_poParentDS ?
                        poGDS->m_poParentDS->GetRasterXSize() : nRasterXSize;
    const int nMainYSize = poGDS->m_poParentDS ?
                        poGDS->m_poParentDS->GetRasterYSize() : nRasterYSize;
    const int nULX = nBlockXOff * nBlockXSize;
    const int nULY = nBlockYOff * nBlockYSize;
    const int nLRX = std::min(nRasterXSize,
                            (nBlockXOff + nXBlocks) * nBlockXSize);
    const int nLRY = std::min(nRasterYSize,
                            (nBlockYOff + nYBlocks) * nBlockYSize);

    CPLJSONObject oUL;
    CPLJSONObject oLR;
    if( poGDS->m_bRequestInGeoreferencedCoordinates )
    {
        double dfULX, dfULY;
        GDALApplyGeoTransform(poGDS->m_adfGeoTransform.data(),
                              nULX, nULY, &dfULX, &dfULY);
        oUL.Add("x", dfULX);
        oUL.Add("y", dfULY);

        double dfLRX, dfLRY;
        GDALApplyGeoTransform(poGDS->m_adfGeoTransform.data(),
                              nLRX, nLRY, &dfLRX, &dfLRY);
        oLR.Add("x", dfLRX);
        oLR.Add("y", dfLRY);
    }
    else
    {
        oUL.Add("x", static_cast<int>(
            (static_cast<GIntBig>(nULX) * nMainXSize) / nRasterXSize) );
        oUL.Add("y", static_cast<int>(
            (static_cast<GIntBig>(nULY) * nMainYSize) / nRasterYSize) );

        oLR.Add("x", (nLRX == nRasterXSize) ? nMainXSize : static_cast<int>(
            (static_cast<GIntBig>(nLRX) * nMainXSize) / nRasterXSize));
        oLR.Add("y", (nLRY == nRasterYSize) ? nMainYSize : static_cast<int>(
            (static_cast<GIntBig>(nLRY) * nMainYSize) / nRasterYSize));
    }
    oBBox.Add("ul", oUL);
    oBBox.Add("lr", oLR);
    oDoc.GetRoot().Add("bbox", oBBox);

    CPLJSONObject oTargetModel;

    CPLJSONObject oStepTargetModel;
    if( poGDS->m_bRequestInGeoreferencedCoordinates )
    {
        oStepTargetModel.Add("x", poGDS->m_adfGeoTransform[1]);
        oStepTargetModel.Add("y", fabs(poGDS->m_adfGeoTransform[5]));
    }
    else
    {
        oStepTargetModel.Add("x", 0);
        oStepTargetModel.Add("y", 0);
    }
    oTargetModel.Add("step", oStepTargetModel);

    CPLJSONObject oSize;
    int nRequestWidth = nLRX - nULX;
    int nRequestHeight = nLRY - nULY;
    oSize.Add("columns", nRequestWidth);
    oSize.Add("lines", nRequestHeight);
    oTargetModel.Add("size", oSize);

    if( poGDS->m_eCurrentResampleAlg == GRIORA_NearestNeighbour )
    {
        oTargetModel.Add("sampling-algo", "NEAREST");
    }
    else if( poGDS->m_eCurrentResampleAlg == GRIORA_Bilinear )
    {
        oTargetModel.Add("sampling-algo", "BILINEAR");
    }
    else if( poGDS->m_eCurrentResampleAlg == GRIORA_Cubic )
    {
        oTargetModel.Add("sampling-algo", "BICUBIC");
    }
    else if( poGDS->m_eCurrentResampleAlg == GRIORA_Average )
    {
        oTargetModel.Add("sampling-algo", "AVERAGE");
    }
    else
    {
        // Defaults to BILINEAR for other GDAL methods not supported by
        // server
        oTargetModel.Add("sampling-algo", "BILINEAR");
    }

    oTargetModel.Add("strictOutputSize", true);

    if( !poGDS->m_bRequestInGeoreferencedCoordinates )
    {
        CPLJSONObject oSRS;
        oSRS.Add("type", "image");
        oTargetModel.Add("srs", oSRS);
    }

    oDoc.GetRoot().Add("target-model", oTargetModel);


    CPLJSONArray oBands;
    bool bOK = true;
    for( auto& iBand: anRequestedBands )
    {
        auto desc = (iBand == MAIN_MASK_BAND_NUMBER) ?
                                   poGDS->m_poMaskBand->GetDescription() :
                                   poGDS->GetRasterBand(iBand)->GetDescription();
        if( EQUAL(desc, "" ) )
            bOK = false;
        else
            oBands.Add( desc );
    }
    if( bOK )
    {
        oDoc.GetRoot().Add("bands", oBands);
    }

    papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS",
                        oDoc.GetRoot().Format(CPLJSONObject::PrettyFormat::Pretty).c_str());

    CPLString osURL( CPLGetConfigOption("GDAL_DAAS_GET_BUFFER_URL",
                                        poGDS->m_osGetBufferURL.c_str()) );
    CPLHTTPResult* psResult = DAAS_CPLHTTPFetch(osURL, papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return CE_Failure;

    if( psResult->pszErrBuf != nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Get request %s failed: %s",
                  osURL.c_str(),
                  psResult->pabyData ? CPLSPrintf("%s: %s",
                    psResult->pszErrBuf,
                    reinterpret_cast<const char*>(psResult->pabyData )) :
                  psResult->pszErrBuf );
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }

    if( psResult->nDataLen == 0 )
    {
        // Presumably HTTP 204 empty
        CPLHTTPDestroyResult(psResult);

        for( int iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
        {
            for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
            {
                for( auto& iBand: anRequestedBands )
                {
                    GByte* pabyDstBuffer = nullptr;
                    GDALDAASRasterBand* poIterBand;
                    if( iBand == MAIN_MASK_BAND_NUMBER )
                    {
                        poIterBand = poGDS->m_poMaskBand;
                    }
                    else
                    {
                        poIterBand = reinterpret_cast<GDALDAASRasterBand*>(
                                                    poGDS->GetRasterBand(iBand));
                    }

                    GDALRasterBlock* poBlock = nullptr;
                    if(  pDstBuffer != nullptr )
                    {
                        pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
                    }
                    else
                    {
                        // Check if the same block in other bands is already in
                        // the GDAL block cache
                        poBlock = poIterBand->TryGetLockedBlockRef(
                                nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                        if( poBlock != nullptr )
                        {
                            // Yes, no need to do further work
                            poBlock->DropLock();
                            continue;
                        }
                        // Instantiate the block
                        poBlock = poIterBand->GetLockedBlockRef(
                                    nBlockXOff + iXBlock,
                                    nBlockYOff + iYBlock, TRUE);
                        if (poBlock == nullptr)
                        {
                            continue;
                        }
                        pabyDstBuffer = static_cast<GByte*>(poBlock->GetDataRef());
                    }

                    const int nDTSize = GDALGetDataTypeSizeBytes(
                        poIterBand->GetRasterDataType());
                    double dfNoDataValue = poIterBand->GetNoDataValue(nullptr);
                    GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                                  pabyDstBuffer,
                                  poIterBand->GetRasterDataType(),
                                  nDTSize,
                                  nBlockXSize * nBlockYSize);
                    if( poBlock )
                        poBlock->DropLock();
                }
            }
        }

        return CE_None;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("DAAS", "Response = '%s'",
             reinterpret_cast<const char*>(psResult->pabyData ));
#endif
    if( !CPLHTTPParseMultipartMime(psResult) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Get request %s failed: "
                 "Invalid content returned by server",
                 osURL.c_str());
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }
    int iMetadataPart = -1;
    int iDataPart = -1;
    // Identify metadata and data parts
    for( int i = 0; i < psResult->nMimePartCount; i++ )
    {
        const char* pszContentType = CSLFetchNameValue(
            psResult->pasMimePart[i].papszHeaders, "Content-Type");
        const char* pszContentDisposition = CSLFetchNameValue(
            psResult->pasMimePart[i].papszHeaders, "Content-Disposition");
        if( pszContentType )
        {
            if( EQUAL(pszContentType, "application/json") )
            {
                iMetadataPart = i;
            }
            else if( EQUAL(pszContentType, osDataContentType) )
            {
                iDataPart = i;
            }
        }
        if( pszContentDisposition )
        {
            if( EQUAL(pszContentDisposition, "form-data; name=\"Data\";") )
            {
                iDataPart = i;
            }
        }
    }
    if( iDataPart < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find part with Content-Type: %s in GetBuffer response",
                 osDataContentType.c_str());
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }
    if( iMetadataPart < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find part with Content-Type: %s in GetBuffer response",
                 "application/json");
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }

    CPLString osJson;
    osJson.assign(reinterpret_cast<const char*>(
                    psResult->pasMimePart[iMetadataPart].pabyData),
                  psResult->pasMimePart[iMetadataPart].nDataLen);
    CPLDebug("DAAS", "GetBuffer metadata response: %s", osJson.c_str());
    if( !oDoc.LoadMemory(osJson) )
    {
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }
    auto oDocRoot = oDoc.GetRoot();
    int nGotHeight = oDocRoot.GetInteger("properties/height");
    int nGotWidth = oDocRoot.GetInteger("properties/width");
    if( nGotHeight != nRequestHeight || nGotWidth != nRequestWidth )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Got buffer of size %dx%d, whereas %dx%d was expected",
                 nGotWidth, nGotHeight, nRequestWidth, nRequestHeight);
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }

    // Get the actual data type of the buffer response
    GDALDataType eBufferDataType =
        anRequestedBands[0] == MAIN_MASK_BAND_NUMBER ? GDT_Byte : poGDS->m_aoBandDesc[anRequestedBands[0]-1].eDT;
    auto oBandArray = oDocRoot.GetArray("properties/bands");
    if( oBandArray.IsValid() && oBandArray.Size() >= 1 )
    {
        bool bIgnored;
        auto oBandProperties = oBandArray[0];
        auto osPixelType =
            GetString(oBandProperties, "pixelType", false, bIgnored);
        if( !osPixelType.empty() )
        {
            eBufferDataType = GetGDALDataTypeFromDAASPixelType(osPixelType);
            if( eBufferDataType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Invalid pixelType: %s", osPixelType.c_str());
                CPLHTTPDestroyResult(psResult);
                return CE_Failure;
            }
        }
    }

    const int nBufferDTSize = GDALGetDataTypeSizeBytes(eBufferDataType);
    GDALDataset* poTileDS;
    if( eRequestFormat == GDALDAASDataset::Format::RAW )
    {
        int nExpectedBytes = nGotHeight * nGotWidth * nBufferDTSize *
            static_cast<int>(anRequestedBands.size());
        if( psResult->pasMimePart[iDataPart].nDataLen != nExpectedBytes )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Got buffer of %d bytes, whereas %d were expected",
                    psResult->pasMimePart[iDataPart].nDataLen, nExpectedBytes);
            CPLHTTPDestroyResult(psResult);
            return CE_Failure;
        }

        GByte* pabySrcData = psResult->pasMimePart[iDataPart].pabyData;
#ifdef CPL_MSB
        GDALSwapWords( pabySrcData,
                    nBufferDTSize,
                    nGotHeight * nGotWidth * static_cast<int>(anRequestedBands.size()),
                    nBufferDTSize );
#endif

        poTileDS = MEMDataset::Create(
            "", nRequestWidth, nRequestHeight, 0, eBufferDataType, nullptr);
        for( int i = 0; i < static_cast<int>(anRequestedBands.size()); i++ )
        {
            char szBuffer0[128] = {};
            char szBuffer[64] = {};
            int nRet = CPLPrintPointer(
                szBuffer,
                pabySrcData + i * nGotHeight * nGotWidth * nBufferDTSize,
                sizeof(szBuffer));
            szBuffer[nRet] = 0;
            snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);
            char* apszOptions[2] = { szBuffer0, nullptr };
            poTileDS->AddBand(eBufferDataType, apszOptions);
        }
    }
    else
    {
        CPLString osTmpMemFile = CPLSPrintf("/vsimem/daas_%p", this);
        VSIFCloseL( VSIFileFromMemBuffer( osTmpMemFile,
                                psResult->pasMimePart[iDataPart].pabyData,
                                psResult->pasMimePart[iDataPart].nDataLen,
                                false ) );
        poTileDS = reinterpret_cast<GDALDataset*>(
                    GDALOpenEx(osTmpMemFile, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                           nullptr, nullptr, nullptr));
        if( !poTileDS )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot decode image");
            VSIUnlink(osTmpMemFile);
            CPLHTTPDestroyResult(psResult);
            return CE_Failure;
        }
    }

    CPLErr eErr = CE_None;
    std::shared_ptr<GDALDataset> ds =
                std::shared_ptr<GDALDataset>(poTileDS);
    poTileDS->MarkSuppressOnClose();

    bool bExpectedImageCharacteristics =
         (poTileDS->GetRasterXSize() == nRequestWidth &&
          poTileDS->GetRasterYSize() == nRequestHeight);
    if( bExpectedImageCharacteristics )
    {
        if( poTileDS->GetRasterCount() == static_cast<int>(anRequestedBands.size()) )
        {
            // ok
        }
        else if( eRequestFormat == GDALDAASDataset::Format::PNG &&
                 anRequestedBands.size() == 1 &&
                 poTileDS->GetRasterCount() == 4 )
        {
            // ok
        }
        else
        {
            bExpectedImageCharacteristics = false;
        }
    }

    if( !bExpectedImageCharacteristics )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Got tile of size %dx%dx%d, whereas %dx%dx%d was expected",
            poTileDS->GetRasterXSize(),
            poTileDS->GetRasterYSize(),
            poTileDS->GetRasterCount(),
            nRequestWidth, nRequestHeight, static_cast<int>(anRequestedBands.size()));
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }

    for( int iYBlock = 0; eErr == CE_None && iYBlock < nYBlocks; iYBlock++ )
    {
        int nBlockActualYSize = std::min(nBlockYSize,
                    nRasterYSize - (iYBlock + nBlockYOff) * nBlockYSize);
        for( int iXBlock = 0; eErr == CE_None && iXBlock < nXBlocks; iXBlock++ )
        {
            int nBlockActualXSize = std::min(nBlockXSize,
                        nRasterXSize - (iXBlock + nBlockXOff) * nBlockXSize);

            for( int i = 0; i < static_cast<int>(anRequestedBands.size()); i++ )
            {
                const int iBand = anRequestedBands[i];
                GByte* pabyDstBuffer = nullptr;
                GDALDAASRasterBand* poIterBand;
                if( iBand == MAIN_MASK_BAND_NUMBER )
                {
                    poIterBand = poGDS->m_poMaskBand;
                }
                else
                {
                    poIterBand = reinterpret_cast<GDALDAASRasterBand*>(
                                                poGDS->GetRasterBand(iBand));
                }

                GDALRasterBlock* poBlock = nullptr;
                if( pDstBuffer != nullptr )
                    pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
                else
                {
                    // Check if the same block in other bands is already in
                    // the GDAL block cache
                    poBlock = poIterBand->TryGetLockedBlockRef(
                            nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                    if( poBlock != nullptr )
                    {
                        // Yes, no need to do further work
                        poBlock->DropLock();
                        continue;
                    }
                    // Instantiate the block
                    poBlock = poIterBand->GetLockedBlockRef(
                                nBlockXOff + iXBlock,
                                nBlockYOff + iYBlock, TRUE);
                    if (poBlock == nullptr)
                    {
                        continue;
                    }
                    pabyDstBuffer = static_cast<GByte*>(poBlock->GetDataRef());
                }

                GDALRasterBand* poTileBand = poTileDS->GetRasterBand(i + 1);
                const auto eIterBandDT = poIterBand->GetRasterDataType();
                const int nDTSize = GDALGetDataTypeSizeBytes(eIterBandDT);
                eErr = poTileBand->RasterIO(GF_Read,
                    iXBlock * nBlockXSize,
                    iYBlock * nBlockYSize,
                    nBlockActualXSize, nBlockActualYSize,
                    pabyDstBuffer,
                    nBlockActualXSize, nBlockActualYSize,
                    eIterBandDT,
                    nDTSize, nDTSize * nBlockXSize, nullptr);

                if( poBlock )
                    poBlock->DropLock();
                if( eErr != CE_None )
                    break;
            }
        }
    }

    CPLHTTPDestroyResult(psResult);
    return eErr;
}

/************************************************************************/
/*                       GDALRegister_DAAS()                            */
/************************************************************************/

void GDALRegister_DAAS()

{
    if( GDALGetDriverByName( "DAAS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "DAAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Airbus DS Intelligence "
                               "Data As A Service driver" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/daas.html" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='GET_METADATA_URL' type='string' "
        "description='URL to GetImageMetadata' "
        "required='true'/>"
"  <Option name='API_KEY' alt_config_option='GDAL_DAAS_API_KEY' type='string' "
        "description='API key'/>"
"  <Option name='CLIENT_ID' alt_config_option='GDAL_DAAS_CLIENT_ID' "
        "type='string' description='Client id'/>"
"  <Option name='ACCESS_TOKEN' alt_config_option='GDAL_DAAS_ACCESS_TOKEN' "
        "type='string' description='Authorization access token'/>"
"  <Option name='X_FORWARDED_USER' "
        "alt_config_option='GDAL_DAAS_X_FORWARDED_USER' type='string' "
        "description='User from which the request originates from'/>"
"  <Option name='BLOCK_SIZE' type='integer' "
                                "description='Size of a block' default='512'/>"
"  <Option name='PIXEL_ENCODING' type='string-select' "
                        "description='Format in which pixels are queried'>"
"       <Value>AUTO</Value>"
"       <Value>RAW</Value>"
"       <Value>PNG</Value>"
"       <Value>JPEG</Value>"
"       <Value>JPEG2000</Value>"
"   </Option>"
"  <Option name='TARGET_SRS' type='string' description="
                                "'SRS name for server-side reprojection.'/>"
"  <Option name='MASKS' type='boolean' "
                    "description='Whether to expose mask bands' default='YES'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "DAAS:" );

    poDriver->pfnIdentify = GDALDAASDataset::Identify;
    poDriver->pfnOpen = GDALDAASDataset::OpenStatic;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
