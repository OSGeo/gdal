/**********************************************************************
 * $Id$
 *
 * Name:     cpl_aws.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Amazon Web Services routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#ifndef CPL_AWS_INCLUDED_H
#define CPL_AWS_INCLUDED_H

#ifndef DOXYGEN_SKIP

#ifdef HAVE_CURL

#include <cstddef>
#include <mutex>

#include "cpl_string.h"

#include <curl/curl.h>
#include <map>

std::string CPLGetLowerCaseHexSHA256(const void *pabyData, size_t nBytes);
std::string CPLGetLowerCaseHexSHA256(const std::string &osStr);

std::string CPLGetAWS_SIGN4_Timestamp(GIntBig timestamp);

std::string CPLAWSURLEncode(const std::string &osURL, bool bEncodeSlash = true);

std::string CPLAWSGetHeaderVal(const struct curl_slist *psExistingHeaders,
                               const char *pszKey);

std::string CPLGetAWS_SIGN4_Signature(
    const std::string &osSecretAccessKey, const std::string &osAccessToken,
    const std::string &osRegion, const std::string &osRequestPayer,
    const std::string &osService, const std::string &osVerb,
    const struct curl_slist *psExistingHeaders, const std::string &osHost,
    const std::string &osCanonicalURI,
    const std::string &osCanonicalQueryString,
    const std::string &osXAMZContentSHA256, bool bAddHeaderAMZContentSHA256,
    const std::string &osTimestamp, std::string &osSignedHeaders);

std::string CPLGetAWS_SIGN4_Authorization(
    const std::string &osSecretAccessKey, const std::string &osAccessKeyId,
    const std::string &osAccessToken, const std::string &osRegion,
    const std::string &osRequestPayer, const std::string &osService,
    const std::string &osVerb, const struct curl_slist *psExistingHeaders,
    const std::string &osHost, const std::string &osCanonicalURI,
    const std::string &osCanonicalQueryString,
    const std::string &osXAMZContentSHA256, bool bAddHeaderAMZContentSHA256,
    const std::string &osTimestamp);

class IVSIS3LikeHandleHelper
{
    CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeHandleHelper)

  protected:
    std::map<std::string, std::string> m_oMapQueryParameters{};

    virtual void RebuildURL() = 0;
    std::string GetQueryString(bool bAddEmptyValueAfterEqual) const;

  public:
    IVSIS3LikeHandleHelper() = default;
    virtual ~IVSIS3LikeHandleHelper() = default;

    void ResetQueryParameters();
    void AddQueryParameter(const std::string &osKey,
                           const std::string &osValue);

    virtual struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const = 0;

    virtual bool AllowAutomaticRedirection()
    {
        return true;
    }

    virtual bool CanRestartOnError(const char *, const char * /* pszHeaders*/,
                                   bool /*bSetError*/)
    {
        return false;
    }

    virtual const std::string &GetURL() const = 0;
    std::string GetURLNoKVP() const;

    virtual std::string GetCopySourceHeader() const
    {
        return std::string();
    }

    virtual const char *GetMetadataDirectiveREPLACE() const
    {
        return "";
    }

    static bool GetBucketAndObjectKey(const char *pszURI,
                                      const char *pszFSPrefix,
                                      bool bAllowNoObject,
                                      std::string &osBucketOut,
                                      std::string &osObjectKeyOut);

    static std::string BuildCanonicalizedHeaders(
        std::map<std::string, std::string> &oSortedMapHeaders,
        const struct curl_slist *psExistingHeaders,
        const char *pszHeaderPrefix);

    static std::string GetRFC822DateTime();
};

enum class AWSCredentialsSource
{
    REGULAR,       // credentials from env variables or ~/.aws/crediential
    EC2,           // credentials from EC2 private networking
    WEB_IDENTITY,  // credentials from Web Identity Token
                   // See
    // https://docs.aws.amazon.com/eks/latest/userguide/iam-roles-for-service-accounts.html
    ASSUMED_ROLE  // credentials from an STS assumed role
                  // See
    // https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_use_switch-role-cli.html
    // and
    // https://docs.aws.amazon.com/IAM/latest/UserGuide/id_credentials_temp_request.html
};

