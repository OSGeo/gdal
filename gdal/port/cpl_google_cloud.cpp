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
/*                GetConfigurationFromAWSConfigFiles()                  */
/************************************************************************/

bool VSIGSHandleHelper::GetConfigurationFromConfigFile(
                                                CPLString& osSecretAccessKey,
                                                CPLString& osAccessKeyId,
                                                CPLString& osCredentials)
{
#ifdef WIN32
    const char* pszHome = CPLGetConfigOption("USERPROFILE", NULL);
#else
    const char* pszHome = CPLGetConfigOption("HOME", NULL);
#endif

    osCredentials =
        // GDAL specific config option (mostly for testing purpose, but also
        // used in production in some cases)
        CPLGetConfigOption( "CPL_GS_CREDENTIALS_FILE",
                        CPLFormFilename( pszHome, ".boto", NULL ) );
    VSILFILE* fp = VSIFOpenL( osCredentials, "rb" );
    if( fp != NULL )
    {
        const char* pszLine;
        bool bInProfile = false;
        while( (pszLine = CPLReadLineL(fp)) != NULL )
        {
            if( pszLine[0] == '[' )
            {
                if( bInProfile )
                    break;
                if( CPLString(pszLine) == "[Credentials]" )
                    bInProfile = true;
            }
            else if( bInProfile )
            {
                char* pszKey = NULL;
                const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
                if( pszKey && pszValue )
                {
                    if( EQUAL(pszKey, "gs_access_key_id") )
                        osAccessKeyId = pszValue;
                    else if( EQUAL(pszKey, "gs_secret_access_key") )
                        osSecretAccessKey = pszValue;
                }
                CPLFree(pszKey);
            }
        }
        VSIFCloseL(fp);
    }

    return !osAccessKeyId.empty() && !osSecretAccessKey.empty();
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSIGSHandleHelper::GetConfiguration(CPLString& osSecretAccessKey,
                                         CPLString& osAccessKeyId,
                                         CPLString& osHeaderFile)
{
    osSecretAccessKey.clear();
    osAccessKeyId.clear();
    osHeaderFile.clear();

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
            return false;
        }

        return true;
    }

    // Next try reading from ~/.boto
    CPLString osCredentials;
    if( GetConfigurationFromConfigFile(osSecretAccessKey, osAccessKeyId,
                                       osCredentials) )
    {
        return true;
    }

    osHeaderFile =
        CPLGetConfigOption("GDAL_HTTP_HEADER_FILE", "");
    if( !osHeaderFile.empty() )
    {
        return true;
    }

    VSIError(VSIE_AWSInvalidCredentials,
                "GS_SECRET_ACCESS_KEY configuration option and %s not defined",
                osCredentials.c_str());
    return false;
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

    CPLString osSecretAccessKey;
    CPLString osAccessKeyId;
    CPLString osHeaderFile;

    if( !GetConfiguration(osSecretAccessKey, osAccessKeyId, osHeaderFile) )
    {
        return NULL;
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
