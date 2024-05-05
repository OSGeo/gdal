/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  Google Cloud Storage routines
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_google_cloud.h"
#include "cpl_vsi_error.h"
#include "cpl_sha1.h"
#include "cpl_sha256.h"
#include "cpl_time.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_aws.h"
#include "cpl_json.h"

#ifdef HAVE_CURL

static CPLMutex *hMutex = nullptr;
static bool bFirstTimeForDebugMessage = true;
static GOA2Manager oStaticManager;

/************************************************************************/
/*                    CPLIsMachineForSureGCEInstance()                  */
/************************************************************************/

/** Returns whether the current machine is surely a Google Compute Engine
 * instance.
 *
 * This does a very quick check without network access.
 * Note: only works for Linux GCE instances.
 *
 * @return true if the current machine is surely a GCE instance.
 * @since GDAL 2.3
 */
bool CPLIsMachineForSureGCEInstance()
{
    if (CPLTestBool(CPLGetConfigOption("CPL_MACHINE_IS_GCE", "NO")))
    {
        return true;
    }
#ifdef __linux
    // If /sys/class/dmi/id/product_name exists, it contains "Google Compute
    // Engine"
    bool bIsGCEInstance = false;
    if (CPLTestBool(CPLGetConfigOption("CPL_GCE_CHECK_LOCAL_FILES", "YES")))
    {
        static bool bIsGCEInstanceStatic = false;
        static bool bDone = false;
        {
            CPLMutexHolder oHolder(&hMutex);
            if (!bDone)
            {
                bDone = true;

                VSILFILE *fp =
                    VSIFOpenL("/sys/class/dmi/id/product_name", "rb");
                if (fp)
                {
                    const char *pszLine = CPLReadLineL(fp);
                    bIsGCEInstanceStatic =
                        pszLine &&
                        STARTS_WITH_CI(pszLine, "Google Compute Engine");
                    VSIFCloseL(fp);
                }
            }
        }
        bIsGCEInstance = bIsGCEInstanceStatic;
    }
    return bIsGCEInstance;
#else
    return false;
#endif
}

/************************************************************************/
/*                 CPLIsMachinePotentiallyGCEInstance()                 */
/************************************************************************/

/** Returns whether the current machine is potentially a Google Compute Engine
 * instance.
 *
 * This does a very quick check without network access. To confirm if the
 * machine is effectively a GCE instance, metadata.google.internal must be
 * queried.
 *
 * @return true if the current machine is potentially a GCE instance.
 * @since GDAL 2.3
 */
bool CPLIsMachinePotentiallyGCEInstance()
{
#ifdef __linux
    bool bIsMachinePotentialGCEInstance = true;
    if (CPLTestBool(CPLGetConfigOption("CPL_GCE_CHECK_LOCAL_FILES", "YES")))
    {
        bIsMachinePotentialGCEInstance = CPLIsMachineForSureGCEInstance();
    }
    return bIsMachinePotentialGCEInstance;
#elif defined(_WIN32)
    // We might add later a way of detecting if we run on GCE using WMI
    // See https://cloud.google.com/compute/docs/instances/managing-instances
    // For now, unconditionally try
    return true;
#else
    // At time of writing GCE instances can be only Linux or Windows
    return false;
#endif
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                            GetGSHeaders()                            */
/************************************************************************/

static struct curl_slist *
GetGSHeaders(const std::string &osPathForOption, const std::string &osVerb,
             const struct curl_slist *psExistingHeaders,
             const std::string &osCanonicalResource,
             const std::string &osSecretAccessKey,
             const std::string &osAccessKeyId, const std::string &osUserProject)
{
    if (osSecretAccessKey.empty())
    {
        // GS_NO_SIGN_REQUEST=YES case
        return nullptr;
    }

    std::string osDate = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                  "CPL_GS_TIMESTAMP", "");
    if (osDate.empty())
    {
        osDate = IVSIS3LikeHandleHelper::GetRFC822DateTime();
    }

    std::map<std::string, std::string> oSortedMapHeaders;
    if (!osUserProject.empty())
        oSortedMapHeaders["x-goog-user-project"] = osUserProject;
    std::string osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
            oSortedMapHeaders, psExistingHeaders, "x-goog-"));

    // See https://cloud.google.com/storage/docs/migrating
    std::string osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign +=
        CPLAWSGetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += osDate + "\n";
    osStringToSign += osCanonicalizedHeaders;
    osStringToSign += osCanonicalResource;
