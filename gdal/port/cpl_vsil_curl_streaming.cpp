/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HTTP/FTP files in streaming mode
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2015, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_vsil_curl_class.h"

#include <algorithm>
#include <map>

#include "cpl_aws.h"
#include "cpl_google_cloud.h"
#include "cpl_azure.h"
#include "cpl_alibaba_oss.h"
#include "cpl_swift.h"
#include "cpl_hash_set.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_time.h"

CPL_CVSID("$Id$")

#if !defined(HAVE_CURL) || defined(CPL_MULTIPROC_STUB)

void VSIInstallCurlStreamingFileHandler(void)
{
    // Not supported.
}

void VSIInstallS3StreamingFileHandler(void)
{
    // Not supported.
}

void VSIInstallGSStreamingFileHandler(void)
{
    // Not supported.
}

void VSIInstallAzureStreamingFileHandler(void)
{
    // Not supported
}

void VSIInstallOSSStreamingFileHandler(void)
{
    // Not supported
}

void VSIInstallSwiftStreamingFileHandler(void)
{
    // Not supported
}

#ifdef HAVE_CURL
void VSICurlStreamingClearCache( void )
{
    // Not supported
}
#endif

#else

//! @cond Doxygen_Suppress

#include <curl/curl.h>

#define ENABLE_DEBUG        0

#define N_MAX_REGIONS       10

#define BKGND_BUFFER_SIZE   (1024 * 1024)

/************************************************************************/
/*                               RingBuffer                             */
/************************************************************************/

class RingBuffer
{
    CPL_DISALLOW_COPY_ASSIGN(RingBuffer)

    GByte* pabyBuffer = nullptr;
    size_t nCapacity = 0;
    size_t nOffset = 0;
    size_t nLength = 0;

    public:
        explicit RingBuffer(size_t nCapacity = BKGND_BUFFER_SIZE);
        ~RingBuffer();

        size_t GetCapacity() const { return nCapacity; }
        size_t GetSize() const { return nLength; }

        void Reset();
        void Write(void* pBuffer, size_t nSize);
        void Read(void* pBuffer, size_t nSize);
};

RingBuffer::RingBuffer( size_t nCapacityIn ) :
    pabyBuffer(static_cast<GByte*>(CPLMalloc(nCapacityIn))),
    nCapacity(nCapacityIn)
{}

RingBuffer::~RingBuffer()
{
    CPLFree(pabyBuffer);
}

void RingBuffer::Reset()
{
    nOffset = 0;
    nLength = 0;
}

void RingBuffer::Write( void* pBuffer, size_t nSize )
{
    CPLAssert(nLength + nSize <= nCapacity);

    const size_t nEndOffset = (nOffset + nLength) % nCapacity;
    const size_t nSz = std::min(nSize, nCapacity - nEndOffset);
    memcpy(pabyBuffer + nEndOffset, pBuffer, nSz);
    if( nSz < nSize )
        memcpy(pabyBuffer, static_cast<GByte *>(pBuffer) + nSz, nSize - nSz);

    nLength += nSize;
}

void RingBuffer::Read( void* pBuffer, size_t nSize )
{
    CPLAssert(nSize <= nLength);

    if( pBuffer )
    {
        const size_t nSz = std::min(nSize, nCapacity - nOffset);
        memcpy(pBuffer, pabyBuffer + nOffset, nSz);
        if( nSz < nSize )
          memcpy(static_cast<GByte *>(pBuffer) + nSz, pabyBuffer, nSize - nSz);
    }

    nOffset = (nOffset + nSize) % nCapacity;
    nLength -= nSize;
}

/************************************************************************/

namespace {

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

typedef struct
{
    ExistStatus     eExists;
    int             bHasComputedFileSize;
    vsi_l_offset    fileSize;
    int             bIsDirectory;
#ifdef notdef
    unsigned int    nChecksumOfFirst1024Bytes;
#endif
} CachedFileProp;

typedef struct
{
    char*           pBuffer;
    size_t          nSize;
    int             bIsHTTP;
    int             bIsInHeader;
    int             nHTTPCode;
    int             bDownloadHeaderOnly;
} WriteFuncStruct;

/************************************************************************/
/*                       VSICurlStreamingFSHandler                      */
/************************************************************************/

class VSICurlStreamingHandle;

class VSICurlStreamingFSHandler : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlStreamingFSHandler)

    std::map<CPLString, CachedFileProp*>   cacheFileSize{};

protected:
    CPLMutex           *hMutex = nullptr;

    virtual CPLString GetFSPrefix() { return "/vsicurl_streaming/"; }
    virtual VSICurlStreamingHandle* CreateFileHandle(const char* pszURL);

public:
    VSICurlStreamingFSHandler();
    virtual ~VSICurlStreamingFSHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError,
                                    CSLConstList /* papszOptions */ ) override;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;

    const char* GetActualURL(const char* pszFilename) override;

    const char* GetOptions() override
                        { return VSIGetFileSystemOptions("/vsicurl/"); }

    void                AcquireMutex();
    void                ReleaseMutex();

    CachedFileProp*     GetCachedFileProp(const char*     pszURL);

    virtual void    ClearCache();
};

/************************************************************************/
/*                        VSICurlStreamingHandle                        */
/************************************************************************/

class VSICurlStreamingHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlStreamingHandle)

  protected:
    VSICurlStreamingFSHandler* m_poFS = nullptr;
    char**          m_papszHTTPOptions = nullptr;

  private:
    char*           m_pszURL = nullptr;

#ifdef notdef
    unsigned int    nRecomputedChecksumOfFirst1024Bytes = 0;
#endif
    vsi_l_offset    curOffset = 0;
    vsi_l_offset    fileSize = 0;
    int             bHasComputedFileSize = 0;
    ExistStatus     eExists = EXIST_UNKNOWN;
    int             bIsDirectory = 0;

    bool            bCanTrustCandidateFileSize = true;
    bool            bHasCandidateFileSize = false;
    vsi_l_offset    nCandidateFileSize = 0;

    bool            bEOF = false;

    size_t          nCachedSize = 0;
    GByte          *pCachedData = nullptr;

    volatile int    bDownloadInProgress = FALSE;
    volatile int    bDownloadStopped = FALSE;
    volatile int    bAskDownloadEnd = FALSE;
    vsi_l_offset    nRingBufferFileOffset = 0;
    CPLJoinableThread *hThread = nullptr;
    CPLMutex       *hRingBufferMutex = nullptr;
    CPLCond        *hCondProducer = nullptr;
    CPLCond        *hCondConsumer = nullptr;
    RingBuffer      oRingBuffer{};
    void            StartDownload();
    void            StopDownload();
    void            PutRingBufferInCache();

    GByte          *pabyHeaderData = nullptr;
    size_t          nHeaderSize = 0;
    vsi_l_offset    nBodySize = 0;
    int             nHTTPCode = 0;

    void                AcquireMutex();
    void                ReleaseMutex();

    void                AddRegion( vsi_l_offset    nFileOffsetStart,
                                   size_t          nSize,
                                   GByte          *pData );

  protected:
    virtual struct curl_slist* GetCurlHeaders(const CPLString&,
                                const struct curl_slist* /* psExistingHeaders */ )
        { return nullptr; }
    virtual bool StopReceivingBytesOnError() { return true; }
    virtual bool CanRestartOnError( const char* /*pszErrorMsg*/,
                                    const char* /*pszHeaders*/,
                                    bool /*bSetError*/ ) { return false; }
    virtual bool InterpretRedirect() { return true; }
    void SetURL( const char* pszURL );

  public:
    VSICurlStreamingHandle( VSICurlStreamingFSHandler* poFS,
                            const char* pszURL );
    ~VSICurlStreamingHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    void                 DownloadInThread();
    size_t               ReceivedBytes( GByte *buffer, size_t count,
                                        size_t nmemb);
    size_t               ReceivedBytesHeader( GByte *buffer, size_t count,
                                              size_t nmemb );

    int                  IsKnownFileSize() const
        { return bHasComputedFileSize; }
    vsi_l_offset         GetFileSize();
    int                  Exists();
    int                  IsDirectory() const { return bIsDirectory; }

    const char          *GetURL() const { return m_pszURL; }
};

