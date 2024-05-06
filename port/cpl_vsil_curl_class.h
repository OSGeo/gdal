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
#include "cpl_azure.h"
#include "cpl_port.h"
#include "cpl_json.h"
#include "cpl_string.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_mem_cache.h"

#include "cpl_curl_priv.h"

#include <algorithm>
#include <condition_variable>
#include <set>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// To avoid aliasing to CopyFile to CopyFileA on Windows
#ifdef CopyFile
#undef CopyFile
#endif

//! @cond Doxygen_Suppress

// Leave it for backward compatibility, but deprecate.
#define HAVE_CURLINFO_REDIRECT_URL

void VSICurlStreamingClearCache(void);  // from cpl_vsil_curl_streaming.cpp

struct curl_slist *VSICurlSetOptions(CURL *hCurlHandle, const char *pszURL,
                                     const char *const *papszOptions);
struct curl_slist *VSICurlMergeHeaders(struct curl_slist *poDest,
                                       struct curl_slist *poSrcToDestroy);

struct curl_slist *VSICurlSetContentTypeFromExt(struct curl_slist *polist,
                                                const char *pszPath);

struct curl_slist *VSICurlSetCreationHeadersFromOptions(
    struct curl_slist *headers, CSLConstList papszOptions, const char *pszPath);

namespace cpl
{

typedef enum
{
    EXIST_UNKNOWN = -1,
    EXIST_NO,
    EXIST_YES,
} ExistStatus;

class FileProp
{
  public:
    unsigned int nGenerationAuthParameters = 0;
    ExistStatus eExists = EXIST_UNKNOWN;
    int nHTTPCode = 0;
    vsi_l_offset fileSize = 0;
    time_t mTime = 0;
    time_t nExpireTimestampLocal = 0;
    std::string osRedirectURL{};
    bool bHasComputedFileSize = false;
    bool bIsDirectory = false;
    int nMode = 0;  // st_mode member of struct stat
    bool bS3LikeRedirect = false;
    std::string ETag{};
};

struct CachedDirList
{
    bool bGotFileList = false;
    unsigned int nGenerationAuthParameters = 0;
    CPLStringList oFileList{}; /* only file name without path */
};

struct WriteFuncStruct
{
    char *pBuffer = nullptr;
    size_t nSize = 0;
    bool bIsHTTP = false;
    bool bMultiRange = false;
    vsi_l_offset nStartOffset = 0;
    vsi_l_offset nEndOffset = 0;
    int nHTTPCode = 0;
    vsi_l_offset nContentLength = 0;
    bool bFoundContentRange = false;
    bool bError = false;
    bool bInterruptDownload = false;
    bool bDetectRangeDownloadingError = false;
    GIntBig nTimestampDate = 0;  // Corresponds to Date: header field

    VSILFILE *fp = nullptr;
    VSICurlReadCbkFunc pfnReadCbk = nullptr;
    void *pReadCbkUserData = nullptr;
    bool bInterrupted = false;
};

struct PutData
{
    const GByte *pabyData = nullptr;
    size_t nOff = 0;
    size_t nTotalSize = 0;

