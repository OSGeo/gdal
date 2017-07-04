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

#include <cstddef>

#include "cpl_string.h"

CPLString CPLGetAWS_SIGN4_Authorization(const CPLString& osSecretAccessKey,
                                        const CPLString& osAccessKeyId,
                                        const CPLString& osAccessToken,
                                        const CPLString& osAWSRegion,
                                        const CPLString& osRequestPayer,
                                        const CPLString& osService,
                                        const CPLString& osVerb,
                                        const CPLString& osHost,
                                        const CPLString& osCanonicalURI,
                                        const CPLString& osCanonicalQueryString,
                                        const CPLString& osXAMZContentSHA256,
                                        const CPLString& osTimestamp);

CPLString CPLGetLowerCaseHexSHA256( const void *pabyData, size_t nBytes );
CPLString CPLGetLowerCaseHexSHA256( const CPLString& osStr );

CPLString CPLGetAWS_SIGN4_Timestamp();

CPLString CPLAWSURLEncode(const CPLString& osURL, bool bEncodeSlash = true);

#ifdef HAVE_CURL

#include <curl/curl.h>
#include <map>

class VSIS3HandleHelper
{
        CPLString m_osURL;
        CPLString m_osSecretAccessKey;
        CPLString m_osAccessKeyId;
        CPLString m_osSessionToken;
        CPLString m_osAWSS3Endpoint;
        CPLString m_osAWSRegion;
        CPLString m_osRequestPayer;
        CPLString m_osBucket;
        CPLString m_osObjectKey;
        bool m_bUseHTTPS;
        bool m_bUseVirtualHosting;
        std::map<CPLString, CPLString> m_oMapQueryParameters;

        static bool GetBucketAndObjectKey(const char* pszURI, const char* pszFSPrefix,
                                          bool bAllowNoObject,
                                          CPLString &osBucketOut, CPLString &osObjectKeyOut);
        void RebuildURL();

        static bool GetConfigurationFromEC2(CPLString& osSecretAccessKey,
                                            CPLString& osAccessKeyId,
                                            CPLString& osSessionToken);

        static bool GetConfigurationFromAWSConfigFiles(
                                     CPLString& osSecretAccessKey,
                                     CPLString& osAccessKeyId,
                                     CPLString& osSessionToken,
                                     CPLString& osRegion,
                                     CPLString& osCredentials);
  protected:

    public:
        VSIS3HandleHelper(const CPLString& osSecretAccessKey,
                    const CPLString& osAccessKeyId,
                    const CPLString& osSessionToken,
                    const CPLString& osAWSS3Endpoint,
                    const CPLString& osAWSRegion,
                    const CPLString& osRequestPayer,
                    const CPLString& osBucket,
                    const CPLString& osObjectKey,
                    bool bUseHTTPS, bool bUseVirtualHosting);
       ~VSIS3HandleHelper();

        static VSIS3HandleHelper* BuildFromURI(const char* pszURI, const char* pszFSPrefix,
                                               bool bAllowNoObject);
        static CPLString BuildURL(const CPLString& osAWSS3Endpoint,
                                  const CPLString& osBucket,
                                  const CPLString& osObjectKey,
                                  bool bUseHTTPS, bool bUseVirtualHosting);

        void ResetQueryParameters();
        void AddQueryParameter(const CPLString& osKey, const CPLString& osValue);
        struct curl_slist* GetCurlHeaders(const CPLString& osVerb,
                                          const void *pabyDataContent = NULL,
                                          size_t nBytesContent = 0);
        bool CanRestartOnError(const char* pszErrorMsg) { return CanRestartOnError(pszErrorMsg, false); }
        bool CanRestartOnError(const char*, bool bSetError);

        const CPLString& GetURL() const { return m_osURL; }
        const CPLString& GetBucket() const { return m_osBucket; }
        const CPLString& GetObjectKey() const { return m_osObjectKey; }
        const CPLString& GetAWSS3Endpoint()const  { return m_osAWSS3Endpoint; }
        const CPLString& GetAWSRegion() const { return m_osAWSRegion; }
        const CPLString& GetRequestPayer() const { return m_osRequestPayer; }
        bool GetVirtualHosting() const { return m_bUseVirtualHosting; }
        void SetAWSS3Endpoint(const CPLString &osStr);
        void SetAWSRegion(const CPLString &osStr);
        void SetRequestPayer(const CPLString &osStr);
        void SetVirtualHosting(bool b);
        void SetObjectKey(const CPLString &osStr);

        static bool GetConfiguration(CPLString& osSecretAccessKey,
                                     CPLString& osAccessKeyId,
                                     CPLString& osSessionToken,
                                     CPLString& osRegion);
        static void CleanMutex();
        static void ClearCache();
};

class VSIS3UpdateParams
{
    public:
        CPLString m_osAWSRegion;
        CPLString m_osAWSS3Endpoint;
        CPLString m_osRequestPayer;
        bool m_bUseVirtualHosting;

        VSIS3UpdateParams(const CPLString& osAWSRegion = "",
                          const CPLString& osAWSS3Endpoint = "",
                          const CPLString& osRequestPayer = "",
                          bool bUseVirtualHosting = false) :
            m_osAWSRegion(osAWSRegion),
            m_osAWSS3Endpoint(osAWSS3Endpoint),
            m_osRequestPayer(osRequestPayer),
            m_bUseVirtualHosting(bUseVirtualHosting) {}
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AWS_INCLUDED_H */
