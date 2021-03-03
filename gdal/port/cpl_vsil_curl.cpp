/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HTTP/FTP files
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2018, Even Rouault <even.rouault at spatialys.com>
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
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <algorithm>
#include <array>
#include <set>
#include <map>
#include <memory>

#include "cpl_aws.h"
#include "cpl_json.h"
#include "cpl_json_header.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_http.h"
#include "cpl_mem_cache.h"

#ifndef S_IRUSR
#define S_IRUSR     00400
#define S_IWUSR     00200
#define S_IXUSR     00100
#define S_IRGRP     00040
#define S_IWGRP     00020
#define S_IXGRP     00010
#define S_IROTH     00004
#define S_IWOTH     00002
#define S_IXOTH     00001
#endif

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallCurlFileHandler( void )
{
    // Not supported.
}

void VSICurlClearCache( void )
{
    // Not supported.
}

void VSICurlPartialClearCache(const char* )
{
    // Not supported.
}

void VSICurlAuthParametersChanged()
{
    // Not supported.
}

void VSINetworkStatsReset( void )
{
    // Not supported
}

char *VSINetworkStatsGetAsSerializedJSON( char** /* papszOptions */ )
{
    // Not supported
    return nullptr;
}


/************************************************************************/
/*                      VSICurlInstallReadCbk()                         */
/************************************************************************/

int VSICurlInstallReadCbk ( VSILFILE* /* fp */,
                            VSICurlReadCbkFunc /* pfnReadCbk */,
                            void* /* pfnUserData */,
                            int /* bStopOnInterruptUntilUninstall */)
{
    return FALSE;
}

/************************************************************************/
/*                    VSICurlUninstallReadCbk()                         */
/************************************************************************/

int VSICurlUninstallReadCbk( VSILFILE* /* fp */ )
{
    return FALSE;
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 1
#define ENABLE_DEBUG_VERBOSE 0

/***********************************************************ù************/
/*                    VSICurlAuthParametersChanged()                    */
/************************************************************************/

static unsigned int gnGenerationAuthParameters = 0;

void VSICurlAuthParametersChanged()
{
    gnGenerationAuthParameters++;
}

namespace cpl {

// Do not access those 2 variables directly !
// Use VSICURLGetDownloadChunkSize() and GetMaxRegions()
static int N_MAX_REGIONS_DO_NOT_USE_DIRECTLY = 1000;
static int DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY = 16384;

/************************************************************************/
/*                    VSICURLReadGlobalEnvVariables()                   */
/************************************************************************/

static void VSICURLReadGlobalEnvVariables()
{
    struct Initializer
    {
        Initializer()
        {
            DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY = atoi(
                    CPLGetConfigOption("CPL_VSIL_CURL_CHUNK_SIZE", "16384"));
            if( DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY < 1024 ||
                DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY > 10 * 1024* 1024 )
                DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY = 16384;

            GIntBig nCacheSize = CPLAtoGIntBig(
                CPLGetConfigOption("CPL_VSIL_CURL_CACHE_SIZE", "16384000"));
            if( nCacheSize < DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY ||
                nCacheSize / DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY > INT_MAX )
            {
                nCacheSize = 16384000;
            }
            N_MAX_REGIONS_DO_NOT_USE_DIRECTLY = std::max(1,
                static_cast<int>(nCacheSize / DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY));
        }
    };
    static Initializer initializer;
}

/************************************************************************/
/*                     VSICURLGetDownloadChunkSize()                    */
/************************************************************************/

int VSICURLGetDownloadChunkSize()
{
    VSICURLReadGlobalEnvVariables();
    return DOWNLOAD_CHUNK_SIZE_DO_NOT_USE_DIRECTLY;
}

/************************************************************************/
/*                            GetMaxRegions()                           */
/************************************************************************/

static int GetMaxRegions()
{
    VSICURLReadGlobalEnvVariables();
    return N_MAX_REGIONS_DO_NOT_USE_DIRECTLY;
}


/************************************************************************/
/*          VSICurlFindStringSensitiveExceptEscapeSequences()           */
/************************************************************************/

static int
VSICurlFindStringSensitiveExceptEscapeSequences( char ** papszList,
                                                 const char * pszTarget )

{
    if( papszList == nullptr )
        return -1;

    for( int i = 0; papszList[i] != nullptr; i++ )
    {
        const char* pszIter1 = papszList[i];
        const char* pszIter2 = pszTarget;
        char ch1 = '\0';
        char ch2 = '\0';
        /* The comparison is case-sensitive, escape for escaped */
        /* sequences where letters of the hexadecimal sequence */
        /* can be uppercase or lowercase depending on the quoting algorithm */
        while( true )
        {
            ch1 = *pszIter1;
            ch2 = *pszIter2;
            if( ch1 == '\0' || ch2 == '\0' )
                break;
            if( ch1 == '%' && ch2 == '%' &&
                pszIter1[1] != '\0' && pszIter1[2] != '\0' &&
                pszIter2[1] != '\0' && pszIter2[2] != '\0' )
            {
                if( !EQUALN(pszIter1+1, pszIter2+1, 2) )
                    break;
                pszIter1 += 2;
                pszIter2 += 2;
            }
            if( ch1 != ch2 )
                break;
            pszIter1++;
            pszIter2++;
        }
        if( ch1 == ch2 && ch1 == '\0' )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                      VSICurlIsFileInList()                           */
/************************************************************************/

static int VSICurlIsFileInList( char ** papszList, const char * pszTarget )
{
    int nRet =
        VSICurlFindStringSensitiveExceptEscapeSequences(papszList, pszTarget);
    if( nRet >= 0 )
        return nRet;

    // If we didn't find anything, try to URL-escape the target filename.
    char* pszEscaped = CPLEscapeString(pszTarget, -1, CPLES_URL);
    if( strcmp(pszTarget, pszEscaped) != 0 )
    {
        nRet = VSICurlFindStringSensitiveExceptEscapeSequences(papszList,
                                                               pszEscaped);
    }
    CPLFree(pszEscaped);
    return nRet;
}

/************************************************************************/
/*                      VSICurlGetURLFromFilename()                     */
/************************************************************************/

static CPLString VSICurlGetURLFromFilename(const char* pszFilename,
                                           int* pnMaxRetry,
                                           double* pdfRetryDelay,
                                           bool* pbUseHead,
                                           bool* pbListDir,
                                           bool* pbEmptyDir,
                                           char*** ppapszHTTPOptions)
{
    if( !STARTS_WITH(pszFilename, "/vsicurl/") &&
        !STARTS_WITH(pszFilename, "/vsicurl?") )
        return pszFilename;
    pszFilename += strlen("/vsicurl/");
    if( !STARTS_WITH(pszFilename, "http://") &&
        !STARTS_WITH(pszFilename, "https://") &&
        !STARTS_WITH(pszFilename, "ftp://") &&
        !STARTS_WITH(pszFilename, "file://") )
    {
        if( *pszFilename == '?' )
            pszFilename ++;
        char** papszTokens = CSLTokenizeString2( pszFilename, "&", 0 );
        for( int i = 0; papszTokens[i] != nullptr; i++ )
        {
            char* pszUnescaped = CPLUnescapeString( papszTokens[i], nullptr,
                                                    CPLES_URL );
            CPLFree(papszTokens[i]);
            papszTokens[i] = pszUnescaped;
        }

        CPLString osURL;
        for( int i = 0; papszTokens[i]; i++ )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(papszTokens[i], &pszKey);
            if( pszKey && pszValue )
            {
                if( EQUAL(pszKey, "max_retry") )
                {
                    if( pnMaxRetry )
                        *pnMaxRetry = atoi(pszValue);
                }
                else if( EQUAL(pszKey, "retry_delay") )
                {
                    if( pdfRetryDelay )
                        *pdfRetryDelay = CPLAtof(pszValue);
                }
                    else if( EQUAL(pszKey, "use_head") )
                {
                    if( pbUseHead )
                        *pbUseHead = CPLTestBool(pszValue);
                }
                else if( EQUAL(pszKey, "list_dir") )
                {
                    if( pbListDir )
                        *pbListDir = CPLTestBool(pszValue);
                }
                else if( EQUAL(pszKey, "empty_dir") )
                {
                    /* Undocumented. Used by PLScenes driver */
                    /* This more or less emulates the behavior of
                        * GDAL_DISABLE_READDIR_ON_OPEN=EMPTY_DIR */
                    if( pbEmptyDir )
                        *pbEmptyDir = CPLTestBool(pszValue);
                }
                else if( EQUAL(pszKey, "useragent") ||
                            EQUAL(pszKey, "referer") ||
                            EQUAL(pszKey, "cookie") ||
                            EQUAL(pszKey, "header_file") ||
                            EQUAL(pszKey, "unsafessl") ||
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                            EQUAL(pszKey, "timeout") ||
                            EQUAL(pszKey, "connecttimeout") ||
#endif
                            EQUAL(pszKey, "low_speed_time") ||
                            EQUAL(pszKey, "low_speed_limit") ||
                            EQUAL(pszKey, "proxy") ||
                            EQUAL(pszKey, "proxyauth") ||
                            EQUAL(pszKey, "proxyuserpwd") )
                {
                    // Above names are the ones supported by
                    // CPLHTTPSetOptions()
                    if( ppapszHTTPOptions )
                    {
                        *ppapszHTTPOptions = CSLSetNameValue(
                            *ppapszHTTPOptions, pszKey, pszValue);
                    }
                }
                else if( EQUAL(pszKey, "url") )
                {
                    osURL = pszValue;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                                "Unsupported option: %s", pszKey);
                }
            }
            CPLFree(pszKey);
        }

        CSLDestroy(papszTokens);
        if( osURL.empty() )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Missing url parameter");
            return pszFilename;
        }

        return osURL;
    }

    return pszFilename;
}

/************************************************************************/
/*                           VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::VSICurlHandle( VSICurlFilesystemHandler* poFSIn,
                              const char* pszFilename,
                              const char* pszURLIn ) :
    poFS(poFSIn),
    m_osFilename(pszFilename),
    m_nMaxRetry(atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)))),
    // coverity[tainted_data]
    m_dfRetryDelay(CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)))),
    m_bUseHead(CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD",
                                             "YES")))
{
    m_papszHTTPOptions = CPLHTTPGetOptionsFromEnv();
    if( pszURLIn )
    {
        m_pszURL = CPLStrdup(pszURLIn);
    }
    else
    {
        m_pszURL = CPLStrdup(VSICurlGetURLFromFilename(pszFilename,
                                                       &m_nMaxRetry,
                                                       &m_dfRetryDelay,
                                                       &m_bUseHead,
                                                       nullptr, nullptr,
                                                       &m_papszHTTPOptions));
    }

    m_bCached = poFSIn->AllowCachedDataFor(pszFilename);
    poFS->GetCachedFileProp(m_pszURL, oFileProp);
}

/************************************************************************/
/*                          ~VSICurlHandle()                            */
/************************************************************************/

VSICurlHandle::~VSICurlHandle()
{
    if( !m_bCached )
    {
        poFS->InvalidateCachedData(m_pszURL);
        poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );
    }
    CPLFree(m_pszURL);
    CSLDestroy(m_papszHTTPOptions);
}

/************************************************************************/
/*                            SetURL()                                  */
/************************************************************************/

void VSICurlHandle::SetURL(const char* pszURLIn)
{
    CPLFree(m_pszURL);
    m_pszURL = CPLStrdup(pszURLIn);
}

/************************************************************************/
/*                          InstallReadCbk()                            */
/************************************************************************/

int VSICurlHandle::InstallReadCbk( VSICurlReadCbkFunc pfnReadCbkIn,
                                   void* pfnUserDataIn,
                                   int bStopOnInterruptUntilUninstallIn )
{
    if( pfnReadCbk != nullptr )
        return FALSE;

    pfnReadCbk = pfnReadCbkIn;
    pReadCbkUserData = pfnUserDataIn;
    bStopOnInterruptUntilUninstall =
        CPL_TO_BOOL(bStopOnInterruptUntilUninstallIn);
    bInterrupted = false;
    return TRUE;
}

/************************************************************************/
/*                         UninstallReadCbk()                           */
/************************************************************************/

int VSICurlHandle::UninstallReadCbk()
{
    if( pfnReadCbk == nullptr )
        return FALSE;

    pfnReadCbk = nullptr;
    pReadCbkUserData = nullptr;
    bStopOnInterruptUntilUninstall = false;
    bInterrupted = false;
    return TRUE;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICurlHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( nWhence == SEEK_SET )
    {
        curOffset = nOffset;
    }
    else if( nWhence == SEEK_CUR )
    {
        curOffset = curOffset + nOffset;
    }
    else
    {
        curOffset = GetFileSize(false) + nOffset;
    }
    bEOF = false;
    return 0;
}

/************************************************************************/
/*                 VSICurlGetTimeStampFromRFC822DateTime()              */
/************************************************************************/

static GIntBig VSICurlGetTimeStampFromRFC822DateTime( const char* pszDT )
{
    // Sun, 03 Apr 2016 12:07:27 GMT
    if( strlen(pszDT) >= 5 && pszDT[3] == ',' && pszDT[4] == ' ' )
        pszDT += 5;
    int nDay = 0;
    int nYear = 0;
    int nHour = 0;
    int nMinute = 0;
    int nSecond = 0;
    char szMonth[4] = {};
    szMonth[3] = 0;
    if( sscanf(pszDT, "%02d %03s %04d %02d:%02d:%02d GMT",
                &nDay, szMonth, &nYear, &nHour, &nMinute, &nSecond) == 6 )
    {
        static const char* const aszMonthStr[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

        int nMonthIdx0 = -1;
        for( int i = 0; i < 12; i++ )
        {
            if( EQUAL(szMonth, aszMonthStr[i]) )
            {
                nMonthIdx0 = i;
                break;
            }
        }
        if( nMonthIdx0 >= 0 )
        {
            struct tm brokendowntime;
            brokendowntime.tm_year = nYear - 1900;
            brokendowntime.tm_mon = nMonthIdx0;
            brokendowntime.tm_mday = nDay;
            brokendowntime.tm_hour = nHour;
            brokendowntime.tm_min = nMinute;
            brokendowntime.tm_sec = nSecond;
            return CPLYMDHMSToUnixTime(&brokendowntime);
        }
    }
    return 0;
}

/************************************************************************/
/*                    VSICURLInitWriteFuncStruct()                      */
/************************************************************************/

void VSICURLInitWriteFuncStruct( WriteFuncStruct   *psStruct,
                                        VSILFILE          *fp,
                                        VSICurlReadCbkFunc pfnReadCbk,
                                        void              *pReadCbkUserData )
{
    psStruct->pBuffer = nullptr;
    psStruct->nSize = 0;
    psStruct->bIsHTTP = false;
    psStruct->bIsInHeader = true;
    psStruct->bMultiRange = false;
    psStruct->nStartOffset = 0;
    psStruct->nEndOffset = 0;
    psStruct->nHTTPCode = 0;
    psStruct->nContentLength = 0;
    psStruct->bFoundContentRange = false;
    psStruct->bError = false;
    psStruct->bDownloadHeaderOnly = false;
    psStruct->bDetectRangeDownloadingError = true;
    psStruct->nTimestampDate = 0;

    psStruct->fp = fp;
    psStruct->pfnReadCbk = pfnReadCbk;
    psStruct->pReadCbkUserData = pReadCbkUserData;
    psStruct->bInterrupted = false;

#if LIBCURL_VERSION_NUM < 0x073600
    psStruct->bIsProxyConnectHeader = false;
#endif
}

/************************************************************************/
/*                       VSICurlHandleWriteFunc()                       */
/************************************************************************/

size_t VSICurlHandleWriteFunc( void *buffer, size_t count,
                                      size_t nmemb, void *req )
{
    WriteFuncStruct* psStruct = static_cast<WriteFuncStruct *>(req);
    const size_t nSize = count * nmemb;

    char* pNewBuffer = static_cast<char *>(
        VSIRealloc(psStruct->pBuffer, psStruct->nSize + nSize + 1));
    if( pNewBuffer )
    {
        psStruct->pBuffer = pNewBuffer;
        memcpy(psStruct->pBuffer + psStruct->nSize, buffer, nSize);
        psStruct->pBuffer[psStruct->nSize + nSize] = '\0';
        if( psStruct->bIsHTTP && psStruct->bIsInHeader )
        {
            char* pszLine = psStruct->pBuffer + psStruct->nSize;
            if( STARTS_WITH_CI(pszLine, "HTTP/") )
            {
                char* pszSpace = strchr(pszLine, ' ');
                if( pszSpace )
                {
                    psStruct->nHTTPCode = atoi(pszSpace + 1);

#if LIBCURL_VERSION_NUM < 0x073600
                    // Workaround to ignore extra HTTP response headers from
                    // proxies in older versions of curl.
                    // CURLOPT_SUPPRESS_CONNECT_HEADERS fixes this
                    if( psStruct->nHTTPCode >= 200 &&
                        psStruct->nHTTPCode < 300 )
                    {
                        pszSpace = strchr(pszSpace + 1, ' ');
                        if( pszSpace &&
                            // This could be any string really, but we don't
                            // have an easy way to distinguish between proxies
                            // and upstream responses...
                            STARTS_WITH_CI( pszSpace + 1,
                                            "Connection established") )
                        {
                            psStruct->bIsProxyConnectHeader = true;
                        }
                    }
#endif
                }
            }
            else if( STARTS_WITH_CI(pszLine, "Content-Length: ") )
            {
                psStruct->nContentLength =
                    CPLScanUIntBig(pszLine + 16,
                                   static_cast<int>(strlen(pszLine + 16)));
            }
            else if( STARTS_WITH_CI(pszLine, "Content-Range: ") )
            {
                psStruct->bFoundContentRange = true;
            }
            else if( STARTS_WITH_CI(pszLine, "Date: ") )
            {
                CPLString osDate = pszLine + strlen("Date: ");
                size_t nSizeLine = osDate.size();
                while( nSizeLine &&
                       (osDate[nSizeLine-1] == '\r' ||
                        osDate[nSizeLine-1] == '\n') )
                {
                    osDate.resize(nSizeLine-1);
                    nSizeLine--;
                }
                osDate.Trim();

                GIntBig nTimestampDate =
                    VSICurlGetTimeStampFromRFC822DateTime(osDate);
#if DEBUG_VERBOSE
                CPLDebug("VSICURL",
                         "Timestamp = " CPL_FRMT_GIB, nTimestampDate);
#endif
                psStruct->nTimestampDate = nTimestampDate;
            }
            /*if( nSize > 2 && pszLine[nSize - 2] == '\r' &&
                  pszLine[nSize - 1] == '\n' )
            {
                pszLine[nSize - 2] = 0;
                CPLDebug("VSICURL", "%s", pszLine);
                pszLine[nSize - 2] = '\r';
            }*/

            if( pszLine[0] == '\r' || pszLine[0] == '\n' )
            {
                if( psStruct->bDownloadHeaderOnly )
                {
                    // If moved permanently/temporarily, go on.
                    // Otherwise stop now,
                    if( !(psStruct->nHTTPCode == 301 ||
                          psStruct->nHTTPCode == 302) )
                        return 0;
                }
#if LIBCURL_VERSION_NUM < 0x073600
                else if( psStruct->bIsProxyConnectHeader )
                {
                    psStruct->bIsProxyConnectHeader = false;
                }
#endif
                else
                {
                    psStruct->bIsInHeader = false;

                    // Detect servers that don't support range downloading.
                    if( psStruct->nHTTPCode == 200 &&
                        psStruct->bDetectRangeDownloadingError &&
                        !psStruct->bMultiRange &&
                        !psStruct->bFoundContentRange &&
                        (psStruct->nStartOffset != 0 ||
                         psStruct->nContentLength > 10 *
                         (psStruct->nEndOffset - psStruct->nStartOffset + 1)) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Range downloading not supported by this "
                                 "server!");
                        psStruct->bError = true;
                        return 0;
                    }
                }
            }
        }
        else
        {
            if( psStruct->pfnReadCbk )
            {
                if( !psStruct->pfnReadCbk(psStruct->fp, buffer, nSize,
                                          psStruct->pReadCbkUserData) )
                {
                    psStruct->bInterrupted = true;
                    return 0;
                }
            }
        }
        psStruct->nSize += nSize;
        return nmemb;
    }
    else
    {
        return 0;
    }
}

