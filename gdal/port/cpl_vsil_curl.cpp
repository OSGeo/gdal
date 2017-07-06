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

#include "cpl_aws.h"
#include "cpl_google_cloud.h"
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

struct curl_slist* VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL,
                       const char * const* papszOptions);
struct curl_slist* VSICurlMergeHeaders( struct curl_slist* poDest,
                                        struct curl_slist* poSrcToDestroy );

#include <map>

#define ENABLE_DEBUG 1

static const int N_MAX_REGIONS = 1000;
static const int DOWNLOAD_CHUNK_SIZE = 16384;

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
    bool            bS3Redirect;
    time_t          nExpireTimestampLocal;
    CPLString       osRedirectURL;

                    CachedFileProp() :
                        eExists(EXIST_UNKNOWN),
                        bHasComputedFileSize(false),
                        fileSize(0),
                        bIsDirectory(false),
                        mTime(0),
                        bS3Redirect(false),
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
    if( papszList == NULL )
        return -1;

    for( int i = 0; papszList[i] != NULL; i++ )
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
    CPLString       osURL;
    CURL           *hCurlHandle;
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

public:
    VSICurlFilesystemHandler();
    virtual ~VSICurlFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;

    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;
    virtual int      Unlink( const char *pszFilename ) override;
    virtual int      Rename( const char *oldpath, const char *newpath )
        override;
    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual int      Rmdir( const char *pszDirname ) override;
    virtual char   **ReadDirEx( const char *pszDirname, int nMaxFiles )
        override;
            char   **ReadDirInternal( const char *pszDirname, int nMaxFiles,
                                      bool* pbGotFileList );
            void     InvalidateDirContent( const char *pszDirname );

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

    CURL               *GetCurlHandleFor( CPLString osURL );

    virtual void        ClearCache();
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

    bool                m_bS3Redirect;
    time_t              m_nExpireTimestampLocal;
    CPLString           m_osRedirectURL;

    int                 m_nMaxRetry;
    double              m_dfRetryDelay;
    bool                m_bUseHead;

  protected:
    virtual struct curl_slist* GetCurlHeaders( const CPLString& )
        { return NULL; }
    bool CanRestartOnError( const char* pszErrorMsg )
        { return CanRestartOnError(pszErrorMsg, false); }
    virtual bool CanRestartOnError( const char*, bool ) { return false; }
    virtual bool UseLimitRangeGetInsteadOfHead() { return false; }
    virtual void ProcessGetFileSizeResult(const char* /* pszContent */ ) {}
    void SetURL(const char* pszURL);

  public:

    VSICurlHandle( VSICurlFilesystemHandler* poFS,
                   const char* pszFilename,
                   const char* pszURLIn = NULL );
    virtual ~VSICurlHandle();

    virtual int          Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t       Read( void *pBuffer, size_t nSize, size_t nMemb )
        override;
    virtual int          ReadMultiRange( int nRanges, void ** ppData,
                                         const vsi_l_offset* panOffsets,
                                         const size_t* panSizes ) override;
    virtual size_t       Write( const void *pBuffer, size_t nSize,
                                size_t nMemb ) override;
    virtual int          Eof() override;
    virtual int          Flush() override;
    virtual int          Close() override;

    bool                 IsKnownFileSize() const
        { return bHasComputedFileSize; }
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
    if( !STARTS_WITH(pszFilename, "/vsicurl/") )
        return pszFilename;
    pszFilename += strlen("/vsicurl/");
    if( !STARTS_WITH(pszFilename, "http://") &&
        !STARTS_WITH(pszFilename, "https://") &&
        !STARTS_WITH(pszFilename, "ftp://") &&
        !STARTS_WITH(pszFilename, "file://") )
    {
        const char* pszURLArg = strstr(pszFilename, ",url=");
        if( pszURLArg )
        {
            CPLString osOptions( pszFilename );
            osOptions.resize( pszURLArg - pszFilename );
            char** papszTokens = CSLTokenizeString2( osOptions, ",", 0 );
            for( int i = 0; papszTokens && papszTokens[i]; i++ )
            {
                char* pszKey = NULL;
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
                             EQUAL(pszKey, "timeout") ||
                             EQUAL(pszKey, "connecttimeout") ||
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
                    else
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                    "Unsupported option: %s", pszKey);
                    }
                }
                CPLFree(pszKey);
            }
            CSLDestroy(papszTokens);
            return pszURLArg + strlen(",url=");
        }
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
    pfnReadCbk(NULL),
    pReadCbkUserData(NULL),
    bStopOnInterruptUntilUninstall(false),
    bInterrupted(false),
    m_bS3Redirect(false),
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
                                                       NULL, NULL,
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
    if( pfnReadCbk != NULL )
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
    if( pfnReadCbk == NULL )
        return FALSE;

    pfnReadCbk = NULL;
    pReadCbkUserData = NULL;
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
    psStruct->pBuffer = NULL;
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
            if( STARTS_WITH_CI(pszLine, "HTTP/1.0 ") ||
                STARTS_WITH_CI(pszLine, "HTTP/1.1 ") )
            {
                psStruct->nHTTPCode = atoi(pszLine + 9);
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
/*                       VSICurlIsS3SignedURL()                         */
/************************************************************************/

static bool VSICurlIsS3SignedURL( const char* pszURL )
{
    return
        strstr(pszURL, ".s3.amazonaws.com/") != NULL &&
        (strstr(pszURL, "&Signature=") != NULL ||
         strstr(pszURL, "?Signature=") != NULL);
}

/************************************************************************/
/*                      VSICurlGetExpiresFromS3SigneURL()               */
/************************************************************************/

static GIntBig VSICurlGetExpiresFromS3SigneURL( const char* pszURL )
{
    const char* pszExpires = strstr(pszURL, "&Expires=");
    if( pszExpires == NULL )
        pszExpires = strstr(pszURL, "?Expires=");
    if( pszExpires == NULL )
        return 0;
    return CPLAtoGIntBig(pszExpires + strlen("&Expires="));
}

/************************************************************************/
/*                           GetFileSize()                              */
/************************************************************************/

vsi_l_offset VSICurlHandle::GetFileSize( bool bSetError )
{
    if( bHasComputedFileSize )
        return fileSize;

    bHasComputedFileSize = true;

#if LIBCURL_VERSION_NUM < 0x070B00
    // Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have
    // been previously set, so we have to reinit the connection handle.
    poFS->GetCurlHandleFor("");
#endif
    CURL* hCurlHandle = poFS->GetCurlHandleFor(m_pszURL);
    CPLString osURL(m_pszURL);
    bool bRetryWithGet = false;
    bool bS3Redirect = false;
    int nRetryCount = 0;

retry:
    struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osURL, m_papszHTTPOptions);

    // We need that otherwise OSGEO4W's libcurl issue a dummy range request
    // when doing a HEAD when recycling connections.
    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

    WriteFuncStruct sWriteFuncHeaderData;
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);

    CPLString osVerb;
    if( UseLimitRangeGetInsteadOfHead() )
    {
        osVerb = "GET";

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, "0-4095");
    }
    // HACK for mbtiles driver: http://a.tiles.mapbox.com/v3/ doesn't accept
    // HEAD, as it is a redirect to AWS S3 signed URL, but those are only valid
    // for a given type of HTTP request, and thus GET. This is valid for any
    // signed URL for AWS S3.
    else if( strstr(osURL, ".tiles.mapbox.com/") != NULL ||
             VSICurlIsS3SignedURL(osURL) ||
             !m_bUseHead )
    {
        sWriteFuncHeaderData.bDownloadHeaderOnly = true;
        osVerb = "GET";
    }
    else
    {
        sWriteFuncHeaderData.bDownloadHeaderOnly = true;
        curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 1);
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 0);
        curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 1);
        osVerb = "HEAD";
    }

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);
    sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(osURL, "http");

    // Bug with older curl versions (<=7.16.4) and FTP.
    // See http://curl.haxx.se/mail/lib-2007-08/0312.html
    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders(osVerb));
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_FILETIME, 1);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    eExists = EXIST_UNKNOWN;

    long mtime = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_FILETIME, &mtime);

    if( STARTS_WITH(osURL, "ftp") )
    {
        if( sWriteFuncData.pBuffer != NULL &&
            STARTS_WITH(sWriteFuncData.pBuffer, "Content-Length: ") )
        {
            const char* pszBuffer =
                sWriteFuncData.pBuffer + strlen("Content-Length: ");
            eExists = EXIST_YES;
            fileSize = CPLScanUIntBig(
                pszBuffer,
                static_cast<int>(sWriteFuncData.nSize -
                                 strlen("Content-Length: ")));
            if( ENABLE_DEBUG )
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                         osURL.c_str(), fileSize);
        }
    }

    double dfSize = 0;
    if( eExists != EXIST_YES )
    {
        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

        char *pszEffectiveURL = NULL;
        curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL,
                          &pszEffectiveURL);
        if( pszEffectiveURL != NULL && strstr(pszEffectiveURL, osURL) == NULL )
        {
            CPLDebug("VSICURL", "Effective URL: %s", pszEffectiveURL);

            // Is this is a redirect to a S3 URL?
            if( VSICurlIsS3SignedURL(pszEffectiveURL) &&
                !VSICurlIsS3SignedURL(osURL) )
            {
                // Note that this is a redirect as we won't notice after the
                // retry.
                bS3Redirect = true;

                if( !bRetryWithGet && osVerb == "HEAD" && response_code == 403 )
                {
                    CPLDebug("VSICURL",
                             "Redirected to a AWS S3 signed URL. Retrying "
                             "with GET request instead of HEAD since the URL "
                             "might be valid only for GET");
                    bRetryWithGet = true;
                    osURL = pszEffectiveURL;
                    CPLFree(sWriteFuncData.pBuffer);
                    CPLFree(sWriteFuncHeaderData.pBuffer);
                    goto retry;
                }
            }
        }

        if( bS3Redirect && response_code >= 200 && response_code < 300 &&
            sWriteFuncHeaderData.nTimestampDate > 0 &&
            pszEffectiveURL != NULL &&
            CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_S3_REDIRECT",
                                           "TRUE")) )
        {
            const GIntBig nExpireTimestamp =
                VSICurlGetExpiresFromS3SigneURL(pszEffectiveURL);
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
                m_bS3Redirect = true;
                m_nExpireTimestampLocal = time(NULL) + nValidity;
                m_osRedirectURL = pszEffectiveURL;
                CachedFileProp* cachedFileProp =
                    poFS->GetCachedFileProp(m_pszURL);
                cachedFileProp->bS3Redirect = m_bS3Redirect;
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
            if( sWriteFuncHeaderData.pBuffer != NULL )
            {
                const char* pszContentRange =
                    strstr(sWriteFuncHeaderData.pBuffer,
                           "Content-Range: bytes ");
                if( pszContentRange )
                    pszContentRange = strchr(pszContentRange, '/');
                if( pszContentRange )
                {
                    eExists = EXIST_YES;
                    fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(pszContentRange + 1));
                }
            }
        }
        else if( response_code != 200 )
        {
            // If HTTP 502, 503 or 504 gateway timeout error retry after a
            // pause.
            if( (response_code >= 502 && response_code <= 504) &&
                nRetryCount < m_nMaxRetry )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "HTTP error code: %d - %s. "
                            "Retrying again in %.1f secs",
                            static_cast<int>(response_code), m_pszURL,
                            m_dfRetryDelay);
                CPLSleep(m_dfRetryDelay);
                nRetryCount++;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
                goto retry;
            }

            if( UseLimitRangeGetInsteadOfHead() &&
                sWriteFuncData.pBuffer != NULL &&
                CanRestartOnError(sWriteFuncData.pBuffer,
                                  bSetError) )
            {
                bHasComputedFileSize = false;
                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(sWriteFuncHeaderData.pBuffer);
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
        else if( sWriteFuncData.pBuffer != NULL )
        {
            ProcessGetFileSizeResult( (const char*)sWriteFuncData.pBuffer );
        }

        // Try to guess if this is a directory. Generally if this is a
        // directory, curl will retry with an URL with slash added.
        if( pszEffectiveURL != NULL &&
            strncmp(osURL, pszEffectiveURL, osURL.size()) == 0 &&
            pszEffectiveURL[osURL.size()] == '/' )
        {
            eExists = EXIST_YES;
            fileSize = 0;
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
    if( cachedFileProp->bS3Redirect )
    {
        m_bS3Redirect = cachedFileProp->bS3Redirect;
        m_nExpireTimestampLocal = cachedFileProp->nExpireTimestampLocal;
        m_osRedirectURL = cachedFileProp->osRedirectURL;
    }

    CURL* hCurlHandle = poFS->GetCurlHandleFor(m_pszURL);

    CPLString osURL(m_pszURL);
    bool bUsedRedirect = false;
    if( m_bS3Redirect )
    {
        if( time(NULL) + 1 < m_nExpireTimestampLocal )
        {
            CPLDebug("VSICURL",
                     "Using redirect URL as it looks to be still valid "
                     "(%d seconds left)",
                     static_cast<int>(m_nExpireTimestampLocal - time(NULL)));
            osURL = m_osRedirectURL;
            bUsedRedirect = true;
        }
        else
        {
            CPLDebug("VSICURL", "Redirect URL has expired. Using original URL");
            m_bS3Redirect = false;
            cachedFileProp->bS3Redirect = false;
        }
    }

    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;
    int nRetryCount = 0;

retry:
    struct curl_slist* headers =
        VSICurlSetOptions(hCurlHandle, osURL, m_papszHTTPOptions);

    VSICURLInitWriteFuncStruct(&sWriteFuncData,
                               reinterpret_cast<VSILFILE *>(this),
                               pfnReadCbk, pReadCbkUserData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
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

    curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, rangeStr);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET"));
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_FILETIME, 1);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    if( sWriteFuncData.bInterrupted )
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        return false;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = NULL;
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
        m_bS3Redirect = false;
        cachedFileProp->bS3Redirect = false;
        bUsedRedirect = false;
        osURL = m_pszURL;
        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);
        goto retry;
    }

    char *pszEffectiveURL = NULL;
    curl_easy_getinfo(hCurlHandle, CURLINFO_EFFECTIVE_URL, &pszEffectiveURL);
    if( !m_bS3Redirect && pszEffectiveURL != NULL &&
        strstr(pszEffectiveURL, m_pszURL) == NULL )
    {
        CPLDebug("VSICURL", "Effective URL: %s", pszEffectiveURL);
        if( response_code >= 200 && response_code < 300 &&
            sWriteFuncHeaderData.nTimestampDate > 0 &&
            VSICurlIsS3SignedURL(pszEffectiveURL) &&
            !VSICurlIsS3SignedURL(m_pszURL) &&
            CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_USE_S3_REDIRECT",
                                           "TRUE")) )
        {
            GIntBig nExpireTimestamp =
                VSICurlGetExpiresFromS3SigneURL(pszEffectiveURL);
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
                m_bS3Redirect = true;
                m_nExpireTimestampLocal = time(NULL) + nValidity;
                m_osRedirectURL = pszEffectiveURL;
                cachedFileProp->bS3Redirect = m_bS3Redirect;
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
        if( sWriteFuncData.pBuffer != NULL &&
            CanRestartOnError((const char*)sWriteFuncData.pBuffer) )
        {
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
            return DownloadRegion(startOffset, nBlocks);
        }

        // If HTTP 502, 503 or 504 gateway timeout error retry after a
        // pause.
        if( (response_code >= 502 && response_code <= 504) &&
            nRetryCount < m_nMaxRetry )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "HTTP error code: %d - %s. "
                        "Retrying again in %.1f secs",
                        static_cast<int>(response_code), m_pszURL,
                        m_dfRetryDelay);
            CPLSleep(m_dfRetryDelay);
            nRetryCount++;
            CPLFree(sWriteFuncData.pBuffer);
            CPLFree(sWriteFuncHeaderData.pBuffer);
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
        return false;
    }

    if( !bHasComputedFileSize && sWriteFuncHeaderData.pBuffer )
    {
        // Try to retrieve the filesize from the HTTP headers
        // if in the form: "Content-Range: bytes x-y/filesize".
        char* pszContentRange =
            strstr(sWriteFuncHeaderData.pBuffer, "Content-Range: bytes ");
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
        if( psRegion == NULL )
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
                        nOffsetToDownload + i * DOWNLOAD_CHUNK_SIZE) != NULL )
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
        if( psRegion == NULL || psRegion->pData == NULL )
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

    CURL* hCurlHandle = poFS->GetCurlHandleFor(m_pszURL);
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

    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
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

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET"));
    if( headers != NULL )
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    curl_easy_perform(hCurlHandle);

    if( headers != NULL )
        curl_slist_free_all(headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    if( sWriteFuncData.bInterrupted )
    {
        bInterrupted = true;

        CPLFree(sWriteFuncData.pBuffer);
        CPLFree(sWriteFuncHeaderData.pBuffer);

        return -1;
    }

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);

    char *content_type = NULL;
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
        return -1;
    }

    char* pBuffer = sWriteFuncData.pBuffer;
    size_t nSize = sWriteFuncData.nSize;

    // TODO(schwehr): Localize after removing gotos.
    int nRet = -1;
    char* pszBoundary;
    CPLString osBoundary;
    char *pszNext = NULL;
    int iRange = 0;
    int iPart = 0;
    char* pszEOL = NULL;

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
    if( pszBoundary == NULL )
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
    if( pszNext == NULL )
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

            if( pszEOL == NULL )
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
    hMutex = NULL;
    papsRegions = NULL;
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

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
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
/*                      GetCurlHandleFor()                              */
/************************************************************************/

