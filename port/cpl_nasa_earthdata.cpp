/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement credential provider for accessing NASA Earthdata resources
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifdef HAVE_CURL

#include "cpl_error.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "cpl_mem_cache.h"
#include "cpl_nasa_earthdata.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

#include <mutex>

/************************************************************************/
/*                 CPLNasaEarthdataCredentialProvider()                 */
/************************************************************************/

CPLNasaEarthdataCredentialProvider::CPLNasaEarthdataCredentialProvider() =
    default;

/************************************************************************/
/*             CPLNasaEarthdataCredentialProvider::Build()              */
/************************************************************************/

/* static */
std::unique_ptr<CPLNasaEarthdataCredentialProvider>
CPLNasaEarthdataCredentialProvider::Build(
    const std::string &osGetCredentialsURL, const std::string &osEarthdataHost,
    const std::string &osEarthdataToken, const std::string &osEarthdataUsername,
    const std::string &osEarthdataPassword, const std::string &osNetrcFilename)
{
    if (osGetCredentialsURL.empty())
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Earthdata credentials provider: Credentials URL must not be "
                 "empty");
        return nullptr;
    }

    std::string l_osEarthdataHost = osEarthdataHost;
    if (l_osEarthdataHost.empty())
        l_osEarthdataHost = "urs.earthdata.nasa.gov";

    std::string l_osEarthdataToken = osEarthdataToken;
    std::string l_osEarthdataUsername = osEarthdataUsername;
    std::string l_osEarthdataPassword = osEarthdataPassword;

#define osEarthdataHost no_longer_use_me
#define osEarthdataToken no_longer_use_me
#define osEarthdataUsername no_longer_use_me
#define osEarthdataPassword no_longer_use_me

    if (l_osEarthdataToken.empty() &&
        (l_osEarthdataUsername.empty() != l_osEarthdataPassword.empty()))
    {
        CPLError(
            CE_Failure, CPLE_IllegalArg,
            "Both Earthdata username and password must be provided, or none");
        return nullptr;
    }

    if (l_osEarthdataToken.empty() && l_osEarthdataUsername.empty())
    {
        std::string l_osNetrcFilename = osNetrcFilename;
        if (l_osNetrcFilename.empty())
        {
            l_osNetrcFilename = CPLGetConfigOption("NETRC", "");
        }
        if (l_osNetrcFilename.empty())
        {
            const char *pszHomeDir = CPLGetHomeDir();
            if (!pszHomeDir)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Earthdata credentials provider: HOME is not set, and no "
                    "other Earthdata login mechanism defined (EARTHDATA_TOKEN, "
                    "EARTHDATA_USERNAME+EARTHDATA_PASSWORD or NETRC)");
                return nullptr;
            }
#ifdef _WIN32
            constexpr const char *pszNetrcFile = "_netrc";
#else
            constexpr const char *pszNetrcFile = ".netrc";
