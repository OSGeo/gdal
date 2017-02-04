/**********************************************************************
 *
 * Name:     cpl_aws.cpp
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

//! @cond Doxygen_Suppress

#include "cpl_aws.h"
#include "cpl_vsi_error.h"
#include "cpl_sha256.h"
#include "cpl_time.h"
#include "cpl_minixml.h"
#include <algorithm>

CPL_CVSID("$Id$");

// #define DEBUG_VERBOSE 1

/************************************************************************/
/*                         CPLGetLowerCaseHex()                         */
/************************************************************************/

static CPLString CPLGetLowerCaseHex( const GByte *pabyData, size_t nBytes )

{
    CPLString osRet;
    osRet.resize(nBytes * 2);

    static const char achHex[] = "0123456789abcdef";

    for( size_t i = 0; i < nBytes; ++i )
    {
        int nLow = pabyData[i] & 0x0f;
        int nHigh = (pabyData[i] & 0xf0) >> 4;

        osRet[i*2] = achHex[nHigh];
        osRet[i*2+1] = achHex[nLow];
    }

    return osRet;
}

/************************************************************************/
/*                       CPLGetLowerCaseHexSHA256()                     */
/************************************************************************/

CPLString CPLGetLowerCaseHexSHA256( const void *pabyData, size_t nBytes )
{
    GByte hash[CPL_SHA256_HASH_SIZE] = {};
    CPL_SHA256(static_cast<const GByte *>(pabyData), nBytes, hash);
    return CPLGetLowerCaseHex(hash, CPL_SHA256_HASH_SIZE);
}

/************************************************************************/
/*                       CPLGetLowerCaseHexSHA256()                     */
/************************************************************************/

CPLString CPLGetLowerCaseHexSHA256( const CPLString& osStr )
{
    return CPLGetLowerCaseHexSHA256(osStr.c_str(), osStr.size());
}

/************************************************************************/
/*                       CPLAWSURLEncode()                              */
/************************************************************************/

