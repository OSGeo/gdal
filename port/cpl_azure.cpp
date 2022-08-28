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

#include <mutex>

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
    if( osStorageKeyB64.empty() )
    {
        struct curl_slist *headers=nullptr;
        headers = curl_slist_append(
            headers, CPLSPrintf("x-ms-date: %s", osDate.c_str()));
        return headers;
    }

    CPLString osMsVersion("2019-12-12");
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
                                            const CPLString& osSAS,
                                            const CPLString& osAccessToken,
                                            bool bFromManagedIdentities ) :
    m_osURL(BuildURL(osEndpoint,
            osBucket, osObjectKey, osSAS)),
    m_osEndpoint(osEndpoint),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey),
    m_osStorageAccount(osStorageAccount),
    m_osStorageKey(osStorageKey),
    m_osSAS(osSAS),
    m_osAccessToken(osAccessToken),
    m_bFromManagedIdentities(bFromManagedIdentities)
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
        if( bErrorIfMissing )
        {
            CPLDebug("AZURE", "%s", pszMsg);
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
/*                GetConfigurationFromManagedIdentities()               */
/************************************************************************/

std::mutex gMutex;
static CPLString gosAccessToken;
static GIntBig gnGlobalExpiration = 0;


static bool GetConfigurationFromManagedIdentities(CPLString& osAccessToken)
{
    std::lock_guard<std::mutex> guard(gMutex);
    time_t nCurTime;
    time(&nCurTime);
    // Try to reuse credentials if they are still valid, but
    // keep one minute of margin...
    if( !gosAccessToken.empty() && nCurTime < gnGlobalExpiration - 60 )
    {
        osAccessToken = gosAccessToken;
        return true;
    }

    // coverity[tainted_data]
    const CPLString osRootURL(
        CPLGetConfigOption("CPL_AZURE_VM_API_ROOT_URL", "http://169.254.169.254"));
    if( osRootURL == "disabled" )
        return false;

    // Fetch credentials
    CPLStringList oResponse;
    const char* const apszOptions[] = { "HEADERS=Metadata: true", nullptr };
    CPLHTTPResult* psResult = CPLHTTPFetch(
        (osRootURL + "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F").c_str(),
        apszOptions );
    if( psResult )
    {
        if( psResult->nStatus == 0 && psResult->pabyData != nullptr )
        {
            const CPLString osJSon =
                    reinterpret_cast<char*>(psResult->pabyData);
            oResponse = ParseSimpleJson(osJSon);
            if( oResponse.FetchNameValue("error") )
            {
                CPLDebug("AZURE", "Cannot retrieve managed identities credentials: %s",
                         osJSon.c_str());
            }
        }
        CPLHTTPDestroyResult(psResult);
    }
    osAccessToken = oResponse.FetchNameValueDef("access_token", "");
    const GIntBig nExpiresOn = CPLAtoGIntBig(oResponse.FetchNameValueDef("expires_on", ""));
    if( !osAccessToken.empty() && nExpiresOn > 0 )
    {
        gosAccessToken = osAccessToken;
        gnGlobalExpiration = nExpiresOn;
        CPLDebug("AZURE", "Storing credentials until " CPL_FRMT_GIB,
                 gnGlobalExpiration);
    }

    return !osAccessToken.empty();
}

/************************************************************************/
/*                             ClearCache()                             */
/************************************************************************/

void VSIAzureBlobHandleHelper::ClearCache()
{
    std::lock_guard<std::mutex> guard(gMutex);
    gosAccessToken.clear();
    gnGlobalExpiration = 0;
}

/************************************************************************/
/*                    ParseStorageConnectionString()                    */
/************************************************************************/

static bool ParseStorageConnectionString(const std::string& osStorageConnectionString,
                                         const std::string& osServicePrefix,
                                         bool& bUseHTTPS,
                                         CPLString& osEndpoint,
                                         CPLString& osStorageAccount,
                                         CPLString& osStorageKey)
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

    CPLString osBlobEndpoint = AzureCSGetParameter(
        osStorageConnectionString, "BlobEndpoint", false);
    if( !osBlobEndpoint.empty() )
    {
        osEndpoint = osBlobEndpoint;
    }
    else
    {
        CPLString osEndpointSuffix(AzureCSGetParameter(
            osStorageConnectionString, "EndpointSuffix", false));
        if( !osEndpointSuffix.empty() )
            osEndpoint = (bUseHTTPS ? "https://" : "http://") + osStorageAccount + "." + osServicePrefix + "." + osEndpointSuffix;
    }

    return true;
}

