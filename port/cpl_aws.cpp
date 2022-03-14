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
#include "cpl_multiproc.h"
#include "cpl_http.h"
#include <algorithm>

CPL_CVSID("$Id$")

// #define DEBUG_VERBOSE 1

#ifdef WIN32
#if defined(HAVE_ATLBASE_H)
bool CPLFetchWindowsProductUUID(CPLString &osStr); // defined in cpl_aws_win32.cpp
#endif
const char* CPLGetWineVersion(); // defined in cpl_vsil_win32.cpp
#endif

#ifdef HAVE_CURL
static CPLMutex *ghMutex = nullptr;
static CPLString gosIAMRole;
static CPLString gosGlobalAccessKeyId;
static CPLString gosGlobalSecretAccessKey;
static CPLString gosGlobalSessionToken;
static GIntBig gnGlobalExpiration = 0;
static std::string gosRegion;

// The below variables are used for credentials retrieved through a STS AssumedRole
// operation
static std::string gosRoleArn;
static std::string gosExternalId;
static std::string gosMFASerial;
static std::string gosRoleSessionName;
static std::string gosSourceProfileAccessKeyId;
static std::string gosSourceProfileSecretAccessKey;
static std::string gosSourceProfileSessionToken;

/************************************************************************/
/*                         CPLGetLowerCaseHex()                         */
/************************************************************************/

static CPLString CPLGetLowerCaseHex( const GByte *pabyData, size_t nBytes )

{
    CPLString osRet;
    osRet.resize(nBytes * 2);

    constexpr char achHex[] = "0123456789abcdef";

    for( size_t i = 0; i < nBytes; ++i )
    {
        const int nLow = pabyData[i] & 0x0f;
        const int nHigh = (pabyData[i] & 0xf0) >> 4;

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
            osRet += CPLSPrintf("%%%02X", static_cast<unsigned char>(ch));
        }
    }
    return osRet;
}


/************************************************************************/
/*                         CPLAWSGetHeaderVal()                         */
/************************************************************************/

CPLString CPLAWSGetHeaderVal(const struct curl_slist* psExistingHeaders,
                             const char* pszKey)
{
    CPLString osKey(pszKey);
    osKey += ":";
    const struct curl_slist* psIter = psExistingHeaders;
    for(; psIter != nullptr; psIter = psIter->next)
    {
        if( STARTS_WITH(psIter->data, osKey.c_str()) )
            return CPLString(psIter->data + osKey.size()).Trim();
    }
    return CPLString();
}


/************************************************************************/
/*                 CPLGetAWS_SIGN4_Signature()                          */
/************************************************************************/

// See:
// http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
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
                               CPLString& osSignedHeaders )
{
/* -------------------------------------------------------------------- */
/*      Compute canonical request string.                               */
/* -------------------------------------------------------------------- */
    CPLString osCanonicalRequest = osVerb + "\n";

    osCanonicalRequest += osCanonicalURI + "\n";

    osCanonicalRequest += osCanonicalQueryString + "\n";

    std::map<CPLString, CPLString> oSortedMapHeaders;
    oSortedMapHeaders["host"] = osHost;
    if( osXAMZContentSHA256 != "UNSIGNED-PAYLOAD" && bAddHeaderAMZContentSHA256 )
    {
        oSortedMapHeaders["x-amz-content-sha256"] = osXAMZContentSHA256;
        oSortedMapHeaders["x-amz-date"] = osTimestamp;
    }
    if( !osRequestPayer.empty() )
        oSortedMapHeaders["x-amz-request-payer"] = osRequestPayer;
    if( !osAccessToken.empty() )
        oSortedMapHeaders["x-amz-security-token"] = osAccessToken;
    CPLString osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
                            oSortedMapHeaders,
                            psExistingHeaders,
                            "x-amz-"));

    osCanonicalRequest += osCanonicalizedHeaders + "\n";

    osSignedHeaders.clear();
    std::map<CPLString, CPLString>::const_iterator oIter = oSortedMapHeaders.begin();
    for(; oIter != oSortedMapHeaders.end(); ++oIter )
    {
        if( !osSignedHeaders.empty() )
            osSignedHeaders += ";";
        osSignedHeaders += oIter->first;
    }

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
    osScope += osRegion;
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
                     osRegion.c_str(), osRegion.size(),
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

    return osSignature;
}

/************************************************************************/
/*                CPLGetAWS_SIGN4_Authorization()                       */
/************************************************************************/

CPLString
CPLGetAWS_SIGN4_Authorization( const CPLString& osSecretAccessKey,
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
                               const CPLString& osTimestamp )
{
    CPLString osSignedHeaders;
    CPLString osSignature(CPLGetAWS_SIGN4_Signature(osSecretAccessKey,
                                                    osAccessToken,
                                                    osRegion,
                                                    osRequestPayer,
                                                    osService,
                                                    osVerb,
                                                    psExistingHeaders,
                                                    osHost,
                                                    osCanonicalURI,
                                                    osCanonicalQueryString,
                                                    osXAMZContentSHA256,
                                                    true, // bAddHeaderAMZContentSHA256
                                                    osTimestamp,
                                                    osSignedHeaders));

    CPLString osYYMMDD(osTimestamp);
    osYYMMDD.resize(8);

/* -------------------------------------------------------------------- */
/*      Build authorization header.                                     */
/* -------------------------------------------------------------------- */
    CPLString osAuthorization;
    osAuthorization = "AWS4-HMAC-SHA256 Credential=";
    osAuthorization += osAccessKeyId;
    osAuthorization += "/";
    osAuthorization += osYYMMDD;
    osAuthorization += "/";
    osAuthorization += osRegion;
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
    CPLUnixTimeToYMDHMS(time(nullptr), &brokenDown);

    char szTimeStamp[80] = {};
    snprintf(szTimeStamp, sizeof(szTimeStamp), "%04d%02d%02dT%02d%02d%02dZ",
            brokenDown.tm_year + 1900,
            brokenDown.tm_mon + 1,
            brokenDown.tm_mday,
            brokenDown.tm_hour,
            brokenDown.tm_min,
            brokenDown.tm_sec);
    return szTimeStamp;
}


/************************************************************************/
/*                         VSIS3HandleHelper()                          */
/************************************************************************/
VSIS3HandleHelper::VSIS3HandleHelper( const CPLString& osSecretAccessKey,
                                      const CPLString& osAccessKeyId,
                                      const CPLString& osSessionToken,
                                      const CPLString& osEndpoint,
                                      const CPLString& osRegion,
                                      const CPLString& osRequestPayer,
                                      const CPLString& osBucket,
                                      const CPLString& osObjectKey,
                                      bool bUseHTTPS,
                                      bool bUseVirtualHosting,
                                      AWSCredentialsSource eCredentialsSource ) :
    m_osURL(BuildURL(osEndpoint, osBucket, osObjectKey, bUseHTTPS,
                     bUseVirtualHosting)),
    m_osSecretAccessKey(osSecretAccessKey),
    m_osAccessKeyId(osAccessKeyId),
    m_osSessionToken(osSessionToken),
    m_osEndpoint(osEndpoint),
    m_osRegion(osRegion),
    m_osRequestPayer(osRequestPayer),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey),
    m_bUseHTTPS(bUseHTTPS),
    m_bUseVirtualHosting(bUseVirtualHosting),
    m_eCredentialsSource(eCredentialsSource)
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

