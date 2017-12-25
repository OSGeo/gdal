/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HTTP/FTP files
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2010-2015, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <algorithm>
#include <set>
#include <map>

#include "cpl_aws.h"
#include "cpl_google_cloud.h"
#include "cpl_azure.h"
#include "cpl_alibaba_oss.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_http.h"

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallCurlFileHandler( void )
{
    // Not supported.
}

void VSIInstallS3FileHandler( void )
{
    // Not supported.
}

void VSIInstallGSFileHandler( void )
{
    // Not supported.
}

void VSIInstallAzureFileHandler( void )
{
    // Not supported
}

void VSIInstallOSSFileHandler( void )
{
    // Not supported
}

void VSICurlClearCache( void )
{
    // Not supported.
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

#include <curl/curl.h>

void VSICurlStreamingClearCache( void ); // from cpl_vsil_curl_streaming.cpp

struct curl_slist* VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL,
                       const char * const* papszOptions);
struct curl_slist* VSICurlMergeHeaders( struct curl_slist* poDest,
                                        struct curl_slist* poSrcToDestroy );

#include <map>

#define ENABLE_DEBUG 1

static int N_MAX_REGIONS = 1000;
static int DOWNLOAD_CHUNK_SIZE = 16384;

const char GDAL_MARKER_FOR_DIR[] = ".gdal_marker_for_dir";

namespace {

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

class CachedFileProp
{
  public:
    ExistStatus     eExists;
    bool            bHasComputedFileSize;
    vsi_l_offset    fileSize;
    bool            bIsDirectory;
    time_t          mTime;
    bool            bS3LikeRedirect;
    time_t          nExpireTimestampLocal;
    CPLString       osRedirectURL;

                    CachedFileProp() :
                        eExists(EXIST_UNKNOWN),
                        bHasComputedFileSize(false),
                        fileSize(0),
                        bIsDirectory(false),
                        mTime(0),
                        bS3LikeRedirect(false),
                        nExpireTimestampLocal(0)
                        {}
};

typedef struct
{
    bool            bGotFileList;
    char**          papszFileList; /* only file name without path */
} CachedDirList;

typedef struct
{
    unsigned long   pszURLHash;
    vsi_l_offset    nFileOffsetStart;
    size_t          nSize;
    char           *pData;
} CachedRegion;

typedef struct
{
    char*           pBuffer;
    size_t          nSize;
    bool            bIsHTTP;
    bool            bIsInHeader;
    bool            bMultiRange;
    vsi_l_offset    nStartOffset;
    vsi_l_offset    nEndOffset;
    int             nHTTPCode;
    vsi_l_offset    nContentLength;
    bool            bFoundContentRange;
    bool            bError;
    bool            bDownloadHeaderOnly;
    bool            bDetectRangeDownloadingError;
    GIntBig         nTimestampDate; // Corresponds to Date: header field

    VSILFILE           *fp;
    VSICurlReadCbkFunc  pfnReadCbk;
    void               *pReadCbkUserData;
    bool                bInterrupted;
} WriteFuncStruct;

static const char* VSICurlGetCacheFileName()
{
    return "gdal_vsicurl_cache.bin";
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
/*                     VSICurlFilesystemHandler                         */
/************************************************************************/

typedef struct
{
    CURLM          *hCurlMultiHandle;
} CachedConnection;

class VSICurlHandle;

class VSICurlFilesystemHandler : public VSIFilesystemHandler
{
    CachedRegion  **papsRegions;
    int             nRegions;

    std::map<CPLString, CachedFileProp*>   cacheFileSize;
    std::map<CPLString, CachedDirList*>        cacheDirList;

    bool            bUseCacheDisk;

    // Per-thread Curl connection cache.
    std::map<GIntBig, CachedConnection*> mapConnections;

    char**              ParseHTMLFileList(const char* pszFilename,
                                          int nMaxFiles,
                                          char* pszData,
                                          bool* pbGotFileList);

protected:
    CPLMutex       *hMutex;

    virtual VSICurlHandle* CreateFileHandle(const char* pszFilename);
    virtual char** GetFileList(const char *pszFilename,
                               int nMaxFiles,
                               bool* pbGotFileList);
    virtual CPLString GetURLFromDirname( const CPLString& osDirname );

    void AnalyseS3FileList( const CPLString& osBaseURL,
                            const char* pszXML,
                            CPLStringList& osFileList,
                            int nMaxFiles,
                            bool& bIsTruncated,
                            CPLString& osNextMarker );

    void AnalyseAzureFileList( const CPLString& osBaseURL,
                               bool bCacheResults,
                            const char* pszXML,
                            CPLStringList& osFileList,
                            int nMaxFiles,
                            bool& bIsTruncated,
                            CPLString& osNextMarker );

public:
    VSICurlFilesystemHandler();
    ~VSICurlFilesystemHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
    int Unlink( const char *pszFilename ) override;
    int Rename( const char *oldpath, const char *newpath ) override;
    int Mkdir( const char *pszDirname, long nMode ) override;
    int Rmdir( const char *pszDirname ) override;
    char **ReadDir( const char *pszDirname ) override
        { return ReadDirEx(pszDirname, 0); }
    char **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;

    int HasOptimizedReadMultiRange( const char* /* pszPath */ )
        override { return true; }

    char **ReadDirInternal( const char *pszDirname, int nMaxFiles,
                            bool* pbGotFileList );
    void InvalidateDirContent( const char *pszDirname );

    virtual CPLString GetFSPrefix() { return "/vsicurl/"; }
    virtual bool      AllowCachedDataFor(const char* pszFilename);

    const CachedRegion* GetRegion( const char* pszURL,
                                   vsi_l_offset nFileOffsetStart );

    void                AddRegion( const char* pszURL,
                                   vsi_l_offset nFileOffsetStart,
                                   size_t nSize,
                                   const char *pData );

    CachedFileProp*     GetCachedFileProp( const char* pszURL );
    void                InvalidateCachedData( const char* pszURL );

    void                AddRegionToCacheDisk( CachedRegion* psRegion );
    const CachedRegion* GetRegionFromCacheDisk( const char* pszURL,
                                                vsi_l_offset nFileOffsetStart );

    CURLM              *GetCurlMultiHandleFor( const CPLString& osURL );

    virtual void        ClearCache();

    bool ExistsInCacheDirList( const CPLString& osDirname, bool *pbIsDir )
    {
        CPLMutexHolder oHolder( &hMutex );
        std::map<CPLString, CachedDirList*>::const_iterator oIter =
            cacheDirList.find(osDirname);
        if( pbIsDir )
        {
            *pbIsDir = oIter != cacheDirList.end() &&
                       oIter->second->papszFileList != nullptr;
        }
        return oIter != cacheDirList.end();
    }

};

/************************************************************************/
/*                           VSICurlHandle                              */
/************************************************************************/

class VSICurlHandle : public VSIVirtualHandle
{

  protected:
    VSICurlFilesystemHandler* poFS;

    bool            m_bCached;

    vsi_l_offset    fileSize;
    bool            bHasComputedFileSize;
    ExistStatus     eExists;
    bool            bIsDirectory;
    CPLString       m_osFilename; // e.g "/vsicurl/http://example.com/foo"
    char*           m_pszURL;     // e.g "http://example.com/foo"

    char          **m_papszHTTPOptions;

  private:

    vsi_l_offset    curOffset;
    time_t          mTime;

    vsi_l_offset    lastDownloadedOffset;
    int             nBlocksToDownload;
    bool            bEOF;

    bool            DownloadRegion(vsi_l_offset startOffset, int nBlocks);

    VSICurlReadCbkFunc  pfnReadCbk;
    void               *pReadCbkUserData;
    bool                bStopOnInterruptUntilUninstall;
    bool                bInterrupted;

    bool                m_bS3LikeRedirect;
    time_t              m_nExpireTimestampLocal;
    CPLString           m_osRedirectURL;

    int                 m_nMaxRetry;
    double              m_dfRetryDelay;
    bool                m_bUseHead;

    int          ReadMultiRangeSingleGet( int nRanges, void ** ppData,
                                         const vsi_l_offset* panOffsets,
                                         const size_t* panSizes );
    CPLString    GetRedirectURLIfValid(CachedFileProp* cachedFileProp,
                                               bool& bHasExpired);

  protected:
    virtual struct curl_slist* GetCurlHeaders( const CPLString& /*osVerb*/,
                                const struct curl_slist* /* psExistingHeaders */)
        { return nullptr; }
    virtual bool AllowAutomaticRedirection() { return true; }
    virtual bool CanRestartOnError( const char*, const char*, bool ) { return false; }
    virtual bool UseLimitRangeGetInsteadOfHead() { return false; }
    virtual bool IsDirectoryFromExists( const char* /*pszVerb*/, int /*response_code*/ ) { return false; }
    virtual void ProcessGetFileSizeResult(const char* /* pszContent */ ) {}
    void SetURL(const char* pszURL);

  public:

    VSICurlHandle( VSICurlFilesystemHandler* poFS,
                   const char* pszFilename,
                   const char* pszURLIn = nullptr );
    ~VSICurlHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    int ReadMultiRange( int nRanges, void ** ppData,
                        const vsi_l_offset* panOffsets,
                        const size_t* panSizes ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    bool IsKnownFileSize() const { return bHasComputedFileSize; }
    vsi_l_offset         GetFileSize() { return GetFileSize(false); }
    vsi_l_offset         GetFileSize( bool bSetError );
    bool                 Exists( bool bSetError );
    bool                 IsDirectory() const { return bIsDirectory; }
    time_t               GetMTime() const { return mTime; }

    int                  InstallReadCbk( VSICurlReadCbkFunc pfnReadCbk,
                                         void* pfnUserData,
                                         int bStopOnInterruptUntilUninstall );
    int                  UninstallReadCbk();
};


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
                    /* This more or less emulates the behaviour of
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
    m_bCached(true),
    curOffset(0),
    lastDownloadedOffset(VSI_L_OFFSET_MAX),
    nBlocksToDownload(1),
    bEOF(false),
    pfnReadCbk(nullptr),
    pReadCbkUserData(nullptr),
    bStopOnInterruptUntilUninstall(false),
    bInterrupted(false),
    m_bS3LikeRedirect(false),
    m_nExpireTimestampLocal(0),
    m_nMaxRetry(atoi(CPLGetConfigOption("GDAL_HTTP_MAX_RETRY",
                                   CPLSPrintf("%d",CPL_HTTP_MAX_RETRY)))),
    m_dfRetryDelay(CPLAtof(CPLGetConfigOption("GDAL_HTTP_RETRY_DELAY",
                                CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY)))),
    m_bUseHead(CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_HEAD",
                                             "YES")))
{
    m_osFilename = pszFilename;
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
    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(m_pszURL);
    eExists = cachedFileProp->eExists;
    fileSize = cachedFileProp->fileSize;
    bHasComputedFileSize = cachedFileProp->bHasComputedFileSize;
    bIsDirectory = cachedFileProp->bIsDirectory;
    mTime = cachedFileProp->mTime;
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
        curOffset = GetFileSize() + nOffset;
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

static void VSICURLInitWriteFuncStruct( WriteFuncStruct   *psStruct,
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
}

/************************************************************************/
/*                       VSICurlHandleWriteFunc()                       */
/************************************************************************/

static size_t VSICurlHandleWriteFunc( void *buffer, size_t count,
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
                const char* pszSpace = strchr(
                    const_cast<const char*>(pszLine), ' ');
                if( pszSpace )
                    psStruct->nHTTPCode = atoi(pszSpace + 1);
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
         strstr(pszURL, ".storage.googleapis.com/") != nullptr) &&
        (strstr(pszURL, "&Signature=") != nullptr ||
         strstr(pszURL, "?Signature=") != nullptr);
}

/************************************************************************/
/*                  VSICurlGetExpiresFromS3LikeSignedURL()              */
/************************************************************************/

static GIntBig VSICurlGetExpiresFromS3LikeSignedURL( const char* pszURL )
{
    const char* pszExpires = strstr(pszURL, "&Expires=");
    if( pszExpires == nullptr )
        pszExpires = strstr(pszURL, "?Expires=");
    if( pszExpires == nullptr )
        return 0;
    return CPLAtoGIntBig(pszExpires + strlen("&Expires="));
}

/************************************************************************/
/*                           MultiPerform()                             */
/************************************************************************/

static void MultiPerform(CURLM* hCurlMultiHandle,
                         CURL* hEasyHandle = nullptr)
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
/*                           GetFileSize()                              */
/************************************************************************/

vsi_l_offset VSICurlHandle::GetFileSize( bool bSetError )
{
    if( bHasComputedFileSize )
        return fileSize;

    bHasComputedFileSize = true;

    CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(m_pszURL);

    CPLString osURL(m_pszURL);
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
    if( UseLimitRangeGetInsteadOfHead() )
    {
        osVerb = "GET";
        const int nBufSize = std::max(1024, std::min(10 * 1024 * 1024,
            atoi(CPLGetConfigOption("GDAL_INGESTED_BYTES_AT_OPEN", "1024"))));
        nRoundedBufSize = ((nBufSize + DOWNLOAD_CHUNK_SIZE - 1)
            / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;

        // so it gets included in Azure signature
        osRange.Printf("Range: bytes=0-%d", nRoundedBufSize-1);
        headers = curl_slist_append(headers, osRange.c_str());
        sWriteFuncHeaderData.bDetectRangeDownloadingError = false;
    }
    // HACK for mbtiles driver: http://a.tiles.mapbox.com/v3/ doesn't accept
    // HEAD, as it is a redirect to AWS S3 signed URL, but those are only valid
    // for a given type of HTTP request, and thus GET. This is valid for any
    // signed URL for AWS S3.
    else if( strstr(osURL, ".tiles.mapbox.com/") != nullptr ||
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

    if( headers != nullptr )
        curl_slist_free_all(headers);

    eExists = EXIST_UNKNOWN;

    long mtime = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_FILETIME, &mtime);

    if( STARTS_WITH(osURL, "ftp") )
    {
        if( sWriteFuncData.pBuffer != nullptr )
        {
            const char* pszContentLength = strstr(
                const_cast<const char*>(sWriteFuncData.pBuffer), "Content-Length: ");
            if( pszContentLength )
            {
                pszContentLength += strlen("Content-Length: ");
                eExists = EXIST_YES;
                fileSize = CPLScanUIntBig(
                    pszContentLength,
                    static_cast<int>(strlen(pszContentLength)));
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                            osURL.c_str(), fileSize);
            }
        }
    }

    double dfSize = 0;
    if( eExists != EXIST_YES )
    {
        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

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
            CPLDebug("VSICURL", "Effective URL: %s", osEffectiveURL.c_str());

            // Is this is a redirect to a S3 URL?
            if( VSICurlIsS3LikeSignedURL(osEffectiveURL) &&
                !VSICurlIsS3LikeSignedURL(osURL) )
            {
                // Note that this is a redirect as we won't notice after the
                // retry.
                bS3LikeRedirect = true;

                if( !bRetryWithGet && osVerb == "HEAD" && response_code == 403 )
                {
                    CPLDebug("VSICURL",
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
                CPLDebug("VSICURL",
                         "Will use redirect URL for the next %d seconds",
                         nValidity);
                // As our local clock might not be in sync with server clock,
                // figure out the expiration timestamp in local time
                m_bS3LikeRedirect = true;
                m_nExpireTimestampLocal = time(nullptr) + nValidity;
                m_osRedirectURL = osEffectiveURL;
                CachedFileProp* cachedFileProp =
                    poFS->GetCachedFileProp(m_pszURL);
                cachedFileProp->bS3LikeRedirect = m_bS3LikeRedirect;
                cachedFileProp->nExpireTimestampLocal = m_nExpireTimestampLocal;
                cachedFileProp->osRedirectURL = m_osRedirectURL;
            }
        }

        const CURLcode code =
            curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                              &dfSize );
        if( code == 0 )
        {
            eExists = EXIST_YES;
            if( dfSize < 0 )
                fileSize = 0;
            else
                fileSize = static_cast<GUIntBig>(dfSize);
        }

        if( UseLimitRangeGetInsteadOfHead() && response_code == 206 )
        {
            eExists = EXIST_NO;
            fileSize = 0;
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
                    eExists = EXIST_YES;
                    fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(pszContentRange + 1));
                }

                // Add first bytes to cache
                if( sWriteFuncData.pBuffer != nullptr )
                {
                    for( size_t nOffset = 0;
                         nOffset + DOWNLOAD_CHUNK_SIZE <= sWriteFuncData.nSize;
                         nOffset += DOWNLOAD_CHUNK_SIZE )
                    {
                        poFS->AddRegion(m_pszURL,
                                        nOffset,
                                        DOWNLOAD_CHUNK_SIZE,
                                        sWriteFuncData.pBuffer + nOffset);
                    }
                }
            }
        }
        else if ( IsDirectoryFromExists(osVerb,
                                        static_cast<int>(response_code)) )
        {
            eExists = EXIST_YES;
            fileSize = 0;
            bIsDirectory = true;
        }
        else if( response_code == 416 )
        {
            eExists = EXIST_YES;
            fileSize = 0;
        }
        else if( response_code != 200 )
        {
            // If HTTP 429, 502, 503 or 504 gateway timeout error retry after a
            // pause.
            const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
                static_cast<int>(response_code), dfRetryDelay);
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
                bHasComputedFileSize = false;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                curl_easy_cleanup(hCurlHandle);
                return GetFileSize(bSetError);
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

