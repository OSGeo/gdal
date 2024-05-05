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
 * DEALINGS IN THE SOFTWARE.
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
};

namespace cpl
{
int GetAzureBufferSize();
}

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AZURE_INCLUDED_H */
