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

class VSIAzureBlobHandleHelper final: public IVSIS3LikeHandleHelper
{
        CPLString m_osURL;
        CPLString m_osEndpoint;
        CPLString m_osBlobEndpoint;
        CPLString m_osBucket;
        CPLString m_osObjectKey;
        CPLString m_osStorageAccount;
        CPLString m_osStorageKey;
        bool      m_bUseHTTPS;

        static bool     GetConfiguration(CSLConstList papszOptions,
                                         bool& bUseHTTPS,
                                         CPLString& osEndpoint,
                                         CPLString& osBlobEndpoint,
                                         CPLString& osStorageAccount,
                                         CPLString& osStorageKey);

        static CPLString BuildURL(const CPLString& osEndpoint,
                                  const CPLString& osBlobEndpoint,
                                  const CPLString& osStorageAccount,
                                  const CPLString& osBucket,
                                  const CPLString& osObjectKey,
                                  bool bUseHTTPS);

        void RebuildURL() override;

    public:
        VSIAzureBlobHandleHelper(const CPLString& osEndpoint,
                                 const CPLString& osBlobEndpoint,
                                 const CPLString& osBucket,
                                 const CPLString& osObjectKey,
                                 const CPLString& osStorageAccount,
                                 const CPLString& osStorageKey,
                                 bool bUseHTTPS);
       ~VSIAzureBlobHandleHelper();

        static VSIAzureBlobHandleHelper* BuildFromURI(const char* pszURI,
                                                      const char* pszFSPrefix,
                                                      CSLConstList papszOptions = nullptr);

        struct curl_slist* GetCurlHeaders(const CPLString& osVerbosVerb,
                                          const struct curl_slist* psExistingHeaders,
                                          const void *pabyDataContent = nullptr,
                                          size_t nBytesContent = 0) const override;

        const CPLString& GetURL() const override { return m_osURL; }

        CPLString GetSignedURL(CSLConstList papszOptions);
};


#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_AZURE_INCLUDED_H */