    static size_t ReadCallBackBuffer(char *buffer, size_t size, size_t nitems,
                                     void *instream)
    {
        PutData *poThis = static_cast<PutData *>(instream);
        const size_t nSizeMax = size * nitems;
        const size_t nSizeToWrite =
            std::min(nSizeMax, poThis->nTotalSize - poThis->nOff);
        memcpy(buffer, poThis->pabyData + poThis->nOff, nSizeToWrite);
        poThis->nOff += nSizeToWrite;
        return nSizeToWrite;
    }
};

/************************************************************************/
/*                     VSICurlFilesystemHandler                         */
/************************************************************************/

class VSICurlHandle;

class VSICurlFilesystemHandlerBase : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlFilesystemHandlerBase)

    struct FilenameOffsetPair
    {
        std::string filename_;
        vsi_l_offset offset_;

        FilenameOffsetPair(const std::string &filename, vsi_l_offset offset)
            : filename_(filename), offset_(offset)
        {
        }

        bool operator==(const FilenameOffsetPair &other) const
        {
            return filename_ == other.filename_ && offset_ == other.offset_;
        }
    };

    struct FilenameOffsetPairHasher
    {
        std::size_t operator()(const FilenameOffsetPair &k) const
        {
            return std::hash<std::string>()(k.filename_) ^
                   std::hash<vsi_l_offset>()(k.offset_);
        }
    };

    using RegionCacheType = lru11::Cache<
        FilenameOffsetPair, std::shared_ptr<std::string>, lru11::NullLock,
        std::unordered_map<
            FilenameOffsetPair,
            typename std::list<lru11::KeyValuePair<
                FilenameOffsetPair, std::shared_ptr<std::string>>>::iterator,
            FilenameOffsetPairHasher>>;

    std::unique_ptr<RegionCacheType>
        m_poRegionCacheDoNotUseDirectly{};  // do not access directly. Use
                                            // GetRegionCache();
    RegionCacheType *GetRegionCache();

    // LRU cache that just keeps in memory if this file system handler is
    // spposed to know the file properties of a file. The actual cache is a
    // shared one among all network file systems.
    // The aim of that design is that invalidating /vsis3/foo results in
    // /vsis3_streaming/foo to be invalidated as well.
    lru11::Cache<std::string, bool> oCacheFileProp;

    int nCachedFilesInDirList = 0;
    lru11::Cache<std::string, CachedDirList> oCacheDirList;

    char **ParseHTMLFileList(const char *pszFilename, int nMaxFiles,
                             char *pszData, bool *pbGotFileList);

    // Data structure and map to store regions that are in progress, to
    // avoid simultaneous downloads of the same region in different threads
    // Cf https://github.com/OSGeo/gdal/issues/8041
    struct RegionInDownload
    {
        std::mutex oMutex{};
        std::condition_variable oCond{};
        bool bDownloadInProgress = false;
        int nWaiters = 0;
        std::string osData{};
    };

    std::mutex m_oMutex{};
    std::map<std::string, std::unique_ptr<RegionInDownload>>
        m_oMapRegionInDownload{};

  protected:
    CPLMutex *hMutex = nullptr;

    virtual VSICurlHandle *CreateFileHandle(const char *pszFilename);
    virtual char **GetFileList(const char *pszFilename, int nMaxFiles,
                               bool *pbGotFileList);

    void RegisterEmptyDir(const std::string &osDirname);

    bool
    AnalyseS3FileList(const std::string &osBaseURL, const char *pszXML,
                      CPLStringList &osFileList, int nMaxFiles,
                      const std::set<std::string> &oSetIgnoredStorageClasses,
                      bool &bIsTruncated);

    void AnalyseSwiftFileList(const std::string &osBaseURL,
                              const std::string &osPrefix, const char *pszJson,
                              CPLStringList &osFileList, int nMaxFilesThisQuery,
                              int nMaxFiles, bool &bIsTruncated,
                              std::string &osNextMarker);

    static const char *GetOptionsStatic();

    VSICurlFilesystemHandlerBase();

  public:
    ~VSICurlFilesystemHandlerBase() override;

    static bool IsAllowedFilename(const char *pszFilename);

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Rename(const char *oldpath, const char *newpath) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
    char **SiblingFiles(const char *pszFilename) override;

    int HasOptimizedReadMultiRange(const char * /* pszPath */) override
    {
        return true;
    }

    const char *GetActualURL(const char *pszFilename) override;

    const char *GetOptions() override;

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

    char **ReadDirInternal(const char *pszDirname, int nMaxFiles,
                           bool *pbGotFileList);
    void InvalidateDirContent(const char *pszDirname);

    virtual const char *GetDebugKey() const = 0;

    virtual std::string GetFSPrefix() const = 0;
    virtual bool AllowCachedDataFor(const char *pszFilename);

    virtual bool IsLocal(const char * /* pszPath */) override
    {
        return false;
    }

    virtual bool
    SupportsSequentialWrite(const char * /* pszPath */,
                            bool /* bAllowLocalTempFile */) override
    {
        return false;
    }

    virtual bool SupportsRandomWrite(const char * /* pszPath */,
                                     bool /* bAllowLocalTempFile */) override
    {
        return false;
    }

    std::shared_ptr<std::string> GetRegion(const char *pszURL,
                                           vsi_l_offset nFileOffsetStart);

    void AddRegion(const char *pszURL, vsi_l_offset nFileOffsetStart,
                   size_t nSize, const char *pData);

    std::pair<bool, std::string>
    NotifyStartDownloadRegion(const std::string &osURL,
                              vsi_l_offset startOffset, int nBlocks);
    void NotifyStopDownloadRegion(const std::string &osURL,
                                  vsi_l_offset startOffset, int nBlocks,
                                  const std::string &osData);

    bool GetCachedFileProp(const char *pszURL, FileProp &oFileProp);
    void SetCachedFileProp(const char *pszURL, FileProp &oFileProp);
    void InvalidateCachedData(const char *pszURL);

    CURLM *GetCurlMultiHandleFor(const std::string &osURL);

    virtual void ClearCache();
    virtual void PartialClearCache(const char *pszFilename);

    bool GetCachedDirList(const char *pszURL, CachedDirList &oCachedDirList);
    void SetCachedDirList(const char *pszURL, CachedDirList &oCachedDirList);
    bool ExistsInCacheDirList(const std::string &osDirname, bool *pbIsDir);

    virtual std::string GetURLFromFilename(const std::string &osFilename);

    std::string
    GetStreamingFilename(const std::string &osFilename) const override = 0;

    static std::set<std::string> GetS3IgnoredStorageClasses();
};

