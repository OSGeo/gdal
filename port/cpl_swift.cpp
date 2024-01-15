/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  OpenStack Swift Object Storage routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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
 * DEALINSwift IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_swift.h"
#include "cpl_vsi_error.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_json.h"

// HOWTO setup a Docker-based SWIFT server:
// https://github.com/MorrisJobke/docker-swift-onlyone

//! @cond Doxygen_Suppress

#ifdef HAVE_CURL

static CPLMutex *g_hMutex = nullptr;
static std::string g_osLastAuthURL;
static std::string g_osLastUser;
static std::string g_osLastKey;
static std::string g_osLastStorageURL;
static std::string g_osLastAuthToken;

/************************************************************************/
/*                          GetSwiftHeaders()                           */
/************************************************************************/

static struct curl_slist *GetSwiftHeaders(const std::string &osAuthToken)
{
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(
        headers, CPLSPrintf("x-auth-token: %s", osAuthToken.c_str()));
    return headers;
}

/************************************************************************/
/*                     VSISwiftHandleHelper()                           */
/************************************************************************/
VSISwiftHandleHelper::VSISwiftHandleHelper(const std::string &osStorageURL,
                                           const std::string &osAuthToken,
                                           const std::string &osBucket,
                                           const std::string &osObjectKey)
    : m_osURL(BuildURL(osStorageURL, osBucket, osObjectKey)),
      m_osStorageURL(osStorageURL), m_osAuthToken(osAuthToken),
      m_osBucket(osBucket), m_osObjectKey(osObjectKey)
{
}

/************************************************************************/
/*                      ~VSISwiftHandleHelper()                         */
/************************************************************************/

VSISwiftHandleHelper::~VSISwiftHandleHelper()
{
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSISwiftHandleHelper::GetConfiguration(const std::string &osPathForOption,
                                            std::string &osStorageURL,
                                            std::string &osAuthToken)
{
    osStorageURL = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                            "SWIFT_STORAGE_URL", "");
    if (!osStorageURL.empty())
    {
        osAuthToken = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                               "SWIFT_AUTH_TOKEN", "");
        if (osAuthToken.empty())
        {
            const char *pszMsg = "Missing SWIFT_AUTH_TOKEN";
            CPLDebug("SWIFT", "%s", pszMsg);
            VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
            return false;
        }
        return true;
    }

    const std::string osAuthVersion = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "OS_IDENTITY_API_VERSION", "");
    if (osAuthVersion == "3")
    {
        const std::string osAuthType = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_AUTH_TYPE", "");
        if (!CheckCredentialsV3(osPathForOption, osAuthType))
            return false;
        if (osAuthType == "v3applicationcredential")
        {
            if (GetCached(osPathForOption, "OS_AUTH_URL",
                          "OS_APPLICATION_CREDENTIAL_ID",
                          "OS_APPLICATION_CREDENTIAL_SECRET", osStorageURL,
                          osAuthToken))
                return true;
        }
        else
        {
            if (GetCached(osPathForOption, "OS_AUTH_URL", "OS_USERNAME",
                          "OS_PASSWORD", osStorageURL, osAuthToken))
                return true;
        }
        if (AuthV3(osPathForOption, osAuthType, osStorageURL, osAuthToken))
            return true;
    }
    else
    {
        const std::string osAuthV1URL = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "SWIFT_AUTH_V1_URL", "");
        if (!osAuthV1URL.empty())
        {
            if (!CheckCredentialsV1(osPathForOption))
                return false;
            if (GetCached(osPathForOption, "SWIFT_AUTH_V1_URL", "SWIFT_USER",
                          "SWIFT_KEY", osStorageURL, osAuthToken))
                return true;
            if (AuthV1(osPathForOption, osStorageURL, osAuthToken))
                return true;
        }
    }

    const char *pszMsg = "Missing SWIFT_STORAGE_URL+SWIFT_AUTH_TOKEN or "
                         "appropriate authentication options";
    CPLDebug("SWIFT", "%s", pszMsg);
    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);

    return false;
}