/************************************************************************/
/*                    VSICurlIsS3LikeSignedURL()                        */
/************************************************************************/

static bool VSICurlIsS3LikeSignedURL( const char* pszURL )
{
    return
        (strstr(pszURL, ".s3.amazonaws.com/") != nullptr ||
         strstr(pszURL, ".s3.amazonaws.com:") != nullptr ||
         strstr(pszURL, ".storage.googleapis.com/") != nullptr ||
         strstr(pszURL, ".storage.googleapis.com:") != nullptr) &&
        (strstr(pszURL, "&Signature=") != nullptr ||
         strstr(pszURL, "?Signature=") != nullptr ||
         strstr(pszURL, "&X-Amz-Signature=") != nullptr ||
         strstr(pszURL, "?X-Amz-Signature=") != nullptr);
}

/************************************************************************/
/*                  VSICurlGetExpiresFromS3LikeSignedURL()              */
/************************************************************************/

static GIntBig VSICurlGetExpiresFromS3LikeSignedURL( const char* pszURL )
{
    const char* pszExpires = strstr(pszURL, "&Expires=");
    if( pszExpires == nullptr )
        pszExpires = strstr(pszURL, "?Expires=");
    if( pszExpires != nullptr )
        return CPLAtoGIntBig(pszExpires + strlen("&Expires="));

    pszExpires = strstr(pszURL, "?X-Amz-Expires=");
    if( pszExpires == nullptr )
        pszExpires = strstr(pszURL, "?X-Amz-Expires=");
    if( pszExpires != nullptr )
        return CPLAtoGIntBig(pszExpires + strlen("&X-Amz-Expires="));

    return 0;
}

/************************************************************************/
/*                           MultiPerform()                             */
/************************************************************************/

void MultiPerform(CURLM* hCurlMultiHandle, CURL* hEasyHandle)
{
    int repeats = 0;

    if( hEasyHandle )
        curl_multi_add_handle(hCurlMultiHandle, hEasyHandle);

    void* old_handler = CPLHTTPIgnoreSigPipe();
    while( true )
    {
        int still_running;
        while (curl_multi_perform(hCurlMultiHandle, &still_running) ==
                                        CURLM_CALL_MULTI_PERFORM )
        {
            // loop
        }
        if( !still_running )
        {
            break;
        }

#ifdef undef
        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(hCurlMultiHandle, &msgq);
            if(msg && (msg->msg == CURLMSG_DONE))
            {
                CURL *e = msg->easy_handle;
            }
        } while(msg);
#endif

        CPLMultiPerformWait(hCurlMultiHandle, repeats);
    }
    CPLHTTPRestoreSigPipeHandler(old_handler);

    if( hEasyHandle )
        curl_multi_remove_handle(hCurlMultiHandle, hEasyHandle);
}

/************************************************************************/
/*                       VSICurlDummyWriteFunc()                        */
/************************************************************************/

static size_t VSICurlDummyWriteFunc( void *, size_t , size_t , void * )
{
    return 0;
}

/************************************************************************/
/*                  VSICURLResetHeaderAndWriterFunctions()              */
/************************************************************************/

void VSICURLResetHeaderAndWriterFunctions(CURL* hCurlHandle)
{
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlDummyWriteFunc);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlDummyWriteFunc);
}

/************************************************************************/
/*                     GetFileSizeOrHeaders()                           */
/************************************************************************/

vsi_l_offset VSICurlHandle::GetFileSizeOrHeaders( bool bSetError, bool bGetHeaders )
{
    if( oFileProp.bHasComputedFileSize && !bGetHeaders )
        return oFileProp.fileSize;

    NetworkStatisticsFileSystem oContextFS(poFS->GetFSPrefix());
    NetworkStatisticsFile oContextFile(m_osFilename);
    NetworkStatisticsAction oContextAction("GetFileSize");

    oFileProp.bHasComputedFileSize = true;

    CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(m_pszURL);

    CPLString osURL(m_pszURL + m_osQueryString);
    bool bRetryWithGet = false;
    bool bS3LikeRedirect = false;
    int nRetryCount = 0;
    double dfRetryDelay = m_dfRetryDelay;

retry:
    CURL* hCurlHandle = curl_easy_init();

    struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osURL, m_papszHTTPOptions);

    WriteFuncStruct sWriteFuncHeaderData;
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);

    CPLString osVerb;
    CPLString osRange; // leave in this scope !
    int nRoundedBufSize = 0;
    const int knDOWNLOAD_CHUNK_SIZE = VSICURLGetDownloadChunkSize();
    if( UseLimitRangeGetInsteadOfHead() )
    {
        osVerb = "GET";
        const int nBufSize = std::max(1024, std::min(10 * 1024 * 1024,
            atoi(CPLGetConfigOption("GDAL_INGESTED_BYTES_AT_OPEN", "1024"))));
        nRoundedBufSize = ((nBufSize + knDOWNLOAD_CHUNK_SIZE - 1)
            / knDOWNLOAD_CHUNK_SIZE) * knDOWNLOAD_CHUNK_SIZE;

        // so it gets included in Azure signature
        osRange.Printf("Range: bytes=0-%d", nRoundedBufSize-1);
        headers = curl_slist_append(headers, osRange.c_str());
        sWriteFuncHeaderData.bDetectRangeDownloadingError = false;
    }
    // HACK for mbtiles driver: http://a.tiles.mapbox.com/v3/ doesn't accept
    // HEAD, as it is a redirect to AWS S3 signed URL, but those are only valid
    // for a given type of HTTP request, and thus GET. This is valid for any
    // signed URL for AWS S3.
    else if( bRetryWithGet ||
             strstr(osURL, ".tiles.mapbox.com/") != nullptr ||
             VSICurlIsS3LikeSignedURL(osURL) ||
             !m_bUseHead )
    {
        sWriteFuncHeaderData.bDownloadHeaderOnly = true;
        osVerb = "GET";
    }
    else
    {
        sWriteFuncHeaderData.bDetectRangeDownloadingError = false;
        curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 1);
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 0);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 1);
        osVerb = "HEAD";
    }

    if( !AllowAutomaticRedirection() )
        curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0);

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(osURL, "http");

    // Bug with older curl versions (<=7.16.4) and FTP.
    // See http://curl.haxx.se/mail/lib-2007-08/0312.html
    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders(osVerb, headers));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_FILETIME, 1);

    MultiPerform(hCurlMultiHandle, hCurlHandle);

    VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

    curl_slist_free_all(headers);

    oFileProp.eExists = EXIST_UNKNOWN;

    long mtime = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_FILETIME, &mtime);

    if( osVerb == "GET" )
        NetworkStatisticsLogger::LogGET(sWriteFuncData.nSize);
    else
        NetworkStatisticsLogger::LogHEAD();

    if( STARTS_WITH(osURL, "ftp") )
    {
        if( sWriteFuncData.pBuffer != nullptr )
        {
            const char* pszContentLength = strstr(
                const_cast<const char*>(sWriteFuncData.pBuffer), "Content-Length: ");
            if( pszContentLength )
            {
                pszContentLength += strlen("Content-Length: ");
                oFileProp.eExists = EXIST_YES;
                oFileProp.fileSize = CPLScanUIntBig(
                    pszContentLength,
                    static_cast<int>(strlen(pszContentLength)));
                if( ENABLE_DEBUG )
                    CPLDebug(poFS->GetDebugKey(),
                             "GetFileSize(%s)=" CPL_FRMT_GUIB,
                            osURL.c_str(), oFileProp.fileSize);
            }
        }
    }

    if( ENABLE_DEBUG && szCurlErrBuf[0] != '\0' &&
        sWriteFuncHeaderData.bDownloadHeaderOnly &&
        EQUAL(szCurlErrBuf, "Failed writing header") )
    {
        // Not really an error since we voluntarily interrupted the download !
        szCurlErrBuf[0] = 0;
    }

    double dfSize = 0;
    if( oFileProp.eExists != EXIST_YES )
    {
        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

        if( ENABLE_DEBUG && szCurlErrBuf[0] != '\0' )
        {
            CPLDebug(poFS->GetDebugKey(),
                     "GetFileSize(%s): response_code=%d, msg=%s",
                     osURL.c_str(),
                     static_cast<int>(response_code),
                     szCurlErrBuf);
        }

        CPLString osEffectiveURL;
        {
            char *pszEffectiveURL = nullptr;
            curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL,
                              &pszEffectiveURL);
            if( pszEffectiveURL )
                osEffectiveURL = pszEffectiveURL;
        }

        if( !osEffectiveURL.empty() && strstr(osEffectiveURL, osURL) == nullptr )
        {
            CPLDebug(poFS->GetDebugKey(),
                     "Effective URL: %s", osEffectiveURL.c_str());

            // Is this is a redirect to a S3 URL?
            if( VSICurlIsS3LikeSignedURL(osEffectiveURL) &&
                !VSICurlIsS3LikeSignedURL(osURL) )
            {
                // Note that this is a redirect as we won't notice after the
                // retry.
                bS3LikeRedirect = true;

                if( !bRetryWithGet && osVerb == "HEAD" && response_code == 403 )
                {
                    CPLDebug(poFS->GetDebugKey(),
                             "Redirected to a AWS S3 signed URL. Retrying "
                             "with GET request instead of HEAD since the URL "
                             "might be valid only for GET");
                    bRetryWithGet = true;
                    osURL = osEffectiveURL;
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                    curl_easy_cleanup(hCurlHandle);
                    goto retry;
                }
            }
        }

        if( bS3LikeRedirect && response_code >= 200 && response_code < 300 &&
            sWriteFuncHeaderData.nTimestampDate > 0 &&
            !osEffectiveURL.empty() &&
            CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_S3_REDIRECT",
                                           "TRUE")) )
        {
            const GIntBig nExpireTimestamp =
                VSICurlGetExpiresFromS3LikeSignedURL(osEffectiveURL);
            if( nExpireTimestamp > sWriteFuncHeaderData.nTimestampDate + 10 )
            {
                const int nValidity =
                    static_cast<int>(nExpireTimestamp -
                                     sWriteFuncHeaderData.nTimestampDate);
                CPLDebug(poFS->GetDebugKey(),
                         "Will use redirect URL for the next %d seconds",
                         nValidity);
                // As our local clock might not be in sync with server clock,
                // figure out the expiration timestamp in local time
                oFileProp.bS3LikeRedirect = true;
                oFileProp.nExpireTimestampLocal = time(nullptr) + nValidity;
                oFileProp.osRedirectURL = osEffectiveURL;
                poFS->SetCachedFileProp(m_pszURL, oFileProp);
            }
        }

        const CURLcode code =
            curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                              &dfSize );
        if( code == 0 )
        {
            oFileProp.eExists = EXIST_YES;
            if( dfSize < 0 )
            {
                if( osVerb == "HEAD" && !bRetryWithGet && response_code == 200 )
                {
                    CPLDebug(poFS->GetDebugKey(),
                             "HEAD did not provide file size. Retrying with GET");
                    bRetryWithGet = true;
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                    curl_easy_cleanup(hCurlHandle);
                    goto retry;
                }
                oFileProp.fileSize = 0;
            }
            else
                oFileProp.fileSize = static_cast<GUIntBig>(dfSize);
        }

        if( sWriteFuncHeaderData.pBuffer != nullptr &&
            (response_code == 200 || response_code == 206 ) )
        {
            const char* pzETag = strstr(
                sWriteFuncHeaderData.pBuffer, "ETag: \"");
            if( pzETag )
            {
                pzETag += strlen("ETag: \"");
                const char* pszEndOfETag = strchr(pzETag, '"');
                if( pszEndOfETag )
                {
                    oFileProp.ETag.assign(pzETag, pszEndOfETag - pzETag);
                }
            }

            // Azure Data Lake Storage
            const char* pszPermissions = strstr(sWriteFuncHeaderData.pBuffer, "x-ms-permissions: ");
            if( pszPermissions )
            {
                pszPermissions += strlen("x-ms-permissions: ");
                const char* pszEOL = strstr(pszPermissions, "\r\n");
                if( pszEOL )
                {
                    bool bIsDir = strstr(sWriteFuncHeaderData.pBuffer, "x-ms-resource-type: directory\r\n") != nullptr;
                    bool bIsFile = strstr(sWriteFuncHeaderData.pBuffer, "x-ms-resource-type: file\r\n") != nullptr;
                    if( bIsDir || bIsFile )
                    {
                        oFileProp.bIsDirectory = bIsDir;
                        CPLString osPermissions;
                        osPermissions.assign(pszPermissions,
                                            pszEOL - pszPermissions);
                        if( bIsDir )
                            oFileProp.nMode = S_IFDIR;
                        else
                            oFileProp.nMode = S_IFREG;
                        oFileProp.nMode |= VSICurlParseUnixPermissions(osPermissions);
                    }
                }
            }

            if( bGetHeaders )
            {
                char** papszHeaders = CSLTokenizeString2(sWriteFuncHeaderData.pBuffer, "\r\n", 0);
                for( int i = 0; papszHeaders[i]; ++i )
                {
                    char* pszKey = nullptr;
                    const char* pszValue = CPLParseNameValue(papszHeaders[i], &pszKey);
                    if( pszKey && pszValue )
                    {
                        m_aosHeaders.SetNameValue(pszKey, pszValue);
                    }
                    CPLFree(pszKey);
                }
                CSLDestroy(papszHeaders);
            }
        }

        if( UseLimitRangeGetInsteadOfHead() && response_code == 206 )
        {
            oFileProp.eExists = EXIST_NO;
            oFileProp.fileSize = 0;
            if( sWriteFuncHeaderData.pBuffer != nullptr )
            {
                const char* pszContentRange =
                    strstr(sWriteFuncHeaderData.pBuffer,
                           "Content-Range: bytes ");
                if( pszContentRange == nullptr )
                    pszContentRange = strstr(sWriteFuncHeaderData.pBuffer,
                           "content-range: bytes ");
                if( pszContentRange )
                    pszContentRange = strchr(pszContentRange, '/');
                if( pszContentRange )
                {
                    oFileProp.eExists = EXIST_YES;
                    oFileProp.fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(pszContentRange + 1));
                }

                // Add first bytes to cache
                if( sWriteFuncData.pBuffer != nullptr )
                {
                    for( size_t nOffset = 0;
                            nOffset + knDOWNLOAD_CHUNK_SIZE <= sWriteFuncData.nSize;
                            nOffset += knDOWNLOAD_CHUNK_SIZE )
                    {
                        poFS->AddRegion(m_pszURL,
                                        nOffset,
                                        knDOWNLOAD_CHUNK_SIZE,
                                        sWriteFuncData.pBuffer + nOffset);
                    }
                }
            }
        }
        else if ( IsDirectoryFromExists(osVerb,
                                        static_cast<int>(response_code)) )
        {
            oFileProp.eExists = EXIST_YES;
            oFileProp.fileSize = 0;
            oFileProp.bIsDirectory = true;
        }
        // 405 = Method not allowed
        else if (response_code == 405 && !bRetryWithGet && osVerb == "HEAD" )
        {
            CPLDebug(poFS->GetDebugKey(), "HEAD not allowed. Retrying with GET");
            bRetryWithGet = true;
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            curl_easy_cleanup(hCurlHandle);
            goto retry;
        }
        else if( response_code == 416 )
        {
            oFileProp.eExists = EXIST_YES;
            oFileProp.fileSize = 0;
        }
        else if( response_code != 200 )
        {
            // Look if we should attempt a retry
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay,
                sWriteFuncHeaderData.pBuffer, szCurlErrBuf);
            if( dfNewRetryDelay > 0 &&
                nRetryCount < m_nMaxRetry )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "HTTP error code: %d - %s. "
                            "Retrying again in %.1f secs",
                            static_cast<int>(response_code), m_pszURL,
                            dfRetryDelay);
                CPLSleep(dfRetryDelay);
                dfRetryDelay = dfNewRetryDelay;
                nRetryCount++;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                curl_easy_cleanup(hCurlHandle);
                goto retry;
            }

            if( UseLimitRangeGetInsteadOfHead() &&
                sWriteFuncData.pBuffer != nullptr &&
                CanRestartOnError(sWriteFuncData.pBuffer,
                                  sWriteFuncHeaderData.pBuffer,
                                  bSetError) )
            {
                oFileProp.bHasComputedFileSize = false;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                curl_easy_cleanup(hCurlHandle);
                return GetFileSizeOrHeaders(bSetError, bGetHeaders);
            }

            // If there was no VSI error thrown in the process,
            // fail by reporting the HTTP response code.
            if( bSetError && VSIGetLastErrorNo() == 0 )
            {
                if( strlen(szCurlErrBuf) > 0 )
                {
                    if( response_code == 0 )
                    {
                        VSIError(VSIE_HttpError,
                                 "CURL error: %s", szCurlErrBuf);
                    }
                    else
                    {
                        VSIError(VSIE_HttpError,
                                 "HTTP response code: %d - %s",
                                 static_cast<int>(response_code), szCurlErrBuf);
                    }
                }
                else
                {
                    VSIError(VSIE_HttpError, "HTTP response code: %d",
                             static_cast<int>(response_code));
                }
            }
            else
            {
                if( response_code != 400 && response_code != 404 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "HTTP response code on %s: %d",
                             osURL.c_str(), static_cast<int>(response_code));
                }
                // else a CPLDebug() is emitted below
            }

            oFileProp.eExists = EXIST_NO;
            oFileProp.fileSize = 0;
        }
        else if( sWriteFuncData.pBuffer != nullptr )
        {
            ProcessGetFileSizeResult( reinterpret_cast<const char*>(sWriteFuncData.pBuffer) );
        }

        // Try to guess if this is a directory. Generally if this is a
        // directory, curl will retry with an URL with slash added.
        if( !osEffectiveURL.empty() &&
            strncmp(osURL, osEffectiveURL, osURL.size()) == 0 &&
            osEffectiveURL[osURL.size()] == '/' )
        {
            oFileProp.eExists = EXIST_YES;
            oFileProp.fileSize = 0;
            oFileProp.bIsDirectory = true;
        }
        else if( osURL.back() == '/' )
        {
            oFileProp.bIsDirectory = true;
        }

        if( ENABLE_DEBUG && szCurlErrBuf[0] == '\0' )
        {
            CPLDebug(poFS->GetDebugKey(),
                     "GetFileSize(%s)=" CPL_FRMT_GUIB
                     "  response_code=%d",
                     osURL.c_str(), oFileProp.fileSize,
                     static_cast<int>(response_code));
        }
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
    curl_easy_cleanup(hCurlHandle);

    oFileProp.bHasComputedFileSize = true;
    if( mtime > 0 )
        oFileProp.mTime = mtime;
    poFS->SetCachedFileProp(m_pszURL, oFileProp);

    return oFileProp.fileSize;
}