class VSICurlFilesystemHandler : public VSICurlFilesystemHandlerBase
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlFilesystemHandler)

  public:
    VSICurlFilesystemHandler() = default;

    const char *GetDebugKey() const override
    {
        return "VSICURL";
    }

    std::string GetFSPrefix() const override
    {
        return "/vsicurl/";
    }

    std::string
    GetStreamingFilename(const std::string &osFilename) const override;
};

/************************************************************************/
/*                           VSICurlHandle                              */
/************************************************************************/

class VSICurlHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlHandle)

  protected:
    VSICurlFilesystemHandlerBase *poFS = nullptr;

    bool m_bCached = true;

    mutable FileProp oFileProp{};

    mutable std::mutex m_oMutex{};
    std::string m_osFilename{};  // e.g "/vsicurl/http://example.com/foo"
    char *m_pszURL = nullptr;    // e.g "http://example.com/foo"
    mutable std::string m_osQueryString{};  // e.g. an Azure SAS

    char **m_papszHTTPOptions = nullptr;

    vsi_l_offset lastDownloadedOffset = VSI_L_OFFSET_MAX;
    int nBlocksToDownload = 1;

    bool bStopOnInterruptUntilUninstall = false;
    bool bInterrupted = false;
    VSICurlReadCbkFunc pfnReadCbk = nullptr;
    void *pReadCbkUserData = nullptr;

    int m_nMaxRetry = 0;
    double m_dfRetryDelay = 0.0;

    CPLStringList m_aosHeaders{};

    void DownloadRegionPostProcess(const vsi_l_offset startOffset,
                                   const int nBlocks, const char *pBuffer,
                                   size_t nSize);

  private:
    vsi_l_offset curOffset = 0;

    bool bEOF = false;

    virtual std::string DownloadRegion(vsi_l_offset startOffset, int nBlocks);

    bool m_bUseHead = false;
    bool m_bUseRedirectURLIfNoQueryStringParams = false;

    // Specific to Planetary Computer signing:
    // https://planetarycomputer.microsoft.com/docs/concepts/sas/
    mutable bool m_bPlanetaryComputerURLSigning = false;
    mutable std::string m_osPlanetaryComputerCollection{};
    void ManagePlanetaryComputerSigning() const;

    int ReadMultiRangeSingleGet(int nRanges, void **ppData,
                                const vsi_l_offset *panOffsets,
                                const size_t *panSizes);
    std::string GetRedirectURLIfValid(bool &bHasExpired) const;

    void UpdateRedirectInfo(CURL *hCurlHandle,
                            const WriteFuncStruct &sWriteFuncHeaderData);

    // Used by AdviseRead()
    struct AdviseReadRange
    {
        bool bDone = false;
        std::mutex oMutex{};
        std::condition_variable oCV{};
        vsi_l_offset nStartOffset = 0;
        size_t nSize = 0;
        std::vector<GByte> abyData{};
    };

    std::vector<std::unique_ptr<AdviseReadRange>> m_aoAdviseReadRanges{};
    std::thread m_oThreadAdviseRead{};

  protected:
    virtual struct curl_slist *
    GetCurlHeaders(const std::string & /*osVerb*/,
                   const struct curl_slist * /* psExistingHeaders */)
    {
        return nullptr;
    }

    virtual bool AllowAutomaticRedirection()
    {
        return true;
    }

    virtual bool CanRestartOnError(const char *, const char *, bool)
    {
        return false;
    }

    virtual bool UseLimitRangeGetInsteadOfHead()
    {
        return false;
    }

    virtual bool IsDirectoryFromExists(const char * /*pszVerb*/,
                                       int /*response_code*/)
    {
        return false;
    }

    virtual void ProcessGetFileSizeResult(const char * /* pszContent */)
    {
    }

    void SetURL(const char *pszURL);

    virtual bool Authenticate(const char * /* pszFilename */)
    {
        return false;
    }

  public:
    VSICurlHandle(VSICurlFilesystemHandlerBase *poFS, const char *pszFilename,
                  const char *pszURLIn = nullptr);
    ~VSICurlHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    int ReadMultiRange(int nRanges, void **ppData,
                       const vsi_l_offset *panOffsets,
                       const size_t *panSizes) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    bool HasPRead() const override
    {
        return true;
    }

    size_t PRead(void *pBuffer, size_t nSize,
                 vsi_l_offset nOffset) const override;

    void AdviseRead(int nRanges, const vsi_l_offset *panOffsets,
                    const size_t *panSizes) override;

    size_t GetAdviseReadTotalBytesLimit() const override;

    bool IsKnownFileSize() const
    {
        return oFileProp.bHasComputedFileSize;
    }

    vsi_l_offset GetFileSizeOrHeaders(bool bSetError, bool bGetHeaders);

    virtual vsi_l_offset GetFileSize(bool bSetError)
    {
        return GetFileSizeOrHeaders(bSetError, false);
    }

    bool Exists(bool bSetError);

    bool IsDirectory() const
    {
        return oFileProp.bIsDirectory;
    }

    int GetMode() const
    {
        return oFileProp.nMode;
    }

    time_t GetMTime() const
    {
        return oFileProp.mTime;
    }

    const CPLStringList &GetHeaders()
    {
        return m_aosHeaders;
    }

    int InstallReadCbk(VSICurlReadCbkFunc pfnReadCbk, void *pfnUserData,
                       int bStopOnInterruptUntilUninstall);
    int UninstallReadCbk();

    const char *GetURL() const
    {
        return m_pszURL;
    }
};

