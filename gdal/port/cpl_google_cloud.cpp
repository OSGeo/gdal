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

#include "cpl_google_cloud.h"
#include "cpl_vsi_error.h"
#include "cpl_sha1.h"
#include "cpl_sha256.h"
#include "cpl_time.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_aws.h"
#include "cpl_json.h"

CPL_CVSID("$Id$")

#ifdef HAVE_CURL

static CPLMutex *hMutex = nullptr;
static bool bFirstTimeForDebugMessage = true;
static GOA2Manager oStaticManager;

/************************************************************************/
/*                    CPLIsMachineForSureGCEInstance()                  */
/************************************************************************/

/** Returns whether the current machine is surely a Google Compute Engine instance.
 *
 * This does a very quick check without network access.
 * Note: only works for Linux GCE instances.
 *
 * @return true if the current machine is surely a GCE instance.
 * @since GDAL 2.3
 */
bool CPLIsMachineForSureGCEInstance()
{
    if( CPLTestBool(CPLGetConfigOption(
                        "CPL_MACHINE_IS_GCE", "NO")) )
    {
        return true;
    }
#ifdef __linux
    // If /sys/class/dmi/id/product_name exists, it contains "Google Compute Engine"
    bool bIsGCEInstance = false;
    if( CPLTestBool(CPLGetConfigOption(
                        "CPL_GCE_CHECK_LOCAL_FILES", "YES")) )
    {
        static bool bIsGCEInstanceStatic = false;
        static bool bDone = false;
        {
            CPLMutexHolder oHolder( &hMutex );
            if( !bDone )
            {
                bDone = true;

                VSILFILE* fp = VSIFOpenL("/sys/class/dmi/id/product_name", "rb");
                if( fp )
                {
                    const char* pszLine = CPLReadLineL(fp);
                    bIsGCEInstanceStatic = pszLine &&
                        STARTS_WITH_CI(pszLine, "Google Compute Engine");
                    VSIFCloseL(fp);
                }
            }
        }
        bIsGCEInstance = bIsGCEInstanceStatic;
    }
    return bIsGCEInstance;
#else
    return false;
#endif
}

/************************************************************************/
/*                 CPLIsMachinePotentiallyGCEInstance()                 */
/************************************************************************/

/** Returns whether the current machine is potentially a Google Compute Engine instance.
 *
 * This does a very quick check without network access. To confirm if the
 * machine is effectively a GCE instance, metadata.google.internal must be
 * queried.
 *
 * @return true if the current machine is potentially a GCE instance.
 * @since GDAL 2.3
 */
bool CPLIsMachinePotentiallyGCEInstance()
{
#ifdef __linux
    bool bIsMachinePotentialGCEInstance = true;
    if( CPLTestBool(CPLGetConfigOption(
                        "CPL_GCE_CHECK_LOCAL_FILES", "YES")) )
    {
        bIsMachinePotentialGCEInstance = CPLIsMachineForSureGCEInstance();
    }
    return bIsMachinePotentialGCEInstance;
#elif defined(WIN32)
    // We might add later a way of detecting if we run on GCE using WMI
    // See https://cloud.google.com/compute/docs/instances/managing-instances
    // For now, unconditionally try
    return true;
#else
    // At time of writing GCE instances can be only Linux or Windows
    return false;
#endif
}


//! @cond Doxygen_Suppress


/************************************************************************/
/*                            GetGSHeaders()                            */
/************************************************************************/

static
struct curl_slist* GetGSHeaders( const CPLString& osVerb,
                                 const struct curl_slist* psExistingHeaders,
                                 const CPLString& osCanonicalResource,
                                 const CPLString& osSecretAccessKey,
                                 const CPLString& osAccessKeyId,
                                 const std::string& osUserProject )
{
    if( osSecretAccessKey.empty() )
    {
        // GS_NO_SIGN_REQUEST=YES case
        return nullptr;
    }

    CPLString osDate = CPLGetConfigOption("CPL_GS_TIMESTAMP", "");
    if( osDate.empty() )
    {
        osDate = IVSIS3LikeHandleHelper::GetRFC822DateTime();
    }

    std::map<CPLString, CPLString> oSortedMapHeaders;
    if( !osUserProject.empty() )
        oSortedMapHeaders["x-goog-user-project"] = osUserProject;
    CPLString osCanonicalizedHeaders(
        IVSIS3LikeHandleHelper::BuildCanonicalizedHeaders(
                            oSortedMapHeaders,
                            psExistingHeaders,
                            "x-goog-"));

    // See https://cloud.google.com/storage/docs/migrating
    CPLString osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-MD5") + "\n";
    osStringToSign += CPLAWSGetHeaderVal(psExistingHeaders, "Content-Type") + "\n";
    osStringToSign += osDate + "\n";
    osStringToSign += osCanonicalizedHeaders;
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

    struct curl_slist *headers=nullptr;
    headers = curl_slist_append(
        headers, CPLSPrintf("Date: %s", osDate.c_str()));
    headers = curl_slist_append(
        headers, CPLSPrintf("Authorization: %s", osAuthorization.c_str()));
    if( !osUserProject.empty() )
    {
        headers = curl_slist_append(
            headers, CPLSPrintf("x-goog-user-project: %s", osUserProject.c_str()));
    }
    return headers;
}