/************************************************************************/
/*                               AuthV1()                               */
/************************************************************************/

bool VSISwiftHandleHelper::AuthV1(const std::string &osPathForOption,
                                  std::string &osStorageURL,
                                  std::string &osAuthToken)
{
    std::string osAuthURL = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                     "SWIFT_AUTH_V1_URL", "");
    std::string osUser =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "SWIFT_USER", "");
    std::string osKey =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "SWIFT_KEY", "");
    char **papszHeaders =
        CSLSetNameValue(nullptr, "HEADERS",
                        CPLSPrintf("X-Auth-User: %s\r\n"
                                   "X-Auth-Key: %s",
                                   osUser.c_str(), osKey.c_str()));
    CPLHTTPResult *psResult = CPLHTTPFetch(osAuthURL.c_str(), papszHeaders);
    CSLDestroy(papszHeaders);
    if (psResult == nullptr)
        return false;
    osStorageURL =
        CSLFetchNameValueDef(psResult->papszHeaders, "X-Storage-Url", "");
    osAuthToken =
        CSLFetchNameValueDef(psResult->papszHeaders, "X-Auth-Token", "");
    std::string osErrorMsg =
        psResult->pabyData ? reinterpret_cast<const char *>(psResult->pabyData)
                           : "";
    CPLHTTPDestroyResult(psResult);
    if (osStorageURL.empty() || osAuthToken.empty())
    {
        CPLDebug("SWIFT", "Authentication failed: %s", osErrorMsg.c_str());
        VSIError(VSIE_AWSInvalidCredentials, "Authentication failed: %s",
                 osErrorMsg.c_str());
        return false;
    }

    // Cache credentials
    {
        CPLMutexHolder oHolder(&g_hMutex);
        g_osLastAuthURL = std::move(osAuthURL);
        g_osLastUser = std::move(osUser);
        g_osLastKey = std::move(osKey);
        g_osLastStorageURL = osStorageURL;
        g_osLastAuthToken = osAuthToken;
    }

    return true;
}

/************************************************************************/
/*                      CreateAuthV3RequestObject()                     */
/************************************************************************/

CPLJSONObject VSISwiftHandleHelper::CreateAuthV3RequestObject(
    const std::string &osPathForOption, const std::string &osAuthType)
{
    CPLJSONArray methods;
    CPLJSONObject identity;
    CPLJSONObject scope;
    if (osAuthType == "v3applicationcredential")
    {
        std::string osApplicationCredentialID = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_APPLICATION_CREDENTIAL_ID", "");
        std::string osApplicationCredentialSecret = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_APPLICATION_CREDENTIAL_SECRET", "");
        CPLJSONObject applicationCredential;
        applicationCredential.Add("id", osApplicationCredentialID);
        applicationCredential.Add("secret", osApplicationCredentialSecret);
        methods.Add("application_credential");
        identity.Add("application_credential", applicationCredential);
        // Application credentials cannot request a scope.
    }
    else
    {
        std::string osUser = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                      "OS_USERNAME", "");
        std::string osPassword = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_PASSWORD", "");

        CPLJSONObject user;
        user.Add("name", osUser);
        user.Add("password", osPassword);

        std::string osUserDomainName = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_USER_DOMAIN_NAME", "");
        if (!osUserDomainName.empty())
        {
            CPLJSONObject userDomain;
            userDomain.Add("name", osUserDomainName);
            user.Add("domain", userDomain);
        }

        CPLJSONObject password;
        password.Add("user", user);
        methods.Add("password");
        identity.Add("password", password);

        // Request a scope if one is specified in the configuration
        std::string osProjectName = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_PROJECT_NAME", "");
        if (!osProjectName.empty())
        {
            CPLJSONObject project;
            project.Add("name", osProjectName);

            std::string osProjectDomainName = VSIGetPathSpecificOption(
                osPathForOption.c_str(), "OS_PROJECT_DOMAIN_NAME", "");
            if (!osProjectDomainName.empty())
            {
                CPLJSONObject projectDomain;
                projectDomain.Add("name", osProjectDomainName);
                project.Add("domain", projectDomain);
            }

            scope.Add("project", project);
        }
    }

    identity.Add("methods", methods);

    CPLJSONObject auth;
    auth.Add("identity", identity);
    if (!scope.GetChildren().empty())
        auth.Add("scope", scope);

    CPLJSONObject obj;
    obj.Add("auth", auth);
    return obj;
}