CPLString VSIS3HandleHelper::BuildURL(const CPLString& osEndpoint,
                                      const CPLString& osBucket,
                                      const CPLString& osObjectKey,
                                      bool bUseHTTPS, bool bUseVirtualHosting)
{
    const char* pszProtocol = (bUseHTTPS) ? "https" : "http";
    if( osBucket.empty()  )
        return CPLSPrintf("%s://%s", pszProtocol,
                                        osEndpoint.c_str());
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

void VSIS3HandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint, m_osBucket, m_osObjectKey,
                       m_bUseHTTPS, m_bUseVirtualHosting);
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                        GetBucketAndObjectKey()                       */
/************************************************************************/

bool IVSIS3LikeHandleHelper::GetBucketAndObjectKey( const char* pszURI,
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
/*                      BuildCanonicalizedHeaders()                    */
/************************************************************************/

CPLString IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
                            std::map<CPLString, CPLString>& oSortedMapHeaders,
                            const struct curl_slist* psExistingHeaders,
                            const char* pszHeaderPrefix)
{
    const struct curl_slist* psIter = psExistingHeaders;
    for(; psIter != nullptr; psIter = psIter->next)
    {
        if( STARTS_WITH_CI(psIter->data, pszHeaderPrefix) ||
            STARTS_WITH_CI(psIter->data, "Content-MD5") )
        {
            const char* pszColumn = strstr(psIter->data, ":");
            if( pszColumn )
            {
                CPLString osKey(psIter->data);
                osKey.resize( pszColumn - psIter->data);
                oSortedMapHeaders[osKey.tolower()] =
                    CPLString(pszColumn + strlen(":")).Trim();
            }
        }
    }

    CPLString osCanonicalizedHeaders;
    std::map<CPLString, CPLString>::const_iterator oIter =
        oSortedMapHeaders.begin();
    for(; oIter != oSortedMapHeaders.end(); ++oIter )
    {
        osCanonicalizedHeaders += oIter->first + ":" + oIter->second + "\n";
    }
    return osCanonicalizedHeaders;
}

/************************************************************************/
/*                         GetRFC822DateTime()                          */
/************************************************************************/

CPLString IVSIS3LikeHandleHelper::GetRFC822DateTime()
{
    char szDate[64];
    time_t nNow = time(nullptr);
    struct tm tm;
    CPLUnixTimeToYMDHMS(nNow, &tm);
    int nRet = CPLPrintTime(szDate, sizeof(szDate)-1,
                    "%a, %d %b %Y %H:%M:%S GMT", &tm, "C");
    szDate[nRet] = 0;
    return szDate;
}

/************************************************************************/
/*                          ParseSimpleJson()                           */
/*                                                                      */
/*      Return a string list of name/value pairs extracted from a       */
/*      JSON doc.  The EC2 IAM web service returns simple JSON          */
/*      responses.  The parsing as done currently is very fragile       */
/*      and depends on JSON documents being in a very very simple       */
/*      form.                                                           */
/************************************************************************/

static CPLStringList ParseSimpleJson(const char *pszJson)

{
/* -------------------------------------------------------------------- */
/*      We are expecting simple documents like the following with no    */
/*      hierarchy or complex structure.                                 */
/* -------------------------------------------------------------------- */
/*
    {
    "Code" : "Success",
    "LastUpdated" : "2017-07-03T16:20:17Z",
    "Type" : "AWS-HMAC",
    "AccessKeyId" : "bla",
    "SecretAccessKey" : "bla",
    "Token" : "bla",
    "Expiration" : "2017-07-03T22:42:58Z"
    }
*/

    CPLStringList oWords(
        CSLTokenizeString2(pszJson, " \n\t,:{}", CSLT_HONOURSTRINGS ));
    CPLStringList oNameValue;

    for( int i=0; i < oWords.size(); i += 2 )
    {
        oNameValue.SetNameValue(oWords[i], oWords[i+1]);
    }

    return oNameValue;
}

/************************************************************************/
/*                        Iso8601ToUnixTime()                           */
/************************************************************************/

static bool Iso8601ToUnixTime(const char* pszDT, GIntBig* pnUnixTime)
{
    int nYear;
    int nMonth;
    int nDay;
    int nHour;
    int nMinute;
    int nSecond;
    if( sscanf(pszDT, "%04d-%02d-%02dT%02d:%02d:%02d",
                &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) == 6 )
    {
        struct tm brokendowntime;
        brokendowntime.tm_year = nYear - 1900;
        brokendowntime.tm_mon = nMonth - 1;
        brokendowntime.tm_mday = nDay;
        brokendowntime.tm_hour = nHour;
        brokendowntime.tm_min = nMinute;
        brokendowntime.tm_sec = nSecond;
        *pnUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
        return true;
    }
    return false;
}

/************************************************************************/
/*                  IsMachinePotentiallyEC2Instance()                   */
/************************************************************************/

static bool IsMachinePotentiallyEC2Instance()
{
#if defined(__linux) || defined(WIN32)
    const auto IsMachinePotentiallyEC2InstanceFromLinuxHost = []()
    {
        // On the newer Nitro Hypervisor (C5, M5, H1, T3), use
        // /sys/devices/virtual/dmi/id/sys_vendor = 'Amazon EC2' instead.

        // On older Xen hypervisor EC2 instances, a /sys/hypervisor/uuid file will
        // exist with a string beginning with 'ec2'.

        // If the files exist but don't contain the correct content, then we're not EC2 and
        // do not attempt any network access

        // Check for Xen Hypervisor instances
        // This file doesn't exist on Nitro instances
        VSILFILE* fp = VSIFOpenL("/sys/hypervisor/uuid", "rb");
        if( fp != nullptr )
        {
            char uuid[36+1] = { 0 };
            VSIFReadL( uuid, 1, sizeof(uuid)-1, fp );
            VSIFCloseL(fp);
            return EQUALN( uuid, "ec2", 3 );
        }

        // Check for Nitro Hypervisor instances
        // This file may exist on Xen instances with a value of 'Xen'
        // (but that doesn't mean we're on EC2)
        fp = VSIFOpenL("/sys/devices/virtual/dmi/id/sys_vendor", "rb");
        if( fp != nullptr )
        {
            char buf[10+1] = { 0 };
            VSIFReadL( buf, 1, sizeof(buf)-1, fp );
            VSIFCloseL(fp);
            return EQUALN( buf, "Amazon EC2", 10 );
        }

        // Fallback: Check via the network
        return true;
    };
#endif

#ifdef __linux
    // Optimization on Linux to avoid the network request
    // See http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/identify_ec2_instances.html
    // Skip if either:
    // - CPL_AWS_AUTODETECT_EC2=NO
    // - CPL_AWS_CHECK_HYPERVISOR_UUID=NO (deprecated)

    if( ! CPLTestBool(CPLGetConfigOption("CPL_AWS_AUTODETECT_EC2", "YES")) )
    {
        return true;
    }
    else
    {
        CPLString opt = CPLGetConfigOption("CPL_AWS_CHECK_HYPERVISOR_UUID", "");
        if ( ! opt.empty() )
        {
            CPLDebug("AWS", "CPL_AWS_CHECK_HYPERVISOR_UUID is deprecated. Use CPL_AWS_AUTODETECT_EC2 instead");
            if ( ! CPLTestBool(opt) )
            {
                return true;
            }
        }
    }

    return IsMachinePotentiallyEC2InstanceFromLinuxHost();

#elif defined(WIN32)
    if( ! CPLTestBool(CPLGetConfigOption("CPL_AWS_AUTODETECT_EC2", "YES")) )
    {
        return true;
    }

    // Regular UUID is not valid for WINE, fetch from sysfs instead.
    if( CPLGetWineVersion() != nullptr )
    {
        return IsMachinePotentiallyEC2InstanceFromLinuxHost();
    }
    else
    {
#if defined(HAVE_ATLBASE_H)
        CPLString osMachineUUID;
        if( CPLFetchWindowsProductUUID(osMachineUUID) )
        {
            if( osMachineUUID.length() >= 3 && EQUALN(osMachineUUID.c_str(), "EC2", 3) )
            {
                return true;
            }
            else if( osMachineUUID.length() >= 8 && osMachineUUID[4] == '2' &&
                     osMachineUUID[6] == 'E' && osMachineUUID[7] == 'C' )
            {
                return true;
            }
            else
            {
                return false;
            }
        }
#endif
    }

    // Fallback: Check via the network
    return true;
#else
    // At time of writing EC2 instances can be only Linux or Windows
    return false;
#endif
}