            eExists = EXIST_NO;
            fileSize = 0;
        }
        else if( sWriteFuncData.pBuffer != nullptr )
        {
            ProcessGetFileSizeResult( (const char*)sWriteFuncData.pBuffer );
        }

        // Try to guess if this is a directory. Generally if this is a
        // directory, curl will retry with an URL with slash added.
        if( !osEffectiveURL.empty() &&
            strncmp(osURL, osEffectiveURL, osURL.size()) == 0 &&
            osEffectiveURL[osURL.size()] == '/' )
        {
            eExists = EXIST_YES;
            fileSize = 0;
            bIsDirectory = true;
        }
        else if( osURL.back() == '/' )
        {
            bIsDirectory = true;
        }

        if( ENABLE_DEBUG )
            CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB
                     "  response_code=%d",
                     osURL.c_str(), fileSize,
                     static_cast<int>(response_code));
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
    curl_easy_cleanup(hCurlHandle);

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(m_pszURL);
    cachedFileProp->bHasComputedFileSize = true;
    cachedFileProp->fileSize = fileSize;
    cachedFileProp->eExists = eExists;
    cachedFileProp->bIsDirectory = bIsDirectory;
    if( mtime != 0 )
        cachedFileProp->mTime = mtime;

    return fileSize;
}

/************************************************************************/
/*                                 Exists()                             */
/************************************************************************/

bool VSICurlHandle::Exists( bool bSetError )
{
    if( eExists == EXIST_UNKNOWN )
    {
        GetFileSize(bSetError);
    }
    return eExists == EXIST_YES;
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

CPLString VSICurlHandle::GetRedirectURLIfValid(CachedFileProp* cachedFileProp,
                                               bool& bHasExpired)
{
    bHasExpired = false;
    if( cachedFileProp->bS3LikeRedirect )
    {
        m_bS3LikeRedirect = cachedFileProp->bS3LikeRedirect;
        m_nExpireTimestampLocal = cachedFileProp->nExpireTimestampLocal;
        m_osRedirectURL = cachedFileProp->osRedirectURL;
    }

    CPLString osURL(m_pszURL);
    if( m_bS3LikeRedirect )
    {
        if( time(nullptr) + 1 < m_nExpireTimestampLocal )
        {
            CPLDebug("VSICURL",
                     "Using redirect URL as it looks to be still valid "
                     "(%d seconds left)",
                     static_cast<int>(m_nExpireTimestampLocal - time(nullptr)));
            osURL = m_osRedirectURL;
        }
        else
        {
            CPLDebug("VSICURL", "Redirect URL has expired. Using original URL");
            m_bS3LikeRedirect = false;
            cachedFileProp->bS3LikeRedirect = false;
            bHasExpired = true;
        }
    }
    return osURL;
}

/************************************************************************/
/*                          DownloadRegion()                            */
/************************************************************************/

bool VSICurlHandle::DownloadRegion( const vsi_l_offset startOffset,
                                    const int nBlocks )
{
    if( bInterrupted && bStopOnInterruptUntilUninstall )
        return false;

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(m_pszURL);
    if( cachedFileProp->eExists == EXIST_NO )
        return false;

    CURLM* hCurlMultiHandle = poFS->GetCurlMultiHandleFor(m_pszURL);

    bool bHasExpired = false;
    CPLString osURL(GetRedirectURLIfValid(cachedFileProp, bHasExpired));
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
        startOffset + nBlocks * DOWNLOAD_CHUNK_SIZE - 1;
    // Some servers don't like we try to read after end-of-file (#5786).
    if( cachedFileProp->bHasComputedFileSize &&
        sWriteFuncHeaderData.nEndOffset >= cachedFileProp->fileSize )
    {
        sWriteFuncHeaderData.nEndOffset = cachedFileProp->fileSize - 1;
    }

    char rangeStr[512] = {};
    snprintf(rangeStr, sizeof(rangeStr),
             CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, startOffset,
            sWriteFuncHeaderData.nEndOffset);

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Downloading %s (%s)...", rangeStr, osURL.c_str());

    CPLString osHeaderRange; // leave in this scope
    if( sWriteFuncHeaderData.bIsHTTP )
    {
        osHeaderRange.Printf("Range: bytes=%s", rangeStr);
        // So it gets included in Azure signature
        headers = curl_slist_append(headers, osHeaderRange.c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);
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

    if( headers != nullptr )
        curl_slist_free_all(headers);

    if( sWriteFuncData.bInterrupted )
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);

        return false;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = nullptr;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

    long mtime = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_FILETIME, &mtime);
    if( mtime != 0 )
        cachedFileProp->mTime = mtime;

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Got response_code=%ld", response_code);

    if( response_code == 403 && bUsedRedirect )
    {
        CPLDebug("VSICURL",
                 "Got an error with redirect URL. Retrying with original one");
        m_bS3LikeRedirect = false;
        cachedFileProp->bS3LikeRedirect = false;
        bUsedRedirect = false;
        osURL = m_pszURL;
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        goto retry;
    }

    CPLString osEffectiveURL;
    {
        char *pszEffectiveURL = nullptr;
        curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL, &pszEffectiveURL);
        if( pszEffectiveURL )
            osEffectiveURL = pszEffectiveURL;
    }

    if( !m_bS3LikeRedirect && !osEffectiveURL.empty() &&
        strstr(osEffectiveURL, m_pszURL) == nullptr )
    {
        CPLDebug("VSICURL", "Effective URL: %s", osEffectiveURL.c_str());
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
                CPLDebug("VSICURL",
                         "Will use redirect URL for the next %d seconds",
                         nValidity);
                // As our local clock might not be in sync with server clock,
                // figure out the expiration timestamp in local time.
                m_bS3LikeRedirect = true;
                m_nExpireTimestampLocal = time(nullptr) + nValidity;
                m_osRedirectURL = osEffectiveURL;
                cachedFileProp->bS3LikeRedirect = m_bS3LikeRedirect;
                cachedFileProp->nExpireTimestampLocal = m_nExpireTimestampLocal;
                cachedFileProp->osRedirectURL = m_osRedirectURL;
            }
        }
    }

    if( (response_code != 200 && response_code != 206 &&
         response_code != 225 && response_code != 226 &&
         response_code != 426) ||
        sWriteFuncHeaderData.bError )
    {
        if( sWriteFuncData.pBuffer != nullptr &&
            CanRestartOnError((const char*)sWriteFuncData.pBuffer,
                              (const char*)sWriteFuncHeaderData.pBuffer, false) )
        {
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            curl_easy_cleanup(hCurlHandle);
            return DownloadRegion(startOffset, nBlocks);
        }

        // If HTTP 429, 502, 503 or 504 gateway timeout error retry after a
        // pause.
        const double dfNewRetryDelay = CPLHTTPGetNewRetryDelay(
            static_cast<int>(response_code), dfRetryDelay);
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
        if( !bHasComputedFileSize && startOffset == 0 )
        {
            cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize = 0;
            cachedFileProp->eExists = eExists = EXIST_NO;
        }
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        curl_easy_cleanup(hCurlHandle);
        return false;
    }

    if( !bHasComputedFileSize && sWriteFuncHeaderData.pBuffer )
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
                    fileSize =
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

                    fileSize =
                        CPLScanUIntBig(pszSize,
                                       static_cast<int>(strlen(pszSize)));
                }
            }
        }

        if( fileSize != 0 )
        {
            eExists = EXIST_YES;

            if( ENABLE_DEBUG )
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB
                         "  response_code=%d",
                         m_pszURL, fileSize, static_cast<int>(response_code));

            bHasComputedFileSize = cachedFileProp->bHasComputedFileSize = true;
            cachedFileProp->fileSize = fileSize;
            cachedFileProp->eExists = eExists;
        }
    }

    lastDownloadedOffset = startOffset + nBlocks * DOWNLOAD_CHUNK_SIZE;

    char* pBuffer = sWriteFuncData.pBuffer;
    size_t nSize = sWriteFuncData.nSize;

    if( nSize > static_cast<size_t>(nBlocks) * DOWNLOAD_CHUNK_SIZE )
    {
        if( ENABLE_DEBUG )
            CPLDebug(
                "VSICURL", "Got more data than expected : %u instead of %u",
                static_cast<unsigned int>(nSize),
                static_cast<unsigned int>(nBlocks * DOWNLOAD_CHUNK_SIZE));
    }

    vsi_l_offset l_startOffset = startOffset;
    while( nSize > 0 )
    {
#if DEBUG_VERBOSE
        if( ENABLE_DEBUG )
            CPLDebug(
                "VSICURL",
                "Add region %u - %u",
                static_cast<unsigned int>(startOffset),
                static_cast<unsigned int>(
                    std::min(static_cast<size_t>(DOWNLOAD_CHUNK_SIZE), nSize)));
#endif
        const size_t nChunkSize =
            std::min(static_cast<size_t>(DOWNLOAD_CHUNK_SIZE), nSize);
        poFS->AddRegion(m_pszURL, l_startOffset, nChunkSize, pBuffer);
        l_startOffset += nChunkSize;
        pBuffer += nChunkSize;
        nSize -= nChunkSize;
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);
    curl_easy_cleanup(hCurlHandle);

    return true;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICurlHandle::Read( void * const pBufferIn, size_t const nSize,
                            size_t const  nMemb )
{
    size_t nBufferRequestSize = nSize * nMemb;
    if( nBufferRequestSize == 0 )
        return 0;

    void* pBuffer = pBufferIn;

#if DEBUG_VERBOSE
    CPLDebug("VSICURL", "offset=%d, size=%d",
             static_cast<int>(curOffset), static_cast<int>(nBufferRequestSize));
#endif

    vsi_l_offset iterOffset = curOffset;
    while( nBufferRequestSize )
    {
        // Don't try to read after end of file.
        CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(m_pszURL);
        if( cachedFileProp->bHasComputedFileSize &&
            iterOffset >= cachedFileProp->fileSize )
        {
            if( iterOffset == curOffset )
            {
                CPLDebug("VSICURL", "Request at offset " CPL_FRMT_GUIB
                         ", after end of file", iterOffset);
            }
            break;
        }

        const CachedRegion* psRegion = poFS->GetRegion(m_pszURL, iterOffset);
        if( psRegion == nullptr )
        {
            const vsi_l_offset nOffsetToDownload =
                (iterOffset / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;

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
                ((iterOffset + nBufferRequestSize) / DOWNLOAD_CHUNK_SIZE) *
                DOWNLOAD_CHUNK_SIZE;
            const int nMinBlocksToDownload =
                1 +
                static_cast<int>(
                    (nEndOffsetToDownload - nOffsetToDownload) /
                    DOWNLOAD_CHUNK_SIZE);
            if( nBlocksToDownload < nMinBlocksToDownload )
                nBlocksToDownload = nMinBlocksToDownload;

            // Avoid reading already cached data.
            for( int i = 1; i < nBlocksToDownload; i++ )
            {
                if( poFS->GetRegion(
                        m_pszURL,
                        nOffsetToDownload + i * DOWNLOAD_CHUNK_SIZE) != nullptr )
                {
                    nBlocksToDownload = i;
                    break;
                }
            }

            if( nBlocksToDownload > N_MAX_REGIONS )
                nBlocksToDownload = N_MAX_REGIONS;

            if( DownloadRegion(nOffsetToDownload, nBlocksToDownload) == false )
            {
                if( !bInterrupted )
                    bEOF = true;
                return 0;
            }
            psRegion = poFS->GetRegion(m_pszURL, iterOffset);
        }
        if( psRegion == nullptr || psRegion->pData == nullptr )
        {
            bEOF = true;
            return 0;
        }
        const int nToCopy = static_cast<int>(
            std::min(static_cast<vsi_l_offset>(nBufferRequestSize),
                     psRegion->nSize -
                     (iterOffset - psRegion->nFileOffsetStart)));
        memcpy(pBuffer,
               psRegion->pData + iterOffset - psRegion->nFileOffsetStart,
               nToCopy);
        pBuffer = static_cast<char *>(pBuffer) + nToCopy;
        iterOffset += nToCopy;
        nBufferRequestSize -= nToCopy;
        if( psRegion->nSize != static_cast<size_t>(DOWNLOAD_CHUNK_SIZE) &&
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

    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(m_pszURL);
    if( cachedFileProp->eExists == EXIST_NO )
        return -1;

    const char* pszMultiRangeStrategy =
        CPLGetConfigOption("GDAL_HTTP_MULTIRANGE", "");
    if( EQUAL(pszMultiRangeStrategy, "SINGLE_GET") )
    {
        // Just in case someone needs it, but the interest of this mode is rather
        // dubious now. We could probably remove it
        return ReadMultiRangeSingleGet(nRanges, ppData, panOffsets, panSizes);
    }
    else if( EQUAL(pszMultiRangeStrategy, "SERIAL") )
    {
        return VSIVirtualHandle::ReadMultiRange(
                                    nRanges, ppData, panOffsets, panSizes);
    }

    bool bHasExpired = false;
    CPLString osURL(GetRedirectURLIfValid(cachedFileProp, bHasExpired));
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
    std::vector<WriteFuncStruct> asWriteFuncData;
    std::vector<WriteFuncStruct> asWriteFuncHeaderData;
    std::vector<char*> apszRanges;
    std::vector<struct curl_slist*> aHeaders;

    asWriteFuncData.resize(nRanges);
    asWriteFuncHeaderData.resize(nRanges);

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
            CPLDebug("VSICURL", "Downloading %s (%s)...", rangeStr, osURL.c_str());

        if( asWriteFuncHeaderData[iRequest].bIsHTTP )
        {
            CPLString osHeaderRange;
            osHeaderRange.Printf("Range: bytes=%s", rangeStr);
            // So it gets included in Azure signature
            char* pszRange = CPLStrdup(osHeaderRange);
            apszRanges.push_back(pszRange);
            headers = curl_slist_append(headers, pszRange);
            curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);
        }
        else
        {
            apszRanges.push_back(nullptr);
            curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);
        }

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
        curl_easy_cleanup(aHandles[iReq]);
        CPLFree(apszRanges[iReq]);
        CPLFree(asWriteFuncData[iReq].pBuffer);
        CPLFree(asWriteFuncHeaderData[iReq].pBuffer);
        curl_slist_free_all(aHeaders[iReq]);
    }

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Download completed");

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
            CPLDebug("VSICURL", "Downloading %s (%s)...",
                     osRanges.c_str(), m_pszURL);
        else
            CPLDebug("VSICURL", "Downloading %s, ..., %s (" CPL_FRMT_GUIB
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

    if( headers != nullptr )
        curl_slist_free_all(headers);

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

    char *content_type = nullptr;
    curl_easy_getinfo(hCurlHandle, CURLINFO_CONTENT_TYPE, &content_type);

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

VSICurlFilesystemHandler::VSICurlFilesystemHandler()
{
    hMutex = nullptr;
    papsRegions = nullptr;
    nRegions = 0;
    bUseCacheDisk =
        CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_CACHE", "NO"));
}

/************************************************************************/
/*                  ~VSICurlFilesystemHandler()                         */
/************************************************************************/

VSICurlFilesystemHandler::~VSICurlFilesystemHandler()
{
    ClearCache();

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
    CPLMutexHolder oHolder( &hMutex );

    std::map<GIntBig, CachedConnection*>::const_iterator iterConnections =
        mapConnections.find(CPLGetPID());
    if( iterConnections == mapConnections.end() )
    {
        CURLM* hCurlMultiHandle = curl_multi_init();
        CachedConnection* psCachedConnection = new CachedConnection;
        psCachedConnection->hCurlMultiHandle = hCurlMultiHandle;
        mapConnections[CPLGetPID()] = psCachedConnection;
        return hCurlMultiHandle;
    }

    return iterConnections->second->hCurlMultiHandle;
}

/************************************************************************/
/*                   GetRegionFromCacheDisk()                           */
/************************************************************************/

const CachedRegion*
VSICurlFilesystemHandler::GetRegionFromCacheDisk(const char* pszURL,
                                                 vsi_l_offset nFileOffsetStart)
{
    nFileOffsetStart =
        (nFileOffsetStart / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;
    VSILFILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "rb");
    if( fp )
    {
        const unsigned long pszURLHash = CPLHashSetHashStr(pszURL);
        while( true )
        {
            unsigned long pszURLHashCached = 0;
            if( VSIFReadL(&pszURLHashCached, sizeof(unsigned long),
                          1, fp) == 0 )
                break;
            vsi_l_offset nFileOffsetStartCached = 0;
            if( VSIFReadL(&nFileOffsetStartCached, sizeof(vsi_l_offset),
                          1, fp) == 0)
                break;
            size_t nSizeCached = 0;
            if( VSIFReadL(&nSizeCached, sizeof(size_t), 1, fp) == 0)
                break;
            if( pszURLHash == pszURLHashCached &&
                nFileOffsetStart == nFileOffsetStartCached )
            {
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL", "Got data at offset "
                             CPL_FRMT_GUIB " from disk", nFileOffsetStart);
                if( nSizeCached )
                {
                    char* pBuffer = static_cast<char *>(CPLMalloc(nSizeCached));
                    if( VSIFReadL(pBuffer, 1, nSizeCached, fp) != nSizeCached )
                    {
                        CPLFree(pBuffer);
                        break;
                    }
                    AddRegion(pszURL, nFileOffsetStart, nSizeCached, pBuffer);
                    CPLFree(pBuffer);
                }
                else
                {
                    AddRegion(pszURL, nFileOffsetStart, 0, nullptr);
                }
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return GetRegion(pszURL, nFileOffsetStart);
            }
            else
            {
                if( VSIFSeekL(fp, nSizeCached, SEEK_CUR) != 0 )
                    break;
            }
        }
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    }
    return nullptr;
}

/************************************************************************/
/*                  AddRegionToCacheDisk()                                */
/************************************************************************/

void VSICurlFilesystemHandler::AddRegionToCacheDisk(CachedRegion* psRegion)
{
    VSILFILE* fp = VSIFOpenL(VSICurlGetCacheFileName(), "r+b");
    if( fp )
    {
        while( true )
        {
            unsigned long pszURLHashCached = 0;
            if( VSIFReadL(&pszURLHashCached, 1, sizeof(unsigned long),
                          fp) == 0 )
                break;
            vsi_l_offset nFileOffsetStartCached = 0;
            if( VSIFReadL(&nFileOffsetStartCached, sizeof(vsi_l_offset), 1, fp)
                == 0 )
                break;
            size_t nSizeCached = 0;
            if( VSIFReadL(&nSizeCached, sizeof(size_t), 1, fp) == 0 )
                break;
            if( psRegion->pszURLHash == pszURLHashCached &&
                psRegion->nFileOffsetStart == nFileOffsetStartCached )
            {
                CPLAssert(psRegion->nSize == nSizeCached);
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return;
            }
            else
            {
                if( VSIFSeekL(fp, nSizeCached, SEEK_CUR) != 0 )
                    break;
            }
        }
    }
    else
    {
        fp = VSIFOpenL(VSICurlGetCacheFileName(), "wb");
    }
    if( fp )
    {
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL",
                     "Write data at offset " CPL_FRMT_GUIB " to disk",
                     psRegion->nFileOffsetStart);
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->pszURLHash, 1,
                                      sizeof(unsigned long), fp));
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->nFileOffsetStart, 1,
                                      sizeof(vsi_l_offset), fp));
        CPL_IGNORE_RET_VAL(VSIFWriteL(&psRegion->nSize, 1, sizeof(size_t), fp));
        if( psRegion->nSize )
            CPL_IGNORE_RET_VAL(
                VSIFWriteL(psRegion->pData, 1, psRegion->nSize, fp));

        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    }
    return;
}

