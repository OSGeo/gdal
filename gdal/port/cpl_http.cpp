/******************************************************************************
 *
 * Project:  libcurl based HTTP client
 * Purpose:  libcurl based HTTP client
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "cpl_http.h"

#include <cstddef>
#include <cstring>

#include <map>
#include <string>

#include "cpl_http.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

#ifdef HAVE_CURL
#  include <curl/curl.h>
// CURLINFO_RESPONSE_CODE was known as CURLINFO_HTTP_CODE in libcurl 7.10.7 and
// earlier.
#if LIBCURL_VERSION_NUM < 0x070a07
#define CURLINFO_RESPONSE_CODE CURLINFO_HTTP_CODE
#endif

#endif

CPL_CVSID("$Id$")

// list of named persistent http sessions

#ifdef HAVE_CURL
static std::map<CPLString, CURL*>* poSessionMap = NULL;
static CPLMutex *hSessionMapMutex = NULL;
#endif

/************************************************************************/
/*                            CPLWriteFct()                             */
/*                                                                      */
/*      Append incoming text to our collection buffer, reallocating     */
/*      it larger as needed.                                            */
/************************************************************************/

#ifdef HAVE_CURL

typedef struct
{
    CPLHTTPResult* psResult;
    int            nMaxFileSize;
} CPLHTTPResultWithLimit;

static size_t
CPLWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)

{
    CPLHTTPResultWithLimit *psResultWithLimit =
        static_cast<CPLHTTPResultWithLimit *>(reqInfo);
    CPLHTTPResult* psResult = psResultWithLimit->psResult;

    int nBytesToWrite = static_cast<int>(nmemb)*static_cast<int>(size);
    int nNewSize = psResult->nDataLen + nBytesToWrite + 1;
    if( nNewSize > psResult->nDataAlloc )
    {
        psResult->nDataAlloc = static_cast<int>(nNewSize * 1.25 + 100);
        GByte* pabyNewData = static_cast<GByte *>(
            VSIRealloc(psResult->pabyData, psResult->nDataAlloc));
        if( pabyNewData == NULL )
        {
            VSIFree(psResult->pabyData);
            psResult->pabyData = NULL;
            psResult->pszErrBuf = CPLStrdup(CPLString().Printf("Out of memory allocating %d bytes for HTTP data buffer.", psResult->nDataAlloc));
            psResult->nDataAlloc = psResult->nDataLen = 0;

            return 0;
        }
        psResult->pabyData = pabyNewData;
    }

    memcpy( psResult->pabyData + psResult->nDataLen, buffer, nBytesToWrite );

    psResult->nDataLen += nBytesToWrite;
    psResult->pabyData[psResult->nDataLen] = 0;

    if( psResultWithLimit->nMaxFileSize > 0 &&
        psResult->nDataLen > psResultWithLimit->nMaxFileSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Maximum file size reached");
        return 0;
    }

    return nmemb;
}

/************************************************************************/
/*                           CPLHdrWriteFct()                           */
/************************************************************************/
static size_t CPLHdrWriteFct( void *buffer, size_t size, size_t nmemb,
                              void *reqInfo )
{
    CPLHTTPResult *psResult = static_cast<CPLHTTPResult *>(reqInfo);
    // Copy the buffer to a char* and initialize with zeros (zero
    // terminate as well).
    char* pszHdr = static_cast<char *>(CPLCalloc(nmemb + 1, size));
    CPLPrintString(pszHdr, static_cast<char *>(buffer),
                   static_cast<int>(nmemb) * static_cast<int>(size));
    char *pszKey = NULL;
    const char *pszValue = CPLParseNameValue(pszHdr, &pszKey );
    psResult->papszHeaders =
        CSLSetNameValue(psResult->papszHeaders, pszKey, pszValue);
    CPLFree(pszHdr);
    CPLFree(pszKey);
    return nmemb;
}

#endif /* def HAVE_CURL */

/************************************************************************/
/*                       CPLHTTPGetOptionsFromEnv()                     */
/************************************************************************/

typedef struct
{
    const char* pszEnvVar;
    const char* pszOptionName;
} TupleEnvVarOptionName;

static const TupleEnvVarOptionName asAssocEnvVarOptionName[] =
{
    { "GDAL_HTTP_CONNECTTIMEOUT", "CONNECTTIMEOUT" },
    { "GDAL_HTTP_TIMEOUT", "TIMEOUT" },
    { "GDAL_HTTP_LOW_SPEED_TIME", "LOW_SPEED_TIME" },
    { "GDAL_HTTP_LOW_SPEED_LIMIT", "LOW_SPEED_LIMIT" },
    { "GDAL_HTTP_PROXY", "PROXY" },
    { "GDAL_HTTP_PROXYUSERPWD", "PROXYUSERPWD" },
    { "GDAL_PROXY_AUTH", "PROXYAUTH" },
    { "GDAL_HTTP_NETRC", "NETRC" },
    { "GDAL_HTTP_MAX_RETRY", "MAX_RETRY" },
    { "GDAL_HTTP_RETRY_DELAY", "RETRY_DELAY" },
    { "CURL_CA_BUNDLE", "CAINFO" },
    { "SSL_CERT_FILE", "CAINFO" },
    { "GDAL_HTTP_HEADER_FILE", "HEADER_FILE" }
};

