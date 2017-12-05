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

class VSIGSHandleHelper: public IVSIS3LikeHandleHelper
{
        CPLString m_osURL;
        CPLString m_osEndpoint;
        CPLString m_osBucketObjectKey;
        CPLString m_osSecretAccessKey;
        CPLString m_osAccessKeyId;
        bool      m_bUseHeaderFile;
        GOA2Manager m_oManager;

        static bool     GetConfiguration(CPLString& osSecretAccessKey,
                                         CPLString& osAccessKeyId,
                                         CPLString& osHeaderFile,
                                         GOA2Manager& oManager);

        static bool     GetConfigurationFromConfigFile(
                                         CPLString& osSecretAccessKey,
                                         CPLString& osAccessKeyId,
                                         CPLString& osOAuth2RefreshToken,
                                         CPLString& osOAuth2ClientId,
                                         CPLString& osOAuth2ClientSecret,
                                         CPLString& osCredentials);

        virtual void RebuildURL() CPL_OVERRIDE;

    public:
        VSIGSHandleHelper(const CPLString& osEndpoint,
                          const CPLString& osBucketObjectKey,
                          const CPLString& osSecretAccessKey,
                          const CPLString& osAccessKeyId,
                          bool bUseHeaderFile,
                          const GOA2Manager& oManager);
       ~VSIGSHandleHelper();

        static VSIGSHandleHelper* BuildFromURI(const char* pszURI,
                                               const char* pszFSPrefix);

        struct curl_slist* GetCurlHeaders(const CPLString& osVerbosVerb,
                                          const  struct curl_slist* psExistingHeaders,
                                          const void *pabyDataContent = NULL,
                                          size_t nBytesContent = 0) const CPL_OVERRIDE;

        const CPLString& GetURL() const CPL_OVERRIDE { return m_osURL; }

        static void CleanMutex();
        static void ClearCache();
};

#endif /* HAVE_CURL */

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* CPL_GOOGLE_CLOUD_INCLUDED_H */