CURL* VSICurlFilesystemHandler::GetCurlHandleFor(CPLString osURL)
{
    CPLMutexHolder oHolder( &hMutex );

    std::map<GIntBig, CachedConnection*>::const_iterator iterConnections =
        mapConnections.find(CPLGetPID());
    if( iterConnections == mapConnections.end() )
    {
        CURL* hCurlHandle = curl_easy_init();
        CachedConnection* psCachedConnection = new CachedConnection;
        psCachedConnection->osURL = osURL;
        psCachedConnection->hCurlHandle = hCurlHandle;
        mapConnections[CPLGetPID()] = psCachedConnection;
        return hCurlHandle;
    }

    CachedConnection* psCachedConnection = iterConnections->second;
    if( osURL == psCachedConnection->osURL )
        return psCachedConnection->hCurlHandle;

    const char* pszURL = osURL.c_str();
    const char* pszEndOfServ = strchr(pszURL, '.');
    if( pszEndOfServ != NULL )
        pszEndOfServ = strchr(pszEndOfServ, '/');
    if( pszEndOfServ == NULL )
        pszURL = pszURL + strlen(pszURL);
    const bool bReinitConnection =
        strncmp(psCachedConnection->osURL, pszURL, pszEndOfServ-pszURL) != 0;

    if( bReinitConnection )
    {
        if( psCachedConnection->hCurlHandle )
            curl_easy_cleanup(psCachedConnection->hCurlHandle);
        psCachedConnection->hCurlHandle = curl_easy_init();
    }
    psCachedConnection->osURL = osURL;

    return psCachedConnection->hCurlHandle;
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
                    AddRegion(pszURL, nFileOffsetStart, 0, NULL);
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
    return NULL;
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
    return NULL;
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

    CachedRegion* psRegion = NULL;
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
    psRegion->pData = nSize ? static_cast<char *>(CPLMalloc(nSize)) : NULL;
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
    if( cachedFileProp == NULL )
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
    papsRegions = NULL;

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
        curl_easy_cleanup(iterConnections->second->hCurlHandle);
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
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_FILENAME", NULL);
    if( pszAllowedFilename != NULL )
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
        CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", NULL);
    if( pszAllowedExtensions )
    {
        char** papszExtensions =
            CSLTokenizeString2( pszAllowedExtensions, ", ", 0 );
        const size_t nURLLen = strlen(pszFilename);
        bool bFound = false;
        for( int i = 0; papszExtensions[i] != NULL; i++ )
        {
            const size_t nExtensionLen = strlen(papszExtensions[i]);
            if( EQUAL(papszExtensions[i], "{noext}") )
            {
                const char* pszLastSlash = strrchr(pszFilename, '/');
                if( pszLastSlash != NULL && strchr(pszLastSlash, '.') == NULL )
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
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return NULL;

    if( strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsicurl");
        return NULL;
    }
    if( !IsAllowedFilename( pszFilename ) )
        return NULL;

    bool bListDir = true;
    bool bEmptyDir = false;
    CPLString osURL(
        VSICurlGetURLFromFilename(pszFilename, NULL, NULL, NULL,
                                  &bListDir, &bEmptyDir, NULL));

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
    if( !(cachedFileProp != NULL && cachedFileProp->eExists == EXIST_YES) &&
        strchr(CPLGetFilename(osFilename), '.') != NULL &&
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
                return NULL;
            }
        }
        CSLDestroy(papszFileList);
    }

    VSICurlHandle* poHandle =
        CreateFileHandle(osFilename);
    if( poHandle == NULL )
        return NULL;
    if( !bGotFileList || bForceExistsCheck )
    {
        // If we didn't get a filelist, check that the file really exists.
        if( !poHandle->Exists(bSetError) )
        {
            delete poHandle;
            return NULL;
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
        return NULL;

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

    CPLString osURL(VSICurlGetURLFromFilename(pszFilename, NULL, NULL, NULL,
                                              NULL, NULL, NULL));
    const char* pszDir = strchr(osURL.c_str(), '/');
    if( pszDir == NULL )
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
        char* pszUnescapedDir = CPLUnescapeString(pszDir, NULL, CPLES_URL);
        osExpectedString_unescaped = "<title>Index of ";
        osExpectedString_unescaped += pszUnescapedDir;
        osExpectedString_unescaped += "</title>";
        CPLFree(pszUnescapedDir);
    }

    char* c = NULL;
    int nCount = 0;
    int nCountTable = 0;
    CPLStringList oFileList;
    char* pszLine = pszData;
    bool bIsHTMLDirList = false;

    while( (c = VSICurlParserFindEOL( pszLine )) != NULL )
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
                return NULL;
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
            if( pszSubDir == NULL )
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
                 (strstr(pszLine, "<a href=\"") != NULL ||
                  strstr(pszLine, "<A HREF=\"") != NULL) &&
                 // Exclude absolute links, like to subversion home.
                 strstr(pszLine, "<a href=\"http://") == NULL &&
                 // exclude parent directory.
                 strstr(pszLine, "Parent Directory") == NULL )
        {
            char *beginFilename = strstr(pszLine, "<a href=\"");
            if( beginFilename == NULL )
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
    if( psTree == NULL )
        return;
    CPLXMLNode* psListBucketResult = CPLGetXMLNode(psTree, "=ListBucketResult");
    if( psListBucketResult )
    {
        CPLString osPrefix = CPLGetXMLValue(psListBucketResult, "Prefix", "");
        CPLXMLNode* psIter = psListBucketResult->psChild;
        for( ; psIter != NULL; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( strcmp(psIter->pszValue, "Contents") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Key", NULL);
                if( pszKey && strlen(pszKey) > osPrefix.size() )
                {
                    CPLString osCachedFilename = osBaseURL + pszKey;
#if DEBUG_VERBOSE
                    CPLDebug("S3", "Cache %s", osCachedFilename.c_str());
#endif

                    CachedFileProp* cachedFileProp =
                        GetCachedFileProp(osCachedFilename);
                    cachedFileProp->eExists = EXIST_YES;
                    cachedFileProp->bHasComputedFileSize = true;
                    cachedFileProp->fileSize = static_cast<GUIntBig>(
                        CPLAtoGIntBig(CPLGetXMLValue(psIter, "Size", "0")));
                    cachedFileProp->bIsDirectory = false;
                    cachedFileProp->mTime = 0;

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
                        cachedFileProp->mTime =
                            static_cast<time_t>(
                                CPLYMDHMSToUnixTime(&brokendowntime));
                    }

                    osFileList.AddString(pszKey + osPrefix.size());
                }
            }
            else if( strcmp(psIter->pszValue, "CommonPrefixes") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(psIter, "Prefix", NULL);
                if( pszKey && strncmp(pszKey, osPrefix, osPrefix.size()) == 0 )
                {
                    CPLString osKey = pszKey;
                    if( !osKey.empty() && osKey.back() == '/' )
                        osKey.resize(osKey.size()-1);
                    if( osKey.size() > osPrefix.size() )
                    {
                        CPLString osCachedFilename = osBaseURL + osKey;
#if DEBUG_VERBOSE
                        CPLDebug("S3", "Cache %s", osCachedFilename.c_str());
#endif

                        CachedFileProp* cachedFileProp =
                            GetCachedFileProp(osCachedFilename);
                        cachedFileProp->eExists = EXIST_YES;
                        cachedFileProp->bIsDirectory = true;
                        cachedFileProp->mTime = 0;

                        osFileList.AddString(osKey.c_str() + osPrefix.size());
                    }
                }
            }

            if( nMaxFiles > 0 && osFileList.Count() > nMaxFiles )
                break;
        }

        if( !(nMaxFiles > 0 && osFileList.Count() > nMaxFiles) )
        {
            osNextMarker = CPLGetXMLValue(psListBucketResult, "NextMarker", "");
            bIsTruncated =
                CPLTestBool(CPLGetXMLValue(psListBucketResult,
                                           "IsTruncated", "false"));
        }
    }
    CPLDestroyXMLNode(psTree);
}

/************************************************************************/
/*                         VSICurlGetToken()                            */
/************************************************************************/

static char* VSICurlGetToken( char* pszCurPtr, char** ppszNextToken )
{
    if( pszCurPtr == NULL )
        return NULL;

    while( (*pszCurPtr) == ' ' )
        pszCurPtr++;
    if( *pszCurPtr == '\0' )
        return NULL;

    char* pszToken = pszCurPtr;
    while( (*pszCurPtr) != ' ' && (*pszCurPtr) != '\0' )
        pszCurPtr++;
    if( *pszCurPtr == '\0' )
    {
        *ppszNextToken = NULL;
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
    if( pszPermissions == NULL || strlen(pszPermissions) != 10 )
        return false;
    bIsDirectory = pszPermissions[0] == 'd';

    for( int i = 0; i < 3; i++ )
    {
        if( VSICurlGetToken(pszNextToken, &pszNextToken) == NULL )
            return false;
    }

    char* pszSize = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszSize == NULL )
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
    if( pszMonth == NULL || strlen(pszMonth) != 3 )
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
    if( pszDay == NULL || (strlen(pszDay) != 1 && strlen(pszDay) != 2) )
        return false;
    int nDay = atoi(pszDay);
    if( nDay >= 1 && nDay <= 31 )
        brokendowntime.tm_mday = nDay;
    else
        bBrokenDownTimeValid = false;

    char* pszHourOrYear = VSICurlGetToken(pszNextToken, &pszNextToken);
    if( pszHourOrYear == NULL ||
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

    if( pszNextToken == NULL )
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
    return VSICurlGetURLFromFilename(osDirname, NULL, NULL, NULL, NULL, NULL, NULL);
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
        VSICurlGetURLFromFilename(pszDirname, NULL, NULL, NULL,
                                  &bListDir, &bEmptyDir, NULL));
    if( bEmptyDir )
    {
        *pbGotFileList = true;
        return CSLAddString(NULL, ".");
    }
    if( !bListDir )
        return NULL;

    // HACK (optimization in fact) for MBTiles driver.
    if( strstr(pszDirname, ".tiles.mapbox.com") != NULL )
        return NULL;

    if( STARTS_WITH(osURL, "ftp://") )
    {
        WriteFuncStruct sWriteFuncData;
        sWriteFuncData.pBuffer = NULL;

        CPLString osDirname(osURL);
        osDirname += '/';

        char** papszFileList = NULL;

        for( int iTry = 0; iTry < 2; iTry++ )
        {
            CURL* hCurlHandle = GetCurlHandleFor(osDirname);
            struct curl_slist* headers =
                VSICurlSetOptions(hCurlHandle, osDirname.c_str(), NULL);

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

            VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
            curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                             VSICurlHandleWriteFunc);

            char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
            curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

            if( headers != NULL )
                curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

            curl_easy_perform(hCurlHandle);

            if( headers != NULL )
                curl_slist_free_all(headers);

            if( sWriteFuncData.pBuffer == NULL )
                return NULL;

            char* pszLine = sWriteFuncData.pBuffer;
            char* c = NULL;
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

                while( (c = strchr(pszLine, '\n')) != NULL)
                {
                    *c = 0;
                    if( c - pszLine > 0 && c[-1] == '\r' )
                        c[-1] = 0;

                    char* pszFilename = NULL;
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

                if( c == NULL )
                {
                    papszFileList = oFileList.StealList();
                    break;
                }
            }
            else
            {
                CPLStringList oFileList;
                *pbGotFileList = true;

                while( (c = strchr(pszLine, '\n')) != NULL)
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
            sWriteFuncData.pBuffer = NULL;
        }

        CPLFree(sWriteFuncData.pBuffer);

        return papszFileList;
    }

    // Try to recognize HTML pages that list the content of a directory.
    // Currently this supports what Apache and shttpd can return.
    else if( STARTS_WITH(osURL, "http://") ||
             STARTS_WITH(osURL, "https://") )
    {
        CPLString osDirname(osURL);
        osDirname += '/';

#if LIBCURL_VERSION_NUM < 0x070B00
        // Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have
        // been previously set, so we have to reinit the connection handle.
        GetCurlHandleFor("");
#endif

        CURL* hCurlHandle = GetCurlHandleFor(osDirname);
        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osDirname.c_str(), NULL);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        if( headers != NULL )
            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        curl_easy_perform(hCurlHandle);

        if( headers != NULL )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == NULL )
            return NULL;

        char** papszFileList = NULL;
        if( STARTS_WITH_CI(sWriteFuncData.pBuffer, "<?xml") &&
            strstr(sWriteFuncData.pBuffer, "<ListBucketResult") != NULL )
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
        return papszFileList;
    }

    return NULL;
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
        VSICurlGetURLFromFilename(pszFilename, NULL, NULL, NULL,
                                  &bListDir, &bEmptyDir, NULL));

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
    else if( strchr(CPLGetFilename(osFilename), '.') != NULL &&
             !STARTS_WITH_CI(CPLGetExtension(osFilename), "zip") &&
             strstr(osFilename, ".zip.") != NULL &&
             strstr(osFilename, ".ZIP.") != NULL &&
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
    if( poHandle == NULL )
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
    while( osDirname.back() == '/' )
        osDirname.erase(osDirname.size() - 1);

    const char* pszUpDir = strstr(osDirname, "/..");
    if( pszUpDir != NULL )
    {
        int pos = static_cast<int>(pszUpDir - osDirname.c_str() - 1);
        while( pos >= 0 && osDirname[pos] != '/' )
            pos--;
        if( pos >= 1 )
        {
            osDirname = osDirname.substr(0, pos) + CPLString(pszUpDir + 3);
        }
    }

    if( osDirname.size() <= GetFSPrefix().size() )
    {
        if( pbGotFileList )
            *pbGotFileList = true;
        return NULL;
    }

    CPLMutexHolder oHolder( &hMutex );

    // If we know the file exists and is not a directory,
    // then don't try to list its content.
    CachedFileProp* cachedFileProp =
        GetCachedFileProp(GetURLFromDirname(osDirname));
    if( cachedFileProp->eExists == EXIST_YES && !cachedFileProp->bIsDirectory )
    {
        if( pbGotFileList )
            *pbGotFileList = true;
        return NULL;
    }

    CachedDirList* psCachedDirList = cacheDirList[osDirname];
    if( psCachedDirList == NULL )
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
    return ReadDirInternal(pszDirname, nMaxFiles, NULL);
}