char** CPLHTTPGetOptionsFromEnv()
{
    char** papszOptions = NULL;
    for( size_t i = 0; i < CPL_ARRAYSIZE(asAssocEnvVarOptionName); ++i )
    {
        const char* pszVal = CPLGetConfigOption(
            asAssocEnvVarOptionName[i].pszEnvVar, NULL);
        if( pszVal != NULL )
        {
            papszOptions = CSLSetNameValue(papszOptions,
                asAssocEnvVarOptionName[i].pszOptionName, pszVal);
        }
    }
    return papszOptions;
}

/************************************************************************/
/*                           CPLHTTPFetch()                             */
/************************************************************************/

/**
 * \brief Fetch a document from an url and return in a string.
 *
 * @param pszURL valid URL recognized by underlying download library (libcurl)
 * @param papszOptions option list as a NULL-terminated array of strings. May be NULL.
 *                     The following options are handled :
 * <ul>
 * <li>CONNECTTIMEOUT=val, where val is in seconds (possibly with decimals).
 *     This is the maximum delay for the connection to be established before
 *     being aborted (GDAL >= 2.2).</li>
 * <li>TIMEOUT=val, where val is in seconds. This is the maximum delay for the whole
 *     request to complete before being aborted.</li>
 * <li>LOW_SPEED_TIME=val, where val is in seconds. This is the maximum time where the
 *      transfer speed should be below the LOW_SPEED_LIMIT (if not specified 1b/s),
 *      before the transfer to be considered too slow and aborted. (GDAL >= 2.1)</li>
 * <li>LOW_SPEED_LIMIT=val, where val is in bytes/second. See LOW_SPEED_TIME. Has only
 *     effect if LOW_SPEED_TIME is specified too. (GDAL >= 2.1)</li>
 * <li>HEADERS=val, where val is an extra header to use when getting a web page.
 *                  For example "Accept: application/x-ogcwkt"</li>
 * <li>HEADER_FILE=filename: filename of a text file with "key: value" headers.
 *     (GDAL >= 2.2)</li>
 * <li>HTTPAUTH=[BASIC/NTLM/GSSNEGOTIATE/ANY] to specify an authentication scheme to use.</li>
 * <li>USERPWD=userid:password to specify a user and password for authentication</li>
 * <li>POSTFIELDS=val, where val is a nul-terminated string to be passed to the server
 *                     with a POST request.</li>
 * <li>PROXY=val, to make requests go through a proxy server, where val is of the
 *                form proxy.server.com:port_number</li>
 * <li>PROXYUSERPWD=val, where val is of the form username:password</li>
 * <li>PROXYAUTH=[BASIC/NTLM/DIGEST/ANY] to specify an proxy authentication scheme to use.</li>
 * <li>NETRC=[YES/NO] to enable or disable use of $HOME/.netrc, default YES.</li>
 * <li>CUSTOMREQUEST=val, where val is GET, PUT, POST, DELETE, etc.. (GDAL >= 1.9.0)</li>
 * <li>COOKIE=val, where val is formatted as COOKIE1=VALUE1; COOKIE2=VALUE2; ...</li>
 * <li>MAX_RETRY=val, where val is the maximum number of retry attempts if a 502, 503 or
 *               504 HTTP error occurs. Default is 0. (GDAL >= 2.0)</li>
 * <li>RETRY_DELAY=val, where val is the number of seconds between retry attempts.
 *                 Default is 30. (GDAL >= 2.0)</li>
 * <li>MAX_FILE_SIZE=val, where val is a number of bytes (GDAL >= 2.2)</li>
 * <li>CAINFO=/path/to/bundle.crt. This is path to Certificate Authority (CA)
 *     bundle file. By default, it will be looked in a system location. If
 *     the CAINFO options is not defined, GDAL will also look if the CURL_CA_BUNDLE
 *     environment variable is defined to use it as the CAINFO value, and as a
 *     fallback to the SSL_CERT_FILE environment variable. (GDAL >= 2.1.3)</li>
 * </ul>
 *
 * Alternatively, if not defined in the papszOptions arguments, the
 * CONNECTTIMEOUT, TIMEOUT,
 * LOW_SPEED_TIME, LOW_SPEED_LIMIT, PROXY, PROXYUSERPWD, PROXYAUTH, NETRC,
 * MAX_RETRY and RETRY_DELAY, HEADER_FILE values are searched in the configuration
 * options named GDAL_HTTP_CONNECTTIMEOUT, GDAL_HTTP_TIMEOUT,
 * GDAL_HTTP_LOW_SPEED_TIME, GDAL_HTTP_LOW_SPEED_LIMIT,
 * GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD, GDAL_PROXY_AUTH,
 * GDAL_HTTP_NETRC, GDAL_HTTP_MAX_RETRY, GDAL_HTTP_RETRY_DELAY,
 * GDAL_HTTP_HEADER_FILE.
 *
 * @return a CPLHTTPResult* structure that must be freed by
 * CPLHTTPDestroyResult(), or NULL if libcurl support is disabled
 */
CPLHTTPResult *CPLHTTPFetch( const char *pszURL, char **papszOptions )

