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

#ifndef CPL_GOOGLE_CLOUD_INCLUDED_H
#define CPL_GOOGLE_CLOUD_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include <cstddef>

#include "cpl_string.h"

#ifdef HAVE_CURL

#include <curl/curl.h>
#include "cpl_http.h"
#include "cpl_aws.h"
#include <map>

class VSIGSHandleHelper final : public IVSIS3LikeHandleHelper
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGSHandleHelper)

    std::string m_osURL;
    std::string m_osEndpoint;
    std::string m_osBucketObjectKey;
    std::string m_osSecretAccessKey;
    std::string m_osAccessKeyId;
    bool m_bUseAuthenticationHeader;
    GOA2Manager m_oManager;
    std::string m_osUserProject{};

    static bool GetConfiguration(const std::string &osPathForOption,
                                 CSLConstList papszOptions,
                                 std::string &osSecretAccessKey,
                                 std::string &osAccessKeyId,
                                 bool &bUseAuthenticationHeader,
                                 GOA2Manager &oManager);

    static bool GetConfigurationFromConfigFile(
        std::string &osSecretAccessKey, std::string &osAccessKeyId,
        std::string &osOAuth2RefreshToken, std::string &osOAuth2ClientId,
        std::string &osOAuth2ClientSecret, std::string &osCredentials);

    void RebuildURL() override;

  public:
    VSIGSHandleHelper(const std::string &osEndpoint,
                      const std::string &osBucketObjectKey,
                      const std::string &osSecretAccessKey,
                      const std::string &osAccessKeyId, bool bUseHeaderFile,
                      const GOA2Manager &oManager,
                      const std::string &osUserProject);
    ~VSIGSHandleHelper();

    static VSIGSHandleHelper *
    BuildFromURI(const char *pszURI, const char *pszFSPrefix,
                 const char *pszURIForPathSpecificOption = nullptr,
                 CSLConstList papszOptions = nullptr);

    bool UsesHMACKey() const;

    struct curl_slist *
    GetCurlHeaders(const std::string &osVerbosVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const override;

    const std::string &GetURL() const override
    {
        return m_osURL;
    }

    std::string GetCopySourceHeader() const override
    {
        return "x-goog-copy-source";
    }

    const char *GetMetadataDirectiveREPLACE() const override
    {
        return "x-goog-metadata-directive: REPLACE";
    }

    std::string GetSignedURL(CSLConstList papszOptions);

    static void CleanMutex();
    static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_GOOGLE_CLOUD_INCLUDED_H */