/************************************************************************/
/*                  VSICurlFilesystemHandlerBaseWritable                */
/************************************************************************/

class VSICurlFilesystemHandlerBaseWritable : public VSICurlFilesystemHandlerBase
{
    CPL_DISALLOW_COPY_ASSIGN(VSICurlFilesystemHandlerBaseWritable)

  protected:
    VSICurlFilesystemHandlerBaseWritable() = default;

    virtual VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename, CSLConstList papszOptions) = 0;

  public:
    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError, CSLConstList papszOptions) override;

    bool SupportsSequentialWrite(const char * /* pszPath */,
                                 bool /* bAllowLocalTempFile */) override
    {
        return true;
    }

    bool SupportsRandomWrite(const char * /* pszPath */,
                             bool /* bAllowLocalTempFile */) override;
};

/************************************************************************/
/*                        IVSIS3LikeFSHandler                           */
/************************************************************************/

class IVSIS3LikeFSHandler : public VSICurlFilesystemHandlerBaseWritable
{
    CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeFSHandler)

    virtual int MkdirInternal(const char *pszDirname, long nMode,
                              bool bDoStatCheck);

  protected:
    char **GetFileList(const char *pszFilename, int nMaxFiles,
                       bool *pbGotFileList) override;

    virtual IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                                       bool bAllowNoObject) = 0;

    virtual int CopyObject(const char *oldpath, const char *newpath,
                           CSLConstList papszMetadata);

    int RmdirRecursiveInternal(const char *pszDirname, int nBatchSize);

    virtual bool
    IsAllowedHeaderForObjectCreation(const char * /* pszHeaderName */)
    {
        return false;
    }

    IVSIS3LikeFSHandler() = default;

  public:
    int Unlink(const char *pszFilename) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Rename(const char *oldpath, const char *newpath) override;

    virtual int CopyFile(const char *pszSource, const char *pszTarget,
                         VSILFILE *fpSource, vsi_l_offset nSourceSize,
                         const char *const *papszOptions,
                         GDALProgressFunc pProgressFunc,
                         void *pProgressData) override;

    virtual int DeleteObject(const char *pszFilename);

    virtual int *DeleteObjectBatch(CSLConstList papszFilesOrDirs);

    bool Sync(const char *pszSource, const char *pszTarget,
              const char *const *papszOptions, GDALProgressFunc pProgressFunc,
              void *pProgressData, char ***ppapszOutputs) override;

    VSIDIR *OpenDir(const char *pszPath, int nRecurseDepth,
                    const char *const *papszOptions) override;

    // Multipart upload
    virtual bool SupportsParallelMultipartUpload() const
    {
        return false;
    }

    virtual std::string InitiateMultipartUpload(
        const std::string &osFilename, IVSIS3LikeHandleHelper *poS3HandleHelper,
        int nMaxRetry, double dfRetryDelay, CSLConstList papszOptions);
    virtual std::string
    UploadPart(const std::string &osFilename, int nPartNumber,
               const std::string &osUploadID, vsi_l_offset nPosition,
               const void *pabyBuffer, size_t nBufferSize,
               IVSIS3LikeHandleHelper *poS3HandleHelper, int nMaxRetry,
               double dfRetryDelay, CSLConstList papszOptions);
    virtual bool CompleteMultipart(const std::string &osFilename,
                                   const std::string &osUploadID,
                                   const std::vector<std::string> &aosEtags,
                                   vsi_l_offset nTotalSize,
                                   IVSIS3LikeHandleHelper *poS3HandleHelper,
                                   int nMaxRetry, double dfRetryDelay);
    virtual bool AbortMultipart(const std::string &osFilename,
                                const std::string &osUploadID,
                                IVSIS3LikeHandleHelper *poS3HandleHelper,
                                int nMaxRetry, double dfRetryDelay);

    bool AbortPendingUploads(const char *pszFilename) override;
};

