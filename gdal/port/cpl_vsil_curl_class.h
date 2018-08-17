/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Declarations for /vsicurl/ and related file systems
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

#ifndef CPL_VSIL_CURL_CLASS_H_INCLUDED
#define CPL_VSIL_CURL_CLASS_H_INCLUDED

#ifdef HAVE_CURL

#include "cpl_aws.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_mem_cache.h"

#include <curl/curl.h>

#include <set>
#include <map>
#include <memory>

//! @cond Doxygen_Suppress

// 7.18.1
#if LIBCURL_VERSION_NUM >= 0x071201
#define HAVE_CURLINFO_REDIRECT_URL
#endif

void VSICurlStreamingClearCache( void ); // from cpl_vsil_curl_streaming.cpp

struct curl_slist* VSICurlSetOptions(CURL* hCurlHandle, const char* pszURL,
                       const char * const* papszOptions);
struct curl_slist* VSICurlMergeHeaders( struct curl_slist* poDest,
                                        struct curl_slist* poSrcToDestroy );

namespace cpl {

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

class CachedFileProp
{
  public:
    ExistStatus     eExists = EXIST_UNKNOWN;
    bool            bHasComputedFileSize = false;
    vsi_l_offset    fileSize = 0;
    bool            bIsDirectory = false;
    time_t          mTime = 0;
    bool            bS3LikeRedirect = false;
    time_t          nExpireTimestampLocal = 0;
    CPLString       osRedirectURL{};
};

typedef struct
{
    bool            bGotFileList;
    char**          papszFileList; /* only file name without path */
} CachedDirList;

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
    CPL_DISALLOW_COPY_ASSIGN(VSICurlFilesystemHandler)

    struct FilenameOffsetPair
    {
        std::string filename_;
        vsi_l_offset offset_;

        FilenameOffsetPair(const std::string& filename,
                           vsi_l_offset offset) :
            filename_(filename), offset_(offset) {}

        bool operator==(const FilenameOffsetPair& other) const
        {
            return filename_ == other.filename_ &&
                   offset_ == other.offset_;
        }
    };
    struct FilenameOffsetPairHasher
    {
        std::size_t operator()(const FilenameOffsetPair& k) const
        {
            return std::hash<std::string>()(k.filename_) ^
                   std::hash<vsi_l_offset>()(k.offset_);
        }
    };

    using CacheType =
        lru11::Cache<FilenameOffsetPair, std::shared_ptr<std::string>,
            lru11::NullLock,
            std::unordered_map<
                FilenameOffsetPair,
                typename std::list<lru11::KeyValuePair<FilenameOffsetPair,
                    std::shared_ptr<std::string>>>::iterator,
                    FilenameOffsetPairHasher>>;

    CacheType oRegionCache;

    std::map<CPLString, CachedFileProp*>   cacheFileSize{};
    std::map<CPLString, CachedDirList*>        cacheDirList{};

    // Per-thread Curl connection cache.
    std::map<GIntBig, CachedConnection*> mapConnections{};

    char**              ParseHTMLFileList(const char* pszFilename,
                                          int nMaxFiles,
                                          char* pszData,
                                          bool* pbGotFileList);

protected:
    CPLMutex       *hMutex = nullptr;

    virtual VSICurlHandle* CreateFileHandle(const char* pszFilename);
    virtual char** GetFileList(const char *pszFilename,
                               int nMaxFiles,
                               bool* pbGotFileList);
    virtual CPLString GetURLFromFilename( const CPLString& osFilename );

    void RegisterEmptyDir( const CPLString& osDirname );

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

    void AnalyseSwiftFileList( const CPLString& osBaseURL,
                               const CPLString& osPrefix,
                            const char* pszJson,
                            CPLStringList& osFileList,
                            int nMaxFilesThisQuery,
                            int nMaxFiles,
                            bool& bIsTruncated,
                            CPLString& osNextMarker );

    static const char* GetOptionsStatic();

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

    const char* GetActualURL(const char* pszFilename) override;

    const char* GetOptions() override;

    char **ReadDirInternal( const char *pszDirname, int nMaxFiles,
                            bool* pbGotFileList );
    void InvalidateDirContent( const char *pszDirname );

    virtual CPLString GetFSPrefix() { return "/vsicurl/"; }
    virtual bool      AllowCachedDataFor(const char* pszFilename);

    std::shared_ptr<std::string> GetRegion( const char* pszURL,
                                   vsi_l_offset nFileOffsetStart );

    void                AddRegion( const char* pszURL,
                                   vsi_l_offset nFileOffsetStart,
                                   size_t nSize,
                                   const char *pData );

    CachedFileProp*     GetCachedFileProp( const char* pszURL );
    void                InvalidateCachedData( const char* pszURL );

    CURLM              *GetCurlMultiHandleFor( const CPLString& osURL );

    virtual void        ClearCache();
    virtual void        PartialClearCache(const char* pszFilename);

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
    CPL_DISALLOW_COPY_ASSIGN(VSICurlHandle)

  protected:
    VSICurlFilesystemHandler* poFS = nullptr;

    bool            m_bCached = true;

    vsi_l_offset    fileSize = 0;
    bool            bHasComputedFileSize = false;
    ExistStatus     eExists = EXIST_UNKNOWN;
    bool            bIsDirectory = false;
    time_t          mTime = 0;
    CPLString       m_osFilename{}; // e.g "/vsicurl/http://example.com/foo"
    char*           m_pszURL = nullptr;     // e.g "http://example.com/foo"