#endif
            l_osNetrcFilename =
                CPLFormFilenameSafe(pszHomeDir, pszNetrcFile, nullptr);
        }
        auto fp =
            VSIFilesystemHandler::OpenStatic(l_osNetrcFilename.c_str(), "rb");
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: cannot open %s, and no "
                     "other Earthdata login mechanism defined (EARTHDATA_TOKEN "
                     "or EARTHDATA_USERNAME+EARTHDATA_PASSWORD)",
                     l_osNetrcFilename.c_str());
            return nullptr;
        }
        constexpr int MAXLINE_LENGTH = 1024;  // Arbitrary
        std::string osExpectedLineStart("machine ");
        osExpectedLineStart += l_osEarthdataHost;
        osExpectedLineStart += ' ';
        bool bMatchFound = false;
        while (const char *pszLine =
                   CPLReadLine2L(fp.get(), MAXLINE_LENGTH, nullptr))
        {
            if (STARTS_WITH(pszLine, osExpectedLineStart.c_str()))
            {
                bMatchFound = true;
                const CPLStringList aosTokens(CSLTokenizeString2(
                    pszLine + osExpectedLineStart.size(), " ", 0));
                for (int i = 0; i < aosTokens.size(); ++i)
                {
                    if (EQUAL(aosTokens[i], "login") &&
                        i + 1 < aosTokens.size())
                    {
                        l_osEarthdataUsername = aosTokens[i + 1];
                        ++i;
                    }
                    else if (EQUAL(aosTokens[i], "password") &&
                             i + 1 < aosTokens.size())
                    {
                        l_osEarthdataPassword = aosTokens[i + 1];
                        ++i;
                    }
                }
                break;
            }
        }
        if (!bMatchFound)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Earthdata credentials provider: no credentials for host %s "
                "found in %s, and no other Earthdata login mechanism defined "
                "(EARTHDATA_TOKEN or EARTHDATA_USERNAME+EARTHDATA_PASSWORD)",
                l_osEarthdataHost.c_str(), l_osNetrcFilename.c_str());
            return nullptr;
        }
        if (l_osEarthdataUsername.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: line with credentials "
                     "for host %s found in %s, but missing 'login'",
                     l_osEarthdataHost.c_str(), l_osNetrcFilename.c_str());
            return nullptr;
        }
        if (l_osEarthdataPassword.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: line with credentials "
                     "for host %s found in %s, but missing 'password'",
                     l_osEarthdataHost.c_str(), l_osNetrcFilename.c_str());
            return nullptr;
        }
    }

    if (l_osEarthdataToken.empty())
    {
        CPLStringList aosOptions;
        aosOptions.SetNameValue("CUSTOMREQUEST", "POST");
        aosOptions.SetNameValue("USERPWD", std::string(l_osEarthdataUsername)
                                               .append(":")
                                               .append(l_osEarthdataPassword)
                                               .c_str());
        aosOptions.SetNameValue("ACCEPT", "application/json");
        std::string osURL;
        if (!cpl::starts_with(l_osEarthdataHost, "http://") &&
            !cpl::starts_with(l_osEarthdataHost, "https://"))
        {
            osURL += "https://";
        }
        osURL += l_osEarthdataHost;
        osURL += "/api/users/find_or_create_token";
        CPLHTTPResult *psResult =
            CPLHTTPFetch(osURL.c_str(), aosOptions.List());
        if (!psResult || psResult->nStatus != 0 || !psResult->pabyData)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: request to %s to get "
                     "access token failed: %s",
                     osURL.c_str(),
                     psResult && psResult->pszErrBuf ? psResult->pszErrBuf
                                                     : "(null)");
            CPLHTTPDestroyResult(psResult);
            return nullptr;
        }

        const CPLStringList aosResponse =
            CPLParseKeyValueJson(reinterpret_cast<char *>(psResult->pabyData));
        CPLHTTPDestroyResult(psResult);

        if (const char *pszError = aosResponse.FetchNameValue("error"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: %s in response of %s",
                     pszError, osURL.c_str());
            return nullptr;
        }

        const char *pszAccessToken = aosResponse.FetchNameValue("access_token");
        if (!pszAccessToken)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: missing 'access_token' "
                     "in response of %s",
                     osURL.c_str());
            return nullptr;
        }
        const char *pszTokenType = aosResponse.FetchNameValue("token_type");
        if (!pszTokenType)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: missing 'token_type' in "
                     "response of %s",
                     osURL.c_str());
            return nullptr;
        }
        constexpr const char *pszExpectedTokenType = "Bearer";
        if (!EQUAL(pszTokenType, pszExpectedTokenType))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: in response of %s, got "
                     "'token_type'='%s'. Expected '%s'",
                     osURL.c_str(), pszTokenType, pszExpectedTokenType);
            return nullptr;
        }
        l_osEarthdataToken = pszAccessToken;
    }

    auto poProvider = std::unique_ptr<CPLNasaEarthdataCredentialProvider>(
        new CPLNasaEarthdataCredentialProvider());
    poProvider->m_osGetCredentialsURL = osGetCredentialsURL;
    poProvider->m_osEarthdataToken = l_osEarthdataToken;
    if (!poProvider->RefreshIfNeeded())
        return nullptr;
    return poProvider;
}

/************************************************************************/
/*        CPLNasaEarthdataCredentialProvider::RefreshIfNeeded()         */
/************************************************************************/

