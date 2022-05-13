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

CPLString CPLGetLowerCaseHexSHA256( const void *pabyData, size_t nBytes );
CPLString CPLGetLowerCaseHexSHA256( const CPLString& osStr );

CPLString CPLGetAWS_SIGN4_Timestamp();

CPLString CPLAWSURLEncode(const CPLString& osURL, bool bEncodeSlash = true);

CPLString CPLAWSGetHeaderVal(const struct curl_slist* psExistingHeaders,
                             const char* pszKey);

CPLString
CPLGetAWS_SIGN4_Signature( const CPLString& osSecretAccessKey,
                               const CPLString& osAccessToken,
                               const CPLString& osRegion,
                               const CPLString& osRequestPayer,
                               const CPLString& osService,
                               const CPLString& osVerb,
                               const struct curl_slist* psExistingHeaders,
                               const CPLString& osHost,
                               const CPLString& osCanonicalURI,
                               const CPLString& osCanonicalQueryString,
                               const CPLString& osXAMZContentSHA256,
                               bool bAddHeaderAMZContentSHA256,
                               const CPLString& osTimestamp,
                               CPLString& osSignedHeaders );

CPLString CPLGetAWS_SIGN4_Authorization(const CPLString& osSecretAccessKey,
                                        const CPLString& osAccessKeyId,
                                        const CPLString& osAccessToken,
                                        const CPLString& osRegion,
                                        const CPLString& osRequestPayer,
                                        const CPLString& osService,
                                        const CPLString& osVerb,
                                        const struct curl_slist* psExistingHeaders,
                                        const CPLString& osHost,
                                        const CPLString& osCanonicalURI,
                                        const CPLString& osCanonicalQueryString,
                                        const CPLString& osXAMZContentSHA256,
                                        const CPLString& osTimestamp);

class IVSIS3LikeHandleHelper
{
        CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeHandleHelper)

protected:
        std::map<CPLString, CPLString> m_oMapQueryParameters{};

        virtual void RebuildURL() = 0;
        CPLString GetQueryString(bool bAddEmptyValueAfterEqual) const;

public:
        IVSIS3LikeHandleHelper() = default;
        virtual ~IVSIS3LikeHandleHelper() = default;

        void ResetQueryParameters();
        void AddQueryParameter(const CPLString& osKey, const CPLString& osValue);

        virtual struct curl_slist* GetCurlHeaders(const CPLString& osVerb,
                                          const struct curl_slist* psExistingHeaders,
                                          const void *pabyDataContent = nullptr,
                                          size_t nBytesContent = 0) const = 0;

        virtual bool AllowAutomaticRedirection() { return true; }
        virtual bool CanRestartOnError(const char*, const char* /* pszHeaders*/,
                                       bool /*bSetError*/, bool* /*pbUpdateMap*/ = nullptr) { return false;}

        virtual const CPLString& GetURL() const = 0;
        CPLString GetURLNoKVP() const;

        virtual CPLString GetCopySourceHeader() const { return std::string(); }
        virtual const char* GetMetadataDirectiveREPLACE() const { return ""; }

        static bool GetBucketAndObjectKey(const char* pszURI,
                                          const char* pszFSPrefix,
                                          bool bAllowNoObject,
                                          CPLString &osBucketOut,
                                          CPLString &osObjectKeyOut);

        static CPLString BuildCanonicalizedHeaders(
                            std::map<CPLString, CPLString>& oSortedMapHeaders,
                            const struct curl_slist* psExistingHeaders,
                            const char* pszHeaderPrefix);

        static CPLString GetRFC822DateTime();
};

enum class AWSCredentialsSource
{
    REGULAR,         // credentials from env variables or ~/.aws/crediential
    EC2,             // credentials from EC2 private networking
    ASSUMED_ROLE     // credentials from an STS assumed role
                     // See https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_use_switch-role-cli.html
                     // and https://docs.aws.amazon.com/IAM/latest/UserGuide/id_credentials_temp_request.html
};

class VSIS3HandleHelper final: public IVSIS3LikeHandleHelper
{
        CPL_DISALLOW_COPY_ASSIGN(VSIS3HandleHelper)

        CPLString m_osURL{};
        mutable CPLString m_osSecretAccessKey{};
        mutable CPLString m_osAccessKeyId{};
        mutable CPLString m_osSessionToken{};
        CPLString m_osEndpoint{};
        CPLString m_osRegion{};
        CPLString m_osRequestPayer{};
        CPLString m_osBucket{};
        CPLString m_osObjectKey{};
        bool m_bUseHTTPS = false;
        bool m_bUseVirtualHosting = false;
        AWSCredentialsSource m_eCredentialsSource = AWSCredentialsSource::REGULAR;

        void RebuildURL() override;