/************************************************************************/
/*                         VSIGSHandleHelper()                          */
/************************************************************************/
VSIGSHandleHelper::VSIGSHandleHelper( const CPLString& osEndpoint,
                                      const CPLString& osBucketObjectKey,
                                      const CPLString& osSecretAccessKey,
                                      const CPLString& osAccessKeyId,
                                      bool bUseHeaderFile,
                                      const GOA2Manager& oManager,
                                      const std::string& osUserProject ) :
    m_osURL(osEndpoint + CPLAWSURLEncode(osBucketObjectKey, false)),
    m_osEndpoint(osEndpoint),
    m_osBucketObjectKey(osBucketObjectKey),
    m_osSecretAccessKey(osSecretAccessKey),
    m_osAccessKeyId(osAccessKeyId),
    m_bUseHeaderFile(bUseHeaderFile),
    m_oManager(oManager),
    m_osUserProject(osUserProject)
{
    if( m_osBucketObjectKey.find('/') == std::string::npos )
        m_osURL += "/";
}

/************************************************************************/
/*                        ~VSIGSHandleHelper()                          */
/************************************************************************/

VSIGSHandleHelper::~VSIGSHandleHelper()
{
}


/************************************************************************/
/*                GetConfigurationFromAWSConfigFiles()                  */
/************************************************************************/