bool CPLNasaEarthdataCredentialProvider::RefreshIfNeeded()
{
    std::lock_guard oLock(m_oMutex);

    constexpr int knExpirationDelayMargin = 60;
    if (m_osAccessKeyId.empty() ||
        time(nullptr) + knExpirationDelayMargin > m_nTokenExpirationTimestamp)
    {
        m_osAccessKeyId.clear();
        m_osSecretAccessKey.clear();
        m_osSessionToken.clear();
        m_nTokenExpirationTimestamp = 0;

        CPLStringList aosOptions;
        aosOptions.SetNameValue("HTTPAUTH", "BEARER");
        aosOptions.SetNameValue("HTTP_BEARER", m_osEarthdataToken.c_str());
        CPLHTTPResult *psResult =
            CPLHTTPFetch(m_osGetCredentialsURL.c_str(), aosOptions.List());
        if (!psResult || psResult->nStatus != 0 || !psResult->pabyData)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: request to %s to get "
                     "access token failed: %s",
                     m_osGetCredentialsURL.c_str(),
                     psResult && psResult->pszErrBuf ? psResult->pszErrBuf
                                                     : "(null)");
            CPLHTTPDestroyResult(psResult);
            return false;
        }

        const CPLStringList aosResponse =
            CPLParseKeyValueJson(reinterpret_cast<char *>(psResult->pabyData));
        CPLHTTPDestroyResult(psResult);

        m_osAccessKeyId = aosResponse.FetchNameValueDef("accessKeyId", "");
        m_osSecretAccessKey =
            aosResponse.FetchNameValueDef("secretAccessKey", "");
        m_osSessionToken = aosResponse.FetchNameValueDef("sessionToken", "");
        const char *pszExpiration = aosResponse.FetchNameValue("expiration");
        if (m_osAccessKeyId.empty() || m_osSecretAccessKey.empty() ||
            m_osSessionToken.empty() || !pszExpiration)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: request to %s failed to "
                     "return one of 'accessKeyId', 'secretAccessKey', "
                     "'sessionToken' and/or 'expiration'",
                     m_osGetCredentialsURL.c_str());
            return false;
        }

        int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMin = 0, nSec = 0;
        if (sscanf(pszExpiration, "%04d-%02d-%02d %02d:%02d:%02d+00:00", &nYear,
                   &nMonth, &nDay, &nHour, &nMin, &nSec) != 6)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Earthdata credentials provider: request to %s returned "
                     "expiration='%s' which is an unexpected time format",
                     m_osGetCredentialsURL.c_str(), pszExpiration);
            return false;
        }
        struct tm brokendowntime;
        brokendowntime.tm_year = nYear - 1900;
        brokendowntime.tm_mon = nMonth - 1;
        brokendowntime.tm_mday = nDay;
        brokendowntime.tm_hour = nHour;
        brokendowntime.tm_min = nMin;
        brokendowntime.tm_sec = nSec;
        m_nTokenExpirationTimestamp = CPLYMDHMSToUnixTime(&brokendowntime);

        CPLDebug("EARTHDATA", "Got S3 credentials until %s", pszExpiration);
    }

    return true;
}

/************************************************************************/
/*                              GetCache()                              */
/************************************************************************/

using EarthdataCacheType =
    lru11::Cache<std::string,
                 std::shared_ptr<CPLNasaEarthdataCredentialProvider>,
                 std::mutex>;

static EarthdataCacheType &GetCache()
{
    static EarthdataCacheType oCache;
    return oCache;
}

/************************************************************************/
/*              CPLNasaEarthdataCredentialProvider::Get()               */
/************************************************************************/

/* static */
std::shared_ptr<CPLNasaEarthdataCredentialProvider>
CPLNasaEarthdataCredentialProvider::Get(const std::string &osFilename,
                                        bool *pbErrorOccurred)
{
    if (pbErrorOccurred)
        *pbErrorOccurred = false;

    const char *pszCredentialsURL = VSIGetPathSpecificOption(
        osFilename.c_str(), "VSIS3_EARTHDATA_CREDENTIALS_URL", nullptr);
    if (!pszCredentialsURL)
        return nullptr;

    const char *pszEarthdataHost = VSIGetPathSpecificOption(
        osFilename.c_str(), "EARTHDATA_HOST",
        VSIGetPathSpecificOption(osFilename.c_str(), "DEFAULT_EARTHDATA_HOST",
                                 ""));
    const char *pszEarthdataToken =
        VSIGetPathSpecificOption(osFilename.c_str(), "EARTHDATA_TOKEN", "");
    const char *pszEarthdataUserame =
        VSIGetPathSpecificOption(osFilename.c_str(), "EARTHDATA_USERNAME", "");
    const char *pszEarthdataPassword =
        VSIGetPathSpecificOption(osFilename.c_str(), "EARTHDATA_PASSWORD", "");

    auto &oCache = GetCache();
    std::shared_ptr<CPLNasaEarthdataCredentialProvider> ret;
    std::string osCacheKey = pszCredentialsURL;
    osCacheKey += '|';
    osCacheKey += pszEarthdataHost;
    osCacheKey += '|';
    osCacheKey += pszEarthdataToken;
    osCacheKey += '|';
    osCacheKey += pszEarthdataUserame;
    osCacheKey += '|';
    osCacheKey += pszEarthdataPassword;
    if (!oCache.tryGet(osCacheKey, ret))
    {
        ret = Build(pszCredentialsURL, pszEarthdataHost, pszEarthdataToken,
                    pszEarthdataUserame, pszEarthdataPassword);
        if (ret)
        {
            oCache.insert(osCacheKey, ret);
        }
        else if (pbErrorOccurred)
        {
            *pbErrorOccurred = true;
        }
    }
    return ret;
}

/************************************************************************/
/*           CPLNasaEarthdataCredentialProvider::ClearCache()           */
/************************************************************************/

/* static */
void CPLNasaEarthdataCredentialProvider::ClearCache()
{
    GetCache().clear();
}

#endif  // HAVE_CURL
