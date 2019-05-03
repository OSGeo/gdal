/******************************************************************************
 *
 * Project:  Earth Engine Data API Images driver
 * Purpose:  Earth Engine Data API Images driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Planet Labs
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
#include "eeda.h"
#include "ogrgeojsonreader.h"

#include <stdlib.h>
#include <limits>

std::vector<EEDAIBandDesc> BuildBandDescArray(json_object* poBands,
                                std::map<CPLString, CPLString>& oMapCodeToWKT)
{
    const auto nBandCount = json_object_array_length( poBands );
    std::vector<EEDAIBandDesc> aoBandDesc;

    for(auto i = decltype(nBandCount){0}; i < nBandCount; i++)
    {
        json_object* poBand = json_object_array_get_idx(poBands, i);
        if( poBand == nullptr || json_object_get_type(poBand) != json_type_object )
            continue;

        json_object* poId = CPL_json_object_object_get(poBand, "id");
        const char* pszBandId = json_object_get_string(poId);
        if( pszBandId == nullptr )
            continue;

        json_object* poDataType = CPL_json_object_object_get(poBand,
                                                             "dataType");
        if( poDataType == nullptr ||
            json_object_get_type(poDataType) != json_type_object )
        {
            continue;
        }

        json_object* poPrecision = CPL_json_object_object_get(poDataType,
                                                              "precision");
        const char* pszPrecision = json_object_get_string(poPrecision);
        if( pszPrecision == nullptr )
            continue;
        GDALDataType eDT = GDT_Byte;
        bool bSignedByte = false;
        if( EQUAL(pszPrecision, "INT") )
        {
            json_object* poRange = CPL_json_object_object_get(poDataType,
                                                              "range");
            if( poRange && json_object_get_type(poRange) == json_type_object )
            {
                int nMin = 0;
                int nMax = 0;
                json_object* poMin = CPL_json_object_object_get(poRange,
                                                                "min");
                if( poMin )
                {
                    nMin = json_object_get_int(poMin);
                }
                json_object* poMax = CPL_json_object_object_get(poRange,
                                                                "max");
                if( poMax )
                {
                    nMax = json_object_get_int(poMax);
                }

                if( nMin == -128 && nMax == 127 )
                {
                    bSignedByte = true;
                }
                else if( nMin < std::numeric_limits<GInt16>::min() )
                {
                    eDT = GDT_Int32;
                }
                else if( nMax > std::numeric_limits<GUInt16>::max() )
                {
                    eDT = GDT_UInt32;
                }
                else if( nMin < 0 )
                {
                    eDT = GDT_Int16;
                }
                else if( nMax > std::numeric_limits<GByte>::max() )
                {
                    eDT = GDT_UInt16;
                }
            }
        }
        else if( EQUAL(pszPrecision, "FLOAT") )
        {
            eDT = GDT_Float32;
        }
        else if( EQUAL(pszPrecision, "DOUBLE") )
        {
            eDT = GDT_Float64;
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unhandled dataType %s for band %s",
                     pszPrecision, pszBandId);
            continue;
        }

        json_object* poGrid = CPL_json_object_object_get(poBand,
                                                              "grid");
        if( poGrid == nullptr ||
            json_object_get_type(poGrid) != json_type_object )
        {
            continue;
        }

        CPLString osWKT;
        json_object* poCrs = CPL_json_object_object_get(poGrid,
                                                        "crsCode");
        if( poCrs == nullptr )
            poCrs = CPL_json_object_object_get(poGrid, "wkt");
        OGRSpatialReference oSRS;
        if( poCrs )
        {
            const char* pszStr = json_object_get_string(poCrs);
            if( pszStr == nullptr )
                continue;
            if( STARTS_WITH(pszStr, "SR-ORG:") )
            {
                // For EEDA:MCD12Q1 for example
                pszStr = CPLSPrintf("http://spatialreference.org/ref/sr-org/%s/",
                                    pszStr + strlen("SR-ORG:"));
            }

            std::map<CPLString, CPLString>::const_iterator oIter =
                oMapCodeToWKT.find(pszStr);
            if( oIter != oMapCodeToWKT.end() )
            {
                osWKT = oIter->second;
            }
            else if( oSRS.SetFromUserInput(pszStr) != OGRERR_NONE )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unrecognized crs: %s", pszStr);
                oMapCodeToWKT[pszStr] = "";
            }
            else
            {
                char* pszWKT = nullptr;
                oSRS.exportToWkt(&pszWKT);
                if( pszWKT != nullptr )
                    osWKT = pszWKT;
                CPLFree(pszWKT);
                oMapCodeToWKT[pszStr] = osWKT;
            }
        }

        json_object* poAT = CPL_json_object_object_get(poGrid,
                                                           "affineTransform");
        if( poAT == nullptr ||
            json_object_get_type(poAT) != json_type_object )
        {
            continue;
        }
        std::vector<double> adfGeoTransform{
            json_object_get_double(
                CPL_json_object_object_get(poAT, "translateX")),
            json_object_get_double(
                CPL_json_object_object_get(poAT, "scaleX")),
            json_object_get_double(
                CPL_json_object_object_get(poAT, "shearX")),
            json_object_get_double(
                CPL_json_object_object_get(poAT, "translateY")),
            json_object_get_double(
                CPL_json_object_object_get(poAT, "shearY")),
            json_object_get_double(
                CPL_json_object_object_get(poAT, "scaleY")),
        };

        json_object* poDimensions = CPL_json_object_object_get(poGrid,
                                                               "dimensions");
        if( poDimensions == nullptr ||
            json_object_get_type(poDimensions) != json_type_object )
        {
            continue;
        }
        json_object* poWidth = CPL_json_object_object_get(poDimensions,
                                                          "width");
        int nWidth = json_object_get_int(poWidth);
        json_object* poHeight = CPL_json_object_object_get(poDimensions,
                                                           "height");
        int nHeight = json_object_get_int(poHeight);

#if 0
        if( poWidth == nullptr && poHeight == nullptr && poX == nullptr && poY == nullptr &&
            dfResX == 1.0 && dfResY == 1.0 )
        {
            // e.g. EEDAI:LT5_L1T_8DAY_EVI/19840109
            const char* pszAuthorityName = oSRS.GetAuthorityName(nullptr);
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if( pszAuthorityName && pszAuthorityCode &&
                EQUAL(pszAuthorityName, "EPSG") &&
                EQUAL(pszAuthorityCode, "4326") )
            {
                dfX = -180;
                dfY = 90;
                nWidth = 1 << 30;
                nHeight = 1 << 29;
                dfResX = 360.0 / nWidth;
                dfResY = -dfResX;
            }
        }
#endif

        if( nWidth <= 0 || nHeight <= 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid width/height for band %s", pszBandId);
            continue;
        }

        EEDAIBandDesc oDesc;
        oDesc.osName = pszBandId;
        oDesc.osWKT = osWKT;
        oDesc.eDT = eDT;
        oDesc.bSignedByte = bSignedByte;
        oDesc.adfGeoTransform = adfGeoTransform;
        oDesc.nWidth = nWidth;
        oDesc.nHeight = nHeight;
        aoBandDesc.push_back(oDesc);
    }
    return aoBandDesc;
}

/************************************************************************/
/*                      GDALEEDABaseDataset()                           */
/************************************************************************/

