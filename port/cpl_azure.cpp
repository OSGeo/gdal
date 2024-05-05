/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  Microsoft Azure Storage Blob routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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
 * DEALINAzureBlob IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_azure.h"
#include "cpl_json.h"
#include "cpl_vsi_error.h"
#include "cpl_sha256.h"
#include "cpl_time.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_vsi_virtual.h"

#include <mutex>

//! @cond Doxygen_Suppress

#ifdef HAVE_CURL

/************************************************************************/
/*                      RemoveTrailingSlash()                           */
/************************************************************************/

static std::string RemoveTrailingSlash(const std::string &osStr)
{
    std::string osRet(osStr);
    if (!osRet.empty() && osRet.back() == '/')
        osRet.pop_back();
    return osRet;
}

/************************************************************************/
/*                     CPLAzureGetSignature()                           */
/************************************************************************/

static std::string CPLAzureGetSignature(const std::string &osStringToSign,
                                        const std::string &osStorageKeyB64)
{

    /* -------------------------------------------------------------------- */
    /*      Compute signature.                                              */
    /* -------------------------------------------------------------------- */

    std::string osStorageKeyUnbase64(osStorageKeyB64);
    int nB64Length = CPLBase64DecodeInPlace(
        reinterpret_cast<GByte *>(&osStorageKeyUnbase64[0]));
    osStorageKeyUnbase64.resize(nB64Length);
#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "signing key size: %d", nB64Length);
#endif

    GByte abySignature[CPL_SHA256_HASH_SIZE] = {};
    CPL_HMAC_SHA256(osStorageKeyUnbase64.c_str(), nB64Length,
                    osStringToSign.c_str(), osStringToSign.size(),
                    abySignature);

    char *pszB64Signature = CPLBase64Encode(CPL_SHA256_HASH_SIZE, abySignature);
    std::string osSignature(pszB64Signature);
    CPLFree(pszB64Signature);
    return osSignature;
}

/************************************************************************/
/*                          GetAzureBlobHeaders()                       */
/************************************************************************/

static struct curl_slist *GetAzureBlobHeaders(
    const std::string &osVerb, const struct curl_slist *psExistingHeaders,
    const std::string &osResource,
    const std::map<std::string, std::string> &oMapQueryParameters,
    const std::string &osStorageAccount, const std::string &osStorageKeyB64,
    bool bIncludeMSVersion)
{
    /* See
     * https://docs.microsoft.com/en-us/rest/api/storageservices/authentication-for-the-azure-storage-services
     */

    std::string osDate = CPLGetConfigOption("CPL_AZURE_TIMESTAMP", "");
    if (osDate.empty())
    {
        osDate = IVSIS3LikeHandleHelper::GetRFC822DateTime();
    }
    if (osStorageKeyB64.empty())
    {
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(
            headers, CPLSPrintf("x-ms-date: %s", osDate.c_str()));
        return headers;
    }

    std::string osMsVersion("2019-12-12");
    std::map<std::string, std::string> oSortedMapMSHeaders;
    if (bIncludeMSVersion)
        oSortedMapMSHeaders["x-ms-version"] = osMsVersion;
    oSortedMapMSHeaders["x-ms-date"] = osDate;
    std::string osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
            oSortedMapMSHeaders, psExistingHeaders, "x-ms-"));

    std::string osCanonicalizedResource;
    osCanonicalizedResource += "/" + osStorageAccount;
    osCanonicalizedResource += osResource;

    // We assume query parameters are in lower case and they are not repeated
    std::map<std::string, std::string>::const_iterator oIter =
        oMapQueryParameters.begin();
    for (; oIter != oMapQueryParameters.end(); ++oIter)
    {
        osCanonicalizedResource += "\n";
        osCanonicalizedResource += oIter->first;
        osCanonicalizedResource += ":";
        osCanonicalizedResource += oIter->second;
    }

    std::string osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-Encoding") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-Language") + "\n";
    std::string osContentLength(
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-Length"));
    if (osContentLength == "0")
        osContentLength.clear();  // since x-ms-version 2015-02-21
    osStringToSign += osContentLength + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Date") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "If-Modified-Since") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "If-Match") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "If-None-Match") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "If-Unmodified-Since") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Range") + "\n";
    osStringToSign += osCanonicalizedHeaders;
    osStringToSign += osCanonicalizedResource;

#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "osStringToSign = '%s'", osStringToSign.c_str());
#endif

    /* -------------------------------------------------------------------- */
    /*      Compute signature.                                              */
    /* -------------------------------------------------------------------- */

    std::string osAuthorization(
        "SharedKey " + osStorageAccount + ":" +
        CPLAzureGetSignature(osStringToSign, osStorageKeyB64));

    struct curl_slist *headers = nullptr;
    headers =
        curl_slist_append(headers, CPLSPrintf("x-ms-date: %s", osDate.c_str()));
    if (bIncludeMSVersion)
    {
        headers = curl_slist_append(
            headers, CPLSPrintf("x-ms-version: %s", osMsVersion.c_str()));
    }
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    return headers;
}

