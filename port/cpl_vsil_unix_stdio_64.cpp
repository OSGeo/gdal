/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Unix platforms
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2010-2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************
 *
 * NB: Note that in wrappers we are always saving the error state (errno
 * variable) to avoid side effects during debug prints or other possible
 * standard function calls (error states will be overwritten after such
 * a call).
 *
 ****************************************************************************/

//! @cond Doxygen_Suppress

#if !defined(_WIN32)

// #define VSI_COUNT_BYTES_READ
// #define VSI_DEBUG

// Some unusual filesystems do not work if _FORTIFY_SOURCE in GCC or
// clang is used within this source file, especially if techniques
// like those in vsipreload are used.  Fortify source interacts poorly with
// filesystems that use fread for forward seeks.  This leads to SIGSEGV within
// fread calls.
//
// See this for hardening background info: https://wiki.debian.org/Hardening
#undef _FORTIFY_SOURCE

// 64 bit IO is only available on 32-bit android since API 24 / Android 7.0
// See
// https://android.googlesource.com/platform/bionic/+/master/docs/32-bit-abi.md
#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
#define _FILE_OFFSET_BITS 64
#endif

#include "cpl_port.h"

#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_STATVFS
#include <sys/statvfs.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PREAD_BSD
#include <sys/uio.h>
#endif

#if defined(__MACH__) && defined(__APPLE__)
#define HAS_CASE_INSENSITIVE_FILE_SYSTEM
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#endif

#include <algorithm>
#include <array>
#include <limits>
#include <new>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_error.h"

#if defined(UNIX_STDIO_64)

#ifndef VSI_OPEN64
#define VSI_OPEN64 open64
#endif
#ifndef VSI_LSEEK64
#define VSI_LSEEK64 lseek64
#endif
#ifndef VSI_STAT64
#define VSI_STAT64 stat64
#endif
#ifndef VSI_STAT64_T
#define VSI_STAT64_T stat64
#endif
#ifndef VSI_FTRUNCATE64
#define VSI_FTRUNCATE64 ftruncate64
#endif

#else /* not UNIX_STDIO_64 */

#ifndef VSI_OPEN64
#define VSI_OPEN64 open
#endif
#ifndef VSI_LSEEK64
#define VSI_LSEEK64 lseek
#endif
#ifndef VSI_STAT64
#define VSI_STAT64 stat
#endif
#ifndef VSI_STAT64_T
#define VSI_STAT64_T stat
#endif
#ifndef VSI_FTRUNCATE64
#define VSI_FTRUNCATE64 ftruncate
#endif

#endif /* ndef UNIX_STDIO_64 */

#ifndef BUILD_WITHOUT_64BIT_OFFSET
// Ensure we have working 64 bit API
static_assert(sizeof(VSI_LSEEK64(0, 0, SEEK_CUR)) == sizeof(vsi_l_offset),
              "File API does not seem to support 64-bit offset. "
              "If you still want to build GDAL without > 4GB file support, "
              "add the -DBUILD_WITHOUT_64BIT_OFFSET define");
static_assert(sizeof(VSIStatBufL::st_size) == sizeof(vsi_l_offset),
              "File API does not seem to support 64-bit file size. "
              "If you still want to build GDAL without > 4GB file support, "
              "add the -DBUILD_WITHOUT_64BIT_OFFSET define");
#endif

/************************************************************************/
/* ==================================================================== */
/*                       VSIUnixStdioFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

struct VSIDIRUnixStdio;

class VSIUnixStdioFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIUnixStdioFilesystemHandler)

#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset nTotalBytesRead = 0;
    CPLMutex *hMutex = nullptr;
#endif

  public:
    VSIUnixStdioFilesystemHandler() = default;
#ifdef VSI_COUNT_BYTES_READ
    ~VSIUnixStdioFilesystemHandler() override;
#endif

    VSIVirtualHandleUniquePtr Open(const char *pszFilename,
                                   const char *pszAccess, bool bSetError,
                                   CSLConstList /* papszOptions */) override;

    VSIVirtualHandleUniquePtr
    CreateOnlyVisibleAtCloseTime(const char *pszFilename,
                                 bool bEmulationAllowed,
                                 CSLConstList papszOptions) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Rename(const char *oldpath, const char *newpath, GDALProgressFunc,
               void *) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
    GIntBig GetDiskFreeSpace(const char *pszDirname) override;
    int SupportsSparseFiles(const char *pszPath) override;

    bool IsLocal(const char *pszPath) const override;
    bool SupportsSequentialWrite(const char *pszPath,
                                 bool /* bAllowLocalTempFile */) override;
    bool SupportsRandomWrite(const char *pszPath,
                             bool /* bAllowLocalTempFile */) override;

    VSIDIR *OpenDir(const char *pszPath, int nRecurseDepth,
                    const char *const *papszOptions) override;

    static std::unique_ptr<VSIDIRUnixStdio>
    OpenDirInternal(const char *pszPath, int nRecurseDepth,
                    const char *const *papszOptions);

#ifdef HAS_CASE_INSENSITIVE_FILE_SYSTEM
    std::string
    GetCanonicalFilename(const std::string &osFilename) const override;
#endif

#ifdef VSI_COUNT_BYTES_READ
    void AddToTotal(vsi_l_offset nBytes);
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIUnixStdioHandle                            */
/* ==================================================================== */
/************************************************************************/

class VSIUnixStdioHandle final : public VSIVirtualHandle
{
  public:
    enum class AccessMode : unsigned char
    {
        NORMAL,
        READ_ONLY,
        WRITE_ONLY,
        // In a+ mode, disable any optimization since the behavior of the file
        // pointer on Mac and other BSD system is to have a seek() to the end of
        // file and thus a call to our Seek(0, SEEK_SET) before a read will be a
        // no-op.
        APPEND_READ_WRITE,
    };