/************************************************************************/
/*                 GetConfigurationFromCLIConfigFile()                  */
/************************************************************************/

static bool GetConfigurationFromCLIConfigFile(const std::string& osServicePrefix,
                                              bool& bUseHTTPS,
                                              CPLString& osEndpoint,
                                              CPLString& osStorageAccount,
                                              CPLString& osStorageKey,
                                              CPLString& osSAS,
                                              CPLString& osAccessToken,
                                              bool& bFromManagedIdentities)
{
#ifdef _WIN32
    const char* pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
    constexpr char SEP_STRING[] = "\\";
#else
    const char* pszHome = CPLGetConfigOption("HOME", nullptr);
    constexpr char SEP_STRING[] = "/";
#endif

    std::string osDotAzure( pszHome ? pszHome : "" );
    osDotAzure += SEP_STRING;
    osDotAzure += ".azure";

    const char* pszAzureConfigDir = CPLGetConfigOption("AZURE_CONFIG_DIR",
                                                       osDotAzure.c_str());
    if( pszAzureConfigDir[0] == '\0' )
        return false;

    std::string osConfigFilename = pszAzureConfigDir;
    osConfigFilename += SEP_STRING;
    osConfigFilename += "config";

    VSILFILE* fp = VSIFOpenL( osConfigFilename.c_str(), "rb" );
    std::string osStorageConnectionString;
    if( fp == nullptr )
        return false;

    bool bInStorageSection = false;
    while( const char* pszLine = CPLReadLineL(fp) )
    {
        if( pszLine[0] == '#' || pszLine[0] == ';' )
        {
            // comment line
        }
        else if( strcmp(pszLine, "[storage]") == 0 )
        {
            bInStorageSection = true;
        }
        else if( pszLine[0] == '[' )
        {
            bInStorageSection = false;
        }
        else if( bInStorageSection )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
            if( pszKey && pszValue )
            {
                if( EQUAL(pszKey, "account") )
                {
                    osStorageAccount = pszValue;
                }
                else if( EQUAL(pszKey, "connection_string") )
                {
                    osStorageConnectionString = pszValue;
                }
                else if( EQUAL(pszKey, "key") )
                {
                    osStorageKey = pszValue;
                }
                else if( EQUAL(pszKey, "sas_token") )
                {
                    osSAS = pszValue;
                    // Az CLI apparently uses configparser with BasicInterpolation
                    // where the % character has a special meaning
                    // See https://docs.python.org/3/library/configparser.html#configparser.BasicInterpolation
                    // A token might end with %%3D which must be transformed to %3D
                    osSAS.replaceAll("%%", '%');
                }
            }
            CPLFree(pszKey);
        }
    }
    VSIFCloseL(fp);

    if( !osStorageConnectionString.empty() )
    {
        return ParseStorageConnectionString(osStorageConnectionString,
                                            osServicePrefix,
                                            bUseHTTPS,
                                            osEndpoint,
                                            osStorageAccount,
                                            osStorageKey);
    }

    if( osStorageAccount.empty() )
    {
        CPLDebug("AZURE", "Missing storage.account in %s",
                 osConfigFilename.c_str());
        return false;
    }

    if( osEndpoint.empty() )
        osEndpoint = (bUseHTTPS ? "https://" : "http://") + osStorageAccount + "." + osServicePrefix + ".core.windows.net";

    osAccessToken = CPLGetConfigOption("AZURE_STORAGE_ACCESS_TOKEN", "");
    if( !osAccessToken.empty() )
        return true;

    if( osStorageKey.empty() && osSAS.empty() )
    {
        if( CPLTestBool(CPLGetConfigOption("AZURE_NO_SIGN_REQUEST", "NO")) )
        {
            return true;
        }

        CPLString osTmpAccessToken;
        if( GetConfigurationFromManagedIdentities(osTmpAccessToken) )
        {
            bFromManagedIdentities = true;
            return true;
        }

        CPLDebug("AZURE", "Missing storage.key or storage.sas_token in %s",
                 osConfigFilename.c_str());
        return false;
    }

    return true;
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIAzureBlobHandleHelper::GetConfiguration(const std::string& osPathForOption,
                                                CSLConstList papszOptions,
                                                Service eService,
                                                bool& bUseHTTPS,
                                                CPLString& osEndpoint,
                                                CPLString& osStorageAccount,
                                                CPLString& osStorageKey,
                                                CPLString& osSAS,
                                                CPLString& osAccessToken,
                                                bool& bFromManagedIdentities)
{
    bFromManagedIdentities = false;

    const CPLString osServicePrefix ( eService == Service::SERVICE_BLOB ? "blob" : "dfs" );
    bUseHTTPS = CPLTestBool(VSIGetCredential(
        osPathForOption.c_str(), "CPL_AZURE_USE_HTTPS", "YES"));
    osEndpoint = VSIGetCredential(
        osPathForOption.c_str(), "CPL_AZURE_ENDPOINT", "");

    const CPLString osStorageConnectionString(
        CSLFetchNameValueDef(papszOptions, "AZURE_STORAGE_CONNECTION_STRING",
        VSIGetCredential(osPathForOption.c_str(), "AZURE_STORAGE_CONNECTION_STRING", "")));
    if( !osStorageConnectionString.empty() )
    {
        return ParseStorageConnectionString(osStorageConnectionString,
                                            osServicePrefix,
                                            bUseHTTPS,
                                            osEndpoint,
                                            osStorageAccount,
                                            osStorageKey);
    }
    else
    {
        osStorageAccount = CSLFetchNameValueDef(papszOptions,
            "AZURE_STORAGE_ACCOUNT",
            VSIGetCredential(osPathForOption.c_str(), "AZURE_STORAGE_ACCOUNT", ""));
        if( !osStorageAccount.empty() )
        {
            if( osEndpoint.empty() )
                osEndpoint = (bUseHTTPS ? "https://" : "http://") + osStorageAccount + "." + osServicePrefix + ".core.windows.net";

            osAccessToken = CSLFetchNameValueDef(papszOptions,
                "AZURE_STORAGE_ACCESS_TOKEN",
                VSIGetCredential(osPathForOption.c_str(), "AZURE_STORAGE_ACCESS_TOKEN", ""));
            if( !osAccessToken.empty() )
                return true;

            osStorageKey = CSLFetchNameValueDef(papszOptions,
                "AZURE_STORAGE_ACCESS_KEY",
                VSIGetCredential(osPathForOption.c_str(), "AZURE_STORAGE_ACCESS_KEY", ""));
            if( osStorageKey.empty() )
            {
                osSAS = VSIGetCredential(osPathForOption.c_str(),
                                         "AZURE_STORAGE_SAS_TOKEN",
                                         CPLGetConfigOption("AZURE_SAS", "")); // AZURE_SAS for GDAL < 3.5
                if( osSAS.empty() )
                {
                    if( CPLTestBool(VSIGetCredential(
                            osPathForOption.c_str(), "AZURE_NO_SIGN_REQUEST", "NO")) )
                    {
                        return true;
                    }

                    CPLString osTmpAccessToken;
                    if( GetConfigurationFromManagedIdentities(osTmpAccessToken) )
                    {
                        bFromManagedIdentities = true;
                        return true;
                    }

                    const char* pszMsg =
                        "AZURE_STORAGE_ACCESS_KEY or AZURE_STORAGE_SAS_TOKEN or AZURE_NO_SIGN_REQUEST configuration option "
                        "not defined";
                    CPLDebug("AZURE", "%s", pszMsg);
                    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
                    return false;
                }
            }
            return true;
        }
    }

    if( GetConfigurationFromCLIConfigFile(osServicePrefix,
                                          bUseHTTPS,
                                          osEndpoint,
                                          osStorageAccount,
                                          osStorageKey,
                                          osSAS,
                                          osAccessToken,
                                          bFromManagedIdentities) )
    {
        return true;
    }

    const char* pszMsg = "Missing AZURE_STORAGE_ACCOUNT+"
                         "(AZURE_STORAGE_ACCESS_KEY or AZURE_STORAGE_SAS_TOKEN or AZURE_NO_SIGN_REQUEST) or "
                         "AZURE_STORAGE_CONNECTION_STRING "
                         "configuration options or Azure CLI configuration file";
    CPLDebug("AZURE", "%s", pszMsg);
    VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
    return false;
}


