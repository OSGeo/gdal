/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Declarations for classes related to the virtual filesystem.
 *           These would only be normally required by applications implementing
 *           their own virtual file system classes which should be rare.
 *           The class interface may be fragile through versions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef CPL_VSI_VIRTUAL_H_INCLUDED
#define CPL_VSI_VIRTUAL_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#include <map>
#include <memory>
#include <vector>
#include <string>

// To avoid aliasing to GetDiskFreeSpace to GetDiskFreeSpaceA on Windows
#ifdef GetDiskFreeSpace
#undef GetDiskFreeSpace
#endif

// To avoid aliasing to CopyFile to CopyFileA on Windows
#ifdef CopyFile
#undef CopyFile
#endif

/************************************************************************/
/*                           VSIVirtualHandle                           */
/************************************************************************/

/** Virtual file handle */
struct CPL_DLL VSIVirtualHandle
{
  public:
    virtual int Seek(vsi_l_offset nOffset, int nWhence) = 0;
    virtual vsi_l_offset Tell() = 0;
    virtual size_t Read(void *pBuffer, size_t nSize, size_t nCount) = 0;
    virtual int ReadMultiRange(int nRanges, void **ppData,
                               const vsi_l_offset *panOffsets,
                               const size_t *panSizes);

    /** This method is called when code plans to access soon one or several
     * ranges in a file. Some file systems may be able to use this hint to
     * for example asynchronously start such requests.
     *
     * Offsets may be given in a non-increasing order, and may potentially
     * overlap.
     *
     * @param nRanges Size of the panOffsets and panSizes arrays.
     * @param panOffsets Array containing the start offset of each range.
     * @param panSizes Array containing the size (in bytes) of each range.
     * @since GDAL 3.7
     */
    virtual void AdviseRead(CPL_UNUSED int nRanges,
                            CPL_UNUSED const vsi_l_offset *panOffsets,
                            CPL_UNUSED const size_t *panSizes)
    {
    }

    virtual size_t Write(const void *pBuffer, size_t nSize, size_t nCount) = 0;
    virtual int Eof() = 0;
    virtual int Flush()
    {
        return 0;
    }
    virtual int Close() = 0;
    // Base implementation that only supports file extension.
    virtual int Truncate(vsi_l_offset nNewSize);
    virtual void *GetNativeFileDescriptor()
    {
        return nullptr;
    }
    virtual VSIRangeStatus GetRangeStatus(CPL_UNUSED vsi_l_offset nOffset,
                                          CPL_UNUSED vsi_l_offset nLength)
    {
        return VSI_RANGE_STATUS_UNKNOWN;
    }
    virtual bool HasPRead() const;
    virtual size_t PRead(void *pBuffer, size_t nSize,
                         vsi_l_offset nOffset) const;

    // NOTE: when adding new methods, besides the "actual" implementations,
    // also consider the VSICachedFile one.

    virtual ~VSIVirtualHandle()
    {
    }
};

/************************************************************************/
/*                        VSIVirtualHandleCloser                        */
/************************************************************************/

/** Helper close to use with a std:unique_ptr<VSIVirtualHandle>,
 *  such as VSIVirtualHandleUniquePtr. */
struct VSIVirtualHandleCloser
{
    /** Operator () that closes and deletes the file handle. */
    void operator()(VSIVirtualHandle *poHandle)
    {
        if (poHandle)
        {
            poHandle->Close();
            delete poHandle;
        }
    }
};

/** Unique pointer of VSIVirtualHandle that calls the Close() method */
typedef std::unique_ptr<VSIVirtualHandle, VSIVirtualHandleCloser>
    VSIVirtualHandleUniquePtr;

/************************************************************************/
/*                         VSIFilesystemHandler                         */
/************************************************************************/

#ifndef DOXYGEN_SKIP
class CPL_DLL VSIFilesystemHandler
{