CPLString CPLAWSURLEncode( const CPLString& osURL, bool bEncodeSlash )
{
    CPLString osRet;
    for( size_t i = 0; i < osURL.size(); i++ )
    {
        char ch = osURL[i];
        if( (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '~' || ch == '.' )
        {
            osRet += ch;
        }
        else if( ch == '/' )
        {
            if( bEncodeSlash )
                osRet += "%2F";
            else
                osRet += ch;
        }
        else
        {
            osRet += CPLSPrintf("%02X", ch);
        }
    }
    return osRet;
}

/************************************************************************/
/*                CPLGetAWS_SIGN4_Authorization()                       */
/************************************************************************/

// See:
// http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
CPLString
CPLGetAWS_SIGN4_Authorization( const CPLString& osSecretAccessKey,
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
                               const CPLString& osTimestamp )
{
/* -------------------------------------------------------------------- */
/*      Compute canonical request string.                               */
/* -------------------------------------------------------------------- */
    CPLString osCanonicalRequest = osVerb + "\n";

    osCanonicalRequest += osCanonicalURI + "\n";

    osCanonicalRequest += osCanonicalQueryString + "\n";

    CPLString osCanonicalHeaders =
        "host:" + osHost + "\n" +
        "x-amz-content-sha256:" + osXAMZContentSHA256 + "\n" +
        "x-amz-date:" + osTimestamp + "\n";
    if( !osRequestPayer.empty() )
    {
        osCanonicalHeaders += "x-amz-request-payer:";
        osCanonicalHeaders += osRequestPayer;
        osCanonicalHeaders += "\n";
    }
    if( !osAccessToken.empty() )
    {
        osCanonicalHeaders += "x-amz-security-token:";
        osCanonicalHeaders += osAccessToken;
        osCanonicalHeaders += "\n";
    }

    osCanonicalRequest += osCanonicalHeaders + "\n";

    CPLString osSignedHeaders = "host;x-amz-content-sha256;x-amz-date";
    if( !osRequestPayer.empty() )
        osSignedHeaders += ";x-amz-request-payer";
    if( !osAccessToken.empty() )
        osSignedHeaders += ";x-amz-security-token";
    osCanonicalRequest += osSignedHeaders + "\n";

    osCanonicalRequest += osXAMZContentSHA256;

#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "osCanonicalRequest='%s'", osCanonicalRequest.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Compute StringToSign .                                          */
/* -------------------------------------------------------------------- */
    CPLString osStringToSign = "AWS4-HMAC-SHA256\n";
    osStringToSign += osTimestamp + "\n";

    CPLString osYYMMDD(osTimestamp);
    osYYMMDD.resize(8);

    CPLString osScope = osYYMMDD + "/";
    osScope += osAWSRegion;
    osScope += "/";
    osScope += osService;
    osScope += "/aws4_request";
    osStringToSign += osScope + "\n";
    osStringToSign += CPLGetLowerCaseHexSHA256(osCanonicalRequest);

#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "osStringToSign='%s'", osStringToSign.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Compute signing key.                                            */
/* -------------------------------------------------------------------- */
    GByte abySigningKeyIn[CPL_SHA256_HASH_SIZE] = {};
    GByte abySigningKeyOut[CPL_SHA256_HASH_SIZE] = {};

    CPLString osFirstKey(CPLString("AWS4") + osSecretAccessKey);
    CPL_HMAC_SHA256( osFirstKey.c_str(), osFirstKey.size(),
                     osYYMMDD, osYYMMDD.size(),
                     abySigningKeyOut );
    memcpy(abySigningKeyIn, abySigningKeyOut, CPL_SHA256_HASH_SIZE);

    CPL_HMAC_SHA256( abySigningKeyIn, CPL_SHA256_HASH_SIZE,
                     osAWSRegion.c_str(), osAWSRegion.size(),
                     abySigningKeyOut );
    memcpy(abySigningKeyIn, abySigningKeyOut, CPL_SHA256_HASH_SIZE);

    CPL_HMAC_SHA256( abySigningKeyIn, CPL_SHA256_HASH_SIZE,
                     osService.c_str(), osService.size(),
                     abySigningKeyOut );
    memcpy(abySigningKeyIn, abySigningKeyOut, CPL_SHA256_HASH_SIZE);

    CPL_HMAC_SHA256( abySigningKeyIn, CPL_SHA256_HASH_SIZE,
                     "aws4_request", strlen("aws4_request"),
                     abySigningKeyOut );
    memcpy(abySigningKeyIn, abySigningKeyOut, CPL_SHA256_HASH_SIZE);

#ifdef DEBUG_VERBOSE
    CPLString osSigningKey(CPLGetLowerCaseHex(abySigningKeyIn,
                                              CPL_SHA256_HASH_SIZE));
    CPLDebug("S3", "osSigningKey='%s'", osSigningKey.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Compute signature.                                              */
/* -------------------------------------------------------------------- */
    GByte abySignature[CPL_SHA256_HASH_SIZE] = {};
    CPL_HMAC_SHA256( abySigningKeyIn, CPL_SHA256_HASH_SIZE,
                     osStringToSign, osStringToSign.size(),
                     abySignature);
    CPLString osSignature(CPLGetLowerCaseHex(abySignature,
                                             CPL_SHA256_HASH_SIZE));

#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "osSignature='%s'", osSignature.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Build authorization header.                                     */
/* -------------------------------------------------------------------- */
    CPLString osAuthorization;
    osAuthorization = "AWS4-HMAC-SHA256 Credential=";
    osAuthorization += osAccessKeyId;
    osAuthorization += "/";
    osAuthorization += osYYMMDD;
    osAuthorization += "/";
    osAuthorization += osAWSRegion;
    osAuthorization += "/";
    osAuthorization += osService;
    osAuthorization += "/";
    osAuthorization += "aws4_request";
    osAuthorization += ",";
    osAuthorization += "SignedHeaders=";
    osAuthorization += osSignedHeaders;
    osAuthorization += ",";
    osAuthorization += "Signature=";
    osAuthorization += osSignature;

#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "osAuthorization='%s'", osAuthorization.c_str());
#endif

    return osAuthorization;
}

/************************************************************************/
/*                        CPLGetAWS_SIGN4_Timestamp()                   */
/************************************************************************/

CPLString CPLGetAWS_SIGN4_Timestamp()
{
    struct tm brokenDown;
    CPLUnixTimeToYMDHMS(time(NULL), &brokenDown);

    char szTimeStamp[4+2+2+1+2+2+2+1+1] = {};
    snprintf(szTimeStamp, sizeof(szTimeStamp), "%04d%02d%02dT%02d%02d%02dZ",
            brokenDown.tm_year + 1900,
            brokenDown.tm_mon + 1,
            brokenDown.tm_mday,
            brokenDown.tm_hour,
            brokenDown.tm_min,
            brokenDown.tm_sec);
    return szTimeStamp;
}

#ifdef HAVE_CURL

/************************************************************************/
/*                         VSIS3HandleHelper()                          */
/************************************************************************/
VSIS3HandleHelper::VSIS3HandleHelper( const CPLString& osSecretAccessKey,
                                      const CPLString& osAccessKeyId,
                                      const CPLString& osSessionToken,
                                      const CPLString& osAWSS3Endpoint,
                                      const CPLString& osAWSRegion,
                                      const CPLString& osRequestPayer,
                                      const CPLString& osBucket,
                                      const CPLString& osObjectKey,
                                      bool bUseHTTPS,
                                      bool bUseVirtualHosting ) :
    m_osURL(BuildURL(osAWSS3Endpoint, osBucket, osObjectKey, bUseHTTPS,
                     bUseVirtualHosting)),
    m_osSecretAccessKey(osSecretAccessKey),
    m_osAccessKeyId(osAccessKeyId),
    m_osSessionToken(osSessionToken),
    m_osAWSS3Endpoint(osAWSS3Endpoint),
    m_osAWSRegion(osAWSRegion),
    m_osRequestPayer(osRequestPayer),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey),
    m_bUseHTTPS(bUseHTTPS),
    m_bUseVirtualHosting(bUseVirtualHosting)
{}

/************************************************************************/
/*                        ~VSIS3HandleHelper()                          */
/************************************************************************/

VSIS3HandleHelper::~VSIS3HandleHelper()
{
    for( size_t i = 0; i < m_osSecretAccessKey.size(); i++ )
        m_osSecretAccessKey[i] = 0;
}

/************************************************************************/
/*                           BuildURL()                                 */
/************************************************************************/

CPLString VSIS3HandleHelper::BuildURL(const CPLString& osAWSS3Endpoint,
                                      const CPLString& osBucket,
                                      const CPLString& osObjectKey,
                                      bool bUseHTTPS, bool bUseVirtualHosting)
{
    if( bUseVirtualHosting )
        return CPLSPrintf("%s://%s.%s/%s", (bUseHTTPS) ? "https" : "http",
                                        osBucket.c_str(),
                                        osAWSS3Endpoint.c_str(),
                                        osObjectKey.c_str());
    else
        return CPLSPrintf("%s://%s/%s/%s", (bUseHTTPS) ? "https" : "http",
                                        osAWSS3Endpoint.c_str(),
                                        osBucket.c_str(),
                                        osObjectKey.c_str());
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIS3HandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osAWSS3Endpoint, m_osBucket, m_osObjectKey,
                       m_bUseHTTPS, m_bUseVirtualHosting);
    std::map<CPLString, CPLString>::iterator oIter =
        m_oMapQueryParameters.begin();
    for( ; oIter != m_oMapQueryParameters.end(); ++oIter )
    {
        if( oIter == m_oMapQueryParameters.begin() )
            m_osURL += "?";
        else
            m_osURL += "&";
        m_osURL += oIter->first;
        if( !oIter->second.empty() )
        {
            m_osURL += "=";
            m_osURL += oIter->second;
        }
    }
}