/************************************************************************/
/*                     VSIAzureBlobHandleHelper()                       */
/************************************************************************/
VSIAzureBlobHandleHelper::VSIAzureBlobHandleHelper(
    const std::string &osPathForOption, const std::string &osEndpoint,
    const std::string &osBucket, const std::string &osObjectKey,
    const std::string &osStorageAccount, const std::string &osStorageKey,
    const std::string &osSAS, const std::string &osAccessToken,
    bool bFromManagedIdentities)
    : m_osPathForOption(osPathForOption),
      m_osURL(BuildURL(osEndpoint, osBucket, osObjectKey, osSAS)),
      m_osEndpoint(osEndpoint), m_osBucket(osBucket),
      m_osObjectKey(osObjectKey), m_osStorageAccount(osStorageAccount),
      m_osStorageKey(osStorageKey), m_osSAS(osSAS),
      m_osAccessToken(osAccessToken),
      m_bFromManagedIdentities(bFromManagedIdentities)
{
}

/************************************************************************/
/*                     ~VSIAzureBlobHandleHelper()                      */
/************************************************************************/

VSIAzureBlobHandleHelper::~VSIAzureBlobHandleHelper()
{
}

/************************************************************************/
/*                       AzureCSGetParameter()                          */
/************************************************************************/

static std::string AzureCSGetParameter(const std::string &osStr,
                                       const char *pszKey, bool bErrorIfMissing)
{
    std::string osKey(pszKey + std::string("="));
    size_t nPos = osStr.find(osKey);
    if (nPos == std::string::npos)
    {
        const char *pszMsg =
            CPLSPrintf("%s missing in AZURE_STORAGE_CONNECTION_STRING", pszKey);
        if (bErrorIfMissing)
        {
            CPLDebug("AZURE", "%s", pszMsg);
            VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
        }
        return std::string();
    }
    size_t nPos2 = osStr.find(";", nPos);
    return osStr.substr(nPos + osKey.size(), nPos2 == std::string::npos
                                                 ? nPos2
                                                 : nPos2 - nPos - osKey.size());
}

/************************************************************************/
/*                         CPLAzureCachedToken                          */
/************************************************************************/

std::mutex gMutex;

struct CPLAzureCachedToken
{
    std::string osAccessToken{};
    GIntBig nExpiresOn = 0;
};

static std::map<std::string, CPLAzureCachedToken> goMapIMDSURLToCachedToken;

/************************************************************************/
/*                GetConfigurationFromIMDSCredentials()                 */
/************************************************************************/

static bool
GetConfigurationFromIMDSCredentials(const std::string &osPathForOption,
                                    std::string &osAccessToken)
{
    // coverity[tainted_data]
    const std::string osRootURL(CPLGetConfigOption("CPL_AZURE_VM_API_ROOT_URL",
                                                   "http://169.254.169.254"));
    if (osRootURL == "disabled")
        return false;

    std::string osURLResource("/metadata/identity/oauth2/"
                              "token?api-version=2018-02-01&resource=https%"
                              "3A%2F%2Fstorage.azure.com%2F");
    const char *pszObjectId = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "AZURE_IMDS_OBJECT_ID", nullptr);
    if (pszObjectId)
        osURLResource += "&object_id=" + CPLAWSURLEncode(pszObjectId, false);
    const char *pszClientId = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "AZURE_IMDS_CLIENT_ID", nullptr);
    if (pszClientId)
        osURLResource += "&client_id=" + CPLAWSURLEncode(pszClientId, false);
    const char *pszMsiResId = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "AZURE_IMDS_MSI_RES_ID", nullptr);
    if (pszMsiResId)
        osURLResource += "&msi_res_id=" + CPLAWSURLEncode(pszMsiResId, false);

    std::lock_guard<std::mutex> guard(gMutex);

    // Look for cached token corresponding to this IMDS request URL
    auto oIter = goMapIMDSURLToCachedToken.find(osURLResource);
    if (oIter != goMapIMDSURLToCachedToken.end())
    {
        const auto &oCachedToken = oIter->second;
        time_t nCurTime;
        time(&nCurTime);
        // Try to reuse credentials if they are still valid, but
        // keep one minute of margin...
        if (nCurTime < oCachedToken.nExpiresOn - 60)
        {
            osAccessToken = oCachedToken.osAccessToken;
            return true;
        }
    }

    // Fetch credentials
    CPLStringList oResponse;
    const char *const apszOptions[] = {"HEADERS=Metadata: true", nullptr};
    CPLHTTPResult *psResult =
        CPLHTTPFetch((osRootURL + osURLResource).c_str(), apszOptions);
    if (psResult)
    {
        if (psResult->nStatus == 0 && psResult->pabyData != nullptr)
        {
            const std::string osJSon =
                reinterpret_cast<char *>(psResult->pabyData);
            oResponse = CPLParseKeyValueJson(osJSon.c_str());
            if (oResponse.FetchNameValue("error"))
            {
                CPLDebug("AZURE",
                         "Cannot retrieve managed identities credentials: %s",
                         osJSon.c_str());
            }
        }
        CPLHTTPDestroyResult(psResult);
    }
    osAccessToken = oResponse.FetchNameValueDef("access_token", "");
    const GIntBig nExpiresOn =
        CPLAtoGIntBig(oResponse.FetchNameValueDef("expires_on", ""));
    if (!osAccessToken.empty() && nExpiresOn > 0)
    {
        CPLAzureCachedToken cachedToken;
        cachedToken.osAccessToken = osAccessToken;
        cachedToken.nExpiresOn = nExpiresOn;
        goMapIMDSURLToCachedToken[osURLResource] = std::move(cachedToken);
        CPLDebug("AZURE", "Storing credentials for %s until " CPL_FRMT_GIB,
                 osURLResource.c_str(), nExpiresOn);
    }

    return !osAccessToken.empty();
}