/************************************************************************/
/*                      GetAuthV3StorageURL()                           */
/************************************************************************/

bool VSISwiftHandleHelper::GetAuthV3StorageURL(
    const std::string &osPathForOption, const CPLHTTPResult *psResult,
    std::string &storageURL)
{
    if (psResult->pabyData == nullptr)
        return false;

    CPLJSONDocument resultJson;
    resultJson.LoadMemory(psResult->pabyData);
    CPLJSONObject result(resultJson.GetRoot());

    CPLJSONObject token(result.GetObj("token"));
    if (!token.IsValid())
        return false;

    CPLJSONArray catalog(token.GetArray("catalog"));
    if (!catalog.IsValid())
        return false;

    CPLJSONArray endpoints;
    for (int i = 0; i < catalog.Size(); ++i)
    {
        CPLJSONObject item(catalog[i]);
        if (item.GetString("type") == "object-store")
        {
            endpoints = item.GetArray("endpoints");
            break;
        }
    }

    if (endpoints.Size() == 0)
        return false;

    std::string osRegionName =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "OS_REGION_NAME", "");
    if (osRegionName.empty())
    {
        for (int i = 0; i < endpoints.Size(); ++i)
        {
            CPLJSONObject endpoint(endpoints[i]);
            std::string interfaceType =
                endpoint.GetString("interface", "");  // internal, admin, public
            if (interfaceType.empty() || interfaceType == "public")
            {
                storageURL = endpoint.GetString("url");
                return true;
            }
        }
        return false;
    }

    for (int i = 0; i < endpoints.Size(); ++i)
    {
        CPLJSONObject endpoint(endpoints[i]);
        if (endpoint.GetString("region") == osRegionName)
        {
            std::string interfaceType =
                endpoint.GetString("interface", "");  // internal, admin, public
            if (interfaceType.empty() || interfaceType == "public")
            {
                storageURL = endpoint.GetString("url");
                CPLDebug("SWIFT", "Storage URL '%s' for region '%s'",
                         storageURL.c_str(), osRegionName.c_str());
                return true;
            }
        }
    }

    return false;
}

/************************************************************************/
/*                                AuthV3()                              */
/************************************************************************/