/************************************************************************/
/*                      GetConfigurationFromEC2()                       */
/************************************************************************/

bool VSIS3HandleHelper::GetConfigurationFromEC2(const std::string& osPathForOption,
                                                CPLString& osSecretAccessKey,
                                                CPLString& osAccessKeyId,
                                                CPLString& osSessionToken)
{
    CPLMutexHolder oHolder( &ghMutex );
    time_t nCurTime;
    time(&nCurTime);
    // Try to reuse credentials if they are still valid, but
    // keep one minute of margin...
    if( !gosGlobalAccessKeyId.empty() && nCurTime < gnGlobalExpiration - 60 )
    {
        osAccessKeyId = gosGlobalAccessKeyId;
        osSecretAccessKey = gosGlobalSecretAccessKey;
        osSessionToken = gosGlobalSessionToken;
        return true;
    }

    CPLString osURLRefreshCredentials;
    const CPLString osEC2DefaultURL("http://169.254.169.254");
    // coverity[tainted_data]
    const CPLString osEC2RootURL(
        VSIGetCredential(osPathForOption.c_str(), "CPL_AWS_EC2_API_ROOT_URL", osEC2DefaultURL));
    // coverity[tainted_data]
    const CPLString osECSRelativeURI(
        VSIGetCredential(osPathForOption.c_str(), "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI", ""));
    CPLString osToken;
    if( osEC2RootURL == osEC2DefaultURL && !osECSRelativeURI.empty() )
    {
        // See https://docs.aws.amazon.com/AmazonECS/latest/developerguide/task-iam-roles.html
        osURLRefreshCredentials = "http://169.254.170.2" + osECSRelativeURI;
    }
    else
    {
        if( !IsMachinePotentiallyEC2Instance() )
            return false;

        // Use IMDSv2 protocol:
        // https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html

        // Retrieve IMDSv2 token
        {
            const CPLString osEC2_IMDSv2_api_token_URL =
                osEC2RootURL + "/latest/api/token";
            CPLStringList aosOptions;
            aosOptions.SetNameValue("TIMEOUT", "1");
            aosOptions.SetNameValue("CUSTOMREQUEST", "PUT");
            aosOptions.SetNameValue("HEADERS",
                                    "X-aws-ec2-metadata-token-ttl-seconds: 10");
            CPLPushErrorHandler(CPLQuietErrorHandler);
            CPLHTTPResult* psResult =
                        CPLHTTPFetch( osEC2_IMDSv2_api_token_URL, aosOptions.List() );
            CPLPopErrorHandler();
            if( psResult )
            {
                if( psResult->nStatus == 0 && psResult->pabyData != nullptr )
                {
                    osToken = reinterpret_cast<char*>(psResult->pabyData);
                }
                else
                {
                    // Failure: either we are not running on EC2 (or something emulating it)
                    // or this doesn't implement yet IDMSv2
                    // Go on trying IDMSv1
                }
                CPLHTTPDestroyResult(psResult);
            }
        }

        // If we don't know yet the IAM role, fetch it
        const CPLString osEC2CredentialsURL =
            osEC2RootURL + "/latest/meta-data/iam/security-credentials/";
        if( gosIAMRole.empty() )
        {
            CPLStringList aosOptions;
            aosOptions.SetNameValue("TIMEOUT", "1");
            if( !osToken.empty() )
            {
                aosOptions.SetNameValue("HEADERS",
                                ("X-aws-ec2-metadata-token: " + osToken).c_str());
            }
            CPLPushErrorHandler(CPLQuietErrorHandler);
            CPLHTTPResult* psResult =
                        CPLHTTPFetch( osEC2CredentialsURL, aosOptions.List() );
            CPLPopErrorHandler();
            if( psResult )
            {
                if( psResult->nStatus == 0 && psResult->pabyData != nullptr )
                {
                    gosIAMRole = reinterpret_cast<char*>(psResult->pabyData);
                }
                CPLHTTPDestroyResult(psResult);
            }
            if( gosIAMRole.empty() )
            {
                // We didn't get the IAM role. We are definitely not running
                // on EC2 or an emulation of it.
                return false;
            }
        }
        osURLRefreshCredentials = osEC2CredentialsURL + gosIAMRole;
    }

    // Now fetch the refreshed credentials
    CPLStringList oResponse;
    CPLStringList aosOptions;
    if( !osToken.empty() )
    {
        aosOptions.SetNameValue("HEADERS",
                            ("X-aws-ec2-metadata-token: " + osToken).c_str());
    }
    CPLHTTPResult* psResult = CPLHTTPFetch(osURLRefreshCredentials.c_str(), aosOptions.List() );
    if( psResult )
    {
        if( psResult->nStatus == 0 && psResult->pabyData != nullptr )
        {
            const CPLString osJSon =
                    reinterpret_cast<char*>(psResult->pabyData);
            oResponse = ParseSimpleJson(osJSon);
        }
        CPLHTTPDestroyResult(psResult);
    }
    osAccessKeyId = oResponse.FetchNameValueDef("AccessKeyId", "");
    osSecretAccessKey =
                oResponse.FetchNameValueDef("SecretAccessKey", "");
    osSessionToken = oResponse.FetchNameValueDef("Token", "");
    const CPLString osExpiration =
        oResponse.FetchNameValueDef("Expiration", "");
    GIntBig nExpirationUnix = 0;
    if( !osAccessKeyId.empty() &&
        !osSecretAccessKey.empty() &&
        Iso8601ToUnixTime(osExpiration, &nExpirationUnix) )
    {
        gosGlobalAccessKeyId = osAccessKeyId;
        gosGlobalSecretAccessKey = osSecretAccessKey;
        gosGlobalSessionToken = osSessionToken;
        gnGlobalExpiration = nExpirationUnix;
        CPLDebug("AWS", "Storing AIM credentials until %s",
                osExpiration.c_str());
    }
    return !osAccessKeyId.empty() && !osSecretAccessKey.empty();
}


/************************************************************************/
/*                      UpdateAndWarnIfInconsistent()                   */
/************************************************************************/