/************************************************************************/
/*                 GetConfigurationFromWorkloadIdentity()               */
/************************************************************************/

// Last timestamp AZURE_FEDERATED_TOKEN_FILE was read
static GIntBig gnLastReadFederatedTokenFile = 0;
static std::string gosFederatedToken{};

// Azure Active Directory Workload Identity, typically for Azure Kubernetes
// Cf https://github.com/Azure/azure-sdk-for-python/blob/main/sdk/identity/azure-identity/azure/identity/_credentials/workload_identity.py
static bool GetConfigurationFromWorkloadIdentity(std::string &osAccessToken)
{
    const std::string AZURE_CLIENT_ID(
        CPLGetConfigOption("AZURE_CLIENT_ID", ""));
    const std::string AZURE_TENANT_ID(
        CPLGetConfigOption("AZURE_TENANT_ID", ""));
    const std::string AZURE_AUTHORITY_HOST(
        CPLGetConfigOption("AZURE_AUTHORITY_HOST", ""));
    const std::string AZURE_FEDERATED_TOKEN_FILE(
        CPLGetConfigOption("AZURE_FEDERATED_TOKEN_FILE", ""));
    if (AZURE_CLIENT_ID.empty() || AZURE_TENANT_ID.empty() ||
        AZURE_AUTHORITY_HOST.empty() || AZURE_FEDERATED_TOKEN_FILE.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> guard(gMutex);

    time_t nCurTime;
    time(&nCurTime);

    // Look for cached token corresponding to this request URL
    const std::string osURL(AZURE_AUTHORITY_HOST + AZURE_TENANT_ID +
                            "/oauth2/v2.0/token");
    auto oIter = goMapIMDSURLToCachedToken.find(osURL);
    if (oIter != goMapIMDSURLToCachedToken.end())
    {
        const auto &oCachedToken = oIter->second;
        // Try to reuse credentials if they are still valid, but
        // keep one minute of margin...
        if (nCurTime < oCachedToken.nExpiresOn - 60)
        {
            osAccessToken = oCachedToken.osAccessToken;
            return true;
        }
    }

    // Ingest content of AZURE_FEDERATED_TOKEN_FILE if last time was more than
    // 600 seconds.
    if (nCurTime - gnLastReadFederatedTokenFile > 600)
    {
        auto fp = VSIVirtualHandleUniquePtr(
            VSIFOpenL(AZURE_FEDERATED_TOKEN_FILE.c_str(), "rb"));
        if (!fp)
        {
            CPLDebug("AZURE", "Cannot open AZURE_FEDERATED_TOKEN_FILE = %s",
                     AZURE_FEDERATED_TOKEN_FILE.c_str());
            return false;
        }
        fp->Seek(0, SEEK_END);
        const auto nSize = fp->Tell();
        if (nSize == 0 || nSize > 100 * 1024)
        {
            CPLDebug(
                "AZURE",
                "Invalid size for AZURE_FEDERATED_TOKEN_FILE = " CPL_FRMT_GUIB,
                static_cast<GUIntBig>(nSize));
            return false;
        }
        fp->Seek(0, SEEK_SET);
        gosFederatedToken.resize(static_cast<size_t>(nSize));
        if (fp->Read(&gosFederatedToken[0], gosFederatedToken.size(), 1) != 1)
        {
            CPLDebug("AZURE", "Cannot read AZURE_FEDERATED_TOKEN_FILE");
            return false;
        }
        gnLastReadFederatedTokenFile = nCurTime;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare POST request.                                           */
    /* -------------------------------------------------------------------- */
    CPLStringList aosOptions;

    aosOptions.AddString(
        "HEADERS=Content-Type: application/x-www-form-urlencoded");

    std::string osItem("POSTFIELDS=client_assertion=");
    osItem += CPLAWSURLEncode(gosFederatedToken);
    osItem += "&client_assertion_type=urn:ietf:params:oauth:client-assertion-"
              "type:jwt-bearer";
    osItem += "&client_id=";
    osItem += CPLAWSURLEncode(AZURE_CLIENT_ID);
    osItem += "&grant_type=client_credentials";
    osItem += "&scope=https://storage.azure.com/.default";
    aosOptions.AddString(osItem.c_str());

    /* -------------------------------------------------------------------- */
    /*      Submit request by HTTP.                                         */
    /* -------------------------------------------------------------------- */
    CPLHTTPResult *psResult = CPLHTTPFetch(osURL.c_str(), aosOptions.List());
    if (!psResult)
        return false;

    if (!psResult->pabyData || psResult->pszErrBuf)
    {
        if (psResult->pszErrBuf)
            CPLDebug("AZURE", "%s", psResult->pszErrBuf);
        if (psResult->pabyData)
            CPLDebug("AZURE", "%s", psResult->pabyData);

        CPLDebug("AZURE",
                 "Fetching OAuth2 access code from workload identity failed.");
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    CPLStringList oResponse =
        CPLParseKeyValueJson(reinterpret_cast<char *>(psResult->pabyData));
    CPLHTTPDestroyResult(psResult);

    osAccessToken = oResponse.FetchNameValueDef("access_token", "");
    const int nExpiresIn = atoi(oResponse.FetchNameValueDef("expires_in", ""));
    if (!osAccessToken.empty() && nExpiresIn > 0)
    {
        CPLAzureCachedToken cachedToken;
        cachedToken.osAccessToken = osAccessToken;
        cachedToken.nExpiresOn = nCurTime + nExpiresIn;
        goMapIMDSURLToCachedToken[osURL] = cachedToken;
        CPLDebug("AZURE", "Storing credentials for %s until " CPL_FRMT_GIB,
                 osURL.c_str(), cachedToken.nExpiresOn);
    }

    return !osAccessToken.empty();
}

/************************************************************************/
/*                GetConfigurationFromManagedIdentities()               */
/************************************************************************/

static bool
GetConfigurationFromManagedIdentities(const std::string &osPathForOption,
                                      std::string &osAccessToken)
{
    if (GetConfigurationFromWorkloadIdentity(osAccessToken))
        return true;
    return GetConfigurationFromIMDSCredentials(osPathForOption, osAccessToken);
}

/************************************************************************/
/*                             ClearCache()                             */
/************************************************************************/

void VSIAzureBlobHandleHelper::ClearCache()
{
    std::lock_guard<std::mutex> guard(gMutex);
    goMapIMDSURLToCachedToken.clear();
    gnLastReadFederatedTokenFile = 0;
    gosFederatedToken.clear();
}

/************************************************************************/
/*                    ParseStorageConnectionString()                    */
/************************************************************************/

static bool
ParseStorageConnectionString(const std::string &osStorageConnectionString,
                             const std::string &osServicePrefix,
                             bool &bUseHTTPS, std::string &osEndpoint,
                             std::string &osStorageAccount,
                             std::string &osStorageKey, std::string &osSAS)
{
    osStorageAccount =
        AzureCSGetParameter(osStorageConnectionString, "AccountName", false);
    osStorageKey =
        AzureCSGetParameter(osStorageConnectionString, "AccountKey", false);

    const std::string osProtocol(AzureCSGetParameter(
        osStorageConnectionString, "DefaultEndpointsProtocol", false));
    bUseHTTPS = (osProtocol != "http");

    if (osStorageAccount.empty() || osStorageKey.empty())
    {
        osStorageAccount.clear();
        osStorageKey.clear();

        const std::string osBlobEndpoint =
            RemoveTrailingSlash(AzureCSGetParameter(osStorageConnectionString,
                                                    "BlobEndpoint", false));
        osSAS = AzureCSGetParameter(osStorageConnectionString,
                                    "SharedAccessSignature", false);
        if (!osBlobEndpoint.empty() && !osSAS.empty())
        {
            osEndpoint = osBlobEndpoint;
            return true;
        }

        return false;
    }

    const std::string osBlobEndpoint =
        AzureCSGetParameter(osStorageConnectionString, "BlobEndpoint", false);
    if (!osBlobEndpoint.empty())
    {
        osEndpoint = RemoveTrailingSlash(osBlobEndpoint);
    }
    else
    {
        const std::string osEndpointSuffix(AzureCSGetParameter(
            osStorageConnectionString, "EndpointSuffix", false));
        if (!osEndpointSuffix.empty())
            osEndpoint = (bUseHTTPS ? "https://" : "http://") +
                         osStorageAccount + "." + osServicePrefix + "." +
                         RemoveTrailingSlash(osEndpointSuffix);
    }

    return true;
}

/************************************************************************/
/*                 GetConfigurationFromCLIConfigFile()                  */
/************************************************************************/

static bool GetConfigurationFromCLIConfigFile(
    const std::string &osPathForOption, const std::string &osServicePrefix,
    bool &bUseHTTPS, std::string &osEndpoint, std::string &osStorageAccount,
    std::string &osStorageKey, std::string &osSAS, std::string &osAccessToken,
    bool &bFromManagedIdentities)
{
#ifdef _WIN32
    const char *pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
    constexpr char SEP_STRING[] = "\\";
#else
    const char *pszHome = CPLGetConfigOption("HOME", nullptr);
    constexpr char SEP_STRING[] = "/";
#endif

    std::string osDotAzure(pszHome ? pszHome : "");
    osDotAzure += SEP_STRING;
    osDotAzure += ".azure";

    const char *pszAzureConfigDir =
        CPLGetConfigOption("AZURE_CONFIG_DIR", osDotAzure.c_str());
    if (pszAzureConfigDir[0] == '\0')
        return false;

    std::string osConfigFilename = pszAzureConfigDir;
    osConfigFilename += SEP_STRING;
    osConfigFilename += "config";

    VSILFILE *fp = VSIFOpenL(osConfigFilename.c_str(), "rb");
    std::string osStorageConnectionString;
    if (fp == nullptr)
        return false;

    bool bInStorageSection = false;
    while (const char *pszLine = CPLReadLineL(fp))
    {
        if (pszLine[0] == '#' || pszLine[0] == ';')
        {
            // comment line
        }
        else if (strcmp(pszLine, "[storage]") == 0)
        {
            bInStorageSection = true;
        }
        else if (pszLine[0] == '[')
        {
            bInStorageSection = false;
        }
        else if (bInStorageSection)
        {
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(pszLine, &pszKey);
            if (pszKey && pszValue)
            {
                if (EQUAL(pszKey, "account"))
                {
                    osStorageAccount = pszValue;
                }
                else if (EQUAL(pszKey, "connection_string"))
                {
                    osStorageConnectionString = pszValue;
                }
                else if (EQUAL(pszKey, "key"))
                {
                    osStorageKey = pszValue;
                }
                else if (EQUAL(pszKey, "sas_token"))
                {
                    osSAS = pszValue;
                    // Az CLI apparently uses configparser with
                    // BasicInterpolation where the % character has a special
                    // meaning See
                    // https://docs.python.org/3/library/configparser.html#configparser.BasicInterpolation
                    // A token might end with %%3D which must be transformed to
                    // %3D
                    osSAS = CPLString(osSAS).replaceAll("%%", '%');
                }
            }
            CPLFree(pszKey);
        }
    }
    VSIFCloseL(fp);

    if (!osStorageConnectionString.empty())
    {
        return ParseStorageConnectionString(
            osStorageConnectionString, osServicePrefix, bUseHTTPS, osEndpoint,
            osStorageAccount, osStorageKey, osSAS);
    }

    if (osStorageAccount.empty())
    {
        CPLDebug("AZURE", "Missing storage.account in %s",
                 osConfigFilename.c_str());
        return false;
    }

    if (osEndpoint.empty())
        osEndpoint = (bUseHTTPS ? "https://" : "http://") + osStorageAccount +
                     "." + osServicePrefix + ".core.windows.net";

    osAccessToken = CPLGetConfigOption("AZURE_STORAGE_ACCESS_TOKEN", "");
    if (!osAccessToken.empty())
        return true;

    if (osStorageKey.empty() && osSAS.empty())
    {
        if (CPLTestBool(CPLGetConfigOption("AZURE_NO_SIGN_REQUEST", "NO")))
        {
            return true;
        }

        std::string osTmpAccessToken;
        if (GetConfigurationFromManagedIdentities(osPathForOption,
                                                  osTmpAccessToken))
        {
            bFromManagedIdentities = true;
            return true;
        }

        CPLDebug("AZURE", "Missing storage.key or storage.sas_token in %s",
                 osConfigFilename.c_str());
        return false;
    }

    return true;
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIAzureBlobHandleHelper::GetConfiguration(
    const std::string &osPathForOption, CSLConstList papszOptions,
    Service eService, bool &bUseHTTPS, std::string &osEndpoint,
    std::string &osStorageAccount, std::string &osStorageKey,
    std::string &osSAS, std::string &osAccessToken,
    bool &bFromManagedIdentities)
{
    bFromManagedIdentities = false;

    const std::string osServicePrefix(
        eService == Service::SERVICE_BLOB ? "blob" : "dfs");
    bUseHTTPS = CPLTestBool(VSIGetPathSpecificOption(
        osPathForOption.c_str(), "CPL_AZURE_USE_HTTPS", "YES"));
    osEndpoint = RemoveTrailingSlash(VSIGetPathSpecificOption(
        osPathForOption.c_str(), "CPL_AZURE_ENDPOINT", ""));

    const std::string osStorageConnectionString(CSLFetchNameValueDef(
        papszOptions, "AZURE_STORAGE_CONNECTION_STRING",
        VSIGetPathSpecificOption(osPathForOption.c_str(),
                                 "AZURE_STORAGE_CONNECTION_STRING", "")));
    if (!osStorageConnectionString.empty())
    {
        return ParseStorageConnectionString(
            osStorageConnectionString, osServicePrefix, bUseHTTPS, osEndpoint,
            osStorageAccount, osStorageKey, osSAS);
    }
    else
    {
        osStorageAccount = CSLFetchNameValueDef(
            papszOptions, "AZURE_STORAGE_ACCOUNT",
            VSIGetPathSpecificOption(osPathForOption.c_str(),
                                     "AZURE_STORAGE_ACCOUNT", ""));
        if (!osStorageAccount.empty())
        {
            if (osEndpoint.empty())
                osEndpoint = (bUseHTTPS ? "https://" : "http://") +
                             osStorageAccount + "." + osServicePrefix +
                             ".core.windows.net";

            osAccessToken = CSLFetchNameValueDef(
                papszOptions, "AZURE_STORAGE_ACCESS_TOKEN",
                VSIGetPathSpecificOption(osPathForOption.c_str(),
                                         "AZURE_STORAGE_ACCESS_TOKEN", ""));
            if (!osAccessToken.empty())
                return true;

            osStorageKey = CSLFetchNameValueDef(
                papszOptions, "AZURE_STORAGE_ACCESS_KEY",
                VSIGetPathSpecificOption(osPathForOption.c_str(),
                                         "AZURE_STORAGE_ACCESS_KEY", ""));
            if (osStorageKey.empty())
            {
                osSAS = VSIGetPathSpecificOption(
                    osPathForOption.c_str(), "AZURE_STORAGE_SAS_TOKEN",
                    CPLGetConfigOption("AZURE_SAS",
                                       ""));  // AZURE_SAS for GDAL < 3.5
                if (osSAS.empty())
                {
                    if (CPLTestBool(VSIGetPathSpecificOption(
                            osPathForOption.c_str(), "AZURE_NO_SIGN_REQUEST",
                            "NO")))
                    {
                        return true;
                    }

                    std::string osTmpAccessToken;
                    if (GetConfigurationFromManagedIdentities(osPathForOption,
                                                              osTmpAccessToken))
                    {
                        bFromManagedIdentities = true;
                        return true;
                    }

                    const char *pszMsg =
                        "AZURE_STORAGE_ACCESS_KEY or AZURE_STORAGE_SAS_TOKEN "
                        "or AZURE_NO_SIGN_REQUEST configuration option "
                        "not defined";
                    CPLDebug("AZURE", "%s", pszMsg);
                    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
                    return false;
                }
            }
            return true;
        }
    }

    if (GetConfigurationFromCLIConfigFile(
            osPathForOption, osServicePrefix, bUseHTTPS, osEndpoint,
            osStorageAccount, osStorageKey, osSAS, osAccessToken,
            bFromManagedIdentities))
    {
        return true;
    }

    const char *pszMsg =
        "Missing AZURE_STORAGE_ACCOUNT+"
        "(AZURE_STORAGE_ACCESS_KEY or AZURE_STORAGE_SAS_TOKEN or "
        "AZURE_NO_SIGN_REQUEST) or "
        "AZURE_STORAGE_CONNECTION_STRING "
        "configuration options or Azure CLI configuration file";
    CPLDebug("AZURE", "%s", pszMsg);
    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
    return false;
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIAzureBlobHandleHelper *VSIAzureBlobHandleHelper::BuildFromURI(
    const char *pszURI, const char *pszFSPrefix,
    const char *pszURIForPathSpecificOption, CSLConstList papszOptions)
{
    if (strcmp(pszFSPrefix, "/vsiaz/") != 0 &&
        strcmp(pszFSPrefix, "/vsiaz_streaming/") != 0 &&
        strcmp(pszFSPrefix, "/vsiadls/") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported FS prefix");
        return nullptr;
    }

    const auto eService = strcmp(pszFSPrefix, "/vsiaz/") == 0 ||
                                  strcmp(pszFSPrefix, "/vsiaz_streaming/") == 0
                              ? Service::SERVICE_BLOB
                              : Service::SERVICE_ADLS;

    std::string osPathForOption(
        eService == Service::SERVICE_BLOB ? "/vsiaz/" : "/vsiadls/");
    osPathForOption +=
        pszURIForPathSpecificOption ? pszURIForPathSpecificOption : pszURI;

    bool bUseHTTPS = true;
    std::string osStorageAccount;
    std::string osStorageKey;
    std::string osEndpoint;
    std::string osSAS;
    std::string osAccessToken;
    bool bFromManagedIdentities = false;

    if (!GetConfiguration(osPathForOption, papszOptions, eService, bUseHTTPS,
                          osEndpoint, osStorageAccount, osStorageKey, osSAS,
                          osAccessToken, bFromManagedIdentities))
    {
        return nullptr;
    }

    if (CPLTestBool(VSIGetPathSpecificOption(osPathForOption.c_str(),
                                             "AZURE_NO_SIGN_REQUEST", "NO")))
    {
        osStorageKey.clear();
        osSAS.clear();
        osAccessToken.clear();
    }

    // pszURI == bucket/object
    const std::string osBucketObject(pszURI);
    std::string osBucket(osBucketObject);
    std::string osObjectKey;
    size_t nSlashPos = osBucketObject.find('/');
    if (nSlashPos != std::string::npos)
    {
        osBucket = osBucketObject.substr(0, nSlashPos);
        osObjectKey = osBucketObject.substr(nSlashPos + 1);
    }

    return new VSIAzureBlobHandleHelper(
        osPathForOption, osEndpoint, osBucket, osObjectKey, osStorageAccount,
        osStorageKey, osSAS, osAccessToken, bFromManagedIdentities);
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

std::string VSIAzureBlobHandleHelper::BuildURL(const std::string &osEndpoint,
                                               const std::string &osBucket,
                                               const std::string &osObjectKey,
                                               const std::string &osSAS)
{
    std::string osURL = osEndpoint;
    osURL += "/";
    osURL += CPLAWSURLEncode(osBucket, false);
    if (!osObjectKey.empty())
        osURL += "/" + CPLAWSURLEncode(osObjectKey, false);
    if (!osSAS.empty())
        osURL += '?' + osSAS;
    return osURL;
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIAzureBlobHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint, m_osBucket, m_osObjectKey, std::string());
    m_osURL += GetQueryString(false);
    if (!m_osSAS.empty())
        m_osURL += (m_oMapQueryParameters.empty() ? '?' : '&') + m_osSAS;
}

/************************************************************************/
/*                        GetSASQueryString()                           */
/************************************************************************/

std::string VSIAzureBlobHandleHelper::GetSASQueryString() const
{
    if (!m_osSAS.empty())
        return '?' + m_osSAS;
    return std::string();
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *VSIAzureBlobHandleHelper::GetCurlHeaders(
    const std::string &osVerb, const struct curl_slist *psExistingHeaders,
    const void *, size_t) const
{
    if (m_bFromManagedIdentities || !m_osAccessToken.empty())
    {
        std::string osAccessToken;
        if (m_bFromManagedIdentities)
        {
            if (!GetConfigurationFromManagedIdentities(m_osPathForOption,
                                                       osAccessToken))
                return nullptr;
        }
        else
        {
            osAccessToken = m_osAccessToken;
        }

        struct curl_slist *headers = nullptr;

        // Do not use CPLSPrintf() as we could get over the 8K character limit
        // with very large SAS tokens
        std::string osAuthorization = "Authorization: Bearer ";
        osAuthorization += osAccessToken;
        headers = curl_slist_append(headers, osAuthorization.c_str());
        headers = curl_slist_append(headers, "x-ms-version: 2019-12-12");
        return headers;
    }

    std::string osResource;
    const auto nSlashSlashPos = m_osEndpoint.find("//");
    if (nSlashSlashPos != std::string::npos)
    {
        const auto nResourcePos = m_osEndpoint.find('/', nSlashSlashPos + 2);
        if (nResourcePos != std::string::npos)
            osResource = m_osEndpoint.substr(nResourcePos);
    }
    osResource += "/" + m_osBucket;
    if (!m_osObjectKey.empty())
        osResource += "/" + CPLAWSURLEncode(m_osObjectKey, false);

    return GetAzureBlobHeaders(osVerb, psExistingHeaders, osResource,
                               m_oMapQueryParameters, m_osStorageAccount,
                               m_osStorageKey, m_bIncludeMSVersion);
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

std::string VSIAzureBlobHandleHelper::GetSignedURL(CSLConstList papszOptions)
{
    if (m_osStorageKey.empty())
        return m_osURL;

    std::string osStartDate(CPLGetAWS_SIGN4_Timestamp(time(nullptr)));
    const char *pszStartDate = CSLFetchNameValue(papszOptions, "START_DATE");
    if (pszStartDate)
        osStartDate = pszStartDate;
    int nYear, nMonth, nDay, nHour = 0, nMin = 0, nSec = 0;
    if (sscanf(osStartDate.c_str(), "%04d%02d%02dT%02d%02d%02dZ", &nYear,
               &nMonth, &nDay, &nHour, &nMin, &nSec) < 3)
    {
        return std::string();
    }
    osStartDate = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", nYear, nMonth,
                             nDay, nHour, nMin, nSec);

    struct tm brokendowntime;
    brokendowntime.tm_year = nYear - 1900;
    brokendowntime.tm_mon = nMonth - 1;
    brokendowntime.tm_mday = nDay;
    brokendowntime.tm_hour = nHour;
    brokendowntime.tm_min = nMin;
    brokendowntime.tm_sec = nSec;
    GIntBig nStartDate = CPLYMDHMSToUnixTime(&brokendowntime);
    GIntBig nEndDate =
        nStartDate +
        atoi(CSLFetchNameValueDef(papszOptions, "EXPIRATION_DELAY", "3600"));
    CPLUnixTimeToYMDHMS(nEndDate, &brokendowntime);
    nYear = brokendowntime.tm_year + 1900;
    nMonth = brokendowntime.tm_mon + 1;
    nDay = brokendowntime.tm_mday;
    nHour = brokendowntime.tm_hour;
    nMin = brokendowntime.tm_min;
    nSec = brokendowntime.tm_sec;
    std::string osEndDate = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", nYear,
                                       nMonth, nDay, nHour, nMin, nSec);

    std::string osVerb(CSLFetchNameValueDef(papszOptions, "VERB", "GET"));
    std::string osSignedPermissions(CSLFetchNameValueDef(
        papszOptions, "SIGNEDPERMISSIONS",
        (EQUAL(osVerb.c_str(), "GET") || EQUAL(osVerb.c_str(), "HEAD")) ? "r"
                                                                        : "w"));

    std::string osSignedIdentifier(
        CSLFetchNameValueDef(papszOptions, "SIGNEDIDENTIFIER", ""));

    const std::string osSignedVersion("2020-12-06");
    const std::string osSignedProtocol("https");
    const std::string osSignedResource("b");  // blob

    std::string osCanonicalizedResource("/blob/");
    osCanonicalizedResource += CPLAWSURLEncode(m_osStorageAccount, false);
    osCanonicalizedResource += '/';
    osCanonicalizedResource += CPLAWSURLEncode(m_osBucket, false);
    osCanonicalizedResource += '/';
    osCanonicalizedResource += CPLAWSURLEncode(m_osObjectKey, false);

    // Cf https://learn.microsoft.com/en-us/rest/api/storageservices/create-service-sas
    std::string osStringToSign;
    osStringToSign += osSignedPermissions + "\n";
    osStringToSign += osStartDate + "\n";
    osStringToSign += osEndDate + "\n";
    osStringToSign += osCanonicalizedResource + "\n";
    osStringToSign += osSignedIdentifier + "\n";
    osStringToSign += "\n";  // signedIP
    osStringToSign += osSignedProtocol + "\n";
    osStringToSign += osSignedVersion + "\n";
    osStringToSign += osSignedResource + "\n";
    osStringToSign += "\n";  // signedSnapshotTime
    osStringToSign += "\n";  // signedEncryptionScope
    osStringToSign += "\n";  // rscc
    osStringToSign += "\n";  // rscd
    osStringToSign += "\n";  // rsce
    osStringToSign += "\n";  // rscl

#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "osStringToSign = %s", osStringToSign.c_str());
#endif

    /* -------------------------------------------------------------------- */
    /*      Compute signature.                                              */
    /* -------------------------------------------------------------------- */
    std::string osSignature(
        CPLAzureGetSignature(osStringToSign, m_osStorageKey));

    ResetQueryParameters();
    AddQueryParameter("sv", osSignedVersion);
    AddQueryParameter("st", osStartDate);
    AddQueryParameter("se", osEndDate);
    AddQueryParameter("sr", osSignedResource);
    AddQueryParameter("sp", osSignedPermissions);
    AddQueryParameter("spr", osSignedProtocol);
    AddQueryParameter("sig", osSignature);
    if (!osSignedIdentifier.empty())
        AddQueryParameter("si", osSignedIdentifier);
    return m_osURL;
}

#endif  // HAVE_CURL

//! @endcond
