/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  OpenStack Swift Object Storage routines
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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
 * DEALINSwift IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_swift.h"
#include "cpl_vsi_error.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"

// HOWTO setup a Docker-based SWIFT server:
// https://github.com/MorrisJobke/docker-swift-onlyone


//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

#ifdef HAVE_CURL

static CPLMutex *g_hMutex = nullptr;
static CPLString g_osLastAuthURL;
static CPLString g_osLastUser;
static CPLString g_osLastKey;
static CPLString g_osLastStorageURL;
static CPLString g_osLastAuthToken;

/************************************************************************/
/*                          GetSwiftHeaders()                           */
/************************************************************************/

static
struct curl_slist* GetSwiftHeaders( const CPLString& osAuthToken )
{
    struct curl_slist *headers=nullptr;
    headers = curl_slist_append(
        headers, "Accept: application/json");
    headers = curl_slist_append(
        headers, CPLSPrintf("x-auth-token: %s", osAuthToken.c_str()));
    return headers;
}

/************************************************************************/
/*                     VSISwiftHandleHelper()                           */
/************************************************************************/
VSISwiftHandleHelper::VSISwiftHandleHelper(const CPLString& osStorageURL,
                                           const CPLString& osAuthToken,
                                           const CPLString& osBucket,
                                           const CPLString& osObjectKey) :
    m_osURL(BuildURL(osStorageURL, osBucket, osObjectKey)),
    m_osStorageURL(osStorageURL),
    m_osAuthToken(osAuthToken),
    m_osBucket(osBucket),
    m_osObjectKey(osObjectKey)
{
}

/************************************************************************/
/*                      ~VSISwiftHandleHelper()                         */
/************************************************************************/

VSISwiftHandleHelper::~VSISwiftHandleHelper()
{
}

/************************************************************************/
/*                        GetConfiguration()                            */
/************************************************************************/

bool VSISwiftHandleHelper::GetConfiguration(CPLString& osStorageURL,
                                            CPLString& osAuthToken)
{
    osStorageURL = CPLGetConfigOption("SWIFT_STORAGE_URL", "");
    if( !osStorageURL.empty() )
    {
        osAuthToken = CPLGetConfigOption("SWIFT_AUTH_TOKEN", "");
        if( osAuthToken.empty() )
        {
            const char* pszMsg = "Missing SWIFT_AUTH_TOKEN";
            CPLDebug("SWIFT", "%s", pszMsg);
            VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
            return false;
        }
        return true;
    }

    CPLString osAuthURL = CPLGetConfigOption("SWIFT_AUTH_V1_URL", "");
    CPLString osUser = CPLGetConfigOption("SWIFT_USER", "");
    CPLString osKey = CPLGetConfigOption("SWIFT_KEY", "");
    if( osAuthURL.empty() || osUser.empty() || osKey.empty() )
    {
        const char* pszMsg = "Missing SWIFT_STORAGE_URL+SWIFT_AUTH_TOKEN or "
                             "SWIFT_AUTH_V1_URL+SWIFT_USER+SWIFT_KEY "
                             "configuration options";
        CPLDebug("SWIFT", "%s", pszMsg);
        VSIError(VSIE_AWSInvalidCredentials, "%s", pszMsg);
        return false;
    }

    // Re-use cached credentials if available
    {
        CPLMutexHolder oHolder( &g_hMutex );
        // coverity[tainted_data]
        if( osAuthURL == g_osLastAuthURL &&
            osUser == g_osLastUser &&
            osKey == g_osLastKey )
        {
            osStorageURL = g_osLastStorageURL;
            osAuthToken = g_osLastAuthToken;
            return true;
        }
    }

    char** papszHeaders = CSLSetNameValue(nullptr, "HEADERS",
        CPLSPrintf("X-Auth-User: %s\r\n"
                   "X-Auth-Key: %s",
                   osUser.c_str(),
                   osKey.c_str()));
    CPLHTTPResult* psResult = CPLHTTPFetch(osAuthURL, papszHeaders);
    CSLDestroy(papszHeaders);
    if( psResult == nullptr )
        return false;
    osStorageURL = CSLFetchNameValueDef(psResult->papszHeaders,
                                        "X-Storage-Url", "");
    osAuthToken = CSLFetchNameValueDef(psResult->papszHeaders,
                                       "X-Auth-Token", "");
    CPLString osErrorMsg = psResult->pabyData ?
                reinterpret_cast<const char*>(psResult->pabyData) : "";
    CPLHTTPDestroyResult(psResult);
    if( osStorageURL.empty() || osAuthToken.empty() )
    {
        CPLDebug("SWIFT", "Authentication failed: %s", osErrorMsg.c_str());
        VSIError(VSIE_AWSInvalidCredentials,
                 "Authentication failed: %s", osErrorMsg.c_str());
        return false;
    }

    // Cache credentials
    {
        CPLMutexHolder oHolder( &g_hMutex );
        g_osLastAuthURL = osAuthURL;
        g_osLastUser = osUser;
        g_osLastKey = osKey;
        g_osLastStorageURL = osStorageURL;
        g_osLastAuthToken = osAuthToken;
    }

    return true;
}


/************************************************************************/
/*                          BuildFromURI()                              */
/************************************************************************/

VSISwiftHandleHelper* VSISwiftHandleHelper::BuildFromURI( const char* pszURI,
                                                    const char* /*pszFSPrefix*/ )
{
    CPLString osStorageURL;
    CPLString osAuthToken;

    if( !GetConfiguration(osStorageURL, osAuthToken) )
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

    return new VSISwiftHandleHelper( osStorageURL,
                                     osAuthToken,
                                     osBucket,
                                     osObjectKey );
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

CPLString VSISwiftHandleHelper::BuildURL(const CPLString& osStorageURL,
                                         const CPLString& osBucket,
                                         const CPLString& osObjectKey)
{
    CPLString osURL = osStorageURL;
    if( !osBucket.empty() )
        osURL += "/" + CPLAWSURLEncode(osBucket,false);
    if( !osObjectKey.empty() )
        osURL += "/" + CPLAWSURLEncode(osObjectKey,false);
    return osURL;
}


/************************************************************************/
/*                           RebuildURL()                               */
/************************************************************************/

void VSISwiftHandleHelper::RebuildURL()
{
    m_osURL = BuildURL(m_osStorageURL, m_osBucket, m_osObjectKey);
    m_osURL += GetQueryString(false);
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSISwiftHandleHelper::GetCurlHeaders( const CPLString&,
                                          const struct curl_slist*,
                                          const void *,
                                          size_t ) const
{
    return GetSwiftHeaders( m_osAuthToken );
}

/************************************************************************/
/*                          CleanMutex()                                */
/************************************************************************/

void VSISwiftHandleHelper::CleanMutex()
{
    if( g_hMutex != nullptr )
        CPLDestroyMutex( g_hMutex );
    g_hMutex = nullptr;
}

/************************************************************************/
/*                          ClearCache()                                */
/************************************************************************/

void VSISwiftHandleHelper::ClearCache()
{
    CPLMutexHolder oHolder( &g_hMutex );
    g_osLastAuthURL.clear();
    g_osLastUser.clear();
    g_osLastKey.clear();
    g_osLastStorageURL.clear();
    g_osLastAuthToken.clear();
}

#endif

//! @endcond
