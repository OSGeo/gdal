/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  OpenStack Swift Object Storage routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_SWIFT_INCLUDED_H
#define CPL_SWIFT_INCLUDED_H

#ifndef DOXYGEN_SKIP

#ifdef HAVE_CURL

#include <curl/curl.h>
#include "cpl_http.h"
#include "cpl_aws.h"
#include "cpl_json.h"
#include <map>

class VSISwiftHandleHelper final : public IVSIS3LikeHandleHelper
{
    std::string m_osURL;
    std::string m_osStorageURL;
    std::string m_osAuthToken;
    std::string m_osBucket;
    std::string m_osObjectKey;

    static bool GetConfiguration(const std::string &osPathForOption,
                                 std::string &osStorageURL,
                                 std::string &osAuthToken);

    static bool GetCached(const std::string &osPathForOption,
                          const char *pszURLKey, const char *pszUserKey,
                          const char *pszPasswordKey, std::string &osStorageURL,
                          std::string &osAuthToken);

    static std::string BuildURL(const std::string &osStorageURL,
                                const std::string &osBucket,
                                const std::string &osObjectKey);

    void RebuildURL() override;

    // V1 Authentication
    static bool CheckCredentialsV1(const std::string &osPathForOption);
    static bool AuthV1(const std::string &osPathForOption,
                       std::string &osStorageURL, std::string &osAuthToken);

    // V3 Authentication
    static bool CheckCredentialsV3(const std::string &osPathForOption,
                                   const std::string &osAuthType);
    static bool AuthV3(const std::string &osPathForOption,
                       const std::string &osAuthType, std::string &osStorageURL,
                       std::string &osAuthToken);
    static CPLJSONObject
    CreateAuthV3RequestObject(const std::string &osPathForOption,
                              const std::string &osAuthType);
    static bool GetAuthV3StorageURL(const std::string &osPathForOption,
                                    const CPLHTTPResult *psResult,
                                    std::string &storageURL);

  public:
    VSISwiftHandleHelper(const std::string &osStorageURL,
                         const std::string &osAuthToken,
                         const std::string &osBucket,
                         const std::string &osObjectKey);
    ~VSISwiftHandleHelper();

    bool Authenticate(const std::string &osPathForOption);

    static VSISwiftHandleHelper *BuildFromURI(const char *pszURI,
                                              const char *pszFSPrefix);

    struct curl_slist *
    GetCurlHeaders(const std::string &osVerbosVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const override;

    const std::string &GetURL() const override
    {
        return m_osURL;
    }

    static void CleanMutex();
    static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_SWIFT_INCLUDED_H */