/************************************************************************/
/*                                 Exists()                             */
/************************************************************************/

bool VSICurlHandle::Exists( bool bSetError )
{
    if( oFileProp.eExists == EXIST_UNKNOWN )
    {
        GetFileSize(bSetError);
    }
    return oFileProp.eExists == EXIST_YES;
}

/************************************************************************/
/*                                  Tell()                              */
/************************************************************************/

vsi_l_offset VSICurlHandle::Tell()
{
    return curOffset;
}

/************************************************************************/
/*                       GetRedirectURLIfValid()                        */
/************************************************************************/

CPLString VSICurlHandle::GetRedirectURLIfValid(bool& bHasExpired)
{
    bHasExpired = false;
    poFS->GetCachedFileProp(m_pszURL, oFileProp);

    CPLString osURL(m_pszURL + m_osQueryString);
    if( oFileProp.bS3LikeRedirect )
    {
        if( time(nullptr) + 1 < oFileProp.nExpireTimestampLocal )
        {
            CPLDebug(poFS->GetDebugKey(),
                     "Using redirect URL as it looks to be still valid "
                     "(%d seconds left)",
                     static_cast<int>(oFileProp.nExpireTimestampLocal - time(nullptr)));
            osURL = oFileProp.osRedirectURL;
        }
        else
        {
            CPLDebug(poFS->GetDebugKey(),
                     "Redirect URL has expired. Using original URL");
            oFileProp.bS3LikeRedirect = false;
            poFS->SetCachedFileProp(m_pszURL, oFileProp);
            bHasExpired = true;
        }
    }
    return osURL;
}

/************************************************************************/
/*                          DownloadRegion()                            */
/************************************************************************/

std::string VSICurlHandle::DownloadRegion( const vsi_l_offset startOffset,
                                           const int nBlocks )
{
    if( bInterrupted && bStopOnInterruptUntilUninstall )
        return std::string();

    if( oFileProp.eExists == EXIST_NO )
        return std::string();

    CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(m_pszURL);

    bool bHasExpired = false;
    CPLString osURL(GetRedirectURLIfValid(bHasExpired));
    bool bUsedRedirect = osURL != m_pszURL;

    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;
    int nRetryCount = 0;
    double dfRetryDelay = m_dfRetryDelay;

retry:
    CURL* hCurlHandle = curl_easy_init();
    struct curl_slist* headers =
        VSICurlSetOptions(hCurlHandle, osURL, m_papszHTTPOptions);

    if( !AllowAutomaticRedirection() )
        curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0);

    VSICURLInitWriteFuncStruct(&sWriteFuncData,
                               reinterpret_cast<VSILFILE *>(this),
                               pfnReadCbk, pReadCbkUserData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(m_pszURL, "http");
    sWriteFuncHeaderData.nStartOffset = startOffset;
    sWriteFuncHeaderData.nEndOffset =
        startOffset + nBlocks * VSICURLGetDownloadChunkSize() - 1;
    // Some servers don't like we try to read after end-of-file (#5786).
    if( oFileProp.bHasComputedFileSize &&
        sWriteFuncHeaderData.nEndOffset >= oFileProp.fileSize )
    {
        sWriteFuncHeaderData.nEndOffset = oFileProp.fileSize - 1;
    }

    char rangeStr[512] = {};
    snprintf(rangeStr, sizeof(rangeStr),
             CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, startOffset,
            sWriteFuncHeaderData.nEndOffset);

    if( ENABLE_DEBUG )
        CPLDebug(poFS->GetDebugKey(), "Downloading %s (%s)...", rangeStr, osURL.c_str());

    CPLString osHeaderRange; // leave in this scope
    if( sWriteFuncHeaderData.bIsHTTP )
    {
        osHeaderRange.Printf("Range: bytes=%s", rangeStr);
        // So it gets included in Azure signature
        headers = curl_slist_append(headers, osHeaderRange.c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);
    }
    else
        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET", headers));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_FILETIME, 1);

    MultiPerform(hCurlMultiHandle, hCurlHandle);

    VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

    curl_slist_free_all(headers);

    NetworkStatisticsLogger::LogGET(sWriteFuncData.nSize);

    if( sWriteFuncData.bInterrupted )
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);

        return std::string();
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    if( ENABLE_DEBUG && szCurlErrBuf[0] != '\0' )
    {
        CPLDebug(poFS->GetDebugKey(),
                 "DownloadRegion(%s): response_code=%d, msg=%s",
                 osURL.c_str(),
                 static_cast<int>(response_code),
                 szCurlErrBuf);
    }

    long mtime = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_FILETIME, &mtime);
    if( mtime > 0 )
    {
        oFileProp.mTime = mtime;
        poFS->SetCachedFileProp(m_pszURL, oFileProp);
    }

    if( ENABLE_DEBUG )
        CPLDebug(poFS->GetDebugKey(),
                 "Got response_code=%ld", response_code);

    if( response_code == 403 && bUsedRedirect )
    {
        CPLDebug(poFS->GetDebugKey(),
                 "Got an error with redirect URL. Retrying with original one");
        oFileProp.bS3LikeRedirect = false;
        poFS->SetCachedFileProp(m_pszURL, oFileProp);
        bUsedRedirect = false;
        osURL = m_pszURL;
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        goto retry;
    }

    if( response_code == 401 && nRetryCount < m_nMaxRetry )
    {
        CPLDebug(poFS->GetDebugKey(),
                 "Unauthorized, trying to authenticate");
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        nRetryCount++;
        if( Authenticate() )
            goto retry;
        return std::string();
    }

    CPLString osEffectiveURL;
    {
        char *pszEffectiveURL = nullptr;
        curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL, &pszEffectiveURL);
        if( pszEffectiveURL )
            osEffectiveURL = pszEffectiveURL;
    }

    if( !oFileProp.bS3LikeRedirect && !osEffectiveURL.empty() &&
        strstr(osEffectiveURL, m_pszURL) == nullptr )
    {
        CPLDebug(poFS->GetDebugKey(),
                 "Effective URL: %s", osEffectiveURL.c_str());
        if( response_code >= 200 && response_code < 300 &&
            sWriteFuncHeaderData.nTimestampDate > 0 &&
            VSICurlIsS3LikeSignedURL(osEffectiveURL) &&
            !VSICurlIsS3LikeSignedURL(m_pszURL) &&
            CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_S3_REDIRECT",
                                           "TRUE")) )
        {
            GIntBig nExpireTimestamp =
                VSICurlGetExpiresFromS3LikeSignedURL(osEffectiveURL);
            if( nExpireTimestamp > sWriteFuncHeaderData.nTimestampDate + 10 )
            {
                const int nValidity =
                    static_cast<int>(nExpireTimestamp -
                                     sWriteFuncHeaderData.nTimestampDate);
                CPLDebug(poFS->GetDebugKey(),
                         "Will use redirect URL for the next %d seconds",
                         nValidity);
                // As our local clock might not be in sync with server clock,
                // figure out the expiration timestamp in local time.
                oFileProp.bS3LikeRedirect = true;
                oFileProp.nExpireTimestampLocal = time(nullptr) + nValidity;
                oFileProp.osRedirectURL = osEffectiveURL;
                poFS->SetCachedFileProp(m_pszURL, oFileProp);
            }
        }
    }

    if( (response_code != 200 && response_code != 206 &&
         response_code != 225 && response_code != 226 &&
         response_code != 426) ||
        sWriteFuncHeaderData.bError )
    {
        if( sWriteFuncData.pBuffer != nullptr &&
            CanRestartOnError(reinterpret_cast<const char*>(sWriteFuncData.pBuffer),
                              reinterpret_cast<const char*>(sWriteFuncHeaderData.pBuffer), false) )
        {
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            curl_easy_cleanup(hCurlHandle);
            return DownloadRegion(startOffset, nBlocks);
        }

        // Look if we should attempt a retry
        const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
            static_cast<int>(response_code), dfRetryDelay,
            sWriteFuncHeaderData.pBuffer, szCurlErrBuf);
        if( dfNewRetryDelay > 0 &&
            nRetryCount < m_nMaxRetry )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "HTTP error code: %d - %s. "
                        "Retrying again in %.1f secs",
                        static_cast<int>(response_code), m_pszURL,
                        dfRetryDelay);
            CPLSleep(dfRetryDelay);
            dfRetryDelay = dfNewRetryDelay;
            nRetryCount++;
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            curl_easy_cleanup(hCurlHandle);
            goto retry;
        }

        if( response_code >= 400 && szCurlErrBuf[0] != '\0' )
        {
            if( strcmp(szCurlErrBuf, "Couldn't use REST") == 0 )
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "%d: %s, Range downloading not supported by this server!",
                    static_cast<int>(response_code), szCurlErrBuf);
            else
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s",
                         static_cast<int>(response_code), szCurlErrBuf);
        }
        if( !oFileProp.bHasComputedFileSize && startOffset == 0 )
        {
            oFileProp.bHasComputedFileSize = true;
            oFileProp.fileSize = 0;
            oFileProp.eExists = EXIST_NO;
            poFS->SetCachedFileProp(m_pszURL, oFileProp);
        }
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        return std::string();
    }

    if( !oFileProp.bHasComputedFileSize && sWriteFuncHeaderData.pBuffer )
    {
        // Try to retrieve the filesize from the HTTP headers
        // if in the form: "Content-Range: bytes x-y/filesize".
        char* pszContentRange =
            strstr(sWriteFuncHeaderData.pBuffer, "Content-Range: bytes ");
        if( pszContentRange == nullptr )
            pszContentRange = strstr(sWriteFuncHeaderData.pBuffer,
                                     "content-range: bytes ");
        if( pszContentRange )
        {
            char* pszEOL = strchr(pszContentRange, '\n');
            if( pszEOL )
            {
                *pszEOL = 0;
                pszEOL = strchr(pszContentRange, '\r');
                if( pszEOL )
                    *pszEOL = 0;
                char* pszSlash = strchr(pszContentRange, '/');
                if( pszSlash )
                {
                    pszSlash++;
                    oFileProp.fileSize =
                        CPLScanUIntBig(pszSlash,
                                       static_cast<int>(strlen(pszSlash)));
                }
            }
        }
        else if( STARTS_WITH(m_pszURL, "ftp") )
        {
            // Parse 213 answer for FTP protocol.
            char* pszSize = strstr(sWriteFuncHeaderData.pBuffer, "213 ");
            if( pszSize )
            {
                pszSize += 4;
                char* pszEOL = strchr(pszSize, '\n');
                if( pszEOL )
                {
                    *pszEOL = 0;
                    pszEOL = strchr(pszSize, '\r');
                    if( pszEOL )
                        *pszEOL = 0;

                    oFileProp.fileSize =
                        CPLScanUIntBig(pszSize,
                                       static_cast<int>(strlen(pszSize)));
                }
            }
        }

        if( oFileProp.fileSize != 0 )
        {
            oFileProp.eExists = EXIST_YES;

            if( ENABLE_DEBUG )
                CPLDebug(poFS->GetDebugKey(),
                         "GetFileSize(%s)=" CPL_FRMT_GUIB
                         "  response_code=%d",
                         m_pszURL, oFileProp.fileSize, static_cast<int>(response_code));

            oFileProp.bHasComputedFileSize = true;
            poFS->SetCachedFileProp(m_pszURL, oFileProp);
        }
    }

    DownloadRegionPostProcess(startOffset, nBlocks,
                              sWriteFuncData.pBuffer,
                              sWriteFuncData.nSize);

    std::string osRet;
    osRet.assign(sWriteFuncData.pBuffer, sWriteFuncData.nSize);

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
    curl_easy_cleanup(hCurlHandle);

    return osRet;
}

/************************************************************************/
/*                      DownloadRegionPostProcess()                     */
/************************************************************************/

void VSICurlHandle::DownloadRegionPostProcess( const vsi_l_offset startOffset,
                                               const int nBlocks,
                                               const char* pBuffer,
                                               size_t nSize )
{
    const int knDOWNLOAD_CHUNK_SIZE = VSICURLGetDownloadChunkSize();
    lastDownloadedOffset = startOffset + nBlocks * knDOWNLOAD_CHUNK_SIZE;

    if( nSize > static_cast<size_t>(nBlocks) * knDOWNLOAD_CHUNK_SIZE )
    {
        if( ENABLE_DEBUG )
            CPLDebug(
                poFS->GetDebugKey(), "Got more data than expected : %u instead of %u",
                static_cast<unsigned int>(nSize),
                static_cast<unsigned int>(nBlocks * knDOWNLOAD_CHUNK_SIZE));
    }

    vsi_l_offset l_startOffset = startOffset;
    while( nSize > 0 )
    {
#if DEBUG_VERBOSE
        if( ENABLE_DEBUG )
            CPLDebug(
                poFS->GetDebugKey(),
                "Add region %u - %u",
                static_cast<unsigned int>(startOffset),
                static_cast<unsigned int>(
                    std::min(static_cast<size_t>(knDOWNLOAD_CHUNK_SIZE), nSize)));
#endif
        const size_t nChunkSize =
            std::min(static_cast<size_t>(knDOWNLOAD_CHUNK_SIZE), nSize);
        poFS->AddRegion(m_pszURL, l_startOffset, nChunkSize, pBuffer);
        l_startOffset += nChunkSize;
        pBuffer += nChunkSize;
        nSize -= nChunkSize;
    }

}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICurlHandle::Read( void * const pBufferIn, size_t const nSize,
                            size_t const  nMemb )
{
    NetworkStatisticsFileSystem oContextFS(poFS->GetFSPrefix());
    NetworkStatisticsFile oContextFile(m_osFilename);
    NetworkStatisticsAction oContextAction("Read");

    size_t nBufferRequestSize = nSize * nMemb;
    if( nBufferRequestSize == 0 )
        return 0;

    void* pBuffer = pBufferIn;

#if DEBUG_VERBOSE
    CPLDebug(poFS->GetDebugKey(), "offset=%d, size=%d",
             static_cast<int>(curOffset), static_cast<int>(nBufferRequestSize));
#endif

    vsi_l_offset iterOffset = curOffset;
    const int knMAX_REGIONS = GetMaxRegions();
    const int knDOWNLOAD_CHUNK_SIZE = VSICURLGetDownloadChunkSize();
    while( nBufferRequestSize )
    {
        // Don't try to read after end of file.
        poFS->GetCachedFileProp(m_pszURL, oFileProp);
        if( oFileProp.bHasComputedFileSize &&
            iterOffset >= oFileProp.fileSize )
        {
            if( iterOffset == curOffset )
            {
                CPLDebug(poFS->GetDebugKey(), "Request at offset " CPL_FRMT_GUIB
                         ", after end of file", iterOffset);
            }
            break;
        }

        const vsi_l_offset nOffsetToDownload =
                (iterOffset / knDOWNLOAD_CHUNK_SIZE) * knDOWNLOAD_CHUNK_SIZE;
        std::string osRegion;
        std::shared_ptr<std::string> psRegion = poFS->GetRegion(m_pszURL, nOffsetToDownload);
        if( psRegion != nullptr )
        {
            osRegion = *psRegion;
        }
        else
        {
            if( nOffsetToDownload == lastDownloadedOffset )
            {
                // In case of consecutive reads (of small size), we use a
                // heuristic that we will read the file sequentially, so
                // we double the requested size to decrease the number of
                // client/server roundtrips.
                if( nBlocksToDownload < 100 )
                    nBlocksToDownload *= 2;
            }
            else
            {
                // Random reads. Cancel the above heuristics.
                nBlocksToDownload = 1;
            }

            // Ensure that we will request at least the number of blocks
            // to satisfy the remaining buffer size to read.
            const vsi_l_offset nEndOffsetToDownload =
                ((iterOffset + nBufferRequestSize + knDOWNLOAD_CHUNK_SIZE - 1) / knDOWNLOAD_CHUNK_SIZE) *
                knDOWNLOAD_CHUNK_SIZE;
            const int nMinBlocksToDownload =
                static_cast<int>(
                    (nEndOffsetToDownload - nOffsetToDownload) /
                    knDOWNLOAD_CHUNK_SIZE);
            if( nBlocksToDownload < nMinBlocksToDownload )
                nBlocksToDownload = nMinBlocksToDownload;

            // Avoid reading already cached data.
            // Note: this might get evicted if concurrent reads are done, but
            // this should not cause bugs. Just missed optimization.
            for( int i = 1; i < nBlocksToDownload; i++ )
            {
                if( poFS->GetRegion(
                        m_pszURL,
                        nOffsetToDownload + i * knDOWNLOAD_CHUNK_SIZE) != nullptr )
                {
                    nBlocksToDownload = i;
                    break;
                }
            }

            if( nBlocksToDownload > knMAX_REGIONS )
                nBlocksToDownload = knMAX_REGIONS;

            osRegion = DownloadRegion(nOffsetToDownload, nBlocksToDownload);
            if( osRegion.empty() )
            {
                if( !bInterrupted )
                    bEOF = true;
                return 0;
            }
        }

        const vsi_l_offset nRegionOffset = iterOffset - nOffsetToDownload;
        if (osRegion.size() < nRegionOffset)
        {
            if( iterOffset == curOffset )
            {
                CPLDebug(poFS->GetDebugKey(), "Request at offset " CPL_FRMT_GUIB
                         ", after end of file", iterOffset);
            }
            break;
        }

        const int nToCopy = static_cast<int>(
            std::min(static_cast<vsi_l_offset>(nBufferRequestSize),
                     osRegion.size() - nRegionOffset));
        memcpy(pBuffer,
               osRegion.data() + nRegionOffset,
               nToCopy);
        pBuffer = static_cast<char *>(pBuffer) + nToCopy;
        iterOffset += nToCopy;
        nBufferRequestSize -= nToCopy;
        if( osRegion.size() < static_cast<size_t>(knDOWNLOAD_CHUNK_SIZE) &&
            nBufferRequestSize != 0 )
        {
            break;
        }
    }

    const size_t ret = static_cast<size_t>((iterOffset - curOffset) / nSize);
    if( ret != nMemb )
        bEOF = true;

    curOffset = iterOffset;

    return ret;
}

