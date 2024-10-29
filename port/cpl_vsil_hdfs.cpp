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
 * SPDX-License-Identifier: MIT
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

#ifdef HDFS_ENABLED

#include "hdfs.h"

/************************************************************************/
/* ==================================================================== */
/*                        VSIHdfsHandle                               */
/* ==================================================================== */
/************************************************************************/

#define SILENCE(expr)                                                          \
    {                                                                          \
        int hOldStderr = dup(2);                                               \
        int hNewStderr = open("/dev/null", O_WRONLY);                          \
                                                                               \
        if ((hOldStderr != -1) && (hNewStderr != -1) &&                        \
            (dup2(hNewStderr, 2) != -1))                                       \
        {                                                                      \
            close(hNewStderr);                                                 \
            expr;                                                              \
            dup2(hOldStderr, 2);                                               \
            close(hOldStderr);                                                 \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            if (hOldStderr != -1)                                              \
                close(hOldStderr);                                             \
            if (hNewStderr != -1)                                              \
                close(hNewStderr);                                             \
            expr;                                                              \
        }                                                                      \
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
    static constexpr const char *VSIHDFS = "/vsihdfs/";

    VSIHdfsHandle(hdfsFile poFile, hdfsFS poFilesystem, const char *pszFilename,
                  bool bReadOnly);
    ~VSIHdfsHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    vsi_l_offset Length();
    void ClearErr() override;
    int Eof() override;
    int Error() override;
    int Flush() override;
    int Close() override;
};

VSIHdfsHandle::VSIHdfsHandle(hdfsFile _poFile, hdfsFS _poFilesystem,
                             const char *pszFilename, bool /*_bReadOnly*/)
    : poFile(_poFile), poFilesystem(_poFilesystem), oFilename(pszFilename)
{
}

VSIHdfsHandle::~VSIHdfsHandle()
{
    Close();
}

int VSIHdfsHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    bEOF = false;
    switch (nWhence)
    {
        case SEEK_SET:
            return hdfsSeek(poFilesystem, poFile, nOffset);
        case SEEK_CUR:
            return hdfsSeek(poFilesystem, poFile, nOffset + Tell());
        case SEEK_END:
            return hdfsSeek(poFilesystem, poFile,
                            static_cast<tOffset>(Length()) - nOffset);
        default:
            return -1;
    }
}

vsi_l_offset VSIHdfsHandle::Tell()
{
    return hdfsTell(poFilesystem, poFile);
}

size_t VSIHdfsHandle::Read(void *pBuffer, size_t nSize, size_t nMemb)
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
        bytes = hdfsRead(
            poFilesystem, poFile, static_cast<char *>(pBuffer) + bytes_read,
            bytes_to_request > INT_MAX ? INT_MAX : bytes_to_request);

        if (bytes > 0)
        {
            if (static_cast<size_t>(bytes) < bytes_to_request)
                bEOF = true;
            bytes_read += bytes;
        }
        if (bytes == 0)
        {
            bEOF = true;
            return bytes_read / nSize;
        }
        else if (bytes < 0)
        {
            bEOF = false;
            return 0;
        }
    }

    return bytes_read / nSize;
}

size_t VSIHdfsHandle::Write(const void *, size_t, size_t)
{
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return -1;
}

vsi_l_offset VSIHdfsHandle::Length()
{
    hdfsFileInfo *poInfo = hdfsGetPathInfo(poFilesystem, oFilename.c_str());
    if (poInfo != nullptr)
    {
        tOffset nSize = poInfo->mSize;
        hdfsFreeFileInfo(poInfo, 1);
        return static_cast<vsi_l_offset>(nSize);
    }
    return -1;
}

int VSIHdfsHandle::Eof()
{
    return bEOF;
}

int VSIHdfsHandle::Error()
{
    return 0;
}

void VSIHdfsHandle::ClearErr()
{
}

int VSIHdfsHandle::Flush()
{
    return hdfsFlush(poFilesystem, poFile);
}

int VSIHdfsHandle::Close()
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
    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
    int Rename(const char *oldpath, const char *newpath) override;
};

VSIHdfsFilesystemHandler::VSIHdfsFilesystemHandler()
{
}

VSIHdfsFilesystemHandler::~VSIHdfsFilesystemHandler()
{
    if (hMutex != nullptr)
    {
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;
    }

    if (poFilesystem != nullptr)
        hdfsDisconnect(poFilesystem);
    poFilesystem = nullptr;
}

void VSIHdfsFilesystemHandler::EnsureFilesystem()
{
    CPLMutexHolder oHolder(&hMutex);
    if (poFilesystem == nullptr)
        poFilesystem = hdfsConnect("default", 0);
}

VSIVirtualHandle *
VSIHdfsFilesystemHandler::Open(const char *pszFilename, const char *pszAccess,
                               bool, CSLConstList /* papszOptions */)
{
    EnsureFilesystem();

    if (strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
        return nullptr;
    }

    if (strncmp(pszFilename, VSIHdfsHandle::VSIHDFS,
                strlen(VSIHdfsHandle::VSIHDFS)) != 0)
    {
        return nullptr;
    }
    else
    {
        const char *pszPath = pszFilename + strlen(VSIHdfsHandle::VSIHDFS);

        // Open HDFS file, sending Java stack traces to /dev/null.
        hdfsFile poFile = nullptr;
        SILENCE(poFile =
                    hdfsOpenFile(poFilesystem, pszPath, O_RDONLY, 0, 0, 0));

        if (poFile != nullptr)
        {
            VSIHdfsHandle *poHandle =
                new VSIHdfsHandle(poFile, poFilesystem, pszPath, true);
            return poHandle;
        }
    }
    return nullptr;
}

