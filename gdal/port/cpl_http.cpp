/******************************************************************************
 * $Id$
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
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

#include <map>
#include "cpl_http.h"
#include "cpl_multiproc.h"

#ifdef HAVE_CURL
#  include <curl/curl.h>
#endif

CPL_CVSID("$Id$");

// list of named persistent http sessions 

#ifdef HAVE_CURL
static std::map<CPLString,CURL*> oSessionMap;
static void *hSessionMapMutex = NULL;
#endif

/************************************************************************/
/*                            CPLWriteFct()                             */
/*                                                                      */
/*      Append incoming text to our collection buffer, reallocating     */
/*      it larger as needed.                                            */
/************************************************************************/

#ifdef HAVE_CURL
static size_t 
CPLWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)

{
    CPLHTTPResult *psResult = (CPLHTTPResult *) reqInfo;
    int  nNewSize;

    nNewSize = psResult->nDataLen + nmemb*size + 1;
    if( nNewSize > psResult->nDataAlloc )
    {
        psResult->nDataAlloc = (int) (nNewSize * 1.25 + 100);
        GByte* pabyNewData = (GByte *) VSIRealloc(psResult->pabyData,
                                                  psResult->nDataAlloc);
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

    memcpy( psResult->pabyData + psResult->nDataLen, buffer,
            nmemb * size );

    psResult->nDataLen += nmemb * size;
    psResult->pabyData[psResult->nDataLen] = 0;

    return nmemb;
}

/************************************************************************/
/*                           CPLHdrWriteFct()                           */
/************************************************************************/
static size_t CPLHdrWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)
{
    CPLHTTPResult *psResult = (CPLHTTPResult *) reqInfo;
    // copy the buffer to a char* and initialize with zeros (zero terminate as well)
    char* pszHdr = (char*)CPLCalloc(nmemb + 1, size);
    CPLPrintString(pszHdr, (char *)buffer, nmemb * size);
    char *pszKey = NULL;
    const char *pszValue = CPLParseNameValue(pszHdr, &pszKey );
    psResult->papszHeaders = CSLSetNameValue(psResult->papszHeaders, pszKey, pszValue);
    CPLFree(pszHdr);
    CPLFree(pszKey);
    return nmemb; 
}

#endif /* def HAVE_CURL */

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
 * <li>TIMEOUT=val, where val is in seconds</li>
 * <li>HEADERS=val, where val is an extra header to use when getting a web page.
 *                  For example "Accept: application/x-ogcwkt"
 * <li>HTTPAUTH=[BASIC/NTLM/ANY] to specify an authentication scheme to use.
 * <li>USERPWD=userid:password to specify a user and password for authentication
 * </ul>
 *
 * @return a CPLHTTPResult* structure that must be freed by CPLHTTPDestroyResult(),
 *         or NULL if libcurl support is diabled
 */
CPLHTTPResult *CPLHTTPFetch( const char *pszURL, char **papszOptions )

{
#ifndef HAVE_CURL
    CPLError( CE_Failure, CPLE_NotSupported,
              "GDAL/OGR not compiled with libcurl support, remote requests not supported." );
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
    if (pszPersistent)
    {
        CPLString osSessionName = pszPersistent;
        CPLMutexHolder oHolder( &hSessionMapMutex );

        if( oSessionMap.count( osSessionName ) == 0 )
        {
            oSessionMap[osSessionName] = curl_easy_init();
            CPLDebug( "HTTP", "Establish persistent session named '%s'.",
                      osSessionName.c_str() );
        }

        http_handle = oSessionMap[osSessionName];
    }
    else
        http_handle = curl_easy_init();

/* -------------------------------------------------------------------- */
/*      Setup the request.                                              */
/* -------------------------------------------------------------------- */
    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    CPLHTTPResult *psResult;
    struct curl_slist *headers=NULL; 

    CPLDebug( "HTTP", "Fetch(%s)", pszURL );

    psResult = (CPLHTTPResult *) CPLCalloc(1,sizeof(CPLHTTPResult));

    curl_easy_setopt(http_handle, CURLOPT_URL, pszURL );

    /* Support control over HTTPAUTH */
    const char *pszHttpAuth = CSLFetchNameValue( papszOptions, "HTTPAUTH" );
    if( pszHttpAuth == NULL )
        /* do nothing */;

    /* CURLOPT_HTTPAUTH is defined in curl 7.11.0 or newer */
#if LIBCURL_VERSION_NUM >= 0x70B00
    else if( EQUAL(pszHttpAuth,"BASIC") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    else if( EQUAL(pszHttpAuth,"NTLM") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_NTLM );
    else if( EQUAL(pszHttpAuth,"ANY") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
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

    /* Support setting userid:password */
    const char *pszUserPwd = CSLFetchNameValue( papszOptions, "USERPWD" );
    if( pszUserPwd != NULL )
        curl_easy_setopt(http_handle, CURLOPT_USERPWD, pszUserPwd );

    // turn off SSL verification, accept all servers with ssl
    curl_easy_setopt(http_handle, CURLOPT_SSL_VERIFYPEER, FALSE);

    /* Enable following redirections.  Requires libcurl 7.10.1 at least */
    curl_easy_setopt(http_handle, CURLOPT_FOLLOWLOCATION, 1 );
    curl_easy_setopt(http_handle, CURLOPT_MAXREDIRS, 10 );
    
    /* Set timeout.*/
    const char *pszTimeout = CSLFetchNameValue( papszOptions, "TIMEOUT" );
    if( pszTimeout != NULL )
        curl_easy_setopt(http_handle, CURLOPT_TIMEOUT, 
                         atoi(pszTimeout) );

    /* Set Headers.*/
    const char *pszHeaders = CSLFetchNameValue( papszOptions, "HEADERS" );
    if( pszHeaders != NULL ) {
        CPLDebug ("HTTP", "These HTTP headers were set: %s", pszHeaders);
        headers = curl_slist_append(headers, pszHeaders);
        curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, headers);
    }
                         
    /* NOSIGNAL should be set to true for timeout to work in multithread
     * environments on Unix, requires libcurl 7.10 or more recent.
     * (this force avoiding the use of sgnal handlers)
     */
#ifdef CURLOPT_NOSIGNAL
    curl_easy_setopt(http_handle, CURLOPT_NOSIGNAL, 1 );
#endif

    // capture response headers
    curl_easy_setopt(http_handle, CURLOPT_HEADERDATA, psResult);
    curl_easy_setopt(http_handle, CURLOPT_HEADERFUNCTION, CPLHdrWriteFct);
 
    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, psResult );
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, CPLWriteFct );

    szCurlErrBuf[0] = '\0';

    curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