        static bool GetConfigurationFromEC2(const std::string& osPathForOption,
                                            CPLString& osSecretAccessKey,
                                            CPLString& osAccessKeyId,
                                            CPLString& osSessionToken);

        static bool GetConfigurationFromAWSConfigFiles(
                                     const std::string& osPathForOption,
                                     CPLString& osSecretAccessKey,
                                     CPLString& osAccessKeyId,
                                     CPLString& osSessionToken,
                                     CPLString& osRegion,
                                     CPLString& osCredentials,
                                     CPLString& osRoleArn,
                                     CPLString& osSourceProfile,
                                     CPLString& osExternalId,
                                     CPLString& osMFASerial,
                                     CPLString& osRoleSessionName);

        static bool GetConfiguration(const std::string& osPathForOption,
                                     CSLConstList papszOptions,
                                     CPLString& osSecretAccessKey,
                                     CPLString& osAccessKeyId,
                                     CPLString& osSessionToken,
                                     CPLString& osRegion,
                                     AWSCredentialsSource& eCredentialsSource);
  protected:

    public:
        VSIS3HandleHelper(const CPLString& osSecretAccessKey,
                    const CPLString& osAccessKeyId,
                    const CPLString& osSessionToken,
                    const CPLString& osEndpoint,
                    const CPLString& osRegion,
                    const CPLString& osRequestPayer,
                    const CPLString& osBucket,
                    const CPLString& osObjectKey,
                    bool bUseHTTPS, bool bUseVirtualHosting,
                    AWSCredentialsSource eCredentialsSource);
       ~VSIS3HandleHelper();

        static VSIS3HandleHelper* BuildFromURI(const char* pszURI,
                                               const char* pszFSPrefix,
                                               bool bAllowNoObject,
                                               CSLConstList papszOptions = nullptr);
        static CPLString BuildURL(const CPLString& osEndpoint,
                                  const CPLString& osBucket,
                                  const CPLString& osObjectKey,
                                  bool bUseHTTPS, bool bUseVirtualHosting);

        struct curl_slist* GetCurlHeaders(
            const CPLString& osVerb,
            const struct curl_slist* psExistingHeaders,
            const void *pabyDataContent = nullptr,
            size_t nBytesContent = 0) const override;

        bool AllowAutomaticRedirection() override { return false; }
        bool CanRestartOnError(const char*, const char* pszHeaders,
                               bool bSetError,
                               bool* pbUpdateMap = nullptr) override;

        const CPLString& GetURL() const override { return m_osURL; }
        const CPLString& GetBucket() const { return m_osBucket; }
        const CPLString& GetObjectKey() const { return m_osObjectKey; }
        const CPLString& GetEndpoint()const  { return m_osEndpoint; }
        const CPLString& GetRegion() const { return m_osRegion; }
        const CPLString& GetRequestPayer() const { return m_osRequestPayer; }
        bool GetVirtualHosting() const { return m_bUseVirtualHosting; }
        void SetEndpoint(const CPLString &osStr);
        void SetRegion(const CPLString &osStr);
        void SetRequestPayer(const CPLString &osStr);
        void SetVirtualHosting(bool b);

        CPLString GetCopySourceHeader() const override { return "x-amz-copy-source"; }
        const char* GetMetadataDirectiveREPLACE() const override { return "x-amz-metadata-directive: REPLACE"; }

        CPLString GetSignedURL(CSLConstList papszOptions);

        static void CleanMutex();
        static void ClearCache();
};

class VSIS3UpdateParams
{
    public:
        CPLString m_osRegion{};
        CPLString m_osEndpoint{};
        CPLString m_osRequestPayer{};
        bool m_bUseVirtualHosting = false;

        VSIS3UpdateParams() = default;

        explicit VSIS3UpdateParams(const VSIS3HandleHelper* poHelper) :
            m_osRegion(poHelper->GetRegion()),
            m_osEndpoint(poHelper->GetEndpoint()),
            m_osRequestPayer(poHelper->GetRequestPayer()),
            m_bUseVirtualHosting(poHelper->GetVirtualHosting()) {}

        void UpdateHandlerHelper(VSIS3HandleHelper* poHelper) {
            poHelper->SetRegion(m_osRegion);
            poHelper->SetEndpoint(m_osEndpoint);
            poHelper->SetRequestPayer(m_osRequestPayer);
            poHelper->SetVirtualHosting(m_bUseVirtualHosting);
        }

        static std::mutex gsMutex;
        static std::map< CPLString, VSIS3UpdateParams > goMapBucketsToS3Params;
        static void UpdateMapFromHandle( IVSIS3LikeHandleHelper* poHandleHelper );
        static void UpdateHandleFromMap( IVSIS3LikeHandleHelper* poHandleHelper );
        static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AWS_INCLUDED_H */
