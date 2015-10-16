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

#ifndef _CPL_AWS_INCLUDED_H
#define _CPL_AWS_INCLUDED_H

#include "cpl_string.h"

CPLString CPLGetAWS_SIGN4_Authorization(const CPLString& osSecretAccessKey,
                                        const CPLString& osAccessKeyId,
                                        const CPLString& osAWSRegion,
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

#ifdef HAVE_CURL

#include <curl/curl.h>

class VSIS3HandleHelper
{
        CPLString osURL;
        CPLString osSecretAccessKey;
        CPLString osAccessKeyId;
        CPLString osAWSS3Endpoint;
        CPLString osAWSRegion;
        CPLString osBucket;
        CPLString osObjectKey;
        bool bUseHTTPS;
        bool bUseVirtualHosting;

  protected:

    public:
        VSIS3HandleHelper(const CPLString& osSecretAccessKey,
                    const CPLString& osAccessKeyId,
                    const CPLString& osAWSS3Endpoint,
                    const CPLString& osAWSRegion,
                    const CPLString& osBucket,
                    const CPLString& osObjectKey,
                    bool bUseHTTPS, bool bUseVirtualHosting);
       ~VSIS3HandleHelper();

        static VSIS3HandleHelper* BuildFromURI(const char* pszURI, const char* pszFSPrefix);
        static CPLString BuildURL(const CPLString& osAWSS3Endpoint,
                                  const CPLString& osBucket,
                                  const CPLString& osObjectKey,
                                  bool bUseHTTPS, bool bUseVirtualHosting);

        struct curl_slist* GetCurlHeaders(const CPLString& osVerb);
        bool CanRestartOnError(const char*);
        const char* GetURL() { return osURL.c_str(); }
};

#endif /* HAVE_CURL */

#endif /* _CPL_AWS_INCLUDED_H */