int VSIHdfsFilesystemHandler::Stat(const char *pszFilename,
                                   VSIStatBufL *pStatBuf, int)
{
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if (strncmp(pszFilename, VSIHdfsHandle::VSIHDFS,
                strlen(VSIHdfsHandle::VSIHDFS)) != 0)
    {
        return -1;
    }

    EnsureFilesystem();

    // CPLDebug("VSIHDFS", "Stat(%s)", pszFilename);

    hdfsFileInfo *poInfo = hdfsGetPathInfo(
        poFilesystem, pszFilename + strlen(VSIHdfsHandle::VSIHDFS));

    if (poInfo != nullptr)
    {
        pStatBuf->st_dev =
            static_cast<dev_t>(0); /* ID of device containing file */
        pStatBuf->st_ino = static_cast<ino_t>(0); /* inode number */
        switch (poInfo->mKind)
        { /* protection */
            case tObjectKind::kObjectKindFile:
                pStatBuf->st_mode = S_IFREG;
                break;
            case tObjectKind::kObjectKindDirectory:
                pStatBuf->st_mode = S_IFDIR;
                break;
            default:
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unrecognized object kind");
        }
        pStatBuf->st_nlink = static_cast<nlink_t>(0); /* number of hard links */
        pStatBuf->st_uid = getuid();                  /* user ID of owner */
        pStatBuf->st_gid = getgid();                  /* group ID of owner */
        pStatBuf->st_rdev =
            static_cast<dev_t>(0); /* device ID (if special file) */
        pStatBuf->st_size =
            static_cast<off_t>(poInfo->mSize); /* total size, in bytes */
        pStatBuf->st_blksize = static_cast<blksize_t>(
            poInfo->mBlockSize); /* blocksize for filesystem I/O */
        pStatBuf->st_blocks =
            static_cast<blkcnt_t>((poInfo->mBlockSize >> 9) +
                                  1); /* number of 512B blocks allocated */
        pStatBuf->st_atime =
            static_cast<time_t>(poInfo->mLastAccess); /* time of last access */
        pStatBuf->st_mtime = static_cast<time_t>(
            poInfo->mLastMod); /* time of last modification */
        pStatBuf->st_ctime = static_cast<time_t>(
            poInfo->mLastMod); /* time of last status change */
        hdfsFreeFileInfo(poInfo, 1);
        return 0;
    }

    return -1;
}

int VSIHdfsFilesystemHandler::Unlink(const char *)
{
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return -1;
}

int VSIHdfsFilesystemHandler::Mkdir(const char *, long)
{
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return -1;
}

int VSIHdfsFilesystemHandler::Rmdir(const char *)
{
    CPLError(CE_Failure, CPLE_AppDefined, "HDFS driver is read-only");
    return -1;
}

char **VSIHdfsFilesystemHandler::ReadDirEx(const char *pszDirname,
                                           int /* nMaxFiles */)
{
    if (strncmp(pszDirname, VSIHdfsHandle::VSIHDFS,
                strlen(VSIHdfsHandle::VSIHDFS)) != 0)
    {
        return nullptr;
    }

    EnsureFilesystem();

    std::string osDirName(pszDirname);
    if (osDirName.back() != '/')
        osDirName += '/';

    VSIStatBufL sStat;
    if (Stat(osDirName.c_str(), &sStat, 0) != 0 || sStat.st_mode != S_IFDIR)
        return nullptr;

    int nEntries = 0;
    std::string osDirNameWithoutPrefix(
        osDirName.substr(strlen(VSIHdfsHandle::VSIHDFS)));

    // file:///home/user/... is accepted, but if this is used, files returned
    // by hdfsListDirectory() use file:/home/user/...
    if (osDirNameWithoutPrefix.compare(0, strlen("file:///"), "file:///") == 0)
    {
        osDirNameWithoutPrefix =
            "file:/" + osDirNameWithoutPrefix.substr(strlen("file:///"));
    }

    hdfsFileInfo *paoInfo = hdfsListDirectory(
        poFilesystem, osDirNameWithoutPrefix.c_str(), &nEntries);

    if (paoInfo != nullptr)
    {
        CPLStringList aosNames;
        for (int i = 0; i < nEntries; ++i)
        {
            // CPLDebug("VSIHDFS", "[%d]: %s", i, paoInfo[i].mName);
            if (STARTS_WITH(paoInfo[i].mName, osDirNameWithoutPrefix.c_str()))
            {
                aosNames.AddString(paoInfo[i].mName +
                                   osDirNameWithoutPrefix.size());
            }
            else
            {
                CPLDebug("VSIHDFS",
                         "hdfsListDirectory() returned %s, but this is not "
                         "starting with %s",
                         paoInfo[i].mName, osDirNameWithoutPrefix.c_str());
            }
        }
        hdfsFreeFileInfo(paoInfo, nEntries);
        return aosNames.StealList();
    }
    return nullptr;
}

int VSIHdfsFilesystemHandler::Rename(const char *, const char *)
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
    VSIFileManager::InstallHandler(VSIHdfsHandle::VSIHDFS,
                                   new VSIHdfsFilesystemHandler);
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
void VSIInstallHdfsHandler(void)
{
    // Not supported.
}

#endif