    char          **m_papszHTTPOptions = nullptr;

    vsi_l_offset    lastDownloadedOffset = VSI_L_OFFSET_MAX;
    int             nBlocksToDownload = 1;

    bool                bStopOnInterruptUntilUninstall = false;
    bool                bInterrupted = false;
    VSICurlReadCbkFunc  pfnReadCbk = nullptr;
    void               *pReadCbkUserData = nullptr;

    int                 m_nMaxRetry = 0;
    double              m_dfRetryDelay = 0.0;

    void                DownloadRegionPostProcess( const vsi_l_offset startOffset,
                                                   const int nBlocks,
                                                   const char* pBuffer,
                                                   size_t nSize );

  private:

    vsi_l_offset    curOffset = 0;

    bool            bEOF = false;

    virtual bool            DownloadRegion(vsi_l_offset startOffset, int nBlocks);

    bool                m_bS3LikeRedirect = false;
    time_t              m_nExpireTimestampLocal = 0;
    CPLString           m_osRedirectURL{};

    bool                m_bUseHead = false;

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
    virtual vsi_l_offset GetFileSize( bool bSetError );
    bool                 Exists( bool bSetError );
    bool                 IsDirectory() const { return bIsDirectory; }
    time_t               GetMTime() const { return mTime; }

    int                  InstallReadCbk( VSICurlReadCbkFunc pfnReadCbk,
                                         void* pfnUserData,
                                         int bStopOnInterruptUntilUninstall );
    int                  UninstallReadCbk();

    const char          *GetURL() const { return m_pszURL; }
};

/************************************************************************/
/*                        IVSIS3LikeFSHandler                           */
/************************************************************************/

class IVSIS3LikeFSHandler: public VSICurlFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeFSHandler)

  protected:
    char** GetFileList( const char *pszFilename,
                        int nMaxFiles,
                        bool* pbGotFileList ) override;

    virtual IVSIS3LikeHandleHelper* CreateHandleHelper(
            const char* pszURI, bool bAllowNoObject) = 0;

    IVSIS3LikeFSHandler() = default;

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
/*                          IVSIS3LikeHandle                            */
/************************************************************************/

class IVSIS3LikeHandle:  public VSICurlHandle
{
    CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeHandle)

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
/*                            VSIS3WriteHandle                          */
/************************************************************************/

class VSIS3WriteHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3WriteHandle)

    IVSIS3LikeFSHandler     *m_poFS = nullptr;
    CPLString           m_osFilename{};
    IVSIS3LikeHandleHelper  *m_poS3HandleHelper = nullptr;
    bool                m_bUseChunked = false;

    vsi_l_offset        m_nCurOffset = 0;
    int                 m_nBufferOff = 0;
    int                 m_nBufferSize = 0;
    int                 m_nBufferOffReadCallback = 0;
    bool                m_bClosed = false;
    GByte              *m_pabyBuffer = nullptr;
    CPLString           m_osUploadID{};
    int                 m_nPartNumber = 0;
    std::vector<CPLString> m_aosEtags{};
    CPLString           m_osXML{};
    int                 m_nOffsetInXML = 0;
    bool                m_bError = false;

    CURLM              *m_hCurlMulti = nullptr;
    CURL               *m_hCurl = nullptr;
    const void         *m_pBuffer = nullptr;
    CPLString           m_osCurlErrBuf{};
    size_t              m_nChunkedBufferOff = 0;
    size_t              m_nChunkedBufferSize = 0;

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
/*                        VSIAppendWriteHandle                          */
/************************************************************************/

class VSIAppendWriteHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAppendWriteHandle)

    protected:

    VSICurlFilesystemHandler* m_poFS = nullptr;
    CPLString           m_osFSPrefix{};
    CPLString           m_osFilename{};

    vsi_l_offset        m_nCurOffset = 0;
    int                 m_nBufferOff = 0;
    int                 m_nBufferSize = 0;
    int                 m_nBufferOffReadCallback = 0;
    bool                m_bClosed = false;
    GByte              *m_pabyBuffer = nullptr;
    bool                m_bError = false;

    static size_t       ReadCallBackBuffer( char *buffer, size_t size,
                                            size_t nitems, void *instream );
    virtual bool        Send(bool bIsLastBlock) = 0;

    public:
        VSIAppendWriteHandle( VSICurlFilesystemHandler* poFS,
                              const char* pszFSPrefix,
                              const char* pszFilename,
                              int nChunkSize );
        virtual ~VSIAppendWriteHandle();

        int Seek( vsi_l_offset nOffset, int nWhence ) override;
        vsi_l_offset Tell() override;
        size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
        size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
        int  Eof() override;
        int  Close() override;

        bool              IsOK() { return m_pabyBuffer != nullptr; }
};

int VSICURLGetDownloadChunkSize();

void VSICURLInitWriteFuncStruct( WriteFuncStruct   *psStruct,
                                        VSILFILE          *fp,
                                        VSICurlReadCbkFunc pfnReadCbk,
                                        void              *pReadCbkUserData );
size_t VSICurlHandleWriteFunc( void *buffer, size_t count,
                                      size_t nmemb, void *req );
void MultiPerform(CURLM* hCurlMultiHandle,
                         CURL* hEasyHandle = nullptr);
void VSICURLResetHeaderAndWriterFunctions(CURL* hCurlHandle);

} // namespace cpl

//! @endcond

#endif // HAVE_CURL

#endif // CPL_VSIL_CURL_CLASS_H_INCLUDED