/* -------------------------------------------------------------------- */
/*      Execute the request, waiting for results.                       */
/* -------------------------------------------------------------------- */
    psResult->nStatus = (int) curl_easy_perform( http_handle );

/* -------------------------------------------------------------------- */
/*      Fetch content-type if possible.                                 */
/* -------------------------------------------------------------------- */
    CURLcode err;

    psResult->pszContentType = NULL;
    err = curl_easy_getinfo( http_handle, CURLINFO_CONTENT_TYPE, 
                             &(psResult->pszContentType) );
    if( psResult->pszContentType != NULL )
        psResult->pszContentType = CPLStrdup(psResult->pszContentType);

/* -------------------------------------------------------------------- */
/*      Have we encountered some sort of error?                         */
/* -------------------------------------------------------------------- */
    if( strlen(szCurlErrBuf) > 0 )
    {
        psResult->pszErrBuf = CPLStrdup(szCurlErrBuf);
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", szCurlErrBuf );
    }

    if (!pszPersistent)
        curl_easy_cleanup( http_handle );

    curl_slist_free_all(headers);

    return psResult;
#endif /* def HAVE_CURL */
}

/************************************************************************/
/*                           CPLHTTPEnabled()                           */
/************************************************************************/

/**
 * \brief Return if CPLHTTP services can be usefull
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
        std::map<CPLString,CURL*>::iterator oIt;

        for( oIt=oSessionMap.begin(); oIt != oSessionMap.end(); oIt++ )
            curl_easy_cleanup( oIt->second );

        oSessionMap.clear();
    }

    // not quite a safe sequence. 
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
        CPLFree( psResult );
    }
}

/************************************************************************/
/*                     CPLHTTPParseMultipartMime()                      */
/************************************************************************/

/**
 * \brief Parses a a MIME multipart message
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
        pszBound = strstr(psResult->pszContentType,"boundary=");

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
        return FALSE;
    }
    
    osBoundary = "--";
    osBoundary += papszTokens[0];
    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Find the start of the first chunk.                              */
/* -------------------------------------------------------------------- */
    char *pszNext;
    pszNext = (char *) 
        strstr((const char *) psResult->pabyData,osBoundary.c_str());
    
    if( pszNext == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No parts found." );
        return FALSE;
    }

    pszNext += strlen(osBoundary);
    while( *pszNext != '\n' && *pszNext != '\0' )
        pszNext++;
    if( *pszNext == '\n' )
        pszNext++;

/* -------------------------------------------------------------------- */
/*      Loop over parts...                                              */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        psResult->nMimePartCount++;
        psResult->pasMimePart = (CPLMimePart *)
            CPLRealloc(psResult->pasMimePart,
                       sizeof(CPLMimePart) * psResult->nMimePartCount );

        CPLMimePart *psPart = psResult->pasMimePart+psResult->nMimePartCount-1;

        memset( psPart, 0, sizeof(CPLMimePart) );

/* -------------------------------------------------------------------- */
/*      Collect headers.                                                */
/* -------------------------------------------------------------------- */
        while( *pszNext != '\n' && *pszNext != '\0' )
        {
            char *pszEOL = strstr(pszNext,"\n");

            if( pszEOL == NULL )
            {
                CPLAssert( FALSE );
                break;
            }

            *pszEOL = '\0';
            psPart->papszHeaders = 
                CSLAddString( psPart->papszHeaders, pszNext );
            *pszEOL = '\n';
            
            pszNext = pszEOL + 1;
        }

        if( *pszNext == '\n' )
            pszNext++;
            
/* -------------------------------------------------------------------- */
/*      Work out the data block size.                                   */
/* -------------------------------------------------------------------- */
        psPart->pabyData = (GByte *) pszNext;

        int nBytesAvail = psResult->nDataLen - 
            (pszNext - (const char *) psResult->pabyData);

        while( nBytesAvail > 0
               && (*pszNext != '-' 
                   || strncmp(pszNext,osBoundary,strlen(osBoundary)) != 0) )
        {
            pszNext++;
            nBytesAvail--;
        }
        
        if( nBytesAvail == 0 )
        {
            CPLAssert( FALSE );
            break;
        }

        psPart->nDataLen = pszNext - (const char *) psPart->pabyData;
        pszNext += strlen(osBoundary);

        if( strncmp(pszNext,"--",2) == 0 )
        {
            break;
        }
        else if( *pszNext == '\n' )
            pszNext++;
        else
        {
            CPLAssert( FALSE );
            break;
        }
    }

    return TRUE;
}