/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIAzureBlobHandleHelper* VSIAzureBlobHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* pszFSPrefix,
                                                    CSLConstList papszOptions )
{
    if( strcmp(pszFSPrefix, "/vsiaz/") != 0 &&
        strcmp(pszFSPrefix, "/vsiaz_streaming/") != 0 &&
        strcmp(pszFSPrefix, "/vsiadls/") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported FS prefix");
        return nullptr;
    }

    const auto eService = strcmp(pszFSPrefix, "/vsiaz/") == 0 ||
                          strcmp(pszFSPrefix, "/vsiaz_streaming/") == 0 ?
                                                Service::SERVICE_BLOB : Service::SERVICE_ADLS;

    std::string osPathForOption(eService == Service::SERVICE_BLOB ? "/vsiaz/" : "/vsiadls/");
    osPathForOption += pszURI;

    bool bUseHTTPS = true;
    CPLString osStorageAccount;
    CPLString osStorageKey;
    CPLString osEndpoint;
    CPLString osSAS;
    CPLString osAccessToken;
    bool bFromManagedIdentities = false;

    if( !GetConfiguration(osPathForOption, papszOptions, eService,
                    bUseHTTPS, osEndpoint,
                    osStorageAccount, osStorageKey, osSAS,
                    osAccessToken,
                    bFromManagedIdentities) )
    {
        return nullptr;
    }

    if( CPLTestBool(VSIGetCredential(
            osPathForOption.c_str(), "AZURE_NO_SIGN_REQUEST", "NO")) )
    {
        osStorageKey.clear();
        osSAS.clear();
        osAccessToken.clear();
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
                                  osSAS,
                                  osAccessToken,
                                  bFromManagedIdentities );
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

CPLString VSIAzureBlobHandleHelper::BuildURL(const CPLString& osEndpoint,
                                             const CPLString& osBucket,
                                             const CPLString& osObjectKey,
                                             const CPLString& osSAS)
{
    CPLString osURL = osEndpoint;
    osURL += "/";
    osURL += CPLAWSURLEncode(osBucket,false);
    if( !osObjectKey.empty() )
        osURL += "/" + CPLAWSURLEncode(osObjectKey,false);
    if( !osSAS.empty() )
        osURL += '?' + osSAS;
    return osURL;
}


/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIAzureBlobHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osEndpoint,
                       m_osBucket, m_osObjectKey, CPLString());
    m_osURL += GetQueryString(false);
    if( !m_osSAS.empty() )
        m_osURL += (m_oMapQueryParameters.empty() ? '?' : '&') + m_osSAS;
}