/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSICurlHandle::ReadMultiRange( int const nRanges, void ** const ppData,
                                   const vsi_l_offset* const panOffsets,
                                   const size_t* const panSizes )
{
    if( bInterrupted && bStopOnInterruptUntilUninstall )
        return FALSE;

    poFS->GetCachedFileProp(m_pszURL, oFileProp);
    if( oFileProp.eExists == EXIST_NO )
        return -1;

    NetworkStatisticsFileSystem oContextFS(poFS->GetFSPrefix());
    NetworkStatisticsFile oContextFile(m_osFilename);
    NetworkStatisticsAction oContextAction("ReadMultiRange");

    const char* pszMultiRangeStrategy =
        CPLGetConfigOption("GDAL_HTTP_MULTIRANGE", "");
    if( EQUAL(pszMultiRangeStrategy, "SINGLE_GET") )
    {
        // Just in case someone needs it, but the interest of this mode is rather
        // dubious now. We could probably remove it
        return ReadMultiRangeSingleGet(nRanges, ppData, panOffsets, panSizes);
    }
    else if( nRanges == 1 || EQUAL(pszMultiRangeStrategy, "SERIAL") )
    {
        return VSIVirtualHandle::ReadMultiRange(
                                    nRanges, ppData, panOffsets, panSizes);
    }

    bool bHasExpired = false;
    CPLString osURL(GetRedirectURLIfValid(bHasExpired));
    if( bHasExpired )
    {
        return VSIVirtualHandle::ReadMultiRange(
                                    nRanges, ppData, panOffsets, panSizes);
    }

    CURLM * hMultiHandle = poFS->GetCurlMultiHandleFor(osURL);
#ifdef CURLPIPE_MULTIPLEX
    // Enable HTTP/2 multiplexing (ignored if an older version of HTTP is
    // used)
    // Not that this does not enable HTTP/1.1 pipeling, which is not
    // recommended for example by Google Cloud Storage.
    // For HTTP/1.1, parallel connections work better since you can get
    // results out of order.
    if( CPLTestBool(CPLGetConfigOption("GDAL_HTTP_MULTIPLEX", "YES")) )
    {
        curl_multi_setopt(hMultiHandle, CURLMOPT_PIPELINING,
                          CURLPIPE_MULTIPLEX);
    }
#endif

    std::vector<CURL*> aHandles;
    std::vector<WriteFuncStruct> asWriteFuncData(nRanges);
    std::vector<WriteFuncStruct> asWriteFuncHeaderData(nRanges);
    std::vector<char*> apszRanges;
    std::vector<struct curl_slist*> aHeaders;

    struct CurlErrBuffer
    {
        std::array<char,CURL_ERROR_SIZE+1> szCurlErrBuf;
    };
    std::vector<CurlErrBuffer> asCurlErrors(nRanges);

    const bool bMergeConsecutiveRanges = CPLTestBool(CPLGetConfigOption(
        "GDAL_HTTP_MERGE_CONSECUTIVE_RANGES", "TRUE"));

    for( int i = 0, iRequest = 0; i < nRanges; )
    {
        size_t nSize = 0;
        int iNext = i;
        // Identify consecutive ranges
        while( bMergeConsecutiveRanges &&
               iNext + 1 < nRanges &&
               panOffsets[iNext] + panSizes[iNext] == panOffsets[iNext+1] )
        {
            nSize += panSizes[iNext];
            iNext++;
        }
        nSize += panSizes[iNext];
        if( nSize == 0 )
            continue;

        CURL* hCurlHandle = curl_easy_init();
        aHandles.push_back(hCurlHandle);

        // As the multi-range request is likely not the first one, we don't
        // need to wait as we already know if pipelining is possible
        // curl_easy_setopt(hCurlHandle, CURLOPT_PIPEWAIT, 1);

        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osURL, m_papszHTTPOptions);

        VSICURLInitWriteFuncStruct(&asWriteFuncData[iRequest],
                                reinterpret_cast<VSILFILE *>(this),
                                pfnReadCbk, pReadCbkUserData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA,
                         &asWriteFuncData[iRequest]);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                        VSICurlHandleWriteFunc);

        VSICURLInitWriteFuncStruct(&asWriteFuncHeaderData[iRequest],
                                   nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA,
                         &asWriteFuncHeaderData[iRequest]);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);
        asWriteFuncHeaderData[iRequest].bIsHTTP = STARTS_WITH(m_pszURL, "http");
        asWriteFuncHeaderData[iRequest].nStartOffset = panOffsets[i];

        asWriteFuncHeaderData[iRequest].nEndOffset = panOffsets[i] + nSize-1;

        char rangeStr[512] = {};
        snprintf(rangeStr, sizeof(rangeStr),
                CPL_FRMT_GUIB "-" CPL_FRMT_GUIB,
                asWriteFuncHeaderData[iRequest].nStartOffset,
                asWriteFuncHeaderData[iRequest].nEndOffset);

        if( ENABLE_DEBUG )
            CPLDebug(poFS->GetDebugKey(),
                     "Downloading %s (%s)...", rangeStr, osURL.c_str());

        if( asWriteFuncHeaderData[iRequest].bIsHTTP )
        {
            CPLString osHeaderRange;
            osHeaderRange.Printf("Range: bytes=%s", rangeStr);
            // So it gets included in Azure signature
            char* pszRange = CPLStrdup(osHeaderRange);
            apszRanges.push_back(pszRange);
            headers = curl_slist_append(headers, pszRange);
            curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);
        }
        else
        {
            apszRanges.push_back(nullptr);
            curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);
        }

        asCurlErrors[iRequest].szCurlErrBuf[0] = '\0';
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER,
                         &asCurlErrors[iRequest].szCurlErrBuf[0] );

        headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);
        aHeaders.push_back(headers);
        curl_multi_add_handle(hMultiHandle, hCurlHandle);

        i = iNext + 1;
        iRequest ++;
    }

    if( !aHandles.empty() )
    {
        MultiPerform(hMultiHandle);
    }

    int nRet = 0;
    size_t iReq = 0;
    int iRange = 0;
    size_t nTotalDownloaded = 0;
    for( ; iReq < aHandles.size(); iReq++, iRange++ )
    {
        while( iRange < nRanges && panSizes[iRange] == 0 )
        {
            iRange ++;
        }
        if( iRange == nRanges )
            break;

        long response_code = 0;
        curl_easy_getinfo(aHandles[iReq], CURLINFO_HTTP_CODE, &response_code);

        if( ENABLE_DEBUG && asCurlErrors[iRange].szCurlErrBuf[0] != '\0' )
        {
            char rangeStr[512] = {};
            snprintf(rangeStr, sizeof(rangeStr),
                    CPL_FRMT_GUIB "-" CPL_FRMT_GUIB,
                    asWriteFuncHeaderData[iReq].nStartOffset,
                    asWriteFuncHeaderData[iReq].nEndOffset);

            const char* pszErrorMsg = &asCurlErrors[iRange].szCurlErrBuf[0];
            CPLDebug(poFS->GetDebugKey(),
                     "ReadMultiRange(%s), %s: response_code=%d, msg=%s",
                     osURL.c_str(),
                     rangeStr,
                     static_cast<int>(response_code),
                     pszErrorMsg);
        }

        if( (response_code != 206 && response_code != 225) ||
            asWriteFuncHeaderData[iReq].nEndOffset+1 !=
                asWriteFuncHeaderData[iReq].nStartOffset +
                    asWriteFuncData[iReq].nSize )
        {
            char rangeStr[512] = {};
            snprintf(rangeStr, sizeof(rangeStr),
                    CPL_FRMT_GUIB "-" CPL_FRMT_GUIB,
                    asWriteFuncHeaderData[iReq].nStartOffset,
                    asWriteFuncHeaderData[iReq].nEndOffset);

            CPLError(CE_Failure, CPLE_AppDefined,
                     "Request for %s failed", rangeStr);
            nRet = -1;
        }
        else if( nRet == 0 )
        {
            size_t nOffset = 0;
            size_t nRemainingSize = asWriteFuncData[iReq].nSize;
            nTotalDownloaded += nRemainingSize;
            CPLAssert( iRange < nRanges );
            while( true )
            {
                if( nRemainingSize < panSizes[iRange] )
                {
                    nRet = -1;
                    break;
                }

                if( panSizes[iRange] > 0 )
                {
                    memcpy( ppData[iRange],
                            asWriteFuncData[iReq].pBuffer + nOffset,
                            panSizes[iRange] );
                }

                if( bMergeConsecutiveRanges &&
                    iRange + 1 < nRanges &&
                    panOffsets[iRange] + panSizes[iRange] ==
                                                    panOffsets[iRange + 1] )
                {
                    nOffset += panSizes[iRange];
                    nRemainingSize -= panSizes[iRange];
                    iRange++;
                }
                else
                {
                    break;
                }
            }
        }

        curl_multi_remove_handle(hMultiHandle, aHandles[iReq]);
        VSICURLResetHeaderAndWriterFunctions(aHandles[iReq]);
        curl_easy_cleanup(aHandles[iReq]);
        CPLFree(apszRanges[iReq]);
        CPLFree(asWriteFuncData[iReq].pBuffer);
        CPLFree(asWriteFuncHeaderData[iReq].pBuffer);
        curl_slist_free_all(aHeaders[iReq]);
    }

    NetworkStatisticsLogger::LogGET(nTotalDownloaded);

    if( ENABLE_DEBUG )
        CPLDebug(poFS->GetDebugKey(), "Download completed");

    return nRet;
}

/************************************************************************/
/*                       ReadMultiRangeSingleGet()                      */
/************************************************************************/

// TODO: the interest of this mode is rather dubious now. We could probably
// remove it
int VSICurlHandle::ReadMultiRangeSingleGet(
                                   int const nRanges, void ** const ppData,
                                   const vsi_l_offset* const panOffsets,
                                   const size_t* const panSizes )
{
    CPLString osRanges;
    CPLString osFirstRange;
    CPLString osLastRange;
    int nMergedRanges = 0;
    vsi_l_offset nTotalReqSize = 0;
    for( int i=0; i < nRanges; i++ )
    {
        CPLString osCurRange;
        if( i != 0 )
            osRanges.append(",");
        osCurRange = CPLSPrintf(CPL_FRMT_GUIB "-", panOffsets[i]);
        while( i + 1 < nRanges &&
               panOffsets[i] + panSizes[i] == panOffsets[i+1] )
        {
            nTotalReqSize += panSizes[i];
            i++;
        }
        nTotalReqSize += panSizes[i];
        osCurRange.append
            (CPLSPrintf(CPL_FRMT_GUIB, panOffsets[i] + panSizes[i]-1));
        nMergedRanges++;

        osRanges += osCurRange;

        if( nMergedRanges == 1 )
            osFirstRange = osCurRange;
        osLastRange = osCurRange;
    }

    const char* pszMaxRanges =
        CPLGetConfigOption("CPL_VSIL_CURL_MAX_RANGES", "250");
    int nMaxRanges = atoi(pszMaxRanges);
    if( nMaxRanges <= 0 )
        nMaxRanges = 250;
    if( nMergedRanges > nMaxRanges )
    {
        const int nHalf = nRanges / 2;
        const int nRet = ReadMultiRange(nHalf, ppData, panOffsets, panSizes);
        if( nRet != 0 )
            return nRet;
        return ReadMultiRange(nRanges - nHalf, ppData + nHalf,
                              panOffsets + nHalf, panSizes + nHalf);
    }

    CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(m_pszURL);
    CURL* hCurlHandle = curl_easy_init();

    struct curl_slist* headers =
        VSICurlSetOptions(hCurlHandle, m_pszURL, m_papszHTTPOptions);

    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    VSICURLInitWriteFuncStruct(&sWriteFuncData,
                               reinterpret_cast<VSILFILE *>(this),
                               pfnReadCbk, pReadCbkUserData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(m_pszURL, "http");
    sWriteFuncHeaderData.bMultiRange = nMergedRanges > 1;
    if( nMergedRanges == 1 )
    {
        sWriteFuncHeaderData.nStartOffset = panOffsets[0];
        sWriteFuncHeaderData.nEndOffset = panOffsets[0] + nTotalReqSize-1;
    }

    if( ENABLE_DEBUG )
    {
        if( nMergedRanges == 1 )
            CPLDebug(poFS->GetDebugKey(), "Downloading %s (%s)...",
                     osRanges.c_str(), m_pszURL);
        else
            CPLDebug(poFS->GetDebugKey(), "Downloading %s, ..., %s (" CPL_FRMT_GUIB
                     " bytes, %s)...",
                     osFirstRange.c_str(), osLastRange.c_str(),
                     static_cast<GUIntBig>(nTotalReqSize), m_pszURL);
    }

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, osRanges.c_str());

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET", headers));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    MultiPerform(hCurlMultiHandle, hCurlHandle);

    VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

    curl_slist_free_all(headers);

    NetworkStatisticsLogger::LogGET(sWriteFuncData.nSize);

    if( sWriteFuncData.bInterrupted )
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);

        return -1;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    if( (response_code != 200 && response_code != 206 &&
         response_code != 225 && response_code != 226 &&
         response_code != 426)
        || sWriteFuncHeaderData.bError )
    {
        if( response_code >= 400 && szCurlErrBuf[0] != '\0' )
        {
            if( strcmp(szCurlErrBuf, "Couldn't use REST") == 0 )
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "%d: %s, Range downloading not supported by this server!",
                    static_cast<int>(response_code), szCurlErrBuf);
            else
                CPLError(CE_Failure, CPLE_AppDefined, "%d: %s",
                         static_cast<int>(response_code), szCurlErrBuf);
        }
        /*
        if( !bHasComputedFileSize && startOffset == 0 )
        {
            cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        */
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        return -1;
    }

    char* pBuffer = sWriteFuncData.pBuffer;
    size_t nSize = sWriteFuncData.nSize;

    // TODO(schwehr): Localize after removing gotos.
    int nRet = -1;
    char* pszBoundary;
    CPLString osBoundary;
    char *pszNext = nullptr;
    int iRange = 0;
    int iPart = 0;
    char* pszEOL = nullptr;

/* -------------------------------------------------------------------- */
/*      No multipart if a single range has been requested               */
/* -------------------------------------------------------------------- */

    if( nMergedRanges == 1 )
    {
        size_t nAccSize = 0;
        if( static_cast<vsi_l_offset>(nSize) < nTotalReqSize )
            goto end;

        for( int i=0; i < nRanges; i++ )
        {
            memcpy(ppData[i], pBuffer + nAccSize, panSizes[i]);
            nAccSize += panSizes[i];
        }

        nRet = 0;
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Extract boundary name                                           */
/* -------------------------------------------------------------------- */

    pszBoundary = strstr(sWriteFuncHeaderData.pBuffer,
                         "Content-Type: multipart/byteranges; boundary=");
    if( pszBoundary == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Could not find '%s'",
                  "Content-Type: multipart/byteranges; boundary=" );
        goto end;
    }

    pszBoundary += strlen( "Content-Type: multipart/byteranges; boundary=" );

    pszEOL = strchr(pszBoundary, '\r');
    if( pszEOL )
        *pszEOL = 0;
    pszEOL = strchr(pszBoundary, '\n');
    if( pszEOL )
        *pszEOL = 0;

    /* Remove optional double-quote character around boundary name */
    if( pszBoundary[0] == '"' )
    {
        pszBoundary++;
        char* pszLastDoubleQuote = strrchr(pszBoundary, '"');
        if( pszLastDoubleQuote )
            *pszLastDoubleQuote = 0;
    }

    osBoundary = "--";
    osBoundary += pszBoundary;

/* -------------------------------------------------------------------- */
/*      Find the start of the first chunk.                              */
/* -------------------------------------------------------------------- */
    pszNext = strstr(pBuffer, osBoundary.c_str());
    if( pszNext == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No parts found." );
        goto end;
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
    while( iPart < nRanges )
    {
/* -------------------------------------------------------------------- */
/*      Collect headers.                                                */
/* -------------------------------------------------------------------- */
        bool bExpectedRange = false;

        while( *pszNext != '\n' && *pszNext != '\r' && *pszNext != '\0' )
        {
            pszEOL = strstr(pszNext, "\n");

            if( pszEOL == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while parsing multipart content (at line %d)",
                         __LINE__);
                goto end;
            }

            *pszEOL = '\0';
            bool bRestoreAntislashR = false;
            if( pszEOL - pszNext > 1 && pszEOL[-1] == '\r' )
            {
                bRestoreAntislashR = true;
                pszEOL[-1] = '\0';
            }

            if( STARTS_WITH_CI(pszNext, "Content-Range: bytes ") )
            {
                bExpectedRange = true; /* FIXME */
            }

            if( bRestoreAntislashR )
                pszEOL[-1] = '\r';
            *pszEOL = '\n';

            pszNext = pszEOL + 1;
        }

        if( !bExpectedRange )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error while parsing multipart content (at line %d)",
                     __LINE__);
            goto end;
        }

        if( *pszNext == '\r' )
            pszNext++;
        if( *pszNext == '\n' )
            pszNext++;

/* -------------------------------------------------------------------- */
/*      Work out the data block size.                                   */
/* -------------------------------------------------------------------- */
        size_t nBytesAvail = nSize - (pszNext - pBuffer);

        while( true )
        {
            if( nBytesAvail < panSizes[iRange] )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while parsing multipart content (at line %d)",
                         __LINE__);
                goto end;
            }

            memcpy(ppData[iRange], pszNext, panSizes[iRange]);
            pszNext += panSizes[iRange];
            nBytesAvail -= panSizes[iRange];
            if( iRange + 1 < nRanges &&
                panOffsets[iRange] + panSizes[iRange] ==
                panOffsets[iRange + 1] )
            {
                iRange++;
            }
            else
            {
                break;
            }
        }

        iPart++;
        iRange++;

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
            goto end;
        }

        pszNext += osBoundary.size();
        if( STARTS_WITH(pszNext, "--") )
        {
            // End of multipart.
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
            goto end;
        }
    }

    if( iPart == nMergedRanges )
        nRet = 0;
    else
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Got only %d parts, where %d were expected",
                 iPart, nMergedRanges);

end:
    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
    curl_easy_cleanup(hCurlHandle);

    return nRet;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSICurlHandle::Write( const void * /* pBuffer */,
                             size_t /* nSize */,
                             size_t /* nMemb */ )
{
    return 0;
}

/************************************************************************/
/*                                 Eof()                                */
/************************************************************************/

int       VSICurlHandle::Eof()
{
    return bEOF;
}

/************************************************************************/
/*                                 Flush()                              */
/************************************************************************/

int       VSICurlHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                                  Close()                             */
/************************************************************************/

int       VSICurlHandle::Close()
{
    return 0;
}

/************************************************************************/
/*                   VSICurlFilesystemHandler()                         */
/************************************************************************/

VSICurlFilesystemHandler::VSICurlFilesystemHandler():
    oCacheFileProp{100 * 1024},
    oCacheDirList{1024, 0}
{
}

/************************************************************************/
/*                           CachedConnection                           */
/************************************************************************/

namespace {
struct CachedConnection
{
    CURLM          *hCurlMultiHandle = nullptr;
    void            clear();

    ~CachedConnection() { clear(); }
};
} // namespace

#ifdef WIN32
// Currently thread_local and C++ objects don't work well with DLL on Windows
static void FreeCachedConnection( void* pData )
{
    delete static_cast<std::map<VSICurlFilesystemHandler*, CachedConnection>*>(pData);
}