  private:
    friend class VSIUnixStdioFilesystemHandler;
    CPL_DISALLOW_COPY_ASSIGN(VSIUnixStdioHandle)

    int fd = -1;
    vsi_l_offset m_nFilePos = 0;  // Logical/user position
    static constexpr size_t BUFFER_SIZE = 4096;
    std::array<GByte, BUFFER_SIZE> m_abyBuffer{};
    // For read operations, current position within m_abyBuffer
    // This implies that m_nFilePos - m_nBufferCurPos + m_nBufferSize is the
    //current real/kernel file position.
    size_t m_nBufferCurPos = 0;  // Current position within m_abyBuffer
    // Number of valid bytes in m_abyBuffer (both for read and write buffering)
    size_t m_nBufferSize = 0;
    bool m_bBufferDirty = false;
    const AccessMode eAccessMode;
    enum class Operation : unsigned char
    {
        NONE,
        READ,
        WRITE
    };
    Operation eLastOp = Operation::NONE;
    bool bAtEOF = false;
    bool bError = false;
#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset nTotalBytesRead = 0;
    VSIUnixStdioFilesystemHandler *poFS = nullptr;
#endif

    std::string m_osFilename{};
#if defined(__linux)
    bool m_bUnlinkedFile = false;
    bool m_bCancelCreation = false;
#else
    std::string m_osTmpFilename{};
#endif

  public:
    VSIUnixStdioHandle(VSIUnixStdioFilesystemHandler *poFSIn, int fdIn,
                       AccessMode eAccessModeIn);
    ~VSIUnixStdioHandle() override;

    int Seek(vsi_l_offset nOffsetIn, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nBytes) override;
    size_t Write(const void *pBuffer, size_t nBytes) override;
    void ClearErr() override;
    int Eof() override;
    int Error() override;
    int Flush() override;
    int Close() override;
    int Truncate(vsi_l_offset nNewSize) override;

    void *GetNativeFileDescriptor() override
    {
        return reinterpret_cast<void *>(static_cast<uintptr_t>(fd));
    }

    VSIRangeStatus GetRangeStatus(vsi_l_offset nOffset,
                                  vsi_l_offset nLength) override;
#if defined(HAVE_PREAD64) || (defined(HAVE_PREAD_BSD) && SIZEOF_OFF_T == 8)
    bool HasPRead() const override;
    size_t PRead(void * /*pBuffer*/, size_t /* nSize */,
                 vsi_l_offset /*nOffset*/) const override;
#endif

    void CancelCreation() override;
};

/************************************************************************/
/*                         VSIUnixStdioHandle()                         */
/************************************************************************/

VSIUnixStdioHandle::VSIUnixStdioHandle(
#ifndef VSI_COUNT_BYTES_READ
    CPL_UNUSED
#endif
    VSIUnixStdioFilesystemHandler *poFSIn,
    int fdIn, AccessMode eAccessModeIn)
    : fd(fdIn), eAccessMode(eAccessModeIn)
#ifdef VSI_COUNT_BYTES_READ
      ,
      poFS(poFSIn)
#endif
{
}

/************************************************************************/
/*                        ~VSIUnixStdioHandle()                         */
/************************************************************************/

