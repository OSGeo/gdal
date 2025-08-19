/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  Microsoft Azure Storage Blob routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_AZURE_INCLUDED_H
#define CPL_AZURE_INCLUDED_H

#ifndef DOXYGEN_SKIP

#ifdef HAVE_CURL

#include <curl/curl.h>
#include "cpl_http.h"
#include "cpl_aws.h"
#include <map>

class VSIAzureBlobHandleHelper final : public IVSIS3LikeHandleHelper
{
    std::string m_osPathForOption;
    std::string m_osURL;
    std::string m_osEndpoint;
    std::string m_osBucket;
    std::string m_osObjectKey;
    std::string m_osStorageAccount;
    std::string m_osStorageKey;
    std::string m_osSAS;
    std::string m_osAccessToken;
    bool m_bFromManagedIdentities;
    bool m_bIncludeMSVersion = true;

    enum class Service
    {
        SERVICE_BLOB,
        SERVICE_ADLS,
    };

    static bool GetConfiguration(const std::string &osPathForOption,
                                 CSLConstList papszOptions, Service eService,
                                 bool &bUseHTTPS, std::string &osEndpoint,
                                 std::string &osStorageAccount,
                                 std::string &osStorageKey, std::string &osSAS,
                                 std::string &osAccessToken,
                                 bool &bFromManagedIdentities);

    static std::string BuildURL(const std::string &osEndpoint,
                                const std::string &osBucket,
                                const std::string &osObjectKey,
                                const std::string &osSAS);

    void RebuildURL() override;

  public:
    VSIAzureBlobHandleHelper(
        const std::string &osPathForOption, const std::string &osEndpoint,
        const std::string &osBucket, const std::string &osObjectKey,
        const std::string &osStorageAccount, const std::string &osStorageKey,
        const std::string &osSAS, const std::string &osAccessToken,
        bool bFromManagedIdentities);
    ~VSIAzureBlobHandleHelper();

    static VSIAzureBlobHandleHelper *
    BuildFromURI(const char *pszURI, const char *pszFSPrefix,
                 const char *pszURIForPathSpecificOption = nullptr,
                 CSLConstList papszOptions = nullptr);

    void SetIncludeMSVersion(bool bInclude)
    {
        m_bIncludeMSVersion = bInclude;
    }

    struct curl_slist *
    GetCurlHeaders(const std::string &osVerbosVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const override;

    const std::string &GetURL() const override
    {
        return m_osURL;
    }

    std::string GetSignedURL(CSLConstList papszOptions);

    static void ClearCache();

    std::string GetSASQueryString() const;

    const std::string &GetStorageAccount() const
    {
        return m_osStorageAccount;
    }

    const std::string &GetBucket() const
    {
        return m_osBucket;
    }

    static std::string GetSAS(const char *pszFilename);

    static bool IsNoSignRequest(const char *pszFilename);
};

namespace cpl
{
int GetAzureAppendBufferSize();
}

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AZURE_INCLUDED_H */