/************************************************************************/
/*                       VSICurlStreamingHandle()                       */
/************************************************************************/

VSICurlStreamingHandle::VSICurlStreamingHandle( VSICurlStreamingFSHandler* poFS,
                                                const char* pszURL ):
    m_poFS(poFS),
    m_papszHTTPOptions(CPLHTTPGetOptionsFromEnv()),
    m_pszURL(CPLStrdup(pszURL))
{

    poFS->AcquireMutex();
    CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
    eExists = cachedFileProp->eExists;
    fileSize = cachedFileProp->fileSize;
    bHasComputedFileSize = cachedFileProp->bHasComputedFileSize;
    bIsDirectory = cachedFileProp->bIsDirectory;
    poFS->ReleaseMutex();

    hRingBufferMutex = CPLCreateMutex();
    ReleaseMutex();
    hCondProducer = CPLCreateCond();
    hCondConsumer = CPLCreateCond();
}

/************************************************************************/
/*                       ~VSICurlStreamingHandle()                      */
/************************************************************************/

VSICurlStreamingHandle::~VSICurlStreamingHandle()
{
    StopDownload();

    CPLFree(m_pszURL);
    CSLDestroy( m_papszHTTPOptions );

    CPLFree(pCachedData);

    CPLFree(pabyHeaderData);

    CPLDestroyMutex( hRingBufferMutex );
    CPLDestroyCond( hCondProducer );
    CPLDestroyCond( hCondConsumer );
}

/************************************************************************/
/*                            SetURL()                                  */
/************************************************************************/

void VSICurlStreamingHandle::SetURL(const char* pszURLIn)
{
    CPLFree(m_pszURL);
    m_pszURL = CPLStrdup(pszURLIn);
}

/************************************************************************/
/*                         AcquireMutex()                               */
/************************************************************************/

void VSICurlStreamingHandle::AcquireMutex()
{
    CPLAcquireMutex(hRingBufferMutex, 1000.0);
}

/************************************************************************/
/*                          ReleaseMutex()                              */
/************************************************************************/

void VSICurlStreamingHandle::ReleaseMutex()
{
    CPLReleaseMutex(hRingBufferMutex);
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICurlStreamingHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( curOffset >= BKGND_BUFFER_SIZE )
    {
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL",
                     "Invalidating cache and file size due to Seek() "
                     "beyond caching zone");
        CPLFree(pCachedData);
        pCachedData = nullptr;
        nCachedSize = 0;
        AcquireMutex();
        bHasComputedFileSize = FALSE;
        fileSize = 0;
        ReleaseMutex();
    }

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
/*                  VSICURLStreamingInitWriteFuncStruct()               */
/************************************************************************/

static void VSICURLStreamingInitWriteFuncStruct( WriteFuncStruct *psStruct )
{
    psStruct->pBuffer = nullptr;
    psStruct->nSize = 0;
    psStruct->bIsHTTP = FALSE;
    psStruct->bIsInHeader = TRUE;
    psStruct->nHTTPCode = 0;
    psStruct->bDownloadHeaderOnly = FALSE;
}

/************************************************************************/
/*                 VSICurlStreamingHandleWriteFuncForHeader()           */
/************************************************************************/