/************************************************************************/
/*                        GetBucketAndObjectKey()                       */
/************************************************************************/

bool VSIS3HandleHelper::GetBucketAndObjectKey( const char* pszURI,
                                               const char* pszFSPrefix,
                                               bool bAllowNoObject,
                                               CPLString &osBucket,
                                               CPLString &osObjectKey )
{
    osBucket = pszURI;
    if( osBucket.empty() )
    {
        return false;
    }
    size_t nPos = osBucket.find('/');
    if( nPos == std::string::npos )
    {
        if( bAllowNoObject )
        {
            osObjectKey = "";
            return true;
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Filename should be of the form %sbucket/key", pszFSPrefix);
        return false;
    }
    osBucket.resize(nPos);
    osObjectKey = pszURI + nPos + 1;
    return true;
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIS3HandleHelper* VSIS3HandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* pszFSPrefix,
                                                    bool bAllowNoObject )
{
    const CPLString osSecretAccessKey =
        CPLGetConfigOption("AWS_SECRET_ACCESS_KEY", "");
    if( osSecretAccessKey.empty() )
    {
        VSIError(VSIE_AWSInvalidCredentials,
                 "AWS_SECRET_ACCESS_KEY configuration option not defined");
        return NULL;
    }
    const CPLString osAccessKeyId = CPLGetConfigOption("AWS_ACCESS_KEY_ID", "");
    if( osAccessKeyId.empty() )
    {
        VSIError(VSIE_AWSInvalidCredentials,
                 "AWS_ACCESS_KEY_ID configuration option not defined");
        return NULL;
    }
    const CPLString osSessionToken =
        CPLGetConfigOption("AWS_SESSION_TOKEN", "");
    const CPLString osAWSS3Endpoint =
        CPLGetConfigOption("AWS_S3_ENDPOINT", "s3.amazonaws.com");
    const CPLString osAWSRegion = CPLGetConfigOption("AWS_REGION", "us-east-1");
    const CPLString osRequestPayer =
        CPLGetConfigOption("AWS_REQUEST_PAYER", "");
    CPLString osBucket;
    CPLString osObjectKey;
    if( !GetBucketAndObjectKey(pszURI, pszFSPrefix, bAllowNoObject,
                               osBucket, osObjectKey) )
    {
        return NULL;
    }
    const bool bUseHTTPS = CPLTestBool(CPLGetConfigOption("AWS_HTTPS", "YES"));
    const bool bIsValidNameForVirtualHosting =
        osBucket.find('.') == std::string::npos;
    const bool bUseVirtualHosting = CPLTestBool(
        CPLGetConfigOption("AWS_VIRTUAL_HOSTING",
                           bIsValidNameForVirtualHosting ? "TRUE" : "FALSE"));
    return new VSIS3HandleHelper(osSecretAccessKey, osAccessKeyId,
                                 osSessionToken,
                                 osAWSS3Endpoint, osAWSRegion,
                                 osRequestPayer,
                                 osBucket, osObjectKey, bUseHTTPS,
                                 bUseVirtualHosting);
}

/************************************************************************/
/*                       ResetQueryParameters()                         */
/************************************************************************/

void VSIS3HandleHelper::ResetQueryParameters()
{
    m_oMapQueryParameters.clear();
    RebuildURL();
}

/************************************************************************/
/*                         AddQueryParameter()                          */
/************************************************************************/

void VSIS3HandleHelper::AddQueryParameter( const CPLString& osKey,
                                           const CPLString& osValue )
{
    m_oMapQueryParameters[osKey] = osValue;
    RebuildURL();
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIS3HandleHelper::GetCurlHeaders( const CPLString& osVerb,
                                   const void *pabyDataContent,
                                   size_t nBytesContent )
{
    CPLString osXAMZDate = CPLGetConfigOption("AWS_TIMESTAMP", "");
    if( osXAMZDate.empty() )
        osXAMZDate = CPLGetAWS_SIGN4_Timestamp();

    const CPLString osXAMZContentSHA256 =
        CPLGetLowerCaseHexSHA256(pabyDataContent, nBytesContent);

    CPLString osCanonicalQueryString;
    std::map<CPLString, CPLString>::iterator oIter =
        m_oMapQueryParameters.begin();
    for( ; oIter != m_oMapQueryParameters.end(); ++oIter )
    {
        if( !osCanonicalQueryString.empty() )
            osCanonicalQueryString += "&";
        osCanonicalQueryString += oIter->first;
        osCanonicalQueryString += "=";
        osCanonicalQueryString += CPLAWSURLEncode(oIter->second);
    }

    const CPLString osHost(m_bUseVirtualHosting
        ? CPLString(m_osBucket + "." + m_osAWSS3Endpoint) : m_osAWSS3Endpoint);
    const CPLString osAuthorization = CPLGetAWS_SIGN4_Authorization(
        m_osSecretAccessKey,
        m_osAccessKeyId,
        m_osSessionToken,
        m_osAWSRegion,
        m_osRequestPayer,
        "s3",
        osVerb,
        osHost,
        m_bUseVirtualHosting
        ? ("/" + m_osObjectKey).c_str() :
        ("/" + m_osBucket + "/" + m_osObjectKey).c_str(),
        osCanonicalQueryString,
        osXAMZContentSHA256,
        osXAMZDate);

    struct curl_slist *headers=NULL;
    headers = curl_slist_append(
        headers, CPLSPrintf("x-amz-date: %s", osXAMZDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("x-amz-content-sha256: %s",
                            osXAMZContentSHA256.c_str()));
    if( !m_osSessionToken.empty() )
        headers = curl_slist_append(
            headers,
            CPLSPrintf("X-Amz-Security-Token: %s", m_osSessionToken.c_str()));
    if( !m_osRequestPayer.empty() )
        headers = curl_slist_append(
            headers,
            CPLSPrintf("x-amz-request-payer: %s", m_osRequestPayer.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    return headers;
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3HandleHelper::CanRestartOnError( const char* pszErrorMsg,
                                           bool bSetError )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "%s", pszErrorMsg);
#endif

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

    if( EQUAL(pszCode, "AuthorizationHeaderMalformed") )
    {
        const char* pszRegion = CPLGetXMLValue(psTree, "=Error.Region", NULL);
        if( pszRegion == NULL )
        {
            CPLDestroyXMLNode(psTree);
            if( bSetError )
            {
                VSIError(VSIE_AWSError,
                         "Malformed AWS XML response: %s", pszErrorMsg);
            }
            return false;
        }
        SetAWSRegion(pszRegion);
        CPLDebug("S3", "Switching to region %s", m_osAWSRegion.c_str());
        CPLDestroyXMLNode(psTree);
        return true;
    }

    if( EQUAL(pszCode, "PermanentRedirect") )
    {
        const char* pszEndpoint =
            CPLGetXMLValue(psTree, "=Error.Endpoint", NULL);
        if( pszEndpoint == NULL ||
            (m_bUseVirtualHosting &&
             (strncmp(pszEndpoint, m_osBucket.c_str(),
                      m_osBucket.size()) != 0 ||
              pszEndpoint[m_osBucket.size()] != '.')) )
        {
            CPLDestroyXMLNode(psTree);
            if( bSetError )
            {
                VSIError(VSIE_AWSError,
                         "Malformed AWS XML response: %s", pszErrorMsg);
            }
            return false;
        }
        if( !m_bUseVirtualHosting &&
            strncmp(pszEndpoint, m_osBucket.c_str(), m_osBucket.size()) == 0 &&
            pszEndpoint[m_osBucket.size()] == '.' )
        {
            m_bUseVirtualHosting = true;
            CPLDebug("S3", "Switching to virtual hosting");
        }
        SetAWSS3Endpoint(
            m_bUseVirtualHosting
            ? pszEndpoint + m_osBucket.size() + 1
            : pszEndpoint);
        CPLDebug("S3", "Switching to endpoint %s", m_osAWSS3Endpoint.c_str());
        CPLDestroyXMLNode(psTree);
        return true;
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
/*                          SetAWSS3Endpoint()                          */
/************************************************************************/

void VSIS3HandleHelper::SetAWSS3Endpoint( const CPLString &osStr )
{
    m_osAWSS3Endpoint = osStr;
    RebuildURL();
}

/************************************************************************/
/*                           SetAWSRegion()                             */
/************************************************************************/

void VSIS3HandleHelper::SetAWSRegion( const CPLString &osStr )
{
    m_osAWSRegion = osStr;
}

/************************************************************************/
/*                           SetRequestPayer()                          */
/************************************************************************/

void VSIS3HandleHelper::SetRequestPayer( const CPLString &osStr )
{
    m_osRequestPayer = osStr;
}

/************************************************************************/
/*                         SetVirtualHosting()                          */
/************************************************************************/

void VSIS3HandleHelper::SetVirtualHosting( bool b )
{
    m_bUseVirtualHosting = b;
    RebuildURL();
}

/************************************************************************/
/*                           SetObjectKey()                             */
/************************************************************************/

void VSIS3HandleHelper::SetObjectKey( const CPLString &osStr )
{
    m_osObjectKey = osStr;
    RebuildURL();
}

#endif

//! @endcond