/************************************************************************/
/*                          GetRegion()                                 */
/************************************************************************/

const CachedRegion*
VSICurlFilesystemHandler::GetRegion( const char* pszURL,
                                     vsi_l_offset nFileOffsetStart )
{
    CPLMutexHolder oHolder( &hMutex );

    const unsigned long pszURLHash = CPLHashSetHashStr(pszURL);

    nFileOffsetStart =
        (nFileOffsetStart / DOWNLOAD_CHUNK_SIZE) * DOWNLOAD_CHUNK_SIZE;

    for( int i = 0; i < nRegions; i++ )
    {
        CachedRegion* psRegion = papsRegions[i];
        if( psRegion->pszURLHash == pszURLHash &&
            nFileOffsetStart == psRegion->nFileOffsetStart )
        {
            memmove(papsRegions + 1, papsRegions, i * sizeof(CachedRegion*));
            papsRegions[0] = psRegion;
            return psRegion;
        }
    }
    if( bUseCacheDisk )
        return GetRegionFromCacheDisk(pszURL, nFileOffsetStart);
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

    const unsigned long pszURLHash = CPLHashSetHashStr(pszURL);

    CachedRegion* psRegion = nullptr;
    if( nRegions == N_MAX_REGIONS )
    {
        psRegion = papsRegions[N_MAX_REGIONS-1];
        memmove(papsRegions + 1,
                papsRegions,
                (N_MAX_REGIONS-1) * sizeof(CachedRegion*));
        papsRegions[0] = psRegion;
        CPLFree(psRegion->pData);
    }
    else
    {
        papsRegions = static_cast<CachedRegion **>(
            CPLRealloc(papsRegions, (nRegions + 1) * sizeof(CachedRegion*)));
        if( nRegions )
            memmove(papsRegions + 1,
                    papsRegions,
                    nRegions * sizeof(CachedRegion*));
        nRegions++;
        psRegion = static_cast<CachedRegion *>(CPLMalloc(sizeof(CachedRegion)));
        papsRegions[0] = psRegion;
    }

    psRegion->pszURLHash = pszURLHash;
    psRegion->nFileOffsetStart = nFileOffsetStart;
    psRegion->nSize = nSize;
    psRegion->pData = nSize ? static_cast<char *>(CPLMalloc(nSize)) : nullptr;
    if( nSize )
        memcpy(psRegion->pData, pData, nSize);

    if( bUseCacheDisk )
        AddRegionToCacheDisk(psRegion);
}

/************************************************************************/
/*                         GetCachedFileProp()                          */
/************************************************************************/

CachedFileProp *
VSICurlFilesystemHandler::GetCachedFileProp( const char* pszURL )
{
    CPLMutexHolder oHolder( &hMutex );

    CachedFileProp* cachedFileProp = cacheFileSize[pszURL];
    if( cachedFileProp == nullptr )
    {
        cachedFileProp = new CachedFileProp;
        cacheFileSize[pszURL] = cachedFileProp;
    }

    return cachedFileProp;
}

/************************************************************************/
/*                        InvalidateCachedData()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateCachedData( const char* pszURL )
{
    CPLMutexHolder oHolder( &hMutex );

    std::map<CPLString, CachedFileProp*>::iterator oIter =
        cacheFileSize.find(pszURL);
    if( oIter != cacheFileSize.end() )
    {
        delete oIter->second;
        cacheFileSize.erase(oIter);
    }

    // Invalidate all cached regions for this URL
    const unsigned long pszURLHash = CPLHashSetHashStr(pszURL);
    for( int i = 0; i < nRegions; )
    {
        CachedRegion* psRegion = papsRegions[i];
        if( psRegion->pszURLHash == pszURLHash )
        {
            CPLFree(psRegion->pData);
            CPLFree(psRegion);
            if( i < nRegions - 1 )
            {
                memmove(papsRegions, papsRegions + 1,
                        (nRegions - 1 - i) * sizeof(CachedRegion*));
            }
            nRegions --;
        }
        else
        {
            i ++;
        }
    }
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSICurlFilesystemHandler::ClearCache()
{
    CPLMutexHolder oHolder( &hMutex );

    for( int i=0; i < nRegions; i++ )
    {
        CPLFree(papsRegions[i]->pData);
        CPLFree(papsRegions[i]);
    }
    CPLFree(papsRegions);
    nRegions = 0;
    papsRegions = nullptr;

    std::map<CPLString, CachedFileProp*>::const_iterator iterCacheFileSize;
    for( iterCacheFileSize = cacheFileSize.begin();
         iterCacheFileSize != cacheFileSize.end();
         ++iterCacheFileSize )
    {
        delete iterCacheFileSize->second;
    }
    cacheFileSize.clear();

    std::map<CPLString, CachedDirList*>::const_iterator iterCacheDirList;
    for( iterCacheDirList = cacheDirList.begin();
         iterCacheDirList != cacheDirList.end();
         ++iterCacheDirList )
    {
        CSLDestroy(iterCacheDirList->second->papszFileList);
        CPLFree(iterCacheDirList->second);
    }
    cacheDirList.clear();

    std::map<GIntBig, CachedConnection*>::const_iterator iterConnections;
    for( iterConnections = mapConnections.begin();
         iterConnections != mapConnections.end();
         ++iterConnections )
    {
        curl_multi_cleanup(iterConnections->second->hCurlMultiHandle);
        delete iterConnections->second;
    }
    mapConnections.clear();
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
/*                        IsAllowedFilename()                           */
/************************************************************************/

static bool IsAllowedFilename( const char* pszFilename )
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

        return bFound;
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSICurlFilesystemHandler::Open( const char *pszFilename,
                                                  const char *pszAccess,
                                                  bool bSetError )
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
    CPLString osURL(
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
    CachedFileProp* cachedFileProp =
        GetCachedFileProp(osFilename + strlen(GetFSPrefix()));
    if( !(cachedFileProp != nullptr && cachedFileProp->eExists == EXIST_YES) &&
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
    const char* pszDir = strchr(osURL.c_str(), '/');
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
                        CPLSPrintf("%s/%s", pszFilename + strlen("/vsicurl/"),
                                   beginFilename);
                    CachedFileProp* cachedFileProp =
                        GetCachedFileProp(osCachedFilename);
                    cachedFileProp->eExists = EXIST_YES;
                    cachedFileProp->bIsDirectory = bIsDirectory;
                    cachedFileProp->mTime = static_cast<time_t>(mTime);
                    cachedFileProp->bHasComputedFileSize = nFileSize > 0;
                    cachedFileProp->fileSize = nFileSize;

                    oFileList.AddString( beginFilename );
                    if( ENABLE_DEBUG )
                        CPLDebug("VSICURL",
                                 "File[%d] = %s, is_dir = %d, size = "
                                 CPL_FRMT_GUIB
                                 ", time = %04d/%02d/%02d %02d:%02d:%02d",
                                 nCount, beginFilename, bIsDirectory ? 1 : 0,
                                 nFileSize,
                                 brokendowntime.tm_year + 1900,
                                 brokendowntime.tm_mon + 1,
                                 brokendowntime.tm_mday,
                                 brokendowntime.tm_hour, brokendowntime.tm_min,
                                 brokendowntime.tm_sec);
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
/*                          AnalyseS3FileList()                         */
/************************************************************************/

void VSICurlFilesystemHandler::AnalyseS3FileList(
    const CPLString& osBaseURL,
    const char* pszXML,
    CPLStringList& osFileList,
    int nMaxFiles,
    bool& bIsTruncated,
    CPLString& osNextMarker )
{
#if DEBUG_VERBOSE
    CPLDebug("S3", "%s", pszXML);
#endif
    osNextMarker = "";
    bIsTruncated = false;
    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == nullptr )
        return;
    CPLXMLNode* psListBucketResult = CPLGetXMLNode(psTree, "=ListBucketResult");
    CPLXMLNode* psListAllMyBucketsResultBuckets =
        (psListBucketResult != nullptr ) ? nullptr :
        CPLGetXMLNode(psTree, "=ListAllMyBucketsResult.Buckets");

    std::vector< std::pair<CPLString, CachedFileProp> > aoProps;
    // Count the number of occurrences of a path. Can be 1 or 2. 2 in the case
    // that both a filename and directory exist
    std::map<CPLString, int> aoNameCount;

    if( psListBucketResult )
    {
        CPLString osPrefix = CPLGetXMLValue(psListBucketResult, "Prefix", "");
        CPLXMLNode* psIter = psListBucketResult->psChild;
        bool bNonEmpty = false;
        for( ; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Contents") == 0 )
            {
                bNonEmpty = true;
                const char* pszKey = CPLGetXMLValue(psIter, "Key", nullptr);
                if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    CachedFileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Size", "0")));
                    prop.bIsDirectory = false;
                    prop.mTime = 0;

                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMin = 0;
                    int nSec = 0;
                    if( sscanf( CPLGetXMLValue(psIter, "LastModified", ""),
                                "%04d-%02d-%02dT%02d:%02d:%02d",
                                &nYear, &nMonth, &nDay,
                                &nHour, &nMin, &nSec ) == 6 )
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMin;
                        brokendowntime.tm_sec = nSec;
                        prop.mTime =
                            static_cast<time_t>(
                                CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    aoProps.push_back(
                        std::pair<CPLString, CachedFileProp>
                            (pszKey + osPrefix.size(), prop));
                    aoNameCount[pszKey + osPrefix.size()] ++;
                }
            }
            else if( strcmp(psIter->pszValue, "CommonPrefixes") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Prefix", nullptr);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        CachedFileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bIsDirectory = true;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = 0;
                        prop.mTime = 0;

                        aoProps.push_back(
                            std::pair<CPLString, CachedFileProp>
                                (osKey.c_str() + osPrefix.size(), prop));
                        aoNameCount[osKey.c_str() + osPrefix.size()] ++;
                    }
                }
            }

            if( nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles) )
                break;
        }

        if( !(nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles)) )
        {
            osNextMarker = CPLGetXMLValue(psListBucketResult, "NextMarker", "");
            bIsTruncated =
                CPLTestBool(CPLGetXMLValue(psListBucketResult,
                                           "IsTruncated", "false"));
        }

        for( size_t i = 0; i < aoProps.size(); i++ )
        {
            CPLString osSuffix;
            if( aoNameCount[aoProps[i].first] == 2 &&
                    aoProps[i].second.bIsDirectory )
            {
                // Add a / suffix to disambiguish the situation
                // Normally we don't suffix directories with /, but we have
                // no alternative here
                osSuffix = "/";
            }
            if( nMaxFiles != 1 )
            {
                CPLString osCachedFilename =
                        osBaseURL + CPLAWSURLEncode(osPrefix,false) + CPLAWSURLEncode(aoProps[i].first,false) + osSuffix;
#if DEBUG_VERBOSE
                CPLDebug("S3", "Cache %s", osCachedFilename.c_str());
#endif
                *GetCachedFileProp(osCachedFilename) = aoProps[i].second;
            }
            osFileList.AddString( (aoProps[i].first + osSuffix).c_str() );
        }

        // In the case of an empty directory, bNonEmpty will be set since
        // there will be a <Contents> entry with the directory entry
        // In the case of an empty bucket, then we should get an empty
        // Prefix element.
        if( osFileList.size() == 0 && (bNonEmpty || osPrefix.empty()) )
        {
            // To avoid an error to be reported
            osFileList.AddString(".");
        }
    }
    else if( psListAllMyBucketsResultBuckets != nullptr )
    {
        CPLXMLNode* psIter = psListAllMyBucketsResultBuckets->psChild;
        for( ; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Bucket") == 0 )
            {
                const char* pszName = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszName )
                {
                    CachedFileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bIsDirectory = true;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = 0;
                    prop.mTime = 0;

                    CPLString osCachedFilename = osBaseURL + CPLAWSURLEncode(pszName, false);
#if DEBUG_VERBOSE
                    CPLDebug("S3", "Cache %s", osCachedFilename.c_str());
#endif
                    *GetCachedFileProp(osCachedFilename) = prop;

                    osFileList.AddString( pszName );
                }
            }
        }

        if( osFileList.size() == 0 )
        {
            // To avoid an error to be reported
            osFileList.AddString(".");
        }
    }

    CPLDestroyXMLNode(psTree);
}