static size_t
VSICurlStreamingHandleWriteFuncForHeader( void *buffer, size_t count,
                                          size_t nmemb, void *req )
{
    WriteFuncStruct* psStruct = static_cast<WriteFuncStruct *>(req);
    const size_t nSize = count * nmemb;

    char* pNewBuffer = static_cast<char*>(
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

            if( pszLine[0] == '\r' || pszLine[0] == '\n' )
            {
                if( psStruct->bDownloadHeaderOnly )
                {
                    // If moved permanently/temporarily, go on.
                    // Otherwise stop now.
                    if( !(psStruct->nHTTPCode == 301 ||
                          psStruct->nHTTPCode == 302) )
                        return 0;
                }
                else
                {
                    psStruct->bIsInHeader = FALSE;
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
/*                           GetFileSize()                              */
/************************************************************************/

vsi_l_offset VSICurlStreamingHandle::GetFileSize()
{
    WriteFuncStruct sWriteFuncData;
    WriteFuncStruct sWriteFuncHeaderData;

    AcquireMutex();
    if( bHasComputedFileSize )
    {
        const vsi_l_offset nRet = fileSize;
        ReleaseMutex();
        return nRet;
    }
    ReleaseMutex();

    CURL* hLocalHandle = curl_easy_init();

    struct curl_slist* headers =
        VSICurlSetOptions(hLocalHandle, m_pszURL, m_papszHTTPOptions);

    VSICURLStreamingInitWriteFuncStruct(&sWriteFuncHeaderData);

    // HACK for mbtiles driver: Proper fix would be to auto-detect servers that
    // don't accept HEAD http://a.tiles.mapbox.com/v3/ doesn't accept HEAD, so
    // let's start a GET and interrupt is as soon as the header is found.
    CPLString osVerb;
    if( strstr(m_pszURL, ".tiles.mapbox.com/") != nullptr )
    {
        curl_easy_setopt(hLocalHandle, CURLOPT_HEADERDATA,
                         &sWriteFuncHeaderData);
        curl_easy_setopt(hLocalHandle, CURLOPT_HEADERFUNCTION,
                         VSICurlStreamingHandleWriteFuncForHeader);

        sWriteFuncHeaderData.bIsHTTP = STARTS_WITH(m_pszURL, "http");
        sWriteFuncHeaderData.bDownloadHeaderOnly = TRUE;
        osVerb = "GET";
    }
    else
    {
        curl_easy_setopt(hLocalHandle, CURLOPT_NOBODY, 1);
        curl_easy_setopt(hLocalHandle, CURLOPT_HTTPGET, 0);
        curl_easy_setopt(hLocalHandle, CURLOPT_HEADER, 1);
        osVerb = "HEAD";
    }

    headers = VSICurlMergeHeaders(headers, GetCurlHeaders(osVerb, headers));
    curl_easy_setopt(hLocalHandle, CURLOPT_HTTPHEADER, headers);

    // We need that otherwise OSGEO4W's libcurl issue a dummy range request
    // when doing a HEAD when recycling connections.
    curl_easy_setopt(hLocalHandle, CURLOPT_RANGE, nullptr);

    // Bug with older curl versions (<=7.16.4) and FTP.
    // See http://curl.haxx.se/mail/lib-2007-08/0312.html
    VSICURLStreamingInitWriteFuncStruct(&sWriteFuncData);
    curl_easy_setopt(hLocalHandle, CURLOPT_WRITEDATA, &sWriteFuncData);
    curl_easy_setopt(hLocalHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlStreamingHandleWriteFuncForHeader);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    curl_easy_setopt(hLocalHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    void* old_handler = CPLHTTPIgnoreSigPipe();
    curl_easy_perform(hLocalHandle);
    CPLHTTPRestoreSigPipeHandler(old_handler);
    if( headers != nullptr )
        curl_slist_free_all(headers);

    AcquireMutex();

    eExists = EXIST_UNKNOWN;
    bHasComputedFileSize = TRUE;

    if( STARTS_WITH(m_pszURL, "ftp") )
    {
        if( sWriteFuncData.pBuffer != nullptr &&
            STARTS_WITH_CI(sWriteFuncData.pBuffer, "Content-Length: ") )
        {
            const char* pszBuffer =
                sWriteFuncData.pBuffer + strlen("Content-Length: ");
            eExists = EXIST_YES;
            fileSize =
                CPLScanUIntBig(pszBuffer,
                               static_cast<int>(sWriteFuncData.nSize -
                                                strlen("Content-Length: ")));
            if( ENABLE_DEBUG )
                CPLDebug("VSICURL", "GetFileSize(%s)=" CPL_FRMT_GUIB,
                         m_pszURL, fileSize);
        }
    }

    double dfSize = 0;
    if( eExists != EXIST_YES )
    {
        const CURLcode code =
            curl_easy_getinfo(hLocalHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                              &dfSize );
        if( code == 0 )
        {
            eExists = EXIST_YES;
            if( dfSize < 0 )
                fileSize = 0;
            else
                fileSize = static_cast<GUIntBig>(dfSize);
        }
        else
        {
            eExists = EXIST_NO;
            fileSize = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSICurlStreamingHandle::GetFileSize failed");
        }

        long response_code = 0;
        curl_easy_getinfo(hLocalHandle, CURLINFO_HTTP_CODE, &response_code);
        if( response_code != 200 )
        {
            eExists = EXIST_NO;
            fileSize = 0;
        }

        // Try to guess if this is a directory. Generally if this is a
        // directory, curl will retry with an URL with slash added.
        char *pszEffectiveURL = nullptr;
        curl_easy_getinfo(hLocalHandle, CURLINFO_EFFECTIVE_URL,
                          &pszEffectiveURL);
        if( pszEffectiveURL != nullptr &&
            strncmp(m_pszURL, pszEffectiveURL, strlen(m_pszURL)) == 0 &&
            pszEffectiveURL[strlen(m_pszURL)] == '/' )
        {
            eExists = EXIST_YES;
            fileSize = 0;
            bIsDirectory = TRUE;
        }

        if( ENABLE_DEBUG )
            CPLDebug("VSICURL",
                     "GetFileSize(%s)=" CPL_FRMT_GUIB " response_code=%d",
                     m_pszURL, fileSize, static_cast<int>(response_code));
    }

    CPLFree(sWriteFuncData.pBuffer);
    CPLFree(sWriteFuncHeaderData.pBuffer);

    m_poFS->AcquireMutex();
    CachedFileProp* cachedFileProp = m_poFS->GetCachedFileProp(m_pszURL);
    cachedFileProp->bHasComputedFileSize = TRUE;
#ifdef notdef
    cachedFileProp->nChecksumOfFirst1024Bytes =
        nRecomputedChecksumOfFirst1024Bytes;
#endif
    cachedFileProp->fileSize = fileSize;
    cachedFileProp->eExists = eExists;
    cachedFileProp->bIsDirectory = bIsDirectory;
    m_poFS->ReleaseMutex();

    const vsi_l_offset nRet = fileSize;
    ReleaseMutex();

    curl_easy_cleanup(hLocalHandle);

    return nRet;
}

/************************************************************************/
/*                                 Exists()                             */
/************************************************************************/

int VSICurlStreamingHandle::Exists()
{
    if( eExists == EXIST_UNKNOWN )
    {
        // Consider that only the files whose extension ends up with one that is
        // listed in CPL_VSIL_CURL_ALLOWED_EXTENSIONS exist on the server.
        // This can speeds up dramatically open experience, in case the server
        // cannot return a file list.
        // For example:
        // gdalinfo --config CPL_VSIL_CURL_ALLOWED_EXTENSIONS ".tif" /vsicurl_streaming/http://igskmncngs506.cr.usgs.gov/gmted/Global_tiles_GMTED/075darcsec/bln/W030/30N030W_20101117_gmted_bln075.tif */
        const char* pszAllowedExtensions =
            CPLGetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", nullptr);
        if( pszAllowedExtensions )
        {
            char** papszExtensions =
                CSLTokenizeString2( pszAllowedExtensions, ", ", 0 );
            const size_t nURLLen = strlen(m_pszURL);
            bool bFound = false;
            for( int i = 0; papszExtensions[i] != nullptr; i++ )
            {
                const size_t nExtensionLen = strlen(papszExtensions[i]);
                if( nURLLen > nExtensionLen &&
                    EQUAL(m_pszURL + nURLLen - nExtensionLen,
                          papszExtensions[i]) )
                {
                    bFound = true;
                    break;
                }
            }

            if( !bFound )
            {
                eExists = EXIST_NO;
                fileSize = 0;

                m_poFS->AcquireMutex();
                CachedFileProp* cachedFileProp =
                    m_poFS->GetCachedFileProp(m_pszURL);
                cachedFileProp->bHasComputedFileSize = TRUE;
                cachedFileProp->fileSize = fileSize;
                cachedFileProp->eExists = eExists;
                m_poFS->ReleaseMutex();

                CSLDestroy(papszExtensions);

                return 0;
            }

            CSLDestroy(papszExtensions);
        }

        char chFirstByte = '\0';
        int bExists = (Read(&chFirstByte, 1, 1) == 1);

        AcquireMutex();
        m_poFS->AcquireMutex();
        CachedFileProp* cachedFileProp = m_poFS->GetCachedFileProp(m_pszURL);
        cachedFileProp->eExists = eExists = bExists ? EXIST_YES : EXIST_NO;
        m_poFS->ReleaseMutex();
        ReleaseMutex();

        Seek(0, SEEK_SET);
    }

    return eExists == EXIST_YES;
}

/************************************************************************/
/*                                  Tell()                              */
/************************************************************************/

vsi_l_offset VSICurlStreamingHandle::Tell()
{
    return curOffset;
}

/************************************************************************/
/*                         ReceivedBytes()                              */
/************************************************************************/

size_t VSICurlStreamingHandle::ReceivedBytes( GByte *buffer, size_t count,
                                              size_t nmemb )
{
    size_t nSize = count * nmemb;
    nBodySize += nSize;

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Receiving %d bytes...", static_cast<int>(nSize));

    if( bHasCandidateFileSize && bCanTrustCandidateFileSize &&
        !bHasComputedFileSize )
    {
        m_poFS->AcquireMutex();
        CachedFileProp* cachedFileProp = m_poFS->GetCachedFileProp(m_pszURL);
        cachedFileProp->fileSize = fileSize = nCandidateFileSize;
        cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = TRUE;
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL", "File size = " CPL_FRMT_GUIB, fileSize);
        m_poFS->ReleaseMutex();
    }

    AcquireMutex();
    if( eExists == EXIST_UNKNOWN )
    {
        m_poFS->AcquireMutex();
        CachedFileProp* cachedFileProp = m_poFS->GetCachedFileProp(m_pszURL);
        cachedFileProp->eExists = eExists = EXIST_YES;
        m_poFS->ReleaseMutex();
    }
    else if( eExists == EXIST_NO && StopReceivingBytesOnError() )
    {
        ReleaseMutex();
        return 0;
    }

    while( true )
    {
        const size_t nFree = oRingBuffer.GetCapacity() - oRingBuffer.GetSize();
        if( nSize <= nFree )
        {
            oRingBuffer.Write(buffer, nSize);

            // Signal to the consumer that we have added bytes to the buffer.
            CPLCondSignal(hCondProducer);

            if( bAskDownloadEnd )
            {
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL", "Download interruption asked");

                ReleaseMutex();
                return 0;
            }
            break;
        }
        else
        {
            oRingBuffer.Write(buffer, nFree);
            buffer += nFree;
            nSize -= nFree;

            // Signal to the consumer that we have added bytes to the buffer.
            CPLCondSignal(hCondProducer);

            if( ENABLE_DEBUG )
                CPLDebug("VSICURL",
                         "Waiting for reader to consume some bytes...");

            while( oRingBuffer.GetSize() == oRingBuffer.GetCapacity() &&
                   !bAskDownloadEnd )
            {
                CPLCondWait(hCondConsumer, hRingBufferMutex);
            }

            if( bAskDownloadEnd )
            {
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL", "Download interruption asked");

                ReleaseMutex();
                return 0;
            }
        }
    }

    ReleaseMutex();

    return nmemb;
}

/************************************************************************/
/*                 VSICurlStreamingHandleReceivedBytes()                */
/************************************************************************/

static size_t VSICurlStreamingHandleReceivedBytes( void *buffer, size_t count,
                                                   size_t nmemb, void *req )
{
    return
        static_cast<VSICurlStreamingHandle *>(req)->
            ReceivedBytes(static_cast<GByte *>(buffer), count, nmemb);
}

/************************************************************************/
/*              VSICurlStreamingHandleReceivedBytesHeader()             */
/************************************************************************/

#define HEADER_SIZE 32768

size_t VSICurlStreamingHandle::ReceivedBytesHeader( GByte *buffer, size_t count,
                                                    size_t nmemb )
{
    const size_t nSize = count * nmemb;
    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Receiving %d bytes for header...",
                 static_cast<int>(nSize));

    // Reset buffer if we have followed link after a redirect.
    if( nSize >= 9 && InterpretRedirect() &&
        (nHTTPCode == 301 || nHTTPCode == 302) &&
        STARTS_WITH_CI(reinterpret_cast<char *>(buffer), "HTTP/") )
    {
        nHeaderSize = 0;
        nHTTPCode = 0;
    }

    if( nHeaderSize < HEADER_SIZE )
    {
        const size_t nSz = std::min(nSize, HEADER_SIZE - nHeaderSize);
        memcpy(pabyHeaderData + nHeaderSize, buffer, nSz);
        pabyHeaderData[nHeaderSize + nSz] = '\0';
        nHeaderSize += nSz;

#if DEBUG_VERBOSE
        CPLDebug("VSICURL", "Header : %s", pabyHeaderData);
#endif

        AcquireMutex();

        if( eExists == EXIST_UNKNOWN && nHTTPCode == 0 &&
            strchr(reinterpret_cast<char *>(pabyHeaderData), '\n') != nullptr &&
            STARTS_WITH_CI(reinterpret_cast<char *>(pabyHeaderData),
                            "HTTP/") )
        {
            nHTTPCode = 0;
            const char* pszSpace = strchr(
                const_cast<const char*>(
                    reinterpret_cast<char *>(pabyHeaderData)), ' ');
            if( pszSpace )
                nHTTPCode = atoi(pszSpace + 1);
            if( ENABLE_DEBUG )
                CPLDebug("VSICURL", "HTTP code = %d", nHTTPCode);

            // If moved permanently/temporarily, go on.
            if( !(InterpretRedirect() &&
                  (nHTTPCode == 301 || nHTTPCode == 302)) )
            {
                m_poFS->AcquireMutex();
                CachedFileProp* cachedFileProp =
                    m_poFS->GetCachedFileProp(m_pszURL);
                eExists = nHTTPCode == 200 ? EXIST_YES : EXIST_NO;
                cachedFileProp->eExists = eExists;
                m_poFS->ReleaseMutex();
            }
        }

        if( !(InterpretRedirect() && (nHTTPCode == 301 || nHTTPCode == 302)) &&
            !bHasComputedFileSize )
        {
            // Caution: When gzip compression is enabled, the content-length is
            // the compressed size, which we are not interested in, so we must
            // not take it into account.

            const char* pszContentLength =
                strstr(reinterpret_cast<char *>(pabyHeaderData),
                       "Content-Length: ");
            const char* pszEndOfLine =
                pszContentLength ? strchr(pszContentLength, '\n') : nullptr;
            if( bCanTrustCandidateFileSize && pszEndOfLine != nullptr )
            {
                const char* pszVal =
                    pszContentLength + strlen("Content-Length: ");
                bHasCandidateFileSize = true;
                nCandidateFileSize =
                    CPLScanUIntBig(pszVal,
                                   static_cast<int>(pszEndOfLine - pszVal));
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL",
                             "Has found candidate file size = " CPL_FRMT_GUIB,
                             nCandidateFileSize);
            }

            const char* pszContentEncoding =
                strstr(reinterpret_cast<char *>(pabyHeaderData),
                       "Content-Encoding: ");
            pszEndOfLine =
                pszContentEncoding ? strchr(pszContentEncoding, '\n') : nullptr;
            if( bHasCandidateFileSize && pszEndOfLine != nullptr )
            {
                const char* pszVal =
                    pszContentEncoding + strlen("Content-Encoding: ");
                if( STARTS_WITH(pszVal, "gzip") )
                {
                    if( ENABLE_DEBUG )
                        CPLDebug("VSICURL",
                                 "GZip compression enabled --> "
                                 "cannot trust candidate file size");
                    bCanTrustCandidateFileSize = false;
                }
            }
        }

        ReleaseMutex();
    }

    return nmemb;
}

/************************************************************************/
/*                 VSICurlStreamingHandleReceivedBytesHeader()          */
/************************************************************************/

static size_t
VSICurlStreamingHandleReceivedBytesHeader( void *buffer, size_t count,
                                           size_t nmemb, void *req )
{
    return
        static_cast<VSICurlStreamingHandle *>(req)->
            ReceivedBytesHeader(static_cast<GByte *>(buffer), count, nmemb);
}

/************************************************************************/
/*                       DownloadInThread()                             */
/************************************************************************/

void VSICurlStreamingHandle::DownloadInThread()
{
    CURL* hCurlHandle = curl_easy_init();

    struct curl_slist* headers =
        VSICurlSetOptions(hCurlHandle, m_pszURL, m_papszHTTPOptions);
    headers = VSICurlMergeHeaders(headers, GetCurlHeaders("GET", headers));
    curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER, headers);

    static bool bHasCheckVersion = false;
    static bool bSupportGZip = false;
    if( !bHasCheckVersion )
    {
        bSupportGZip = strstr(curl_version(), "zlib/") != nullptr;
        bHasCheckVersion = true;
    }
    if( bSupportGZip &&
        CPLTestBool(CPLGetConfigOption("CPL_CURL_GZIP", "YES")) )
    {
        curl_easy_setopt(hCurlHandle, CURLOPT_ENCODING, "gzip");
    }

    if( pabyHeaderData == nullptr )
        pabyHeaderData = static_cast<GByte *>(CPLMalloc(HEADER_SIZE + 1));
    nHeaderSize = 0;
    nBodySize = 0;
    nHTTPCode = 0;

    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                     VSICurlStreamingHandleReceivedBytesHeader);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                     VSICurlStreamingHandleReceivedBytes);

    char szCurlErrBuf[CURL_ERROR_SIZE+1] = {};
    szCurlErrBuf[0] = '\0';
    curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    void* old_handler = CPLHTTPIgnoreSigPipe();
    CURLcode eRet = curl_easy_perform(hCurlHandle);
    CPLHTTPRestoreSigPipeHandler(old_handler);
    if( headers != nullptr )
        curl_slist_free_all(headers);

    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA, nullptr);
    curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION, nullptr);

    AcquireMutex();
    if( !bAskDownloadEnd && eRet == 0 && !bHasComputedFileSize )
    {
        m_poFS->AcquireMutex();
        CachedFileProp* cachedFileProp = m_poFS->GetCachedFileProp(m_pszURL);
        cachedFileProp->fileSize = fileSize = nBodySize;
        cachedFileProp->bHasComputedFileSize = bHasComputedFileSize = TRUE;
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL", "File size = " CPL_FRMT_GUIB, fileSize);
        m_poFS->ReleaseMutex();
    }

    bDownloadInProgress = FALSE;
    bDownloadStopped = TRUE;

    // Signal to the consumer that the download has ended.
    CPLCondSignal(hCondProducer);
    ReleaseMutex();

    curl_easy_cleanup(hCurlHandle);
}