bool VSIGSHandleHelper::GetConfigurationFromConfigFile(
                                                CPLString& osSecretAccessKey,
                                                CPLString& osAccessKeyId,
                                                CPLString& osOAuth2RefreshToken,
                                                CPLString& osOAuth2ClientId,
                                                CPLString& osOAuth2ClientSecret,
                                                CPLString& osCredentials)
{
#ifdef WIN32
    const char* pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
    constexpr char SEP_STRING[] = "\\";
#else
    const char* pszHome = CPLGetConfigOption("HOME", nullptr);
    constexpr char SEP_STRING[] = "/";
#endif

    // GDAL specific config option (mostly for testing purpose, but also
    // used in production in some cases)
    const char* pszCredentials =
                    CPLGetConfigOption( "CPL_GS_CREDENTIALS_FILE", nullptr);
    if( pszCredentials )
    {
        osCredentials = pszCredentials;
    }
    else
    {
        osCredentials = pszHome ? pszHome : "";
        osCredentials += SEP_STRING;
        osCredentials += ".boto";
    }

    VSILFILE* fp = VSIFOpenL( osCredentials, "rb" );
    if( fp != nullptr )
    {
        const char* pszLine;
        bool bInCredentials = false;
        bool bInOAuth2 = false;
        while( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            if( pszLine[0] == '[' )
            {
                bInCredentials = false;
                bInOAuth2 = false;

                if( CPLString(pszLine) == "[Credentials]" )
                    bInCredentials = true;
                else if( CPLString(pszLine) == "[OAuth2]" )
                    bInOAuth2 = true;
            }
            else if( bInCredentials )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "gs_access_key_id") )
                        osAccessKeyId = CPLString(pszValue).Trim();
                    else if( EQUAL(pszKey, "gs_secret_access_key") )
                        osSecretAccessKey = CPLString(pszValue).Trim();
                    else if( EQUAL(pszKey, "gs_oauth2_refresh_token") )
                        osOAuth2RefreshToken = CPLString(pszValue).Trim();
                }
                CPLFree(pszKey);
            }
            else if( bInOAuth2 )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "client_id") )
                        osOAuth2ClientId = CPLString(pszValue).Trim();
                    else if( EQUAL(pszKey, "client_secret") )
                        osOAuth2ClientSecret = CPLString(pszValue).Trim();
                }
                CPLFree(pszKey);
            }
        }
        VSIFCloseL(fp);
    }

    return (!osAccessKeyId.empty() && !osSecretAccessKey.empty()) ||
            !osOAuth2RefreshToken.empty();
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIGSHandleHelper::GetConfiguration(CSLConstList papszOptions,
                                         CPLString& osSecretAccessKey,
                                         CPLString& osAccessKeyId,
                                         CPLString& osHeaderFile,
                                         GOA2Manager& oManager)
{
    osSecretAccessKey.clear();
    osAccessKeyId.clear();
    osHeaderFile.clear();

    if( CPLTestBool(CPLGetConfigOption("GS_NO_SIGN_REQUEST", "NO")) )
    {
        return true;
    }

    osSecretAccessKey =
        CPLGetConfigOption("GS_SECRET_ACCESS_KEY", "");
    if( !osSecretAccessKey.empty() )
    {
        osAccessKeyId =
            CPLGetConfigOption("GS_ACCESS_KEY_ID", "");
        if( osAccessKeyId.empty() )
        {
            VSIError(VSIE_AWSInvalidCredentials,
                    "GS_ACCESS_KEY_ID configuration option not defined");
            bFirstTimeForDebugMessage = false;
            return false;
        }

        if( bFirstTimeForDebugMessage )
        {
            CPLDebug("GS",
                     "Using GS_SECRET_ACCESS_KEY and "
                     "GS_ACCESS_KEY_ID configuration options");
        }
        bFirstTimeForDebugMessage = false;
        return true;
    }

    osHeaderFile =
        CPLGetConfigOption("GDAL_HTTP_HEADER_FILE", "");
    bool bMayWarnDidNotFindAuth = false;
    if( !osHeaderFile.empty() )
    {
        bool bFoundAuth = false;
        VSILFILE *fp = nullptr;
        // Do not allow /vsicurl/ access from /vsicurl because of GetCurlHandleFor()
        // e.g. "/vsicurl/,HEADER_FILE=/vsicurl/,url= " would cause use of
        // memory after free
        if( strstr(osHeaderFile, "/vsicurl/") == nullptr &&
            strstr(osHeaderFile, "/vsicurl?") == nullptr &&
            strstr(osHeaderFile, "/vsis3/") == nullptr &&
            strstr(osHeaderFile, "/vsigs/") == nullptr &&
            strstr(osHeaderFile, "/vsiaz/") == nullptr &&
            strstr(osHeaderFile, "/vsioss/") == nullptr &&
            strstr(osHeaderFile, "/vsiswift/") == nullptr )
        {
            fp = VSIFOpenL( osHeaderFile, "rb" );
        }
        if( fp == nullptr )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot read %s", osHeaderFile.c_str());
        }
        else
        {
            const char* pszLine = nullptr;
            while( (pszLine = CPLReadLineL(fp)) != nullptr )
            {
                if( STARTS_WITH_CI(pszLine, "Authorization:") )
                {
                    bFoundAuth = true;
                    break;
                }
            }
            VSIFCloseL(fp);
            if( !bFoundAuth )
                bMayWarnDidNotFindAuth = true;
        }
        if( bFoundAuth )
        {
            if( bFirstTimeForDebugMessage )
            {
                CPLDebug("GS", "Using GDAL_HTTP_HEADER_FILE=%s",
                        osHeaderFile.c_str());
            }
            bFirstTimeForDebugMessage = false;
            return true;
        }
        else
        {
            osHeaderFile.clear();
        }
    }

    CPLString osRefreshToken( CPLGetConfigOption("GS_OAUTH2_REFRESH_TOKEN",
                                                 "") );
    if( !osRefreshToken.empty() )
    {
        if( oStaticManager.GetAuthMethod() ==
                                GOA2Manager::ACCESS_TOKEN_FROM_REFRESH )
        {
            CPLMutexHolder oHolder( &hMutex );
            oManager = oStaticManager;
            return true;
        }

        CPLString osClientId =
            CPLGetConfigOption("GS_OAUTH2_CLIENT_ID", "");
        CPLString osClientSecret =
            CPLGetConfigOption("GS_OAUTH2_CLIENT_SECRET", "");

        int nCount = (!osClientId.empty() ? 1 : 0) +
                     (!osClientSecret.empty() ? 1 : 0);
        if( nCount == 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Either both or none of GS_OAUTH2_CLIENT_ID and "
                     "GS_OAUTH2_CLIENT_SECRET must be set");
            return false;
        }

        if( bFirstTimeForDebugMessage )
        {
            CPLString osMsg(
                        "Using GS_OAUTH2_REFRESH_TOKEN configuration option");
            if( osClientId.empty() )
                osMsg += " and GDAL default client_id/client_secret";
            else
                osMsg += " and GS_OAUTH2_CLIENT_ID and GS_OAUTH2_CLIENT_SECRET";
            CPLDebug("GS", "%s", osMsg.c_str());
        }
        bFirstTimeForDebugMessage = false;

        return oManager.SetAuthFromRefreshToken(
            osRefreshToken, osClientId, osClientSecret, nullptr);
    }

    CPLString osServiceAccountJson( CSLFetchNameValueDef(papszOptions,
        "GOOGLE_APPLICATION_CREDENTIALS",
        CPLGetConfigOption("GOOGLE_APPLICATION_CREDENTIALS", "")));
    if( !osServiceAccountJson.empty() )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osServiceAccountJson) )
        {
            return false;
        }
        CPLString osPrivateKey = oDoc.GetRoot().GetString("private_key");
        osPrivateKey.replaceAll("\\n", "\n");
        CPLString osClientEmail = oDoc.GetRoot().GetString("client_email");
        const char* pszScope =
            CSLFetchNameValueDef( papszOptions, "GS_OAUTH2_SCOPE",
            CPLGetConfigOption("GS_OAUTH2_SCOPE",
                "https://www.googleapis.com/auth/devstorage.read_write"));

        return oManager.SetAuthFromServiceAccount(
                osPrivateKey,
                osClientEmail,
                pszScope,
                nullptr,
                nullptr);
    }

    CPLString osPrivateKey = CSLFetchNameValueDef(papszOptions,
        "GS_OAUTH2_PRIVATE_KEY",
        CPLGetConfigOption("GS_OAUTH2_PRIVATE_KEY", ""));
    CPLString osPrivateKeyFile = CSLFetchNameValueDef(papszOptions,
        "GS_OAUTH2_PRIVATE_KEY_FILE",
        CPLGetConfigOption("GS_OAUTH2_PRIVATE_KEY_FILE", ""));
    if( !osPrivateKey.empty() || !osPrivateKeyFile.empty() )
    {
        if( !osPrivateKeyFile.empty() )
        {
            VSILFILE* fp = VSIFOpenL(osPrivateKeyFile, "rb");
            if( fp == nullptr )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                        "Cannot open %s", osPrivateKeyFile.c_str());
                bFirstTimeForDebugMessage = false;
                return false;
            }
            else
            {
                char* pabyBuffer = static_cast<char*>(CPLMalloc(32768));
                size_t nRead = VSIFReadL(pabyBuffer, 1, 32768, fp);
                osPrivateKey.assign(pabyBuffer, nRead);
                VSIFCloseL(fp);
                CPLFree(pabyBuffer);
            }
        }
        osPrivateKey.replaceAll("\\n", "\n");

        CPLString osClientEmail = CSLFetchNameValueDef(papszOptions,
            "GS_OAUTH2_CLIENT_EMAIL",
            CPLGetConfigOption("GS_OAUTH2_CLIENT_EMAIL", ""));
        if( osClientEmail.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GS_OAUTH2_CLIENT_EMAIL not defined");
            bFirstTimeForDebugMessage = false;
            return false;
        }
        const char* pszScope =
            CSLFetchNameValueDef( papszOptions, "GS_OAUTH2_SCOPE",
            CPLGetConfigOption("GS_OAUTH2_SCOPE",
                "https://www.googleapis.com/auth/devstorage.read_write"));

        if( bFirstTimeForDebugMessage )
        {
            CPLDebug("GS",
                     "Using %s, GS_OAUTH2_CLIENT_EMAIL and GS_OAUTH2_SCOPE=%s "
                     "configuration options",
                     !osPrivateKeyFile.empty() ?
                        "GS_OAUTH2_PRIVATE_KEY_FILE" : "GS_OAUTH2_PRIVATE_KEY",
                     pszScope);
        }
        bFirstTimeForDebugMessage = false;

        return oManager.SetAuthFromServiceAccount(
                osPrivateKey,
                osClientEmail,
                pszScope,
                nullptr,
                nullptr);
    }

    // Next try reading from ~/.boto
    CPLString osCredentials;
    CPLString osOAuth2RefreshToken;
    CPLString osOAuth2ClientId;
    CPLString osOAuth2ClientSecret;
    if( GetConfigurationFromConfigFile(osSecretAccessKey,
                                       osAccessKeyId,
                                       osOAuth2RefreshToken,
                                       osOAuth2ClientId,
                                       osOAuth2ClientSecret,
                                       osCredentials) )
    {
        if( !osOAuth2RefreshToken.empty() )
        {
            if( oStaticManager.GetAuthMethod() ==
                                    GOA2Manager::ACCESS_TOKEN_FROM_REFRESH )
            {
                CPLMutexHolder oHolder( &hMutex );
                oManager = oStaticManager;
                return true;
            }

            CPLString osClientId =
                CPLGetConfigOption("GS_OAUTH2_CLIENT_ID", "");
            CPLString osClientSecret =
                CPLGetConfigOption("GS_OAUTH2_CLIENT_SECRET", "");
            bool bClientInfoFromEnv = false;
            bool bClientInfoFromFile = false;

            int nCount = (!osClientId.empty() ? 1 : 0) +
                         (!osClientSecret.empty() ? 1 : 0);
            if( nCount == 1 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Either both or none of GS_OAUTH2_CLIENT_ID and "
                        "GS_OAUTH2_CLIENT_SECRET must be set");
                return false;
            }
            else if( nCount == 2)
            {
                bClientInfoFromEnv = true;
            }
            else if( nCount == 0 )
            {
                nCount = (!osOAuth2ClientId.empty() ? 1 : 0) +
                         (!osOAuth2ClientSecret.empty() ? 1 : 0);
                if( nCount == 1 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                            "Either both or none of client_id and "
                            "client_secret from %s must be set",
                            osCredentials.c_str());
                    return false;
                }
                else if( nCount == 2)
                {
                    osClientId = osOAuth2ClientId;
                    osClientSecret = osOAuth2ClientSecret;
                    bClientInfoFromFile = true;
                }
            }

            if( bFirstTimeForDebugMessage )
            {
                CPLString osMsg;
                osMsg.Printf("Using gs_oauth2_refresh_token from %s",
                             osCredentials.c_str());
                if( bClientInfoFromEnv )
                    osMsg += " and GS_OAUTH2_CLIENT_ID and "
                             "GS_OAUTH2_CLIENT_SECRET configuration options";
                else if( bClientInfoFromFile )
                    osMsg += CPLSPrintf(" and client_id and client_secret from %s",
                                        osCredentials.c_str());
                else
                    osMsg += " and GDAL default client_id/client_secret";
                CPLDebug("GS", "%s", osMsg.c_str());
            }
            bFirstTimeForDebugMessage = false;
            return oManager.SetAuthFromRefreshToken(
                osOAuth2RefreshToken.c_str(), osClientId, osClientSecret,
                nullptr);
        }
        else
        {
            if( bFirstTimeForDebugMessage )
            {
                CPLDebug("GS",
                        "Using gs_access_key_id and gs_secret_access_key from %s",
                        osCredentials.c_str());
            }
            bFirstTimeForDebugMessage = false;
            return true;
        }
    }

    if( oStaticManager.GetAuthMethod() == GOA2Manager::GCE )
    {
        CPLMutexHolder oHolder( &hMutex );
        oManager = oStaticManager;
        return true;
    }
    // Some Travis-CI workers are GCE machines, and for some tests, we don't
    // want this code path to be taken. And on AppVeyor/Window, we would also
    // attempt a network access
    else if( !CPLTestBool(CPLGetConfigOption("CPL_GCE_SKIP", "NO")) &&
             CPLIsMachinePotentiallyGCEInstance() )
    {
        oManager.SetAuthFromGCE(nullptr);
        if( oManager.GetBearer() != nullptr )
        {
            CPLDebug("GS", "Using GCE inherited permissions");

            {
                CPLMutexHolder oHolder( &hMutex );
                oStaticManager = oManager;
            }

            bFirstTimeForDebugMessage = false;
            return true;
        }
    }

    if( bMayWarnDidNotFindAuth )
    {
        CPLDebug("GS", "Cannot find Authorization header in %s",
                 CPLGetConfigOption("GDAL_HTTP_HEADER_FILE", ""));
    }

    CPLString osMsg;
    osMsg.Printf("GS_SECRET_ACCESS_KEY+GS_ACCESS_KEY_ID, "
                 "GS_OAUTH2_REFRESH_TOKEN or "
                 "GOOGLE_APPLICATION_CREDENTIALS or "
                 "GS_OAUTH2_PRIVATE_KEY+GS_OAUTH2_CLIENT_EMAIL and %s, "
                 "or GS_NO_SIGN_REQUEST=YES configuration options not defined",
                 osCredentials.c_str());

    CPLDebug("GS", "%s", osMsg.c_str());
    VSIError(VSIE_AWSInvalidCredentials, "%s", osMsg.c_str());
    return false;
}