bool VSISwiftHandleHelper::AuthV3(const std::string &osPathForOption,
                                  const std::string &osAuthType,
                                  std::string &osStorageURL,
                                  std::string &osAuthToken)
{
    std::string osAuthID;
    std::string osAuthKey;
    if (osAuthType.empty() || osAuthType == "password")
    {
        osAuthID = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                            "OS_USERNAME", "");
        osAuthKey = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                             "OS_PASSWORD", "");
    }
    else if (osAuthType == "v3applicationcredential")
    {
        osAuthID = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                            "OS_APPLICATION_CREDENTIAL_ID", "");
        osAuthKey = VSIGetPathSpecificOption(
            osPathForOption.c_str(), "OS_APPLICATION_CREDENTIAL_SECRET", "");
    }
    else
    {
        CPLDebug("SWIFT", "Unsupported OS SWIFT Auth Type: %s",
                 osAuthType.c_str());
        VSIError(VSIE_AWSInvalidCredentials, "%s", osAuthType.c_str());
        return false;
    }
    CPLJSONObject postObject(
        CreateAuthV3RequestObject(osPathForOption, osAuthType));
    std::string post = postObject.Format(CPLJSONObject::PrettyFormat::Plain);

    // coverity[tainted_data]
    std::string osAuthURL =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "OS_AUTH_URL", "");
    std::string url = osAuthURL;
    if (!url.empty() && url.back() != '/')
        url += '/';
    url += "auth/tokens";

    char **papszOptions = CSLSetNameValue(nullptr, "POSTFIELDS", post.data());
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/json");
    CPLHTTPResult *psResult = CPLHTTPFetchEx(url.c_str(), papszOptions, nullptr,
                                             nullptr, nullptr, nullptr);
    CSLDestroy(papszOptions);

    if (psResult == nullptr)
        return false;

    osAuthToken =
        CSLFetchNameValueDef(psResult->papszHeaders, "X-Subject-Token", "");

    if (!GetAuthV3StorageURL(osPathForOption, psResult, osStorageURL))
    {
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if (osStorageURL.empty() || osAuthToken.empty())
    {
        std::string osErrorMsg =
            reinterpret_cast<const char *>(psResult->pabyData);
        CPLDebug("SWIFT", "Authentication failed: %s", osErrorMsg.c_str());
        VSIError(VSIE_AWSInvalidCredentials, "Authentication failed: %s",
                 osErrorMsg.c_str());
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    CPLHTTPDestroyResult(psResult);

    // Cache credentials
    {
        CPLMutexHolder oHolder(&g_hMutex);
        g_osLastAuthURL = std::move(osAuthURL);
        g_osLastUser = std::move(osAuthID);
        g_osLastKey = std::move(osAuthKey);
        g_osLastStorageURL = osStorageURL;
        g_osLastAuthToken = osAuthToken;
    }
    return true;
}

/************************************************************************/
/*                           Authenticate()                             */
/************************************************************************/

bool VSISwiftHandleHelper::Authenticate(const std::string &osPathForOption)
{
    std::string osAuthV1URL = VSIGetPathSpecificOption(osPathForOption.c_str(),
                                                       "SWIFT_AUTH_V1_URL", "");
    if (!osAuthV1URL.empty() &&
        AuthV1(osPathForOption, m_osStorageURL, m_osAuthToken))
    {
        RebuildURL();
        return true;
    }

    const std::string osAuthVersion = VSIGetPathSpecificOption(
        osPathForOption.c_str(), "OS_IDENTITY_API_VERSION", "");
    const std::string osAuthType =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "OS_AUTH_TYPE", "");
    if (osAuthVersion == "3" &&
        AuthV3(osPathForOption, osAuthType, m_osStorageURL, m_osAuthToken))
    {
        RebuildURL();
        return true;
    }

    return false;
}

/************************************************************************/
/*                         CheckCredentialsV1()                         */
/************************************************************************/

bool VSISwiftHandleHelper::CheckCredentialsV1(
    const std::string &osPathForOption)
{
    const char *pszMissingKey = nullptr;
    std::string osUser =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "SWIFT_USER", "");
    std::string osKey =
        VSIGetPathSpecificOption(osPathForOption.c_str(), "SWIFT_KEY", "");
    if (osUser.empty())
    {
        pszMissingKey = "SWIFT_USER";
    }
    else if (osKey.empty())
    {
        pszMissingKey = "SWIFT_KEY";
    }

    if (pszMissingKey)
    {
        CPLDebug("SWIFT", "Missing %s configuration option", pszMissingKey);
        VSIError(VSIE_AWSInvalidCredentials, "%s", pszMissingKey);
        return false;
    }

    return true;
}

/************************************************************************/
/*                         CheckCredentialsV3()                         */
/************************************************************************/