/************************************************************************/
/*                          IVSIS3LikeHandle                            */
/************************************************************************/

class IVSIS3LikeHandle : public VSICurlHandle
{
    CPL_DISALLOW_COPY_ASSIGN(IVSIS3LikeHandle)

  protected:
    bool UseLimitRangeGetInsteadOfHead() override
    {
        return true;
    }

    bool IsDirectoryFromExists(const char *pszVerb, int response_code) override
    {
        // A bit dirty, but on S3, a GET on a existing directory returns a 416
        return response_code == 416 && EQUAL(pszVerb, "GET") &&
               std::string(m_pszURL).back() == '/';
    }

    void ProcessGetFileSizeResult(const char *pszContent) override
    {
        oFileProp.bIsDirectory =
            strstr(pszContent, "ListBucketResult") != nullptr;
    }

  public:
    IVSIS3LikeHandle(VSICurlFilesystemHandlerBase *poFSIn,
                     const char *pszFilename, const char *pszURLIn)
        : VSICurlHandle(poFSIn, pszFilename, pszURLIn)
    {
    }

    ~IVSIS3LikeHandle() override
    {
    }
};

/************************************************************************/
/*                            VSIS3WriteHandle                          */
/************************************************************************/

class VSIS3WriteHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIS3WriteHandle)

    IVSIS3LikeFSHandler *m_poFS = nullptr;
    std::string m_osFilename{};
    IVSIS3LikeHandleHelper *m_poS3HandleHelper = nullptr;
    bool m_bUseChunked = false;
    CPLStringList m_aosOptions{};
    CPLStringList m_aosHTTPOptions{};

    vsi_l_offset m_nCurOffset = 0;
    int m_nBufferOff = 0;
    int m_nBufferSize = 0;
    bool m_bClosed = false;
    GByte *m_pabyBuffer = nullptr;
    std::string m_osUploadID{};
    int m_nPartNumber = 0;
    std::vector<std::string> m_aosEtags{};
    bool m_bError = false;

    CURLM *m_hCurlMulti = nullptr;
    CURL *m_hCurl = nullptr;
    const void *m_pBuffer = nullptr;
    std::string m_osCurlErrBuf{};
    size_t m_nChunkedBufferOff = 0;
    size_t m_nChunkedBufferSize = 0;
    size_t m_nWrittenInPUT = 0;

    int m_nMaxRetry = 0;
    double m_dfRetryDelay = 0.0;
    WriteFuncStruct m_sWriteFuncHeaderData{};

    bool UploadPart();
    bool DoSinglePartPUT();

    static size_t ReadCallBackBufferChunked(char *buffer, size_t size,
                                            size_t nitems, void *instream);
    size_t WriteChunked(const void *pBuffer, size_t nSize, size_t nMemb);
    int FinishChunkedTransfer();

    void InvalidateParentDirectory();

  public:
    VSIS3WriteHandle(IVSIS3LikeFSHandler *poFS, const char *pszFilename,
                     IVSIS3LikeHandleHelper *poS3HandleHelper, bool bUseChunked,
                     CSLConstList papszOptions);
    ~VSIS3WriteHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Close() override;

    bool IsOK()
    {
        return m_bUseChunked || m_pabyBuffer != nullptr;
    }
};

