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
/*                           GetHeaderVal()                             */
/************************************************************************/

static CPLString GetHeaderVal(const struct curl_slist* psExistingHeaders,
                              const char* pszKey)
{
    CPLString osKey(pszKey);
    osKey += ":";
    const struct curl_slist* psIter = psExistingHeaders;
    for(; psIter != NULL; psIter = psIter->next)
    {
        if( STARTS_WITH(psIter->data, osKey.c_str()) )
            return CPLString(psIter->data + osKey.size()).Trim();
    }
    return CPLString();
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
        char szDate[64];
        time_t nNow = time(NULL);
        struct tm tm;
        CPLUnixTimeToYMDHMS(nNow, &tm);
        strftime(szDate, sizeof(szDate), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        osDate = szDate;
    }

    std::map<CPLString, CPLString> oSortedMapMSHeaders;
    const struct curl_slist* psIter = psExistingHeaders;
    for(; psIter != NULL; psIter = psIter->next)
    {
        if( STARTS_WITH(psIter->data, "x-ms-") )
        {
            const char* pszColumn = strstr(psIter->data, ":");
            if( pszColumn )
            {
                CPLString osKey(psIter->data);
                osKey.resize( pszColumn - psIter->data);
                oSortedMapMSHeaders[osKey] = CPLString(pszColumn + strlen(":")).Trim();
            }
        }
    }
    CPLString osMsVersion("2015-02-21");
    oSortedMapMSHeaders["x-ms-version"] = osMsVersion;
    oSortedMapMSHeaders["x-ms-date"] = osDate;
    CPLString osCanonicalizedHeaders;
    std::map<CPLString, CPLString>::const_iterator oIter = oSortedMapMSHeaders.begin();
    for(; oIter != oSortedMapMSHeaders.end(); ++oIter )
    {
        osCanonicalizedHeaders += oIter->first + ":" + oIter->second + "\n";
    }

    CPLString osCanonicalizedResource;
    osCanonicalizedResource += "/" + osStorageAccount;
    osCanonicalizedResource += osResource;

    // We assume query parameters are in lower case and they are not repeated
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
    osStringToSign += GetHeaderVal(psExistingHeaders, "Content-Encoding") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "Content-Language") + "\n";
    CPLString osContentLength(GetHeaderVal(psExistingHeaders, "Content-Length"));
    if( osContentLength == "0" )
        osContentLength.clear(); // since x-ms-version 2015-02-21
    osStringToSign += osContentLength + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "Date") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "If-Modified-Since") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "If-Match") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "If-None-Match") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "If-Unmodified-Since") + "\n";
    osStringToSign += GetHeaderVal(psExistingHeaders, "Range") + "\n";
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
    osURL += osBucket;
    if( !osObjectKey.empty() )
        osURL += "/" + osObjectKey;
    return osURL;
}


/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIAzureBlobHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint, m_osStorageAccount, m_osBucket,
                       m_osObjectKey, m_bUseHTTPS);

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
/*                       ResetQueryParameters()                         */
/************************************************************************/

void VSIAzureBlobHandleHelper::ResetQueryParameters()
{
    m_oMapQueryParameters.clear();
    RebuildURL();
}

/************************************************************************/
/*                         AddQueryParameter()                          */
/************************************************************************/

void VSIAzureBlobHandleHelper::AddQueryParameter( const CPLString& osKey,
                                           const CPLString& osValue )
{
    m_oMapQueryParameters[osKey] = osValue;
    RebuildURL();
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
        osResource += "/" + m_osObjectKey;

    return GetAzureBlobHeaders( osVerb,
                                psExistingHeaders,
                                osResource,
                                m_oMapQueryParameters,
                                m_osStorageAccount,
                                m_osStorageKey );
}


#endif

//! @endcond