static
void UpdateAndWarnIfInconsistent(const char* pszKeyword,
                                 CPLString& osVal,
                                 const CPLString& osNewVal,
                                 const CPLString& osCredentials,
                                 const CPLString& osConfig)
{
    // nominally defined in ~/.aws/credentials but can
    // be set here too. If both values exist, credentials
    // has the priority
    if( osVal.empty() )
    {
        osVal = osNewVal;
    }
    else if( osVal != osNewVal )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "%s defined in both %s "
                    "and %s. The one of %s will be used",
                    pszKeyword,
                    osCredentials.c_str(),
                    osConfig.c_str(),
                    osCredentials.c_str());
    }
}

/************************************************************************/
/*                         ReadAWSCredentials()                         */
/************************************************************************/

static bool ReadAWSCredentials(const std::string& osProfile,
                               const std::string& osCredentials,
                               CPLString& osSecretAccessKey,
                               CPLString& osAccessKeyId,
                               CPLString& osSessionToken)
{
    osSecretAccessKey.clear();
    osAccessKeyId.clear();
    osSessionToken.clear();

    VSILFILE* fp = VSIFOpenL( osCredentials.c_str(), "rb" );
    if( fp != nullptr )
    {
        const char* pszLine;
        bool bInProfile = false;
        const CPLString osBracketedProfile("[" + osProfile + "]");
        while( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            if( pszLine[0] == '[' )
            {
                if( bInProfile )
                    break;
                if( CPLString(pszLine) == osBracketedProfile )
                    bInProfile = true;
            }
            else if( bInProfile )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "aws_access_key_id") )
                        osAccessKeyId = pszValue;
                    else if( EQUAL(pszKey, "aws_secret_access_key") )
                        osSecretAccessKey = pszValue;
                    else if( EQUAL(pszKey, "aws_session_token") )
                        osSessionToken = pszValue;
                }
                CPLFree(pszKey);
            }
        }
        VSIFCloseL(fp);
    }

    return !osSecretAccessKey.empty() && !osAccessKeyId.empty();
}

/************************************************************************/
/*                GetConfigurationFromAWSConfigFiles()                  */
/************************************************************************/

bool VSIS3HandleHelper::GetConfigurationFromAWSConfigFiles(
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
                                                CPLString& osRoleSessionName)
{
    // See http://docs.aws.amazon.com/cli/latest/userguide/cli-config-files.html
    // If AWS_DEFAULT_PROFILE is set (obsolete, no longer documented), use it in priority
    // Otherwise use AWS_PROFILE
    // Otherwise fallback to "default"
    const char* pszProfile = VSIGetCredential(osPathForOption.c_str(), "AWS_DEFAULT_PROFILE", "");
    if( pszProfile[0] == '\0' )
        pszProfile = VSIGetCredential(osPathForOption.c_str(), "AWS_PROFILE", "");
    const CPLString osProfile(pszProfile[0] != '\0' ? pszProfile : "default");

#ifdef WIN32
    const char* pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
    constexpr char SEP_STRING[] = "\\";
#else
    const char* pszHome = CPLGetConfigOption("HOME", nullptr);
    constexpr char SEP_STRING[] = "/";
#endif

    CPLString osDotAws( pszHome ? pszHome : "" );
    osDotAws += SEP_STRING;
    osDotAws += ".aws";

    // Read first ~/.aws/credential file

    // GDAL specific config option (mostly for testing purpose, but also
    // used in production in some cases)
    const char* pszCredentials =
        VSIGetCredential(osPathForOption.c_str(), "CPL_AWS_CREDENTIALS_FILE", nullptr );
    if( pszCredentials )
    {
        osCredentials = pszCredentials;
    }
    else
    {
        osCredentials = osDotAws;
        osCredentials += SEP_STRING;
        osCredentials += "credentials";
    }

    ReadAWSCredentials(osProfile, osCredentials,
                       osSecretAccessKey, osAccessKeyId, osSessionToken);

    // And then ~/.aws/config file (unless AWS_CONFIG_FILE is defined)
    const char* pszAWSConfigFileEnv =
        VSIGetCredential(osPathForOption.c_str(), "AWS_CONFIG_FILE", nullptr );
    CPLString osConfig;
    if( pszAWSConfigFileEnv )
    {
        osConfig = pszAWSConfigFileEnv;
    }
    else
    {
        osConfig = osDotAws;
        osConfig += SEP_STRING;
        osConfig += "config";
    }
    VSILFILE* fp = VSIFOpenL( osConfig, "rb" );
    if( fp != nullptr )
    {
        const char* pszLine;
        bool bInProfile = false;
        const CPLString osBracketedProfile("[" + osProfile + "]");
        const CPLString osBracketedProfileProfile("[profile " + osProfile + "]");
        while( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            if( pszLine[0] == '[' )
            {
                if( bInProfile )
                    break;
                // In config file, the section name is nominally [profile foo]
                // for the non default profile.
                if( CPLString(pszLine) == osBracketedProfile ||
                    CPLString(pszLine) == osBracketedProfileProfile )
                {
                    bInProfile = true;
                }
            }
            else if( bInProfile )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "aws_access_key_id") )
                    {
                        UpdateAndWarnIfInconsistent(pszKey,
                                                    osAccessKeyId,
                                                    pszValue,
                                                    osCredentials,
                                                    osConfig);
                    }
                    else if( EQUAL(pszKey, "aws_secret_access_key") )
                    {
                        UpdateAndWarnIfInconsistent(pszKey,
                                                    osSecretAccessKey,
                                                    pszValue,
                                                    osCredentials,
                                                    osConfig);
                    }
                    else if( EQUAL(pszKey, "aws_session_token") )
                    {
                        UpdateAndWarnIfInconsistent(pszKey,
                                                    osSessionToken,
                                                    pszValue,
                                                    osCredentials,
                                                    osConfig);
                    }
                    else if( EQUAL(pszKey, "region") )
                    {
                        osRegion = pszValue;
                    }
                    else if( strcmp(pszKey, "role_arn") == 0 )
                    {
                        osRoleArn = pszValue;
                    }
                    else if( strcmp(pszKey, "source_profile") == 0 )
                    {
                        osSourceProfile = pszValue;
                    }
                    else if( strcmp(pszKey, "external_id") == 0 )
                    {
                        osExternalId = pszValue;
                    }
                    else if( strcmp(pszKey, "mfa_serial") == 0 )
                    {
                        osMFASerial = pszValue;
                    }
                    else if( strcmp(pszKey, "role_session_name") == 0 )
                    {
                        osRoleSessionName = pszValue;
                    }
                }
                CPLFree(pszKey);
            }
        }
        VSIFCloseL(fp);
    }
    else if( pszAWSConfigFileEnv != nullptr )
    {
        if( pszAWSConfigFileEnv[0] != '\0' )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s does not exist or cannot be open",
                     pszAWSConfigFileEnv);
        }
    }

    return (!osAccessKeyId.empty() && !osSecretAccessKey.empty()) ||
           (!osRoleArn.empty() && !osSourceProfile.empty());
}


/************************************************************************/
/*                     GetTemporaryCredentialsForRole()                 */
/************************************************************************/