GDALEEDABaseDataset::GDALEEDABaseDataset() :
    m_bMustCleanPersistent(false),
    m_nExpirationTime(0)
{
}

/************************************************************************/
/*                     ~GDALEEDABaseDataset()                           */
/************************************************************************/

GDALEEDABaseDataset::~GDALEEDABaseDataset()
{
    if( m_bMustCleanPersistent )
    {
        char **papszOptions =
            CSLSetNameValue(
                nullptr, "CLOSE_PERSISTENT", CPLSPrintf("EEDAI:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osBaseURL, papszOptions));
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                          ConvertPathToName()                        */
/************************************************************************/

CPLString GDALEEDABaseDataset::ConvertPathToName(const CPLString& path) {
    size_t end = path.find('/');
    CPLString folder = path.substr(0, end);

    if ( folder == "users" )
    {
        return "projects/earthengine-legacy/assets/" + path;
    }
    else if ( folder != "projects" )
    {
        return "projects/earthengine-public/assets/" + path;
    }

    // Find the start and end positions of the third segment, if it exists.
    int segment = 1;
    size_t start = 0;
    while ( end != std::string::npos && segment < 3 )
    {
        segment++;
        start = end + 1;
        end = path.find('/', start);
    }

    end = (end == std::string::npos) ? path.size() : end;
    // segment is 3 if path has at least 3 segments.
    if ( folder == "projects" && segment == 3 )
    {
        // If the first segment is "projects" and the third segment is "assets",
        // path is a name, so return as-is.
        if ( path.substr(start, end - start) == "assets" )
        {
            return path;
        }
    }
    return "projects/earthengine-legacy/assets/" + path;
}

/************************************************************************/
/*                          GetBaseHTTPOptions()                        */
/************************************************************************/

char** GDALEEDABaseDataset::GetBaseHTTPOptions()
{
    m_bMustCleanPersistent = true;

    char** papszOptions = nullptr;
    papszOptions =
        CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=EEDAI:%p", this));

    // Strategy to get the Bearer Authorization value:
    // - if it is specified in the EEDA_BEARER config option, use it
    // - otherwise if EEDA_BEARER_FILE is specified, read it and use its content
    // - otherwise if GOOGLE_APPLICATION_CREDENTIALS is specified, read the
    //   corresponding file to get the private key and client_email, to get a
    //   bearer using OAuth2ServiceAccount method
    // - otherwise if EEDA_PRIVATE_KEY and EEDA_CLIENT_EMAIL are set, use them
    //   to get a bearer using OAuth2ServiceAccount method
    // - otherwise if EEDA_PRIVATE_KEY_FILE and EEDA_CLIENT_EMAIL are set, use
    //   them to get a bearer

    CPLString osBearer(CPLGetConfigOption("EEDA_BEARER", m_osBearer));
    if( osBearer.empty() ||
            (!m_osBearer.empty() && time(nullptr) > m_nExpirationTime) )
    {
        CPLString osBearerFile(CPLGetConfigOption("EEDA_BEARER_FILE", ""));
        if( !osBearerFile.empty() )
        {
            VSILFILE* fp = VSIFOpenL(osBearerFile, "rb");
            if( fp == nullptr )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot open %s", osBearerFile.c_str());
            }
            else
            {
                char abyBuffer[512];
                size_t nRead = VSIFReadL(abyBuffer, 1, sizeof(abyBuffer), fp);
                osBearer.assign(abyBuffer, nRead);
                VSIFCloseL(fp);
            }
        }
        else
        {
            CPLString osPrivateKey(CPLGetConfigOption("EEDA_PRIVATE_KEY", ""));
            CPLString osClientEmail(CPLGetConfigOption("EEDA_CLIENT_EMAIL", ""));

            if( osPrivateKey.empty() )
            {
                CPLString osPrivateKeyFile(
                            CPLGetConfigOption("EEDA_PRIVATE_KEY_FILE", ""));
                if( !osPrivateKeyFile.empty() )
                {
                    VSILFILE* fp = VSIFOpenL(osPrivateKeyFile, "rb");
                    if( fp == nullptr )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                "Cannot open %s", osPrivateKeyFile.c_str());
                    }
                    else
                    {
                        char* pabyBuffer = static_cast<char*>(CPLMalloc(32768));
                        size_t nRead = VSIFReadL(pabyBuffer, 1, 32768, fp);
                        osPrivateKey.assign(pabyBuffer, nRead);
                        VSIFCloseL(fp);
                        CPLFree(pabyBuffer);
                    }
                }
            }

            CPLString osServiceAccountJson(
                CPLGetConfigOption("GOOGLE_APPLICATION_CREDENTIALS", ""));
            if( !osServiceAccountJson.empty() )
            {
                CPLJSONDocument oDoc;
                if( !oDoc.Load(osServiceAccountJson) )
                {
                    CSLDestroy(papszOptions);
                    return nullptr;
                }

                osPrivateKey = oDoc.GetRoot().GetString("private_key");
                osPrivateKey.replaceAll("\\n", "\n");
                osClientEmail = oDoc.GetRoot().GetString("client_email");
            }

            char** papszMD = nullptr;
            if( !osPrivateKey.empty() && !osClientEmail.empty() )
            {
                CPLDebug("EEDA", "Requesting Bearer token");
                osPrivateKey.replaceAll("\\n", "\n");
                //CPLDebug("EEDA", "Private key: %s", osPrivateKey.c_str());
                papszMD =
                    GOA2GetAccessTokenFromServiceAccount(
                        osPrivateKey,
                        osClientEmail,
                        "https://www.googleapis.com/auth/earthengine.readonly",
                        nullptr, nullptr);
                if( papszMD == nullptr )
                {
                    CSLDestroy(papszOptions);
                    return nullptr;
                }
            }
            // Some Travis-CI workers are GCE machines, and for some tests, we don't
            // want this code path to be taken. And on AppVeyor/Window, we would also
            // attempt a network access
            else if( !CPLTestBool(CPLGetConfigOption("CPL_GCE_SKIP", "NO")) &&
                    CPLIsMachinePotentiallyGCEInstance() )
            {
                papszMD = GOA2GetAccessTokenFromCloudEngineVM(nullptr);
            }

            if( papszMD )
            {
                osBearer = CSLFetchNameValueDef(papszMD, "access_token", "");
                m_osBearer = osBearer;
                m_nExpirationTime = CPLAtoGIntBig(
                    CSLFetchNameValueDef(papszMD, "expires_in", "0"));
                if( m_nExpirationTime != 0 )
                    m_nExpirationTime += time(nullptr) - 10;
                CSLDestroy(papszMD);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing EEDA_BEARER, EEDA_BEARER_FILE or "
                    "GOOGLE_APPLICATION_CREDENTIALS or "
                    "EEDA_PRIVATE_KEY/EEDA_PRIVATE_KEY_FILE + "
                    "EEDA_CLIENT_EMAIL config option");
                CSLDestroy(papszOptions);
                return nullptr;
            }
        }
    }
    papszOptions =
        CSLAddString(papszOptions,
                     CPLSPrintf("HEADERS=Authorization: Bearer %s",
                                osBearer.c_str()));

    return papszOptions;
}


