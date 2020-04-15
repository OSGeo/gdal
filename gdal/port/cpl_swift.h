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
 * DEALINGS IN THE SOFTWARE.
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

class VSISwiftHandleHelper final: public IVSIS3LikeHandleHelper
{
        CPLString m_osURL;
        CPLString m_osStorageURL;
        CPLString m_osAuthToken;
        CPLString m_osBucket;
        CPLString m_osObjectKey;

        static bool     GetConfiguration(CPLString& osStorageURL,
                                         CPLString& osAuthToken);

        static bool GetCached(const char* pszURLKey,
                              const char* pszUserKey,
                              const char* pszPasswordKey,
                              CPLString& osStorageURL,
                              CPLString& osAuthToken);

        static CPLString BuildURL(const CPLString& osStorageURL,
                                  const CPLString& osBucket,
                                  const CPLString& osObjectKey);

        void RebuildURL() override;

        // V1 Authentication
        static bool CheckCredentialsV1();
        static bool AuthV1(CPLString& osStorageURL,
                           CPLString& osAuthToken);

        // V3 Authentication
        static bool CheckCredentialsV3();
        static bool AuthV3(CPLString& osStorageURL,
                           CPLString& osAuthToken);
        static CPLJSONObject CreateAuthV3RequestObject();
        static bool GetAuthV3StorageURL(const CPLHTTPResult *psResult,
                                        CPLString& storageURL);

    public:
        VSISwiftHandleHelper(const CPLString& osStorageURL,
                             const CPLString& osAuthToken,
                             const CPLString& osBucket,
                             const CPLString& osObjectKey);
       ~VSISwiftHandleHelper();

        bool Authenticate();

        static VSISwiftHandleHelper* BuildFromURI(const char* pszURI,
                                               const char* pszFSPrefix);

        struct curl_slist* GetCurlHeaders(const CPLString& osVerbosVerb,
                                          const struct curl_slist* psExistingHeaders,
                                          const void *pabyDataContent = nullptr,
                                          size_t nBytesContent = 0) const override;

        const CPLString& GetURL() const override { return m_osURL; }

        static void CleanMutex();
        static void ClearCache();
};


#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_SWIFT_INCLUDED_H */