static void VSICurlDownloadInThread( void* pArg )
{
    static_cast<VSICurlStreamingHandle *>(pArg)->DownloadInThread();
}

/************************************************************************/
/*                            StartDownload()                            */
/************************************************************************/

void VSICurlStreamingHandle::StartDownload()
{
    if( bDownloadInProgress || bDownloadStopped )
        return;

    CPLDebug("VSICURL", "Start download for %s", m_pszURL);

    oRingBuffer.Reset();
    bDownloadInProgress = TRUE;
    nRingBufferFileOffset = 0;
    hThread = CPLCreateJoinableThread(VSICurlDownloadInThread, this);
}

/************************************************************************/
/*                            StopDownload()                            */
/************************************************************************/

void VSICurlStreamingHandle::StopDownload()
{
    if( hThread )
    {
        CPLDebug("VSICURL", "Stop download for %s", m_pszURL);

        AcquireMutex();
        // Signal to the producer that we ask for download interruption.
        bAskDownloadEnd = TRUE;
        CPLCondSignal(hCondConsumer);

        // Wait for the producer to have finished.
        while( bDownloadInProgress )
            CPLCondWait(hCondProducer, hRingBufferMutex);

        bAskDownloadEnd = FALSE;

        ReleaseMutex();

        CPLJoinThread(hThread);
        hThread = nullptr;
    }

    oRingBuffer.Reset();
    bDownloadStopped = FALSE;
}