{
    if( STARTS_WITH(pszURL, "/vsimem/") &&
        // Disabled by default for potential security issues.
        CPLTestBool(CPLGetConfigOption("CPL_CURL_ENABLE_VSIMEM", "FALSE")) )
    {
        CPLString osURL(pszURL);
        const char* pszCustomRequest =
            CSLFetchNameValue( papszOptions, "CUSTOMREQUEST" );
        if( pszCustomRequest != NULL )
        {
            osURL += "&CUSTOMREQUEST=";
            osURL += pszCustomRequest;
        }
        const char* pszPost = CSLFetchNameValue( papszOptions, "POSTFIELDS" );
        if( pszPost != NULL ) // Hack: We append post content to filename.
        {
            osURL += "&POSTFIELDS=";
            osURL += pszPost;
        }
        vsi_l_offset nLength = 0;
        CPLHTTPResult* psResult =
            static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        GByte* pabyData = VSIGetMemFileBuffer( osURL, &nLength, FALSE );
        if( pabyData == NULL )
        {
            CPLDebug("HTTP", "Cannot find %s", osURL.c_str());
            psResult->nStatus = 1;
            psResult->pszErrBuf =
                CPLStrdup(CPLSPrintf("HTTP error code : %d", 404));
            CPLError( CE_Failure, CPLE_AppDefined, "%s", psResult->pszErrBuf );
        }
        else if( nLength != 0 )
        {
            psResult->nDataLen = static_cast<int>(nLength);
            psResult->pabyData = static_cast<GByte *>(
                CPLMalloc(static_cast<size_t>(nLength) + 1));
            memcpy(psResult->pabyData, pabyData, static_cast<size_t>(nLength));
            psResult->pabyData[static_cast<size_t>(nLength)] = 0;
        }

        if( psResult->pabyData != NULL &&
            STARTS_WITH(reinterpret_cast<char *>(psResult->pabyData),
                        "Content-Type: ") )
        {
            const char* pszContentType =
                reinterpret_cast<char *>(psResult->pabyData) +
                strlen("Content-type: ");
            const char* pszEOL = strchr(pszContentType, '\r');
            if( pszEOL )
                pszEOL = strchr(pszContentType, '\n');
            if( pszEOL )
            {
                size_t nContentLength = pszEOL - pszContentType;
                psResult->pszContentType =
                    static_cast<char *>(CPLMalloc(nContentLength + 1));
                memcpy(psResult->pszContentType,
                       pszContentType,
                       nContentLength);
                psResult->pszContentType[nContentLength] = 0;
            }
        }

        return psResult;
    }

#ifndef HAVE_CURL
    (void) papszOptions;
    (void) pszURL;

    CPLError( CE_Failure, CPLE_NotSupported,
              "GDAL/OGR not compiled with libcurl support, "
              "remote requests not supported." );
    return NULL;
#else

/* -------------------------------------------------------------------- */
/*      Are we using a persistent named session?  If so, search for     */
/*      or create it.                                                   */
/*                                                                      */
/*      Currently this code does not attempt to protect against         */
/*      multiple threads asking for the same named session.  If that    */
/*      occurs it will be in use in multiple threads at once which      */
/*      might have bad consequences depending on what guarantees        */
/*      libcurl gives - which I have not investigated.                  */
/* -------------------------------------------------------------------- */
    CURL *http_handle = NULL;

    const char *pszPersistent = CSLFetchNameValue( papszOptions, "PERSISTENT" );
    const char *pszClosePersistent =
        CSLFetchNameValue( papszOptions, "CLOSE_PERSISTENT" );
    if( pszPersistent )
    {
        CPLString osSessionName = pszPersistent;
        CPLMutexHolder oHolder( &hSessionMapMutex );

        if( poSessionMap == NULL )
            poSessionMap = new std::map<CPLString, CURL *>;
        if( poSessionMap->count( osSessionName ) == 0 )
        {
            (*poSessionMap)[osSessionName] = curl_easy_init();
            CPLDebug( "HTTP", "Establish persistent session named '%s'.",
                      osSessionName.c_str() );
        }

        http_handle = (*poSessionMap)[osSessionName];
    }
/* -------------------------------------------------------------------- */
/*      Are we requested to close a persistent named session?           */
/* -------------------------------------------------------------------- */
    else if( pszClosePersistent )
    {
        CPLString osSessionName = pszClosePersistent;
        CPLMutexHolder oHolder( &hSessionMapMutex );

        if( poSessionMap )
        {
            std::map<CPLString, CURL *>::iterator oIter =
                poSessionMap->find( osSessionName );
            if( oIter != poSessionMap->end() )
            {
                curl_easy_cleanup(oIter->second);
                poSessionMap->erase(oIter);
                if( poSessionMap->empty() )
                {
                    delete poSessionMap;
                    poSessionMap = NULL;
                }
                CPLDebug( "HTTP", "Ended persistent session named '%s'.",
                        osSessionName.c_str() );
            }
            else
            {
                CPLDebug(
                    "HTTP", "Could not find persistent session named '%s'.",
                    osSessionName.c_str() );
            }
        }

        return NULL;
    }
    else
        http_handle = curl_easy_init();

/* -------------------------------------------------------------------- */
/*      Setup the request.                                              */
/* -------------------------------------------------------------------- */
    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};

    const char* pszArobase = strchr(pszURL, '@');
    const char* pszSlash = strchr(pszURL, '/');
    const char* pszColon = (pszSlash) ? strchr(pszSlash, ':') : NULL;
    if( pszArobase != NULL && pszColon != NULL && pszArobase - pszColon > 0 )
    {
        /* http://user:password@www.example.com */
        char* pszSanitizedURL = CPLStrdup(pszURL);
        pszSanitizedURL[pszColon-pszURL] = 0;
        CPLDebug( "HTTP", "Fetch(%s:#password#%s)",
                  pszSanitizedURL, pszArobase );
        CPLFree(pszSanitizedURL);
    }
    else
    {
        CPLDebug( "HTTP", "Fetch(%s)", pszURL );
    }

    CPLHTTPResult *psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));

    curl_easy_setopt(http_handle, CURLOPT_URL, pszURL );

    struct curl_slist* headers= reinterpret_cast<struct curl_slist*>(
                            CPLHTTPSetOptions(http_handle, papszOptions));

    // Set Headers.
    const char *pszHeaders = CSLFetchNameValue( papszOptions, "HEADERS" );
    if( pszHeaders != NULL ) {
        CPLDebug ("HTTP", "These HTTP headers were set: %s", pszHeaders);
        char** papszTokensHeaders = CSLTokenizeString2(pszHeaders, "\r\n", 0);
        for( int i=0; papszTokensHeaders[i] != NULL; ++i )
            headers = curl_slist_append(headers, papszTokensHeaders[i]);
        CSLDestroy(papszTokensHeaders);
    }

    if( headers != NULL )
        curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, headers);

    // Are we making a head request.
    const char* pszNoBody = NULL;
    if( (pszNoBody = CSLFetchNameValue( papszOptions, "NO_BODY" )) != NULL )
    {
        if( CPLTestBool(pszNoBody) )
        {
            CPLDebug ("HTTP", "HEAD Request: %s", pszURL);
            curl_easy_setopt(http_handle, CURLOPT_NOBODY, 1L);
        }
    }

    // Capture response headers.
    curl_easy_setopt(http_handle, CURLOPT_HEADERDATA, psResult);
    curl_easy_setopt(http_handle, CURLOPT_HEADERFUNCTION, CPLHdrWriteFct);

    CPLHTTPResultWithLimit sResultWithLimit;
    sResultWithLimit.psResult = psResult;
    sResultWithLimit.nMaxFileSize = 0;
    const char* pszMaxFileSize = CSLFetchNameValue(papszOptions,
                                                   "MAX_FILE_SIZE");
    if( pszMaxFileSize != NULL )
    {
        sResultWithLimit.nMaxFileSize = atoi(pszMaxFileSize);
        // Only useful if size is returned by server before actual download.
        curl_easy_setopt(http_handle, CURLOPT_MAXFILESIZE,
                         sResultWithLimit.nMaxFileSize);
    }

    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, &sResultWithLimit );
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, CPLWriteFct );

    szCurlErrBuf[0] = '\0';

    curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    static bool bHasCheckVersion = false;
    static bool bSupportGZip = false;
    if( !bHasCheckVersion )
    {
        bSupportGZip = strstr(curl_version(), "zlib/") != NULL;
        bHasCheckVersion = true;
    }
    bool bGZipRequested = false;
    if( bSupportGZip &&
        CPLTestBool(CPLGetConfigOption("CPL_CURL_GZIP", "YES")) )
    {
        bGZipRequested = true;
        curl_easy_setopt(http_handle, CURLOPT_ENCODING, "gzip");
    }