// Issue a STS AssumedRole operation to get temporary credentials for an assumed
// role.
static bool GetTemporaryCredentialsForRole(const std::string& osRoleArn,
                                           const std::string& osExternalId,
                                           const std::string& osMFASerial,
                                           const std::string& osRoleSessionName,
                                           const std::string& osSecretAccessKey,
                                           const std::string& osAccessKeyId,
                                           const std::string& osSessionToken,
                                           std::string& osTempSecretAccessKey,
                                           std::string& osTempAccessKeyId,
                                           std::string& osTempSessionToken,
                                           std::string& osExpiration)
{
    std::string osXAMZDate = CPLGetConfigOption("AWS_TIMESTAMP", "");
    if( osXAMZDate.empty() )
        osXAMZDate = CPLGetAWS_SIGN4_Timestamp();
    std::string osDate(osXAMZDate);
    osDate.resize(8);

    const std::string osVerb("GET");
    const std::string osService("sts");
    const std::string osRegion(CPLGetConfigOption("AWS_STS_REGION", "us-east-1"));
    const std::string osHost(CPLGetConfigOption("AWS_STS_ENDPOINT", "sts.amazonaws.com"));

    std::map<std::string, std::string> oMap;
    oMap["X-Amz-Algorithm"] = "AWS4-HMAC-SHA256";
    oMap["X-Amz-Credential"] =
        osAccessKeyId + "/" + osDate + "/" + osRegion + "/" + osService + "/aws4_request";
    oMap["X-Amz-Date"] = osXAMZDate;
    oMap["X-Amz-SignedHeaders"] = "host";
    oMap["Version"] = "2011-06-15";
    oMap["Action"] = "AssumeRole";
    oMap["RoleArn"] = osRoleArn;
    oMap["RoleSessionName"] = !osRoleSessionName.empty() ?
        osRoleSessionName.c_str() :
        CPLGetConfigOption("AWS_ROLE_SESSION_NAME", "GDAL-session");
    if( !osExternalId.empty() )
        oMap["ExternalId"] = osExternalId;
    if( !osMFASerial.empty() )
        oMap["SerialNumber"] = osMFASerial;

    std::string osQueryString;
    for( const auto& kv: oMap )
    {
        if( osQueryString.empty() )
            osQueryString += "?";
        else
            osQueryString += "&";
        osQueryString += kv.first;
        osQueryString += "=";
        osQueryString += CPLAWSURLEncode(kv.second);
    }
    CPLString osCanonicalQueryString(osQueryString.substr(1));

    CPLString osSignedHeaders;
    const CPLString osSignature =
      CPLGetAWS_SIGN4_Signature(
        osSecretAccessKey,
        osSessionToken,
        osRegion,
        std::string(), //m_osRequestPayer,
        osService,
        osVerb,
        nullptr, /* existing headers */
        osHost,
        "/",
        osCanonicalQueryString,
        CPLGetLowerCaseHexSHA256(std::string()),
        false, // bAddHeaderAMZContentSHA256
        osXAMZDate,
        osSignedHeaders);

    bool bRet = false;
    const bool bUseHTTPS = CPLTestBool(CPLGetConfigOption("AWS_HTTPS", "YES"));
    const std::string osURL = (bUseHTTPS ? "https://" : "http://") + osHost + "/" + osQueryString + "&X-Amz-Signature=" + osSignature;
    CPLHTTPResult* psResult = CPLHTTPFetch( osURL.c_str(), nullptr );
    if( psResult )
    {
        if( psResult->nStatus == 0 && psResult->pabyData != nullptr )
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(reinterpret_cast<char*>(psResult->pabyData)));
            if( oTree )
            {
                const auto psCredentials = CPLGetXMLNode(oTree.get(), "=AssumeRoleResponse.AssumeRoleResult.Credentials");
                if( psCredentials )
                {
                    osTempAccessKeyId = CPLGetXMLValue(psCredentials, "AccessKeyId", "");
                    osTempSecretAccessKey = CPLGetXMLValue(psCredentials, "SecretAccessKey", "");
                    osTempSessionToken = CPLGetXMLValue(psCredentials, "SessionToken", "");
                    osExpiration = CPLGetXMLValue(psCredentials, "Expiration", "");
                    bRet = true;
                }
                else
                {
                    CPLDebug("S3", "%s", reinterpret_cast<char*>(psResult->pabyData));
                }
            }
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bRet;
}

/************************************************************************/
/*               GetOrRefreshTemporaryCredentialsForRole()              */
/************************************************************************/