/************************************************************************/
/*                        PutRingBufferInCache()                        */
/************************************************************************/

void VSICurlStreamingHandle::PutRingBufferInCache()
{
    if( nRingBufferFileOffset >= BKGND_BUFFER_SIZE )
        return;

    AcquireMutex();

    // Cache any remaining bytes available in the ring buffer.
    size_t nBufSize = oRingBuffer.GetSize();
    if( nBufSize > 0 )
    {
        if( nRingBufferFileOffset + nBufSize > BKGND_BUFFER_SIZE )
            nBufSize =
                static_cast<size_t>(BKGND_BUFFER_SIZE - nRingBufferFileOffset);
        GByte* pabyTmp = static_cast<GByte *>(CPLMalloc(nBufSize));
        oRingBuffer.Read(pabyTmp, nBufSize);

        // Signal to the producer that we have ingested some bytes.
        CPLCondSignal(hCondConsumer);

        AddRegion(nRingBufferFileOffset, nBufSize, pabyTmp);
        nRingBufferFileOffset += nBufSize;
        CPLFree(pabyTmp);
    }

    ReleaseMutex();
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICurlStreamingHandle::Read( void * const pBuffer, size_t const nSize,
                                     size_t const nMemb )
{
    GByte* pabyBuffer = static_cast<GByte *>(pBuffer);
    const size_t nBufferRequestSize = nSize * nMemb;
    const vsi_l_offset curOffsetOri = curOffset;
    const vsi_l_offset nRingBufferFileOffsetOri = nRingBufferFileOffset;
    if( nBufferRequestSize == 0 )
        return 0;
    size_t nRemaining = nBufferRequestSize;

    AcquireMutex();
    // fileSize might be set wrongly to 0, such as
    // /vsicurl_streaming/https://query.data.world/s/jgsghstpphjhicstradhy5kpjwrnfy
    const int bHasComputedFileSizeLocal = bHasComputedFileSize && fileSize > 0;
    const vsi_l_offset fileSizeLocal = fileSize;
    ReleaseMutex();

    if( bHasComputedFileSizeLocal && curOffset >= fileSizeLocal )
    {
        CPLDebug("VSICURL", "Read attempt beyond end of file");
        bEOF = true;
    }
    if( bEOF )
        return 0;

    if( curOffset < nRingBufferFileOffset )
        PutRingBufferInCache();

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Read [" CPL_FRMT_GUIB ", " CPL_FRMT_GUIB "[ in %s",
                 curOffset, curOffset + nBufferRequestSize, m_pszURL);

#ifdef notdef
    if( pCachedData != nullptr && nCachedSize >= 1024 &&
        nRecomputedChecksumOfFirst1024Bytes == 0 )
    {
        for( size_t i = 0; i < 1024 / sizeof(int); i++ )
        {
            int nVal = 0;
            memcpy(&nVal, pCachedData + i * sizeof(int), sizeof(int));
            nRecomputedChecksumOfFirst1024Bytes += nVal;
        }

        if( bHasComputedFileSizeLocal )
        {
            poFS->AcquireMutex();
            CachedFileProp* cachedFileProp = poFS->GetCachedFileProp(pszURL);
            if( cachedFileProp->nChecksumOfFirst1024Bytes == 0 )
            {
                cachedFileProp->nChecksumOfFirst1024Bytes =
                    nRecomputedChecksumOfFirst1024Bytes;
            }
            else if( nRecomputedChecksumOfFirst1024Bytes !=
                     cachedFileProp->nChecksumOfFirst1024Bytes )
            {
                CPLDebug("VSICURL",
                         "Invalidating previously cached file size. "
                         "First bytes of file have changed!");
                AcquireMutex();
                bHasComputedFileSize = FALSE;
                cachedFileProp->bHasComputedFileSize = FALSE;
                cachedFileProp->nChecksumOfFirst1024Bytes = 0;
                ReleaseMutex();
            }
            poFS->ReleaseMutex();
        }
    }
#endif

    // Can we use the cache?
    if( pCachedData != nullptr && curOffset < nCachedSize )
    {
        const size_t nSz =
            std::min(nRemaining, static_cast<size_t>(nCachedSize - curOffset));
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL", "Using cache for [%d, %d[ in %s",
                     static_cast<int>(curOffset),
                     static_cast<int>(curOffset + nSz), m_pszURL);
        memcpy(pabyBuffer, pCachedData + curOffset, nSz);
        pabyBuffer += nSz;
        curOffset += nSz;
        nRemaining -= nSz;
    }

    // Is the request partially covered by the cache and going beyond file size?
    if( pCachedData != nullptr && bHasComputedFileSizeLocal &&
        curOffset <= nCachedSize &&
        curOffset + nRemaining > fileSizeLocal &&
        fileSize == nCachedSize )
    {
        size_t nSz = static_cast<size_t>(nCachedSize - curOffset);
        if( ENABLE_DEBUG && nSz != 0 )
            CPLDebug("VSICURL", "Using cache for [%d, %d[ in %s",
                     static_cast<int>(curOffset),
                     static_cast<int>(curOffset + nSz), m_pszURL);
        memcpy(pabyBuffer, pCachedData + curOffset, nSz);
        pabyBuffer += nSz;
        curOffset += nSz;
        nRemaining -= nSz;
        bEOF = true;
    }

    // Has a Seek() being done since the last Read()?
    if( !bEOF && nRemaining > 0 && curOffset != nRingBufferFileOffset )
    {
        // Backward seek: Need to restart the download from the beginning.
        if( curOffset < nRingBufferFileOffset )
            StopDownload();

        StartDownload();

        const vsi_l_offset SKIP_BUFFER_SIZE = 32768;
        GByte* pabyTmp = static_cast<GByte *>(CPLMalloc(SKIP_BUFFER_SIZE));

        CPLAssert(curOffset >= nRingBufferFileOffset);
        vsi_l_offset nBytesToSkip = curOffset - nRingBufferFileOffset;
        while(nBytesToSkip > 0)
        {
            vsi_l_offset nBytesToRead = nBytesToSkip;

            AcquireMutex();
            if( nBytesToRead > oRingBuffer.GetSize() )
                nBytesToRead = oRingBuffer.GetSize();
            if( nBytesToRead > SKIP_BUFFER_SIZE )
                nBytesToRead = SKIP_BUFFER_SIZE;
            oRingBuffer.Read(pabyTmp, static_cast<size_t>(nBytesToRead));

            // Signal to the producer that we have ingested some bytes.
            CPLCondSignal(hCondConsumer);
            ReleaseMutex();

            if( nBytesToRead )
                AddRegion(nRingBufferFileOffset,
                          static_cast<size_t>(nBytesToRead), pabyTmp);

            nBytesToSkip -= nBytesToRead;
            nRingBufferFileOffset += nBytesToRead;

            if( nBytesToRead == 0 && nBytesToSkip != 0 )
            {
                if( ENABLE_DEBUG )
                    CPLDebug("VSICURL",
                             "Waiting for writer to produce some bytes...");

                AcquireMutex();
                while(oRingBuffer.GetSize() == 0 && bDownloadInProgress)
                    CPLCondWait(hCondProducer, hRingBufferMutex);
                const int bBufferEmpty = (oRingBuffer.GetSize() == 0);
                ReleaseMutex();

                if( bBufferEmpty && !bDownloadInProgress )
                    break;
            }
        }

        CPLFree(pabyTmp);

        if( nBytesToSkip != 0 )
        {
            bEOF = true;
            return 0;
        }
    }

    if( !bEOF && nRemaining > 0 )
    {
        StartDownload();
        CPLAssert(curOffset == nRingBufferFileOffset);
    }

    // Fill the destination buffer from the ring buffer.
    while(!bEOF && nRemaining > 0)
    {
        AcquireMutex();
        size_t nToRead = oRingBuffer.GetSize();
        if( nToRead > nRemaining )
            nToRead = nRemaining;
        oRingBuffer.Read(pabyBuffer, nToRead);

        // Signal to the producer that we have ingested some bytes.
        CPLCondSignal(hCondConsumer);
        ReleaseMutex();

        if( nToRead )
            AddRegion(curOffset, nToRead, pabyBuffer);

        nRemaining -= nToRead;
        pabyBuffer += nToRead;
        curOffset += nToRead;
        nRingBufferFileOffset += nToRead;

        if( nToRead == 0 && nRemaining != 0 )
        {
            if( ENABLE_DEBUG )
                CPLDebug("VSICURL",
                         "Waiting for writer to produce some bytes...");

            AcquireMutex();
            while(oRingBuffer.GetSize() == 0 && bDownloadInProgress)
                CPLCondWait(hCondProducer, hRingBufferMutex);
            const bool bBufferEmpty = oRingBuffer.GetSize() == 0;
            ReleaseMutex();

            if( bBufferEmpty && !bDownloadInProgress )
                break;
        }
    }

    if( ENABLE_DEBUG )
        CPLDebug("VSICURL", "Read(%d) = %d",
                 static_cast<int>(nBufferRequestSize),
                 static_cast<int>(nBufferRequestSize - nRemaining));
    size_t nRet = (nBufferRequestSize - nRemaining) / nSize;
    if( nRet < nMemb )
        bEOF = true;

    // Give a chance to specialized filesystem to deal with errors to redirect
    // elsewhere.
    if( curOffsetOri == 0 && nRingBufferFileOffsetOri == 0 &&
        !StopReceivingBytesOnError() &&
        eExists == EXIST_NO && nRemaining < nBufferRequestSize )
    {
        const size_t nErrorBufferMaxSize = 4096;
        GByte* pabyErrorBuffer =
            static_cast<GByte *>(CPLMalloc(nErrorBufferMaxSize + 1));
        size_t nRead = nBufferRequestSize - nRemaining;
        size_t nErrorBufferSize = std::min(nErrorBufferMaxSize, nRead);
        memcpy( pabyErrorBuffer, pBuffer, nErrorBufferSize );
        if( nRead < nErrorBufferMaxSize )
            nErrorBufferSize += Read(pabyErrorBuffer + nRead, 1,
                                     nErrorBufferMaxSize - nRead);
        pabyErrorBuffer[nErrorBufferSize] = 0;
        StopDownload();
        if( CanRestartOnError(reinterpret_cast<char *>(pabyErrorBuffer),
                              reinterpret_cast<char *>(pabyHeaderData),
                              true) )
        {
            curOffset = 0;
            nRingBufferFileOffset = 0;
            bEOF = false;
            AcquireMutex();
            eExists = EXIST_UNKNOWN;
            bHasComputedFileSize = FALSE;
            fileSize = 0;
            ReleaseMutex();
            nCachedSize = 0;
            m_poFS->AcquireMutex();
            CachedFileProp* cachedFileProp =
                m_poFS->GetCachedFileProp(m_pszURL);
            cachedFileProp->bHasComputedFileSize = FALSE;
            cachedFileProp->fileSize = 0;
            cachedFileProp->eExists = EXIST_UNKNOWN;
            m_poFS->ReleaseMutex();
            nRet = Read(pBuffer, nSize, nMemb);
        }
        else
        {
            CPLDebug("VSICURL", "Error buffer: %s",
                     reinterpret_cast<char *>(pabyErrorBuffer));
            nRet = 0;
        }

        CPLFree(pabyErrorBuffer);
    }

    return nRet;
}

/************************************************************************/
/*                          AddRegion()                                 */
/************************************************************************/

void VSICurlStreamingHandle::AddRegion( vsi_l_offset nFileOffsetStart,
                                        size_t nSize,
                                        GByte *pData )
{
    if( nFileOffsetStart >= BKGND_BUFFER_SIZE )
        return;

    if( pCachedData == nullptr )
      pCachedData = static_cast<GByte *>(CPLMalloc(BKGND_BUFFER_SIZE));

    if( nFileOffsetStart <= nCachedSize &&
        nFileOffsetStart + nSize > nCachedSize )
    {
        const size_t nSz =
            std::min(nSize,
                     static_cast<size_t>(BKGND_BUFFER_SIZE - nFileOffsetStart));
        if( ENABLE_DEBUG )
            CPLDebug("VSICURL", "Writing [%d, %d[ in cache for %s",
                     static_cast<int>(nFileOffsetStart),
                     static_cast<int>(nFileOffsetStart + nSz), m_pszURL);
        memcpy(pCachedData + nFileOffsetStart, pData, nSz);
        nCachedSize = static_cast<size_t>(nFileOffsetStart + nSz);
    }
}
/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSICurlStreamingHandle::Write( const void * /* pBuffer */,
                                      size_t /* nSize */,
                                      size_t /* nMemb */ )
{
    return 0;
}

/************************************************************************/
/*                                 Eof()                                */
/************************************************************************/

int VSICurlStreamingHandle::Eof()
{
    return bEOF;
}

/************************************************************************/
/*                                 Flush()                              */
/************************************************************************/

int VSICurlStreamingHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                                  Close()                             */
/************************************************************************/

int       VSICurlStreamingHandle::Close()
{
    return 0;
}

/************************************************************************/
/*                      VSICurlStreamingFSHandler()                     */
/************************************************************************/

VSICurlStreamingFSHandler::VSICurlStreamingFSHandler()
{
    hMutex = CPLCreateMutex();
    CPLReleaseMutex(hMutex);
}

/************************************************************************/
/*                      ~VSICurlStreamingFSHandler()                    */
/************************************************************************/

VSICurlStreamingFSHandler::~VSICurlStreamingFSHandler()
{
    VSICurlStreamingFSHandler::ClearCache();

    CPLDestroyMutex( hMutex );
    hMutex = nullptr;
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSICurlStreamingFSHandler::ClearCache()
{
    CPLMutexHolder oHolder( &hMutex );

    for( auto& kv: cacheFileSize )
    {
        CPLFree(kv.second);
    }
    cacheFileSize.clear();
}

/************************************************************************/
/*                         AcquireMutex()                               */
/************************************************************************/

void VSICurlStreamingFSHandler::AcquireMutex()
{
    CPLAcquireMutex(hMutex, 1000.0);
}

/************************************************************************/
/*                         ReleaseMutex()                               */
/************************************************************************/

void VSICurlStreamingFSHandler::ReleaseMutex()
{
    CPLReleaseMutex(hMutex);
}

/************************************************************************/
/*                         GetCachedFileProp()                          */
/************************************************************************/

/* Should be called under the FS Lock */

CachedFileProp *
VSICurlStreamingFSHandler::GetCachedFileProp( const char* pszURL )
{
    CachedFileProp* cachedFileProp = cacheFileSize[pszURL];
    if( cachedFileProp == nullptr )
    {
        cachedFileProp =
            static_cast<CachedFileProp *>(CPLMalloc(sizeof(CachedFileProp)));
        cachedFileProp->eExists = EXIST_UNKNOWN;
        cachedFileProp->bHasComputedFileSize = FALSE;
        cachedFileProp->fileSize = 0;
        cachedFileProp->bIsDirectory = FALSE;
#ifdef notdef
        cachedFileProp->nChecksumOfFirst1024Bytes = 0;
#endif
        cacheFileSize[pszURL] = cachedFileProp;
    }

    return cachedFileProp;
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSICurlStreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    return new VSICurlStreamingHandle(this, pszURL);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSICurlStreamingFSHandler::Open( const char *pszFilename,
                                                   const char *pszAccess,
                                                   bool /* bSetError */,
                                                   CSLConstList /* papszOptions */ )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr ||
        strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for %s",
                 GetFSPrefix().c_str());
        return nullptr;
    }

    VSICurlStreamingHandle* poHandle =
        CreateFileHandle(pszFilename + GetFSPrefix().size());
    // If we didn't get a filelist, check that the file really exists.
    if( poHandle == nullptr || !poHandle->Exists() )
    {
        delete poHandle;
        return nullptr;
    }

    if( CPLTestBool( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
        return VSICreateCachedFile( poHandle );

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICurlStreamingFSHandler::Stat( const char *pszFilename,
                                     VSIStatBufL *pStatBuf,
                                     int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return -1;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    VSICurlStreamingHandle* poHandle =
        CreateFileHandle(pszFilename + GetFSPrefix().size());
    if( poHandle == nullptr )
    {
        return -1;
    }
    if( poHandle->IsKnownFileSize() ||
        ((nFlags & VSI_STAT_SIZE_FLAG) && !poHandle->IsDirectory() &&
         CPLTestBool(CPLGetConfigOption("CPL_VSIL_CURL_SLOW_GET_SIZE",
                                        "YES"))) )
    {
        pStatBuf->st_size = poHandle->GetFileSize();
    }

    int nRet = (poHandle->Exists()) ? 0 : -1;
    pStatBuf->st_mode = poHandle->IsDirectory() ? S_IFDIR : S_IFREG;

    delete poHandle;
    return nRet;
}

/************************************************************************/
/*                          GetActualURL()                              */
/************************************************************************/

const char* VSICurlStreamingFSHandler::GetActualURL(const char* pszFilename)
{
    VSICurlStreamingHandle* poHandle = dynamic_cast<VSICurlStreamingHandle*>(
        Open(pszFilename, "rb", false, nullptr));
    if( poHandle == nullptr )
        return pszFilename;
    CPLString osURL(poHandle->GetURL());
    delete poHandle;
    return CPLSPrintf("%s", osURL.c_str());
}

/************************************************************************/
/*                      IVSIS3LikeStreamingFSHandler                    */
/************************************************************************/

class IVSIS3LikeStreamingFSHandler: public VSICurlStreamingFSHandler
{
        CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeStreamingFSHandler)

public:
        IVSIS3LikeStreamingFSHandler() = default;

        virtual void UpdateMapFromHandle( IVSIS3LikeHandleHelper * /*poHandleHelper*/ ) {}
        virtual void UpdateHandleFromMap( IVSIS3LikeHandleHelper * /*poHandleHelper*/ ) {}
};

/************************************************************************/
/*                       VSIS3StreamingFSHandler                        */
/************************************************************************/

class VSIS3StreamingFSHandler final: public IVSIS3LikeStreamingFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3StreamingFSHandler)

protected:
    CPLString GetFSPrefix() override { return "/vsis3_streaming/"; }
    VSICurlStreamingHandle* CreateFileHandle( const char* pszURL ) override;

public:
    VSIS3StreamingFSHandler() = default;
    ~VSIS3StreamingFSHandler() override = default;

    const char* GetOptions() override
                            { return VSIGetFileSystemOptions("/vsis3/"); }

    void UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper ) override;
    void UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper ) override;

    void ClearCache() override
    {
        IVSIS3LikeStreamingFSHandler::ClearCache();
        VSIS3UpdateParams::ClearCache();
    }
};

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIS3StreamingFSHandler::UpdateMapFromHandle(
    IVSIS3LikeHandleHelper * poHandleHelper )
{
    VSIS3UpdateParams::UpdateMapFromHandle(poHandleHelper);
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIS3StreamingFSHandler::UpdateHandleFromMap(
    IVSIS3LikeHandleHelper * poHandleHelper )
{
    VSIS3UpdateParams::UpdateHandleFromMap(poHandleHelper);
}

/************************************************************************/
/*                          VSIS3LikeStreamingHandle                    */
/************************************************************************/

class VSIS3LikeStreamingHandle final: public VSICurlStreamingHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3LikeStreamingHandle)

    IVSIS3LikeHandleHelper* m_poS3HandleHelper = nullptr;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders) override;
    bool StopReceivingBytesOnError() override { return false; }
    bool CanRestartOnError( const char* pszErrorMsg,
                            const char* pszHeaders,
                            bool bSetError ) override;
    bool InterpretRedirect() override { return false; }

  public:
    VSIS3LikeStreamingHandle( IVSIS3LikeStreamingFSHandler* poFS,
                              IVSIS3LikeHandleHelper* poS3HandleHelper );
    ~VSIS3LikeStreamingHandle() override;
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSIS3StreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    VSIS3HandleHelper* poS3HandleHelper =
            VSIS3HandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str(),
                                            false);
    if( poS3HandleHelper )
    {
        UpdateHandleFromMap(poS3HandleHelper);
        return new VSIS3LikeStreamingHandle(this, poS3HandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                     VSIS3LikeStreamingHandle()                       */
/************************************************************************/

VSIS3LikeStreamingHandle::VSIS3LikeStreamingHandle(
    IVSIS3LikeStreamingFSHandler* poFS,
    IVSIS3LikeHandleHelper* poS3HandleHelper) :
    VSICurlStreamingHandle(poFS, poS3HandleHelper->GetURL()),
    m_poS3HandleHelper(poS3HandleHelper)
{}

/************************************************************************/
/*                     ~VSIS3LikeStreamingHandle()                      */
/************************************************************************/

VSIS3LikeStreamingHandle::~VSIS3LikeStreamingHandle()
{
    delete m_poS3HandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist*
VSIS3LikeStreamingHandle::GetCurlHeaders( const CPLString& osVerb,
                                      const struct curl_slist* psExistingHeaders )
{
    return m_poS3HandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIS3LikeStreamingHandle::CanRestartOnError( const char* pszErrorMsg,
                                                  const char* pszHeaders,
                                                  bool bSetError )
{
    if( m_poS3HandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                              bSetError) )
    {
        static_cast<IVSIS3LikeStreamingFSHandler*>(m_poFS)->
            UpdateMapFromHandle(m_poS3HandleHelper);

        SetURL(m_poS3HandleHelper->GetURL());
        return true;
    }
    return false;
}



/************************************************************************/
/*                       VSIGSStreamingFSHandler                        */
/************************************************************************/

class VSIGSStreamingFSHandler final: public IVSIS3LikeStreamingFSHandler
{
  protected:
    CPLString GetFSPrefix() override { return "/vsigs_streaming/"; }
    VSICurlStreamingHandle* CreateFileHandle( const char* pszURL ) override;

  public:
    VSIGSStreamingFSHandler() {}
    ~VSIGSStreamingFSHandler() override {}

    const char* GetOptions() override
                        { return VSIGetFileSystemOptions("/vsigs/"); }
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSIGSStreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    VSIGSHandleHelper* poGCHandleHelper =
            VSIGSHandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str());
    if( poGCHandleHelper )
    {
        return new VSIS3LikeStreamingHandle(this, poGCHandleHelper);
    }
    return nullptr;
}


/************************************************************************/
/*                      VSIAzureStreamingFSHandler                      */
/************************************************************************/

class VSIAzureStreamingFSHandler final: public IVSIS3LikeStreamingFSHandler
{
  protected:
    CPLString GetFSPrefix() override { return "/vsiaz_streaming/"; }
    VSICurlStreamingHandle* CreateFileHandle( const char* pszURL ) override;

  public:
    VSIAzureStreamingFSHandler() {}
    ~VSIAzureStreamingFSHandler() override {}

    const char* GetOptions() override
                            { return VSIGetFileSystemOptions("/vsiaz/"); }
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSIAzureStreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    VSIAzureBlobHandleHelper* poHandleHelper =
            VSIAzureBlobHandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str());
    if( poHandleHelper )
    {
        return new VSIS3LikeStreamingHandle(this, poHandleHelper);
    }
    return nullptr;
}


/************************************************************************/
/*                       VSIOSSStreamingFSHandler                        */
/************************************************************************/

class VSIOSSStreamingFSHandler final: public IVSIS3LikeStreamingFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSStreamingFSHandler)

    std::map< CPLString, VSIOSSUpdateParams > oMapBucketsToOSSParams{};

  protected:
    CPLString GetFSPrefix() override { return "/vsioss_streaming/"; }
    VSICurlStreamingHandle* CreateFileHandle( const char* pszURL ) override;

  public:
    VSIOSSStreamingFSHandler() = default;
    ~VSIOSSStreamingFSHandler() override = default;

    const char* GetOptions() override
                        { return VSIGetFileSystemOptions("/vsioss/"); }

    void UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper ) override;
    void UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper ) override;
};

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIOSSStreamingFSHandler::UpdateMapFromHandle(
    IVSIS3LikeHandleHelper * poHandleHelper )
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

