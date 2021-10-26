/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HDFS
 * Author:   James McClain, <jmcclain@azavea.com>
 *
 **********************************************************************
 * Copyright (c) 2010-2015, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Azavea
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !defined(_MSC_VER)
#include <unistd.h>
#endif

#include <cstring>
#include <climits>

#include "cpl_port.h"
#include "cpl_vsi.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$")

#ifdef HDFS_ENABLED

#include "hdfs.h"


/************************************************************************/
/* ==================================================================== */
/*                        VSIHdfsHandle                               */
/* ==================================================================== */
/************************************************************************/

#define SILENCE(expr) {\
  int hOldStderr = dup(2);\
  int hNewStderr = open("/dev/null", O_WRONLY);\
\
  if ((hOldStderr != -1) && (hNewStderr != -1) && (dup2(hNewStderr, 2) != -1)) {\
      close(hNewStderr);\
      expr;\
      dup2(hOldStderr, 2);\
      close(hOldStderr);\
  }\
  else {\
    if (hOldStderr != -1) close(hOldStderr);\
    if (hNewStderr != -1) close(hNewStderr);\
    expr;\
  }\
}

class VSIHdfsHandle final : public VSIVirtualHandle
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(VSIHdfsHandle)

    hdfsFile poFile = nullptr;
    hdfsFS poFilesystem = nullptr;
    std::string oFilename;
    bool bEOF = false;

  public:
#if __cplusplus >= 201103L
     static constexpr const char * VSIHDFS = "/vsihdfs/";
#else
     static const char * VSIHDFS = "/vsihdfs/";
#endif
     VSIHdfsHandle(hdfsFile poFile,
                   hdfsFS poFilesystem,
                   const char * pszFilename,
                   bool bReadOnly);
    ~VSIHdfsHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    vsi_l_offset Length();
    int Eof() override;
    int Flush() override;
    int Close() override;
};

VSIHdfsHandle::VSIHdfsHandle(hdfsFile _poFile,
                             hdfsFS _poFilesystem,
                             const char * pszFilename,
                             bool /*_bReadOnly*/)
  : poFile(_poFile), poFilesystem(_poFilesystem), oFilename(pszFilename)
{}

VSIHdfsHandle::~VSIHdfsHandle()
{
  Close();
}

int VSIHdfsHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
  bEOF = false;
  switch(nWhence) {
  case SEEK_SET:
    return hdfsSeek(poFilesystem, poFile, nOffset);
  case SEEK_CUR:
    return hdfsSeek(poFilesystem,
                    poFile,
                    nOffset + Tell());
  case SEEK_END:
    return hdfsSeek(poFilesystem,
                    poFile,
                    static_cast<tOffset>(Length()) - nOffset);
  default:
    return -1;
  }
}

vsi_l_offset
VSIHdfsHandle::Tell()
{
  return hdfsTell(poFilesystem, poFile);
}

size_t
VSIHdfsHandle::Read(void *pBuffer, size_t nSize, size_t nMemb)
{
  if (nSize == 0 || nMemb == 0)
    return 0;

  size_t bytes_wanted = nSize * nMemb;
  size_t bytes_read = 0;

  while (bytes_read < bytes_wanted)
    {
      tSize bytes = 0;
      size_t bytes_to_request = bytes_wanted - bytes_read;

      // The `Read` function can take 64-bit arguments for its
      // read-request size, whereas `hdfsRead` may only take a 32-bit
      // argument.  If the former requests an amount larger than can
      // be encoded in a signed 32-bit number, break the request into
      // 2GB batches.
      bytes = hdfsRead(poFilesystem, poFile,
                       static_cast<char*>(pBuffer) + bytes_read,
                       bytes_to_request > INT_MAX ? INT_MAX : bytes_to_request);

      if (bytes > 0) {
        bytes_read += bytes;
      }
      if (bytes == 0) {
        bEOF = true;
        return bytes_read/nSize;
      }
      else if (bytes < 0) {
        bEOF = false;
        return 0;
      }
    }

  return bytes_read/nSize;
}

size_t
VSIHdfsHandle::Write(const void *, size_t, size_t)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

vsi_l_offset
VSIHdfsHandle::Length()
{
  hdfsFileInfo * poInfo = hdfsGetPathInfo(poFilesystem, oFilename.c_str());
  if (poInfo != nullptr) {
    tOffset nSize = poInfo->mSize;
    hdfsFreeFileInfo(poInfo, 1);
    return static_cast<vsi_l_offset>(nSize);
  }
  return -1;
}

int
VSIHdfsHandle::Eof()
{
  return bEOF;
}

int
VSIHdfsHandle::Flush()
{
  return hdfsFlush(poFilesystem, poFile);
}

int
VSIHdfsHandle::Close()
{
  int retval = 0;

  if (poFilesystem != nullptr && poFile != nullptr)
    retval = hdfsCloseFile(poFilesystem, poFile);
  poFile = nullptr;
  poFilesystem = nullptr;

  return retval;
}

