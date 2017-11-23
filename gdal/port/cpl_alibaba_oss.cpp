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

#include "cpl_alibaba_oss.h"
#include "cpl_vsi_error.h"
#include "cpl_time.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_http.h"
#include "cpl_sha1.h"
#include <algorithm>

CPL_CVSID("$Id$")

// #define DEBUG_VERBOSE 1

#ifdef HAVE_CURL

/************************************************************************/
/*                         CPLGetOSSHeaders()                           */
/************************************************************************/

// See:
// https://www.alibabacloud.com/help/doc-detail/31951.htm?spm=a3c0i.o31982en.b99.178.5HUTqV
static struct curl_slist*
CPLGetOSSHeaders( const CPLString& osSecretAccessKey,
                  const CPLString& osAccessKeyId,
                  const CPLString& osVerb,
                  const struct curl_slist* psExistingHeaders,
                  const CPLString& osCanonicalizedResource )
{
    CPLString osDate = CPLGetConfigOption("CPL_OSS_TIMESTAMP", "");
    if( osDate.empty() )
    {
        char szDate[64];
        time_t nNow = time(NULL);
        struct tm tm;
        CPLUnixTimeToYMDHMS(nNow, &tm);
        strftime(szDate, sizeof(szDate), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        osDate = szDate;
    }

    std::map<CPLString, CPLString> oSortedMapHeaders;
    CPLString osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
                            oSortedMapHeaders,
                            psExistingHeaders,
                            "x-oss-"));

    CPLString osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += osDate + "\n";
    osStringToSign += osCanonicalizedHeaders;
    osStringToSign += osCanonicalizedResource;
#ifdef DEBUG_VERBOSE
    CPLDebug("OSS", "osStringToSign = %s", osStringToSign.c_str());
#endif

    GByte abySignature[CPL_SHA1_HASH_SIZE] = {};
    CPL_HMAC_SHA1( osSecretAccessKey.c_str(), osSecretAccessKey.size(),
                   osStringToSign, osStringToSign.size(),
                   abySignature);

/* -------------------------------------------------------------------- */
/*      Build authorization header.                                     */
/* -------------------------------------------------------------------- */

    char* pszBase64 = CPLBase64Encode( sizeof(abySignature), abySignature );
    CPLString osAuthorization("OSS ");
    osAuthorization += osAccessKeyId;
    osAuthorization += ":";
    osAuthorization += pszBase64;
    CPLFree(pszBase64);

#ifdef DEBUG_VERBOSE
    CPLDebug("OSS", "osAuthorization='%s'", osAuthorization.c_str());
#endif

    struct curl_slist *headers=NULL;
    headers = curl_slist_append(
        headers, CPLSPrintf("Date: %s", osDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    return headers;
}

/************************************************************************/
/*                         VSIOSSHandleHelper()                         */
/************************************************************************/
VSIOSSHandleHelper::VSIOSSHandleHelper( const CPLString& osSecretAccessKey,
                                      const CPLString& osAccessKeyId,
                                      const CPLString& osEndpoint,
                                      const CPLString& osBucket,
                                      const CPLString& osObjectKey,
                                      bool bUseHTTPS,
                                      bool bUseVirtualHosting ) :
    m_osURL(BuildURL(osEndpoint, osBucket, osObjectKey, bUseHTTPS,
                     bUseVirtualHosting)),
    m_osSecretAccessKey(osSecretAccessKey),
    m_osAccessKeyId(osAccessKeyId),
    m_osEndpoint(osEndpoint),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey),
    m_bUseHTTPS(bUseHTTPS),
    m_bUseVirtualHosting(bUseVirtualHosting)
{}

/************************************************************************/
/*                        ~VSIOSSHandleHelper()                         */
/************************************************************************/

VSIOSSHandleHelper::~VSIOSSHandleHelper()
{
    for( size_t i = 0; i < m_osSecretAccessKey.size(); i++ )
        m_osSecretAccessKey[i] = 0;
}

/************************************************************************/
/*                           BuildURL()                                 */
/************************************************************************/