void VSIOSSStreamingFSHandler::UpdateHandleFromMap(
    IVSIS3LikeHandleHelper * poHandleHelper )
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
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSIOSSStreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    VSIOSSHandleHelper* poOSSHandleHelper =
            VSIOSSHandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str(),
                                            false);
    if( poOSSHandleHelper )
    {
        UpdateHandleFromMap(poOSSHandleHelper);
        return new VSIS3LikeStreamingHandle(this, poOSSHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                      VSISwiftStreamingFSHandler                      */
/************************************************************************/

class VSISwiftStreamingFSHandler final: public IVSIS3LikeStreamingFSHandler
{
  protected:
    CPLString GetFSPrefix() override { return "/vsiswift_streaming/"; }
    VSICurlStreamingHandle* CreateFileHandle( const char* pszURL ) override;

  public:
    VSISwiftStreamingFSHandler() {}
    ~VSISwiftStreamingFSHandler() override {}

    const char* GetOptions() override
                        { return VSIGetFileSystemOptions("/vsiswift/"); }
};

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlStreamingHandle *
VSISwiftStreamingFSHandler::CreateFileHandle( const char* pszURL )
{
    VSISwiftHandleHelper* poHandleHelper =
            VSISwiftHandleHelper::BuildFromURI(pszURL, GetFSPrefix().c_str());
    if( poHandleHelper )
    {
        return new VSIS3LikeStreamingHandle(this, poHandleHelper);
    }
    return nullptr;
}



//! @endcond

} /* end of anonymous namespace */

/************************************************************************/
/*                 VSIInstallCurlStreamingFileHandler()                 */
/************************************************************************/

/**
 * \brief Install /vsicurl_streaming/ HTTP/FTP file system handler (requires
 * libcurl).
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsicurl_streaming">/vsicurl_streaming/ documentation</a>
 *
 * @since GDAL 1.10
 */
void VSIInstallCurlStreamingFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsicurl_streaming/",
                                    new VSICurlStreamingFSHandler );
}