bool VSISwiftHandleHelper::CheckCredentialsV3(
    const std::string &osPathForOption, const std::string &osAuthType)
{
    const char *papszMandatoryOptionKeys[3] = {
        "OS_AUTH_URL",
        "",
        "",
    };
    if (osAuthType.empty() || osAuthType == "password")
    {
        papszMandatoryOptionKeys[1] = "OS_USERNAME";
        papszMandatoryOptionKeys[2] = "OS_PASSWORD";
    }
    else if (osAuthType == "v3applicationcredential")
    {
        papszMandatoryOptionKeys[1] = "OS_APPLICATION_CREDENTIAL_ID";
        papszMandatoryOptionKeys[2] = "OS_APPLICATION_CREDENTIAL_SECRET";
    }
    else
    {
        CPLDebug("SWIFT", "Unsupported OS SWIFT Auth Type: %s",
                 osAuthType.c_str());
        VSIError(VSIE_AWSInvalidCredentials, "%s", osAuthType.c_str());
        return false;
    }
    for (auto const *pszOptionKey : papszMandatoryOptionKeys)
    {
        std::string option =
            VSIGetPathSpecificOption(osPathForOption.c_str(), pszOptionKey, "");
        if (option.empty())
        {
            CPLDebug("SWIFT", "Missing %s configuration option", pszOptionKey);
            VSIError(VSIE_AWSInvalidCredentials, "%s", pszOptionKey);
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                            GetCached()                               */
/************************************************************************/

bool VSISwiftHandleHelper::GetCached(const std::string &osPathForOption,
                                     const char *pszURLKey,
                                     const char *pszUserKey,
                                     const char *pszPasswordKey,
                                     std::string &osStorageURL,
                                     std::string &osAuthToken)
{
    std::string osAuthURL =
        VSIGetPathSpecificOption(osPathForOption.c_str(), pszURLKey, "");
    std::string osUser =
        VSIGetPathSpecificOption(osPathForOption.c_str(), pszUserKey, "");
    std::string osKey =
        VSIGetPathSpecificOption(osPathForOption.c_str(), pszPasswordKey, "");

    CPLMutexHolder oHolder(&g_hMutex);
    // Re-use cached credentials if available
    // coverity[tainted_data]
    if (osAuthURL == g_osLastAuthURL && osUser == g_osLastUser &&
        osKey == g_osLastKey)
    {
        osStorageURL = g_osLastStorageURL;
        osAuthToken = g_osLastAuthToken;
        return true;
    }
    return false;
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSISwiftHandleHelper *
VSISwiftHandleHelper::BuildFromURI(const char *pszURI,
                                   const char * /*pszFSPrefix*/)
{
    std::string osPathForOption("/vsiswift/");
    osPathForOption += pszURI;

    std::string osStorageURL;
    std::string osAuthToken;

    if (!GetConfiguration(osPathForOption, osStorageURL, osAuthToken))
    {
        return nullptr;
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

    return new VSISwiftHandleHelper(osStorageURL, osAuthToken, osBucket,
                                    osObjectKey);
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

std::string VSISwiftHandleHelper::BuildURL(const std::string &osStorageURL,
                                           const std::string &osBucket,
                                           const std::string &osObjectKey)
{
    std::string osURL = osStorageURL;
    if (!osBucket.empty())
        osURL += "/" + CPLAWSURLEncode(osBucket, false);
    if (!osObjectKey.empty())
        osURL += "/" + CPLAWSURLEncode(osObjectKey, false);
    return osURL;
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSISwiftHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osStorageURL, m_osBucket, m_osObjectKey);
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *VSISwiftHandleHelper::GetCurlHeaders(
    const std::string &, const struct curl_slist *, const void *, size_t) const
{
    return GetSwiftHeaders(m_osAuthToken);
}

/************************************************************************/
/*                          CleanMutex()                                */
/************************************************************************/

void VSISwiftHandleHelper::CleanMutex()
{
    if (g_hMutex != nullptr)
        CPLDestroyMutex(g_hMutex);
    g_hMutex = nullptr;
}

/************************************************************************/
/*                          ClearCache()                                */
/************************************************************************/

void VSISwiftHandleHelper::ClearCache()
{
    CPLMutexHolder oHolder(&g_hMutex);
    g_osLastAuthURL.clear();
    g_osLastUser.clear();
    g_osLastKey.clear();
    g_osLastStorageURL.clear();
    g_osLastAuthToken.clear();
}

#endif

//! @endcond