// Per-thread and per-filesystem Curl connection cache.
static std::map<VSICurlFilesystemHandler*, CachedConnection>& GetConnectionCache()
{
    static std::map<VSICurlFilesystemHandler*, CachedConnection> dummyCache;
    int bMemoryErrorOccurred = false;
    void* pData = CPLGetTLSEx(CTLS_VSICURL_CACHEDCONNECTION, &bMemoryErrorOccurred);
    if( bMemoryErrorOccurred )
    {
        return dummyCache;
    }
    if( pData == nullptr)
    {
        auto cachedConnection = new std::map<VSICurlFilesystemHandler*, CachedConnection>();
        CPLSetTLSWithFreeFuncEx( CTLS_VSICURL_CACHEDCONNECTION,
                                 cachedConnection,
                                 FreeCachedConnection, &bMemoryErrorOccurred );
        if( bMemoryErrorOccurred )
        {
            delete cachedConnection;
            return dummyCache;
        }
        return *cachedConnection;
    }
    return *static_cast<std::map<VSICurlFilesystemHandler*, CachedConnection>*>(pData);
}
#else
static thread_local std::map<VSICurlFilesystemHandler*, CachedConnection> g_tls_connectionCache;
static std::map<VSICurlFilesystemHandler*, CachedConnection>& GetConnectionCache()
{
    return g_tls_connectionCache;
}
#endif

/************************************************************************/
/*                              clear()                                 */
/************************************************************************/

void CachedConnection::clear()
{
    if( hCurlMultiHandle )
    {
        curl_multi_cleanup(hCurlMultiHandle);
        hCurlMultiHandle = nullptr;
    }
}

/************************************************************************/
/*                  ~VSICurlFilesystemHandler()                         */
/************************************************************************/

extern "C" int CPL_DLL GDALIsInGlobalDestructor();

VSICurlFilesystemHandler::~VSICurlFilesystemHandler()
{
    VSICurlFilesystemHandler::ClearCache();
    if( !GDALIsInGlobalDestructor() )
    {
        GetConnectionCache().erase(this);
    }

    if( hMutex != nullptr )
        CPLDestroyMutex( hMutex );
    hMutex = nullptr;
}

/************************************************************************/
/*                      AllowCachedDataFor()                            */
/************************************************************************/

bool VSICurlFilesystemHandler::AllowCachedDataFor(const char* pszFilename)
{
    bool bCachedAllowed = true;
    char** papszTokens = CSLTokenizeString2(
        CPLGetConfigOption("CPL_VSIL_CURL_NON_CACHED", ""), ":", 0 );
    for( int i = 0; papszTokens && papszTokens[i]; i++)
    {
        if( STARTS_WITH(pszFilename, papszTokens[i]) )
        {
            bCachedAllowed = false;
            break;
        }
    }
    CSLDestroy(papszTokens);
    return bCachedAllowed;
}

/************************************************************************/
/*                     GetCurlMultiHandleFor()                          */
/************************************************************************/

CURLM* VSICurlFilesystemHandler::GetCurlMultiHandleFor(const CPLString& /*osURL*/)
{
    auto& conn = GetConnectionCache()[this];
    if( conn.hCurlMultiHandle == nullptr )
    {
        conn.hCurlMultiHandle = curl_multi_init();
    }
    return conn.hCurlMultiHandle;
}

/************************************************************************/
/*                          GetRegionCache()                            */
/************************************************************************/

VSICurlFilesystemHandler::RegionCacheType* VSICurlFilesystemHandler::GetRegionCache()
{
    // should be called under hMutex taken
    if( m_poRegionCacheDoNotUseDirectly == nullptr )
    {
        m_poRegionCacheDoNotUseDirectly.reset(new RegionCacheType(static_cast<size_t>(GetMaxRegions())));
    }
    return m_poRegionCacheDoNotUseDirectly.get();
}

/************************************************************************/
/*                          GetRegion()                                 */
/************************************************************************/

std::shared_ptr<std::string>
VSICurlFilesystemHandler::GetRegion( const char* pszURL,
                                     vsi_l_offset nFileOffsetStart )
{
    CPLMutexHolder oHolder( &hMutex );

    const int knDOWNLOAD_CHUNK_SIZE = VSICURLGetDownloadChunkSize();
    nFileOffsetStart =
        (nFileOffsetStart / knDOWNLOAD_CHUNK_SIZE) * knDOWNLOAD_CHUNK_SIZE;

    std::shared_ptr<std::string> out;
    if( GetRegionCache()->tryGet(
        FilenameOffsetPair(std::string(pszURL), nFileOffsetStart), out) )
    {
        return out;
    }

    return nullptr;
}

/************************************************************************/
/*                          AddRegion()                                 */
/************************************************************************/

void VSICurlFilesystemHandler::AddRegion( const char* pszURL,
                                          vsi_l_offset nFileOffsetStart,
                                          size_t nSize,
                                          const char *pData )
{
    CPLMutexHolder oHolder( &hMutex );

    std::shared_ptr<std::string> value(new std::string());
    value->assign(pData, nSize);
    GetRegionCache()->insert(
        FilenameOffsetPair(std::string(pszURL), nFileOffsetStart),
        value);
}

/************************************************************************/
/*                         GetCachedFileProp()                          */
/************************************************************************/

bool
VSICurlFilesystemHandler::GetCachedFileProp( const char* pszURL,
                                             FileProp& oFileProp )
{
    CPLMutexHolder oHolder( &hMutex );

    return oCacheFileProp.tryGet(std::string(pszURL), oFileProp) &&
            // Let a chance to use new auth parameters
           !(oFileProp.eExists == EXIST_NO &&
             gnGenerationAuthParameters != oFileProp.nGenerationAuthParameters);
}

/************************************************************************/
/*                         SetCachedFileProp()                          */
/************************************************************************/

void
VSICurlFilesystemHandler::SetCachedFileProp( const char* pszURL,
                                             FileProp& oFileProp )
{
    CPLMutexHolder oHolder( &hMutex );

    oFileProp.nGenerationAuthParameters = gnGenerationAuthParameters;
    oCacheFileProp.insert(std::string(pszURL), oFileProp);
}

/************************************************************************/
/*                         GetCachedDirList()                           */
/************************************************************************/

bool
VSICurlFilesystemHandler::GetCachedDirList( const char* pszURL,
                                            CachedDirList& oCachedDirList )
{
    CPLMutexHolder oHolder( &hMutex );

    return oCacheDirList.tryGet(std::string(pszURL), oCachedDirList) &&
            // Let a chance to use new auth parameters
           gnGenerationAuthParameters == oCachedDirList.nGenerationAuthParameters;
}

/************************************************************************/
/*                         SetCachedDirList()                           */
/************************************************************************/

void
VSICurlFilesystemHandler::SetCachedDirList( const char* pszURL,
                                            CachedDirList& oCachedDirList )
{
    CPLMutexHolder oHolder( &hMutex );

    std::string key(pszURL);
    CachedDirList oldValue;
    if( oCacheDirList.tryGet(key, oldValue) )
    {
        nCachedFilesInDirList -= oldValue.oFileList.size();
        oCacheDirList.remove(key);
    }

    while( (!oCacheDirList.empty() &&
            nCachedFilesInDirList + oCachedDirList.oFileList.size() > 1024 * 1024) ||
            oCacheDirList.size() == oCacheDirList.getMaxAllowedSize() )
    {
        std::string oldestKey;
        oCacheDirList.getOldestEntry(oldestKey, oldValue);
        nCachedFilesInDirList -= oldValue.oFileList.size();
        oCacheDirList.remove(oldestKey);
    }
    oCachedDirList.nGenerationAuthParameters = gnGenerationAuthParameters;

    nCachedFilesInDirList += oCachedDirList.oFileList.size();
    oCacheDirList.insert(key, oCachedDirList);
}

/************************************************************************/
/*                        ExistsInCacheDirList()                        */
/************************************************************************/

bool VSICurlFilesystemHandler::ExistsInCacheDirList(
                            const CPLString& osDirname, bool *pbIsDir )
{
    CachedDirList cachedDirList;
    if( GetCachedDirList(osDirname, cachedDirList) )
    {
        if( pbIsDir )
            *pbIsDir = !cachedDirList.oFileList.empty();
        return false;
    }
    else
    {
        if( pbIsDir )
            *pbIsDir = false;
        return false;
    }
}

/************************************************************************/
/*                        InvalidateCachedData()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateCachedData( const char* pszURL )
{
    CPLMutexHolder oHolder( &hMutex );

    oCacheFileProp.remove(std::string(pszURL));

    // Invalidate all cached regions for this URL
    std::list<FilenameOffsetPair> keysToRemove;
    std::string osURL(pszURL);
    auto lambda = [&keysToRemove, &osURL](
        const lru11::KeyValuePair<FilenameOffsetPair,
                                  std::shared_ptr<std::string>>& kv)
    {
        if( kv.key.filename_ == osURL )
            keysToRemove.push_back(kv.key);
    };
    auto* poRegionCache = GetRegionCache();
    poRegionCache->cwalk(lambda);
    for( auto& key: keysToRemove )
        poRegionCache->remove(key);
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSICurlFilesystemHandler::ClearCache()
{
    CPLMutexHolder oHolder( &hMutex );

    GetRegionCache()->clear();

    oCacheFileProp.clear();

    oCacheDirList.clear();
    nCachedFilesInDirList = 0;

    if( !GDALIsInGlobalDestructor() )
    {
        GetConnectionCache()[this].clear();
    }
}

/************************************************************************/
/*                          PartialClearCache()                         */
/************************************************************************/

