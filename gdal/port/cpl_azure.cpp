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
 * DEALINAzureBlob IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_azure.h"
#include "cpl_vsi_error.h"
#include "cpl_sha256.h"
#include "cpl_time.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

#ifdef HAVE_CURL

/************************************************************************/
/*                            GetSignature()                            */
/************************************************************************/

static CPLString GetSignature(const CPLString& osStringToSign,
                              const CPLString& osStorageKeyB64 )
{

/* -------------------------------------------------------------------- */
/*      Compute signature.                                              */
/* -------------------------------------------------------------------- */

    CPLString osStorageKeyUnbase64(osStorageKeyB64);
    int nB64Length =
        CPLBase64DecodeInPlace(reinterpret_cast<GByte*>(&osStorageKeyUnbase64[0]));
    osStorageKeyUnbase64.resize(nB64Length);
#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "signing key size: %d", nB64Length);
#endif

    GByte abySignature[CPL_SHA256_HASH_SIZE] = {};
    CPL_HMAC_SHA256( osStorageKeyUnbase64, nB64Length,
                     osStringToSign, osStringToSign.size(),
                     abySignature);

    char* pszB64Signature = CPLBase64Encode(CPL_SHA256_HASH_SIZE, abySignature);
    CPLString osSignature(pszB64Signature);
    CPLFree(pszB64Signature);
    return osSignature;
}

/************************************************************************/
/*                          GetAzureBlobHeaders()                       */
/************************************************************************/

static
struct curl_slist* GetAzureBlobHeaders( const CPLString& osVerb,
                                        const struct curl_slist* psExistingHeaders,
                                        const CPLString& osResource,
                                        const std::map<CPLString, CPLString>& oMapQueryParameters,
                                        const CPLString& osStorageAccount,
                                        const CPLString& osStorageKeyB64 )
{
    /* See https://docs.microsoft.com/en-us/rest/api/storageservices/authentication-for-the-azure-storage-services */

    CPLString osDate = CPLGetConfigOption("CPL_AZURE_TIMESTAMP", "");
    if( osDate.empty() )
    {
        osDate = IVSIS3LikeHandleHelper::GetRFC822DateTime();
    }

    CPLString osMsVersion("2015-02-21");
    std::map<CPLString, CPLString> oSortedMapMSHeaders;
    oSortedMapMSHeaders["x-ms-version"] = osMsVersion;
    oSortedMapMSHeaders["x-ms-date"] = osDate;
    CPLString osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
                            oSortedMapMSHeaders,
                            psExistingHeaders,
                            "x-ms-"));

    CPLString osCanonicalizedResource;
    osCanonicalizedResource += "/" + osStorageAccount;
    osCanonicalizedResource += osResource;

    // We assume query parameters are in lower case and they are not repeated
    std::map<CPLString, CPLString>::const_iterator
        oIter = oMapQueryParameters.begin();
    for( ; oIter != oMapQueryParameters.end(); ++oIter )
    {
        osCanonicalizedResource += "\n";
        osCanonicalizedResource += oIter->first;
        osCanonicalizedResource += ":";
        osCanonicalizedResource += oIter->second;
    }

    CPLString osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-Encoding") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-Language") + "\n";
    CPLString osContentLength(CPLAWSGetHeaderVal(psExistingHeaders, "Content-Length"));
    if( osContentLength == "0" )
        osContentLength.clear(); // since x-ms-version 2015-02-21
    osStringToSign += osContentLength + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Date") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "If-Modified-Since") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "If-Match") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "If-None-Match") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "If-Unmodified-Since") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Range") + "\n";
    osStringToSign += osCanonicalizedHeaders;
    osStringToSign += osCanonicalizedResource;