/************************************************************************/
/*                         AnalyseAzureFileList()                       */
/************************************************************************/

void VSICurlFilesystemHandler::AnalyseAzureFileList(
    const CPLString& osBaseURL,
    bool bCacheResults,
    const char* pszXML,
    CPLStringList& osFileList,
    int nMaxFiles,
    bool& bIsTruncated,
    CPLString& osNextMarker )
{
#if DEBUG_VERBOSE
    CPLDebug("AZURE", "%s", pszXML);
#endif
    osNextMarker = "";
    bIsTruncated = false;
    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == nullptr )
        return;
    CPLXMLNode* psEnumerationResults = CPLGetXMLNode(psTree, "=EnumerationResults");

    std::vector< std::pair<CPLString, CachedFileProp> > aoProps;
    // Count the number of occurrences of a path. Can be 1 or 2. 2 in the case
    // that both a filename and directory exist
    std::map<CPLString, int> aoNameCount;

    if( psEnumerationResults )
    {
        bool bNonEmpty = false;
        CPLString osPrefix = CPLGetXMLValue(psEnumerationResults, "Prefix", "");
        CPLXMLNode* psBlobs = CPLGetXMLNode(psEnumerationResults, "Blobs");
        if( psBlobs == nullptr )
        {
            psBlobs = CPLGetXMLNode(psEnumerationResults, "Containers");
            if( psBlobs != nullptr )
                bNonEmpty = true;
        }
        CPLXMLNode* psIter = psBlobs ? psBlobs->psChild : nullptr;
        for( ; psIter != nullptr; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Blob") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszKey && strstr(pszKey, GDAL_MARKER_FOR_DIR) != nullptr )
                {
                    bNonEmpty = true;
                }
                else if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    bNonEmpty = true;

                    CachedFileProp prop;
                    prop.eExists = EXIST_YES;
                    prop.bHasComputedFileSize = true;
                    prop.fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Properties.Content-Length", "0")));
                    prop.bIsDirectory = false;
                    prop.mTime = 0;

                    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
                    if( CPLParseRFC822DateTime(
                        CPLGetXMLValue(psIter, "Properties.Last-Modified", ""),
                                                    &nYear,
                                                    &nMonth,
                                                    &nDay,
                                                    &nHour,
                                                    &nMinute,
                                                    &nSecond,
                                                    nullptr,
                                                    nullptr ) )
                    {
                        struct tm brokendowntime;
                        brokendowntime.tm_year = nYear - 1900;
                        brokendowntime.tm_mon = nMonth - 1;
                        brokendowntime.tm_mday = nDay;
                        brokendowntime.tm_hour = nHour;
                        brokendowntime.tm_min = nMinute;
                        brokendowntime.tm_sec = nSecond < 0 ? 0 : nSecond;
                        prop.mTime =
                            static_cast<time_t>(
                                CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    aoProps.push_back(
                        std::pair<CPLString, CachedFileProp>
                            (pszKey + osPrefix.size(), prop));
                    aoNameCount[pszKey + osPrefix.size()] ++;
                }
            }
            else if( strcmp(psIter->pszValue, "BlobPrefix") == 0 ||
                     strcmp(psIter->pszValue, "Container") == 0 )
            {
                bNonEmpty = true;

                const char* pszKey = CPLGetXMLValue(psIter, "Name", nullptr);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        CachedFileProp prop;
                        prop.eExists = EXIST_YES;
                        prop.bIsDirectory = true;
                        prop.bHasComputedFileSize = true;
                        prop.fileSize = 0;
                        prop.mTime = 0;

                        aoProps.push_back(
                            std::pair<CPLString, CachedFileProp>
                                (osKey.c_str() + osPrefix.size(), prop));
                        aoNameCount[osKey.c_str() + osPrefix.size()] ++;
                    }
                }
            }

            if( nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles) )
                break;
        }

        if( !(nMaxFiles > 0 && aoProps.size() > static_cast<unsigned>(nMaxFiles)) )
        {
            osNextMarker = CPLGetXMLValue(psEnumerationResults, "NextMarker", "");
            bIsTruncated =
                CPLTestBool(CPLGetXMLValue(psEnumerationResults,
                                           "IsTruncated", "false"));
        }

        for( size_t i = 0; i < aoProps.size(); i++ )
        {
            CPLString osSuffix;
            if( aoNameCount[aoProps[i].first] == 2 &&
                    aoProps[i].second.bIsDirectory )
            {
                // Add a / suffix to disambiguish the situation
                // Normally we don't suffix directories with /, but we have
                // no alternative here
                osSuffix = "/";
            }
            if( bCacheResults )
            {
                CPLString osCachedFilename =
                        osBaseURL + "/" + osPrefix + aoProps[i].first + osSuffix;
#if DEBUG_VERBOSE
                CPLDebug("AZURE", "Cache %s", osCachedFilename.c_str());
#endif
                *GetCachedFileProp(osCachedFilename) = aoProps[i].second;
            }
            osFileList.AddString( (aoProps[i].first + osSuffix).c_str() );
        }

        if( osFileList.size() == 0 && (bNonEmpty || osPrefix.empty()) )
        {
            // To avoid an error to be reported
            osFileList.AddString(".");
        }
    }
    CPLDestroyXMLNode(psTree);
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
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString
VSICurlFilesystemHandler::GetURLFromDirname( const CPLString& osDirname )
{
    return VSICurlGetURLFromFilename(osDirname, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

/************************************************************************/
/*                          GetFileList()                               */
/************************************************************************/

char** VSICurlFilesystemHandler::GetFileList(const char *pszDirname,
                                             int nMaxFiles,
                                             bool* pbGotFileList)
{
    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "GetFileList(%s)" , pszDirname);

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

            if( headers != nullptr )
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
                                       pszDirname + strlen("/vsicurl/"),
                                       pszFilename);
                        CachedFileProp* cachedFileProp =
                            GetCachedFileProp(osCachedFilename);
                        cachedFileProp->eExists = EXIST_YES;
                        cachedFileProp->bHasComputedFileSize = bSizeValid;
                        cachedFileProp->fileSize = nFileSize;
                        cachedFileProp->bIsDirectory = bIsDirectory;
                        cachedFileProp->mTime = static_cast<time_t>(mUnixTime);

                        oFileList.AddString(pszFilename);
                        if( ENABLE_DEBUG )
                        {
                            struct tm brokendowntime;
                            CPLUnixTimeToYMDHMS(mUnixTime, &brokendowntime);
                            CPLDebug("VSICURL",
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
                        if( ENABLE_DEBUG )
                            CPLDebug("VSICURL",
                                     "File[%d] = %s", nCount, pszLine);
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

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        if( headers != nullptr )
            curl_slist_free_all(headers);

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
            AnalyseS3FileList( osBaseURL,
                               sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIsTruncated,
                               osNextMarker );
            // If the list is truncated, then don't report it.
            if( !bIsTruncated )
            {
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
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

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
        pStatBuf->st_size = poHandle->GetFileSize();
    }

    const int nRet =
        poHandle->Exists((nFlags & VSI_STAT_SET_ERROR_FLAG) > 0) ? 0 : -1;
    pStatBuf->st_mtime = poHandle->GetMTime();
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

    CPLMutexHolder oHolder( &hMutex );

    // If we know the file exists and is not a directory,
    // then don't try to list its content.
    CachedFileProp* cachedFileProp =
        GetCachedFileProp(GetURLFromDirname(osDirname));
    if( cachedFileProp->eExists == EXIST_YES && !cachedFileProp->bIsDirectory )
    {
        if( osDirnameOri != osDirname )
        {
            cachedFileProp =
                    GetCachedFileProp((GetURLFromDirname(osDirname) + "/").c_str());
            if( cachedFileProp->eExists == EXIST_YES && !cachedFileProp->bIsDirectory )
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

    CachedDirList* psCachedDirList = cacheDirList[osDirname];
    if( psCachedDirList == nullptr )
    {
        psCachedDirList =
            static_cast<CachedDirList *>(CPLMalloc(sizeof(CachedDirList)));
        psCachedDirList->papszFileList =
            GetFileList(osDirname, nMaxFiles,
                        &psCachedDirList->bGotFileList);
        cacheDirList[osDirname] = psCachedDirList;
    }

    if( pbGotFileList )
        *pbGotFileList = psCachedDirList->bGotFileList;

    return CSLDuplicate(psCachedDirList->papszFileList);
}

/************************************************************************/
/*                        InvalidateDirContent()                        */
/************************************************************************/

void VSICurlFilesystemHandler::InvalidateDirContent( const char *pszDirname )
{
    CPLMutexHolder oHolder( &hMutex );
    std::map<CPLString, CachedDirList*>::iterator oIter =
        cacheDirList.find(pszDirname);
    if( oIter != cacheDirList.end() )
    {
        CSLDestroy( oIter->second->papszFileList );
        CPLFree( oIter->second );
        cacheDirList.erase(oIter);
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
/*                        IVSIS3LikeFSHandler                           */
/************************************************************************/

class IVSIS3LikeFSHandler: public VSICurlFilesystemHandler
{
  protected:
    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool* pbGotFileList ) override;

    virtual IVSIS3LikeHandleHelper* CreateHandleHelper(
            const char* pszURI, bool bAllowNoObject) = 0;

  public:
    int Unlink( const char *pszFilename ) override;
    int Mkdir( const char *pszDirname, long nMode ) override;
    int Rmdir( const char *pszDirname ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;

    virtual int      DeleteObject( const char *pszFilename );

    virtual const char* GetDebugKey() const = 0;

    virtual void UpdateMapFromHandle(IVSIS3LikeHandleHelper*) {}
    virtual void UpdateHandleFromMap( IVSIS3LikeHandleHelper * ) {}
};

/************************************************************************/
/*                         VSIS3FSHandler                               */
/************************************************************************/

class VSIS3FSHandler CPL_FINAL : public IVSIS3LikeFSHandler
{
    std::map< CPLString, VSIS3UpdateParams > oMapBucketsToS3Params;

  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    CPLString GetURLFromDirname( const CPLString& osDirname ) override;

    const char* GetDebugKey() const override { return "S3"; }

    IVSIS3LikeHandleHelper* CreateHandleHelper(
    const char* pszURI, bool bAllowNoObject) override;

    CPLString GetFSPrefix() override { return "/vsis3/"; }

    void ClearCache() override;

  public:
    VSIS3FSHandler() {}
    ~VSIS3FSHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    void UpdateMapFromHandle( IVSIS3LikeHandleHelper * poS3HandleHelper )
        override;
    void UpdateHandleFromMap( IVSIS3LikeHandleHelper * poS3HandleHelper )
        override;
};

/************************************************************************/
/*                          IVSIS3LikeHandle                            */
/************************************************************************/

class IVSIS3LikeHandle:  public VSICurlHandle
{
  protected:
    bool UseLimitRangeGetInsteadOfHead() override { return true; }
    bool IsDirectoryFromExists( const char* pszVerb,
                                int response_code ) override
        {
            // A bit dirty, but on S3, a GET on a existing directory returns a 416 
            return response_code == 416 && EQUAL(pszVerb, "GET") &&
                   CPLString(m_pszURL).back() == '/';
        }
    void ProcessGetFileSizeResult( const char* pszContent ) override
        {
            bIsDirectory = strstr(pszContent, "ListBucketResult") != nullptr;
        }

  public:
    IVSIS3LikeHandle( VSICurlFilesystemHandler* poFSIn,
                      const char* pszFilename,
                      const char* pszURLIn = nullptr ) :
        VSICurlHandle(poFSIn, pszFilename, pszURLIn) {}
    ~IVSIS3LikeHandle() override {}
};

/************************************************************************/
/*                            VSIS3Handle                               */
/************************************************************************/

class VSIS3Handle CPL_FINAL : public IVSIS3LikeHandle
{
    VSIS3HandleHelper* m_poS3HandleHelper;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;
    bool CanRestartOnError( const char*, const char*, bool ) override;
    bool AllowAutomaticRedirection() override
        { return m_poS3HandleHelper->AllowAutomaticRedirection(); }

  public:
    VSIS3Handle( VSIS3FSHandler* poFS,
                 const char* pszFilename,
                 VSIS3HandleHelper* poS3HandleHelper );
    ~VSIS3Handle() override;
};

/************************************************************************/
/*                            VSIS3WriteHandle                          */
/************************************************************************/

class VSIS3WriteHandle CPL_FINAL : public VSIVirtualHandle
{
    IVSIS3LikeFSHandler     *m_poFS;
    CPLString           m_osFilename;
    IVSIS3LikeHandleHelper  *m_poS3HandleHelper;
    bool                m_bUseChunked;

    vsi_l_offset        m_nCurOffset;
    int                 m_nBufferOff;
    int                 m_nBufferSize;
    int                 m_nBufferOffReadCallback;
    bool                m_bClosed;
    GByte              *m_pabyBuffer;
    CPLString           m_osUploadID;
    int                 m_nPartNumber;
    std::vector<CPLString> m_aosEtags;
    CPLString           m_osXML;
    int                 m_nOffsetInXML;
    bool                m_bError;

    CURLM              *m_hCurlMulti;
    CURL               *m_hCurl;
    const void         *m_pBuffer;
    CPLString           m_osCurlErrBuf;
    size_t              m_nChunkedBufferOff;
    size_t              m_nChunkedBufferSize;

    static size_t       ReadCallBackBuffer( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    bool                InitiateMultipartUpload();
    bool                UploadPart();
    static size_t       ReadCallBackXML( char *buffer, size_t size,
                                         size_t nitems, void *instream );
    bool                CompleteMultipart();
    bool                AbortMultipart();
    bool                DoSinglePartPUT();

    static size_t       ReadCallBackBufferChunked( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    size_t              WriteChunked( const void *pBuffer,
                                      size_t nSize, size_t nMemb );
    int                 FinishChunkedTransfer();

    void                InvalidateParentDirectory();

    public:
      VSIS3WriteHandle( IVSIS3LikeFSHandler* poFS,
                        const char* pszFilename,
                        IVSIS3LikeHandleHelper* poS3HandleHelper,
                        bool bUseChunked );
      ~VSIS3WriteHandle() override;

      int Seek( vsi_l_offset nOffset, int nWhence ) override;
      vsi_l_offset Tell() override;
      size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
      size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
      int Eof() override;
      int Close() override;

      bool IsOK() { return m_bUseChunked || m_pabyBuffer != nullptr; }
};

/************************************************************************/
/*                         VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::VSIS3WriteHandle( IVSIS3LikeFSHandler* poFS,
                                    const char* pszFilename,
                                    IVSIS3LikeHandleHelper* poS3HandleHelper,
                                    bool bUseChunked ) :
        m_poFS(poFS), m_osFilename(pszFilename),
        m_poS3HandleHelper(poS3HandleHelper),
        m_bUseChunked(bUseChunked),
        m_nCurOffset(0),
        m_nBufferOff(0),
        m_nBufferSize(0),
        m_nBufferOffReadCallback(0),
        m_bClosed(false),
        m_pabyBuffer(nullptr),
        m_nPartNumber(0),
        m_nOffsetInXML(0),
        m_bError(false),
        m_hCurlMulti(nullptr),
        m_hCurl(nullptr),
        m_pBuffer(nullptr),
        m_nChunkedBufferOff(0),
        m_nChunkedBufferSize(0)
{
    // AWS S3 does not support chunked PUT in a convenient way, since you must
    // know in advance the total size... See
    // http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-streaming.html
    // So we must use the mulipart upload mechanism.
    // But this mechanism is not supported by GS. Luckily it does support
    // standard "Transfer-Encoding: chunked" PUT mechanism
    // So two different implementations.

    if( !m_bUseChunked )
    {
        const int nChunkSizeMB = atoi(
            CPLGetConfigOption("VSIS3_CHUNK_SIZE",
                    CPLGetConfigOption("VSIOSS_CHUNK_SIZE", "50")));
        if( nChunkSizeMB <= 0 || nChunkSizeMB > 1000 )
            m_nBufferSize = 0;
        else
            m_nBufferSize = nChunkSizeMB * 1024 * 1024;

        // For testing only !
        const char* pszChunkSizeBytes =
            CPLGetConfigOption("VSIS3_CHUNK_SIZE_BYTES",
                CPLGetConfigOption("VSIOSS_CHUNK_SIZE_BYTES", nullptr));
        if( pszChunkSizeBytes )
            m_nBufferSize = atoi(pszChunkSizeBytes);
        if( m_nBufferSize <= 0 || m_nBufferSize > 1000 * 1024 * 1024 )
            m_nBufferSize = 50 * 1024 * 1024;

        m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
        if( m_pabyBuffer == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot allocate working buffer for %s",
                     m_poFS->GetFSPrefix().c_str());
        }
    }
}

/************************************************************************/
/*                        ~VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::~VSIS3WriteHandle()
{
    Close();
    delete m_poS3HandleHelper;
    CPLFree(m_pabyBuffer);
    if( m_hCurlMulti )
    {
        if( m_hCurl )
        {
            curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
            curl_easy_cleanup(m_hCurl);
        }
        curl_multi_cleanup(m_hCurlMulti);
    }
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIS3WriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( !((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable %s files",
                 m_poFS->GetFSPrefix().c_str());
        m_bError = true;
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIS3WriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIS3WriteHandle::Read( void * /* pBuffer */, size_t /* nSize */,
                               size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read not supported on writable %s files",
             m_poFS->GetFSPrefix().c_str());
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                        InitiateMultipartUpload()                     */
/************************************************************************/

bool VSIS3WriteHandle::InitiateMultipartUpload()
{
    bool bSuccess = true;
    bool bGoOn;
    do
    {
        bGoOn = false;
        CURL* hCurlHandle = curl_easy_init();
        m_poS3HandleHelper->AddQueryParameter("uploads", "");
        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                         m_poS3HandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("POST", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        void* old_handler = CPLHTTPIgnoreSigPipe();
        curl_easy_perform(hCurlHandle);
        CPLHTTPRestoreSigPipeHandler(old_handler);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 || sWriteFuncData.pBuffer == nullptr )
        {
            if( sWriteFuncData.pBuffer != nullptr &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                      sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug(m_poFS->GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "InitiateMultipartUpload of %s failed",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            m_poFS->InvalidateCachedData(
                m_poS3HandleHelper->GetURL().c_str());
            m_poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );

            CPLXMLNode* psNode =
                CPLParseXMLString( sWriteFuncData.pBuffer );
            if( psNode )
            {
                m_osUploadID =
                    CPLGetXMLValue(
                        psNode, "=InitiateMultipartUploadResult.UploadId", "");
                CPLDebug(m_poFS->GetDebugKey(),
                         "UploadId: %s", m_osUploadID.c_str());
                CPLDestroyXMLNode(psNode);
            }
            if( m_osUploadID.empty() )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "InitiateMultipartUpload of %s failed: cannot get UploadId",
                    m_osFilename.c_str());
                bSuccess = false;
            }
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );
    return bSuccess;
}

/************************************************************************/
/*                         ReadCallBackBuffer()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackBuffer( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
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
/*                           UploadPart()                               */
/************************************************************************/

bool VSIS3WriteHandle::UploadPart()
{
    ++m_nPartNumber;
    if( m_nPartNumber > 10000 )
    {
        m_bError = true;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "10000 parts have been uploaded for %s failed. "
            "This is the maximum. "
            "Increase VSIS3_CHUNK_SIZE to a higher value (e.g. 500 for 500 MB)",
            m_osFilename.c_str());
        return false;
    }

    bool bSuccess = true;

    m_nBufferOffReadCallback = 0;
    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("partNumber",
                                          CPLSPrintf("%d", m_nPartNumber));
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                     m_poS3HandleHelper->GetURL().c_str());
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

    struct curl_slist* headers = static_cast<struct curl_slist*>(
        CPLHTTPSetOptions(hCurlHandle, nullptr));
    headers = VSICurlMergeHeaders(headers,
                    m_poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                        m_pabyBuffer,
                                                        m_nBufferOff));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    WriteFuncStruct sWriteFuncHeaderData;
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);

    void* old_handler = CPLHTTPIgnoreSigPipe();
    curl_easy_perform(hCurlHandle);
    CPLHTTPRestoreSigPipeHandler(old_handler);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 200 || sWriteFuncHeaderData.pBuffer == nullptr )
    {
        CPLDebug(m_poFS->GetDebugKey(), "%s",
                 sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s failed",
                    m_nPartNumber, m_osFilename.c_str());
        bSuccess = false;
    }
    else
    {
        const char* pszEtag = strstr(sWriteFuncHeaderData.pBuffer, "ETag: ");
        if( pszEtag != nullptr )
        {
            CPLString osEtag = pszEtag + strlen("ETag: ");
            const size_t nPos = osEtag.find("\r");
            if( nPos != std::string::npos )
                osEtag.resize(nPos);
            CPLDebug(m_poFS->GetDebugKey(), "Etag for part %d is %s",
                     m_nPartNumber, osEtag.c_str());
            m_aosEtags.push_back(osEtag);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "UploadPart(%d) of %s (uploadId = %s) failed",
                     m_nPartNumber, m_osFilename.c_str(), m_osUploadID.c_str());
            bSuccess = false;
        }
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

    return bSuccess;
}

/************************************************************************/
/*                      ReadCallBackBufferChunked()                     */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackBufferChunked( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
    if( poThis->m_nChunkedBufferSize == 0 )
    {
        //CPLDebug("VSIS3WriteHandle", "Writing 0 byte (finish)");
        return 0;
    }
    const size_t nSizeMax = size * nitems;
    size_t nSizeToWrite = nSizeMax;
    size_t nChunckedBufferRemainingSize =
                poThis->m_nChunkedBufferSize - poThis->m_nChunkedBufferOff;
    if( nChunckedBufferRemainingSize < nSizeToWrite )
        nSizeToWrite = nChunckedBufferRemainingSize;
    memcpy(buffer,
           static_cast<const GByte*>(poThis->m_pBuffer) + poThis->m_nChunkedBufferOff,
           nSizeToWrite);
    poThis->m_nChunkedBufferOff += nSizeToWrite;
    //CPLDebug("VSIS3WriteHandle", "Writing %d bytes", nSizeToWrite);
    return nSizeToWrite;
}

/************************************************************************/
/*                          WriteChunked()                              */
/************************************************************************/

size_t
VSIS3WriteHandle::WriteChunked( const void *pBuffer, size_t nSize, size_t nMemb )
{
    const size_t nBytesToWrite = nSize * nMemb;

    struct curl_slist* headers = nullptr;
    if( m_hCurlMulti == nullptr )
    {
        m_hCurlMulti = curl_multi_init();
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                        m_poS3HandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                         ReadCallBackBufferChunked);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);

        headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("PUT", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_osCurlErrBuf.resize(CURL_ERROR_SIZE+1);
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, &m_osCurlErrBuf[0] );

        curl_multi_add_handle(m_hCurlMulti, hCurlHandle);
        m_hCurl = hCurlHandle;
    }
    else if( m_hCurl == nullptr )
    {
        return 0; // An error occurred before
    }

    m_pBuffer = pBuffer;
    m_nChunkedBufferOff = 0;
    m_nChunkedBufferSize = nBytesToWrite;

    int repeats = 0;
    while( m_nChunkedBufferOff <  m_nChunkedBufferSize)
    {
        int still_running;
        while (curl_multi_perform(m_hCurlMulti, &still_running) ==
                                        CURLM_CALL_MULTI_PERFORM &&
            m_nChunkedBufferOff <  m_nChunkedBufferSize)
        {
            // loop
        }
        if( !still_running || m_nChunkedBufferOff == m_nChunkedBufferSize )
            break;

        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(m_hCurlMulti, &msgq);
            if(msg && (msg->msg == CURLMSG_DONE))
            {
                CURL *e = msg->easy_handle;
                if( e == m_hCurl )
                {
                    long response_code;
                    curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE,
                                        &response_code);
                    if( response_code != 200 )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Error %d: %s",
                                static_cast<int>(response_code),
                                m_osCurlErrBuf.c_str());

                        curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
                        curl_easy_cleanup(m_hCurl);
                        m_hCurl = nullptr;

                        if( headers )
                            curl_slist_free_all(headers);
                        return 0;
                    }
                }
            }
        } while(msg);

        CPLMultiPerformWait(m_hCurlMulti, repeats);
    }

    if( headers )
        curl_slist_free_all(headers);

    m_pBuffer = nullptr;

    long response_code;
    curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
    if( response_code != 100 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Error %d: %s",
                    static_cast<int>(response_code),
                    m_osCurlErrBuf.c_str());
        curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
        curl_easy_cleanup(m_hCurl);
        m_hCurl = nullptr;
        return 0;
    }

    return nMemb;
}