/************************************************************************/
/*                         VSIS3FSHandler                               */
/************************************************************************/

class VSIS3FSHandler CPL_FINAL : public VSICurlFilesystemHandler
{
    std::map< CPLString, VSIS3UpdateParams > oMapBucketsToS3Params;

protected:
    virtual VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    virtual char** GetFileList( const char *pszFilename,
                                int nMaxFiles,
                                bool* pbGotFileList ) override;
    virtual CPLString GetURLFromDirname( const CPLString& osDirname ) override;

public:
        VSIS3FSHandler() {}
        virtual ~VSIS3FSHandler();

        virtual VSIVirtualHandle *Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError ) override;
        virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                               int nFlags ) override;
        virtual int      Unlink( const char *pszFilename ) override;

        virtual CPLString GetFSPrefix() override { return "/vsis3/"; }

        void UpdateMapFromHandle( VSIS3HandleHelper * poS3HandleHelper );
        void UpdateHandleFromMap( VSIS3HandleHelper * poS3HandleHelper );

        virtual void        ClearCache() override;
};

/************************************************************************/
/*                            VSIS3Handle                               */
/************************************************************************/

class VSIS3Handle CPL_FINAL : public VSICurlHandle
{
    VSIS3HandleHelper* m_poS3HandleHelper;