VSIUnixStdioHandle::~VSIUnixStdioHandle()
{
    VSIUnixStdioHandle::Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIUnixStdioHandle::Close()

{
    if (fd < 0)
        return 0;

    VSIDebug1("VSIUnixStdioHandle::Close(%d)", fd);

#ifdef VSI_COUNT_BYTES_READ
    poFS->AddToTotal(nTotalBytesRead);
#endif

    int ret = VSIUnixStdioHandle::Flush();

#ifdef __linux
    if (ret == 0 && !m_bCancelCreation && !m_osFilename.empty() &&
        m_bUnlinkedFile)
    {
        ret = fsync(fd);
        if (ret == 0)
        {
            // As advised by "man 2 open" if the caller doesn't have the
            // CAP_DAC_READ_SEARCH capability, which seems to be the default

            char szPath[32];
            snprintf(szPath, sizeof(szPath), "/proc/self/fd/%d", fd);
            ret = linkat(AT_FDCWD, szPath, AT_FDCWD, m_osFilename.c_str(),
                         AT_SYMLINK_FOLLOW);
            if (ret != 0)
                CPLDebug("CPL", "linkat() failed with errno=%d", errno);
        }
    }
#endif

    int ret2 = close(fd);
    if (ret == 0 && ret2 != 0)
        ret = ret2;

#if !defined(__linux)
    if (!m_osTmpFilename.empty() && !m_osFilename.empty())
    {
        ret = rename(m_osTmpFilename.c_str(), m_osFilename.c_str());
    }
#endif

    fd = -1;
    return ret;
}

/************************************************************************/
/*                           CancelCreation()                           */
/************************************************************************/

void VSIUnixStdioHandle::CancelCreation()
{
#if defined(__linux)
    if (!m_osFilename.empty() && !m_bUnlinkedFile)
    {
        unlink(m_osFilename.c_str());
        m_osFilename.clear();
    }
    else if (m_bUnlinkedFile)
        m_bCancelCreation = true;
#else
    if (!m_osTmpFilename.empty())
    {
        unlink(m_osTmpFilename.c_str());
        m_osTmpFilename.clear();
    }
    else if (!m_osFilename.empty())
    {
        unlink(m_osFilename.c_str());
        m_osFilename.clear();
    }
#endif
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIUnixStdioHandle::Seek(vsi_l_offset nOffsetIn, int nWhence)
{
    bAtEOF = false;
    bError = false;

    // Seeks that do nothing are still surprisingly expensive with MSVCRT.
    // try and short circuit if possible.
    if (eAccessMode != AccessMode::APPEND_READ_WRITE && nWhence == SEEK_SET &&
        nOffsetIn == m_nFilePos)
        return 0;

#if !defined(UNIX_STDIO_64) && SIZEOF_UNSIGNED_LONG == 4
    if (nOffsetIn > static_cast<vsi_l_offset>(std::numeric_limits<long>::max()))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Attempt at seeking beyond long extent. Lack of 64-bit file I/O");
        return -1;
    }
#endif

    if (eLastOp == Operation::WRITE)
    {
        eLastOp = Operation::NONE;

        if (m_bBufferDirty && Flush() < 0)
            return -1;

        const auto nNewPos = VSI_LSEEK64(fd, nOffsetIn, nWhence);
        if (nNewPos < 0)
        {
            bError = true;
            return -1;
        }

        m_nFilePos = nNewPos;
        m_nBufferCurPos = 0;
        m_nBufferSize = 0;
    }

    else
    {
        eLastOp = Operation::READ;

        vsi_l_offset nTargetPos;

        // Compute absolute target position
        switch (nWhence)
        {
            case SEEK_SET:
                nTargetPos = nOffsetIn;
                break;
            case SEEK_CUR:
                nTargetPos = m_nFilePos + nOffsetIn;
                break;
            case SEEK_END:
            {
                const auto nNewPos = VSI_LSEEK64(fd, nOffsetIn, SEEK_END);
                if (nNewPos < 0)
                {
                    bError = true;
                    return -1;
                }
                nTargetPos = nNewPos;
                break;
            }
            default:
                return -1;
        }

        // Compute absolute position of the current buffer
        const auto nBufStartOffset = m_nFilePos - m_nBufferCurPos;
        const auto nBufEndOffset = nBufStartOffset + m_nBufferSize;

        if (nTargetPos >= nBufStartOffset && nTargetPos < nBufEndOffset)
        {
            // Seek inside current buffer - adjust buffer pos only
            m_nBufferCurPos = static_cast<size_t>(nTargetPos - nBufStartOffset);
            m_nFilePos = nTargetPos;
        }
        else
        {
            // Outside buffer, do real seek and invalidate buffer
            const auto nNewPos = VSI_LSEEK64(fd, nTargetPos, SEEK_SET);
            if (nNewPos < 0)
            {
                bError = true;
                return -1;
            }

            m_nFilePos = nNewPos;
            m_nBufferCurPos = 0;
            m_nBufferSize = 0;
        }
    }

    VSIDebug4("VSIUnixStdioHandle::Seek(%d," CPL_FRMT_GUIB ", %d) = %" PRIu64,
              fd, nOffsetIn, nWhence, static_cast<uint64_t>(m_nFilePos));

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIUnixStdioHandle::Tell()

{
    return m_nFilePos;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIUnixStdioHandle::Flush()

{
    VSIDebug1("VSIUnixStdioHandle::Flush(%d)", fd);

    if (m_bBufferDirty)
    {
        m_bBufferDirty = false;

        // Sync kernel offset to our logical position before writing
        if (VSI_LSEEK64(fd, m_nFilePos - m_nBufferSize, SEEK_SET) < 0)
        {
            return EOF;
        }

        size_t nOff = 0;
        while (m_nBufferSize > 0)
        {
            errno = 0;
            const auto nWritten =
                write(fd, m_abyBuffer.data() + nOff, m_nBufferSize);
            if (nWritten < 0)
            {
                if (errno == EINTR)
                    continue;
                return EOF;
            }
            CPLAssert(static_cast<size_t>(nWritten) <= m_nBufferSize);
            nOff += nWritten;
            m_nBufferSize -= nWritten;
        }
    }

    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIUnixStdioHandle::Read(void *pBuffer, size_t nBytes)

{
    if (nBytes == 0 || bAtEOF)
        return 0;
    if (eAccessMode == AccessMode::WRITE_ONLY)
    {
        bError = true;
        errno = EINVAL;
        return 0;
    }

    /* -------------------------------------------------------------------- */
    /*      If a fwrite() is followed by an fread(), the POSIX rules are    */
    /*      that some of the write may still be buffered and lost.  We      */
    /*      are required to do a seek between to force flushing.   So we    */
    /*      keep careful track of what happened last to know if we          */
    /*      skipped a flushing seek that we may need to do now.             */
    /* -------------------------------------------------------------------- */
    if (eLastOp == Operation::WRITE)
    {
        if (m_bBufferDirty && Flush() < 0)
        {
            bError = true;
            return -1;
        }
        if (VSI_LSEEK64(fd, m_nFilePos, SEEK_SET) < 0)
        {
            bError = true;
            return -1;
        }
        m_nBufferCurPos = 0;
        m_nBufferSize = 0;
    }

    const size_t nTotal = nBytes;
    size_t nBytesRead = 0;
    GByte *const pabyDest = static_cast<GByte *>(pBuffer);

    while (nBytesRead < nTotal)
    {
        size_t nAvailInBuffer = m_nBufferSize - m_nBufferCurPos;

        // If buffer is empty
        if (nAvailInBuffer == 0)
        {
            size_t nRemaining = nTotal - nBytesRead;
            if (nRemaining >= BUFFER_SIZE)
            {
                // Bypass buffer if large request
                errno = 0;
                const ssize_t nBytesReadThisTime =
                    read(fd, pabyDest + nBytesRead, nRemaining);
                if (nBytesReadThisTime <= 0)
                {
                    if (nBytesReadThisTime == 0)
                        bAtEOF = true;
                    else if (errno == EINTR)
                        continue;
                    else
                        bError = true;
                    break;
                }
                m_nFilePos += nBytesReadThisTime;
                nBytesRead += nBytesReadThisTime;
            }
            else
            {
                // Small request: Refill internal buffer
                errno = 0;
                const ssize_t nBytesReadThisTime =
                    read(fd, m_abyBuffer.data(), BUFFER_SIZE);
                if (nBytesReadThisTime <= 0)
                {
                    if (nBytesReadThisTime == 0)
                        bAtEOF = true;
                    else if (errno == EINTR)
                        continue;
                    else
                        bError = true;
                    break;
                }
                m_nBufferCurPos = 0;
                m_nBufferSize = nBytesReadThisTime;
                nAvailInBuffer = nBytesReadThisTime;
            }
        }

        // Copy from buffer to user
        const size_t nToCopy = std::min(nTotal - nBytesRead, nAvailInBuffer);
        memcpy(pabyDest + nBytesRead, m_abyBuffer.data() + m_nBufferCurPos,
               nToCopy);
        nBytesRead += nToCopy;
        m_nBufferCurPos += nToCopy;
        m_nFilePos += nToCopy;
    }

#ifdef VSI_DEBUG
    const int nError = errno;
    VSIDebug4("VSIUnixStdioHandle::Read(%d,%%ld) = %" PRId64, fd,
              static_cast<long>(nBytes), static_cast<int64_t>(nBytesRead));
    errno = nError;
#endif

    eLastOp = Operation::READ;

#ifdef VSI_COUNT_BYTES_READ
    nTotalBytesRead += nBytesRead;
#endif

    return nBytesRead;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIUnixStdioHandle::Write(const void *pBuffer, size_t nBytes)

{
    if (nBytes == 0)
        return 0;
    if (eAccessMode == AccessMode::READ_ONLY)
    {
        bError = true;
        errno = EINVAL;
        return 0;
    }

    /* -------------------------------------------------------------------- */
    /*      If a fwrite() is followed by an fread(), the POSIX rules are    */
    /*      that some of the write may still be buffered and lost.  We      */
    /*      are required to do a seek between to force flushing.   So we    */
    /*      keep careful track of what happened last to know if we          */
    /*      skipped a flushing seek that we may need to do now.             */
    /* -------------------------------------------------------------------- */
    if (eLastOp == Operation::READ)
    {
        if (VSI_LSEEK64(fd, m_nFilePos, SEEK_SET) < 0)
        {
            bError = true;
            return -1;
        }
        m_nBufferCurPos = 0;
        m_nBufferSize = 0;
    }

    const size_t nTotal = nBytes;
    size_t nBytesWritten = 0;
    const GByte *const pabySrc = static_cast<const GByte *>(pBuffer);
    while (nBytesWritten < nTotal)
    {
        const size_t nRemaining = nTotal - nBytesWritten;

        // Bypass buffer if request > BUFFER_SIZE
        if (m_nBufferSize == 0 && nRemaining >= BUFFER_SIZE)
        {
            const auto nWritten =
                write(fd, pabySrc + nBytesWritten, nRemaining);
            if (nWritten < 0)
            {
                bError = true;
                break;
            }
            CPLAssert(static_cast<size_t>(nWritten) <= nRemaining);
            m_nFilePos += nWritten;
            nBytesWritten += nWritten;
            continue;
        }

        // Small request: Fill internal buffer
        const size_t nFreeSpaceInBuffer = BUFFER_SIZE - m_nBufferSize;
        const size_t nToCopy = std::min(nRemaining, nFreeSpaceInBuffer);
        memcpy(m_abyBuffer.data() + m_nBufferSize, pabySrc + nBytesWritten,
               nToCopy);

        nBytesWritten += nToCopy;
        m_nBufferSize += nToCopy;
        m_nFilePos += nToCopy;
        m_bBufferDirty = true;

        if (m_nBufferSize == BUFFER_SIZE && Flush() < 0)
        {
            break;
        }
    }

#ifdef VSI_DEBUG
    const int nError = errno;
    VSIDebug4("VSIUnixStdioHandle::Write(%d,%ld) = %" PRId64, fd,
              static_cast<long>(nBytes), static_cast<int64_t>(nBytesWritten));
    errno = nError;
#endif

    eLastOp = Operation::WRITE;

    return nBytesWritten;
}

/************************************************************************/
/*                              ClearErr()                              */
/************************************************************************/

void VSIUnixStdioHandle::ClearErr()

{
    bAtEOF = false;
    bError = false;
}

/************************************************************************/
/*                               Error()                                */
/************************************************************************/

int VSIUnixStdioHandle::Error()

{
    return bError ? TRUE : FALSE;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIUnixStdioHandle::Eof()

{
    return bAtEOF ? TRUE : FALSE;
}

/************************************************************************/
/*                              Truncate()                              */
/************************************************************************/

int VSIUnixStdioHandle::Truncate(vsi_l_offset nNewSize)
{
    if (Flush() != 0)
        return -1;
    return VSI_FTRUNCATE64(fd, nNewSize);
}

/************************************************************************/
/*                           GetRangeStatus()                           */
/************************************************************************/

#ifdef __linux
#if !defined(MISSING_LINUX_FS_H)
#include <linux/fs.h>  // FS_IOC_FIEMAP
#endif
#ifdef FS_IOC_FIEMAP
#include <linux/types.h>   // for types used in linux/fiemap.h
#include <linux/fiemap.h>  // struct fiemap
#endif
#include <sys/ioctl.h>
#include <errno.h>
#endif

VSIRangeStatus VSIUnixStdioHandle::GetRangeStatus(vsi_l_offset
#ifdef FS_IOC_FIEMAP
                                                      nOffset
#endif
                                                  ,
                                                  vsi_l_offset
#ifdef FS_IOC_FIEMAP
                                                      nLength
#endif
)
{
#ifdef FS_IOC_FIEMAP
    // fiemap IOCTL documented at
    // https://www.kernel.org/doc/Documentation/filesystems/fiemap.txt

    // The fiemap struct contains a "variable length" array at its end
    // As we are interested in only one extent, we allocate the base size of
    // fiemap + one fiemap_extent.
    GByte abyBuffer[sizeof(struct fiemap) + sizeof(struct fiemap_extent)];
    struct fiemap *psExtentMap = reinterpret_cast<struct fiemap *>(&abyBuffer);
    memset(psExtentMap, 0,
           sizeof(struct fiemap) + sizeof(struct fiemap_extent));
    psExtentMap->fm_start = nOffset;
    psExtentMap->fm_length = nLength;
    psExtentMap->fm_extent_count = 1;
    int ret = ioctl(fd, FS_IOC_FIEMAP, psExtentMap);
    if (ret < 0)
        return VSI_RANGE_STATUS_UNKNOWN;
    if (psExtentMap->fm_mapped_extents == 0)
        return VSI_RANGE_STATUS_HOLE;
    // In case there is one extent with unknown status, retry after having
    // asked the kernel to sync the file.
    const fiemap_extent *pasExtent = &(psExtentMap->fm_extents[0]);
    if (psExtentMap->fm_mapped_extents == 1 &&
        (pasExtent[0].fe_flags & FIEMAP_EXTENT_UNKNOWN) != 0)
    {
        psExtentMap->fm_flags = FIEMAP_FLAG_SYNC;
        psExtentMap->fm_start = nOffset;
        psExtentMap->fm_length = nLength;
        psExtentMap->fm_extent_count = 1;
        ret = ioctl(fd, FS_IOC_FIEMAP, psExtentMap);
        if (ret < 0)
            return VSI_RANGE_STATUS_UNKNOWN;
        if (psExtentMap->fm_mapped_extents == 0)
            return VSI_RANGE_STATUS_HOLE;
    }
    return VSI_RANGE_STATUS_DATA;
#else
    static bool bMessageEmitted = false;
    if (!bMessageEmitted)
    {
        CPLDebug("VSI", "Sorry: GetExtentStatus() not implemented for "
                        "this operating system");
        bMessageEmitted = true;
    }
    return VSI_RANGE_STATUS_UNKNOWN;
#endif
}

/************************************************************************/
/*                              HasPRead()                              */
/************************************************************************/

#if defined(HAVE_PREAD64) || (defined(HAVE_PREAD_BSD) && SIZEOF_OFF_T == 8)
bool VSIUnixStdioHandle::HasPRead() const
{
    return true;
}

/************************************************************************/
/*                               PRead()                                */
/************************************************************************/

size_t VSIUnixStdioHandle::PRead(void *pBuffer, size_t nSize,
                                 vsi_l_offset nOffset) const
{
#ifdef HAVE_PREAD64
    return pread64(fd, pBuffer, nSize, nOffset);
#else
    return pread(fd, pBuffer, nSize, static_cast<off_t>(nOffset));
#endif
}
#endif

/************************************************************************/
/* ==================================================================== */
/*                       VSIUnixStdioFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                   ~VSIUnixStdioFilesystemHandler()                   */
/************************************************************************/

VSIUnixStdioFilesystemHandler::~VSIUnixStdioFilesystemHandler()
{
    CPLDebug(
        "VSI",
        "~VSIUnixStdioFilesystemHandler() : nTotalBytesRead = " CPL_FRMT_GUIB,
        nTotalBytesRead);

    if (hMutex != nullptr)
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIUnixStdioFilesystemHandler::Open(const char *pszFilename,
                                    const char *pszAccess, bool bSetError,
                                    CSLConstList /* papszOptions */)

{
    int flags = O_RDONLY;
    int fd;
    using AccessMode = VSIUnixStdioHandle::AccessMode;
    AccessMode eAccessMode = AccessMode::NORMAL;
    if (strchr(pszAccess, 'w'))
    {
        if (strchr(pszAccess, '+'))
            flags = O_CREAT | O_TRUNC | O_RDWR;
        else
        {
            eAccessMode = AccessMode::WRITE_ONLY;
            flags = O_CREAT | O_TRUNC | O_WRONLY;
        }
        fd = VSI_OPEN64(pszFilename, flags, 0664);
    }
    else if (strchr(pszAccess, 'a'))
    {
        if (strchr(pszAccess, '+'))
        {
            eAccessMode = AccessMode::APPEND_READ_WRITE;
            flags = O_CREAT | O_RDWR;
        }
        else
            flags = O_CREAT | O_WRONLY;
        fd = VSI_OPEN64(pszFilename, flags, 0664);
    }
    else
    {
        if (strchr(pszAccess, '+'))
            flags = O_RDWR;
        else
            eAccessMode = AccessMode::READ_ONLY;
        fd = VSI_OPEN64(pszFilename, flags);
    }
    const int nError = errno;

    VSIDebug3("VSIUnixStdioFilesystemHandler::Open(\"%s\",\"%s\") = %d",
              pszFilename, pszAccess, fd);

    if (fd < 0)
    {
        if (bSetError)
        {
            VSIError(VSIE_FileError, "%s: %s", pszFilename, strerror(nError));
        }
        errno = nError;
        return nullptr;
    }

    auto poHandle = std::make_unique<VSIUnixStdioHandle>(this, fd, eAccessMode);
    poHandle->m_osFilename = pszFilename;

    errno = nError;

    if (strchr(pszAccess, 'a'))
        poHandle->Seek(0, SEEK_END);

    /* -------------------------------------------------------------------- */
    /*      If VSI_CACHE is set we want to use a cached reader instead      */
    /*      of more direct io on the underlying file.                       */
    /* -------------------------------------------------------------------- */
    if (eAccessMode == AccessMode::READ_ONLY &&
        CPLTestBool(CPLGetConfigOption("VSI_CACHE", "FALSE")))
    {
        return VSIVirtualHandleUniquePtr(
            VSICreateCachedFile(poHandle.release()));
    }

    return VSIVirtualHandleUniquePtr(poHandle.release());
}

/************************************************************************/
/*                    CreateOnlyVisibleAtCloseTime()                    */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIUnixStdioFilesystemHandler::CreateOnlyVisibleAtCloseTime(
    const char *pszFilename, bool bEmulationAllowed, CSLConstList papszOptions)
{
#ifdef __linux
    static bool bIsLinkatSupported = []()
    {
        // Check that /proc is accessible, since we will need it to run linkat()
        struct stat statbuf;
        return stat("/proc/self/fd", &statbuf) == 0;
    }();

    const int fd = bIsLinkatSupported
                       ? VSI_OPEN64(CPLGetDirnameSafe(pszFilename).c_str(),
                                    O_TMPFILE | O_RDWR, 0666)
                       : -1;
    if (fd < 0)
    {
        if (bIsLinkatSupported)
        {
            CPLDebugOnce("VSI",
                         "open(O_TMPFILE | O_RDWR) failed with errno=%d (%s). "
                         "Going through emulation",
                         errno, VSIStrerror(errno));
        }
        return VSIFilesystemHandler::CreateOnlyVisibleAtCloseTime(
            pszFilename, bEmulationAllowed, papszOptions);
    }

    auto poHandle = std::make_unique<VSIUnixStdioHandle>(
        this, fd, VSIUnixStdioHandle::AccessMode::NORMAL);
    poHandle->m_osFilename = pszFilename;
    poHandle->m_bUnlinkedFile = true;
    return VSIVirtualHandleUniquePtr(poHandle.release());
#else
    if (!bEmulationAllowed)
        return nullptr;

    std::string osTmpFilename = std::string(pszFilename).append("XXXXXX");
    int fd = mkstemp(osTmpFilename.data());
    if (fd < 0)
    {
        return VSIFilesystemHandler::CreateOnlyVisibleAtCloseTime(
            pszFilename, bEmulationAllowed, papszOptions);
    }
    auto poHandle = std::make_unique<VSIUnixStdioHandle>(
        this, fd, VSIUnixStdioHandle::AccessMode::NORMAL);
    poHandle->m_osTmpFilename = std::move(osTmpFilename);
    poHandle->m_osFilename = pszFilename;
    return VSIVirtualHandleUniquePtr(poHandle.release());
#endif
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Stat(const char *pszFilename,
                                        VSIStatBufL *pStatBuf, int /* nFlags */)
{
    return (VSI_STAT64(pszFilename, pStatBuf));
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Unlink(const char *pszFilename)

{
    return unlink(pszFilename);
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Rename(const char *oldpath,
                                          const char *newpath, GDALProgressFunc,
                                          void *)

{
    return rename(oldpath, newpath);
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Mkdir(const char *pszPathname, long nMode)

{
    return mkdir(pszPathname, static_cast<int>(nMode));
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Rmdir(const char *pszPathname)

{
    return rmdir(pszPathname);
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char **VSIUnixStdioFilesystemHandler::ReadDirEx(const char *pszPath,
                                                int nMaxFiles)

{
    if (strlen(pszPath) == 0)
        pszPath = ".";

    CPLStringList oDir;
    DIR *hDir = opendir(pszPath);
    if (hDir != nullptr)
    {
        // We want to avoid returning NULL for an empty list.
        oDir.Assign(static_cast<char **>(CPLCalloc(2, sizeof(char *))));

        struct dirent *psDirEntry = nullptr;
        while ((psDirEntry = readdir(hDir)) != nullptr)
        {
            oDir.AddString(psDirEntry->d_name);
            if (nMaxFiles > 0 && oDir.Count() > nMaxFiles)
                break;
        }

        closedir(hDir);
    }
    else
    {
        // Should we generate an error?
        // For now we'll just return NULL (at the end of the function).
    }

    return oDir.StealList();
}

/************************************************************************/
/*                          GetDiskFreeSpace()                          */
/************************************************************************/

GIntBig VSIUnixStdioFilesystemHandler::GetDiskFreeSpace(const char *
#ifdef HAVE_STATVFS
                                                            pszDirname
#endif
)
{
    GIntBig nRet = -1;
#ifdef HAVE_STATVFS

#ifdef HAVE_STATVFS64
    struct statvfs64 buf;
    if (statvfs64(pszDirname, &buf) == 0)
    {
        nRet = static_cast<GIntBig>(buf.f_frsize *
                                    static_cast<GUIntBig>(buf.f_bavail));
    }
#else
    struct statvfs buf;
    if (statvfs(pszDirname, &buf) == 0)
    {
        nRet = static_cast<GIntBig>(buf.f_frsize *
                                    static_cast<GUIntBig>(buf.f_bavail));
    }
#endif

#endif
    return nRet;
}

/************************************************************************/
/*                        SupportsSparseFiles()                         */
/************************************************************************/

#ifdef __linux
#include <sys/vfs.h>
#endif

int VSIUnixStdioFilesystemHandler::SupportsSparseFiles(const char *
#ifdef __linux
                                                           pszPath
#endif
)
{
#ifdef __linux
    struct statfs sStatFS;
    if (statfs(pszPath, &sStatFS) == 0)
    {
        // Add here any missing filesystem supporting sparse files.
        // See http://en.wikipedia.org/wiki/Comparison_of_file_systems
        switch (static_cast<unsigned>(sStatFS.f_type))
        {
            // Codes from http://man7.org/linux/man-pages/man2/statfs.2.html
            case 0xef53U:      // ext2, 3, 4
            case 0x52654973U:  // reiser
            case 0x58465342U:  // xfs
            case 0x3153464aU:  // jfs
            case 0x5346544eU:  // ntfs
            case 0x9123683eU:  // brfs
            // nfs: NFS < 4.2 supports creating sparse files (but reading them
            // not efficiently).
            case 0x6969U:
            case 0x01021994U:  // tmpfs
                return TRUE;

            case 0x4d44U:  // msdos
                return FALSE;

            case 0x53464846U:  // Windows Subsystem for Linux fs
            {
                static bool bUnknownFSEmitted = false;
                if (!bUnknownFSEmitted)
                {
                    CPLDebug("VSI",
                             "Windows Subsystem for Linux FS is at "
                             "the time of writing not known to support sparse "
                             "files");
                    bUnknownFSEmitted = true;
                }
                return FALSE;
            }

            default:
            {
                static bool bUnknownFSEmitted = false;
                if (!bUnknownFSEmitted)
                {
                    CPLDebug("VSI",
                             "Filesystem with type %X unknown. "
                             "Assuming it does not support sparse files",
                             static_cast<int>(sStatFS.f_type));
                    bUnknownFSEmitted = true;
                }
                return FALSE;
            }
        }
    }
    return FALSE;
#else
    static bool bMessageEmitted = false;
    if (!bMessageEmitted)
    {
        CPLDebug("VSI", "Sorry: SupportsSparseFiles() not implemented "
                        "for this operating system");
        bMessageEmitted = true;
    }
    return FALSE;
#endif
}

/************************************************************************/
/*                              IsLocal()                               */
/************************************************************************/

bool VSIUnixStdioFilesystemHandler::IsLocal(const char *
#ifdef __linux
                                                pszPath
#endif
) const
{
#ifdef __linux
    struct statfs sStatFS;
    if (statfs(pszPath, &sStatFS) == 0)
    {
        // See http://en.wikipedia.org/wiki/Comparison_of_file_systems
        switch (static_cast<unsigned>(sStatFS.f_type))
        {
            // Codes from http://man7.org/linux/man-pages/man2/statfs.2.html
            case 0x6969U:      // NFS
            case 0x517bU:      // SMB
            case 0xff534d42U:  // CIFS
            case 0xfe534d42U:  // SMB2
                // (https://github.com/libuv/libuv/blob/97dcdb1926f6aca43171e1614338bcef067abd59/src/unix/fs.c#L960)
                return false;
        }
    }
#else
    static bool bMessageEmitted = false;
    if (!bMessageEmitted)
    {
        CPLDebug("VSI", "Sorry: IsLocal() not implemented "
                        "for this operating system");
        bMessageEmitted = true;
    }
#endif
    return true;
}

/************************************************************************/
/*                      SupportsSequentialWrite()                       */
/************************************************************************/

bool VSIUnixStdioFilesystemHandler::SupportsSequentialWrite(
    const char *pszPath, bool /* bAllowLocalTempFile */)
{
    VSIStatBufL sStat;
    if (VSIStatL(pszPath, &sStat) == 0)
        return access(pszPath, W_OK) == 0;
    return access(CPLGetDirnameSafe(pszPath).c_str(), W_OK) == 0;
}

/************************************************************************/
/*                        SupportsRandomWrite()                         */
/************************************************************************/

bool VSIUnixStdioFilesystemHandler::SupportsRandomWrite(
    const char *pszPath, bool /* bAllowLocalTempFile */)
{
    return SupportsSequentialWrite(pszPath, false);
}

/************************************************************************/
/*                           VSIDIRUnixStdio                            */
/************************************************************************/

struct VSIDIRUnixStdio final : public VSIDIR
{
    struct DIRCloser
    {
        void operator()(DIR *d)
        {
            if (d)
                closedir(d);
        }
    };

    CPLString osRootPath{};
    CPLString osBasePath{};
    std::unique_ptr<DIR, DIRCloser> m_psDir{};
    int nRecurseDepth = 0;
    VSIDIREntry entry{};
    std::vector<std::unique_ptr<VSIDIR>> aoStackSubDir{};
    std::string m_osFilterPrefix{};
    bool m_bNameAndTypeOnly = false;

    const VSIDIREntry *NextDirEntry() override;
};

/************************************************************************/
/*                          OpenDirInternal()                           */
/************************************************************************/

/* static */
std::unique_ptr<VSIDIRUnixStdio> VSIUnixStdioFilesystemHandler::OpenDirInternal(
    const char *pszPath, int nRecurseDepth, const char *const *papszOptions)
{
    std::unique_ptr<DIR, VSIDIRUnixStdio::DIRCloser> psDir(opendir(pszPath));
    if (psDir == nullptr)
    {
        return nullptr;
    }
    auto dir = std::make_unique<VSIDIRUnixStdio>();
    dir->osRootPath = pszPath;
    dir->nRecurseDepth = nRecurseDepth;
    dir->m_psDir = std::move(psDir);
    dir->m_osFilterPrefix = CSLFetchNameValueDef(papszOptions, "PREFIX", "");
    dir->m_bNameAndTypeOnly = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "NAME_AND_TYPE_ONLY", "NO"));
    return dir;
}

/************************************************************************/
/*                              OpenDir()                               */
/************************************************************************/

VSIDIR *VSIUnixStdioFilesystemHandler::OpenDir(const char *pszPath,
                                               int nRecurseDepth,
                                               const char *const *papszOptions)
{
    return OpenDirInternal(pszPath, nRecurseDepth, papszOptions).release();
}

/************************************************************************/
/*                            NextDirEntry()                            */
/************************************************************************/

const VSIDIREntry *VSIDIRUnixStdio::NextDirEntry()
{
begin:
    if (VSI_ISDIR(entry.nMode) && nRecurseDepth != 0)
    {
        CPLString osCurFile(osRootPath);
        if (!osCurFile.empty())
            osCurFile += '/';
        osCurFile += entry.pszName;
        auto subdir = VSIUnixStdioFilesystemHandler::OpenDirInternal(
            osCurFile, nRecurseDepth - 1, nullptr);
        if (subdir)
        {
            subdir->osRootPath = osRootPath;
            subdir->osBasePath = entry.pszName;
            subdir->m_osFilterPrefix = m_osFilterPrefix;
            subdir->m_bNameAndTypeOnly = m_bNameAndTypeOnly;
            aoStackSubDir.push_back(std::move(subdir));
        }
        entry.nMode = 0;
    }

    while (!aoStackSubDir.empty())
    {
        auto l_entry = aoStackSubDir.back()->NextDirEntry();
        if (l_entry)
        {
            return l_entry;
        }
        aoStackSubDir.pop_back();
    }

    while (const auto *psEntry = readdir(m_psDir.get()))
    {
        // Skip . and ..entries
        if (psEntry->d_name[0] == '.' &&
            (psEntry->d_name[1] == '\0' ||
             (psEntry->d_name[1] == '.' && psEntry->d_name[2] == '\0')))
        {
            // do nothing
        }
        else
        {
            CPLFree(entry.pszName);
            CPLString osName(osBasePath);
            if (!osName.empty())
                osName += '/';
            osName += psEntry->d_name;

            entry.pszName = CPLStrdup(osName);
            entry.nMode = 0;
            entry.nSize = 0;
            entry.nMTime = 0;
            entry.bModeKnown = false;
            entry.bSizeKnown = false;
            entry.bMTimeKnown = false;

            CPLString osCurFile(osRootPath);
            if (!osCurFile.empty())
                osCurFile += '/';
            osCurFile += entry.pszName;

#if !defined(__sun) && !defined(__HAIKU__)
            if (psEntry->d_type == DT_REG)
                entry.nMode = S_IFREG;
            else if (psEntry->d_type == DT_DIR)
                entry.nMode = S_IFDIR;
            else if (psEntry->d_type == DT_LNK)
                entry.nMode = S_IFLNK;
#endif

            const auto StatFile = [&osCurFile, this]()
            {
                VSIStatBufL sStatL;
                if (VSIStatL(osCurFile, &sStatL) == 0)
                {
                    entry.nMode = sStatL.st_mode;
                    entry.nSize = sStatL.st_size;
                    entry.nMTime = sStatL.st_mtime;
                    entry.bModeKnown = true;
                    entry.bSizeKnown = true;
                    entry.bMTimeKnown = true;
                }
            };

            if (!m_osFilterPrefix.empty() &&
                m_osFilterPrefix.size() > osName.size())
            {
                if (STARTS_WITH(m_osFilterPrefix.c_str(), osName.c_str()) &&
                    m_osFilterPrefix[osName.size()] == '/')
                {
#if !defined(__sun) && !defined(__HAIKU__)
                    if (psEntry->d_type == DT_UNKNOWN)
#endif
                    {
                        StatFile();
                    }
                    if (VSI_ISDIR(entry.nMode))
                    {
                        goto begin;
                    }
                }
                continue;
            }
            if (!m_osFilterPrefix.empty() &&
                !STARTS_WITH(osName.c_str(), m_osFilterPrefix.c_str()))
            {
                continue;
            }

            if (!m_bNameAndTypeOnly
#if !defined(__sun) && !defined(__HAIKU__)
                || psEntry->d_type == DT_UNKNOWN
#endif
            )
            {
                StatFile();
            }

            return &(entry);
        }
    }

    return nullptr;
}

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                             AddToTotal()                             */
/************************************************************************/

void VSIUnixStdioFilesystemHandler::AddToTotal(vsi_l_offset nBytes)
{
    CPLMutexHolder oHolder(&hMutex);
    nTotalBytesRead += nBytes;
}

#endif

/************************************************************************/
/*                        GetCanonicalFilename()                        */
/************************************************************************/

#ifdef HAS_CASE_INSENSITIVE_FILE_SYSTEM
std::string VSIUnixStdioFilesystemHandler::GetCanonicalFilename(
    const std::string &osFilename) const
{
    char szResolvedPath[PATH_MAX];
    const char *pszFilename = osFilename.c_str();
    if (realpath(pszFilename, szResolvedPath))
    {
        const char *pszFilenameLastPart = strrchr(pszFilename, '/');
        const char *pszResolvedFilenameLastPart = strrchr(szResolvedPath, '/');
        if (pszFilenameLastPart && pszResolvedFilenameLastPart &&
            EQUAL(pszFilenameLastPart, pszResolvedFilenameLastPart))
        {
            std::string osRet;
            osRet.assign(pszFilename, pszFilenameLastPart - pszFilename);
            osRet += pszResolvedFilenameLastPart;
            return osRet;
        }
        return szResolvedPath;
    }
    return osFilename;
}
#endif

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

void VSIInstallLargeFileHandler()

{
    VSIFileManager::InstallHandler(
        "", std::make_shared<VSIUnixStdioFilesystemHandler>());
}

#endif  // ndef WIN32

//! @endcond
