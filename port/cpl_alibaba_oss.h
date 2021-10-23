/**********************************************************************
 * $Id$
 *
 * Name:     cpl_alibaba_oss.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Alibaba Cloud Object Storage Service
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

#ifndef CPL_ALIBABA_OSS_INCLUDED_H
#define CPL_ALIBABA_OSS_INCLUDED_H

#ifndef DOXYGEN_SKIP

#include <cstddef>

#include "cpl_string.h"

#ifdef HAVE_CURL

#include <curl/curl.h>
#include <map>
#include "cpl_aws.h"

class VSIOSSHandleHelper final: public IVSIS3LikeHandleHelper
{
        CPL_DISALLOW_COPY_ASSIGN(VSIOSSHandleHelper)

        CPLString m_osURL{};
        CPLString m_osSecretAccessKey{};
        CPLString m_osAccessKeyId{};
        CPLString m_osEndpoint{};
        CPLString m_osBucket{};
        CPLString m_osObjectKey{};
        bool m_bUseHTTPS = false;
        bool m_bUseVirtualHosting = false;

        void RebuildURL() override;

        static bool GetConfiguration(CSLConstList papszOptions,
                                     CPLString& osSecretAccessKey,
                                     CPLString& osAccessKeyId);

  protected:

    public:
        VSIOSSHandleHelper(const CPLString& osSecretAccessKey,
                    const CPLString& osAccessKeyId,
                    const CPLString& osEndpoint,
                    const CPLString& osBucket,
                    const CPLString& osObjectKey,
                    bool bUseHTTPS, bool bUseVirtualHosting);
       ~VSIOSSHandleHelper();

        static VSIOSSHandleHelper* BuildFromURI(const char* pszURI,
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
            size_t nBytesContent = 0 ) const override;

        bool CanRestartOnError(const char*, const char* pszHeaders,
                               bool bSetError,
                               bool* pbUpdateMap = nullptr) override;

        const CPLString& GetURL() const override { return m_osURL; }
        const CPLString& GetBucket() const { return m_osBucket; }
        const CPLString& GetObjectKey() const { return m_osObjectKey; }
        const CPLString& GetEndpoint()const  { return m_osEndpoint; }
        bool GetVirtualHosting() const { return m_bUseVirtualHosting; }

        CPLString GetCopySourceHeader() const override { return "x-oss-copy-source"; }

        void SetEndpoint(const CPLString &osStr);
        void SetVirtualHosting(bool b);

        CPLString GetSignedURL(CSLConstList papszOptions);
};

class VSIOSSUpdateParams
{
    public:
        CPLString m_osEndpoint{};

        VSIOSSUpdateParams() = default;

        explicit VSIOSSUpdateParams(const VSIOSSHandleHelper* poHelper) :
            m_osEndpoint(poHelper->GetEndpoint()) {}

        void UpdateHandlerHelper(VSIOSSHandleHelper* poHelper) {
            poHelper->SetEndpoint(m_osEndpoint);
        }
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_ALIBABA_OSS_INCLUDED_H */
