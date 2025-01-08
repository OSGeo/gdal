/**********************************************************************
 *
 * Name:     cpl_alibaba_oss.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Alibaba Cloud Object Storage Service
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ALIBABA_OSS_INCLUDED_H
#define CPL_ALIBABA_OSS_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include <cstddef>

#include "cpl_string.h"

#ifdef HAVE_CURL

#include <curl/curl.h>
#include <map>
#include "cpl_aws.h"

class VSIOSSHandleHelper final : public IVSIS3LikeHandleHelper
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSHandleHelper)

    std::string m_osURL{};
    std::string m_osSecretAccessKey{};
    std::string m_osAccessKeyId{};
    std::string m_osEndpoint{};
    std::string m_osBucket{};
    std::string m_osObjectKey{};
    bool m_bUseHTTPS = false;
    bool m_bUseVirtualHosting = false;

    void RebuildURL() override;

    static bool GetConfiguration(const std::string &osPathForOption,
                                 CSLConstList papszOptions,
                                 std::string &osSecretAccessKey,
                                 std::string &osAccessKeyId);

  protected:
  public:
    VSIOSSHandleHelper(const std::string &osSecretAccessKey,
                       const std::string &osAccessKeyId,
                       const std::string &osEndpoint,
                       const std::string &osBucket,
                       const std::string &osObjectKey, bool bUseHTTPS,
                       bool bUseVirtualHosting);
    ~VSIOSSHandleHelper();

    static VSIOSSHandleHelper *
    BuildFromURI(const char *pszURI, const char *pszFSPrefix,
                 bool bAllowNoObject, CSLConstList papszOptions = nullptr);
    static std::string BuildURL(const std::string &osEndpoint,
                                const std::string &osBucket,
                                const std::string &osObjectKey, bool bUseHTTPS,
                                bool bUseVirtualHosting);

    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const override;

    bool CanRestartOnError(const char *, const char *pszHeaders,
                           bool bSetError) override;

    const std::string &GetURL() const override
    {
        return m_osURL;
    }

    const std::string &GetBucket() const
    {
        return m_osBucket;
    }

    const std::string &GetObjectKey() const
    {
        return m_osObjectKey;
    }

    const std::string &GetEndpoint() const
    {
        return m_osEndpoint;
    }

    bool GetVirtualHosting() const
    {
        return m_bUseVirtualHosting;
    }

    std::string GetCopySourceHeader() const override
    {
        return "x-oss-copy-source";
    }

    void SetEndpoint(const std::string &osStr);
    void SetVirtualHosting(bool b);

    std::string GetSignedURL(CSLConstList papszOptions);
};

class VSIOSSUpdateParams
{
  private:
    std::string m_osEndpoint{};

    explicit VSIOSSUpdateParams(const VSIOSSHandleHelper *poHelper)
        : m_osEndpoint(poHelper->GetEndpoint())
    {
    }

    void UpdateHandlerHelper(VSIOSSHandleHelper *poHelper)
    {
        poHelper->SetEndpoint(m_osEndpoint);
    }

    static std::mutex gsMutex;
    static std::map<std::string, VSIOSSUpdateParams> goMapBucketsToOSSParams;

  public:
    VSIOSSUpdateParams() = default;

    static void UpdateMapFromHandle(VSIOSSHandleHelper *poHandleHelper);
    static void UpdateHandleFromMap(VSIOSSHandleHelper *poHandleHelper);
    static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_ALIBABA_OSS_INCLUDED_H */