/************************************************************************/
/*                        VSIAppendWriteHandle                          */
/************************************************************************/

class VSIAppendWriteHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIAppendWriteHandle)

  protected:
    VSICurlFilesystemHandlerBase *m_poFS = nullptr;
    std::string m_osFSPrefix{};
    std::string m_osFilename{};

    vsi_l_offset m_nCurOffset = 0;
    int m_nBufferOff = 0;
    int m_nBufferSize = 0;
    int m_nBufferOffReadCallback = 0;
    bool m_bClosed = false;
    GByte *m_pabyBuffer = nullptr;
    bool m_bError = false;

    static size_t ReadCallBackBuffer(char *buffer, size_t size, size_t nitems,
                                     void *instream);
    virtual bool Send(bool bIsLastBlock) = 0;

  public:
    VSIAppendWriteHandle(VSICurlFilesystemHandlerBase *poFS,
                         const char *pszFSPrefix, const char *pszFilename,
                         int nChunkSize);
    virtual ~VSIAppendWriteHandle();

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Close() override;

    bool IsOK()
    {
        return m_pabyBuffer != nullptr;
    }
};

/************************************************************************/
/*                     VSIDIRWithMissingDirSynthesis                    */
/************************************************************************/

struct VSIDIRWithMissingDirSynthesis : public VSIDIR
{
    std::vector<std::unique_ptr<VSIDIREntry>> aoEntries{};

  protected:
    std::vector<std::string> m_aosSubpathsStack{};

    void SynthetizeMissingDirectories(const std::string &osCurSubdir,
                                      bool bAddEntryForThisSubdir);
};

/************************************************************************/
/*                         CurlRequestHelper                            */
/************************************************************************/

struct CurlRequestHelper
{
    WriteFuncStruct sWriteFuncData{};
    WriteFuncStruct sWriteFuncHeaderData{};
    char szCurlErrBuf[CURL_ERROR_SIZE + 1] = {};

    CurlRequestHelper();
    ~CurlRequestHelper();
    long perform(CURL *hCurlHandle,
                 struct curl_slist *headers,  // ownership transferred
                 VSICurlFilesystemHandlerBase *poFS,
                 IVSIS3LikeHandleHelper *poS3HandleHelper);
};

/************************************************************************/
/*                       NetworkStatisticsLogger                        */
/************************************************************************/

class NetworkStatisticsLogger
{
    static int gnEnabled;
    static NetworkStatisticsLogger gInstance;

    NetworkStatisticsLogger() = default;

    std::mutex m_mutex{};

    struct Counters
    {
        GIntBig nHEAD = 0;
        GIntBig nGET = 0;
        GIntBig nPUT = 0;
        GIntBig nPOST = 0;
        GIntBig nDELETE = 0;
        GIntBig nGETDownloadedBytes = 0;
        GIntBig nPUTUploadedBytes = 0;
        GIntBig nPOSTDownloadedBytes = 0;
        GIntBig nPOSTUploadedBytes = 0;
    };