void VSICurlFilesystemHandler::PartialClearCache(const char* pszFilenamePrefix)
{
    CPLMutexHolder oHolder( &hMutex );

    CPLString osURL = GetURLFromFilename(pszFilenamePrefix);
    {
        std::list<FilenameOffsetPair> keysToRemove;
        auto lambda = [&keysToRemove, &osURL](
            const lru11::KeyValuePair<FilenameOffsetPair,
                                                std::shared_ptr<std::string>>& kv)
        {
            if( strncmp(kv.key.filename_.c_str(), osURL, osURL.size()) == 0 )
                keysToRemove.push_back(kv.key);
        };
        auto* poRegionCache = GetRegionCache();
        poRegionCache->cwalk(lambda);
        for( auto& key: keysToRemove )
            poRegionCache->remove(key);
    }

    {
        std::list<std::string> keysToRemove;
        auto lambda = [&keysToRemove, &osURL](
            const lru11::KeyValuePair<std::string, FileProp>& kv)
        {
            if( strncmp(kv.key.c_str(), osURL, osURL.size()) == 0 )
                keysToRemove.push_back(kv.key);
        };
        oCacheFileProp.cwalk(lambda);
        for( auto& key: keysToRemove )
            oCacheFileProp.remove(key);
    }

    {
        const size_t nLen = strlen(pszFilenamePrefix);
        std::list<std::string> keysToRemove;
        auto lambda = [this, &keysToRemove, pszFilenamePrefix, nLen](
            const lru11::KeyValuePair<std::string, CachedDirList>& kv)
        {
            if( strncmp(kv.key.c_str(), pszFilenamePrefix, nLen) == 0 )
            {
                keysToRemove.push_back(kv.key);
                nCachedFilesInDirList -= kv.value.oFileList.size();
            }
        };
        oCacheDirList.cwalk(lambda);
        for( auto& key: keysToRemove )
            oCacheDirList.remove(key);
    }
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSICurlFilesystemHandler::CreateFileHandle(
                                                const char* pszFilename )
{
    return new VSICurlHandle(this, pszFilename);
}

/************************************************************************/
/*                          GetActualURL()                              */
/************************************************************************/

const char* VSICurlFilesystemHandler::GetActualURL(const char* pszFilename)
{
    VSICurlHandle* poHandle = CreateFileHandle(pszFilename);
    if( poHandle == nullptr )
        return pszFilename;
    CPLString osURL(poHandle->GetURL());
    delete poHandle;
    return CPLSPrintf("%s", osURL.c_str());
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

#define VSICURL_OPTIONS \
    "  <Option name='GDAL_HTTP_MAX_RETRY' type='int' " \
        "description='Maximum number of retries' default='0'/>" \
    "  <Option name='GDAL_HTTP_RETRY_DELAY' type='double' " \
        "description='Retry delay in seconds' default='30'/>" \
    "  <Option name='GDAL_HTTP_HEADER_FILE' type='string' " \
        "description='Filename of a file that contains HTTP headers to " \
        "forward to the server'/>" \
    "  <Option name='CPL_VSIL_CURL_USE_HEAD' type='boolean' " \
        "description='Whether to use HTTP HEAD verb to retrieve " \
        "file information' default='YES'/>" \
    "  <Option name='GDAL_HTTP_MULTIRANGE' type='string-select' " \
        "description='Strategy to apply to run multi-range requests' " \
        "default='PARALLEL'>" \
    "       <Value>PARALLEL</Value>" \
    "       <Value>SERIAL</Value>" \
    "  </Option>" \
    "  <Option name='GDAL_HTTP_MULTIPLEX' type='boolean' " \
        "description='Whether to enable HTTP/2 multiplexing' default='YES'/>" \
    "  <Option name='GDAL_HTTP_MERGE_CONSECUTIVE_RANGES' type='boolean' " \
        "description='Whether to merge consecutive ranges in multirange " \
        "requests' default='YES'/>" \
    "  <Option name='CPL_VSIL_CURL_NON_CACHED' type='string' " \
        "description='Colon-separated list of filenames whose content" \
        "must not be cached across open attempts'/>" \
    "  <Option name='CPL_VSIL_CURL_ALLOWED_FILENAME' type='string' " \
        "description='Single filename that is allowed to be opened'/>" \
    "  <Option name='CPL_VSIL_CURL_ALLOWED_EXTENSIONS' type='string' " \
        "description='Comma or space separated list of allowed file " \
        "extensions'/>" \
    "  <Option name='GDAL_DISABLE_READDIR_ON_OPEN' type='string-select' " \
        "description='Whether to disable establishing the list of files in " \
        "the directory of the current filename' default='NO'>" \
    "       <Value>NO</Value>" \
    "       <Value>YES</Value>" \
    "       <Value>EMPTY_DIR</Value>" \
    "  </Option>" \
    "  <Option name='VSI_CACHE' type='boolean' " \
        "description='Whether to cache in memory the contents of the opened " \
        "file as soon as they are read' default='NO'/>" \
    "  <Option name='CPL_VSIL_CURL_CHUNK_SIZE' type='integer' " \
        "description='Size in bytes of the minimum amount of data read in a " \
        "file' default='16384' min='1024' max='10485760'/>" \
    "  <Option name='CPL_VSIL_CURL_CACHE_SIZE' type='integer' " \
        "description='Size in bytes of the global /vsicurl/ cache' " \
        "default='16384000'/>" \
    "  <Option name='CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE' type='boolean' " \
        "description='Whether to skip files with Glacier storage class in " \
        "directory listing.' default='YES'/>"

const char* VSICurlFilesystemHandler::GetOptionsStatic()
{
    return VSICURL_OPTIONS;
}

const char* VSICurlFilesystemHandler::GetOptions()
{
    static CPLString osOptions(CPLString("<Options>") + GetOptionsStatic() + "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                        IsAllowedFilename()                           */
/************************************************************************/

bool VSICurlFilesystemHandler::IsAllowedFilename( const char* pszFilename )
{
    const char* pszAllowedFilename =
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME", nullptr);
    if( pszAllowedFilename != nullptr )
    {
        return strcmp( pszFilename, pszAllowedFilename ) == 0;
    }

    // Consider that only the files whose extension ends up with one that is
    // listed in CPL_VSIL_CURL_ALLOWED_EXTENSIONS exist on the server.  This can
    // speeds up dramatically open experience, in case the server cannot return
    // a file list.  {noext} can be used as a special token to mean file with no
    // extension.
    // For example:
    // gdalinfo --config CPL_VSIL_CURL_ALLOWED_EXTENSIONS ".tif" /vsicurl/http://igskmncngs506.cr.usgs.gov/gmted/Global_tiles_GMTED/075darcsec/bln/W030/30N030W_20101117_gmted_bln075.tif
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", nullptr);
    if( pszAllowedExtensions )
    {
        char** papszExtensions =
            CSLTokenizeString2( pszAllowedExtensions, ", ", 0 );
        const char *queryStart = strchr(pszFilename, '?');
        char *pszFilenameWithoutQuery = nullptr;
        if (queryStart != nullptr)
        {
            pszFilenameWithoutQuery = CPLStrdup(pszFilename);
            pszFilenameWithoutQuery[queryStart - pszFilename]='\0';
            pszFilename = pszFilenameWithoutQuery;
        }
        const size_t nURLLen = strlen(pszFilename);
        bool bFound = false;
        for( int i = 0; papszExtensions[i] != nullptr; i++ )
        {
            const size_t nExtensionLen = strlen(papszExtensions[i]);
            if( EQUAL(papszExtensions[i], "{noext}") )
            {
                const char* pszLastSlash = strrchr(pszFilename, '/');
                if( pszLastSlash != nullptr && strchr(pszLastSlash, '.') == nullptr )
                {
                    bFound = true;
                    break;
                }
            }
            else if( nURLLen > nExtensionLen &&
                     EQUAL(pszFilename + nURLLen - nExtensionLen,
                           papszExtensions[i]) )
            {
                bFound = true;
                break;
            }
        }

        CSLDestroy(papszExtensions);
        if( pszFilenameWithoutQuery ) {
            CPLFree(pszFilenameWithoutQuery);
        }

        return bFound;
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSICurlFilesystemHandler::Open( const char *pszFilename,
                                                  const char *pszAccess,
                                                  bool bSetError,
                                                  CSLConstList /* papszOptions */ )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) &&
        !STARTS_WITH_CI(pszFilename, "/vsicurl?") )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr ||
        strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsicurl");
        return nullptr;
    }
    if( !IsAllowedFilename( pszFilename ) )
        return nullptr;

    bool bListDir = true;
    bool bEmptyDir = false;
    CPL_IGNORE_RET_VAL(
        VSICurlGetURLFromFilename(pszFilename, nullptr, nullptr, nullptr,
                                  &bListDir, &bEmptyDir, nullptr));

    const char* pszOptionVal =
        CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
    const bool bSkipReadDir = !bListDir || bEmptyDir ||
        EQUAL(pszOptionVal, "EMPTY_DIR") || CPLTestBool(pszOptionVal) ||
        !AllowCachedDataFor(pszFilename);

    CPLString osFilename(pszFilename);
    bool bGotFileList = true;
    bool bForceExistsCheck = false;
    FileProp cachedFileProp;
    if( !(GetCachedFileProp(osFilename + strlen(GetFSPrefix()), cachedFileProp) &&
          cachedFileProp.eExists == EXIST_YES) &&
        strchr(CPLGetFilename(osFilename), '.') != nullptr &&
        !STARTS_WITH(CPLGetExtension(osFilename), "zip") && !bSkipReadDir)
    {
        char** papszFileList =
            ReadDirInternal(CPLGetDirname(osFilename), 0, &bGotFileList);
        const bool bFound =
            VSICurlIsFileInList(papszFileList,
                                CPLGetFilename(osFilename)) != -1;
        if( bGotFileList && !bFound )
        {
            // Some file servers are case insensitive, so in case there is a
            // match with case difference, do a full check just in case.
            // e.g.
            // http://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg004/MEGA90N000CB.IMG
            // that is queried by
            // gdalinfo /vsicurl/http://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg004/mega90n000cb.lbl
            if( CSLFindString(papszFileList, CPLGetFilename(osFilename)) != -1 )
            {
                bForceExistsCheck = true;
            }
            else
            {
                CSLDestroy(papszFileList);
                return nullptr;
            }
        }
        CSLDestroy(papszFileList);
    }

    VSICurlHandle* poHandle =
        CreateFileHandle(osFilename);
    if( poHandle == nullptr )
        return nullptr;
    if( !bGotFileList || bForceExistsCheck )
    {
        // If we didn't get a filelist, check that the file really exists.
        if( !poHandle->Exists(bSetError) )
        {
            delete poHandle;
            return nullptr;
        }
    }

    if( CPLTestBool( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
        return VSICreateCachedFile( poHandle );
    else
        return poHandle;
}

/************************************************************************/
/*                        VSICurlParserFindEOL()                        */
/*                                                                      */
/*      Small helper function for VSICurlPaseHTMLFileList() to find     */
/*      the end of a line in the directory listing.  Either a <br>      */
/*      or newline.                                                     */
/************************************************************************/

static char *VSICurlParserFindEOL( char *pszData )

{
    while( *pszData != '\0' && *pszData != '\n' &&
           !STARTS_WITH_CI(pszData, "<br>") )
        pszData++;

    if( *pszData == '\0' )
        return nullptr;

    return pszData;
}

/************************************************************************/
/*                   VSICurlParseHTMLDateTimeFileSize()                 */
/************************************************************************/

static const char* const apszMonths[] = { "January", "February", "March",
                                          "April", "May", "June", "July",
                                          "August", "September", "October",
                                          "November", "December" };

static bool VSICurlParseHTMLDateTimeFileSize( const char* pszStr,
                                              struct tm& brokendowntime,
                                              GUIntBig& nFileSize,
                                              GIntBig& mTime )
{
    for( int iMonth = 0; iMonth < 12; iMonth++ )
    {
        char szMonth[32] = {};
        szMonth[0] = '-';
        memcpy(szMonth + 1, apszMonths[iMonth], 3);
        szMonth[4] = '-';
        szMonth[5] = '\0';
        const char* pszMonthFound = strstr(pszStr, szMonth);
        if (pszMonthFound)
        {
            // Format of Apache, like in
            // http://download.osgeo.org/gdal/data/gtiff/
            // "17-May-2010 12:26"
            if( pszMonthFound - pszStr > 2 && strlen(pszMonthFound) > 15 &&
                pszMonthFound[-2 + 11] == ' ' && pszMonthFound[-2 + 14] == ':' )
            {
                pszMonthFound -= 2;
                int nDay = atoi(pszMonthFound);
                int nYear = atoi(pszMonthFound + 7);
                int nHour = atoi(pszMonthFound + 12);
                int nMin = atoi(pszMonthFound + 15);
                if( nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60 )
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
            }
            return false;
        }

        /* Microsoft IIS */
        snprintf( szMonth, sizeof(szMonth), " %s ", apszMonths[iMonth] );
        pszMonthFound = strstr(pszStr, szMonth);
        if( pszMonthFound )
        {
            int nLenMonth = static_cast<int>(strlen(apszMonths[iMonth]));
            if( pszMonthFound - pszStr > 2 &&
                pszMonthFound[-1] != ',' &&
                pszMonthFound[-2] != ' ' &&
                static_cast<int>(strlen(pszMonthFound - 2)) >
                2 + 1 + nLenMonth + 1 + 4 + 1 + 5 + 1 + 4 )
            {
                /* Format of http://ortho.linz.govt.nz/tifs/1994_95/ */
                /* "        Friday, 21 April 2006 12:05 p.m.     48062343 m35a_fy_94_95.tif" */
                pszMonthFound -= 2;
                int nDay = atoi(pszMonthFound);
                int nCurOffset = 2 + 1 + nLenMonth + 1;
                int nYear = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 4 + 1;
                int nHour = atoi(pszMonthFound + nCurOffset);
                if( nHour < 10 )
                    nCurOffset += 1 + 1;
                else
                    nCurOffset += 2 + 1;
                const int nMin = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                if( STARTS_WITH(pszMonthFound + nCurOffset, "p.m.") )
                    nHour += 12;
                else if( !STARTS_WITH(pszMonthFound + nCurOffset, "a.m.") )
                    nHour = -1;
                nCurOffset += 4;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while( *pszFilesize == ' ' )
                    pszFilesize++;
                if( *pszFilesize >= '1' && *pszFilesize <= '9' )
                    nFileSize =
                        CPLScanUIntBig(pszFilesize,
                                       static_cast<int>(strlen(pszFilesize)));

                if( nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60 )
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
                nFileSize = 0;
            }
            else if( pszMonthFound - pszStr > 1 &&
                     pszMonthFound[-1] == ',' &&
                     static_cast<int>(strlen(pszMonthFound)) >
                     1 + nLenMonth + 1 + 2 + 1 + 1 + 4 + 1 + 5 + 1 + 2 )
            {
                // Format of http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/
                // "        Sunday, June 20, 2010  6:46 PM    233170905 NWF2007LULCForSDE.zip"
                pszMonthFound += 1;
                int nCurOffset = nLenMonth + 1;
                int nDay = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1 + 1;
                int nYear = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 4 + 1;
                int nHour = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                const int nMin = atoi(pszMonthFound + nCurOffset);
                nCurOffset += 2 + 1;
                if( STARTS_WITH(pszMonthFound + nCurOffset, "PM") )
                    nHour += 12;
                else if( !STARTS_WITH(pszMonthFound + nCurOffset, "AM") )
                    nHour = -1;
                nCurOffset += 2;

                const char* pszFilesize = pszMonthFound + nCurOffset;
                while( *pszFilesize == ' ' )
                    pszFilesize++;
                if( *pszFilesize >= '1' && *pszFilesize <= '9' )
                    nFileSize =
                        CPLScanUIntBig(pszFilesize,
                                       static_cast<int>(strlen(pszFilesize)));

                if( nDay >= 1 && nDay <= 31 && nYear >= 1900 &&
                    nHour >= 0 && nHour <= 24 && nMin >= 0 && nMin < 60 )
                {
                    brokendowntime.tm_year = nYear - 1900;
                    brokendowntime.tm_mon = iMonth;
                    brokendowntime.tm_mday = nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMin;
                    mTime = CPLYMDHMSToUnixTime(&brokendowntime);

                    return true;
                }
                nFileSize = 0;
            }
            return false;
        }
    }

    return false;
}

/************************************************************************/
/*                          ParseHTMLFileList()                         */
/*                                                                      */
/*      Parse a file list document and return all the components.       */
/************************************************************************/

char** VSICurlFilesystemHandler::ParseHTMLFileList( const char* pszFilename,
                                                    int nMaxFiles,
                                                    char* pszData,
                                                    bool* pbGotFileList )
{
    *pbGotFileList = false;

    CPLString osURL(VSICurlGetURLFromFilename(pszFilename, nullptr, nullptr, nullptr,
                                              nullptr, nullptr, nullptr));
    const char* pszDir = nullptr;
    if( STARTS_WITH_CI(osURL, "http://") )
        pszDir = strchr(osURL.c_str() + strlen("http://"), '/');
    else if( STARTS_WITH_CI(osURL, "https://") )
        pszDir = strchr(osURL.c_str() + strlen("https://"), '/');
    else if( STARTS_WITH_CI(osURL, "ftp://") )
        pszDir = strchr(osURL.c_str() + strlen("ftp://"), '/');
    if( pszDir == nullptr )
        pszDir = "";

    /* Apache */
    CPLString osExpectedString = "<title>Index of ";
    osExpectedString += pszDir;
    osExpectedString += "</title>";
    /* shttpd */
    CPLString osExpectedString2 = "<title>Index of ";
    osExpectedString2 += pszDir;
    osExpectedString2 += "/</title>";
    /* FTP */
    CPLString osExpectedString3 = "FTP Listing of ";
    osExpectedString3 += pszDir;
    osExpectedString3 += "/";
    /* Apache 1.3.33 */
    CPLString osExpectedString4 = "<TITLE>Index of ";
    osExpectedString4 += pszDir;
    osExpectedString4 += "</TITLE>";

    // The listing of
    // http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/
    // has
    // "<title>Index of /srtm/SRTM_image_sample/picture examples</title>"
    // so we must try unescaped %20 also.
    // Similar with
    // http://datalib.usask.ca/gis/Data/Central_America_goodbutdoweown%3f/
    CPLString osExpectedString_unescaped;
    if( strchr(pszDir, '%') )
    {
        char* pszUnescapedDir = CPLUnescapeString(pszDir, nullptr, CPLES_URL);
        osExpectedString_unescaped = "<title>Index of ";
        osExpectedString_unescaped += pszUnescapedDir;
        osExpectedString_unescaped += "</title>";
        CPLFree(pszUnescapedDir);
    }

    char* c = nullptr;
    int nCount = 0;
    int nCountTable = 0;
    CPLStringList oFileList;
    char* pszLine = pszData;
    bool bIsHTMLDirList = false;

    while( (c = VSICurlParserFindEOL( pszLine )) != nullptr )
    {
        *c = '\0';

        // To avoid false positive on pages such as
        // http://www.ngs.noaa.gov/PC_PROD/USGG2009BETA
        // This is a heuristics, but normal HTML listing of files have not more
        // than one table.
        if( strstr(pszLine, "<table") )
        {
            nCountTable++;
            if( nCountTable == 2 )
            {
                *pbGotFileList = false;
                return nullptr;
            }
        }

        if( !bIsHTMLDirList &&
            (strstr(pszLine, osExpectedString.c_str()) ||
             strstr(pszLine, osExpectedString2.c_str()) ||
             strstr(pszLine, osExpectedString3.c_str()) ||
             strstr(pszLine, osExpectedString4.c_str()) ||
             (!osExpectedString_unescaped.empty() &&
              strstr(pszLine, osExpectedString_unescaped.c_str()))) )
        {
            bIsHTMLDirList = true;
            *pbGotFileList = true;
        }
        // Subversion HTTP listing
        // or Microsoft-IIS/6.0 listing
        // (e.g. http://ortho.linz.govt.nz/tifs/2005_06/) */
        else if( !bIsHTMLDirList && strstr(pszLine, "<title>") )
        {
            // Detect something like:
            // <html><head><title>gdal - Revision 20739: /trunk/autotest/gcore/data</title></head> */
            // The annoying thing is that what is after ': ' is a subpart of
            // what is after http://server/
            char* pszSubDir = strstr(pszLine, ": ");
            if( pszSubDir == nullptr )
                // or <title>ortho.linz.govt.nz - /tifs/2005_06/</title>
                pszSubDir = strstr(pszLine, "- ");
            if( pszSubDir )
            {
                pszSubDir += 2;
                char* pszTmp = strstr(pszSubDir, "</title>");
                if( pszTmp )
                {
                    if( pszTmp[-1] == '/' )
                        pszTmp[-1] = 0;
                    else
                        *pszTmp = 0;
                    if( strstr(pszDir, pszSubDir) )
                    {
                        bIsHTMLDirList = true;
                        *pbGotFileList = true;
                    }
                }
            }
        }
        else if( bIsHTMLDirList &&
                 (strstr(pszLine, "<a href=\"") != nullptr ||
                  strstr(pszLine, "<A HREF=\"") != nullptr) &&
                 // Exclude absolute links, like to subversion home.
                 strstr(pszLine, "<a href=\"http://") == nullptr &&
                 // exclude parent directory.
                 strstr(pszLine, "Parent Directory") == nullptr )
        {
            char *beginFilename = strstr(pszLine, "<a href=\"");
            if( beginFilename == nullptr )
                beginFilename = strstr(pszLine, "<A HREF=\"");
            beginFilename += strlen("<a href=\"");
            char *endQuote = strchr(beginFilename, '"');
            if( endQuote &&
                !STARTS_WITH(beginFilename, "?C=") &&
                !STARTS_WITH(beginFilename, "?N=") )
            {
                struct tm brokendowntime;
                memset(&brokendowntime, 0, sizeof(brokendowntime));
                GUIntBig nFileSize = 0;
                GIntBig mTime = 0;

                VSICurlParseHTMLDateTimeFileSize(pszLine,
                                                 brokendowntime,
                                                 nFileSize,
                                                 mTime);

                *endQuote = '\0';

                // Remove trailing slash, that are returned for directories by
                // Apache.
                bool bIsDirectory = false;
                if( endQuote[-1] == '/' )
                {
                    bIsDirectory = true;
                    endQuote[-1] = 0;
                }

                // shttpd links include slashes from the root directory.
                // Skip them.
                while( strchr(beginFilename, '/') )
                    beginFilename = strchr(beginFilename, '/') + 1;

                if( strcmp(beginFilename, ".") != 0 &&
                    strcmp(beginFilename, "..") != 0 )
                {
                    CPLString osCachedFilename =
                        CPLSPrintf("%s/%s", osURL.c_str(),
                                   beginFilename);

                    FileProp cachedFileProp;
                    GetCachedFileProp(osCachedFilename, cachedFileProp);
                    cachedFileProp.eExists = EXIST_YES;
                    cachedFileProp.bIsDirectory = bIsDirectory;
                    cachedFileProp.mTime = static_cast<time_t>(mTime);
                    cachedFileProp.bHasComputedFileSize = nFileSize > 0;
                    cachedFileProp.fileSize = nFileSize;
                    SetCachedFileProp(osCachedFilename, cachedFileProp);

                    oFileList.AddString( beginFilename );
                    if( ENABLE_DEBUG_VERBOSE )
                    {
                        CPLDebug(GetDebugKey(),
                                 "File[%d] = %s, is_dir = %d, size = "
                                 CPL_FRMT_GUIB
                                 ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                 nCount, osCachedFilename.c_str(), bIsDirectory ? 1 : 0,
                                 nFileSize,
                                 brokendowntime.tm_year + 1900,
                                 brokendowntime.tm_mon + 1,
                                 brokendowntime.tm_mday,
                                 brokendowntime.tm_hour, brokendowntime.tm_min,
                                 brokendowntime.tm_sec);
                    }
                    nCount++;

                    if( nMaxFiles > 0 && oFileList.Count() > nMaxFiles )
                        break;
                }
            }
        }
        pszLine = c + 1;
    }

    return oFileList.StealList();
}

/************************************************************************/
/*                         VSICurlGetToken()                            */
/************************************************************************/

static char* VSICurlGetToken( char* pszCurPtr, char** ppszNextToken )
{
    if( pszCurPtr == nullptr )
        return nullptr;

    while( (*pszCurPtr) == ' ' )
        pszCurPtr++;
    if( *pszCurPtr == '\0' )
        return nullptr;

    char* pszToken = pszCurPtr;
    while( (*pszCurPtr) != ' ' && (*pszCurPtr) != '\0' )
        pszCurPtr++;
    if( *pszCurPtr == '\0' )
    {
        *ppszNextToken = nullptr;
    }
    else
    {
        *pszCurPtr = '\0';
        pszCurPtr++;
        while( (*pszCurPtr) == ' ' )
            pszCurPtr++;
        *ppszNextToken = pszCurPtr;
    }

    return pszToken;
}

/************************************************************************/
/*                    VSICurlParseFullFTPLine()                         */
/************************************************************************/

/* Parse lines like the following ones :
-rw-r--r--    1 10003    100           430 Jul 04  2008 COPYING
lrwxrwxrwx    1 ftp      ftp            28 Jun 14 14:13 MPlayer -> mirrors/mplayerhq.hu/MPlayer
-rw-r--r--    1 ftp      ftp      725614592 May 13 20:13 Fedora-15-x86_64-Live-KDE.iso
drwxr-xr-x  280 1003  1003  6656 Aug 26 04:17 gnu
*/

static bool VSICurlParseFullFTPLine( char* pszLine,
                                     char*& pszFilename,
                                     bool& bSizeValid,
                                     GUIntBig& nSize,
                                     bool& bIsDirectory,
                                     GIntBig& nUnixTime )
{
    char* pszNextToken = pszLine;
    char* pszPermissions = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszPermissions == nullptr || strlen(pszPermissions) != 10 )
        return false;
    bIsDirectory = pszPermissions[0] == 'd';

    for( int i = 0; i < 3; i++ )
    {
        if( VSICurlGetToken(pszNextToken, &pszNextToken) == nullptr )
            return false;
    }

    char* pszSize = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszSize == nullptr )
        return false;

    if( pszPermissions[0] == '-' )
    {
        // Regular file.
        bSizeValid = true;
        nSize = CPLScanUIntBig(pszSize, static_cast<int>(strlen(pszSize)));
    }

    struct tm brokendowntime;
    memset(&brokendowntime, 0, sizeof(brokendowntime));
    bool bBrokenDownTimeValid = true;

    char* pszMonth = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszMonth == nullptr || strlen(pszMonth) != 3 )
        return false;

    int i = 0;  // Used after for.
    for( ; i < 12; i++ )
    {
        if( EQUALN(pszMonth, apszMonths[i], 3) )
            break;
    }
    if( i < 12 )
        brokendowntime.tm_mon = i;
    else
        bBrokenDownTimeValid = false;

    char* pszDay = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszDay == nullptr || (strlen(pszDay) != 1 && strlen(pszDay) != 2) )
        return false;
    int nDay = atoi(pszDay);
    if( nDay >= 1 && nDay <= 31 )
        brokendowntime.tm_mday = nDay;
    else
        bBrokenDownTimeValid = false;

    char* pszHourOrYear = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszHourOrYear == nullptr ||
        (strlen(pszHourOrYear) != 4 && strlen(pszHourOrYear) != 5) )
        return false;
    if( strlen(pszHourOrYear) == 4 )
    {
        brokendowntime.tm_year = atoi(pszHourOrYear) - 1900;
    }
    else
    {
        time_t sTime;
        time(&sTime);
        struct tm currentBrokendowntime;
        CPLUnixTimeToYMDHMS(static_cast<GIntBig>(sTime),
                            &currentBrokendowntime);
        brokendowntime.tm_year = currentBrokendowntime.tm_year;
        brokendowntime.tm_hour = atoi(pszHourOrYear);
        brokendowntime.tm_min = atoi(pszHourOrYear + 3);
    }

    if( bBrokenDownTimeValid )
        nUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
    else
        nUnixTime = 0;

    if( pszNextToken == nullptr )
        return false;

    pszFilename = pszNextToken;

    char* pszCurPtr = pszFilename;
    while( *pszCurPtr != '\0')
    {
        // In case of a link, stop before the pointed part of the link.
        if( pszPermissions[0] == 'l' && STARTS_WITH(pszCurPtr, " -> ") )
        {
            break;
        }
        pszCurPtr++;
    }
    *pszCurPtr = '\0';

    return true;
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