  protected:
        virtual struct curl_slist* GetCurlHeaders( const CPLString& osVerb )
            override;
        virtual bool CanRestartOnError( const char*, bool ) override;
        virtual bool UseLimitRangeGetInsteadOfHead() override { return true; }
        virtual void ProcessGetFileSizeResult( const char* pszContent )
            override;

    public:
        VSIS3Handle( VSIS3FSHandler* poFS,
                     const char* pszFilename,
                     VSIS3HandleHelper* poS3HandleHelper );
        virtual ~VSIS3Handle();
};

/************************************************************************/
/*                            VSIS3WriteHandle                          */
/************************************************************************/

class VSIS3WriteHandle CPL_FINAL : public VSIVirtualHandle
{
    VSIS3FSHandler     *m_poFS;
    CPLString           m_osFilename;
    VSIS3HandleHelper  *m_poS3HandleHelper;
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

    static size_t       ReadCallBackBuffer( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    bool                InitiateMultipartUpload();
    bool                UploadPart();
    static size_t       ReadCallBackXML( char *buffer, size_t size,
                                         size_t nitems, void *instream );
    bool                CompleteMultipart();
    bool                AbortMultipart();
    bool                DoSinglePartPUT();

    public:
        VSIS3WriteHandle( VSIS3FSHandler* poFS,
                          const char* pszFilename,
                          VSIS3HandleHelper* poS3HandleHelper );
        virtual ~VSIS3WriteHandle();

        virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
        virtual vsi_l_offset Tell() override;
        virtual size_t    Read( void *pBuffer, size_t nSize,
                                size_t nMemb ) override;
        virtual size_t    Write( const void *pBuffer, size_t nSize,
                                 size_t nMemb ) override;
        virtual int       Eof() override;
        virtual int       Close() override;

        bool              IsOK() { return m_pabyBuffer != NULL; }
};

/************************************************************************/
/*                         VSIS3WriteHandle()                           */
/************************************************************************/

VSIS3WriteHandle::VSIS3WriteHandle( VSIS3FSHandler* poFS,
                                    const char* pszFilename,
                                    VSIS3HandleHelper* poS3HandleHelper ) :
        m_poFS(poFS), m_osFilename(pszFilename),
        m_poS3HandleHelper(poS3HandleHelper),
        m_nCurOffset(0),
        m_nBufferOff(0),
        m_nBufferOffReadCallback(0),
        m_bClosed(false),
        m_nPartNumber(0),
        m_nOffsetInXML(0),
        m_bError(false)
{
    const int nChunkSizeMB = atoi(CPLGetConfigOption("VSIS3_CHUNK_SIZE", "50"));
    if( nChunkSizeMB <= 0 || nChunkSizeMB > 1000 )
        m_nBufferSize = 0;
    else
        m_nBufferSize = nChunkSizeMB * 1024 * 1024;
    m_pabyBuffer = static_cast<GByte *>(VSIMalloc(m_nBufferSize));
    if( m_pabyBuffer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate working buffer for /vsis3");
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
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIS3WriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( (nWhence == SEEK_SET && nOffset != m_nCurOffset) ||
        nOffset != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable /vsis3 files");
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
             "Read not supported on writable /vsis3 files");
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
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

        struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("POST");
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        m_poS3HandleHelper->ResetQueryParameters();

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 || sWriteFuncData.pBuffer == NULL )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer) )
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
                CPLDebug("S3", "UploadId: %s", m_osUploadID.c_str());
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
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

    struct curl_slist* headers =
        m_poS3HandleHelper->GetCurlHeaders("PUT",
                                           m_pabyBuffer,
                                           m_nBufferOff);
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    WriteFuncStruct sWriteFuncHeaderData;
    VSICURLInitWriteFuncStruct(&sWriteFuncHeaderData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, &sWriteFuncHeaderData);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

    curl_slist_free_all(headers);

    long response_code = 0;
    curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
    if( response_code != 200 || sWriteFuncHeaderData.pBuffer == NULL )
    {
        CPLDebug("S3", "%s",
                 sWriteFuncData.pBuffer ? sWriteFuncData.pBuffer : "(null)");
        CPLError(CE_Failure, CPLE_AppDefined, "UploadPart(%d) of %s failed",
                    m_nPartNumber, m_osFilename.c_str());
        bSuccess = false;
    }
    else
    {
        const char* pszEtag = strstr(sWriteFuncHeaderData.pBuffer, "ETag: ");
        if( pszEtag != NULL )
        {
            CPLString osEtag = pszEtag + strlen("ETag: ");
            const size_t nPos = osEtag.find("\r");
            if( nPos != std::string::npos )
                osEtag.resize(nPos);
            CPLDebug("S3", "Etag for part %d is %s",
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
/*                               Write()                                */
/************************************************************************/

size_t
VSIS3WriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if( m_bError )
        return false;

    size_t nBytesToWrite = nSize * nMemb;

    while( nBytesToWrite > 0 )
    {
        const int nToWriteInBuffer = static_cast<int>(
            std::min(static_cast<size_t>(m_nBufferSize - m_nBufferOff),
                     nBytesToWrite));
        memcpy(m_pabyBuffer + m_nBufferOff, pBuffer, nToWriteInBuffer);
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
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackBuffer);
        curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
        curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, m_nBufferOff);

        struct curl_slist* headers =
            m_poS3HandleHelper->GetCurlHeaders("PUT",
                                               m_pabyBuffer,
                                               m_nBufferOff);
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                m_poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer) )
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
            m_poFS->InvalidateCachedData(
                m_poS3HandleHelper->GetURL().c_str() );
            m_poFS->InvalidateDirContent( CPLGetDirname(m_osFilename) );
        }

        CPLFree(sWriteFuncData.pBuffer);

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
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION, ReadCallBackXML);
    curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE,
                     static_cast<int>(m_osXML.size()));
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");

    struct curl_slist* headers =
        m_poS3HandleHelper->GetCurlHeaders("POST", m_osXML.c_str(),
                                           m_osXML.size());
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

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
    CPLHTTPSetOptions(hCurlHandle, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

    struct curl_slist* headers = m_poS3HandleHelper->GetCurlHeaders("DELETE");
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    m_poS3HandleHelper->ResetQueryParameters();

    WriteFuncStruct sWriteFuncData;
    VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlHandleWriteFunc);

    curl_easy_perform(hCurlHandle);

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
        if( m_osUploadID.empty() )
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
        return NULL;

    if( strchr(pszAccess, 'w') != NULL || strchr(pszAccess, 'a') != NULL )
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
        if( poS3HandleHelper == NULL )
            return NULL;
        UpdateHandleFromMap(poS3HandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poS3HandleHelper);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            poHandle = NULL;
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
/*                                Stat()                                */
/************************************************************************/