static bool GetOrRefreshTemporaryCredentialsForRole(CPLString& osSecretAccessKey,
                                                    CPLString& osAccessKeyId,
                                                    CPLString& osSessionToken,
                                                    CPLString& osRegion)
{
    CPLMutexHolder oHolder( &ghMutex );
    time_t nCurTime;
    time(&nCurTime);
    // Try to reuse credentials if they are still valid, but
    // keep one minute of margin...
    if( !gosGlobalAccessKeyId.empty() && nCurTime < gnGlobalExpiration - 60 )
    {
        osAccessKeyId = gosGlobalAccessKeyId;
        osSecretAccessKey = gosGlobalSecretAccessKey;
        osSessionToken = gosGlobalSessionToken;
        osRegion = gosRegion;
        return true;
    }

    std::string osExpiration;
    gosGlobalSecretAccessKey.clear();
    gosGlobalAccessKeyId.clear();
    gosGlobalSessionToken.clear();
    if( GetTemporaryCredentialsForRole(gosRoleArn,
                                       gosExternalId,
                                       gosMFASerial,
                                       gosRoleSessionName,
                                       gosSourceProfileSecretAccessKey,
                                       gosSourceProfileAccessKeyId,
                                       gosSourceProfileSessionToken,
                                       gosGlobalSecretAccessKey,
                                       gosGlobalAccessKeyId,
                                       gosGlobalSessionToken,
                                       osExpiration) )
    {
        Iso8601ToUnixTime(osExpiration.c_str(), &gnGlobalExpiration);
        osAccessKeyId = gosGlobalAccessKeyId;
        osSecretAccessKey = gosGlobalSecretAccessKey;
        osSessionToken = gosGlobalSessionToken;
        osRegion = gosRegion;
        return true;
    }
    return false;
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIS3HandleHelper::GetConfiguration(const std::string& osPathForOption,
                                         CSLConstList papszOptions,
                                         CPLString& osSecretAccessKey,
                                         CPLString& osAccessKeyId,
                                         CPLString& osSessionToken,
                                         CPLString& osRegion,
                                         AWSCredentialsSource& eCredentialsSource)
{
    eCredentialsSource = AWSCredentialsSource::REGULAR;

    // AWS_REGION is GDAL specific. Later overloaded by standard
    // AWS_DEFAULT_REGION
    osRegion = CSLFetchNameValueDef(papszOptions, "AWS_REGION",
            VSIGetCredential(osPathForOption.c_str(), "AWS_REGION", "us-east-1"));

    if( CPLTestBool(VSIGetCredential(osPathForOption.c_str(), "AWS_NO_SIGN_REQUEST", "NO")) )
    {
        osSecretAccessKey.clear();
        osAccessKeyId.clear();
        osSessionToken.clear();
        return true;
    }

    osSecretAccessKey = CSLFetchNameValueDef(papszOptions,
        "AWS_SECRET_ACCESS_KEY",
        VSIGetCredential(osPathForOption.c_str(), "AWS_SECRET_ACCESS_KEY", ""));
    if( !osSecretAccessKey.empty() )
    {
        osAccessKeyId = CSLFetchNameValueDef(papszOptions,
            "AWS_ACCESS_KEY_ID",
            VSIGetCredential(osPathForOption.c_str(), "AWS_ACCESS_KEY_ID", ""));
        if( osAccessKeyId.empty() )
        {
            VSIError(VSIE_AWSInvalidCredentials,
                    "AWS_ACCESS_KEY_ID configuration option not defined");
            return false;
        }

        osSessionToken = CSLFetchNameValueDef(papszOptions,
            "AWS_SESSION_TOKEN",
            VSIGetCredential(osPathForOption.c_str(), "AWS_SESSION_TOKEN", ""));
        return true;
    }

    // Next try to see if we have a current assumed role
    bool bAssumedRole = false;
    {
        CPLMutexHolder oHolder( &ghMutex );
        bAssumedRole = !gosRoleArn.empty();
    }
    if( bAssumedRole &&
        GetOrRefreshTemporaryCredentialsForRole(osSecretAccessKey,
                                                osAccessKeyId,
                                                osSessionToken,
                                                osRegion) )
    {
        eCredentialsSource = AWSCredentialsSource::ASSUMED_ROLE;
        return true;
    }

    // Next try reading from ~/.aws/credentials and ~/.aws/config
    CPLString osCredentials;
    CPLString osRoleArn;
    CPLString osSourceProfile;
    CPLString osExternalId;
    CPLString osMFASerial;
    CPLString osRoleSessionName;
    // coverity[tainted_data]
    if( GetConfigurationFromAWSConfigFiles(osPathForOption,
                                           osSecretAccessKey, osAccessKeyId,
                                           osSessionToken, osRegion,
                                           osCredentials,
                                           osRoleArn,
                                           osSourceProfile,
                                           osExternalId,
                                           osMFASerial,
                                           osRoleSessionName) )
    {
        if( osSecretAccessKey.empty() && !osRoleArn.empty() )
        {
            // Get the credentials for the source profile, that will be
            // used to sign the STS AssumedRole request.
            if( !ReadAWSCredentials(osSourceProfile, osCredentials,
                                    osSecretAccessKey,
                                    osAccessKeyId,
                                    osSessionToken) )
            {
                VSIError(VSIE_AWSInvalidCredentials,
                         "Cannot retrieve credentials for source profile %s",
                         osSourceProfile.c_str());
                return false;
            }

            std::string osTempSecretAccessKey;
            std::string osTempAccessKeyId;
            std::string osTempSessionToken;
            std::string osExpiration;
            if( GetTemporaryCredentialsForRole(osRoleArn,
                                               osExternalId,
                                               osMFASerial,
                                               osRoleSessionName,
                                               osSecretAccessKey,
                                               osAccessKeyId,
                                               osSessionToken,
                                               osTempSecretAccessKey,
                                               osTempAccessKeyId,
                                               osTempSessionToken,
                                               osExpiration) )
            {
                CPLDebug("S3", "Using assumed role %s", osRoleArn.c_str());
                {
                    // Store global variables to be able to reuse the
                    // temporary credentials
                    CPLMutexHolder oHolder( &ghMutex );
                    Iso8601ToUnixTime(osExpiration.c_str(), &gnGlobalExpiration);
                    gosRoleArn = osRoleArn;
                    gosExternalId = osExternalId;
                    gosMFASerial = osMFASerial;
                    gosRoleSessionName = osRoleSessionName;
                    gosSourceProfileSecretAccessKey = osSecretAccessKey;
                    gosSourceProfileAccessKeyId = osAccessKeyId;
                    gosSourceProfileSessionToken = osSessionToken;
                    gosGlobalAccessKeyId = osTempAccessKeyId;
                    gosGlobalSecretAccessKey = osTempSecretAccessKey;
                    gosGlobalSessionToken = osTempSessionToken;
                    gosRegion = osRegion;
                }
                osSecretAccessKey = osTempSecretAccessKey;
                osAccessKeyId = osTempAccessKeyId;
                osSessionToken = osTempSessionToken;
                eCredentialsSource = AWSCredentialsSource::ASSUMED_ROLE;
                return true;
            }
            return false;
        }

        return true;
    }

    // Last method: use IAM role security credentials on EC2 instances
    if( GetConfigurationFromEC2(osPathForOption,
                                osSecretAccessKey, osAccessKeyId,
                                osSessionToken) )
    {
        eCredentialsSource = AWSCredentialsSource::EC2;
        return true;
    }

    VSIError(VSIE_AWSInvalidCredentials,
                "AWS_SECRET_ACCESS_KEY and AWS_NO_SIGN_REQUEST configuration "
                "options not defined, and %s not filled",
                osCredentials.c_str());
    return false;
}

/************************************************************************/
/*                          CleanMutex()                                */
/************************************************************************/

void VSIS3HandleHelper::CleanMutex()
{
    if( ghMutex != nullptr )
        CPLDestroyMutex( ghMutex );
    ghMutex = nullptr;
}

/************************************************************************/
/*                          ClearCache()                                */
/************************************************************************/

void VSIS3HandleHelper::ClearCache()
{
    CPLMutexHolder oHolder( &ghMutex );

    gosIAMRole.clear();
    gosGlobalAccessKeyId.clear();
    gosGlobalSecretAccessKey.clear();
    gosGlobalSessionToken.clear();
    gnGlobalExpiration = 0;
    gosRoleArn.clear();
    gosExternalId.clear();
    gosMFASerial.clear();
    gosRoleSessionName.clear();
    gosSourceProfileAccessKeyId.clear();
    gosSourceProfileSecretAccessKey.clear();
    gosSourceProfileSessionToken.clear();
    gosRegion.clear();
}

/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIS3HandleHelper* VSIS3HandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* pszFSPrefix,
                                                    bool bAllowNoObject,
                                                    CSLConstList papszOptions )
{
    std::string osPathForOption("/vsis3/");
    if( pszURI )
        osPathForOption += pszURI;

    CPLString osSecretAccessKey;
    CPLString osAccessKeyId;
    CPLString osSessionToken;
    CPLString osRegion;
    AWSCredentialsSource eCredentialsSource = AWSCredentialsSource::REGULAR;
    if( !GetConfiguration(osPathForOption,
                          papszOptions,
                          osSecretAccessKey, osAccessKeyId,
                          osSessionToken, osRegion, eCredentialsSource) )
    {
        return nullptr;
    }

    // According to http://docs.aws.amazon.com/cli/latest/userguide/cli-environment.html
    // " This variable overrides the default region of the in-use profile, if set."
    const CPLString osDefaultRegion = CSLFetchNameValueDef(
        papszOptions, "AWS_DEFAULT_REGION",
        VSIGetCredential(osPathForOption.c_str(), "AWS_DEFAULT_REGION", ""));
    if( !osDefaultRegion.empty() )
    {
        osRegion = osDefaultRegion;
    }

    const CPLString osEndpoint =
        VSIGetCredential(osPathForOption.c_str(), "AWS_S3_ENDPOINT", "s3.amazonaws.com");
    const CPLString osRequestPayer =
        VSIGetCredential(osPathForOption.c_str(), "AWS_REQUEST_PAYER", "");
    CPLString osBucket;
    CPLString osObjectKey;
    if( pszURI != nullptr && pszURI[0] != '\0' &&
        !GetBucketAndObjectKey(pszURI, pszFSPrefix, bAllowNoObject,
                               osBucket, osObjectKey) )
    {
        return nullptr;
    }
    const bool bUseHTTPS = CPLTestBool(VSIGetCredential(osPathForOption.c_str(), "AWS_HTTPS", "YES"));
    const bool bIsValidNameForVirtualHosting =
        osBucket.find('.') == std::string::npos;
    const bool bUseVirtualHosting = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "AWS_VIRTUAL_HOSTING",
                VSIGetCredential(osPathForOption.c_str(), "AWS_VIRTUAL_HOSTING",
                           bIsValidNameForVirtualHosting ? "TRUE" : "FALSE")));
    return new VSIS3HandleHelper(osSecretAccessKey, osAccessKeyId,
                                 osSessionToken,
                                 osEndpoint, osRegion,
                                 osRequestPayer,
                                 osBucket, osObjectKey, bUseHTTPS,
                                 bUseVirtualHosting, eCredentialsSource);
}