CPLString
VSICurlFilesystemHandler::GetURLFromFilename( const CPLString& osFilename )
{
    return VSICurlGetURLFromFilename(osFilename, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

/************************************************************************/
/*                         RegisterEmptyDir()                           */
/************************************************************************/

void VSICurlFilesystemHandler::RegisterEmptyDir( const CPLString& osDirname )
{
    CachedDirList cachedDirList;
    cachedDirList.bGotFileList = true;
    cachedDirList.oFileList.AddString(".");
    SetCachedDirList(osDirname, cachedDirList);
}

/************************************************************************/
/*                          GetFileList()                               */
/************************************************************************/

char** VSICurlFilesystemHandler::GetFileList(const char *pszDirname,
                                             int nMaxFiles,
                                             bool* pbGotFileList)
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);

    *pbGotFileList = false;

    bool bListDir = true;
    bool bEmptyDir = false;
    CPLString osURL(
        VSICurlGetURLFromFilename(pszDirname, nullptr, nullptr, nullptr,
                                  &bListDir, &bEmptyDir, nullptr));
    if( bEmptyDir )
    {
        *pbGotFileList = true;
        return CSLAddString(nullptr, ".");
    }
    if( !bListDir )
        return nullptr;

    // HACK (optimization in fact) for MBTiles driver.
    if( strstr(pszDirname, ".tiles.mapbox.com") != nullptr )
        return nullptr;

    if( STARTS_WITH(osURL, "ftp://") )
    {
        WriteFuncStruct sWriteFuncData;
        sWriteFuncData.pBuffer = nullptr;

        CPLString osDirname(osURL);
        osDirname += '/';

        char** papszFileList = nullptr;

        CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osDirname);
        CURL* hCurlHandle = curl_easy_init();

        for( int iTry = 0; iTry < 2; iTry++ )
        {
            struct curl_slist* headers =
                VSICurlSetOptions(hCurlHandle, osDirname.c_str(), nullptr);

            // On the first pass, we want to try fetching all the possible
            // information (filename, file/directory, size). If that does not
            // work, then try again with CURLOPT_DIRLISTONLY set.
            if( iTry == 1 )
            {
// 7.16.4
#if LIBCURL_VERSION_NUM <= 0x071004
                curl_easy_setopt(hCurlHandle, CURLOPT_FTPLISTONLY, 1);
#elif LIBCURL_VERSION_NUM > 0x071004
                curl_easy_setopt(hCurlHandle, CURLOPT_DIRLISTONLY, 1);
#endif
            }

            VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                             VSICurlHandleWriteFunc);

            char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
            curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

            MultiPerform(hCurlMultiHandle, hCurlHandle);

            curl_slist_free_all(headers);

            if( sWriteFuncData.pBuffer == nullptr )
            {
                curl_easy_cleanup(hCurlHandle);
                return nullptr;
            }

            char* pszLine = sWriteFuncData.pBuffer;
            char* c = nullptr;
            int nCount = 0;

            if( STARTS_WITH_CI(pszLine, "<!DOCTYPE HTML") ||
                STARTS_WITH_CI(pszLine, "<HTML>") )
            {
                papszFileList = ParseHTMLFileList(pszDirname,
                                                  nMaxFiles,
                                                  sWriteFuncData.pBuffer,
                                                  pbGotFileList);
                break;
            }
            else if( iTry == 0 )
            {
                CPLStringList oFileList;
                *pbGotFileList = true;

                while( (c = strchr(pszLine, '\n')) != nullptr)
                {
                    *c = 0;
                    if( c - pszLine > 0 && c[-1] == '\r' )
                        c[-1] = 0;

                    char* pszFilename = nullptr;
                    bool bSizeValid = false;
                    GUIntBig nFileSize = 0;
                    bool bIsDirectory = false;
                    GIntBig mUnixTime = 0;
                    if( !VSICurlParseFullFTPLine(pszLine, pszFilename,
                                                 bSizeValid, nFileSize,
                                                 bIsDirectory, mUnixTime) )
                        break;

                    if( strcmp(pszFilename, ".") != 0 &&
                        strcmp(pszFilename, "..") != 0 )
                    {
                        CPLString osCachedFilename =
                            CPLSPrintf("%s/%s",
                                       osURL.c_str(),
                                       pszFilename);

                        FileProp cachedFileProp;
                        GetCachedFileProp(osCachedFilename, cachedFileProp);
                        cachedFileProp.eExists = EXIST_YES;
                        cachedFileProp.bIsDirectory = bIsDirectory;
                        cachedFileProp.mTime = static_cast<time_t>(mUnixTime);
                        cachedFileProp.bHasComputedFileSize = bSizeValid;
                        cachedFileProp.fileSize = nFileSize;
                        SetCachedFileProp(osCachedFilename, cachedFileProp);

                        oFileList.AddString(pszFilename);
                        if( ENABLE_DEBUG_VERBOSE )
                        {
                            struct tm brokendowntime;
                            CPLUnixTimeToYMDHMS(mUnixTime, &brokendowntime);
                            CPLDebug(GetDebugKey(),
                                     "File[%d] = %s, is_dir = %d, size = "
                                     CPL_FRMT_GUIB
                                     ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                     nCount, pszFilename, bIsDirectory ? 1 : 0,
                                     nFileSize,
                                     brokendowntime.tm_year + 1900,
                                     brokendowntime.tm_mon + 1,
                                     brokendowntime.tm_mday,
                                     brokendowntime.tm_hour,
                                     brokendowntime.tm_min,
                                     brokendowntime.tm_sec);
                        }

                        nCount++;

                        if( nMaxFiles > 0 && oFileList.Count() > nMaxFiles )
                            break;
                    }

                    pszLine = c + 1;
                }

                if( c == nullptr )
                {
                    papszFileList = oFileList.StealList();
                    break;
                }
            }
            else
            {
                CPLStringList oFileList;
                *pbGotFileList = true;

                while( (c = strchr(pszLine, '\n')) != nullptr)
                {
                    *c = 0;
                    if( c - pszLine > 0 && c[-1] == '\r' )
                        c[-1] = 0;

                    if( strcmp(pszLine, ".") != 0 &&
                        strcmp(pszLine, "..") != 0 )
                    {
                        oFileList.AddString(pszLine);
                        if( ENABLE_DEBUG_VERBOSE )
                        {
                            CPLDebug(GetDebugKey(),
                                     "File[%d] = %s", nCount, pszLine);
                        }
                        nCount++;
                    }

                    pszLine = c + 1;
                }

                papszFileList = oFileList.StealList();
            }

            CPLFree(sWriteFuncData.pBuffer);
            sWriteFuncData.pBuffer = nullptr;
        }

        CPLFree(sWriteFuncData.pBuffer);
        curl_easy_cleanup(hCurlHandle);

        return papszFileList;
    }

    // Try to recognize HTML pages that list the content of a directory.
    // Currently this supports what Apache and shttpd can return.
    else if( STARTS_WITH(osURL, "http://") ||
             STARTS_WITH(osURL, "https://") )
    {
        CPLString osDirname(osURL);
        osDirname += '/';

        CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osDirname);
        CURL* hCurlHandle = curl_easy_init();

        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osDirname.c_str(), nullptr);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, nullptr);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        curl_slist_free_all(headers);

        NetworkStatisticsLogger::LogGET(sWriteFuncData.nSize);

        if( sWriteFuncData.pBuffer == nullptr )
        {
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }

        char** papszFileList = nullptr;
        if( STARTS_WITH_CI(sWriteFuncData.pBuffer, "<?xml") &&
            strstr(sWriteFuncData.pBuffer, "<ListBucketResult") != nullptr )
        {
            CPLString osNextMarker;
            CPLStringList osFileList;
            CPLString osBaseURL(pszDirname);
            osBaseURL += "/";
            bool bIsTruncated = true;
            const bool bIgnoreGlacier = CPLTestBool(
                CPLGetConfigOption("CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE", "YES"));
            bool ret = AnalyseS3FileList( osBaseURL,
                               sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIgnoreGlacier,
                               bIsTruncated );
            // If the list is truncated, then don't report it.
            if( ret && !bIsTruncated )
            {
                if( osFileList.empty() )
                {
                    // To avoid an error to be reported
                    osFileList.AddString(".");
                }
                papszFileList = osFileList.StealList();
                *pbGotFileList = true;
            }
        }
        else
        {
            papszFileList = ParseHTMLFileList(pszDirname,
                                              nMaxFiles,
                                              sWriteFuncData.pBuffer,
                                              pbGotFileList);
        }

        CPLFree(sWriteFuncData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        return papszFileList;
    }

    return nullptr;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Stat( const char *pszFilename,
                                    VSIStatBufL *pStatBuf,
                                    int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) &&
        !STARTS_WITH_CI(pszFilename, "/vsicurl?") )
        return -1;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("Stat");

    const CPLString osFilename(pszFilename);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if( !IsAllowedFilename( pszFilename ) )
        return -1;

    bool bListDir = true;
    bool bEmptyDir = false;
    CPLString osURL(
        VSICurlGetURLFromFilename(pszFilename, nullptr, nullptr, nullptr,
                                  &bListDir, &bEmptyDir, nullptr));

    const char* pszOptionVal =
        CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
    const bool bSkipReadDir = !bListDir || bEmptyDir ||
        EQUAL(pszOptionVal, "EMPTY_DIR") || CPLTestBool(pszOptionVal) ||
        !AllowCachedDataFor(pszFilename);

    // Does it look like a FTP directory?
    if( STARTS_WITH(osURL, "ftp://") &&
        osFilename.back() == '/' && !bSkipReadDir )
    {
        char** papszFileList = ReadDirEx(osFilename, 0);
        if( papszFileList )
        {
            pStatBuf->st_mode = S_IFDIR;
            pStatBuf->st_size = 0;

            CSLDestroy(papszFileList);

            return 0;
        }
        return -1;
    }
    else if( strchr(CPLGetFilename(osFilename), '.') != nullptr &&
             !STARTS_WITH_CI(CPLGetExtension(osFilename), "zip") &&
             strstr(osFilename, ".zip.") != nullptr &&
             strstr(osFilename, ".ZIP.") != nullptr &&
             !bSkipReadDir )
    {
        bool bGotFileList = false;
        char** papszFileList =
            ReadDirInternal(CPLGetDirname(osFilename), 0, &bGotFileList);
        const bool bFound =
            VSICurlIsFileInList(papszFileList,
                                CPLGetFilename(osFilename)) != -1;
        CSLDestroy(papszFileList);
        if( bGotFileList && !bFound )
        {
            return -1;
        }
    }

    VSICurlHandle* poHandle =
        CreateFileHandle( osFilename );
    if( poHandle == nullptr )
        return -1;

    if( poHandle->IsKnownFileSize() ||
        ((nFlags & VSI_STAT_SIZE_FLAG) && !poHandle->IsDirectory() &&
         CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_SLOW_GET_SIZE",
                                        "YES"))) )
    {
        pStatBuf->st_size = poHandle->GetFileSize(false);
    }

    const int nRet =
        poHandle->Exists((nFlags & VSI_STAT_SET_ERROR_FLAG) > 0) ? 0 : -1;
    pStatBuf->st_mtime = poHandle->GetMTime();
    pStatBuf->st_mode = static_cast<unsigned short>(poHandle->GetMode());
    if( pStatBuf->st_mode == 0 )
        pStatBuf->st_mode = poHandle->IsDirectory() ? S_IFDIR : S_IFREG;
    delete poHandle;
    return nRet;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Unlink( const char * /* pszFilename */ )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSICurlFilesystemHandler::Rename( const char * /* oldpath */,
                                      const char * /* newpath */ )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Mkdir( const char * /* pszDirname */,
                                     long /* nMode */ )
{
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSICurlFilesystemHandler::Rmdir( const char * /* pszDirname */ )
{
    return -1;
}

/************************************************************************/
/*                             ReadDirInternal()                        */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDirInternal( const char *pszDirname,
                                                  int nMaxFiles,
                                                  bool* pbGotFileList )
{
    CPLString osDirname(pszDirname);

    const char* pszUpDir = strstr(osDirname, "/..");
    if( pszUpDir != nullptr )
    {
        int pos = static_cast<int>(pszUpDir - osDirname.c_str() - 1);
        while( pos >= 0 && osDirname[pos] != '/' )
            pos--;
        if( pos >= 1 )
        {
            osDirname = osDirname.substr(0, pos) + CPLString(pszUpDir + 3);
        }
    }

    CPLString osDirnameOri(osDirname);
    if( osDirname + "/" == GetFSPrefix() )
    {
        osDirname += "/";
    }
    else if( osDirname != GetFSPrefix() )
    {
        while( !osDirname.empty() && osDirname.back() == '/' )
            osDirname.erase(osDirname.size() - 1);
    }

    if( osDirname.size() < GetFSPrefix().size() )
    {
        if( pbGotFileList )
            *pbGotFileList = true;
        return nullptr;
    }

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("ReadDir");

    CPLMutexHolder oHolder( &hMutex );

    // If we know the file exists and is not a directory,
    // then don't try to list its content.
    FileProp cachedFileProp;
    if( GetCachedFileProp(GetURLFromFilename(osDirname), cachedFileProp) &&
        cachedFileProp.eExists == EXIST_YES && !cachedFileProp.bIsDirectory )
    {
        if( osDirnameOri != osDirname )
        {
            if( GetCachedFileProp((GetURLFromFilename(osDirname) + "/").c_str(), cachedFileProp) &&
                cachedFileProp.eExists == EXIST_YES && !cachedFileProp.bIsDirectory )
            {
                if( pbGotFileList )
                    *pbGotFileList = true;
                return nullptr;
            }
        }
        else
        {
            if( pbGotFileList )
                *pbGotFileList = true;
            return nullptr;
        }
    }

    CachedDirList cachedDirList;
    if( !GetCachedDirList(osDirname, cachedDirList) )
    {
        cachedDirList.oFileList.Assign(
            GetFileList(osDirname, nMaxFiles,
                        &cachedDirList.bGotFileList), true);
        if( cachedDirList.bGotFileList && cachedDirList.oFileList.empty() )
        {
            // To avoid an error to be reported
            cachedDirList.oFileList.AddString(".");
        }
        if( nMaxFiles <= 0 || cachedDirList.oFileList.size() < nMaxFiles )
        {
            // Only cache content if we didn't hit the limitation
            SetCachedDirList(osDirname, cachedDirList);
        }
    }

    if( pbGotFileList )
        *pbGotFileList = cachedDirList.bGotFileList;

    return CSLDuplicate(cachedDirList.oFileList.List());
}

/************************************************************************/
/*                        InvalidateDirContent()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateDirContent( const char *pszDirname )
{
    CPLMutexHolder oHolder( &hMutex );

    CachedDirList oCachedDirList;
    if( oCacheDirList.tryGet(std::string(pszDirname), oCachedDirList) )
    {
        nCachedFilesInDirList -= oCachedDirList.oFileList.size();
        oCacheDirList.remove(std::string(pszDirname));
    }
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char** VSICurlFilesystemHandler::ReadDirEx( const char *pszDirname,
                                            int nMaxFiles )
{
    return ReadDirInternal(pszDirname, nMaxFiles, nullptr);
}

/************************************************************************/
/*                             SiblingFiles()                           */
/************************************************************************/

char** VSICurlFilesystemHandler::SiblingFiles( const char *pszFilename )
{
    /* Small optimization to avoid unnecessary stat'ing from PAux or ENVI */
    /* drivers. The MBTiles driver needs no companion file. */
    if( EQUAL(CPLGetExtension( pszFilename ),"mbtiles") )
    {
        return static_cast<char**> (CPLCalloc(1,sizeof(char*)));
    }
    return nullptr;
    
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char** VSICurlFilesystemHandler::GetFileMetadata( const char* pszFilename,
                                                  const char* pszDomain,
                                                  CSLConstList )
{
    if( pszDomain == nullptr || !EQUAL(pszDomain, "HEADERS") )
        return nullptr;
    std::unique_ptr<VSICurlHandle> poHandle(CreateFileHandle(pszFilename));
    if( poHandle == nullptr )
        return nullptr;

    NetworkStatisticsFileSystem oContextFS(GetFSPrefix());
    NetworkStatisticsAction oContextAction("GetFileMetadata");

    poHandle->GetFileSizeOrHeaders(true, true);
    return CSLDuplicate(poHandle->GetHeaders().List());
}

/************************************************************************/
/*                       VSIAppendWriteHandle()                         */
/************************************************************************/

VSIAppendWriteHandle::VSIAppendWriteHandle( VSICurlFilesystemHandler* poFS,
                                            const char* pszFSPrefix,
                                            const char* pszFilename,
                                            int nChunkSize ) :
        m_poFS(poFS),
        m_osFSPrefix(pszFSPrefix),
        m_osFilename(pszFilename),
        m_nBufferSize(nChunkSize)
{
    m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
    if( m_pabyBuffer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot allocate working buffer for %s writing",
                 m_osFSPrefix.c_str());
    }
}

/************************************************************************/
/*                      ~VSIAppendWriteHandle()                         */
/************************************************************************/

VSIAppendWriteHandle::~VSIAppendWriteHandle()
{
    /* WARNING: implementation should call Close() themselves */
    /* cannot be done safely from here, since Send() can be called. */
    CPLFree(m_pabyBuffer);
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIAppendWriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( !((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable %s files",
                 m_osFSPrefix.c_str());
        m_bError = true;
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIAppendWriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIAppendWriteHandle::Read( void * /* pBuffer */, size_t /* nSize */,
                               size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read not supported on writable %s files",
             m_osFSPrefix.c_str());
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                         ReadCallBackBuffer()                         */
/************************************************************************/

size_t VSIAppendWriteHandle::ReadCallBackBuffer( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIAppendWriteHandle* poThis = static_cast<VSIAppendWriteHandle *>(instream);
    const int nSizeMax = static_cast<int>(size * nitems);
    const int nSizeToWrite =
        std::min(nSizeMax,
                 poThis->m_nBufferOff - poThis->m_nBufferOffReadCallback);
    memcpy(buffer, poThis->m_pabyBuffer + poThis->m_nBufferOffReadCallback,
           nSizeToWrite);
    poThis->m_nBufferOffReadCallback += nSizeToWrite;
    return nSizeToWrite;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t
VSIAppendWriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if( m_bError )
        return 0;

    size_t nBytesToWrite = nSize * nMemb;
    if( nBytesToWrite == 0 )
        return 0;

    const GByte* pabySrcBuffer = reinterpret_cast<const GByte*>(pBuffer);
    while( nBytesToWrite > 0 )
    {
        if( m_nBufferOff == m_nBufferSize )
        {
            if( !Send(false) )
            {
                m_bError = true;
                return 0;
            }
            m_nBufferOff = 0;
        }

        const int nToWriteInBuffer = static_cast<int>(
            std::min(static_cast<size_t>(m_nBufferSize - m_nBufferOff),
                     nBytesToWrite));
        memcpy(m_pabyBuffer + m_nBufferOff, pabySrcBuffer, nToWriteInBuffer);
        pabySrcBuffer += nToWriteInBuffer;
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
    }
    return nMemb;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIAppendWriteHandle::Eof()
{
    return FALSE;
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIAppendWriteHandle::Close()
{
    int nRet = 0;
    if( !m_bClosed )
    {
        m_bClosed = true;
        if( !m_bError && !Send(true) )
            nRet = -1;
    }
    return nRet;
}

/************************************************************************/
/*                         CurlRequestHelper()                          */
/************************************************************************/

CurlRequestHelper::CurlRequestHelper()
{
    VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
}

/************************************************************************/
/*                        ~CurlRequestHelper()                          */
/************************************************************************/

CurlRequestHelper::~CurlRequestHelper()
{
    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
}

/************************************************************************/
/*                             perform()                                */
/************************************************************************/

long CurlRequestHelper::perform(CURL* hCurlHandle,
                struct curl_slist* headers,
                VSICurlFilesystemHandler *poFS,
                IVSIS3LikeHandleHelper *poS3HandleHelper)
{
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    poS3HandleHelper->ResetQueryParameters();

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                        VSICurlHandleWriteFunc);

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                        VSICurlHandleWriteFunc);

    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    MultiPerform(poFS->GetCurlMultiHandleFor(poS3HandleHelper->GetURL()),
                    hCurlHandle);

    VSICURLResetHeaderAndWriterFunctions(hCurlHandle);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    return response_code;
}

/************************************************************************/
/*                       NetworkStatisticsLogger                        */
/************************************************************************/

// Global variable
NetworkStatisticsLogger NetworkStatisticsLogger::gInstance{};
int NetworkStatisticsLogger::gnEnabled = -1; // unknown state

static void ShowNetworkStats()
{
     printf("Network statistics:\n%s\n",    // ok
            NetworkStatisticsLogger::GetReportAsSerializedJSON().c_str());
}

void NetworkStatisticsLogger::ReadEnabled()
{
    const bool bShowNetworkStats =
        CPLTestBool(CPLGetConfigOption("CPL_VSIL_SHOW_NETWORK_STATS", "NO"));
    gnEnabled = (bShowNetworkStats || CPLTestBool(
        CPLGetConfigOption("CPL_VSIL_NETWORK_STATS_ENABLED", "NO"))) ? TRUE : FALSE;
    if( bShowNetworkStats )
    {
        static bool bRegistered = false;
        if( !bRegistered )
        {
            bRegistered = true;
            atexit( ShowNetworkStats );
        }
    }
}

void NetworkStatisticsLogger::EnterFileSystem(const char* pszName)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].push_back(
        ContextPathItem(ContextPathType::FILESYSTEM, pszName));
}

void NetworkStatisticsLogger::LeaveFileSystem()
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].pop_back();
}