class VSIHdfsFilesystemHandler final : public VSIFilesystemHandler
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(VSIHdfsFilesystemHandler)

    hdfsFS poFilesystem = nullptr;
    CPLMutex *hMutex = nullptr;

  public:
    VSIHdfsFilesystemHandler();
    ~VSIHdfsFilesystemHandler() override;

    void EnsureFilesystem();
    VSIVirtualHandle *Open(const char *pszFilename,
                           const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;
    int Stat(const char *pszFilename,
             VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char ** ReadDir(const char *pszDirname) override;
    int Rename(const char *oldpath, const char *newpath) override;

};

VSIHdfsFilesystemHandler::VSIHdfsFilesystemHandler()
{}

VSIHdfsFilesystemHandler::~VSIHdfsFilesystemHandler()
{
  if(hMutex != nullptr) {
    CPLDestroyMutex(hMutex);
    hMutex = nullptr;
  }

  if (poFilesystem != nullptr)
    hdfsDisconnect(poFilesystem);
  poFilesystem = nullptr;
}

void
VSIHdfsFilesystemHandler::EnsureFilesystem() {
  CPLMutexHolder oHolder(&hMutex);
  if (poFilesystem == nullptr)
    poFilesystem = hdfsConnect("default", 0);
}

VSIVirtualHandle *
VSIHdfsFilesystemHandler::Open( const char *pszFilename,
                                const char *pszAccess,
                                bool,
                                CSLConstList /* papszOptions */ )
{
  EnsureFilesystem();

  if (strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr) {
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return nullptr;
  }

  if (strncmp(pszFilename, VSIHdfsHandle::VSIHDFS, strlen(VSIHdfsHandle::VSIHDFS)) != 0) {
    return nullptr;
  }
  else {
    const char * pszPath = pszFilename + strlen(VSIHdfsHandle::VSIHDFS);

    // Open HDFS file, sending Java stack traces to /dev/null.
    hdfsFile poFile = nullptr;
    SILENCE(poFile = hdfsOpenFile(poFilesystem, pszPath, O_RDONLY, 0, 0, 0));

    if (poFile != nullptr) {
      VSIHdfsHandle * poHandle = new VSIHdfsHandle(poFile, poFilesystem, pszPath, true);
      return poHandle;
    }
  }
  return nullptr;
}

int
VSIHdfsFilesystemHandler::Stat( const char *pszeFilename, VSIStatBufL *pStatBuf, int)
{
  EnsureFilesystem();

  hdfsFileInfo * poInfo = hdfsGetPathInfo(poFilesystem, pszeFilename + strlen(VSIHdfsHandle::VSIHDFS));

  if (poInfo != nullptr) {
    pStatBuf->st_dev = static_cast<dev_t>(0);                               /* ID of device containing file */
    pStatBuf->st_ino = static_cast<ino_t>(0);                               /* inode number */
    switch(poInfo->mKind) {                                                 /* protection */
    case tObjectKind::kObjectKindFile:
      pStatBuf->st_mode = S_IFREG;
      break;
    case tObjectKind::kObjectKindDirectory:
      pStatBuf->st_mode = S_IFDIR;
      break;
    default:
      CPLError(CE_Failure, CPLE_AppDefined, "Unrecognized object kind");
    }
    pStatBuf->st_nlink = static_cast<nlink_t>(0);                           /* number of hard links */
    pStatBuf->st_uid = getuid();                                            /* user ID of owner */
    pStatBuf->st_gid = getgid();                                            /* group ID of owner */
    pStatBuf->st_rdev = static_cast<dev_t>(0);                              /* device ID (if special file) */
    pStatBuf->st_size = static_cast<off_t>(poInfo->mSize);                  /* total size, in bytes */
    pStatBuf->st_blksize = static_cast<blksize_t>(poInfo->mBlockSize);      /* blocksize for filesystem I/O */
    pStatBuf->st_blocks = static_cast<blkcnt_t>((poInfo->mBlockSize>>9)+1); /* number of 512B blocks allocated */
    pStatBuf->st_atime = static_cast<time_t>(poInfo->mLastAccess);          /* time of last access */
    pStatBuf->st_mtime = static_cast<time_t>(poInfo->mLastMod);             /* time of last modification */
    pStatBuf->st_ctime = static_cast<time_t>(poInfo->mLastMod);             /* time of last status change */
    hdfsFreeFileInfo(poInfo, 1);
    return 0;
  }

  return -1;
}

int
VSIHdfsFilesystemHandler::Unlink(const char *)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

int
VSIHdfsFilesystemHandler::Mkdir(const char *, long)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

int
VSIHdfsFilesystemHandler::Rmdir(const char *)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

char **
VSIHdfsFilesystemHandler::ReadDir(const char *pszDirname)
{
  EnsureFilesystem();

  int mEntries = 0;
  hdfsFileInfo * paoInfo = hdfsListDirectory(poFilesystem, pszDirname + strlen(VSIHdfsHandle::VSIHDFS), &mEntries);
  char ** retval = nullptr;

  if (paoInfo != nullptr) {
    CPLStringList aosNames;
    for (int i = 0; i < mEntries; ++i)
      aosNames.AddString(paoInfo[i].mName);
    retval = aosNames.StealList();
    hdfsFreeFileInfo(paoInfo, mEntries);
    return retval;
  }
  return nullptr;
}

int
VSIHdfsFilesystemHandler::Rename(const char *, const char *)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

#endif

//! @endcond

#ifdef HDFS_ENABLED

/************************************************************************/
/*                       VSIInstallHdfsHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsihdfs/ file system handler (requires JVM and HDFS support)
 *
 * @since GDAL 2.4.0
 */
void VSIInstallHdfsHandler()
{
    VSIFileManager::InstallHandler(VSIHdfsHandle::VSIHDFS, new VSIHdfsFilesystemHandler);
}

#else

/************************************************************************/
/*                       VSIInstallHdfsHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsihdfs/ file system handler (non-functional stub)
 *
 * @since GDAL 2.4.0
 */
void VSIInstallHdfsHandler( void )
{
    // Not supported.
}

#endif