CPLString VSIOSSHandleHelper::BuildURL(const CPLString& osEndpoint,
                                       const CPLString& osBucket,
                                       const CPLString& osObjectKey,
                                       bool bUseHTTPS, bool bUseVirtualHosting)
{
    const char* pszProtocol = (bUseHTTPS) ? "https" : "http";
    if( osBucket.empty()  )
    {
        return CPLSPrintf("%s://%s", pszProtocol,
                          osEndpoint.c_str());
    }
    else if( bUseVirtualHosting )
        return CPLSPrintf("%s://%s.%s/%s", pszProtocol,
                                        osBucket.c_str(),
                                        osEndpoint.c_str(),
                                        CPLAWSURLEncode(osObjectKey, false).c_str());
    else
        return CPLSPrintf("%s://%s/%s/%s", pszProtocol,
                                        osEndpoint.c_str(),
                                        osBucket.c_str(),
                                        CPLAWSURLEncode(osObjectKey, false).c_str());
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIOSSHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint, m_osBucket, m_osObjectKey,
                       m_bUseHTTPS, m_bUseVirtualHosting);
    m_osURL += GetQueryString();
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIOSSHandleHelper::GetConfiguration(CPLString& osSecretAccessKey,
                                          CPLString& osAccessKeyId)
{
    osSecretAccessKey =
        CPLGetConfigOption("OSS_SECRET_ACCESS_KEY", "");

    if( !osSecretAccessKey.empty() )
    {
        osAccessKeyId = CPLGetConfigOption("OSS_ACCESS_KEY_ID", "");
        if( osAccessKeyId.empty() )
        {
            VSIError(VSIE_AWSInvalidCredentials,
                    "OSS_ACCESS_KEY_ID configuration option not defined");
            return false;
        }

        return true;
    }

    VSIError(VSIE_AWSInvalidCredentials,
                "OSS_SECRET_ACCESS_KEY configuration option not defined");
    return false;
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIOSSHandleHelper* VSIOSSHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* pszFSPrefix,
                                                    bool bAllowNoObject )
{
    CPLString osSecretAccessKey;
    CPLString osAccessKeyId;
    if( !GetConfiguration(osSecretAccessKey, osAccessKeyId) )
    {
        return NULL;
    }

    const CPLString osEndpoint =
        CPLGetConfigOption("OSS_ENDPOINT", "oss-us-east-1.aliyuncs.com");
    CPLString osBucket;
    CPLString osObjectKey;
    if( pszURI != NULL && pszURI[0] != '\0' &&
        !GetBucketAndObjectKey(pszURI, pszFSPrefix, bAllowNoObject,
                               osBucket, osObjectKey) )
    {
        return NULL;
    }
    const bool bUseHTTPS = CPLTestBool(CPLGetConfigOption("OSS_HTTPS", "YES"));
    const bool bIsValidNameForVirtualHosting =
        osBucket.find('.') == std::string::npos;
    const bool bUseVirtualHosting = CPLTestBool(
        CPLGetConfigOption("OSS_VIRTUAL_HOSTING",
                           bIsValidNameForVirtualHosting ? "TRUE" : "FALSE"));
    return new VSIOSSHandleHelper(osSecretAccessKey, osAccessKeyId,
                                 osEndpoint,
                                 osBucket, osObjectKey, bUseHTTPS,
                                 bUseVirtualHosting);
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIOSSHandleHelper::GetCurlHeaders( const CPLString& osVerb,
                                   const struct curl_slist* psExistingHeaders,
                                   const void * /*pabyDataContent*/,
                                   size_t /*nBytesContent*/ ) const
{
    CPLString osCanonicalQueryString;
    if( !m_osObjectKey.empty() )
    {
        osCanonicalQueryString = GetQueryString();
    }

    CPLString osCanonicalizedResource( m_osBucket.empty() ? CPLString("/") :
        "/" + m_osBucket +  "/" + CPLAWSURLEncode(m_osObjectKey, false));
    osCanonicalizedResource += osCanonicalQueryString;

    return CPLGetOSSHeaders(
        m_osSecretAccessKey,
        m_osAccessKeyId,
        osVerb,
        psExistingHeaders,
        osCanonicalizedResource);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIOSSHandleHelper::CanRestartOnError( const char* pszErrorMsg,
                                            const char*,
                                            bool bSetError,
                                            bool* pbUpdateMap )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("OSS", "%s", pszErrorMsg);
#endif

    if( pbUpdateMap != NULL )
        *pbUpdateMap = true;

    if( !STARTS_WITH(pszErrorMsg, "<?xml") )
    {
        if( bSetError )
        {
            VSIError(VSIE_AWSError, "Invalid AWS response: %s", pszErrorMsg);
        }
        return false;
    }

    CPLXMLNode* psTree = CPLParseXMLString(pszErrorMsg);
    if( psTree == NULL )
    {
        if( bSetError )
        {
            VSIError(VSIE_AWSError,
                     "Malformed AWS XML response: %s", pszErrorMsg);
        }
        return false;
    }

    const char* pszCode = CPLGetXMLValue(psTree, "=Error.Code", NULL);
    if( pszCode == NULL )
    {
        CPLDestroyXMLNode(psTree);
        if( bSetError )
        {
            VSIError(VSIE_AWSError,
                     "Malformed AWS XML response: %s", pszErrorMsg);
        }
        return false;
    }

    if( EQUAL(pszCode, "AccessDenied") )
    {
        const char* pszEndpoint =
            CPLGetXMLValue(psTree, "=Error.Endpoint", NULL);
        if( pszEndpoint && pszEndpoint != m_osEndpoint )
        {
            SetEndpoint(pszEndpoint);
            CPLDebug("OSS", "Switching to endpoint %s", m_osEndpoint.c_str());
            CPLDestroyXMLNode(psTree);
            return true;
        }
    }

    if( bSetError )
    {
        // Translate AWS errors into VSI errors.
        const char* pszMessage = CPLGetXMLValue(psTree, "=Error.Message", NULL);

        if( pszMessage == NULL ) {
            VSIError(VSIE_AWSError, "%s", pszErrorMsg);
        } else if( EQUAL(pszCode, "AccessDenied") ) {
            VSIError(VSIE_AWSAccessDenied, "%s", pszMessage);
        } else if( EQUAL(pszCode, "NoSuchBucket") ) {
            VSIError(VSIE_AWSBucketNotFound, "%s", pszMessage);
        } else if( EQUAL(pszCode, "NoSuchKey") ) {
            VSIError(VSIE_AWSObjectNotFound, "%s", pszMessage);
        } else if( EQUAL(pszCode, "SignatureDoesNotMatch") ) {
            VSIError(VSIE_AWSSignatureDoesNotMatch, "%s", pszMessage);
        } else {
            VSIError(VSIE_AWSError, "%s", pszMessage);
        }
    }

    CPLDestroyXMLNode(psTree);

    return false;
}

/************************************************************************/
/*                            SetEndpoint()                             */
/************************************************************************/

void VSIOSSHandleHelper::SetEndpoint( const CPLString &osStr )
{
    m_osEndpoint = osStr;
    RebuildURL();
}

#endif // HAVE_CURL

//! @endcond