/************************************************************************/
/*                        FinishChunkedTransfer()                       */
/************************************************************************/

int VSIS3WriteHandle::FinishChunkedTransfer()
{
    if( m_hCurl == nullptr )
        return -1;

    m_pBuffer = nullptr;
    m_nChunkedBufferOff = 0;
    m_nChunkedBufferSize = 0;

    MultiPerform(m_hCurlMulti);

    long response_code;
    curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
    if( response_code == 200 )
    {
        InvalidateParentDirectory();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Error %d: %s",
                    static_cast<int>(response_code),
                    m_osCurlErrBuf.c_str());
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t
VSIS3WriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if( m_bError )
        return 0;

    size_t nBytesToWrite = nSize * nMemb;
    if( nBytesToWrite == 0 )
        return 0;

    if( m_bUseChunked )
    {
        return WriteChunked(pBuffer, nSize, nMemb);
    }

    const GByte* pabySrcBuffer = reinterpret_cast<const GByte*>(pBuffer);
    while( nBytesToWrite > 0 )
    {
        const int nToWriteInBuffer = static_cast<int>(
            std::min(static_cast<size_t>(m_nBufferSize - m_nBufferOff),
                     nBytesToWrite));
        memcpy(m_pabyBuffer + m_nBufferOff, pabySrcBuffer, nToWriteInBuffer);
        pabySrcBuffer += nToWriteInBuffer;
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
        if( m_nBufferOff == m_nBufferSize )
        {
            if( m_nCurOffset == (vsi_l_offset)m_nBufferSize )
            {
                if( !InitiateMultipartUpload() )
                {
                    m_bError = true;
                    return 0;
                }
            }
            if( !UploadPart() )
            {
                m_bError = true;
                return 0;
            }
            m_nBufferOff = 0;
        }
    }
    return nMemb;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIS3WriteHandle::Eof()
{
    return FALSE;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIS3WriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(
        m_poS3HandleHelper->GetURL().c_str() );

    CPLString osFilenameWithoutSlash(m_osFilename);
    if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
        osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );
    m_poFS->InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
}

/************************************************************************/
/*                           DoSinglePartPUT()                          */
/************************************************************************/

bool VSIS3WriteHandle::DoSinglePartPUT()
{
    bool bSuccess = true;
    bool bGoOn;
    do
    {
        bGoOn = false;
        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                         m_poS3HandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));
        headers = VSICurlMergeHeaders(headers,
                        m_poS3HandleHelper->GetCurlHeaders("PUT", headers,
                                                           m_pabyBuffer,
                                                           m_nBufferOff));
        headers = curl_slist_append(headers, "Expect: 100-continue");
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        void* old_handler = CPLHTTPIgnoreSigPipe();
        curl_easy_perform(hCurlHandle);
        CPLHTTPRestoreSigPipeHandler(old_handler);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            if( sWriteFuncData.pBuffer != nullptr &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                      sWriteFuncHeaderData.pBuffer,
                                                      false) )
            {
                m_poFS->UpdateMapFromHandle(m_poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "DoSinglePartPUT of %s failed",
                         m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );
    return bSuccess;
}

/************************************************************************/
/*                            ReadCallBackXML()                         */
/************************************************************************/