/* -------------------------------------------------------------------- */
/*      If 502, 503 or 504 status code retry this HTTP call until max   */
/*      retry has been reached                                          */
/* -------------------------------------------------------------------- */
    const char *pszRetryDelay =
        CSLFetchNameValue( papszOptions, "RETRY_DELAY" );
    if( pszRetryDelay == NULL )
        pszRetryDelay = CPLGetConfigOption( "GDAL_HTTP_RETRY_DELAY",
                                    CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY) );
    const char *pszMaxRetries = CSLFetchNameValue( papszOptions, "MAX_RETRY" );
    if( pszMaxRetries == NULL )
        pszMaxRetries = CPLGetConfigOption( "GDAL_HTTP_MAX_RETRY",
                                    CPLSPrintf("%d",CPL_HTTP_MAX_RETRY) );
    double dfRetryDelaySecs = CPLAtof(pszRetryDelay);
    int nMaxRetries = atoi(pszMaxRetries);
    int nRetryCount = 0;
    bool bRequestRetry;

    do
    {
        bRequestRetry = false;

/* -------------------------------------------------------------------- */
/*      Execute the request, waiting for results.                       */
/* -------------------------------------------------------------------- */
        psResult->nStatus = static_cast<int>(curl_easy_perform(http_handle));

/* -------------------------------------------------------------------- */
/*      Fetch content-type if possible.                                 */
/* -------------------------------------------------------------------- */
        psResult->pszContentType = NULL;
        curl_easy_getinfo( http_handle, CURLINFO_CONTENT_TYPE,
                           &(psResult->pszContentType) );
        if( psResult->pszContentType != NULL )
            psResult->pszContentType = CPLStrdup(psResult->pszContentType);

/* -------------------------------------------------------------------- */
/*      Have we encountered some sort of error?                         */
/* -------------------------------------------------------------------- */
        if( strlen(szCurlErrBuf) > 0 )
        {
            bool bSkipError = false;

            // Some servers such as
            // http://115.113.193.14/cgi-bin/world/qgis_mapserv.fcgi?VERSION=1.1.1&SERVICE=WMS&REQUEST=GetCapabilities
            // invalidly return Content-Length as the uncompressed size, with
            // makes curl to wait for more data and time-out finally. If we got
            // the expected data size, then we don't emit an error but turn off
            // GZip requests.
            if( bGZipRequested &&
                strstr(szCurlErrBuf, "transfer closed with") &&
                strstr(szCurlErrBuf, "bytes remaining to read") )
            {
                const char* pszContentLength =
                    CSLFetchNameValue(psResult->papszHeaders, "Content-Length");
                if( pszContentLength && psResult->nDataLen != 0 &&
                    atoi(pszContentLength) == psResult->nDataLen )
                {
                    const char* pszCurlGZIPOption =
                        CPLGetConfigOption("CPL_CURL_GZIP", NULL);
                    if( pszCurlGZIPOption == NULL )
                    {
                        CPLSetConfigOption("CPL_CURL_GZIP", "NO");
                        CPLDebug("HTTP",
                                 "Disabling CPL_CURL_GZIP, "
                                 "because %s doesn't support it properly",
                                 pszURL);
                    }
                    psResult->nStatus = 0;
                    bSkipError = true;
                }
            }
            if( !bSkipError )
            {
                psResult->pszErrBuf = CPLStrdup(szCurlErrBuf);
                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s", szCurlErrBuf );
            }
        }
        else
        {
            // HTTP errors do not trigger curl errors. But we need to
            // propagate them to the caller though.
            long response_code = 0;
            curl_easy_getinfo(http_handle, CURLINFO_RESPONSE_CODE,
                              &response_code);

            if( response_code >= 400 && response_code < 600 )
            {
                // If HTTP 502, 503 or 504 gateway timeout error retry after a
                // pause.
                if( (response_code >= 502 && response_code <= 504) &&
                    nRetryCount < nMaxRetries )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "HTTP error code: %d - %s. "
                             "Retrying again in %.1f secs",
                             static_cast<int>(response_code), pszURL,
                             dfRetryDelaySecs);
                    CPLSleep(dfRetryDelaySecs);
                    nRetryCount++;

                    CPLFree(psResult->pszContentType);
                    psResult->pszContentType = NULL;
                    CSLDestroy(psResult->papszHeaders);
                    psResult->papszHeaders = NULL;
                    CPLFree(psResult->pabyData);
                    psResult->pabyData = NULL;
                    psResult->nDataLen = 0;
                    psResult->nDataAlloc = 0;

                    bRequestRetry = true;
                }
                else
                {
                    psResult->pszErrBuf =
                        CPLStrdup(CPLSPrintf("HTTP error code : %d",
                                             static_cast<int>(response_code)));
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s", psResult->pszErrBuf);
                }
            }
        }
    }
    while( bRequestRetry );

    if( !pszPersistent )
        curl_easy_cleanup( http_handle );

    curl_slist_free_all(headers);

    return psResult;
