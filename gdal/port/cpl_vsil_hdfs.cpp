/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for HDFS
 * Author:   James McClain, <jmcclain@azavea.com>
 *
 **********************************************************************
 * Copyright (c) 2010-2015, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <unistd.h>

#include <cstring>

#include "cpl_port.h"
#include "cpl_vsi.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$")

#ifdef HDFS_ENABLED

#include "hdfs.h"

const char * VSIHDFS = "/vsihdfs/";

/************************************************************************/
/* ==================================================================== */
/*                        VSIHdfsHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIHdfsHandle final : public VSIVirtualHandle
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(VSIHdfsHandle)

    hdfsFile poFile = nullptr;
    hdfsFS poFilesystem = nullptr;
    std::string oFilename;
    bool bReadOnly;

  public:
     VSIHdfsHandle(hdfsFile poFile,
                   hdfsFS poFilesystem,
                   const char * pszFilename,
                   bool bReadOnly);
    ~VSIHdfsHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    vsi_l_offset Length()
    {
      hdfsFileInfo * poInfo = hdfsGetPathInfo(this->poFilesystem, this->oFilename.c_str());
      if (poInfo != NULL) {
        tOffset nSize = poInfo->mSize;
        hdfsFreeFileInfo(poInfo, 1);
        return static_cast<vsi_l_offset>(nSize);
      }
      return -1;
    }
    int Eof() override;
    int Flush() override;
    int Close() override;
};

VSIHdfsHandle::VSIHdfsHandle(hdfsFile _poFile,
                             hdfsFS _poFilesystem,
                             const char * pszFilename,
                             bool _bReadOnly)
  : poFile(_poFile), poFilesystem(_poFilesystem), oFilename(pszFilename), bReadOnly(_bReadOnly)
{}

VSIHdfsHandle::~VSIHdfsHandle()
{
  this->Close();
}

int VSIHdfsHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
  switch(nWhence) {
  case SEEK_SET:
    return hdfsSeek(this->poFilesystem, this->poFile, nOffset);
  case SEEK_CUR:
    return hdfsSeek(this->poFilesystem,
                    this->poFile,
                    nOffset + this->Tell());
  case SEEK_END:
    return hdfsSeek(this->poFilesystem,
                    this->poFile,
                    static_cast<tOffset>(this->Length()) - nOffset);
  default:
    return -1;
  }
}

vsi_l_offset VSIHdfsHandle::Tell()
{
  return hdfsTell(this->poFilesystem, this->poFile);
}

size_t VSIHdfsHandle::Read(void *pBuffer, size_t nSize, size_t nMemb)
{
  return hdfsRead(this->poFilesystem, this->poFile, pBuffer, nSize * nMemb);
}

size_t VSIHdfsHandle::Write(const void *pBuffer, size_t nSize, size_t nMemb)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

int VSIHdfsHandle::Eof()
{
  return (this->Tell() == this->Length());
}

int VSIHdfsHandle::Flush()
{
  return hdfsFlush(this->poFilesystem, this->poFile);
}

int VSIHdfsHandle::Close()
{
  int retval = 0;

  if (this->poFilesystem != nullptr && this->poFile != nullptr)
    retval = hdfsCloseFile(this->poFilesystem, this->poFile);
  this->poFile = nullptr;
  this->poFilesystem = nullptr;

  return retval;
}

class VSIHdfsFilesystemHandler final : public VSIFilesystemHandler
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(VSIHdfsFilesystemHandler)

    hdfsFS poFilesystem;

  public:
    VSIHdfsFilesystemHandler();
    ~VSIHdfsFilesystemHandler() override;

    VSIVirtualHandle *Open(const char *pszFilename,
                           const char *pszAccess,
                           bool bSetError ) override;
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
{
  this->poFilesystem = hdfsConnect("default", 0);
}

VSIHdfsFilesystemHandler::~VSIHdfsFilesystemHandler()
{
  if (this->poFilesystem != nullptr)
    hdfsDisconnect(this->poFilesystem);
  this->poFilesystem = nullptr;
}

VSIVirtualHandle *
VSIHdfsFilesystemHandler::Open( const char *pszFilename,
                                const char *pszAccess,
                                bool)
{
  if (strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr) {
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return nullptr;
  }

  if (strncmp(pszFilename, VSIHDFS, strlen(VSIHDFS)) != 0) {
    return nullptr;
  }
  else {
    const char * pszPath = pszFilename + strlen(VSIHDFS);
    hdfsFile poFile = hdfsOpenFile(poFilesystem, pszPath, O_RDONLY, 0, 0, 0);
    if (poFile != NULL) {
      VSIHdfsHandle * poHandle = new VSIHdfsHandle(poFile, this->poFilesystem, pszPath, true);
      return poHandle;
    }
  }
  return nullptr;
}

int
VSIHdfsFilesystemHandler::Stat( const char *pszeFilename, VSIStatBufL *pStatBuf, int)
{
  memset(pStatBuf, 0, sizeof(*pStatBuf)); // XXX
  hdfsFileInfo * poInfo = hdfsGetPathInfo(this->poFilesystem, pszeFilename);

  if (poInfo != NULL) {
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
VSIHdfsFilesystemHandler::Unlink(const char *pszFilename)
{
  return hdfsDelete(this->poFilesystem, pszFilename, 0);
}

int
VSIHdfsFilesystemHandler::Mkdir(const char *pszDirname, long nMode)
{
  CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
  return -1;
}

int
VSIHdfsFilesystemHandler::Rmdir(const char *pszDirname)
{
  return hdfsDelete(this->poFilesystem, pszDirname, 1);
}

char **
VSIHdfsFilesystemHandler::ReadDir(const char *pszDirname)
{
  int mEntries = 0;
  char ** papszNames = nullptr;
  char ** retval = nullptr;

  hdfsFileInfo * paoInfo = hdfsListDirectory(this->poFilesystem, pszDirname, &mEntries);
  if (paoInfo != NULL) {
    papszNames = new char*[mEntries+1];
    for (int i = 0; i < mEntries; ++i) papszNames[i] = paoInfo[i].mName;
    papszNames[mEntries] = nullptr;
    retval = CSLDuplicate(papszNames);
    delete[] papszNames;
    return retval;
  }
  return nullptr;
}

int
VSIHdfsFilesystemHandler::Rename(const char *oldpath, const char *newpath)
{
  return hdfsRename(this->poFilesystem, oldpath, newpath);
}

//! @endcond

/************************************************************************/
/*                       VSIInstallHdfsHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsihdfs/ file system handler
 *
 * @since GDAL 2.4.0
 */
void VSIInstallHdfsHandler()
{
    VSIFileManager::InstallHandler(VSIHDFS, new VSIHdfsFilesystemHandler);
}

#else

void VSIInstallHdfsHandler( void )
{
    // Not supported.
}

#endif