size_t VSIS3WriteHandle::ReadCallBackXML( char *buffer, size_t size,
                                          size_t nitems, void *instream )
{
    VSIS3WriteHandle* poThis = static_cast<VSIS3WriteHandle *>(instream);
    const int nSizeMax = static_cast<int>(size * nitems);
    const int nSizeToWrite =
        std::min(nSizeMax,
                 static_cast<int>(poThis->m_osXML.size()) -
                 poThis->m_nOffsetInXML);
    memcpy(buffer, poThis->m_osXML.c_str() + poThis->m_nOffsetInXML,
           nSizeToWrite);
    poThis->m_nOffsetInXML += nSizeToWrite;
    return nSizeToWrite;
}

/************************************************************************/
/*                        CompleteMultipart()                           */
/************************************************************************/

bool VSIS3WriteHandle::CompleteMultipart()
{
    bool bSuccess = true;

    m_osXML = "<CompleteMultipartUpload>\n";
    for( size_t i = 0; i < m_aosEtags.size(); i++ )
    {
        m_osXML += "<Part>\n";
        m_osXML += CPLSPrintf("<PartNumber>%d</PartNumber>",
                              static_cast<int>(i+1));
        m_osXML += "<ETag>" + m_aosEtags[i] + "</ETag>";
        m_osXML += "</Part>\n";
    }
    m_osXML += "</CompleteMultipartUpload>\n";

    m_nOffsetInXML = 0;
    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                     m_poS3HandleHelper->GetURL().c_str());
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackXML);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                     static_cast<int>(m_osXML.size()));
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

    struct curl_slist* headers = static_cast<struct curl_slist*>(
        CPLHTTPSetOptions(hCurlHandle, nullptr));
    headers = VSICurlMergeHeaders(headers,
                    m_poS3HandleHelper->GetCurlHeaders("POST", headers,
                                                        m_osXML.c_str(),
                                                        m_osXML.size()));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    void* old_handler = CPLHTTPIgnoreSigPipe();
    curl_easy_perform(hCurlHandle);
    CPLHTTPRestoreSigPipeHandler(old_handler);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 200 )
    {
        CPLDebug("S3", "%s",
                 sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CompleteMultipart of %s (uploadId=%s) failed",
                 m_osFilename.c_str(), m_osUploadID.c_str());
        bSuccess = false;
    }
    else
    {
        InvalidateParentDirectory();
    }

    CPLFree(sWriteFuncData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

    return bSuccess;
}

/************************************************************************/
/*                          AbortMultipart()                            */
/************************************************************************/

bool VSIS3WriteHandle::AbortMultipart()
{
    bool bSuccess = true;

    CURL* hCurlHandle = curl_easy_init();
    m_poS3HandleHelper->AddQueryParameter("uploadId", m_osUploadID);
    curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                     m_poS3HandleHelper->GetURL().c_str());
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

    struct curl_slist* headers = static_cast<struct curl_slist*>(
        CPLHTTPSetOptions(hCurlHandle, nullptr));
    headers = VSICurlMergeHeaders(headers,
                    m_poS3HandleHelper->GetCurlHeaders("DELETE", headers));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    void* old_handler = CPLHTTPIgnoreSigPipe();
    curl_easy_perform(hCurlHandle);
    CPLHTTPRestoreSigPipeHandler(old_handler);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 204 )
    {
        CPLDebug("S3", "%s",
                 sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AbortMultipart of %s (uploadId=%s) failed",
                 m_osFilename.c_str(), m_osUploadID.c_str());
        bSuccess = false;
    }

    CPLFree(sWriteFuncData.pBuffer);

    curl_easy_cleanup(hCurlHandle);

    return bSuccess;
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIS3WriteHandle::Close()
{
    int nRet = 0;
    if( !m_bClosed )
    {
        m_bClosed = true;
        if( m_bUseChunked && m_hCurlMulti != nullptr )
        {
            nRet = FinishChunkedTransfer();
        }
        else if( m_osUploadID.empty() )
        {
            if( !m_bError && !DoSinglePartPUT() )
                nRet = -1;
        }
        else
        {
            if( m_bError )
            {
                if( !AbortMultipart() )
                    nRet = -1;
            }
            else if( m_nBufferOff > 0 && !UploadPart() )
                nRet = -1;
            else if( !CompleteMultipart() )
                nRet = -1;
        }
    }
    return nRet;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIS3FSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        /*if( strchr(pszAccess, '+') != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "w+ not supported for /vsis3. Only w");
            return NULL;
        }*/
        VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str(), false);
        if( poS3HandleHelper == nullptr )
            return nullptr;
        UpdateHandleFromMap(poS3HandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poS3HandleHelper, false);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = nullptr;
        }
        return poHandle;
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
}

/************************************************************************/
/*                         ~VSIS3FSHandler()                            */
/************************************************************************/

VSIS3FSHandler::~VSIS3FSHandler()
{
    VSIS3HandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIS3FSHandler::ClearCache()
{
    VSICurlFilesystemHandler::ClearCache();

    oMapBucketsToS3Params.clear();

    VSIS3HandleHelper::ClearCache();
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Mkdir( const char * pszDirname, long /* nMode */ )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) == 0 &&
        sStat.st_mode == S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "Directory %s already exists", osDirname.c_str());
        errno = EEXIST;
        return -1;
    }

    VSILFILE* fp = VSIFOpenL(osDirname, "wb");
    if( fp != nullptr )
    {
        CPLErrorReset();
        VSIFCloseL(fp);
        return CPLGetLastErrorType() == CPLE_None ? 0 : -1;
    }
    else
    {
        return -1;
    }
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Rmdir( const char * pszDirname )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) != 0 )
    {
        CPLDebug(GetDebugKey(), "%s is not a object", pszDirname);
        errno = ENOENT;
        return -1;
    }
    else if( sStat.st_mode != S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "%s is not a directory", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    char** papszFileList = ReadDirEx(osDirname, 1);
    bool bEmptyDir = (papszFileList != nullptr && EQUAL(papszFileList[0], ".") &&
                      papszFileList[1] == nullptr);
    CSLDestroy(papszFileList);
    if( !bEmptyDir )
    {
        CPLDebug(GetDebugKey(), "%s is not empty", pszDirname);
        errno = ENOTEMPTY;
        return -1;
    }


    CPLString osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );
    if( osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
                                                        std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is a bucket", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    return DeleteObject(osDirname);
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int IVSIS3LikeFSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                          int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    CPLString osFilename(pszFilename);
    if( osFilename.find('/', GetFSPrefix().size()) == std::string::npos )
        osFilename += "/";
    return VSICurlFilesystemHandler::Stat(osFilename, pStatBuf, nFlags);
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIS3FSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false);
    if( poS3HandleHelper )
    {
        UpdateHandleFromMap(poS3HandleHelper);
        return new VSIS3Handle(this, pszFilename, poS3HandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIS3FSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    CPLString osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());

    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(osDirnameWithoutPrefix,
                                        GetFSPrefix().c_str(), true);
    if( poS3HandleHelper == nullptr )
    {
        return "";
    }
    UpdateHandleFromMap(poS3HandleHelper);
    CPLString osBaseURL(poS3HandleHelper->GetURL());
    if( !osBaseURL.empty() && osBaseURL.back() == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poS3HandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIS3FSHandler::CreateHandleHelper(const char* pszURI,
                                                          bool bAllowNoObject)
{
    return VSIS3HandleHelper::BuildFromURI(
                                pszURI, GetFSPrefix().c_str(), bAllowNoObject);
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int IVSIS3LikeFSHandler::Unlink( const char *pszFilename )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    if( osNameWithoutPrefix.find('/') == std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is not a file", pszFilename);
        errno = EISDIR;
        return -1;
    }

    VSIStatBufL sStat;
    if( VSIStatL(pszFilename, &sStat) != 0 )
    {
        CPLDebug(GetDebugKey(), "%s is not a object", pszFilename);
        errno = ENOENT;
        return -1;
    }
    else if( sStat.st_mode != S_IFREG )
    {
        CPLDebug(GetDebugKey(), "%s is not a file", pszFilename);
        errno = EISDIR;
        return -1;
    }

    return DeleteObject(pszFilename);
}

/************************************************************************/
/*                           DeleteObject()                             */
/************************************************************************/

int IVSIS3LikeFSHandler::DeleteObject( const char *pszFilename )
{
    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    IVSIS3LikeHandleHelper* poS3HandleHelper =
        CreateHandleHelper(osNameWithoutPrefix, false);
    if( poS3HandleHelper == nullptr )
    {
        return -1;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    int nRet = 0;

    bool bGoOn;
    do
    {
        bGoOn = false;
        CURL* hCurlHandle = curl_easy_init();
        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                         poS3HandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));
        headers = VSICurlMergeHeaders(headers,
                        poS3HandleHelper->GetCurlHeaders("DELETE", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        void* old_handler = CPLHTTPIgnoreSigPipe();
        curl_easy_perform(hCurlHandle);
        CPLHTTPRestoreSigPipeHandler(old_handler);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        // S3 and GS respond with 204. Azure with 202
        if( response_code != 204 && response_code != 202)
        {
            if( sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                    sWriteFuncHeaderData.pBuffer,
                                                    false) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer
                         : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined, "Delete of %s failed",
                         pszFilename);
                nRet = -1;
            }
        }
        else
        {
            InvalidateCachedData(poS3HandleHelper->GetURL().c_str());

            CPLString osFilenameWithoutSlash(pszFilename);
            if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
                osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );

            InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
        }

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );

    delete poS3HandleHelper;
    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** IVSIS3LikeFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLAssert( strlen(pszDirname) >= GetFSPrefix().size() );
    CPLString osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();
    if( !osDirnameWithoutPrefix.empty() &&
                                osDirnameWithoutPrefix.back() == '/' )
    {
        osDirnameWithoutPrefix.resize(osDirnameWithoutPrefix.size()-1);
    }

    CPLString osBucket(osDirnameWithoutPrefix);
    CPLString osObjectKey;
    size_t nSlashPos = osDirnameWithoutPrefix.find('/');
    if( nSlashPos != std::string::npos )
    {
        osBucket = osDirnameWithoutPrefix.substr(0, nSlashPos);
        osObjectKey = osDirnameWithoutPrefix.substr(nSlashPos+1);
    }

    IVSIS3LikeHandleHelper* poS3HandleHelper =
        CreateHandleHelper(osBucket, true);
    if( poS3HandleHelper == nullptr )
    {
        return nullptr;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    CPLString osMaxKeys = CPLGetConfigOption("AWS_MAX_KEYS", "");
    if( nMaxFiles > 0 && nMaxFiles < 100 &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys)) )
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    while( true )
    {
        poS3HandleHelper->ResetQueryParameters();
        CPLString osBaseURL(poS3HandleHelper->GetURL());

        CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osBaseURL);
        CURL* hCurlHandle = curl_easy_init();

        if( !osBucket.empty() )
        {
            poS3HandleHelper->AddQueryParameter("delimiter", "/");
            if( !osNextMarker.empty() )
                poS3HandleHelper->AddQueryParameter("marker", osNextMarker);
            if( !osMaxKeys.empty() )
                poS3HandleHelper->AddQueryParameter("max-keys", osMaxKeys);
            if( !osObjectKey.empty() )
                poS3HandleHelper->AddQueryParameter("prefix", osObjectKey + "/");
        }

        struct curl_slist* headers = 
            VSICurlSetOptions(hCurlHandle, poS3HandleHelper->GetURL(), nullptr);
        // Disable automatic redirection
        curl_easy_setopt(hCurlHandle, CURLOPT_FOLLOWLOCATION, 0 );

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        WriteFuncStruct sWriteFuncHeaderData;
        VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                               poS3HandleHelper->GetCurlHeaders("GET", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        if( headers != nullptr )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == nullptr)
        {
            delete poS3HandleHelper;
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            bool bUpdateMap = true;
            if( sWriteFuncData.pBuffer != nullptr &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer,
                                                    sWriteFuncHeaderData.pBuffer,
                                                    false, &bUpdateMap) )
            {
                if( bUpdateMap )
                {
                    UpdateMapFromHandle(poS3HandleHelper);
                }
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
            }
            else
            {
                CPLDebug(GetDebugKey(), "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer : "(null)");
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                delete poS3HandleHelper;
                curl_easy_cleanup(hCurlHandle);
                return nullptr;
            }
        }
        else
        {
            *pbGotFileList = true;
            bool bIsTruncated;
            AnalyseS3FileList( osBaseURL,
                               sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIsTruncated,
                               osNextMarker );

            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);

            if( osNextMarker.empty() )
            {
                delete poS3HandleHelper;
                curl_easy_cleanup(hCurlHandle);
                return osFileList.StealList();
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIS3HandleHelper * poS3HandleHelper =
        dynamic_cast<VSIS3HandleHelper *>(poHandleHelper);
    CPLAssert( poS3HandleHelper );
    if( !poS3HandleHelper )
        return;
    oMapBucketsToS3Params[ poS3HandleHelper->GetBucket() ] =
        VSIS3UpdateParams ( poS3HandleHelper );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIS3HandleHelper * poS3HandleHelper =
        dynamic_cast<VSIS3HandleHelper *>(poHandleHelper);
    CPLAssert( poS3HandleHelper );
    if( !poS3HandleHelper )
        return;
    std::map< CPLString, VSIS3UpdateParams>::iterator oIter =
        oMapBucketsToS3Params.find(poS3HandleHelper->GetBucket());
    if( oIter != oMapBucketsToS3Params.end() )
    {
        oIter->second.UpdateHandlerHelper(poS3HandleHelper);
    }
}

/************************************************************************/
/*                             VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::VSIS3Handle( VSIS3FSHandler* poFSIn,
                          const char* pszFilename,
                          VSIS3HandleHelper* poS3HandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poS3HandleHelper->GetURL()),
        m_poS3HandleHelper(poS3HandleHelper)
{
}

/************************************************************************/
/*                            ~VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::~VSIS3Handle()
{
    delete m_poS3HandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist* VSIS3Handle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3Handle::CanRestartOnError(const char* pszErrorMsg,
                                    const char* pszHeaders, bool bSetError)
{
    bool bUpdateMap = false;
    if( m_poS3HandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                              bSetError, &bUpdateMap) )
    {
        if( bUpdateMap )
        {
            static_cast<VSIS3FSHandler *>(poFS)->
                UpdateMapFromHandle(m_poS3HandleHelper);
        }

        SetURL(m_poS3HandleHelper->GetURL());
        return true;
    }
    return false;
}

/************************************************************************/
/*                         VSIGSFSHandler                               */
/************************************************************************/

class VSIGSFSHandler CPL_FINAL : public IVSIS3LikeFSHandler
{
  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    const char* GetDebugKey() const override { return "GS"; }

    CPLString GetFSPrefix() override { return "/vsigs/"; }
    CPLString GetURLFromDirname( const CPLString& osDirname ) override;

    IVSIS3LikeHandleHelper* CreateHandleHelper(
        const char* pszURI, bool bAllowNoObject) override;

    void ClearCache() override;

  public:
    VSIGSFSHandler() {}
    ~VSIGSFSHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;
};

/************************************************************************/
/*                            VSIGSHandle                               */
/************************************************************************/

class VSIGSHandle CPL_FINAL : public IVSIS3LikeHandle
{
    VSIGSHandleHelper* m_poHandleHelper;