#endif /* def HAVE_CURL */
}

#ifdef HAVE_CURL

#ifdef WIN32

#include <windows.h>

/************************************************************************/
/*                     CPLFindWin32CurlCaBundleCrt()                    */
/************************************************************************/

static const char* CPLFindWin32CurlCaBundleCrt()
{
    DWORD nResLen;
    const DWORD nBufSize = MAX_PATH + 1;
    char *pszFilePart = NULL;

    char *pszPath = static_cast<char*>(CPLCalloc(1, nBufSize));

    nResLen = SearchPathA(NULL, "curl-ca-bundle.crt", NULL,
                          nBufSize, pszPath, &pszFilePart);
    if (nResLen > 0)
    {
        const char* pszRet = CPLSPrintf("%s", pszPath);
        CPLFree(pszPath);
        return pszRet;
    }
    CPLFree(pszPath);
    return NULL;
}

#endif // WIN32

/************************************************************************/
/*                         CPLHTTPSetOptions()                          */
/************************************************************************/

void* CPLHTTPSetOptions(void *pcurl, const char * const* papszOptions)
{
    CURL *http_handle = reinterpret_cast<CURL *>(pcurl);

    if( CPLTestBool(CPLGetConfigOption("CPL_CURL_VERBOSE", "NO")) )
        curl_easy_setopt(http_handle, CURLOPT_VERBOSE, 1);

    const char *pszHttpVersion =
        CSLFetchNameValue( papszOptions, "HTTP_VERSION");
    if( pszHttpVersion && strcmp(pszHttpVersion, "1.0") == 0 )
        curl_easy_setopt(http_handle, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_1_0);

    /* Support control over HTTPAUTH */
    const char *pszHttpAuth = CSLFetchNameValue( papszOptions, "HTTPAUTH" );
    if( pszHttpAuth == NULL )
        pszHttpAuth = CPLGetConfigOption( "GDAL_HTTP_AUTH", NULL );
    if( pszHttpAuth == NULL )
        /* do nothing */;

    /* CURLOPT_HTTPAUTH is defined in curl 7.11.0 or newer */
#if LIBCURL_VERSION_NUM >= 0x70B00
    else if( EQUAL(pszHttpAuth, "BASIC") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    else if( EQUAL(pszHttpAuth, "NTLM") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_NTLM );
    else if( EQUAL(pszHttpAuth, "ANY") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
#ifdef CURLAUTH_GSSNEGOTIATE
    else if( EQUAL(pszHttpAuth, "NEGOTIATE") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_GSSNEGOTIATE );
#endif
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unsupported HTTPAUTH value '%s', ignored.",
                  pszHttpAuth );
    }
#else
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "HTTPAUTH option needs curl >= 7.11.0" );
    }