int VSIS3FSHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
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
    return NULL;
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
    if( poS3HandleHelper == NULL )
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
/*                               Unlink()                               */
/************************************************************************/

int VSIS3FSHandler::Unlink( const char *pszFilename )
{
    CPLString osNameWithoutPrefix = pszFilename + GetFSPrefix().size();
    VSIS3HandleHelper* poS3HandleHelper =
        VSIS3HandleHelper::BuildFromURI(osNameWithoutPrefix,
                                        GetFSPrefix().c_str(), false);
    if( poS3HandleHelper == NULL )
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
        CPLHTTPSetOptions(hCurlHandle, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* headers = poS3HandleHelper->GetCurlHeaders("DELETE");
        curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        WriteFuncStruct sWriteFuncData;
        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        curl_easy_perform(hCurlHandle);

        curl_slist_free_all(headers);

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 204 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                bGoOn = true;
            }
            else
            {
                CPLDebug("S3", "%s",
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
            InvalidateDirContent( CPLGetDirname(pszFilename) );
        }

        CPLFree(sWriteFuncData.pBuffer);

        curl_easy_cleanup(hCurlHandle);
    }
    while( bGoOn );

    delete poS3HandleHelper;
    return nRet;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSIS3FSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug("S3", "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;
    CPLString osDirnameWithoutPrefix = pszDirname + GetFSPrefix().size();

    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(osDirnameWithoutPrefix,
                                            GetFSPrefix().c_str(), true);
    if( poS3HandleHelper == NULL )
    {
        return NULL;
    }
    UpdateHandleFromMap(poS3HandleHelper);

    CPLString osObjectKey = poS3HandleHelper->GetObjectKey();
    poS3HandleHelper->SetObjectKey("");

    WriteFuncStruct sWriteFuncData;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    CPLString osMaxKeys = CPLGetConfigOption("AWS_MAX_KEYS", "");

    while( true )
    {
        poS3HandleHelper->ResetQueryParameters();
        CPLString osBaseURL(poS3HandleHelper->GetURL());

#if LIBCURL_VERSION_NUM < 0x070B00
        // Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have
        // been previously set, so we have to reinit the connection handle.
        GetCurlHandleFor("");
#endif

        CURL* hCurlHandle = GetCurlHandleFor(osBaseURL);

        poS3HandleHelper->AddQueryParameter("delimiter", "/");
        if( !osNextMarker.empty() )
            poS3HandleHelper->AddQueryParameter("marker", osNextMarker);
        if( !osMaxKeys.empty() )
             poS3HandleHelper->AddQueryParameter("max-keys", osMaxKeys);
        if( !osObjectKey.empty() )
             poS3HandleHelper->AddQueryParameter("prefix", osObjectKey + "/");

        struct curl_slist* headers = 
            VSICurlSetOptions(hCurlHandle, poS3HandleHelper->GetURL(), NULL);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                               poS3HandleHelper->GetCurlHeaders("GET"));
        if( headers != NULL )
            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        curl_easy_perform(hCurlHandle);

        if( headers != NULL )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == NULL)
        {
            delete poS3HandleHelper;
            return NULL;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            if( sWriteFuncData.pBuffer != NULL &&
                poS3HandleHelper->CanRestartOnError(sWriteFuncData.pBuffer) )
            {
                UpdateMapFromHandle(poS3HandleHelper);
                CPLFree(sWriteFuncData.pBuffer);
            }
            else
            {
                CPLDebug("S3", "%s",
                         sWriteFuncData.pBuffer
                         ? sWriteFuncData.pBuffer : "(null)");
                CPLFree(sWriteFuncData.pBuffer);
                delete poS3HandleHelper;
                return NULL;
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

            if( osNextMarker.empty() )
            {
                delete poS3HandleHelper;
                return osFileList.StealList();
            }
        }
    }
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateMapFromHandle( VSIS3HandleHelper * poS3HandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    oMapBucketsToS3Params[ poS3HandleHelper->GetBucket() ] =
        VSIS3UpdateParams ( poS3HandleHelper->GetAWSRegion(),
                      poS3HandleHelper->GetAWSS3Endpoint(),
                      poS3HandleHelper->GetRequestPayer(),
                      poS3HandleHelper->GetVirtualHosting() );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3FSHandler::UpdateHandleFromMap( VSIS3HandleHelper * poS3HandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    std::map< CPLString, VSIS3UpdateParams>::iterator oIter =
        oMapBucketsToS3Params.find(poS3HandleHelper->GetBucket());
    if( oIter != oMapBucketsToS3Params.end() )
    {
        poS3HandleHelper->SetAWSRegion(oIter->second.m_osAWSRegion);
        poS3HandleHelper->SetAWSS3Endpoint(oIter->second.m_osAWSS3Endpoint);
        poS3HandleHelper->SetRequestPayer(oIter->second.m_osRequestPayer);
        poS3HandleHelper->SetVirtualHosting(oIter->second.m_bUseVirtualHosting);
    }
}

/************************************************************************/
/*                             VSIS3Handle()                            */
/************************************************************************/

VSIS3Handle::VSIS3Handle( VSIS3FSHandler* poFSIn,
                          const char* pszFilename,
                          VSIS3HandleHelper* poS3HandleHelper ) :
        VSICurlHandle(poFSIn, pszFilename, poS3HandleHelper->GetURL()),
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

struct curl_slist* VSIS3Handle::GetCurlHeaders( const CPLString& osVerb )
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3Handle::CanRestartOnError(const char* pszErrorMsg, bool bSetError)
{
    if( m_poS3HandleHelper->CanRestartOnError(pszErrorMsg, bSetError) )
    {
        static_cast<VSIS3FSHandler *>(poFS)->
            UpdateMapFromHandle(m_poS3HandleHelper);

        SetURL(m_poS3HandleHelper->GetURL());
        return true;
    }
    return false;
}

/************************************************************************/
/*                    ProcessGetFileSizeResult()                        */
/************************************************************************/

void VSIS3Handle::ProcessGetFileSizeResult( const char* pszContent )
{
    bIsDirectory = strstr(pszContent, "ListBucketResult") != NULL;
}


/************************************************************************/
/*                         VSIGSFSHandler                               */
/************************************************************************/

class VSIGSFSHandler CPL_FINAL : public VSICurlFilesystemHandler
{
protected:
    virtual VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
    virtual char** GetFileList( const char *pszFilename,
                                int nMaxFiles,
                                bool* pbGotFileList ) override;

public:
        VSIGSFSHandler() {}

        virtual CPLString GetFSPrefix() override { return "/vsigs/"; }
        virtual CPLString GetURLFromDirname( const CPLString& osDirname ) override;
};

/************************************************************************/
/*                            VSIGSHandle                               */
/************************************************************************/

class VSIGSHandle CPL_FINAL : public VSICurlHandle
{
    VSIGSHandleHelper* m_poHandleHelper;

  protected:
        virtual struct curl_slist* GetCurlHeaders( const CPLString& osVerb )
            override;

    public:
        VSIGSHandle( VSIGSFSHandler* poFS, const char* pszFilename,
                     VSIGSHandleHelper* poHandleHelper);
        virtual ~VSIGSHandle();
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIGSFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( pszFilename + GetFSPrefix().size(),
                                         GetFSPrefix() );
    if( poHandleHelper == NULL )
        return NULL;
    return new VSIGSHandle(this, pszFilename, poHandleHelper);
}

/************************************************************************/
/*                          GetURLFromDirname()                         */
/************************************************************************/

CPLString VSIGSFSHandler::GetURLFromDirname( const CPLString& osDirname )
{
    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( osDirname, GetFSPrefix() );
    if( poHandleHelper == NULL )
        return CPLString();
    CPLString osURL( poHandleHelper->GetURL() );
    delete poHandleHelper;
    return osURL;
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** VSIGSFSHandler::GetFileList( const char *pszDirname,
                                    int nMaxFiles,
                                    bool* pbGotFileList )
{
    if( ENABLE_DEBUG )
        CPLDebug("GS", "GetFileList(%s)" , pszDirname);
    *pbGotFileList = false;

    WriteFuncStruct sWriteFuncData;

    CPLAssert( STARTS_WITH(pszDirname, GetFSPrefix().c_str()) );
    CPLString osBucketObjectKey(pszDirname + GetFSPrefix().size());
    CPLString osBucket(osBucketObjectKey);
    CPLString osObjectKey;
    size_t nSlashPos = osBucketObjectKey.find('/');
    if( nSlashPos != std::string::npos )
    {
        osBucket = osBucketObjectKey.substr(0, nSlashPos);
        osObjectKey = osBucketObjectKey.substr(nSlashPos+1);
    }

    VSIGSHandleHelper* poHandleHelper =
        VSIGSHandleHelper::BuildFromURI( osBucket, GetFSPrefix() );
    if( poHandleHelper == NULL )
        return NULL;

    CPLStringList osFileList; // must be left in this scope !
    CPLString osNextMarker; // must be left in this scope !

    const CPLString osMaxKeys = CPLGetConfigOption("AWS_MAX_KEYS", "");
    const CPLString osBaseURL ( poHandleHelper->GetURL() );

    while( true )
    {

#if LIBCURL_VERSION_NUM < 0x070B00
        // Curl 7.10.X doesn't manage to unset the CURLOPT_RANGE that would have
        // been previously set, so we have to reinit the connection handle.
        GetCurlHandleFor("");
#endif

        CURL* hCurlHandle = GetCurlHandleFor(osBaseURL);
        CPLString osURL ( osBaseURL );
        osURL += "?delimiter=/";
        if( !osNextMarker.empty() )
            osURL += "&marker=" + osNextMarker;
        if( !osMaxKeys.empty() )
            osURL += "&max-keys=" + osMaxKeys;
        if( !osObjectKey.empty() )
            osURL += "&prefix=" + osObjectKey + "/";

        struct curl_slist* headers =
            VSICurlSetOptions(hCurlHandle, osURL, NULL);

        curl_easy_setopt(hCurlHandle, CURLOPT_RANGE, NULL);

        VSICURLInitWriteFuncStruct(&sWriteFuncData, NULL, NULL, NULL);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
        curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                         VSICurlHandleWriteFunc);

        char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
        curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

        headers = VSICurlMergeHeaders(headers,
                                      poHandleHelper->GetCurlHeaders("GET"));
        if( headers != NULL )
            curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

        curl_easy_perform(hCurlHandle);

        if( headers != NULL )
            curl_slist_free_all(headers);

        if( sWriteFuncData.pBuffer == NULL)
        {
            delete poHandleHelper;
            return NULL;
        }

        long response_code = 0;
        curl_easy_getinfo(hCurlHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code == 200 )
        {
            *pbGotFileList = true;
            bool bIsTruncated;
            AnalyseS3FileList( osBaseURL + "/",
                               sWriteFuncData.pBuffer,
                               osFileList,
                               nMaxFiles,
                               bIsTruncated,
                               osNextMarker );

            CPLFree(sWriteFuncData.pBuffer);

            if( osNextMarker.empty() )
            {
                delete poHandleHelper;
                return osFileList.StealList();
            }
        }
        else
        {
            CPLFree(sWriteFuncData.pBuffer);
            delete poHandleHelper;
            return NULL;
        }
    }
}

/************************************************************************/
/*                             VSIGSHandle()                            */
/************************************************************************/

VSIGSHandle::VSIGSHandle( VSIGSFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIGSHandleHelper* poHandleHelper ) :
        VSICurlHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
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

struct curl_slist* VSIGSHandle::GetCurlHeaders( const CPLString& osVerb )
{
    if( CSLFetchNameValue(m_papszHTTPOptions, "HEADER_FILE") )
        return NULL;
    return m_poHandleHelper->GetCurlHeaders( osVerb );
}

} /* end of anoymous namespace */

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

    curl_easy_setopt(hCurlHandle, CURLOPT_NOBODY, 0);
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADER, 0);