  protected:
    struct curl_slist* GetCurlHeaders( const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;

  public:
    VSIGSHandle( VSIGSFSHandler* poFS, const char* pszFilename,
                 VSIGSHandleHelper* poHandleHelper);
    ~VSIGSHandle() override;
};

/************************************************************************/
/*                          ~VSIGSFSHandler()                           */
/************************************************************************/

VSIGSFSHandler::~VSIGSFSHandler()
{
    VSIGSHandleHelper::CleanMutex();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIGSFSHandler::ClearCache()
{
    VSICurlFilesystemHandler::ClearCache();

    VSIGSHandleHelper::ClearCache();
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIGSFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( pszFilename + GetFSPrefix().size(),
                                         GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return nullptr;
    return new VSIGSHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIGSFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        VSIGSHandleHelper* poHandleHelper =
            VSIGSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str());
        if( poHandleHelper == nullptr )
            return nullptr;
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poHandleHelper, true);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = nullptr;
        }
        return poHandle;
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIGSFSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    CPLString osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( osDirnameWithoutPrefix, GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return CPLString();
    CPLString osURL( poHandleHelper->GetURL() );
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIGSFSHandler::CreateHandleHelper(const char* pszURI,
                                                           bool)
{
    return VSIGSHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                             VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::VSIGSHandle( VSIGSFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIGSHandleHelper* poHandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                            ~VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::~VSIGSHandle()
{
    delete m_poHandleHelper;
}


/************************************************************************/
/*                          GetCurlHeaders()                            */
/************************************************************************/

struct curl_slist* VSIGSHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders( osVerb, psExistingHeaders );
}


/************************************************************************/
/*                       VSIAzureFSHandler                              */
/************************************************************************/

class VSIAzureFSHandler CPL_FINAL : public IVSIS3LikeFSHandler
{
  protected:
    VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    CPLString GetURLFromDirname( const CPLString& osDirname ) override;

    IVSIS3LikeHandleHelper* CreateHandleHelper(
        const char* pszURI, bool bAllowNoObject) override;

    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool* pbGotFileList ) override;

    void InvalidateRecursive( const CPLString& osDirnameIn );

  public:
    VSIAzureFSHandler() {}
    ~VSIAzureFSHandler() override;

    CPLString GetFSPrefix() override { return "/vsiaz/"; }
    const char* GetDebugKey() const override { return "AZURE"; }

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    int Unlink( const char *pszFilename ) override;
    int Mkdir( const char *, long  ) override;
    int Rmdir( const char * ) override;

    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool bCacheResults,
                        bool* pbGotFileList );
};

/************************************************************************/
/*                          VSIAzureHandle                              */
/************************************************************************/

class VSIAzureHandle CPL_FINAL : public VSICurlHandle
{
    VSIAzureBlobHandleHelper* m_poHandleHelper;

  protected:
        virtual struct curl_slist* GetCurlHeaders( const CPLString& osVerb,
                    const struct curl_slist* psExistingHeaders ) override;
        virtual bool IsDirectoryFromExists( const char* pszVerb,
                                            int response_code ) override;

    public:
        VSIAzureHandle( VSIAzureFSHandler* poFS, const char* pszFilename,
                     VSIAzureBlobHandleHelper* poHandleHelper);
        virtual ~VSIAzureHandle();
};

/************************************************************************/
/*                        ~VSIAzureFSHandler()                          */
/************************************************************************/

VSIAzureFSHandler::~VSIAzureFSHandler()
{
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIAzureFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIAzureBlobHandleHelper* poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI( pszFilename + GetFSPrefix().size(),
                                         GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return nullptr;
    return new VSIAzureHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                          VSIAzureWriteHandle                         */
/************************************************************************/

class VSIAzureWriteHandle CPL_FINAL : public VSIVirtualHandle
{
    VSIAzureFSHandler  *m_poFS;
    CPLString           m_osFilename;
    VSIAzureBlobHandleHelper  *m_poHandleHelper;

    vsi_l_offset        m_nCurOffset;
    int                 m_nBufferOff;
    int                 m_nBufferSize;
    int                 m_nBufferOffReadCallback;
    bool                m_bClosed;
    GByte              *m_pabyBuffer;
    bool                m_bError;

    static size_t       ReadCallBackBuffer( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    bool                DoPUT(bool bBlockBob, bool bInitOnly);

    void                InvalidateParentDirectory();

    public:
        VSIAzureWriteHandle( VSIAzureFSHandler* poFS,
                          const char* pszFilename,
                          VSIAzureBlobHandleHelper* poHandleHelper );
        virtual ~VSIAzureWriteHandle();

        int Seek( vsi_l_offset nOffset, int nWhence ) override;
        vsi_l_offset Tell() override;
        size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
        size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
        int  Eof() override;
        int  Close() override;

        bool              IsOK() { return m_pabyBuffer != nullptr; }
};

/************************************************************************/
/*                       VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::VSIAzureWriteHandle( VSIAzureFSHandler* poFS,
                                    const char* pszFilename,
                                    VSIAzureBlobHandleHelper* poHandleHelper) :
        m_poFS(poFS),
        m_osFilename(pszFilename),
        m_poHandleHelper(poHandleHelper),
        m_nCurOffset(0),
        m_nBufferOff(0),
        m_nBufferSize(0),
        m_nBufferOffReadCallback(0),
        m_bClosed(false),
        m_pabyBuffer(nullptr),
        m_bError(false)
{
    int nChunkSizeMB = atoi(CPLGetConfigOption("VSIAZ_CHUNK_SIZE", "4"));
    if( nChunkSizeMB <= 0 || nChunkSizeMB > 4 )
        m_nBufferSize = 4 * 1024 * 1024;
    else
        m_nBufferSize = nChunkSizeMB * 1024 * 1024;

    // For testing only !
    const char* pszChunkSizeBytes =
        CPLGetConfigOption("VSIAZ_CHUNK_SIZE_BYTES", nullptr);
    if( pszChunkSizeBytes )
        m_nBufferSize = atoi(pszChunkSizeBytes);
    if( m_nBufferSize <= 0 || m_nBufferSize > 4 * 1024 * 1024 )
        m_nBufferSize = 4 * 1024 * 1024;

    m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
    if( m_pabyBuffer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot allocate working buffer for /vsiaz");
    }
}

/************************************************************************/
/*                      ~VSIAzureWriteHandle()                          */
/************************************************************************/

VSIAzureWriteHandle::~VSIAzureWriteHandle()
{
    Close();
    delete m_poHandleHelper;
    CPLFree(m_pabyBuffer);
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIAzureWriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( !((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable %s files",
                 m_poFS->GetFSPrefix().c_str());
        m_bError = true;
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIAzureWriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIAzureWriteHandle::Read( void * /* pBuffer */, size_t /* nSize */,
                               size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read not supported on writable %s files",
             m_poFS->GetFSPrefix().c_str());
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                         ReadCallBackBuffer()                         */
/************************************************************************/

size_t VSIAzureWriteHandle::ReadCallBackBuffer( char *buffer, size_t size,
                                             size_t nitems, void *instream )
{
    VSIAzureWriteHandle* poThis = static_cast<VSIAzureWriteHandle *>(instream);
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
VSIAzureWriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if( m_bError )
        return 0;

    size_t nBytesToWrite = nSize * nMemb;
    if( nBytesToWrite == 0 )
        return 0;

    const GByte* pabySrcBuffer = reinterpret_cast<const GByte*>(pBuffer);
    while( nBytesToWrite > 0 )
    {
        const int nToWriteInBuffer = static_cast<int>(
            std::min(static_cast<size_t>(m_nBufferSize - m_nBufferOff),
                     nBytesToWrite));
        memcpy(m_pabyBuffer + m_nBufferOff, pabySrcBuffer, nToWriteInBuffer);
        pabySrcBuffer += nToWriteInBuffer;
        m_nBufferOff += nToWriteInBuffer;
        m_nCurOffset += nToWriteInBuffer;
        nBytesToWrite -= nToWriteInBuffer;
        if( m_nBufferOff == m_nBufferSize )
        {
            if( m_nCurOffset == (vsi_l_offset)m_nBufferSize )
            {
                if( !DoPUT(false, true) )
                {
                    m_bError = true;
                    return 0;
                }
            }
            if( !DoPUT(false, false) )
            {
                m_bError = true;
                return 0;
            }
            m_nBufferOff = 0;
        }
    }
    return nMemb;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIAzureWriteHandle::Eof()
{
    return FALSE;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIAzureWriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(
        m_poHandleHelper->GetURL().c_str() );

    CPLString osFilenameWithoutSlash(m_osFilename);
    if( !osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/' )
        osFilenameWithoutSlash.resize( osFilenameWithoutSlash.size() - 1 );
    m_poFS->InvalidateDirContent( CPLGetDirname(osFilenameWithoutSlash) );
}

/************************************************************************/
/*                             DoPUT()                                  */
/************************************************************************/

bool VSIAzureWriteHandle::DoPUT(bool bBlockBob, bool bInitOnly)
{
    bool bSuccess = true;

    for( int i = 0; i < 2; i++ )
    {
        m_nBufferOffReadCallback = 0;
        CURL* hCurlHandle = curl_easy_init();

        m_poHandleHelper->ResetQueryParameters();
        if( !bBlockBob && !bInitOnly )
        {
            m_poHandleHelper->AddQueryParameter("comp", "appendblock");
        }

        curl_easy_setopt(hCurlHandle, CURLOPT_URL,
                            m_poHandleHelper->GetURL().c_str());
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);

        struct curl_slist* headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(hCurlHandle, nullptr));

        CPLString osContentLength; // leave it in this scope
        if( bBlockBob )
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);
            if( m_nBufferOff )
                headers = curl_slist_append(headers, "Expect: 100-continue");
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            headers = curl_slist_append(headers, "x-ms-blob-type: BlockBlob");
        }
        else if( bInitOnly )
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, 0);
            headers = curl_slist_append(headers, "Content-Length: 0");
            headers = curl_slist_append(headers, "x-ms-blob-type: AppendBlob");
        }
        else
        {
            curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);
            osContentLength.Printf("Content-Length: %d", m_nBufferOff);
            headers = curl_slist_append(headers, osContentLength.c_str());
            headers = curl_slist_append(headers, "x-ms-blob-type: AppendBlob");
        }

        headers = VSICurlMergeHeaders(headers,
                        m_poHandleHelper->GetCurlHeaders("PUT", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                            VSICurlHandleWriteFunc);

        void* old_handler = CPLHTTPIgnoreSigPipe();
        curl_easy_perform(hCurlHandle);
        CPLHTTPRestoreSigPipeHandler(old_handler);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

        bool bRetry = false;
        if( i == 0 && response_code == 409 )
        {
            CPLDebug(m_poFS->GetDebugKey(), "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer
                        : "(null)");

            // The blob type is invalid for this operation
            // Delete the file, and retry
            if( m_poFS->DeleteObject(m_osFilename) == 0 )
            {
                bRetry = true;
            }
        }
        else if( response_code != 201 )
        {
            CPLDebug(m_poFS->GetDebugKey(), "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer
                        : "(null)");
            CPLError(CE_Failure, CPLE_AppDefined,
                        "PUT of %s failed",
                        m_osFilename.c_str());
            bSuccess = false;
        }
        else
        {
            InvalidateParentDirectory();
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);

        if( !bRetry )
            break;
    }

    return bSuccess;
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIAzureWriteHandle::Close()
{
    int nRet = 0;
    if( !m_bClosed )
    {
        m_bClosed = true;
        if( m_nCurOffset < static_cast<vsi_l_offset>(m_nBufferSize) )
        {
            if( !m_bError && !DoPUT(true, false) )
                nRet = -1;
        }
        else
        {
            if( !m_bError && m_nBufferOff > 0 && !DoPUT(false, false) )
                nRet = -1;
        }
    }
    return nRet;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIAzureFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        /*if( strchr(pszAccess, '+') != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "w+ not supported for /vsis3. Only w");
            return NULL;
        }*/
        VSIAzureBlobHandleHelper* poHandleHelper =
            VSIAzureBlobHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str());
        if( poHandleHelper == nullptr )
            return nullptr;
        return new VSIAzureWriteHandle(this, pszFilename, poHandleHelper);
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIAzureFSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    CPLString osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());
    VSIAzureBlobHandleHelper* poHandleHelper =
        VSIAzureBlobHandleHelper::BuildFromURI( osDirnameWithoutPrefix, GetFSPrefix() );
    if( poHandleHelper == nullptr )
        return CPLString();
    CPLString osURL( poHandleHelper->GetURL() );
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIAzureFSHandler::CreateHandleHelper(const char* pszURI,
                                                           bool)
{
    return VSIAzureBlobHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str());
}

/************************************************************************/
/*                         InvalidateRecursive()                        */
/************************************************************************/

void VSIAzureFSHandler::InvalidateRecursive( const CPLString& osDirnameIn )
{
    // As Azure directories disappear as soon there is no remaining file
    // we may need to invalidate the whole hierarchy
    CPLString osDirname(osDirnameIn);
    while( osDirname.size() > GetFSPrefix().size() )
    {
        InvalidateDirContent(osDirname);
        InvalidateCachedData( GetURLFromDirname(osDirname) );
        osDirname = CPLGetDirname(osDirname);
    }
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIAzureFSHandler::Unlink( const char *pszFilename )
{
    int ret = IVSIS3LikeFSHandler::Unlink(pszFilename);
    if( ret != 0 )
        return ret;

    InvalidateRecursive(CPLGetDirname(pszFilename));
    return 0;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIAzureFSHandler::Mkdir( const char * pszDirname, long /* nMode */ )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) == 0 &&
        sStat.st_mode == S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "Directory %s already exists", osDirname.c_str());
        errno = EEXIST;
        return -1;
    }