void NetworkStatisticsLogger::EnterFile(const char* pszName)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].push_back(
        ContextPathItem(ContextPathType::FILE, pszName));
}

void NetworkStatisticsLogger::LeaveFile()
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].pop_back();
}

void NetworkStatisticsLogger::EnterAction(const char* pszName)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].push_back(
        ContextPathItem(ContextPathType::ACTION, pszName));
}

void NetworkStatisticsLogger::LeaveAction()
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_mapThreadIdToContextPath[CPLGetPID()].pop_back();
}

std::vector<NetworkStatisticsLogger::Counters*>
NetworkStatisticsLogger::GetCountersForContext()
{
    std::vector<Counters*> v;
    auto& contextPath = gInstance.m_mapThreadIdToContextPath[CPLGetPID()];

    Stats* curStats = &m_stats;
    v.push_back(&(curStats->counters));

    bool inFileSystem = false;
    bool inFile = false;
    bool inAction = false;
    for(const auto& item: contextPath )
    {
        if( item.eType == ContextPathType::FILESYSTEM )
        {
            if( inFileSystem )
                continue;
            inFileSystem = true;
        }
        else if( item.eType == ContextPathType::FILE )
        {
            if( inFile )
                continue;
            inFile = true;
        }
        else if( item.eType == ContextPathType::ACTION )
        {
            if( inAction )
                continue;
            inAction = true;
        }

        curStats = &(curStats->children[item]);
        v.push_back(&(curStats->counters));
    }

    return v;
}

void NetworkStatisticsLogger::LogGET(size_t nDownloadedBytes)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    for( auto counters: gInstance.GetCountersForContext() )
    {
        counters->nGET ++;
        counters->nGETDownloadedBytes += nDownloadedBytes;
    }
}

void NetworkStatisticsLogger::LogPUT(size_t nUploadedBytes)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    for( auto counters: gInstance.GetCountersForContext() )
    {
        counters->nPUT ++;
        counters->nPUTUploadedBytes += nUploadedBytes;
    }
}

void NetworkStatisticsLogger::LogHEAD()
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    for( auto counters: gInstance.GetCountersForContext() )
    {
        counters->nHEAD++;
    }
}

void NetworkStatisticsLogger::LogPOST(size_t nUploadedBytes,
                                      size_t nDownloadedBytes)
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    for( auto counters: gInstance.GetCountersForContext() )
    {
        counters->nPOST++;
        counters->nPOSTUploadedBytes += nUploadedBytes;
        counters->nPOSTDownloadedBytes += nDownloadedBytes;
    }
}

void NetworkStatisticsLogger::LogDELETE()
{
    if( !IsEnabled() ) return;
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    for( auto counters: gInstance.GetCountersForContext() )
    {
        counters->nDELETE++;
    }
}

void NetworkStatisticsLogger::Reset()
{
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);
    gInstance.m_stats = Stats();
    gnEnabled = -1;
}

void NetworkStatisticsLogger::Stats::AsJSON(CPLJSONObject& oJSON) const
{
    CPLJSONObject oMethods;
    if( counters.nHEAD )
        oMethods.Add("HEAD/count", counters.nHEAD);
    if( counters.nGET )
        oMethods.Add("GET/count", counters.nGET);
    if( counters.nGETDownloadedBytes )
        oMethods.Add("GET/downloaded_bytes", counters.nGETDownloadedBytes);
    if( counters.nPUT )
        oMethods.Add("PUT/count", counters.nPUT);
    if( counters.nPUTUploadedBytes )
        oMethods.Add("PUT/uploaded_bytes", counters.nPUTUploadedBytes);
    if( counters.nPOST )
        oMethods.Add("POST/count", counters.nPOST);
    if( counters.nPOSTUploadedBytes )
        oMethods.Add("POST/uploaded_bytes", counters.nPOSTUploadedBytes);
    if( counters.nPOSTDownloadedBytes )
        oMethods.Add("POST/downloaded_bytes", counters.nPOSTDownloadedBytes);
    if( counters.nDELETE )
        oMethods.Add("DELETE/count", counters.nDELETE);
    oJSON.Add("methods", oMethods);
    CPLJSONObject oFiles;
    bool bFilesAdded = false;
    for( const auto &kv: children )
    {
        CPLJSONObject childJSON;
        kv.second.AsJSON(childJSON);
        if( kv.first.eType == ContextPathType::FILESYSTEM )
        {
            CPLString osName( kv.first.osName );
            if( !osName.empty() && osName[0] == '/' )
                osName = osName.substr(1);
            if( !osName.empty() && osName.back() == '/' )
                osName.resize(osName.size() - 1);
            oJSON.Add( ("handlers/" + osName).c_str(), childJSON);
        }
        else if( kv.first.eType == ContextPathType::FILE )
        {
            if( !bFilesAdded )
            {
                bFilesAdded = true;
                oJSON.Add("files", oFiles);
            }
            oFiles.AddNoSplitName(kv.first.osName.c_str(), childJSON);
        }
        else if( kv.first.eType == ContextPathType::ACTION )
        {
            oJSON.Add( ("actions/" + kv.first.osName).c_str(), childJSON);
        }
    }
}

CPLString NetworkStatisticsLogger::GetReportAsSerializedJSON()
{
    std::lock_guard<std::mutex> oLock(gInstance.m_mutex);

    CPLJSONObject oJSON;
    gInstance.m_stats.AsJSON(oJSON);
    return oJSON.Format(CPLJSONObject::PrettyFormat::Pretty);
}

/************************************************************************/
/*                     VSICurlParseUnixPermissions()                    */
/************************************************************************/

int VSICurlParseUnixPermissions(const char* pszPermissions)
{
    if( strlen(pszPermissions) != 9 )
        return 0;
    int nMode = 0;
    if( pszPermissions[0] == 'r' )
        nMode |= S_IRUSR;
    if( pszPermissions[1] == 'w' )
        nMode |= S_IWUSR;
    if( pszPermissions[2] == 'x' )
        nMode |= S_IXUSR;
    if( pszPermissions[3] == 'r' )
        nMode |= S_IRGRP;
    if( pszPermissions[4] == 'w' )
        nMode |= S_IWGRP;
    if( pszPermissions[5] == 'x' )
        nMode |= S_IXGRP;
    if( pszPermissions[6] == 'r' )
        nMode |= S_IROTH;
    if( pszPermissions[7] == 'w' )
        nMode |= S_IWOTH;
    if( pszPermissions[8] == 'x' )
        nMode |= S_IXOTH;
    return nMode;
}


} /* end of namespace cpl */

/************************************************************************/
/*                      VSICurlInstallReadCbk()                         */
/************************************************************************/

int VSICurlInstallReadCbk( VSILFILE* fp,
                           VSICurlReadCbkFunc pfnReadCbk,
                           void* pfnUserData,
                           int bStopOnInterruptUntilUninstall )
{
    return reinterpret_cast<cpl::VSICurlHandle *>(fp)->
        InstallReadCbk(pfnReadCbk, pfnUserData, bStopOnInterruptUntilUninstall);
}

/************************************************************************/
/*                    VSICurlUninstallReadCbk()                         */
/************************************************************************/

int VSICurlUninstallReadCbk( VSILFILE* fp )
{
    return reinterpret_cast<cpl::VSICurlHandle *>(fp)->UninstallReadCbk();
}

/************************************************************************/
/*                       VSICurlSetOptions()                            */
/************************************************************************/

struct curl_slist* VSICurlSetOptions(
                        CURL* hCurlHandle, const char* pszURL,
                        const char * const* papszOptions )
{
    struct curl_slist* headers = static_cast<struct curl_slist*>(
        CPLHTTPSetOptions(hCurlHandle, pszURL, papszOptions));

// 7.16
#if LIBCURL_VERSION_NUM >= 0x071000
    long option = CURLFTPMETHOD_SINGLECWD;
    curl_easy_setopt(hCurlHandle, CURLOPT_FTP_FILEMETHOD, option);
#endif

// 7.12.3
#if LIBCURL_VERSION_NUM > 0x070C03
    // ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/
    // doesn't like EPSV command,
    curl_easy_setopt(hCurlHandle, CURLOPT_FTP_USE_EPSV, 0);
#endif

    return headers;
}

/************************************************************************/
/*                     VSICurlMergeHeaders()                            */
/************************************************************************/

struct curl_slist* VSICurlMergeHeaders( struct curl_slist* poDest,
                                        struct curl_slist* poSrcToDestroy )
{
    struct curl_slist* iter = poSrcToDestroy;
    while( iter != nullptr )
    {
        poDest = curl_slist_append(poDest, iter->data);
        iter = iter->next;
    }
    if( poSrcToDestroy )
        curl_slist_free_all(poSrcToDestroy);
    return poDest;
}

/************************************************************************/
/*                    VSICurlSetContentTypeFromExt()                    */
/************************************************************************/

struct curl_slist* VSICurlSetContentTypeFromExt(struct curl_slist* poList,
                                                const char *pszPath)
{
    struct curl_slist* iter = poList;
    while( iter != nullptr )
    {
        if( STARTS_WITH_CI(iter->data, "Content-Type") )
        {
            return poList;
        }
        iter = iter->next;
    }

    static const struct
    {
        const char* ext;
        const char* mime;
    }
    aosExtMimePairs[] =
    {
        {"txt", "text/plain"},
        {"json", "application/json"},
        {"tif", "image/tiff"}, {"tiff", "image/tiff"},
        {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"},
        {"jp2", "image/jp2"}, {"jpx", "image/jp2"}, {"j2k", "image/jp2"}, {"jpc", "image/jp2"},
        {"png", "image/png"},
    };

    const char *pszExt = CPLGetExtension(pszPath);
    if( pszExt )
    {
        for( const auto& pair: aosExtMimePairs )
        {
            if( EQUAL(pszExt, pair.ext) )
            {

                CPLString osContentType;
                osContentType.Printf("Content-Type: %s", pair.mime);
                poList = curl_slist_append(poList, osContentType.c_str());
#ifdef DEBUG_VERBOSE
                CPLDebug("HTTP", "Setting %s, based on lookup table.", osContentType.c_str());
#endif
                break;
            }
        }
    }

    return poList;
}

/************************************************************************/
/*                VSICurlSetCreationHeadersFromOptions()                */
/************************************************************************/

struct curl_slist* VSICurlSetCreationHeadersFromOptions(struct curl_slist* headers,
                                                        CSLConstList papszOptions,
                                                        const char *pszPath)
{
    bool bContentTypeFound = false;
    for( CSLConstList papszIter = papszOptions; papszIter && *papszIter; ++papszIter )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszKey && pszValue )
        {
            if( EQUAL(pszKey, "Content-Type") )
            {
                bContentTypeFound = true;
            }
            CPLString osVal;
            osVal.Printf("%s: %s", pszKey, pszValue);
            headers = curl_slist_append(headers, osVal.c_str());
        }
        CPLFree(pszKey);
    }

    // If Content-type not found in papszOptions, try to set it from the
    // filename exstension.
    if( !bContentTypeFound )
    {
        headers = VSICurlSetContentTypeFromExt(headers, pszPath);
    }

    return headers;
}

#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                   VSIInstallCurlFileHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsicurl/ HTTP/FTP file system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsicurl">/vsicurl/ documentation</a>
 *
 * @since GDAL 1.8.0
 */
void VSIInstallCurlFileHandler( void )
{
    VSIFilesystemHandler* poHandler = new cpl::VSICurlFilesystemHandler;
    VSIFileManager::InstallHandler( "/vsicurl/", poHandler );
    VSIFileManager::InstallHandler( "/vsicurl?", poHandler );
}

/************************************************************************/
/*                         VSICurlClearCache()                          */
/************************************************************************/

/**
 * \brief Clean local cache associated with /vsicurl/ (and related file systems)
 *
 * /vsicurl (and related file systems like /vsis3/, /vsigs/, /vsiaz/, /vsioss/,
 * /vsiswift/) cache a number of
 * metadata and data for faster execution in read-only scenarios. But when the
 * content on the server-side may change during the same process, those
 * mechanisms can prevent opening new files, or give an outdated version of them.
 *
 * @since GDAL 2.2.1
 */

void VSICurlClearCache( void )
{
    // FIXME ? Currently we have different filesystem instances for
    // vsicurl/, /vsis3/, /vsigs/ . So each one has its own cache of regions,
    // file size, etc.
    CSLConstList papszPrefix = VSIFileManager::GetPrefixes();
    for( size_t i = 0; papszPrefix && papszPrefix[i]; ++i )
    {
        auto poFSHandler =
            dynamic_cast<cpl::VSICurlFilesystemHandler*>(
                VSIFileManager::GetHandler( papszPrefix[i] ));

        if( poFSHandler )
            poFSHandler->ClearCache();
    }

    VSICurlStreamingClearCache();
}

/************************************************************************/
/*                      VSICurlPartialClearCache()                      */
/************************************************************************/

/**
 * \brief Clean local cache associated with /vsicurl/ (and related file systems)
 * for a given filename (and its subfiles and subdirectories if it is a
 * directory)
 *
 * /vsicurl (and related file systems like /vsis3/, /vsigs/, /vsiaz/, /vsioss/,
 * /vsiswift/) cache a number of
 * metadata and data for faster execution in read-only scenarios. But when the
 * content on the server-side may change during the same process, those
 * mechanisms can prevent opening new files, or give an outdated version of them.
 *
 * @param pszFilenamePrefix Filename prefix
 * @since GDAL 2.4.0
 */

void VSICurlPartialClearCache(const char* pszFilenamePrefix)
{
     auto poFSHandler =
            dynamic_cast<cpl::VSICurlFilesystemHandler*>(
                VSIFileManager::GetHandler( pszFilenamePrefix ));

    if( poFSHandler )
        poFSHandler->PartialClearCache(pszFilenamePrefix);
}

/************************************************************************/
/*                        VSINetworkStatsReset()                        */
/************************************************************************/

/**
 * \brief Clear network related statistics.
 *
 * The effect of the CPL_VSIL_NETWORK_STATS_ENABLED configuration option
 * will also be reset. That is, that the next network access will check its
 * value again.
 *
 * @since GDAL 3.2.0
 */

void VSINetworkStatsReset( void )
{
    cpl::NetworkStatisticsLogger::Reset();
}

/************************************************************************/
/*                 VSINetworkStatsGetAsSerializedJSON()                 */
/************************************************************************/

/**
 * \brief Return network related statistics, as a JSON serialized object.
 *
 * Statistics collecting should be enabled with the CPL_VSIL_NETWORK_STATS_ENABLED
 * configuration option set to YES before any network activity starts
 * (for efficiency, reading it is cached on first access, until VSINetworkStatsReset() is called)
 *
 * Statistics can also be emitted on standard output at process termination if
 * the CPL_VSIL_SHOW_NETWORK_STATS configuration option is set to YES.
 *
 * Example of output:
 * <pre>
 * {
 *   "methods":{
 *     "GET":{
 *       "count":6,
 *       "downloaded_bytes":40825
 *     },
 *     "PUT":{
 *       "count":1,
 *       "uploaded_bytes":35472
 *     }
 *   },
 *   "handlers":{
 *     "vsigs":{
 *       "methods":{
 *         "GET":{
 *           "count":2,
 *           "downloaded_bytes":446
 *         },
 *         "PUT":{
 *           "count":1,
 *           "uploaded_bytes":35472
 *         }
 *       },
 *       "files":{
 *         "\/vsigs\/spatialys\/byte.tif":{
 *           "methods":{
 *             "PUT":{
 *               "count":1,
 *               "uploaded_bytes":35472
 *             }
 *           },
 *           "actions":{
 *             "Write":{
 *               "methods":{
 *                 "PUT":{
 *                   "count":1,
 *                   "uploaded_bytes":35472
 *                 }
 *               }
 *             }
 *           }
 *         }
 *       },
 *       "actions":{
 *         "Stat":{
 *           "methods":{
 *             "GET":{
 *               "count":2,
 *               "downloaded_bytes":446
 *             }
 *           },
 *           "files":{
 *             "\/vsigs\/spatialys\/byte.tif\/":{
 *               "methods":{
 *                 "GET":{
 *                   "count":1,
 *                   "downloaded_bytes":181
 *                 }
 *               }
 *             }
 *           }
 *         }
 *       }
 *     },
 *     "vsis3":{
 *          [...]
 *     } 
 *   }
 * }

 * </pre>
 *
 * @param papszOptions Unused.
 * @return a JSON serialized string to free with VSIFree(), or nullptr
 * @since GDAL 3.2.0
 */

char* VSINetworkStatsGetAsSerializedJSON( CPL_UNUSED char** papszOptions )
{
    return CPLStrdup(cpl::NetworkStatisticsLogger::GetReportAsSerializedJSON());
}

#endif /* HAVE_CURL */