/************************************************************************/
/*                          GetQueryString()                            */
/************************************************************************/

CPLString IVSIS3LikeHandleHelper::GetQueryString(bool bAddEmptyValueAfterEqual) const
{
    CPLString osQueryString;
    std::map<CPLString, CPLString>::const_iterator oIter =
        m_oMapQueryParameters.begin();
    for( ; oIter != m_oMapQueryParameters.end(); ++oIter )
    {
        if( oIter == m_oMapQueryParameters.begin() )
            osQueryString += "?";
        else
            osQueryString += "&";
        osQueryString += oIter->first;
        if( !oIter->second.empty() || bAddEmptyValueAfterEqual )
        {
            osQueryString += "=";
            osQueryString += CPLAWSURLEncode(oIter->second);
        }
    }
    return osQueryString;
}

/************************************************************************/
/*                       ResetQueryParameters()                         */
/************************************************************************/

void IVSIS3LikeHandleHelper::ResetQueryParameters()
{
    m_oMapQueryParameters.clear();
    RebuildURL();
}

/************************************************************************/
/*                         AddQueryParameter()                          */
/************************************************************************/

void IVSIS3LikeHandleHelper::AddQueryParameter( const CPLString& osKey,
                                                const CPLString& osValue )
{
    m_oMapQueryParameters[osKey] = osValue;
    RebuildURL();
}

/************************************************************************/
/*                           GetURLNoKVP()                              */
/************************************************************************/