/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSIGSHandleHelper* VSIGSHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* /*pszFSPrefix*/,
                                                    CSLConstList papszOptions )
{
    // pszURI == bucket/object
    const CPLString osBucketObject( pszURI );
    const CPLString osEndpoint( CPLGetConfigOption("CPL_GS_ENDPOINT",
                                    "https://storage.googleapis.com/") );

    CPLString osSecretAccessKey;
    CPLString osAccessKeyId;
    CPLString osHeaderFile;
    GOA2Manager oManager;

    if( !GetConfiguration(papszOptions,
                          osSecretAccessKey, osAccessKeyId, osHeaderFile,
                          oManager) )
    {
        return nullptr;
    }

    // https://cloud.google.com/storage/docs/xml-api/reference-headers#xgooguserproject
    // The Project ID for an existing Google Cloud project to bill for access
    // charges associated with the request.
    const std::string osUserProject = CPLGetConfigOption("GS_USER_PROJECT", "");

    return new VSIGSHandleHelper( osEndpoint,
                                  osBucketObject,
                                  osSecretAccessKey,
                                  osAccessKeyId,
                                  !osHeaderFile.empty(),
                                  oManager,
                                  osUserProject );
}

/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSIGSHandleHelper::RebuildURL()
{
    m_osURL = m_osEndpoint + CPLAWSURLEncode(m_osBucketObjectKey, false);
    if( !m_osBucketObjectKey.empty() &&
        m_osBucketObjectKey.find('/') == std::string::npos )
        m_osURL += "/";
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                           UsesHMACKey()                              */
/************************************************************************/

bool VSIGSHandleHelper::UsesHMACKey() const
{
    return m_oManager.GetAuthMethod() == GOA2Manager::NONE;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIGSHandleHelper::GetCurlHeaders( const CPLString& osVerb,
                                   const struct curl_slist* psExistingHeaders,
                                   const void *,
                                   size_t ) const
{
    if( m_bUseHeaderFile )
        return nullptr;

    if( m_oManager.GetAuthMethod() != GOA2Manager::NONE )
    {
        const char* pszBearer = m_oManager.GetBearer();
        if( pszBearer == nullptr )
            return nullptr;

        {
            CPLMutexHolder oHolder( &hMutex );
            oStaticManager = m_oManager;
        }

        struct curl_slist *headers=nullptr;
        headers = curl_slist_append(
            headers, CPLSPrintf("Authorization: Bearer %s", pszBearer));

        if( !m_osUserProject.empty() )
        {
            headers = curl_slist_append(
                headers, CPLSPrintf("x-goog-user-project: %s", m_osUserProject.c_str()));
        }
        return headers;
    }

    CPLString osCanonicalResource("/" + CPLAWSURLEncode(m_osBucketObjectKey, false));
    if( !m_osBucketObjectKey.empty() &&
        m_osBucketObjectKey.find('/') == std::string::npos )
        osCanonicalResource += "/";
    else
    {
        const auto osQueryString(GetQueryString(false));
        if( osQueryString == "?uploads" || osQueryString == "?acl" )
            osCanonicalResource += osQueryString;
    }

    return GetGSHeaders( osVerb,
                         psExistingHeaders,
                         osCanonicalResource,
                         m_osSecretAccessKey,
                         m_osAccessKeyId,
                         m_osUserProject );
}

/************************************************************************/
/*                          CleanMutex()                                */
/************************************************************************/

void VSIGSHandleHelper::CleanMutex()
{
    if( hMutex != nullptr )
        CPLDestroyMutex( hMutex );
    hMutex = nullptr;
}
/************************************************************************/
/*                          ClearCache()                                */
/************************************************************************/

void VSIGSHandleHelper::ClearCache()
{
    CPLMutexHolder oHolder( &hMutex );

    oStaticManager = GOA2Manager();
    bFirstTimeForDebugMessage = true;
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

CPLString VSIGSHandleHelper::GetSignedURL(CSLConstList papszOptions)
{
    if( !((!m_osAccessKeyId.empty() && !m_osSecretAccessKey.empty()) ||
          m_oManager.GetAuthMethod() == GOA2Manager::SERVICE_ACCOUNT) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Signed URL for Google Cloud Storage is only available with "
                 "AWS style authentication with GS_ACCESS_KEY_ID+GS_SECRET_ACCESS_KEY, "
                 "or with service account authentication");
        return CPLString();
    }

    GIntBig nStartDate = static_cast<GIntBig>(time(nullptr));
    const char* pszStartDate = CSLFetchNameValue(papszOptions, "START_DATE");
    if( pszStartDate )
    {
        int nYear, nMonth, nDay, nHour, nMin, nSec;
        if( sscanf(pszStartDate, "%04d%02d%02dT%02d%02d%02dZ",
                   &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec) == 6 )
        {
            struct tm brokendowntime;
            brokendowntime.tm_year = nYear - 1900;
            brokendowntime.tm_mon = nMonth - 1;
            brokendowntime.tm_mday = nDay;
            brokendowntime.tm_hour = nHour;
            brokendowntime.tm_min = nMin;
            brokendowntime.tm_sec = nSec;
            nStartDate = CPLYMDHMSToUnixTime(&brokendowntime);
        }
    }
    GIntBig nExpiresIn = nStartDate + atoi(
        CSLFetchNameValueDef(papszOptions, "EXPIRATION_DELAY", "3600"));
    CPLString osExpires(CSLFetchNameValueDef(papszOptions, "EXPIRES",
                                    CPLSPrintf(CPL_FRMT_GIB, nExpiresIn)));

    CPLString osVerb(CSLFetchNameValueDef(papszOptions, "VERB", "GET"));

    CPLString osCanonicalizedResource("/" + CPLAWSURLEncode(m_osBucketObjectKey, false));

    CPLString osStringToSign;
    osStringToSign += osVerb + "\n";
    osStringToSign += "\n"; // Content_MD5
    osStringToSign += "\n"; // Content_Type
    osStringToSign += osExpires + "\n";
    // osStringToSign += // Canonicalized_Extension_Headers
    osStringToSign += osCanonicalizedResource;
#ifdef DEBUG_VERBOSE
    CPLDebug("GS", "osStringToSign = %s", osStringToSign.c_str());
#endif

    if( !m_osAccessKeyId.empty() )
    {
        // No longer documented but actually works !
        GByte abySignature[CPL_SHA1_HASH_SIZE] = {};
        CPL_HMAC_SHA1( m_osSecretAccessKey.c_str(), m_osSecretAccessKey.size(),
                    osStringToSign, osStringToSign.size(),
                    abySignature);

        char* pszBase64 = CPLBase64Encode( sizeof(abySignature), abySignature );
        CPLString osSignature(pszBase64);
        CPLFree(pszBase64);

        ResetQueryParameters();
        AddQueryParameter("GoogleAccessId", m_osAccessKeyId);
        AddQueryParameter("Expires", osExpires);
        AddQueryParameter("Signature", osSignature);
    }
    else
    {
        unsigned nSignatureLen = 0;
        GByte* pabySignature = CPL_RSA_SHA256_Sign(
            m_oManager.GetPrivateKey().c_str(),
            osStringToSign.data(),
            static_cast<unsigned>(osStringToSign.size()),
            &nSignatureLen);
        if( pabySignature == nullptr )
            return CPLString();
        char* pszBase64 = CPLBase64Encode( nSignatureLen, pabySignature );
        CPLFree(pabySignature);
        CPLString osSignature(pszBase64);
        CPLFree(pszBase64);

        ResetQueryParameters();
        AddQueryParameter("GoogleAccessId", m_oManager.GetClientEmail());
        AddQueryParameter("Expires", osExpires);
        AddQueryParameter("Signature", osSignature);
    }
    return m_osURL;
}

#endif

//! @endcond