/************************************************************************/
/*                   VSIInstallS3StreamingFileHandler()                 */
/************************************************************************/

/**
 * \brief Install /vsis3_streaming/ Amazon S3 file system handler (requires
 * libcurl).
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsis3_streaming">/vsis3_streaming/ documentation</a>
 *
 * @since GDAL 2.1
 */
void VSIInstallS3StreamingFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsis3_streaming/",
                                    new VSIS3StreamingFSHandler );
}

/************************************************************************/
/*                      VSIInstallGSStreamingFileHandler()              */
/************************************************************************/

/**
 * \brief Install /vsigs_streaming/ Google Cloud Storage file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsigs_streaming">/vsigs_streaming/ documentation</a>
 *
 * @since GDAL 2.2
 */

void VSIInstallGSStreamingFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsigs_streaming/", new VSIGSStreamingFSHandler );
}

/************************************************************************/
/*                   VSIInstallAzureStreamingFileHandler()              */
/************************************************************************/

/**
 * \brief Install /vsiaz_streaming/ Microsoft Azure Blob file system handler
 * (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsiaz_streaming">/vsiaz_streaming/ documentation</a>1
 *
 * @since GDAL 2.3
 */

void VSIInstallAzureStreamingFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsiaz_streaming/",
                                    new VSIAzureStreamingFSHandler );
}