/************************************************************************/
/*                        GetSASQueryString()                           */
/************************************************************************/

std::string VSIAzureBlobHandleHelper::GetSASQueryString() const
{
    if( !m_osSAS.empty() )
        return '?' + m_osSAS;
    return std::string();
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
    if( m_bFromManagedIdentities || !m_osAccessToken.empty() )
    {
        CPLString osAccessToken;
        if( m_bFromManagedIdentities )
        {
            if( !GetConfigurationFromManagedIdentities(osAccessToken) )
                return nullptr;
        }
        else
        {
            osAccessToken = m_osAccessToken;
        }

        struct curl_slist *headers=nullptr;
        headers = curl_slist_append(
            headers, CPLSPrintf("Authorization: Bearer %s", osAccessToken.c_str()));
        headers = curl_slist_append(headers, "x-ms-version: 2019-12-12");
        return headers;
    }

    CPLString osResource;
    const auto nSlashSlashPos = m_osEndpoint.find("//");
    if( nSlashSlashPos != std::string::npos )
    {
        const auto nResourcePos = m_osEndpoint.find('/', nSlashSlashPos + 2);
        if( nResourcePos != std::string::npos )
            osResource = m_osEndpoint.substr(nResourcePos);
    }
    osResource += "/" + m_osBucket;
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
    if( m_osStorageKey.empty() )
        return m_osURL;

    CPLString osStartDate(CPLGetAWS_SIGN4_Timestamp(time(nullptr)));
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


#endif // HAVE_CURL

//! @endcond