/* 7.16.4 */
#if LIBCURL_VERSION_NUM <= 0x071004
    curl_easy_setopt(hCurlHandle, CURLOPT_FTPLISTONLY, 0);
#elif LIBCURL_VERSION_NUM > 0x071004
    curl_easy_setopt(hCurlHandle, CURLOPT_DIRLISTONLY, 0);
#endif

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, NULL);

    return headers;
}

/************************************************************************/
/*                     VSICurlMergeHeaders()                            */
/************************************************************************/

struct curl_slist* VSICurlMergeHeaders( struct curl_slist* poDest,
                                        struct curl_slist* poSrcToDestroy )
{
    struct curl_slist* iter = poSrcToDestroy;
    while( iter != NULL )
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
 * A special file handler is installed that allows on-the-fly random reading of
 * files available through HTTP/FTP web protocols, without prior download of the
 * entire file.
 *
 * Recognized filenames are of the form /vsicurl/http://path/to/remote/resource
 * or /vsicurl/ftp://path/to/remote/resource where path/to/remote/resource is
 * the URL of a remote resource.
 * 
 * Starting with GDAL 2.3, options can be passed in the filename with the
 * following syntax:
 * /vsicurl/option1=val1[,optionN=valN]*,url=http://...
 * Currently supported options are :
 * <ul>
 * <li>use_head=yes/no: whether the HTTP HEAD request can be emitted.
 *     Default to YES.
 *     Setting this option overrides the behaviour of the
 *     CPL_VSIL_CURL_USE_HEAD configuration option.</li>
 * <li>max_retry=number: default to 0.
 *     Setting this option overrides the behaviour of the
 *     GDAL_HTTP_MAX_RETRY configuration option.</li>
 * <li>retry_delay=number_in_seconds: default to 30.
 *     Setting this option overrides the behaviour of the
 *     GDAL_HTTP_RETRY_DELAY configuration option.</li>
 * <li>list_dir=yes/no: whether an attempt to read the file list of the
 *     directory where the file is located should be done. Default to YES.</li>
 * </ul>
 *
 * Partial downloads (requires the HTTP server to support random reading) are
 * done with a 16 KB granularity by default. If the driver detects sequential
 * reading it will progressively increase the chunk size up to 2 MB to improve
 * download performance.
 *
 * The GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD and GDAL_PROXY_AUTH configuration
 * options can be used to define a proxy server. The syntax to use is the one of
 * Curl CURLOPT_PROXY, CURLOPT_PROXYUSERPWD and CURLOPT_PROXYAUTH options.
 * 
 * Starting with GDAL 2.3, the GDAL_HTTP_MAX_RETRY (number of attempts) and
 * GDAL_HTTP_RETRY_DELAY (in seconds) configuration option can be set, so that
 * request retries are done in case of HTTP errors 502, 503 or 504.
 *
 * The file can be cached in RAM by setting the
 * configuration option VSI_CACHE to TRUE. The cache size defaults to 25 MB, but
 * can be modified by setting the configuration option VSI_CACHE_SIZE (in
 * bytes). Content in that cache is discarded when the file handle is closed.
 * 
 * In addition, a global LRU cache of 16 MB shared among all downloaded content 
 * is enabled by default, and content in it may be reused after a file handle
 * has been closed and reopen. Starting with GDAL 2.3, the
 * CPL_VSIL_CURL_NON_CACHED configuration option can be set to values like
 * "/vsicurl/http://example.com/foo.tif:/vsicurl/http://example.com/some_directory",
 * so that at file handle closing, all cached content related to the mentioned
 * file(s) is no longer cached. This can help when dealing with resources that
 * can be modified during execution of GDAL related code. Alternatively,
 * VSICurlClearCache() can be used.
 *
 * Starting with GDAL 2.1, /vsicurl/ will try to query directly redirected URLs
 * to Amazon S3 signed URLs during their validity period, so as to minimize
 * round-trips. This behaviour can be disabled by setting the configuration
 * option CPL_VSIL_CURL_USE_S3_REDIRECT to NO.
 *
 * Starting with GDAL 2.1.3, the CURL_CA_BUNDLE or SSL_CERT_FILE configuration
 * options can be used to set the path to the Certification Authority (CA)
 * bundle file (if not specified, curl will use a file in a system location).
 *
 * VSIStatL() will return the size in st_size member and file nature- file or
 * directory - in st_mode member (the later only reliable with FTP resources for
 * now).
 *
 * VSIReadDir() should be able to parse the HTML directory listing returned by
 * the most popular web servers, such as Apache or Microsoft IIS.
 *
 * This special file handler can be combined with other virtual filesystems
 * handlers, such as /vsizip. For example,
 * /vsizip//vsicurl/path/to/remote/file.zip/path/inside/zip
 *
 * @since GDAL 1.8.0
 */
void VSIInstallCurlFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsicurl/", new VSICurlFilesystemHandler );
}