    enum class ContextPathType
    {
        FILESYSTEM,
        FILE,
        ACTION,
    };

    struct ContextPathItem
    {
        ContextPathType eType;
        std::string osName;

        ContextPathItem(ContextPathType eTypeIn, const std::string &osNameIn)
            : eType(eTypeIn), osName(osNameIn)
        {
        }

        bool operator<(const ContextPathItem &other) const
        {
            if (static_cast<int>(eType) < static_cast<int>(other.eType))
                return true;
            if (static_cast<int>(eType) > static_cast<int>(other.eType))
                return false;
            return osName < other.osName;
        }
    };

    struct Stats
    {
        Counters counters{};
        std::map<ContextPathItem, Stats> children{};

        void AsJSON(CPLJSONObject &oJSON) const;
    };

    // Workaround bug in Coverity Scan
    // coverity[generated_default_constructor_used_in_field_initializer]
    Stats m_stats{};
    std::map<GIntBig, std::vector<ContextPathItem>>
        m_mapThreadIdToContextPath{};

    static void ReadEnabled();

    std::vector<Counters *> GetCountersForContext();

  public:
    static inline bool IsEnabled()
    {
        if (gnEnabled < 0)
        {
            ReadEnabled();
        }
        return gnEnabled == TRUE;
    }

    static void EnterFileSystem(const char *pszName);

    static void LeaveFileSystem();

    static void EnterFile(const char *pszName);

    static void LeaveFile();

    static void EnterAction(const char *pszName);

    static void LeaveAction();

    static void LogHEAD();

    static void LogGET(size_t nDownloadedBytes);

    static void LogPUT(size_t nUploadedBytes);

    static void LogPOST(size_t nUploadedBytes, size_t nDownloadedBytes);

    static void LogDELETE();

    static void Reset();

    static std::string GetReportAsSerializedJSON();
};

struct NetworkStatisticsFileSystem
{
    inline explicit NetworkStatisticsFileSystem(const char *pszName)
    {
        NetworkStatisticsLogger::EnterFileSystem(pszName);
    }

    inline ~NetworkStatisticsFileSystem()
    {
        NetworkStatisticsLogger::LeaveFileSystem();
    }
};

struct NetworkStatisticsFile
{
    inline explicit NetworkStatisticsFile(const char *pszName)
    {
        NetworkStatisticsLogger::EnterFile(pszName);
    }

    inline ~NetworkStatisticsFile()
    {
        NetworkStatisticsLogger::LeaveFile();
    }
};

struct NetworkStatisticsAction
{
    inline explicit NetworkStatisticsAction(const char *pszName)
    {
        NetworkStatisticsLogger::EnterAction(pszName);
    }

    inline ~NetworkStatisticsAction()
    {
        NetworkStatisticsLogger::LeaveAction();
    }
};

}  // namespace cpl

int VSICURLGetDownloadChunkSize();

void VSICURLInitWriteFuncStruct(cpl::WriteFuncStruct *psStruct, VSILFILE *fp,
                                VSICurlReadCbkFunc pfnReadCbk,
                                void *pReadCbkUserData);
size_t VSICurlHandleWriteFunc(void *buffer, size_t count, size_t nmemb,
                              void *req);
void VSICURLMultiPerform(CURLM *hCurlMultiHandle, CURL *hEasyHandle = nullptr);
void VSICURLResetHeaderAndWriterFunctions(CURL *hCurlHandle);

int VSICurlParseUnixPermissions(const char *pszPermissions);

// Cache of file properties (size, etc.)
bool VSICURLGetCachedFileProp(const char *pszURL, cpl::FileProp &oFileProp);
void VSICURLSetCachedFileProp(const char *pszURL, cpl::FileProp &oFileProp);
void VSICURLInvalidateCachedFileProp(const char *pszURL);
void VSICURLInvalidateCachedFilePropPrefix(const char *pszURL);
void VSICURLDestroyCacheFileProp();

void VSICURLMultiCleanup(CURLM *hCurlMultiHandle);

//! @endcond

#endif  // HAVE_CURL

#endif  // CPL_VSIL_CURL_CLASS_H_INCLUDED