  public:
    virtual ~VSIFilesystemHandler()
    {
    }

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess);

    virtual VSIVirtualHandle *Open(const char *pszFilename,
                                   const char *pszAccess, bool bSetError,
                                   CSLConstList papszOptions) = 0;
    virtual int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                     int nFlags) = 0;
    virtual int Unlink(const char *pszFilename)
    {
        (void)pszFilename;
        errno = ENOENT;
        return -1;
    }
    virtual int *UnlinkBatch(CSLConstList papszFiles);
    virtual int Mkdir(const char *pszDirname, long nMode)
    {
        (void)pszDirname;
        (void)nMode;
        errno = ENOENT;
        return -1;
    }
    virtual int Rmdir(const char *pszDirname)
    {
        (void)pszDirname;
        errno = ENOENT;
        return -1;
    }
    virtual int RmdirRecursive(const char *pszDirname);
    char **ReadDir(const char *pszDirname)
    {
        return ReadDirEx(pszDirname, 0);
    }
    virtual char **ReadDirEx(const char * /*pszDirname*/, int /* nMaxFiles */)
    {
        return nullptr;
    }
    virtual char **SiblingFiles(const char * /*pszFilename*/)
    {
        return nullptr;
    }
    virtual int Rename(const char *oldpath, const char *newpath)
    {
        (void)oldpath;
        (void)newpath;
        errno = ENOENT;
        return -1;
    }
    virtual int IsCaseSensitive(const char *pszFilename)
    {
        (void)pszFilename;
        return TRUE;
    }
    virtual GIntBig GetDiskFreeSpace(const char * /* pszDirname */)
    {
        return -1;
    }
    virtual int SupportsSparseFiles(const char * /* pszPath */)
    {
        return FALSE;
    }
    virtual int HasOptimizedReadMultiRange(const char * /* pszPath */)
    {
        return FALSE;
    }
    virtual const char *GetActualURL(const char * /*pszFilename*/)
    {
        return nullptr;
    }
    virtual const char *GetOptions()
    {
        return nullptr;
    }
    virtual char *GetSignedURL(const char * /*pszFilename*/,
                               CSLConstList /* papszOptions */)
    {
        return nullptr;
    }
    virtual bool Sync(const char *pszSource, const char *pszTarget,
                      const char *const *papszOptions,
                      GDALProgressFunc pProgressFunc, void *pProgressData,
                      char ***ppapszOutputs);

    virtual int CopyFile(const char *pszSource, const char *pszTarget,
                         VSILFILE *fpSource, vsi_l_offset nSourceSize,
                         const char *const *papszOptions,
                         GDALProgressFunc pProgressFunc, void *pProgressData);

    virtual VSIDIR *OpenDir(const char *pszPath, int nRecurseDepth,
                            const char *const *papszOptions);

    virtual char **GetFileMetadata(const char *pszFilename,
                                   const char *pszDomain,
                                   CSLConstList papszOptions);

    virtual bool SetFileMetadata(const char *pszFilename,
                                 CSLConstList papszMetadata,
                                 const char *pszDomain,
                                 CSLConstList papszOptions);

    virtual bool AbortPendingUploads(const char * /*pszFilename*/)
    {
        return true;
    }

    virtual std::string
    GetStreamingFilename(const std::string &osFilename) const
    {
        return osFilename;
    }

    /** Return the canonical filename.
     *
     * May be implemented by case-insensitive filesystems
     * (currently Win32 and MacOSX)
     * to return the filename with its actual case (i.e. the one that would
     * be used when listing the content of the directory).
     */
    virtual std::string
    GetCanonicalFilename(const std::string &osFilename) const
    {
        return osFilename;
    }

    virtual bool IsLocal(const char * /* pszPath */)
    {
        return true;
    }
    virtual bool SupportsSequentialWrite(const char * /* pszPath */,
                                         bool /* bAllowLocalTempFile */)
    {
        return true;
    }
    virtual bool SupportsRandomWrite(const char * /* pszPath */,
                                     bool /* bAllowLocalTempFile */)
    {
        return true;
    }
    virtual bool SupportsRead(const char * /* pszPath */)
    {
        return true;
    }

    virtual VSIFilesystemHandler *Duplicate(const char * /* pszPrefix */)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Duplicate() not supported on this file system");
        return nullptr;
    }
};
#endif /* #ifndef DOXYGEN_SKIP */

/************************************************************************/
/*                            VSIFileManager                            */
/************************************************************************/

#ifndef DOXYGEN_SKIP
class CPL_DLL VSIFileManager
{
  private:
    VSIFilesystemHandler *poDefaultHandler = nullptr;
    std::map<std::string, VSIFilesystemHandler *> oHandlers{};

    VSIFileManager();

    static VSIFileManager *Get();

    CPL_DISALLOW_COPY_ASSIGN(VSIFileManager)

  public:
    ~VSIFileManager();

    static VSIFilesystemHandler *GetHandler(const char *);
    static void InstallHandler(const std::string &osPrefix,
                               VSIFilesystemHandler *);
    /* RemoveHandler is never defined. */
    /* static void RemoveHandler( const std::string& osPrefix ); */

    static char **GetPrefixes();
};
#endif /* #ifndef DOXYGEN_SKIP */

/************************************************************************/
/* ==================================================================== */
/*                       VSIArchiveFilesystemHandler                   */
/* ==================================================================== */
/************************************************************************/

#ifndef DOXYGEN_SKIP

class VSIArchiveEntryFileOffset
{
  public:
    virtual ~VSIArchiveEntryFileOffset();
};