#ifdef DEBUG_VERBOSE
    CPLDebug("GS", "osStringToSign = %s", osStringToSign.c_str());
#endif

    GByte abySignature[CPL_SHA1_HASH_SIZE] = {};
    CPL_HMAC_SHA1(osSecretAccessKey.c_str(), osSecretAccessKey.size(),
                  osStringToSign.c_str(), osStringToSign.size(), abySignature);

    char *pszBase64 = CPLBase64Encode(sizeof(abySignature), abySignature);
    std::string osAuthorization("GOOG1 ");
    osAuthorization += osAccessKeyId;
    osAuthorization += ":";
    osAuthorization += pszBase64;
    CPLFree(pszBase64);

    struct curl_slist *headers = nullptr;
    headers =
        curl_slist_append(headers, CPLSPrintf("Date: %s", osDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    if (!osUserProject.empty())
    {
        headers =
            curl_slist_append(headers, CPLSPrintf("x-goog-user-project: %s",
                                                  osUserProject.c_str()));
    }
    return headers;
}

/************************************************************************/
/*                         VSIGSHandleHelper()                          */
/************************************************************************/
VSIGSHandleHelper::VSIGSHandleHelper(const std::string &osEndpoint,
                                     const std::string &osBucketObjectKey,
                                     const std::string &osSecretAccessKey,
                                     const std::string &osAccessKeyId,
                                     bool bUseAuthenticationHeader,
                                     const GOA2Manager &oManager,
                                     const std::string &osUserProject)
    : m_osURL(osEndpoint + CPLAWSURLEncode(osBucketObjectKey, false)),
      m_osEndpoint(osEndpoint), m_osBucketObjectKey(osBucketObjectKey),
      m_osSecretAccessKey(osSecretAccessKey), m_osAccessKeyId(osAccessKeyId),
      m_bUseAuthenticationHeader(bUseAuthenticationHeader),
      m_oManager(oManager), m_osUserProject(osUserProject)
{
    if (m_osBucketObjectKey.find('/') == std::string::npos)
        m_osURL += "/";
}

/************************************************************************/
/*                        ~VSIGSHandleHelper()                          */
/************************************************************************/

VSIGSHandleHelper::~VSIGSHandleHelper()
{
}

/************************************************************************/
/*                GetConfigurationFromAWSConfigFiles()                  */
/************************************************************************/

bool VSIGSHandleHelper::GetConfigurationFromConfigFile(
    std::string &osSecretAccessKey, std::string &osAccessKeyId,
    std::string &osOAuth2RefreshToken, std::string &osOAuth2ClientId,
    std::string &osOAuth2ClientSecret, std::string &osCredentials)
{
#ifdef _WIN32
    const char *pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
    constexpr char SEP_STRING[] = "\\";
#else
    const char *pszHome = CPLGetConfigOption("HOME", nullptr);
    constexpr char SEP_STRING[] = "/";
#endif

    // GDAL specific config option (mostly for testing purpose, but also
    // used in production in some cases)
    const char *pszCredentials =
        CPLGetConfigOption("CPL_GS_CREDENTIALS_FILE", nullptr);
    if (pszCredentials)
    {
        osCredentials = pszCredentials;
    }
    else
    {
        osCredentials = pszHome ? pszHome : "";
        osCredentials += SEP_STRING;
        osCredentials += ".boto";
    }

    VSILFILE *fp = VSIFOpenL(osCredentials.c_str(), "rb");
    if (fp != nullptr)
    {
        const char *pszLine;
        bool bInCredentials = false;
        bool bInOAuth2 = false;
        while ((pszLine = CPLReadLineL(fp)) != nullptr)
        {
            if (pszLine[0] == '[')
            {
                bInCredentials = false;
                bInOAuth2 = false;

                if (std::string(pszLine) == "[Credentials]")
                    bInCredentials = true;
                else if (std::string(pszLine) == "[OAuth2]")
                    bInOAuth2 = true;
            }
            else if (bInCredentials)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(pszLine, &pszKey);
                if (pszKey && pszValue)
                {
                    if (EQUAL(pszKey, "gs_access_key_id"))
                        osAccessKeyId = CPLString(pszValue).Trim();
                    else if (EQUAL(pszKey, "gs_secret_access_key"))
                        osSecretAccessKey = CPLString(pszValue).Trim();
                    else if (EQUAL(pszKey, "gs_oauth2_refresh_token"))
                        osOAuth2RefreshToken = CPLString(pszValue).Trim();
                }
                CPLFree(pszKey);
            }
            else if (bInOAuth2)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(pszLine, &pszKey);
                if (pszKey && pszValue)
                {
                    if (EQUAL(pszKey, "client_id"))
                        osOAuth2ClientId = CPLString(pszValue).Trim();
                    else if (EQUAL(pszKey, "client_secret"))
                        osOAuth2ClientSecret = CPLString(pszValue).Trim();
                }
                CPLFree(pszKey);
            }
        }
        VSIFCloseL(fp);
    }

    return (!osAccessKeyId.empty() && !osSecretAccessKey.empty()) ||
           !osOAuth2RefreshToken.empty();
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIGSHandleHelper::GetConfiguration(const std::string &osPathForOption,
                                         CSLConstList papszOptions,
                                         std::string &osSecretAccessKey,
                                         std::string &osAccessKeyId,
                                         bool &bUseAuthenticationHeader,
                                         GOA2Manager &oManager)
{
    osSecretAccessKey.clear();
    osAccessKeyId.clear();
    bUseAuthenticationHeader = false;

    if (CPLTestBool(VSIGetPathSpecificOption(osPathForOption.c_str(),
                                             "GS_NO_SIGN_REQUEST", "NO")))
    {
        return true;
    }

    osSecretAccessKey = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                 "GS_SECRET_ACCESS_KEY", "");
    if (!osSecretAccessKey.empty())
    {
        osAccessKeyId = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                 "GS_ACCESS_KEY_ID", "");
        if (osAccessKeyId.empty())
        {
            VSIError(VSIE_AWSInvalidCredentials,
                     "GS_ACCESS_KEY_ID configuration option not defined");
            bFirstTimeForDebugMessage = false;
            return false;
        }

        if (bFirstTimeForDebugMessage)
        {
            CPLDebug("GS", "Using GS_SECRET_ACCESS_KEY and "
                           "GS_ACCESS_KEY_ID configuration options");
        }
        bFirstTimeForDebugMessage = false;
        return true;
    }

    const std::string osHeaderFile = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "GDAL_HTTP_HEADER_FILE", "");
    bool bMayWarnDidNotFindAuth = false;
    if (!osHeaderFile.empty())
    {
        bool bFoundAuth = false;
        VSILFILE *fp = nullptr;
        // Do not allow /vsicurl/ access from /vsicurl because of
        // GetCurlHandleFor() e.g. "/vsicurl/,HEADER_FILE=/vsicurl/,url= " would
        // cause use of memory after free
        if (strstr(osHeaderFile.c_str(), "/vsicurl/") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsicurl?") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsis3/") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsigs/") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsiaz/") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsioss/") == nullptr &&
            strstr(osHeaderFile.c_str(), "/vsiswift/") == nullptr)
        {
            fp = VSIFOpenL(osHeaderFile.c_str(), "rb");
        }
        if (fp == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot read %s",
                     osHeaderFile.c_str());
        }
        else
        {
            const char *pszLine = nullptr;
            while ((pszLine = CPLReadLineL(fp)) != nullptr)
            {
                if (STARTS_WITH_CI(pszLine, "Authorization:"))
                {
                    bFoundAuth = true;
                    break;
                }
            }
            VSIFCloseL(fp);
            if (!bFoundAuth)
                bMayWarnDidNotFindAuth = true;
        }
        if (bFoundAuth)
        {
            if (bFirstTimeForDebugMessage)
            {
                CPLDebug("GS", "Using GDAL_HTTP_HEADER_FILE=%s",
                         osHeaderFile.c_str());
            }
            bFirstTimeForDebugMessage = false;
            bUseAuthenticationHeader = true;
            return true;
        }
    }

    const char *pszHeaders = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "GDAL_HTTP_HEADERS", nullptr);
    if (pszHeaders && strstr(pszHeaders, "Authorization:") != nullptr)
    {
        bUseAuthenticationHeader = true;
        return true;
    }

    std::string osRefreshToken(VSIGetPathSpecificOption(
        osPathForOption.c_str(), "GS_OAUTH2_REFRESH_TOKEN", ""));
    if (!osRefreshToken.empty())
    {
        if (oStaticManager.GetAuthMethod() ==
            GOA2Manager::ACCESS_TOKEN_FROM_REFRESH)
        {
            CPLMutexHolder oHolder(&hMutex);
            oManager = oStaticManager;
            return true;
        }

        std::string osClientId = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "GS_OAUTH2_CLIENT_ID", "");
        std::string osClientSecret = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "GS_OAUTH2_CLIENT_SECRET", "");

        int nCount =
            (!osClientId.empty() ? 1 : 0) + (!osClientSecret.empty() ? 1 : 0);
        if (nCount == 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Either both or none of GS_OAUTH2_CLIENT_ID and "
                     "GS_OAUTH2_CLIENT_SECRET must be set");
            return false;
        }

        if (bFirstTimeForDebugMessage)
        {
            std::string osMsg(
                "Using GS_OAUTH2_REFRESH_TOKEN configuration option");
            if (osClientId.empty())
                osMsg += " and GDAL default client_id/client_secret";
            else
                osMsg += " and GS_OAUTH2_CLIENT_ID and GS_OAUTH2_CLIENT_SECRET";
            CPLDebug("GS", "%s", osMsg.c_str());
        }
        bFirstTimeForDebugMessage = false;

        return oManager.SetAuthFromRefreshToken(
            osRefreshToken.c_str(), osClientId.c_str(), osClientSecret.c_str(),
            nullptr);
    }

    std::string osJsonFile(CSLFetchNameValueDef(
        papszOptions, "GOOGLE_APPLICATION_CREDENTIALS",
        VSIGetPathSpecificOption(osPathForOption.c_str(),
                                 "GOOGLE_APPLICATION_CREDENTIALS", "")));
    if (!osJsonFile.empty())
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osJsonFile))
        {
            return false;
        }

        // JSON file can be of type 'service_account' or 'authorized_user'
        std::string osJsonFileType = oDoc.GetRoot().GetString("type");

        if (strcmp(osJsonFileType.c_str(), "service_account") == 0)
        {
            CPLString osPrivateKey = oDoc.GetRoot().GetString("private_key");
            osPrivateKey.replaceAll("\\n", "\n")
                .replaceAll("\n\n", "\n")
                .replaceAll("\r", "");
            std::string osClientEmail =
                oDoc.GetRoot().GetString("client_email");
            const char *pszScope = CSLFetchNameValueDef(
                papszOptions, "GS_OAUTH2_SCOPE",
                VSIGetPathSpecificOption(
                    osPathForOption.c_str(), "GS_OAUTH2_SCOPE",
                    "https://www.googleapis.com/auth/devstorage.read_write"));

            return oManager.SetAuthFromServiceAccount(
                osPrivateKey.c_str(), osClientEmail.c_str(), pszScope, nullptr,
                nullptr);
        }
        else if (strcmp(osJsonFileType.c_str(), "authorized_user") == 0)
        {
            std::string osClientId = oDoc.GetRoot().GetString("client_id");
            std::string osClientSecret =
                oDoc.GetRoot().GetString("client_secret");
            osRefreshToken = oDoc.GetRoot().GetString("refresh_token");

            return oManager.SetAuthFromRefreshToken(
                osRefreshToken.c_str(), osClientId.c_str(),
                osClientSecret.c_str(), nullptr);
        }
        return false;
    }

    CPLString osPrivateKey = CSLFetchNameValueDef(
        papszOptions, "GS_OAUTH2_PRIVATE_KEY",
        VSIGetPathSpecificOption(osPathForOption.c_str(),
                                 "GS_OAUTH2_PRIVATE_KEY", ""));
    std::string osPrivateKeyFile = CSLFetchNameValueDef(
        papszOptions, "GS_OAUTH2_PRIVATE_KEY_FILE",
        VSIGetPathSpecificOption(osPathForOption.c_str(),
                                 "GS_OAUTH2_PRIVATE_KEY_FILE", ""));
    if (!osPrivateKey.empty() || !osPrivateKeyFile.empty())
    {
        if (!osPrivateKeyFile.empty())
        {
            VSILFILE *fp = VSIFOpenL(osPrivateKeyFile.c_str(), "rb");
            if (fp == nullptr)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         osPrivateKeyFile.c_str());
                bFirstTimeForDebugMessage = false;
                return false;
            }
            else
            {
                char *pabyBuffer = static_cast<char *>(CPLMalloc(32768));
                size_t nRead = VSIFReadL(pabyBuffer, 1, 32768, fp);
                osPrivateKey.assign(pabyBuffer, nRead);
                VSIFCloseL(fp);
                CPLFree(pabyBuffer);
            }
        }
        osPrivateKey.replaceAll("\\n", "\n")
            .replaceAll("\n\n", "\n")
            .replaceAll("\r", "");

        std::string osClientEmail = CSLFetchNameValueDef(
            papszOptions, "GS_OAUTH2_CLIENT_EMAIL",
            VSIGetPathSpecificOption(osPathForOption.c_str(),
                                     "GS_OAUTH2_CLIENT_EMAIL", ""));
        if (osClientEmail.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GS_OAUTH2_CLIENT_EMAIL not defined");
            bFirstTimeForDebugMessage = false;
            return false;
        }
        const char *pszScope = CSLFetchNameValueDef(
            papszOptions, "GS_OAUTH2_SCOPE",
            VSIGetPathSpecificOption(
                osPathForOption.c_str(), "GS_OAUTH2_SCOPE",
                "https://www.googleapis.com/auth/devstorage.read_write"));

        if (bFirstTimeForDebugMessage)
        {
            CPLDebug("GS",
                     "Using %s, GS_OAUTH2_CLIENT_EMAIL and GS_OAUTH2_SCOPE=%s "
                     "configuration options",
                     !osPrivateKeyFile.empty() ? "GS_OAUTH2_PRIVATE_KEY_FILE"
                                               : "GS_OAUTH2_PRIVATE_KEY",
                     pszScope);
        }
        bFirstTimeForDebugMessage = false;

        return oManager.SetAuthFromServiceAccount(osPrivateKey.c_str(),
                                                  osClientEmail.c_str(),
                                                  pszScope, nullptr, nullptr);
    }

    // Next try reading from ~/.boto
    std::string osCredentials;
    std::string osOAuth2RefreshToken;
    std::string osOAuth2ClientId;
    std::string osOAuth2ClientSecret;
    if (GetConfigurationFromConfigFile(osSecretAccessKey, osAccessKeyId,
                                       osOAuth2RefreshToken, osOAuth2ClientId,
                                       osOAuth2ClientSecret, osCredentials))
    {
        if (!osOAuth2RefreshToken.empty())
        {
            if (oStaticManager.GetAuthMethod() ==
                GOA2Manager::ACCESS_TOKEN_FROM_REFRESH)
            {
                CPLMutexHolder oHolder(&hMutex);
                oManager = oStaticManager;
                return true;
            }

            std::string osClientId =
                CPLGetConfigOption("GS_OAUTH2_CLIENT_ID", "");
            std::string osClientSecret =
                CPLGetConfigOption("GS_OAUTH2_CLIENT_SECRET", "");
            bool bClientInfoFromEnv = false;
            bool bClientInfoFromFile = false;

            int nCount = (!osClientId.empty() ? 1 : 0) +
                         (!osClientSecret.empty() ? 1 : 0);
            if (nCount == 1)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Either both or none of GS_OAUTH2_CLIENT_ID and "
                         "GS_OAUTH2_CLIENT_SECRET must be set");
                return false;
            }
            else if (nCount == 2)
            {
                bClientInfoFromEnv = true;
            }
            else if (nCount == 0)
            {
                nCount = (!osOAuth2ClientId.empty() ? 1 : 0) +
                         (!osOAuth2ClientSecret.empty() ? 1 : 0);
                if (nCount == 1)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Either both or none of client_id and "
                             "client_secret from %s must be set",
                             osCredentials.c_str());
                    return false;
                }
                else if (nCount == 2)
                {
                    osClientId = std::move(osOAuth2ClientId);
                    osClientSecret = std::move(osOAuth2ClientSecret);
                    bClientInfoFromFile = true;
                }
            }

            if (bFirstTimeForDebugMessage)
            {
                CPLString osMsg;
                osMsg.Printf("Using gs_oauth2_refresh_token from %s",
                             osCredentials.c_str());
                if (bClientInfoFromEnv)
                    osMsg += " and GS_OAUTH2_CLIENT_ID and "
                             "GS_OAUTH2_CLIENT_SECRET configuration options";
                else if (bClientInfoFromFile)
                    osMsg +=
                        CPLSPrintf(" and client_id and client_secret from %s",
                                   osCredentials.c_str());
                else
                    osMsg += " and GDAL default client_id/client_secret";
                CPLDebug("GS", "%s", osMsg.c_str());
            }
            bFirstTimeForDebugMessage = false;
            return oManager.SetAuthFromRefreshToken(
                osOAuth2RefreshToken.c_str(), osClientId.c_str(),
                osClientSecret.c_str(), nullptr);
        }
        else
        {
            if (bFirstTimeForDebugMessage)
            {
                CPLDebug(
                    "GS",
                    "Using gs_access_key_id and gs_secret_access_key from %s",
                    osCredentials.c_str());
            }
            bFirstTimeForDebugMessage = false;
            return true;
        }
    }

    if (oStaticManager.GetAuthMethod() == GOA2Manager::GCE)
    {
        CPLMutexHolder oHolder(&hMutex);
        oManager = oStaticManager;
        return true;
    }
    // Some Travis-CI workers are GCE machines, and for some tests, we don't
    // want this code path to be taken. And on AppVeyor/Window, we would also
    // attempt a network access
    else if (!CPLTestBool(CPLGetConfigOption("CPL_GCE_SKIP", "NO")) &&
             CPLIsMachinePotentiallyGCEInstance())
    {
        oManager.SetAuthFromGCE(nullptr);
        if (oManager.GetBearer() != nullptr)
        {
            CPLDebug("GS", "Using GCE inherited permissions");

            {
                CPLMutexHolder oHolder(&hMutex);
                oStaticManager = oManager;
            }

            bFirstTimeForDebugMessage = false;
            return true;
        }
    }

    if (bMayWarnDidNotFindAuth)
    {
        CPLDebug("GS", "Cannot find Authorization header in %s",
                 CPLGetConfigOption("GDAL_HTTP_HEADER_FILE", ""));
    }

    CPLString osMsg;
    osMsg.Printf("GS_SECRET_ACCESS_KEY+GS_ACCESS_KEY_ID, "
                 "GS_OAUTH2_REFRESH_TOKEN or "
                 "GOOGLE_APPLICATION_CREDENTIALS or "
                 "GS_OAUTH2_PRIVATE_KEY+GS_OAUTH2_CLIENT_EMAIL and %s, "
                 "or GS_NO_SIGN_REQUEST=YES configuration options not defined",
                 osCredentials.c_str());

    CPLDebug("GS", "%s", osMsg.c_str());
    VSIError(VSIE_AWSInvalidCredentials, "%s", osMsg.c_str());
    return false;
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIGSHandleHelper *VSIGSHandleHelper::BuildFromURI(
    const char *pszURI, const char * /*pszFSPrefix*/,
    const char *pszURIForPathSpecificOption, CSLConstList papszOptions)
{
    std::string osPathForOption("/vsigs/");
    osPathForOption +=
        pszURIForPathSpecificOption ? pszURIForPathSpecificOption : pszURI;

    // pszURI == bucket/object
    const std::string osBucketObject(pszURI);
    std::string osEndpoint(VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                    "CPL_GS_ENDPOINT", ""));
    if (osEndpoint.empty())
        osEndpoint = "https://storage.googleapis.com/";

    std::string osSecretAccessKey;
    std::string osAccessKeyId;
    bool bUseAuthenticationHeader;
    GOA2Manager oManager;

    if (!GetConfiguration(osPathForOption, papszOptions, osSecretAccessKey,
                          osAccessKeyId, bUseAuthenticationHeader, oManager))
    {
        return nullptr;
    }

    // https://cloud.google.com/storage/docs/xml-api/reference-headers#xgooguserproject
    // The Project ID for an existing Google Cloud project to bill for access
    // charges associated with the request.
    const std::string osUserProject = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "GS_USER_PROJECT", "");

    return new VSIGSHandleHelper(osEndpoint, osBucketObject, osSecretAccessKey,
                                 osAccessKeyId, bUseAuthenticationHeader,
                                 oManager, osUserProject);
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIGSHandleHelper::RebuildURL()
{
    m_osURL = m_osEndpoint + CPLAWSURLEncode(m_osBucketObjectKey, false);
    if (!m_osBucketObjectKey.empty() &&
        m_osBucketObjectKey.find('/') == std::string::npos)
        m_osURL += "/";
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                           UsesHMACKey()                              */
/************************************************************************/

bool VSIGSHandleHelper::UsesHMACKey() const
{
    return m_oManager.GetAuthMethod() == GOA2Manager::NONE;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIGSHandleHelper::GetCurlHeaders(const std::string &osVerb,
                                  const struct curl_slist *psExistingHeaders,
                                  const void *, size_t) const
{
    if (m_bUseAuthenticationHeader)
        return nullptr;

    if (m_oManager.GetAuthMethod() != GOA2Manager::NONE)
    {
        const char *pszBearer = m_oManager.GetBearer();
        if (pszBearer == nullptr)
            return nullptr;

        {
            CPLMutexHolder oHolder(&hMutex);
            oStaticManager = m_oManager;
        }

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(
            headers, CPLSPrintf("Authorization: Bearer %s", pszBearer));

        if (!m_osUserProject.empty())
        {
            headers =
                curl_slist_append(headers, CPLSPrintf("x-goog-user-project: %s",
                                                      m_osUserProject.c_str()));
        }
        return headers;
    }

    std::string osCanonicalResource(
        "/" + CPLAWSURLEncode(m_osBucketObjectKey, false));
    if (!m_osBucketObjectKey.empty() &&
        m_osBucketObjectKey.find('/') == std::string::npos)
        osCanonicalResource += "/";
    else
    {
        const auto osQueryString(GetQueryString(false));
        if (osQueryString == "?uploads" || osQueryString == "?acl")
            osCanonicalResource += osQueryString;
    }

    return GetGSHeaders("/vsigs/" + m_osBucketObjectKey, osVerb,
                        psExistingHeaders, osCanonicalResource,
                        m_osSecretAccessKey, m_osAccessKeyId, m_osUserProject);
}

/************************************************************************/
/*                          CleanMutex()                                */
/************************************************************************/

void VSIGSHandleHelper::CleanMutex()
{
    if (hMutex != nullptr)
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
}

/************************************************************************/
/*                          ClearCache()                                */
/************************************************************************/

void VSIGSHandleHelper::ClearCache()
{
    CPLMutexHolder oHolder(&hMutex);

    oStaticManager = GOA2Manager();
    bFirstTimeForDebugMessage = true;
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

std::string VSIGSHandleHelper::GetSignedURL(CSLConstList papszOptions)
{
    if (!((!m_osAccessKeyId.empty() && !m_osSecretAccessKey.empty()) ||
          m_oManager.GetAuthMethod() == GOA2Manager::SERVICE_ACCOUNT))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Signed URL for Google Cloud Storage is only available with "
                 "AWS style authentication with "
                 "GS_ACCESS_KEY_ID+GS_SECRET_ACCESS_KEY, "
                 "or with service account authentication");
        return std::string();
    }

    GIntBig nStartDate = static_cast<GIntBig>(time(nullptr));
    const char *pszStartDate = CSLFetchNameValue(papszOptions, "START_DATE");
    if (pszStartDate)
    {
        int nYear, nMonth, nDay, nHour, nMin, nSec;
        if (sscanf(pszStartDate, "%04d%02d%02dT%02d%02d%02dZ", &nYear, &nMonth,
                   &nDay, &nHour, &nMin, &nSec) == 6)
        {
            struct tm brokendowntime;
            brokendowntime.tm_year = nYear - 1900;
            brokendowntime.tm_mon = nMonth - 1;
            brokendowntime.tm_mday = nDay;
            brokendowntime.tm_hour = nHour;
            brokendowntime.tm_min = nMin;
            brokendowntime.tm_sec = nSec;
            nStartDate = CPLYMDHMSToUnixTime(&brokendowntime);
        }
    }
    GIntBig nExpiresIn =
        nStartDate +
        atoi(CSLFetchNameValueDef(papszOptions, "EXPIRATION_DELAY", "3600"));
    std::string osExpires(CSLFetchNameValueDef(
        papszOptions, "EXPIRES", CPLSPrintf(CPL_FRMT_GIB, nExpiresIn)));

    std::string osVerb(CSLFetchNameValueDef(papszOptions, "VERB", "GET"));

    std::string osCanonicalizedResource(
        "/" + CPLAWSURLEncode(m_osBucketObjectKey, false));

    std::string osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += "\n";  // Content_MD5
    osStringToSign += "\n";  // Content_Type
    osStringToSign += osExpires + "\n";
    // osStringToSign += // Canonicalized_Extension_Headers
    osStringToSign += osCanonicalizedResource;
#ifdef DEBUG_VERBOSE
    CPLDebug("GS", "osStringToSign = %s", osStringToSign.c_str());
#endif

    if (!m_osAccessKeyId.empty())
    {
        // No longer documented but actually works !
        GByte abySignature[CPL_SHA1_HASH_SIZE] = {};
        CPL_HMAC_SHA1(m_osSecretAccessKey.c_str(), m_osSecretAccessKey.size(),
                      osStringToSign.c_str(), osStringToSign.size(),
                      abySignature);

        char *pszBase64 = CPLBase64Encode(sizeof(abySignature), abySignature);
        std::string osSignature(pszBase64);
        CPLFree(pszBase64);

        ResetQueryParameters();
        AddQueryParameter("GoogleAccessId", m_osAccessKeyId);
        AddQueryParameter("Expires", osExpires);
        AddQueryParameter("Signature", osSignature);
    }
    else
    {
        unsigned nSignatureLen = 0;
        GByte *pabySignature = CPL_RSA_SHA256_Sign(
            m_oManager.GetPrivateKey().c_str(), osStringToSign.data(),
            static_cast<unsigned>(osStringToSign.size()), &nSignatureLen);
        if (pabySignature == nullptr)
            return std::string();
        char *pszBase64 = CPLBase64Encode(nSignatureLen, pabySignature);
        CPLFree(pabySignature);
        std::string osSignature(pszBase64);
        CPLFree(pszBase64);

        ResetQueryParameters();
        AddQueryParameter("GoogleAccessId", m_oManager.GetClientEmail());
        AddQueryParameter("Expires", osExpires);
        AddQueryParameter("Signature", osSignature);
    }
    return m_osURL;
}

#endif

//! @endcond
