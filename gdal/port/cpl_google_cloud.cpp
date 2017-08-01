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

//! @cond Doxygen_Suppress

#include "cpl_google_cloud.h"
#include "cpl_vsi_error.h"
#include "cpl_sha1.h"
#include "cpl_time.h"

CPL_CVSID("$Id$")

#ifdef HAVE_CURL

/************************************************************************/
/*                            GetGSHeaders()                            */
/************************************************************************/

static
struct curl_slist* GetGSHeaders( const CPLString& osVerb,
                                 const CPLString& osCanonicalResource,
                                 const CPLString& osSecretAccessKey,
                                 const CPLString& osAccessKeyId )
{
    if( osSecretAccessKey.empty() )
    {
        VSIError(VSIE_AWSInvalidCredentials,
                 "GS_SECRET_ACCESS_KEY configuration option not defined");
        return NULL;
    }

    if( osAccessKeyId.empty() )
    {
        VSIError(VSIE_AWSInvalidCredentials,
                 "GS_ACCESS_KEY_ID configuration option not defined");
        return NULL;
    }

    CPLString osDate = CPLGetConfigOption("CPL_GS_TIMESTAMP", "");
    if( osDate.empty() )
    {
        char szDate[64];
        time_t nNow = time(NULL);
        struct tm tm;
        CPLUnixTimeToYMDHMS(nNow, &tm);
        strftime(szDate, sizeof(szDate), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        osDate = szDate;
    }

    // See https://cloud.google.com/storage/docs/migrating
    CPLString osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += /* Content-MD5 */ "\n";
    osStringToSign += /* Content-Type */ "\n";
    osStringToSign += osDate + "\n";
    osStringToSign += osCanonicalResource;
#ifdef DEBUG_VERBOSE
    CPLDebug("GS", "osStringToSign = %s", osStringToSign.c_str());
#endif

    GByte abySignature[CPL_SHA1_HASH_SIZE] = {};
    CPL_HMAC_SHA1( osSecretAccessKey.c_str(), osSecretAccessKey.size(),
                   osStringToSign, osStringToSign.size(),
                   abySignature);

    char* pszBase64 = CPLBase64Encode( sizeof(abySignature), abySignature );
    CPLString osAuthorization("GOOG1 ");
    osAuthorization += osAccessKeyId;
    osAuthorization += ":";
    osAuthorization += pszBase64;
    CPLFree(pszBase64);

    struct curl_slist *headers=NULL;
    headers = curl_slist_append(
        headers, CPLSPrintf("Date: %s", osDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    return headers;
}

/************************************************************************/
/*                         VSIGSHandleHelper()                          */
/************************************************************************/
VSIGSHandleHelper::VSIGSHandleHelper( const CPLString& osEndpoint,
                                      const CPLString& osBucketObjectKey,
                                      const CPLString& osSecretAccessKey,
                                      const CPLString& osAccessKeyId,
                                      bool bUseHeaderFile ) :
    m_osURL(osEndpoint + osBucketObjectKey),
    m_osEndpoint(osEndpoint),
    m_osBucketObjectKey(osBucketObjectKey),
    m_osSecretAccessKey(osSecretAccessKey),
    m_osAccessKeyId(osAccessKeyId),
    m_bUseHeaderFile(bUseHeaderFile)
{}

/************************************************************************/
/*                        ~VSIGSHandleHelper()                          */
/************************************************************************/

VSIGSHandleHelper::~VSIGSHandleHelper()
{
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIGSHandleHelper* VSIGSHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* /*pszFSPrefix*/ )
{
    // pszURI == bucket/object
    const CPLString osBucketObject( pszURI );
    const CPLString osEndpoint( CPLGetConfigOption("CPL_GS_ENDPOINT",
                                    "https://storage.googleapis.com/") );
    const CPLString osSecretAccessKey(
        CPLGetConfigOption("GS_SECRET_ACCESS_KEY", ""));
    const CPLString osAccessKeyId(
        CPLGetConfigOption("GS_ACCESS_KEY_ID", ""));
    const CPLString osHeaderFile(
        CPLGetConfigOption("GDAL_HTTP_HEADER_FILE", "") );

    if( osHeaderFile.empty() )
    {
        // coverity[tainted_data]
        struct curl_slist* headers = 
            GetGSHeaders( "GET", "",  osSecretAccessKey, osAccessKeyId );
        if( headers == NULL )
            return NULL;
        curl_slist_free_all(headers);
    }

    return new VSIGSHandleHelper( osEndpoint,
                                  osBucketObject,
                                  osSecretAccessKey,
                                  osAccessKeyId,
                                  !osHeaderFile.empty() );
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIGSHandleHelper::GetCurlHeaders( const CPLString& osVerb ) const
{
    if( m_bUseHeaderFile )
        return NULL;
    return GetGSHeaders( osVerb,
                         "/" + m_osBucketObjectKey,
                         m_osSecretAccessKey,
                         m_osAccessKeyId );
}

#endif

//! @endcond