#endif

    // Support use of .netrc - default enabled.
    const char *pszHttpNetrc = CSLFetchNameValue( papszOptions, "NETRC" );
    if( pszHttpNetrc == NULL )
        pszHttpNetrc = CPLGetConfigOption( "GDAL_HTTP_NETRC", "YES" );
    if( pszHttpNetrc == NULL || CPLTestBool(pszHttpNetrc) )
        curl_easy_setopt(http_handle, CURLOPT_NETRC, 1L);

    // Support setting userid:password.
    const char *pszUserPwd = CSLFetchNameValue( papszOptions, "USERPWD" );
    if( pszUserPwd == NULL )
        pszUserPwd = CPLGetConfigOption("GDAL_HTTP_USERPWD", NULL);
    if( pszUserPwd != NULL )
        curl_easy_setopt(http_handle, CURLOPT_USERPWD, pszUserPwd );

    // Set Proxy parameters.
    const char* pszProxy = CSLFetchNameValue( papszOptions, "PROXY" );
    if( pszProxy == NULL )
        pszProxy = CPLGetConfigOption("GDAL_HTTP_PROXY", NULL);
    if( pszProxy )
        curl_easy_setopt(http_handle, CURLOPT_PROXY, pszProxy);

    const char* pszProxyUserPwd =
        CSLFetchNameValue( papszOptions, "PROXYUSERPWD" );
    if( pszProxyUserPwd == NULL )
        pszProxyUserPwd = CPLGetConfigOption("GDAL_HTTP_PROXYUSERPWD", NULL);
    if( pszProxyUserPwd )
        curl_easy_setopt(http_handle, CURLOPT_PROXYUSERPWD, pszProxyUserPwd);

    // Support control over PROXYAUTH.
    const char *pszProxyAuth = CSLFetchNameValue( papszOptions, "PROXYAUTH" );
    if( pszProxyAuth == NULL )
        pszProxyAuth = CPLGetConfigOption( "GDAL_PROXY_AUTH", NULL );
    if( pszProxyAuth == NULL )
    {
        // Do nothing.
    }
    // CURLOPT_PROXYAUTH is defined in curl 7.11.0 or newer.
#if LIBCURL_VERSION_NUM >= 0x70B00
    else if( EQUAL(pszProxyAuth, "BASIC") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_BASIC );
    else if( EQUAL(pszProxyAuth, "NTLM") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_NTLM );
    else if( EQUAL(pszProxyAuth, "DIGEST") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_DIGEST );
    else if( EQUAL(pszProxyAuth, "ANY") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY );
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unsupported PROXYAUTH value '%s', ignored.",
                  pszProxyAuth );
    }
#else
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "PROXYAUTH option needs curl >= 7.11.0" );
    }