CPLString IVSIS3LikeHandleHelper::GetURLNoKVP() const
{
    CPLString osURL(GetURL());
    const auto nPos = osURL.find('?');
    if( nPos != std::string::npos )
        osURL.resize(nPos);
    return osURL;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIS3HandleHelper::GetCurlHeaders( const CPLString& osVerb,
                                   const struct curl_slist* psExistingHeaders,
                                   const void *pabyDataContent,
                                   size_t nBytesContent ) const
{
    std::string osPathForOption("/vsis3/");
    osPathForOption += m_osBucket;
    osPathForOption += '/';
    osPathForOption += m_osObjectKey;

    if( m_eCredentialsSource == AWSCredentialsSource::EC2 )
    {
        CPLString osSecretAccessKey, osAccessKeyId, osSessionToken;
        if( GetConfigurationFromEC2(osPathForOption.c_str(),
                                    osSecretAccessKey,
                                    osAccessKeyId,
                                    osSessionToken) )
        {
            m_osSecretAccessKey = osSecretAccessKey;
            m_osAccessKeyId = osAccessKeyId;
            m_osSessionToken = osSessionToken;
        }
    }
    else if( m_eCredentialsSource == AWSCredentialsSource::ASSUMED_ROLE )
    {
        CPLString osSecretAccessKey, osAccessKeyId, osSessionToken;
        CPLString osRegion;
        if( GetOrRefreshTemporaryCredentialsForRole(osSecretAccessKey,
                                                    osAccessKeyId,
                                                    osSessionToken,
                                                    osRegion) )
        {
            m_osSecretAccessKey = osSecretAccessKey;
            m_osAccessKeyId = osAccessKeyId;
            m_osSessionToken = osSessionToken;
        }
    }

    CPLString osXAMZDate = VSIGetCredential(osPathForOption.c_str(), "AWS_TIMESTAMP", "");
    if( osXAMZDate.empty() )
        osXAMZDate = CPLGetAWS_SIGN4_Timestamp();

    const CPLString osXAMZContentSHA256 =
        CPLGetLowerCaseHexSHA256(pabyDataContent, nBytesContent);

    CPLString osCanonicalQueryString(GetQueryString(true));
    if( !osCanonicalQueryString.empty() )
        osCanonicalQueryString = osCanonicalQueryString.substr(1);

    const CPLString osHost(m_bUseVirtualHosting && !m_osBucket.empty()
        ? CPLString(m_osBucket + "." + m_osEndpoint) : m_osEndpoint);
    const CPLString osAuthorization = m_osSecretAccessKey.empty() ? CPLString():
      CPLGetAWS_SIGN4_Authorization(
        m_osSecretAccessKey,
        m_osAccessKeyId,
        m_osSessionToken,
        m_osRegion,
        m_osRequestPayer,
        "s3",
        osVerb,
        psExistingHeaders,
        osHost,
        m_bUseVirtualHosting
        ? CPLAWSURLEncode("/" + m_osObjectKey, false).c_str() :
        CPLAWSURLEncode("/" + m_osBucket + "/" + m_osObjectKey, false).c_str(),
        osCanonicalQueryString,
        osXAMZContentSHA256,
        osXAMZDate);

    struct curl_slist *headers=nullptr;
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
    if( !osAuthorization.empty() )
    {
        headers = curl_slist_append(
            headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    }
    return headers;
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3HandleHelper::CanRestartOnError( const char* pszErrorMsg,
                                           const char* pszHeaders,
                                           bool bSetError, bool* pbUpdateMap )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("S3", "%s", pszErrorMsg);
    CPLDebug("S3", "%s", pszHeaders ? pszHeaders : "");
#endif

    if( pbUpdateMap != nullptr )
        *pbUpdateMap = true;

    if( !STARTS_WITH(pszErrorMsg, "<?xml") &&
        !STARTS_WITH(pszErrorMsg, "<Error>") )
    {
        if( bSetError )
        {
            VSIError(VSIE_AWSError, "Invalid AWS response: %s", pszErrorMsg);
        }
        return false;
    }

    CPLXMLNode* psTree = CPLParseXMLString(pszErrorMsg);
    if( psTree == nullptr )
    {
        if( bSetError )
        {
            VSIError(VSIE_AWSError,
                     "Malformed AWS XML response: %s", pszErrorMsg);
        }
        return false;
    }

    const char* pszCode = CPLGetXMLValue(psTree, "=Error.Code", nullptr);
    if( pszCode == nullptr )
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
        const char* pszRegion = CPLGetXMLValue(psTree, "=Error.Region", nullptr);
        if( pszRegion == nullptr )
        {
            CPLDestroyXMLNode(psTree);
            if( bSetError )
            {
                VSIError(VSIE_AWSError,
                         "Malformed AWS XML response: %s", pszErrorMsg);
            }
            return false;
        }
        SetRegion(pszRegion);
        CPLDebug("S3", "Switching to region %s", m_osRegion.c_str());
        CPLDestroyXMLNode(psTree);
        return true;
    }

    if( EQUAL(pszCode, "PermanentRedirect") || EQUAL(pszCode, "TemporaryRedirect") )
    {
        const bool bIsTemporaryRedirect = EQUAL(pszCode, "TemporaryRedirect");
        const char* pszEndpoint =
            CPLGetXMLValue(psTree, "=Error.Endpoint", nullptr);
        if( pszEndpoint == nullptr ||
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
            /* If we have a body with
            <Error><Code>PermanentRedirect</Code><Message>The bucket you are attempting to access must be addressed using the specified endpoint. Please send all future requests to this endpoint.</Message><Bucket>bucket.with.dot</Bucket><Endpoint>bucket.with.dot.s3.amazonaws.com</Endpoint></Error>
            and headers like
            x-amz-bucket-region: eu-west-1
            and the bucket name has dot in it,
            then we must use s3.$(x-amz-bucket-region).amazon.com as endpoint.
            See #7154 */
            const char* pszRegionPtr = (pszHeaders != nullptr) ?
                strstr(pszHeaders, "x-amz-bucket-region: "): nullptr;
            if( strchr(m_osBucket.c_str(), '.') != nullptr && pszRegionPtr != nullptr )
            {
                CPLString osRegion(pszRegionPtr + strlen("x-amz-bucket-region: "));
                size_t nPos = osRegion.find('\r');
                if( nPos != std::string::npos )
                    osRegion.resize(nPos);
                SetEndpoint( CPLSPrintf("s3.%s.amazonaws.com", osRegion.c_str()) );
                SetRegion(osRegion.c_str());
                CPLDebug("S3", "Switching to endpoint %s", m_osEndpoint.c_str());
                CPLDebug("S3", "Switching to region %s", m_osRegion.c_str());
                CPLDestroyXMLNode(psTree);
                if( bIsTemporaryRedirect && pbUpdateMap != nullptr)
                    *pbUpdateMap = false;
                return true;
            }

            m_bUseVirtualHosting = true;
            CPLDebug("S3", "Switching to virtual hosting");
        }
        SetEndpoint(
            m_bUseVirtualHosting
            ? pszEndpoint + m_osBucket.size() + 1
            : pszEndpoint);
        CPLDebug("S3", "Switching to endpoint %s", m_osEndpoint.c_str());
        CPLDestroyXMLNode(psTree);

        if( bIsTemporaryRedirect && pbUpdateMap != nullptr)
            *pbUpdateMap = false;

        return true;
    }

    if( bSetError )
    {
        // Translate AWS errors into VSI errors.
        const char* pszMessage = CPLGetXMLValue(psTree, "=Error.Message", nullptr);

        if( pszMessage == nullptr ) {
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
/*                          SetEndpoint()                          */
/************************************************************************/

void VSIS3HandleHelper::SetEndpoint( const CPLString &osStr )
{
    m_osEndpoint = osStr;
    RebuildURL();
}

/************************************************************************/
/*                           SetRegion()                             */
/************************************************************************/

void VSIS3HandleHelper::SetRegion( const CPLString &osStr )
{
    m_osRegion = osStr;
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
/*                           GetSignedURL()                             */
/************************************************************************/

CPLString VSIS3HandleHelper::GetSignedURL(CSLConstList papszOptions)
{
    std::string osPathForOption("/vsis3/");
    osPathForOption += m_osBucket;
    osPathForOption += '/';
    osPathForOption += m_osObjectKey;

    CPLString osXAMZDate = CSLFetchNameValueDef(papszOptions, "START_DATE",
        VSIGetCredential(osPathForOption.c_str(), "AWS_TIMESTAMP", ""));
    if( osXAMZDate.empty() )
        osXAMZDate = CPLGetAWS_SIGN4_Timestamp();
    CPLString osDate(osXAMZDate);
    osDate.resize(8);

    CPLString osXAMZExpires =
        CSLFetchNameValueDef(papszOptions, "EXPIRATION_DELAY", "3600");

    CPLString osVerb(CSLFetchNameValueDef(papszOptions, "VERB", "GET"));

    ResetQueryParameters();
    AddQueryParameter("X-Amz-Algorithm", "AWS4-HMAC-SHA256");
    AddQueryParameter("X-Amz-Credential",
        m_osAccessKeyId + "/" + osDate + "/" + m_osRegion + "/s3/aws4_request");
    AddQueryParameter("X-Amz-Date", osXAMZDate);
    AddQueryParameter("X-Amz-Expires", osXAMZExpires);
    AddQueryParameter("X-Amz-SignedHeaders", "host");

    CPLString osCanonicalQueryString(GetQueryString(true).substr(1));

    const CPLString osHost(m_bUseVirtualHosting && !m_osBucket.empty()
        ? CPLString(m_osBucket + "." + m_osEndpoint) : m_osEndpoint);
    CPLString osSignedHeaders;
    const CPLString osSignature =
      CPLGetAWS_SIGN4_Signature(
        m_osSecretAccessKey,
        m_osSessionToken,
        m_osRegion,
        m_osRequestPayer,
        "s3",
        osVerb,
        nullptr, /* existing headers */
        osHost,
        m_bUseVirtualHosting
        ? CPLAWSURLEncode("/" + m_osObjectKey, false).c_str() :
        CPLAWSURLEncode("/" + m_osBucket + "/" + m_osObjectKey, false).c_str(),
        osCanonicalQueryString,
        "UNSIGNED-PAYLOAD",
        false, // bAddHeaderAMZContentSHA256
        osXAMZDate,
        osSignedHeaders);

    AddQueryParameter("X-Amz-Signature", osSignature);
    return m_osURL;
}

/************************************************************************/
/*                        UpdateMapFromHandle()                         */
/************************************************************************/

std::mutex VSIS3UpdateParams::gsMutex{};
std::map< CPLString, VSIS3UpdateParams > VSIS3UpdateParams::goMapBucketsToS3Params{};

void VSIS3UpdateParams::UpdateMapFromHandle( IVSIS3LikeHandleHelper* poHandleHelper )
{
    std::lock_guard<std::mutex> guard(gsMutex);

    VSIS3HandleHelper * poS3HandleHelper =
        cpl::down_cast<VSIS3HandleHelper *>(poHandleHelper);
    goMapBucketsToS3Params[ poS3HandleHelper->GetBucket() ] =
        VSIS3UpdateParams ( poS3HandleHelper );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3UpdateParams::UpdateHandleFromMap( IVSIS3LikeHandleHelper* poHandleHelper )
{
    std::lock_guard<std::mutex> guard(gsMutex);

    VSIS3HandleHelper * poS3HandleHelper =
        cpl::down_cast<VSIS3HandleHelper *>(poHandleHelper);
    std::map< CPLString, VSIS3UpdateParams>::iterator oIter =
        goMapBucketsToS3Params.find(poS3HandleHelper->GetBucket());
    if( oIter != goMapBucketsToS3Params.end() )
    {
        oIter->second.UpdateHandlerHelper(poS3HandleHelper);
    }
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIS3UpdateParams::ClearCache()
{
    std::lock_guard<std::mutex> guard(gsMutex);

    goMapBucketsToS3Params.clear();
}

#endif

//! @endcond