typedef struct
{
    char *fileName;
    vsi_l_offset uncompressed_size;
    VSIArchiveEntryFileOffset *file_pos;
    int bIsDir;
    GIntBig nModifiedTime;
} VSIArchiveEntry;

class VSIArchiveContent
{
  public:
    time_t mTime = 0;
    vsi_l_offset nFileSize = 0;
    int nEntries = 0;
    VSIArchiveEntry *entries = nullptr;

    ~VSIArchiveContent();
};

class VSIArchiveReader
{
  public:
    virtual ~VSIArchiveReader();

    virtual int GotoFirstFile() = 0;
    virtual int GotoNextFile() = 0;
    virtual VSIArchiveEntryFileOffset *GetFileOffset() = 0;
    virtual GUIntBig GetFileSize() = 0;
    virtual CPLString GetFileName() = 0;
    virtual GIntBig GetModifiedTime() = 0;
    virtual int GotoFileOffset(VSIArchiveEntryFileOffset *pOffset) = 0;
};

class VSIArchiveFilesystemHandler : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIArchiveFilesystemHandler)

  protected:
    CPLMutex *hMutex = nullptr;
    /* We use a cache that contains the list of files contained in a VSIArchive
     * file as */
    /* unarchive.c is quite inefficient in listing them. This speeds up access
     * to VSIArchive files */
    /* containing ~1000 files like a CADRG product */
    std::map<CPLString, VSIArchiveContent *> oFileList{};

    virtual const char *GetPrefix() = 0;
    virtual std::vector<CPLString> GetExtensions() = 0;
    virtual VSIArchiveReader *CreateReader(const char *pszArchiveFileName) = 0;

  public:
    VSIArchiveFilesystemHandler();
    virtual ~VSIArchiveFilesystemHandler();

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Rename(const char *oldpath, const char *newpath) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;

    virtual const VSIArchiveContent *
    GetContentOfArchive(const char *archiveFilename,
                        VSIArchiveReader *poReader = nullptr);
    virtual char *SplitFilename(const char *pszFilename,
                                CPLString &osFileInArchive,
                                int bCheckMainFileExists);
    virtual VSIArchiveReader *OpenArchiveFile(const char *archiveFilename,
                                              const char *fileInArchiveName);
    virtual int FindFileInArchive(const char *archiveFilename,
                                  const char *fileInArchiveName,
                                  const VSIArchiveEntry **archiveEntry);

    virtual bool IsLocal(const char *pszPath) override;
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
};

/************************************************************************/
/*                              VSIDIR                                  */
/************************************************************************/

struct CPL_DLL VSIDIR
{
    VSIDIR() = default;
    virtual ~VSIDIR();

    virtual const VSIDIREntry *NextDirEntry() = 0;

  private:
    VSIDIR(const VSIDIR &) = delete;
    VSIDIR &operator=(const VSIDIR &) = delete;
};

#endif /* #ifndef DOXYGEN_SKIP */

VSIVirtualHandle CPL_DLL *
VSICreateBufferedReaderHandle(VSIVirtualHandle *poBaseHandle);
VSIVirtualHandle *
VSICreateBufferedReaderHandle(VSIVirtualHandle *poBaseHandle,
                              const GByte *pabyBeginningContent,
                              vsi_l_offset nCheatFileSize);
constexpr int VSI_CACHED_DEFAULT_CHUNK_SIZE = 32768;
VSIVirtualHandle CPL_DLL *
VSICreateCachedFile(VSIVirtualHandle *poBaseHandle,
                    size_t nChunkSize = VSI_CACHED_DEFAULT_CHUNK_SIZE,
                    size_t nCacheSize = 0);

const int CPL_DEFLATE_TYPE_GZIP = 0;
const int CPL_DEFLATE_TYPE_ZLIB = 1;
const int CPL_DEFLATE_TYPE_RAW_DEFLATE = 2;
VSIVirtualHandle CPL_DLL *VSICreateGZipWritable(VSIVirtualHandle *poBaseHandle,
                                                int nDeflateType,
                                                int bAutoCloseBaseHandle);

VSIVirtualHandle *VSICreateGZipWritable(VSIVirtualHandle *poBaseHandle,
                                        int nDeflateType,
                                        bool bAutoCloseBaseHandle, int nThreads,
                                        size_t nChunkSize,
                                        size_t nSOZIPIndexEltSize,
                                        std::vector<uint8_t> *panSOZIPIndex);

VSIVirtualHandle *
VSICreateUploadOnCloseFile(VSIVirtualHandleUniquePtr &&poWritableHandle,
                           VSIVirtualHandleUniquePtr &&poTmpFile,
                           const std::string &osTmpFilename);

#endif /* ndef CPL_VSI_VIRTUAL_H_INCLUDED */