/************************************************************************/
/*                    VSIInstallOSSStreamingFileHandler()               */
/************************************************************************/

/**
 * \brief Install /vsioss_streaming/ Alibaba Cloud Object Storage Service (OSS)
 * file system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsioss_streaming">/vsioss_streaming/ documentation</a>
 *
 * @since GDAL 2.3
 */

void VSIInstallOSSStreamingFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsioss_streaming/",
                                    new VSIOSSStreamingFSHandler );
}

/************************************************************************/
/*                  VSIInstallSwiftStreamingFileHandler()               */
/************************************************************************/

/**
  * \brief Install /vsiswift_streamin/ OpenStack Swif Object Storage (Swift) file
 * system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsiswift_streaming">/vsiswift_streaming/ documentation</a>
 *
 * @since GDAL 2.3
 */

void VSIInstallSwiftStreamingFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsiswift_streaming/",
                                    new VSISwiftStreamingFSHandler );
}
//! @cond Doxygen_Suppress

/************************************************************************/
/*                      VSICurlStreamingClearCache()                    */
/************************************************************************/

void VSICurlStreamingClearCache( void )
{
    // FIXME ? Currently we have different filesystem instances for
    // vsicurl/, /vsis3/, /vsigs/ . So each one has its own cache of regions,
    // file size, etc.
    CSLConstList papszPrefix = VSIFileManager::GetPrefixes();
    for( size_t i = 0; papszPrefix && papszPrefix[i]; ++i )
    {
        auto poFSHandler =
            dynamic_cast<VSICurlStreamingFSHandler*>(
                VSIFileManager::GetHandler( papszPrefix[i] ));

        if( poFSHandler )
            poFSHandler->ClearCache();
    }
}

//! @endcond

#endif  // !defined(HAVE_CURL) || defined(CPL_MULTIPROC_STUB)