class VSIS3HandleHelper final : public IVSIS3LikeHandleHelper
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3HandleHelper)

    std::string m_osURL{};
    mutable std::string m_osSecretAccessKey{};
    mutable std::string m_osAccessKeyId{};
    mutable std::string m_osSessionToken{};
    std::string m_osEndpoint{};
    std::string m_osRegion{};
    std::string m_osRequestPayer{};
    std::string m_osBucket{};
    std::string m_osObjectKey{};
    bool m_bUseHTTPS = false;
    bool m_bUseVirtualHosting = false;
    AWSCredentialsSource m_eCredentialsSource = AWSCredentialsSource::REGULAR;

    void RebuildURL() override;

    static bool GetOrRefreshTemporaryCredentialsForRole(
        bool bForceRefresh, std::string &osSecretAccessKey,
        std::string &osAccessKeyId, std::string &osSessionToken,
        std::string &osRegion);

    static bool GetConfigurationFromAssumeRoleWithWebIdentity(
        bool bForceRefresh, const std::string &osPathForOption,
        const std::string &osRoleArnIn,
        const std::string &osWebIdentityTokenFileIn,
        std::string &osSecretAccessKey, std::string &osAccessKeyId,
        std::string &osSessionToken);

    static bool GetConfigurationFromEC2(bool bForceRefresh,
                                        const std::string &osPathForOption,
                                        std::string &osSecretAccessKey,
                                        std::string &osAccessKeyId,
                                        std::string &osSessionToken);

    static bool GetConfigurationFromAWSConfigFiles(
        const std::string &osPathForOption, const char *pszProfile,
        std::string &osSecretAccessKey, std::string &osAccessKeyId,
        std::string &osSessionToken, std::string &osRegion,
        std::string &osCredentials, std::string &osRoleArn,
        std::string &osSourceProfile, std::string &osExternalId,
        std::string &osMFASerial, std::string &osRoleSessionName,
        std::string &osWebIdentityTokenFile);

    static bool GetConfiguration(const std::string &osPathForOption,
                                 CSLConstList papszOptions,
                                 std::string &osSecretAccessKey,
                                 std::string &osAccessKeyId,
                                 std::string &osSessionToken,
                                 std::string &osRegion,
                                 AWSCredentialsSource &eCredentialsSource);

    void RefreshCredentials(const std::string &osPathForOption,
                            bool bForceRefresh) const;

  protected:
  public:
    VSIS3HandleHelper(
        const std::string &osSecretAccessKey, const std::string &osAccessKeyId,
        const std::string &osSessionToken, const std::string &osEndpoint,
        const std::string &osRegion, const std::string &osRequestPayer,
        const std::string &osBucket, const std::string &osObjectKey,
        bool bUseHTTPS, bool bUseVirtualHosting,
        AWSCredentialsSource eCredentialsSource);
    ~VSIS3HandleHelper();

    static VSIS3HandleHelper *BuildFromURI(const char *pszURI,
                                           const char *pszFSPrefix,
                                           bool bAllowNoObject,
                                           CSLConstList papszOptions = nullptr);
    static std::string BuildURL(const std::string &osEndpoint,
                                const std::string &osBucket,
                                const std::string &osObjectKey, bool bUseHTTPS,
                                bool bUseVirtualHosting);

    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders,
                   const void *pabyDataContent = nullptr,
                   size_t nBytesContent = 0) const override;

    bool AllowAutomaticRedirection() override
    {
        return false;
    }

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

    const std::string &GetRegion() const
    {
        return m_osRegion;
    }

    const std::string &GetRequestPayer() const
    {
        return m_osRequestPayer;
    }

    bool GetVirtualHosting() const
    {
        return m_bUseVirtualHosting;
    }

    void SetEndpoint(const std::string &osStr);
    void SetRegion(const std::string &osStr);
    void SetRequestPayer(const std::string &osStr);
    void SetVirtualHosting(bool b);

    std::string GetCopySourceHeader() const override
    {
        return "x-amz-copy-source";
    }

    const char *GetMetadataDirectiveREPLACE() const override
    {
        return "x-amz-metadata-directive: REPLACE";
    }

    std::string GetSignedURL(CSLConstList papszOptions);

    static void CleanMutex();
    static void ClearCache();
};

class VSIS3UpdateParams
{
  private:
    std::string m_osRegion{};
    std::string m_osEndpoint{};
    std::string m_osRequestPayer{};
    bool m_bUseVirtualHosting = false;

    explicit VSIS3UpdateParams(const VSIS3HandleHelper *poHelper)
        : m_osRegion(poHelper->GetRegion()),
          m_osEndpoint(poHelper->GetEndpoint()),
          m_osRequestPayer(poHelper->GetRequestPayer()),
          m_bUseVirtualHosting(poHelper->GetVirtualHosting())
    {
    }

    void UpdateHandlerHelper(VSIS3HandleHelper *poHelper)
    {
        poHelper->SetRegion(m_osRegion);
        poHelper->SetEndpoint(m_osEndpoint);
        poHelper->SetRequestPayer(m_osRequestPayer);
        poHelper->SetVirtualHosting(m_bUseVirtualHosting);
    }

    static std::mutex gsMutex;
    static std::map<std::string, VSIS3UpdateParams> goMapBucketsToS3Params;

  public:
    VSIS3UpdateParams() = default;

    static void UpdateMapFromHandle(VSIS3HandleHelper *poS3HandleHelper);
    static void UpdateHandleFromMap(VSIS3HandleHelper *poS3HandleHelper);
    static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AWS_INCLUDED_H */
