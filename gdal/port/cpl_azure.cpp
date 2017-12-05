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
        char szDate[64];
        time_t nNow = time(NULL);
        struct tm tm;
        CPLUnixTimeToYMDHMS(nNow, &tm);
        strftime(szDate, sizeof(szDate), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        osDate = szDate;
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
    CPLString osAuthorization("SharedKey " + osStorageAccount + ":" + pszB64Signature);
    CPLFree(pszB64Signature);

    struct curl_slist *headers=NULL;
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

bool VSIAzureBlobHandleHelper::GetConfiguration(bool& bUseHTTPS,
                                                CPLString& osEndpoint,
                                                CPLString& osStorageAccount,
                                                CPLString& osStorageKey)
{
    bUseHTTPS = CPLTestBool(CPLGetConfigOption("CPL_AZURE_USE_HTTPS", "YES"));
    osEndpoint = 
        CPLGetConfigOption("CPL_AZURE_ENDPOINT",
                                    "blob.core.windows.net");

    const CPLString osStorageConnectionString(
        CPLGetConfigOption("AZURE_STORAGE_CONNECTION_STRING", ""));
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
        osStorageAccount =
            CPLGetConfigOption("AZURE_STORAGE_ACCOUNT", "");
        if( !osStorageAccount.empty() )
        {
            osStorageKey =
                CPLGetConfigOption("AZURE_STORAGE_ACCESS_KEY", "");
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
                                                    const char* /*pszFSPrefix*/ )
{
    bool bUseHTTPS = true;
    CPLString osStorageAccount;
    CPLString osStorageKey;
    CPLString osEndpoint;

    if( !GetConfiguration(bUseHTTPS, osEndpoint, osStorageAccount, osStorageKey) )
    {
        return NULL;
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
    m_osURL += GetQueryString();
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


#endif

//! @endcond