    CPLString osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );
    InvalidateCachedData( GetURLFromDirname(osDirname) );
    InvalidateCachedData( GetURLFromDirname(osDirnameWithoutEndSlash) );
    InvalidateDirContent( CPLGetDirname(osDirnameWithoutEndSlash) );

    VSILFILE* fp = VSIFOpenL((osDirname + GDAL_MARKER_FOR_DIR).c_str(),
                             "wb");
    if( fp != nullptr )
    {
        CPLErrorReset();
        VSIFCloseL(fp);
        return CPLGetLastErrorType() == CPLE_None ? 0 : -1;
    }
    else
    {
        return -1;
    }
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIAzureFSHandler::Rmdir( const char * pszDirname )
{
    if( !STARTS_WITH_CI(pszDirname, GetFSPrefix()) )
        return -1;

    CPLString osDirname(pszDirname);
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";

    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) != 0 )
    {
        InvalidateCachedData(
            GetURLFromDirname(osDirname.substr(0, osDirname.size() - 1)) );
        CPLDebug(GetDebugKey(), "%s is not a object", pszDirname);
        errno = ENOENT;
        return -1;
    }
    else if( sStat.st_mode != S_IFDIR )
    {
        CPLDebug(GetDebugKey(), "%s is not a directory", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    char** papszFileList = ReadDirEx(osDirname, 1);
    bool bEmptyDir = (papszFileList != nullptr && EQUAL(papszFileList[0], ".") &&
                      papszFileList[1] == nullptr);
    CSLDestroy(papszFileList);
    if( !bEmptyDir )
    {
        CPLDebug(GetDebugKey(), "%s is not empty", pszDirname);
        errno = ENOTEMPTY;
        return -1;
    }

    CPLString osDirnameWithoutEndSlash(osDirname);
    osDirnameWithoutEndSlash.resize( osDirnameWithoutEndSlash.size() - 1 );
    InvalidateCachedData( GetURLFromDirname(osDirname) );
    InvalidateCachedData( GetURLFromDirname(osDirnameWithoutEndSlash) );
    InvalidateRecursive( CPLGetDirname(osDirnameWithoutEndSlash) );
    if( osDirnameWithoutEndSlash.find('/', GetFSPrefix().size()) ==
                                                        std::string::npos )
    {
        CPLDebug(GetDebugKey(), "%s is a container", pszDirname);
        errno = ENOTDIR;
        return -1;
    }

    return DeleteObject((osDirname + GDAL_MARKER_FOR_DIR).c_str());
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSIAzureFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    return GetFileList(pszDirname, nMaxFiles, true, pbGotFileList);
}


char** VSIAzureFSHandler::GetFileList( const char *pszDirname,
                                       int nMaxFiles,
                                       bool bCacheResults,
                                       bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug(GetDebugKey(), "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLString osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();
    if( !osDirnameWithoutPrefix.empty() &&
                                osDirnameWithoutPrefix.back() == '/' )
    {
        osDirnameWithoutPrefix.resize(osDirnameWithoutPrefix.size()-1);
    }

    CPLString osBucket(osDirnameWithoutPrefix);
    CPLString osObjectKey;
    size_t nSlashPos = osDirnameWithoutPrefix.find('/');
    if( nSlashPos != std::string::npos )
    {
        osBucket = osDirnameWithoutPrefix.substr(0, nSlashPos);
        osObjectKey = osDirnameWithoutPrefix.substr(nSlashPos+1);
    }

    IVSIS3LikeHandleHelper* poHandleHelper =
        CreateHandleHelper(osBucket, true);
    if( poHandleHelper == nullptr )
    {
        return nullptr;
    }

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    CPLString osMaxKeys = CPLGetConfigOption("AZURE_MAX_RESULTS", "");
    const int AZURE_SERVER_LIMIT_SINGLE_REQUEST = 5000;
    if( nMaxFiles > 0 && nMaxFiles < AZURE_SERVER_LIMIT_SINGLE_REQUEST &&
        (osMaxKeys.empty() || nMaxFiles < atoi(osMaxKeys)) )
    {
        osMaxKeys.Printf("%d", nMaxFiles);
    }

    while( true )
    {
        poHandleHelper->ResetQueryParameters();
        CPLString osBaseURL(poHandleHelper->GetURL());

        CURLM* hCurlMultiHandle = GetCurlMultiHandleFor(osBaseURL);
        CURL* hCurlHandle = curl_easy_init();

        poHandleHelper->AddQueryParameter("comp", "list");
        if( !osNextMarker.empty() )
            poHandleHelper->AddQueryParameter("marker", osNextMarker);
        if( !osMaxKeys.empty() )
             poHandleHelper->AddQueryParameter("maxresults", osMaxKeys);

        if( !osDirnameWithoutPrefix.empty() )
        {
            poHandleHelper->AddQueryParameter("restype", "container");

            poHandleHelper->AddQueryParameter("delimiter", "/");
            if( !osObjectKey.empty() )
                poHandleHelper->AddQueryParameter("prefix", osObjectKey + "/");
        }

        struct curl_slist* headers = 
            VSICurlSetOptions(hCurlHandle, poHandleHelper->GetURL(), nullptr);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr, nullptr);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                               poHandleHelper->GetCurlHeaders("GET", headers));
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        MultiPerform(hCurlMultiHandle, hCurlHandle);

        if( headers != nullptr )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == nullptr)
        {
            delete poHandleHelper;
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            CPLDebug(GetDebugKey(), "%s",
                        sWriteFuncData.pBuffer
                        ? sWriteFuncData.pBuffer : "(null)");
            CPLFree(sWriteFuncData.pBuffer);
            delete poHandleHelper;
            curl_easy_cleanup(hCurlHandle);
            return nullptr;
        }
        else
        {
            *pbGotFileList = true;
            bool bIsTruncated;
            AnalyseAzureFileList( osBaseURL,
                                  bCacheResults,
                                  sWriteFuncData.pBuffer,
                                  osFileList,
                                  nMaxFiles,
                                  bIsTruncated,
                                  osNextMarker );

            CPLFree(sWriteFuncData.pBuffer);

            if( osNextMarker.empty() )
            {
                delete poHandleHelper;
                curl_easy_cleanup(hCurlHandle);
                return osFileList.StealList();
            }
        }

        curl_easy_cleanup(hCurlHandle);
    }
}

/************************************************************************/
/*                           VSIAzureHandle()                           */
/************************************************************************/

VSIAzureHandle::VSIAzureHandle( VSIAzureFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIAzureBlobHandleHelper* poHandleHelper ) :
        VSICurlHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                          ~VSIAzureHandle()                           */
/************************************************************************/

VSIAzureHandle::~VSIAzureHandle()
{
    delete m_poHandleHelper;
}


/************************************************************************/
/*                          GetCurlHeaders()                            */
/************************************************************************/

struct curl_slist* VSIAzureHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders( osVerb, psExistingHeaders );
}

/************************************************************************/
/*                         IsDirectoryFromExists()                      */
/************************************************************************/

bool VSIAzureHandle::IsDirectoryFromExists( const char* /*pszVerb*/,
                                            int response_code )
{
    if( response_code != 404 )
        return false;

    CPLString osDirname(m_osFilename);
    if( osDirname.size() > poFS->GetFSPrefix().size() && osDirname.back() == '/' )
        osDirname.resize(osDirname.size() - 1);
    bool bIsDir;
    if( poFS->ExistsInCacheDirList(osDirname, &bIsDir) )
        return bIsDir;

    bool bGotFileList = false;
    char** papszDirContent = reinterpret_cast<VSIAzureFSHandler*>(poFS)
                        ->GetFileList( osDirname, 1, false, &bGotFileList );
    bIsDir = papszDirContent != nullptr && papszDirContent[0] != nullptr;
    CSLDestroy(papszDirContent);
    return bIsDir;
}


/************************************************************************/
/*                         VSIOSSFSHandler                              */
/************************************************************************/

class VSIOSSFSHandler CPL_FINAL : public IVSIS3LikeFSHandler
{
    std::map< CPLString, VSIOSSUpdateParams > oMapBucketsToOSSParams;

protected:
        VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
        CPLString GetURLFromDirname( const CPLString& osDirname ) override;

        const char* GetDebugKey() const override { return "OSS"; }

        IVSIS3LikeHandleHelper* CreateHandleHelper(
            const char* pszURI, bool bAllowNoObject) override;

        CPLString GetFSPrefix() override { return "/vsioss/"; }

        void ClearCache() override;

public:
        VSIOSSFSHandler() {}
        ~VSIOSSFSHandler() override;

        VSIVirtualHandle *Open( const char *pszFilename,
                                const char *pszAccess,
                                bool bSetError ) override;

        void UpdateMapFromHandle(
            IVSIS3LikeHandleHelper * poHandleHelper ) override;
        void UpdateHandleFromMap(
            IVSIS3LikeHandleHelper * poHandleHelper ) override;
};

/************************************************************************/
/*                            VSIOSSHandle                              */
/************************************************************************/

class VSIOSSHandle CPL_FINAL : public IVSIS3LikeHandle
{
    VSIOSSHandleHelper* m_poHandleHelper;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;
    bool CanRestartOnError( const char*, const char*, bool ) override;

  public:
    VSIOSSHandle( VSIOSSFSHandler* poFS,
                  const char* pszFilename,
                  VSIOSSHandleHelper* poHandleHelper );
    ~VSIOSSHandle() override;
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIOSSFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError)
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        /*if( strchr(pszAccess, '+') != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "w+ not supported for /vsioss. Only w");
            return NULL;
        }*/
        VSIOSSHandleHelper* poHandleHelper =
            VSIOSSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str(), false);
        if( poHandleHelper == nullptr )
            return nullptr;
        UpdateHandleFromMap(poHandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poHandleHelper, false);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = nullptr;
        }
        return poHandle;
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError);
}

/************************************************************************/
/*                       ~VSIOSSFSHandler()                             */
/************************************************************************/

VSIOSSFSHandler::~VSIOSSFSHandler()
{
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIOSSFSHandler::ClearCache()
{
    VSICurlFilesystemHandler::ClearCache();

    oMapBucketsToOSSParams.clear();
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIOSSFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIOSSHandleHelper* poHandleHelper =
        VSIOSSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false);
    if( poHandleHelper )
    {
        UpdateHandleFromMap(poHandleHelper);
        return new VSIOSSHandle(this, pszFilename, poHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIOSSFSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    CPLString osDirnameWithoutPrefix = osDirname.substr(GetFSPrefix().size());

    VSIOSSHandleHelper* poHandleHelper =
        VSIOSSHandleHelper::BuildFromURI(osDirnameWithoutPrefix,
                                        GetFSPrefix().c_str(), true);
    if( poHandleHelper == nullptr )
    {
        return "";
    }
    UpdateHandleFromMap(poHandleHelper);
    CPLString osBaseURL(poHandleHelper->GetURL());
    if( !osBaseURL.empty() && osBaseURL.back() == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poHandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIOSSFSHandler::CreateHandleHelper(const char* pszURI,
                                                          bool bAllowNoObject)
{
    return VSIOSSHandleHelper::BuildFromURI(
                                pszURI, GetFSPrefix().c_str(), bAllowNoObject);
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIOSSFSHandler::UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIOSSHandleHelper * poOSSHandleHelper =
        dynamic_cast<VSIOSSHandleHelper *>(poHandleHelper);
    CPLAssert( poOSSHandleHelper );
    if( !poOSSHandleHelper )
        return;
    oMapBucketsToOSSParams[ poOSSHandleHelper->GetBucket() ] =
        VSIOSSUpdateParams ( poOSSHandleHelper );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIOSSFSHandler::UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIOSSHandleHelper * poOSSHandleHelper =
        dynamic_cast<VSIOSSHandleHelper *>(poHandleHelper);
    CPLAssert( poOSSHandleHelper );
    if( !poOSSHandleHelper )
        return;
    std::map< CPLString, VSIOSSUpdateParams>::iterator oIter =
        oMapBucketsToOSSParams.find(poOSSHandleHelper->GetBucket());
    if( oIter != oMapBucketsToOSSParams.end() )
    {
        oIter->second.UpdateHandlerHelper(poOSSHandleHelper);
    }
}

/************************************************************************/
/*                            VSIOSSHandle()                            */
/************************************************************************/

VSIOSSHandle::VSIOSSHandle( VSIOSSFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIOSSHandleHelper* poHandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
        m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                            ~VSIOSSHandle()                           */
/************************************************************************/

VSIOSSHandle::~VSIOSSHandle()
{
    delete m_poHandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist* VSIOSSHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIOSSHandle::CanRestartOnError(const char* pszErrorMsg,
                                     const char* pszHeaders, bool bSetError)
{
    if( m_poHandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                            bSetError, nullptr) )
    {
        static_cast<VSIOSSFSHandler *>(poFS)->
            UpdateMapFromHandle(m_poHandleHelper);

        SetURL(m_poHandleHelper->GetURL());
        return true;
    }
    return false;
}



} /* end of anonymous namespace */

/************************************************************************/
/*                      VSICurlInstallReadCbk()                         */
/************************************************************************/

int VSICurlInstallReadCbk( VSILFILE* fp,
                           VSICurlReadCbkFunc pfnReadCbk,
                           void* pfnUserData,
                           int bStopOnInterruptUntilUninstall )
{
    return reinterpret_cast<VSICurlHandle *>(fp)->
        InstallReadCbk(pfnReadCbk, pfnUserData, bStopOnInterruptUntilUninstall);
}

/************************************************************************/
/*                    VSICurlUninstallReadCbk()                         */
/************************************************************************/

int VSICurlUninstallReadCbk( VSILFILE* fp )
{
    return reinterpret_cast<VSICurlHandle *>(fp)->UninstallReadCbk();
}

/************************************************************************/
/*                       VSICurlSetOptions()                            */
/************************************************************************/

struct curl_slist* VSICurlSetOptions(
                        CURL* hCurlHandle, const char* pszURL,
                        const char * const* papszOptions )
{
    curl_easy_setopt(hCurlHandle, CURLOPT_URL, pszURL);

    struct curl_slist* headers = static_cast<struct curl_slist*>(
        CPLHTTPSetOptions(hCurlHandle, papszOptions));

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
    DOWNLOAD_CHUNK_SIZE = atoi(
            CPLGetConfigOption("CPL_VSIL_CURL_CHUNK_SIZE", "16384"));
    if( DOWNLOAD_CHUNK_SIZE < 1024 || DOWNLOAD_CHUNK_SIZE > 10 * 1024* 1024 )
        DOWNLOAD_CHUNK_SIZE = 16384;

    GIntBig nCacheSize = CPLAtoGIntBig(
        CPLGetConfigOption("CPL_VSIL_CURL_CACHE_SIZE", "16384000"));
    if( nCacheSize < DOWNLOAD_CHUNK_SIZE ||
        nCacheSize / DOWNLOAD_CHUNK_SIZE > INT_MAX )
    {
        nCacheSize = 16384000;
    }
    N_MAX_REGIONS = std::max(1,
                        static_cast<int>(nCacheSize / DOWNLOAD_CHUNK_SIZE));

    VSIFilesystemHandler* poHandler = new VSICurlFilesystemHandler;
    VSIFileManager::InstallHandler( "/vsicurl/", poHandler );
    VSIFileManager::InstallHandler( "/vsicurl?", poHandler );
}

/************************************************************************/
/*                      VSIInstallS3FileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsis3/ Amazon S3 file system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsis3">/vsis3/ documentation</a>
 *
 * @since GDAL 2.1
 */
void VSIInstallS3FileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsis3/", new VSIS3FSHandler );
}

/************************************************************************/
/*                      VSIInstallGSFileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsigs/ Google Cloud Storage file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsigs">/vsigs/ documentation</a>
 *
 * @since GDAL 2.2
 */

void VSIInstallGSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsigs/", new VSIGSFSHandler );
}

/************************************************************************/
/*                      VSIInstallAzureFileHandler()                    */
/************************************************************************/

/**
 * \brief Install /vsiaz/ Microsoft Azure Blob file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsiaz">/vsiaz/ documentation</a>
 *
 * @since GDAL 2.3
 */

void VSIInstallAzureFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsiaz/", new VSIAzureFSHandler );
}

/************************************************************************/
/*                      VSIInstallOSSFileHandler()                      */
/************************************************************************/

/**
 * \brief Install /vsioss/ Alibaba Cloud Object Storage Service (OSS) file
 * system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsioss">/vsioss/ documentation</a>
 *
 * @since GDAL 2.3
 */
void VSIInstallOSSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsioss/", new VSIOSSFSHandler );
}

/************************************************************************/
/*                         VSICurlClearCache()                          */
/************************************************************************/

/**
 * \brief Clean local cache associated with /vsicurl/ (and related file systems)
 *
 * /vsicurl (and related file systems like /vsis3/ , /vsigs/) cache a number of
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
    const char* const apszFS[] = { "/vsicurl/", "/vsis3/", "/vsigs/",
                                   "/vsiaz/", "/vsioss/" };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszFS); ++i )
    {
        VSICurlFilesystemHandler *poFSHandler =
            dynamic_cast<VSICurlFilesystemHandler*>(
                VSIFileManager::GetHandler( apszFS[i] ));

        if( poFSHandler )
            poFSHandler->ClearCache();
    }

    VSICurlStreamingClearCache();
}

#endif /* HAVE_CURL */