#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "osStringToSign = %s", osStringToSign.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Compute signature.                                              */
/* -------------------------------------------------------------------- */

    CPLString osAuthorization("SharedKey " + osStorageAccount + ":" +
                              GetSignature(osStringToSign, osStorageKeyB64));

    struct curl_slist *headers=nullptr;
    headers = curl_slist_append(
        headers, CPLSPrintf("x-ms-date: %s", osDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("x-ms-version: %s", osMsVersion.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    return headers;
}

/************************************************************************/
/*                     VSIAzureBlobHandleHelper()                       */
/************************************************************************/
VSIAzureBlobHandleHelper::VSIAzureBlobHandleHelper(
                                            const CPLString& osEndpoint,
                                            const CPLString& osBucket,
                                            const CPLString& osObjectKey,
                                            const CPLString& osStorageAccount,
                                            const CPLString& osStorageKey,
                                            bool bUseHTTPS ) :
    m_osURL(BuildURL(osEndpoint, osStorageAccount,
            osBucket, osObjectKey, bUseHTTPS)),
    m_osEndpoint(osEndpoint),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey),
    m_osStorageAccount(osStorageAccount),
    m_osStorageKey(osStorageKey),
    m_bUseHTTPS(bUseHTTPS)
{
}

/************************************************************************/
/*                     ~VSIAzureBlobHandleHelper()                      */
/************************************************************************/

VSIAzureBlobHandleHelper::~VSIAzureBlobHandleHelper()
{
}


/************************************************************************/
/*                       AzureCSGetParameter()                          */
/************************************************************************/

static
CPLString AzureCSGetParameter(const CPLString& osStr, const char* pszKey,
                              bool bErrorIfMissing)
{
    CPLString osKey(pszKey + CPLString("="));
    size_t nPos = osStr.find(osKey);
    if( nPos == std::string::npos )
    {
        const char* pszMsg = CPLSPrintf(
            "%s missing in AZURE_STORAGE_CONNECTION_STRING", pszKey);
        CPLDebug("AZURE", "%s", pszMsg);
        if( bErrorIfMissing )
        {
            VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
        }
        return CPLString();
    }
    size_t nPos2 = osStr.find(";", nPos);
    return osStr.substr(
        nPos + osKey.size(),
        nPos2 == std::string::npos ? nPos2 : nPos2 - nPos - osKey.size());
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIAzureBlobHandleHelper::GetConfiguration(CSLConstList papszOptions,
                                                bool& bUseHTTPS,
                                                CPLString& osEndpoint,
                                                CPLString& osStorageAccount,
                                                CPLString& osStorageKey)
{
    bUseHTTPS = CPLTestBool(CPLGetConfigOption("CPL_AZURE_USE_HTTPS", "YES"));
    osEndpoint =
        CPLGetConfigOption("CPL_AZURE_ENDPOINT",
                                    "blob.core.windows.net");

    const CPLString osStorageConnectionString(
        CSLFetchNameValueDef(papszOptions, "AZURE_STORAGE_CONNECTION_STRING",
        CPLGetConfigOption("AZURE_STORAGE_CONNECTION_STRING", "")));
    if( !osStorageConnectionString.empty() )
    {
        osStorageAccount = AzureCSGetParameter(osStorageConnectionString,
                                               "AccountName", true);
        osStorageKey = AzureCSGetParameter(osStorageConnectionString,
                                               "AccountKey", true);
        if( osStorageAccount.empty() || osStorageKey.empty() )
            return false;

        CPLString osProtocol(AzureCSGetParameter(
            osStorageConnectionString, "DefaultEndpointsProtocol", false));
        bUseHTTPS = (osProtocol != "http");

        CPLString osEndpointSuffix(AzureCSGetParameter(
            osStorageConnectionString, "EndpointSuffix", false));
        if( STARTS_WITH(osEndpointSuffix, "127.0.0.1") )
            osEndpoint = osEndpointSuffix;
        else if( !osEndpointSuffix.empty() )
            osEndpoint = "blob." + osEndpointSuffix;

        return true;
    }
    else
    {
        osStorageAccount = CSLFetchNameValueDef(papszOptions,
            "AZURE_STORAGE_ACCOUNT",
            CPLGetConfigOption("AZURE_STORAGE_ACCOUNT", ""));
        if( !osStorageAccount.empty() )
        {
            osStorageKey = CSLFetchNameValueDef(papszOptions,
                "AZURE_STORAGE_ACCESS_KEY",
                CPLGetConfigOption("AZURE_STORAGE_ACCESS_KEY", ""));
            if( osStorageKey.empty() )
            {
                const char* pszMsg =
                    "AZURE_STORAGE_ACCESS_KEY configuration option "
                    "not defined";
                CPLDebug("AZURE", "%s", pszMsg);
                VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
                return false;
            }
            return true;
        }
    }
    const char* pszMsg = "Missing AZURE_STORAGE_ACCOUNT+"
                         "AZURE_STORAGE_ACCESS_KEY or "
                         "AZURE_STORAGE_CONNECTION_STRING "
                         "configuration options";
    CPLDebug("AZURE", "%s", pszMsg);
    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
    return false;
}


/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIAzureBlobHandleHelper* VSIAzureBlobHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* /*pszFSPrefix*/,
                                                    CSLConstList papszOptions )
{
    bool bUseHTTPS = true;
    CPLString osStorageAccount;
    CPLString osStorageKey;
    CPLString osEndpoint;

    if( !GetConfiguration(papszOptions,
                    bUseHTTPS, osEndpoint, osStorageAccount, osStorageKey) )
    {
        return nullptr;
    }

    // pszURI == bucket/object
    const CPLString osBucketObject( pszURI );
    CPLString osBucket(osBucketObject);
    CPLString osObjectKey;
    size_t nSlashPos = osBucketObject.find('/');
    if( nSlashPos != std::string::npos )
    {
        osBucket = osBucketObject.substr(0, nSlashPos);
        osObjectKey = osBucketObject.substr(nSlashPos+1);
    }

    return new VSIAzureBlobHandleHelper( osEndpoint,
                                  osBucket,
                                  osObjectKey,
                                  osStorageAccount,
                                  osStorageKey,
                                  bUseHTTPS );
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

CPLString VSIAzureBlobHandleHelper::BuildURL(const CPLString& osEndpoint,
                                             const CPLString& osStorageAccount,
                                             const CPLString& osBucket,
                                             const CPLString& osObjectKey,
                                             bool bUseHTTPS)
{
    CPLString osURL = (bUseHTTPS) ? "https://" : "http://";
    if( STARTS_WITH(osEndpoint, "127.0.0.1") )
    {
        osURL += osEndpoint + "/azure/blob/" + osStorageAccount;
    }
    else
    {
        osURL += osStorageAccount + "." + osEndpoint;
    }
    osURL += "/";
    osURL += CPLAWSURLEncode(osBucket,false);
    if( !osObjectKey.empty() )
        osURL += "/" + CPLAWSURLEncode(osObjectKey,false);
    return osURL;
}


/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIAzureBlobHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint, m_osStorageAccount, m_osBucket,
                       m_osObjectKey, m_bUseHTTPS);
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIAzureBlobHandleHelper::GetCurlHeaders( const CPLString& osVerb,
                                          const struct curl_slist* psExistingHeaders,
                                          const void *,
                                          size_t ) const
{
    CPLString osResource("/" + m_osBucket);
    if( !m_osObjectKey.empty() )
        osResource += "/" + CPLAWSURLEncode(m_osObjectKey,false);

    return GetAzureBlobHeaders( osVerb,
                                psExistingHeaders,
                                osResource,
                                m_oMapQueryParameters,
                                m_osStorageAccount,
                                m_osStorageKey );
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

CPLString VSIAzureBlobHandleHelper::GetSignedURL(CSLConstList papszOptions)
{
    CPLString osStartDate(CPLGetAWS_SIGN4_Timestamp());
    const char* pszStartDate = CSLFetchNameValue(papszOptions, "START_DATE");
    if( pszStartDate )
        osStartDate = pszStartDate;
    int nYear, nMonth, nDay, nHour = 0, nMin = 0, nSec = 0;
    if( sscanf(osStartDate, "%04d%02d%02dT%02d%02d%02dZ",
                &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec) < 3 )
    {
        return CPLString();
    }
    osStartDate = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                nYear, nMonth, nDay, nHour, nMin, nSec);

    struct tm brokendowntime;
    brokendowntime.tm_year = nYear - 1900;
    brokendowntime.tm_mon = nMonth - 1;
    brokendowntime.tm_mday = nDay;
    brokendowntime.tm_hour = nHour;
    brokendowntime.tm_min = nMin;
    brokendowntime.tm_sec = nSec;
    GIntBig nStartDate = CPLYMDHMSToUnixTime(&brokendowntime);
    GIntBig nEndDate = nStartDate + atoi(
        CSLFetchNameValueDef(papszOptions, "EXPIRATION_DELAY", "3600"));
    CPLUnixTimeToYMDHMS(nEndDate, &brokendowntime);
    nYear = brokendowntime.tm_year + 1900;
    nMonth = brokendowntime.tm_mon + 1;
    nDay = brokendowntime.tm_mday;
    nHour = brokendowntime.tm_hour;
    nMin = brokendowntime.tm_min;
    nSec = brokendowntime.tm_sec;
    CPLString osEndDate = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                nYear, nMonth, nDay, nHour, nMin, nSec);

    CPLString osVerb(CSLFetchNameValueDef(papszOptions, "VERB", "GET"));
    CPLString osSignedPermissions(CSLFetchNameValueDef(papszOptions,
        "SIGNEDPERMISSIONS", 
        (EQUAL(osVerb, "GET") || EQUAL(osVerb, "HEAD")) ? "r" : "w"  ));

    CPLString osSignedIdentifier(CSLFetchNameValueDef(papszOptions,
                                                     "SIGNEDIDENTIFIER", ""));

    CPLString osStringToSign;
    osStringToSign += osSignedPermissions + "\n";
    osStringToSign += osStartDate + "\n";
    osStringToSign += osEndDate + "\n";
    osStringToSign += "/" + m_osStorageAccount + "/" + m_osBucket + "\n";
    osStringToSign += osSignedIdentifier + "\n";
    osStringToSign += "2012-02-12";

#ifdef DEBUG_VERBOSE
    CPLDebug("AZURE", "osStringToSign = %s", osStringToSign.c_str());
#endif

/* -------------------------------------------------------------------- */
/*      Compute signature.                                              */
/* -------------------------------------------------------------------- */
    CPLString osSignature(GetSignature(osStringToSign, m_osStorageKey));

    ResetQueryParameters();
    AddQueryParameter("sv", "2012-02-12");
    AddQueryParameter("st", osStartDate);
    AddQueryParameter("se", osEndDate);
    AddQueryParameter("sr", "c");
    AddQueryParameter("sp", osSignedPermissions);
    AddQueryParameter("sig", osSignature);
    if( !osSignedIdentifier.empty() )
        AddQueryParameter("si", osSignedIdentifier);
    return m_osURL;
}


#endif

//! @endcond