#endif

    // Enable following redirections.  Requires libcurl 7.10.1 at least.
    curl_easy_setopt(http_handle, CURLOPT_FOLLOWLOCATION, 1 );
    curl_easy_setopt(http_handle, CURLOPT_MAXREDIRS, 10 );
    curl_easy_setopt(http_handle, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL );

    // Set connect timeout.
    const char *pszConnectTimeout =
        CSLFetchNameValue( papszOptions, "CONNECTTIMEOUT" );
    if( pszConnectTimeout == NULL )
        pszConnectTimeout = CPLGetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", NULL);
    if( pszConnectTimeout != NULL )
        curl_easy_setopt(http_handle, CURLOPT_CONNECTTIMEOUT_MS,
                         static_cast<int>(1000 * CPLAtof(pszConnectTimeout)) );

    // Set timeout.
    const char *pszTimeout = CSLFetchNameValue( papszOptions, "TIMEOUT" );
    if( pszTimeout == NULL )
        pszTimeout = CPLGetConfigOption("GDAL_HTTP_TIMEOUT", NULL);
    if( pszTimeout != NULL )
        curl_easy_setopt(http_handle, CURLOPT_TIMEOUT_MS,
                         static_cast<int>(1000 * CPLAtof(pszTimeout)) );

    // Set low speed time and limit.
    const char *pszLowSpeedTime =
        CSLFetchNameValue( papszOptions, "LOW_SPEED_TIME" );
    if( pszLowSpeedTime == NULL )
        pszLowSpeedTime = CPLGetConfigOption("GDAL_HTTP_LOW_SPEED_TIME", NULL);
    if( pszLowSpeedTime != NULL )
    {
        curl_easy_setopt(http_handle, CURLOPT_LOW_SPEED_TIME,
                         atoi(pszLowSpeedTime) );

        const char *pszLowSpeedLimit =
            CSLFetchNameValue( papszOptions, "LOW_SPEED_LIMIT" );
        if( pszLowSpeedLimit == NULL )
            pszLowSpeedLimit =
                CPLGetConfigOption("GDAL_HTTP_LOW_SPEED_LIMIT", "1");
        curl_easy_setopt(http_handle, CURLOPT_LOW_SPEED_LIMIT,
                         atoi(pszLowSpeedLimit) );
    }

    /* Disable some SSL verification */
    const char *pszUnsafeSSL = CSLFetchNameValue( papszOptions, "UNSAFESSL" );
    if( pszUnsafeSSL == NULL )
        pszUnsafeSSL = CPLGetConfigOption("GDAL_HTTP_UNSAFESSL", NULL);
    if( pszUnsafeSSL != NULL && CPLTestBool(pszUnsafeSSL) )
    {
        curl_easy_setopt(http_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(http_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Custom path to SSL certificates.
    const char* pszCAInfo = CSLFetchNameValue( papszOptions, "CAINFO" );
    if( pszCAInfo == NULL )
        // Name of environment variable used by the curl binary
        pszCAInfo = CPLGetConfigOption("CURL_CA_BUNDLE", NULL);
    if( pszCAInfo == NULL )
        // Name of environment variable used by the curl binary (tested
        // after CURL_CA_BUNDLE
        pszCAInfo = CPLGetConfigOption("SSL_CERT_FILE", NULL);
#ifdef WIN32
    if( pszCAInfo == NULL )
    {
        pszCAInfo = CPLFindWin32CurlCaBundleCrt();
    }
#endif
    if( pszCAInfo != NULL )
    {
        curl_easy_setopt(http_handle, CURLOPT_CAINFO, pszCAInfo);
    }

    /* Set Referer */
    const char *pszReferer = CSLFetchNameValue(papszOptions, "REFERER");
    if( pszReferer != NULL )
        curl_easy_setopt(http_handle, CURLOPT_REFERER, pszReferer);

    /* Set User-Agent */
    const char *pszUserAgent = CSLFetchNameValue(papszOptions, "USERAGENT");
    if( pszUserAgent == NULL )
        pszUserAgent = CPLGetConfigOption("GDAL_HTTP_USERAGENT", NULL);
    if( pszUserAgent != NULL )
        curl_easy_setopt(http_handle, CURLOPT_USERAGENT, pszUserAgent);

    /* NOSIGNAL should be set to true for timeout to work in multithread
     * environments on Unix, requires libcurl 7.10 or more recent.
     * (this force avoiding the use of signal handlers)
     */
#if LIBCURL_VERSION_NUM >= 0x070A00
    curl_easy_setopt(http_handle, CURLOPT_NOSIGNAL, 1 );
#endif

    /* Set POST mode */
    const char* pszPost = CSLFetchNameValue( papszOptions, "POSTFIELDS" );
    if( pszPost != NULL )
    {
        CPLDebug("HTTP", "These POSTFIELDS were sent:%.4000s", pszPost);
        curl_easy_setopt(http_handle, CURLOPT_POST, 1 );
        curl_easy_setopt(http_handle, CURLOPT_POSTFIELDS, pszPost );
    }

    const char* pszCustomRequest =
        CSLFetchNameValue( papszOptions, "CUSTOMREQUEST" );
    if( pszCustomRequest != NULL )
    {
        curl_easy_setopt(http_handle, CURLOPT_CUSTOMREQUEST, pszCustomRequest );
    }

    const char* pszCookie = CSLFetchNameValue(papszOptions, "COOKIE");
    if( pszCookie == NULL )
        pszCookie = CPLGetConfigOption("GDAL_HTTP_COOKIE", NULL);
    if( pszCookie != NULL )
        curl_easy_setopt(http_handle, CURLOPT_COOKIE, pszCookie);

    struct curl_slist* headers = NULL;
    const char *pszHeaderFile = CSLFetchNameValue( papszOptions, "HEADER_FILE" );
    if( pszHeaderFile == NULL )
        pszHeaderFile = CPLGetConfigOption( "GDAL_HTTP_HEADER_FILE", NULL );
    if( pszHeaderFile != NULL )
    {
        VSILFILE *fp = NULL;
        // Do not allow /vsicurl/ access from /vsicurl because of GetCurlHandleFor()
        // e.g. "/vsicurl/,HEADER_FILE=/vsicurl/,url= " would cause use of
        // memory after free
        if( strstr(pszHeaderFile, "/vsicurl/") == NULL &&
            strstr(pszHeaderFile, "/vsis3/") == NULL &&
            strstr(pszHeaderFile, "/vsigs/") == NULL )
        {
            fp = VSIFOpenL( pszHeaderFile, "rb" );
        }
        if( fp == NULL )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot read %s", pszHeaderFile);
        }
        else
        {
            const char* pszLine = NULL;
            while( (pszLine = CPLReadLineL(fp)) != NULL )
            {
                headers = curl_slist_append(headers, pszLine);
            }
            VSIFCloseL(fp);
        }
    }
    return headers;
}
#endif  // def HAVE_CURL

/************************************************************************/
/*                           CPLHTTPEnabled()                           */
/************************************************************************/

/**
 * \brief Return if CPLHTTP services can be useful
 *
 * Those services depend on GDAL being build with libcurl support.
 *
 * @return TRUE if libcurl support is enabled
 */
int CPLHTTPEnabled()

{
#ifdef HAVE_CURL
    return TRUE;
#else
    return FALSE;
#endif
}

/************************************************************************/
/*                           CPLHTTPCleanup()                           */
/************************************************************************/

/**
 * \brief Cleanup function to call at application termination
 */
void CPLHTTPCleanup()

{
#ifdef HAVE_CURL
    if( !hSessionMapMutex )
        return;

    {
        CPLMutexHolder oHolder( &hSessionMapMutex );
        if( poSessionMap )
        {
            for( std::map<CPLString, CURL *>::iterator oIt =
                     poSessionMap->begin();
                 oIt != poSessionMap->end();
                 oIt++ )
            {
                curl_easy_cleanup( oIt->second );
            }
            delete poSessionMap;
            poSessionMap = NULL;
        }
    }

    // Not quite a safe sequence.
    CPLDestroyMutex( hSessionMapMutex );
    hSessionMapMutex = NULL;
#endif
}

/************************************************************************/
/*                        CPLHTTPDestroyResult()                        */
/************************************************************************/

/**
 * \brief Clean the memory associated with the return value of CPLHTTPFetch()
 *
 * @param psResult pointer to the return value of CPLHTTPFetch()
 */
void CPLHTTPDestroyResult( CPLHTTPResult *psResult )

{
    if( psResult )
    {
        CPLFree( psResult->pabyData );
        CPLFree( psResult->pszErrBuf );
        CPLFree( psResult->pszContentType );
        CSLDestroy( psResult->papszHeaders );

        for( int i = 0; i < psResult->nMimePartCount; i++ )
        {
            CSLDestroy( psResult->pasMimePart[i].papszHeaders );
        }
        CPLFree(psResult->pasMimePart);

        CPLFree( psResult );
    }
}

/************************************************************************/
/*                     CPLHTTPParseMultipartMime()                      */
/************************************************************************/

/**
 * \brief Parses a MIME multipart message.
 *
 * This function will iterate over each part and put it in a separate
 * element of the pasMimePart array of the provided psResult structure.
 *
 * @param psResult pointer to the return value of CPLHTTPFetch()
 * @return TRUE if the message contains MIME multipart message.
 */
int CPLHTTPParseMultipartMime( CPLHTTPResult *psResult )

{
/* -------------------------------------------------------------------- */
/*      Is it already done?                                             */
/* -------------------------------------------------------------------- */
    if( psResult->nMimePartCount > 0 )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Find the boundary setting in the content type.                  */
/* -------------------------------------------------------------------- */
    const char *pszBound = NULL;

    if( psResult->pszContentType != NULL )
        pszBound = strstr(psResult->pszContentType, "boundary=");

    if( pszBound == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to parse multi-part mime, no boundary setting." );
        return FALSE;
    }

    CPLString osBoundary;
    char **papszTokens =
        CSLTokenizeStringComplex( pszBound + 9, "\n ;",
                                  TRUE, FALSE );

    if( CSLCount(papszTokens) == 0 || strlen(papszTokens[0]) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to parse multi-part mime, boundary not parsable." );
        CSLDestroy( papszTokens );
        return FALSE;
    }

    osBoundary = "--";
    osBoundary += papszTokens[0];
    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Find the start of the first chunk.                              */
/* -------------------------------------------------------------------- */
    char *pszNext =
        strstr(reinterpret_cast<char *>(psResult->pabyData),
               osBoundary.c_str());

    if( pszNext == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No parts found." );
        return FALSE;
    }

    pszNext += osBoundary.size();
    while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        pszNext++;
    if( *pszNext == '\r' )
        pszNext++;
    if( *pszNext == '\n' )
        pszNext++;

/* -------------------------------------------------------------------- */
/*      Loop over parts...                                              */
/* -------------------------------------------------------------------- */
    while( true )
    {
        psResult->nMimePartCount++;
        psResult->pasMimePart = static_cast<CPLMimePart *>(
            CPLRealloc(psResult->pasMimePart,
                       sizeof(CPLMimePart) * psResult->nMimePartCount));

        CPLMimePart *psPart = psResult->pasMimePart+psResult->nMimePartCount-1;

        memset( psPart, 0, sizeof(CPLMimePart) );

/* -------------------------------------------------------------------- */
/*      Collect headers.                                                */
/* -------------------------------------------------------------------- */
        while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        {
            char *pszEOL = strstr(pszNext, "\n");

            if( pszEOL == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while parsing multipart content (at line %d)",
                         __LINE__);
                return FALSE;
            }

            *pszEOL = '\0';
            bool bRestoreAntislashR = false;
            if( pszEOL - pszNext > 1 && pszEOL[-1] == '\r' )
            {
                bRestoreAntislashR = true;
                pszEOL[-1] = '\0';
            }
            psPart->papszHeaders =
                CSLAddString( psPart->papszHeaders, pszNext );
            if( bRestoreAntislashR )
                pszEOL[-1] = '\r';
            *pszEOL = '\n';

            pszNext = pszEOL + 1;
        }

        if( *pszNext == '\r' )
            pszNext++;
        if( *pszNext == '\n' )
            pszNext++;

/* -------------------------------------------------------------------- */
/*      Work out the data block size.                                   */
/* -------------------------------------------------------------------- */
        psPart->pabyData = reinterpret_cast<GByte *>(pszNext);

        int nBytesAvail =
            psResult->nDataLen -
            static_cast<int>(
                pszNext - reinterpret_cast<char *>(psResult->pabyData));

        while( nBytesAvail > 0
               && (*pszNext != '-'
                   || strncmp(pszNext, osBoundary, osBoundary.size()) != 0) )
        {
            pszNext++;
            nBytesAvail--;
        }

        if( nBytesAvail == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error while parsing multipart content (at line %d)",
                     __LINE__);
            return FALSE;
        }

        psPart->nDataLen =
            static_cast<int>(
                pszNext - reinterpret_cast<char *>(psPart->pabyData));
        pszNext += osBoundary.size();

        if( STARTS_WITH(pszNext, "--") )
        {
            break;
        }

        if( *pszNext == '\r' )
            pszNext++;
        if( *pszNext == '\n' )
            pszNext++;
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error while parsing multipart content (at line %d)",
                     __LINE__);
            return FALSE;
        }
    }

    return TRUE;
}