/* Add a small amount of random jitter to avoid cyclic server stampedes */
static double EEDABackoffFactor(double base)
{
    // coverity[dont_call]
    return base + rand() * 0.5 / RAND_MAX;
}

/************************************************************************/
/*                           EEDAHTTPFetch()                            */
/************************************************************************/

CPLHTTPResult* EEDAHTTPFetch(const char* pszURL, char** papszOptions)
{
    CPLHTTPResult* psResult;
    const int RETRY_COUNT = 4;
    double dfRetryDelay = 1.0;
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
            const char* pszErrorText = psResult->pszErrBuf ? psResult->pszErrBuf : "(null)";

            /* Get HTTP status code */
            int nHTTPStatus = -1;
            if( psResult->pszErrBuf != nullptr &&
                EQUALN(psResult->pszErrBuf, "HTTP error code : ", strlen("HTTP error code : ")) )
            {
                nHTTPStatus = atoi(psResult->pszErrBuf + strlen("HTTP error code : "));
                if( psResult->pabyData )
                    pszErrorText = reinterpret_cast<const char*>(psResult->pabyData);
            }

            if( (nHTTPStatus == 429 || nHTTPStatus == 500 ||
                 (nHTTPStatus >= 502 && nHTTPStatus <= 504)) &&
                 i < RETRY_COUNT )
            {
                CPLError( CE_Warning, CPLE_FileIO,
                          "GET error when downloading %s, HTTP status=%d, retrying in %.2fs : %s",
                          pszURL, nHTTPStatus, dfRetryDelay, pszErrorText);
                CPLHTTPDestroyResult(psResult);
                psResult = nullptr;

                CPLSleep( dfRetryDelay );
                dfRetryDelay *= EEDABackoffFactor(4);
            }
            else
            {
                break;
            }
        }
    }

    return psResult;
}