/************************************************************************/
/*                      VSIInstallS3FileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsis3/ Amazon S3 file system handler (requires libcurl)
 *
 * A special file handler is installed that allows on-the-fly random reading of
 * non-public  files available in AWS S3 buckets, without prior download of the
 * entire file.
 * It also allows sequential writing of files (no seeks or read operations are
 * then allowed).
 *
 * Recognized filenames are of the form /vsis3/bucket/key where
 * bucket is the name of the S3 bucket and key the S3 object "key", i.e.
 * a filename potentially containing subdirectories.
 *
 * Partial downloads are done with a 16 KB granularity by default.
 * If the driver detects sequential reading
 * it will progressively increase the chunk size up to 2 MB to improve download
 * performance.
 *
 * The AWS_SECRET_ACCESS_KEY and AWS_ACCESS_KEY_ID configuration options *must*
 * be set.  The AWS_SESSION_TOKEN configuration option must be set when
 * temporary credentials are used.  The AWS_REGION (or AWS_DEFAULT_REGION
 * starting with GDAL 2.3) configuration option may be
 * set to one of the supported
 * <a href="http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region">S3
 * regions</a> and defaults to 'us-east-1'.
 * 
 * Starting with GDAL 2.3, alternate ways of providing credentials similar to
 * what the "aws" command line utility or Boto3 support can be used. If the
 * above mentionned environment variables are not provided, the ~/.aws/credentials
 * or %UserProfile%/.aws/credentials file will be read (or the file pointed by
 * CPL_AWS_CREDENTIALS_FILE). The profile may be
 * specified with the AWS_PROFILE environment variable (the default profile is "default")
 * The ~/.aws/config or %UserProfile%/.aws/config file may also be used (or the
 * file pointer by AWS_CONFIG_FILE) to retrieve credentials and the AWS region.
 * If none of the above method succeeds, instance profile credentials will be
 * retrieved when GDAL is used on EC2 instances.
 * 
 * Starting with GDAL 2.2, the
 * AWS_REQUEST_PAYER configuration option may be set to "requester" to
 * facilitate use with
 * <a href="http://docs.aws.amazon.com/AmazonS3/latest/dev/RequesterPaysBuckets.html">Requester
 * Pays buckets</a>.
 * 
 * The AWS_S3_ENDPOINT configuration option defaults to s3.amazonaws.com. 
 *
 * The GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD and GDAL_PROXY_AUTH configuration
 * options can be used to define a proxy server. The syntax to use is the one of
 * Curl CURLOPT_PROXY, CURLOPT_PROXYUSERPWD and CURLOPT_PROXYAUTH options.
 *
 * Starting with GDAL 2.1.3, the CURL_CA_BUNDLE or SSL_CERT_FILE configuration
 * options can be used to set the path to the Certification Authority (CA)
 * bundle file (if not specified, curl will use a file in a system location).
 *
 * On reading, the file can be cached in RAM by setting the
 * configuration option VSI_CACHE to TRUE. The cache size defaults to 25 MB, but
 * can be modified by setting the configuration option VSI_CACHE_SIZE (in
 * bytes). Content in that cache is discarded when the file handle is closed.
 * 
 * In addition, a global LRU cache of 16 MB shared among all downloaded content 
 * is enabled by default, and content in it may be reused after a file handle
 * has been closed and reopen. Starting with GDAL 2.3, the
 * CPL_VSIL_CURL_NON_CACHED configuration option can be set to values like
 * "/vsis3/bucket/foo.tif:/vsis3/another_bucket/some_directory",
 * so that at file handle closing, all cached content related to the mentioned
 * file(s) is no longer cached. This can help when dealing with resources that
 * can be modified during execution of GDAL related code. Alternatively,
 * VSICurlClearCache() can be used.
 * 
 * On writing, the file is uploaded using the S3
 * <a href="http://docs.aws.amazon.com/AmazonS3/latest/API/mpUploadInitiate.html">multipart upload API</a>.
 * The size of chunks is set to 50 MB by default, allowing creating files up to
 * 500 GB (10000 parts of 50 MB each). If larger files are needed, then increase
 * the value of the VSIS3_CHUNK_SIZE config option to a larger value (expressed
 * in MB).  In case the process is killed and the file not properly closed, the
 * multipart upload will remain open, causing Amazon to charge you for the parts
 * storage. You'll have to abort yourself with other means such "ghost" uploads
 * (e.g. with the <a href="http://s3tools.org/s3cmd">s3cmd</a> utility) For
 * files smaller than the chunk size, a simple PUT request is used instead of
 * the multipart upload API.
 *
 * VSIStatL() will return the size in st_size member.
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
 * A special file handler is installed that allows on-the-fly random reading of
 * non-public files available in Google Cloud Storage buckets, without prior
 * download of the entire file.
 * Read-only support for now.
 *
 * Recognized filenames are of the form /vsigs/bucket/key where
 * bucket is the name of the bucket and key the object "key", i.e.
 * a filename potentially containing subdirectories.
 *
 * Partial downloads are done with a 16 KB granularity by default.
 * If the driver detects sequential reading
 * it will progressively increase the chunk size up to 2 MB to improve download
 * performance.
 *
 * The GS_SECRET_ACCESS_KEY and GS_ACCESS_KEY_ID configuration options must be
 * set to use the AWS S3 authentication compatibility method.
 * 
 * Alternatively, it is possible to set the GDAL_HTTP_HEADER_FILE configuration
 * option to point to a filename of a text file with "key: value" headers.
 * Typically, it must contain a "Authorization: Bearer XXXXXXXXX" line.
 *
 * The GDAL_HTTP_PROXY, GDAL_HTTP_PROXYUSERPWD and GDAL_PROXY_AUTH configuration
 * options can be used to define a proxy server. The syntax to use is the one of
 * Curl CURLOPT_PROXY, CURLOPT_PROXYUSERPWD and CURLOPT_PROXYAUTH options.
 *
 * The CURL_CA_BUNDLE or SSL_CERT_FILE configuration
 * options can be used to set the path to the Certification Authority (CA)
 * bundle file (if not specified, curl will use a file in a system location).
 *
 * On reading, the file can be cached in RAM by setting the
 * configuration option VSI_CACHE to TRUE. The cache size defaults to 25 MB, but
 * can be modified by setting the configuration option VSI_CACHE_SIZE (in
 * bytes). Content in that cache is discarded when the file handle is closed.
 * 
 * In addition, a global LRU cache of 16 MB shared among all downloaded content 
 * is enabled by default, and content in it may be reused after a file handle
 * has been closed and reopen. Starting with GDAL 2.3, the
 * CPL_VSIL_CURL_NON_CACHED configuration option can be set to values like
 * "/vsigs/bucket/foo.tif:/vsigs/another_bucket/some_directory",
 * so that at file handle closing, all cached content related to the mentioned
 * file(s) is no longer cached. This can help when dealing with resources that
 * can be modified during execution of GDAL related code. Alternatively,
 * VSICurlClearCache() can be used.
 *
 * VSIStatL() will return the size in st_size member.
 *
 * @since GDAL 2.2
 */

void VSIInstallGSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsigs/", new VSIGSFSHandler );
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
    const char* const apszFS[] = { "/vsicurl/", "/vsis3/", "/vsigs/" };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszFS); ++i )
    {
        VSICurlFilesystemHandler *poFSHandler =
            dynamic_cast<VSICurlFilesystemHandler*>(
                VSIFileManager::GetHandler( apszFS[i] ));

        if( poFSHandler )
            poFSHandler->ClearCache();
    }
}

#endif /* HAVE_CURL */
