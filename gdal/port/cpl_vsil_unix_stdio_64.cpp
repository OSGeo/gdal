/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Unix platforms with fseek64()
 *           and ftell64() such as IRIX.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************
 *
 * NB: Note that in wrappers we are always saving the error state (errno
 * variable) to avoid side effects during debug prints or other possible
 * standard function calls (error states will be overwritten after such
 * a call).
 *
 ****************************************************************************/

//! @cond Doxygen_Suppress

//#define VSI_COUNT_BYTES_READ

// Some unusual filesystems do not work if _FORTIFY_SOURCE in GCC or
// clang is used within this source file, especially if techniques
// like those in vsipreload are used.  Fortify source interacts poorly with
// filesystems that use fread for forward seeks.  This leads to SIGSEGV within
// fread calls.
//
// See this for hardening background info: https://wiki.debian.org/Hardening
#undef _FORTIFY_SOURCE

// 64 bit IO is only available on 32-bit android since API 24 / Android 7.0
// See https://android.googlesource.com/platform/bionic/+/master/docs/32-bit-abi.md
#if defined(__ANDROID_API__) && __ANDROID_API__ >= 24
#define _FILE_OFFSET_BITS 64
#endif

#include "cpl_port.h"

#if !defined(WIN32)

#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_STATVFS
#include <sys/statvfs.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <new>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_error.h"

CPL_CVSID("$Id$")

#if defined(UNIX_STDIO_64)

#ifndef VSI_FTELL64
#define VSI_FTELL64 ftell64
#endif
#ifndef VSI_FSEEK64
#define VSI_FSEEK64 fseek64
#endif
#ifndef VSI_FOPEN64
#define VSI_FOPEN64 fopen64
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

#ifndef VSI_FTELL64
#define VSI_FTELL64 ftell
#endif
#ifndef VSI_FSEEK64
#define VSI_FSEEK64 fseek
#endif
#ifndef VSI_FOPEN64
#define VSI_FOPEN64 fopen
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

/************************************************************************/
/* ==================================================================== */
/*                       VSIUnixStdioFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

class VSIUnixStdioFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIUnixStdioFilesystemHandler)

#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset  nTotalBytesRead  = 0;
    CPLMutex     *hMutex = nullptr;
#endif

public:
    VSIUnixStdioFilesystemHandler() = default;
#ifdef VSI_COUNT_BYTES_READ
    ~VSIUnixStdioFilesystemHandler() override;
#endif

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
    int Unlink( const char *pszFilename ) override;
    int Rename( const char *oldpath, const char *newpath ) override;
    int Mkdir( const char *pszDirname, long nMode ) override;
    int Rmdir( const char *pszDirname ) override;
    char **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;
    GIntBig GetDiskFreeSpace( const char* pszDirname ) override;
    int SupportsSparseFiles( const char* pszPath ) override;

    VSIDIR* OpenDir( const char *pszPath, int nRecurseDepth,
                             const char* const *papszOptions) override;

#ifdef VSI_COUNT_BYTES_READ
    void             AddToTotal(vsi_l_offset nBytes);
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIUnixStdioHandle                            */
/* ==================================================================== */
/************************************************************************/

class VSIUnixStdioHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIUnixStdioHandle)

    FILE          *fp = nullptr;
    vsi_l_offset  m_nOffset = 0;
    bool          bReadOnly = true;
    bool          bLastOpWrite = false;
    bool          bLastOpRead = false;
    bool          bAtEOF = false;
    // In a+ mode, disable any optimization since the behavior of the file
    // pointer on Mac and other BSD system is to have a seek() to the end of
    // file and thus a call to our Seek(0, SEEK_SET) before a read will be a
    // no-op.
    bool          bModeAppendReadWrite = false;
#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset  nTotalBytesRead = 0;
    VSIUnixStdioFilesystemHandler *poFS = nullptr;
#endif
  public:
    VSIUnixStdioHandle( VSIUnixStdioFilesystemHandler *poFSIn,
                        FILE* fpIn, bool bReadOnlyIn,
                        bool bModeAppendReadWriteIn );

    int Seek( vsi_l_offset nOffsetIn, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
    int Truncate( vsi_l_offset nNewSize ) override;
    void *GetNativeFileDescriptor() override {
        return reinterpret_cast<void *>(static_cast<size_t>(fileno(fp))); }
    VSIRangeStatus GetRangeStatus( vsi_l_offset nOffset,
                                   vsi_l_offset nLength ) override;
};

/************************************************************************/
/*                       VSIUnixStdioHandle()                           */
/************************************************************************/

VSIUnixStdioHandle::VSIUnixStdioHandle(
#ifndef VSI_COUNT_BYTES_READ
CPL_UNUSED
#endif
                                       VSIUnixStdioFilesystemHandler *poFSIn,
                                       FILE* fpIn, bool bReadOnlyIn,
                                       bool bModeAppendReadWriteIn) :
    fp(fpIn),
    bReadOnly(bReadOnlyIn),
    bModeAppendReadWrite(bModeAppendReadWriteIn)
#ifdef VSI_COUNT_BYTES_READ
    ,
    poFS(poFSIn)
#endif
{}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIUnixStdioHandle::Close()

{
    VSIDebug1( "VSIUnixStdioHandle::Close(%p)", fp );

#ifdef VSI_COUNT_BYTES_READ
    poFS->AddToTotal(nTotalBytesRead);
#endif

    return fclose( fp );
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIUnixStdioHandle::Seek( vsi_l_offset nOffsetIn, int nWhence )
{
    bAtEOF = false;

    // Seeks that do nothing are still surprisingly expensive with MSVCRT.
    // try and short circuit if possible.
    if( !bModeAppendReadWrite && nWhence == SEEK_SET && nOffsetIn == m_nOffset )
        return 0;

    // On a read-only file, we can avoid a lseek() system call to be issued
    // if the next position to seek to is within the buffered page.
    if( bReadOnly && nWhence == SEEK_SET )
    {
        const int l_PAGE_SIZE = 4096;
        if( nOffsetIn > m_nOffset && nOffsetIn < l_PAGE_SIZE + m_nOffset )
        {
            const int nDiff = static_cast<int>(nOffsetIn - m_nOffset);
            // Do not zero-initialize the buffer. We don't read from it
            GByte abyTemp[l_PAGE_SIZE];
            const int nRead = static_cast<int>(fread(abyTemp, 1, nDiff, fp));
            if( nRead == nDiff )
            {
                m_nOffset = nOffsetIn;
                bLastOpWrite = false;
                bLastOpRead = false;
                return 0;
            }
        }
    }

    const int nResult = VSI_FSEEK64( fp, nOffsetIn, nWhence );
    const int nError = errno;

#ifdef VSI_DEBUG

    if( nWhence == SEEK_SET )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB
                   ",SEEK_SET) = %d",
                   fp, nOffsetIn, nResult );
    }
    else if( nWhence == SEEK_END )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB
                   ",SEEK_END) = %d",
                   fp, nOffsetIn, nResult );
    }
    else if( nWhence == SEEK_CUR )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB
                   ",SEEK_CUR) = %d",
                   fp, nOffsetIn, nResult );
    }
    else
    {
        VSIDebug4( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB
                   ",%d-Unknown) = %d",
                   fp, nOffsetIn, nWhence, nResult );
    }

#endif

    if( nResult != -1 )
    {
        if( nWhence == SEEK_SET )
        {
            m_nOffset = nOffsetIn;
        }
        else if( nWhence == SEEK_END )
        {
            m_nOffset = VSI_FTELL64( fp );
        }
        else if( nWhence == SEEK_CUR )
        {
            if( nOffsetIn > INT_MAX )
            {
                //printf("likely negative offset intended\n");
            }
            m_nOffset += nOffsetIn;
        }
    }

    bLastOpWrite = false;
    bLastOpRead = false;

    errno = nError;
    return nResult;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIUnixStdioHandle::Tell()

{
#if 0
    const vsi_l_offset nOffset = VSI_FTELL64( fp );
    const int nError = errno;

    VSIDebug2( "VSIUnixStdioHandle::Tell(%p) = %ld",
               fp, static_cast<long>(nOffset) );

    errno = nError;
#endif

    return m_nOffset;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIUnixStdioHandle::Flush()

{
    VSIDebug1( "VSIUnixStdioHandle::Flush(%p)", fp );

    return fflush( fp );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIUnixStdioHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
/* -------------------------------------------------------------------- */
/*      If a fwrite() is followed by an fread(), the POSIX rules are    */
/*      that some of the write may still be buffered and lost.  We      */
/*      are required to do a seek between to force flushing.   So we    */
/*      keep careful track of what happened last to know if we          */
/*      skipped a flushing seek that we may need to do now.             */
/* -------------------------------------------------------------------- */
    if( !bModeAppendReadWrite && bLastOpWrite )
    {
        if( VSI_FSEEK64( fp, m_nOffset, SEEK_SET ) != 0 )
        {
            VSIDebug1("Write calling seek failed. %d", m_nOffset);
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform the read.                                               */
/* -------------------------------------------------------------------- */
    const size_t nResult = fread( pBuffer, nSize, nCount, fp );

#ifdef VSI_DEBUG
    const int nError = errno;
    VSIDebug4( "VSIUnixStdioHandle::Read(%p,%ld,%ld) = %ld",
               fp, static_cast<long>(nSize), static_cast<long>(nCount),
               static_cast<long>(nResult) );
    errno = nError;
#endif

/* -------------------------------------------------------------------- */
/*      Update current offset.                                          */
/* -------------------------------------------------------------------- */

#ifdef VSI_COUNT_BYTES_READ
    nTotalBytesRead += nSize * nResult;
#endif

    m_nOffset += nSize * nResult;
    bLastOpWrite = false;
    bLastOpRead = true;

    if( nResult != nCount )
    {
        errno = 0;
        vsi_l_offset nNewOffset = VSI_FTELL64( fp );
        if( errno == 0 ) // ftell() can fail if we are end of file with a pipe.
            m_nOffset = nNewOffset;
        else
            CPLDebug("VSI", "%s", VSIStrerror(errno));
        bAtEOF = CPL_TO_BOOL(feof(fp));
    }

    return nResult;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIUnixStdioHandle::Write( const void * pBuffer, size_t nSize,
                                  size_t nCount )

{
/* -------------------------------------------------------------------- */
/*      If a fwrite() is followed by an fread(), the POSIX rules are    */
/*      that some of the write may still be buffered and lost.  We      */
/*      are required to do a seek between to force flushing.   So we    */
/*      keep careful track of what happened last to know if we          */
/*      skipped a flushing seek that we may need to do now.             */
/* -------------------------------------------------------------------- */
    if( !bModeAppendReadWrite && bLastOpRead )
    {
        if( VSI_FSEEK64( fp, m_nOffset, SEEK_SET ) != 0 )
        {
            VSIDebug1("Write calling seek failed. %d", m_nOffset);
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform the write.                                              */
/* -------------------------------------------------------------------- */
    const size_t nResult = fwrite( pBuffer, nSize, nCount, fp );

#if VSI_DEBUG
    const int nError = errno;

    VSIDebug4( "VSIUnixStdioHandle::Write(%p,%ld,%ld) = %ld",
               fp, static_cast<long>(nSize), static_cast<long>(nCount),
               static_cast<long>(nResult) );

    errno = nError;
#endif

/* -------------------------------------------------------------------- */
/*      Update current offset.                                          */
/* -------------------------------------------------------------------- */
    m_nOffset += nSize * nResult;
    bLastOpWrite = true;
    bLastOpRead = false;

    return nResult;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIUnixStdioHandle::Eof()

{
    return bAtEOF ? TRUE : FALSE;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

int VSIUnixStdioHandle::Truncate( vsi_l_offset nNewSize )
{
    fflush(fp);
    return VSI_FTRUNCATE64( fileno(fp), nNewSize );
}

/************************************************************************/
/*                          GetRangeStatus()                            */
/************************************************************************/

#ifdef __linux
#include <linux/fs.h>  // FS_IOC_FIEMAP
#ifdef FS_IOC_FIEMAP
#include <linux/types.h>   // for types used in linux/fiemap.h
#include <linux/fiemap.h>  // struct fiemap
#endif
#include <sys/ioctl.h>
#include <errno.h>
#endif

VSIRangeStatus VSIUnixStdioHandle::GetRangeStatus( vsi_l_offset
#ifdef FS_IOC_FIEMAP
                                                                nOffset
#endif
                                                     , vsi_l_offset
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
    int fd = fileno(fp);
    struct fiemap *psExtentMap = reinterpret_cast<struct fiemap *>(&abyBuffer);
    memset(psExtentMap,
           0,
           sizeof(struct fiemap) + sizeof(struct fiemap_extent));
    psExtentMap->fm_start = nOffset;
    psExtentMap->fm_length = nLength;
    psExtentMap->fm_extent_count = 1;
    int ret = ioctl(fd, FS_IOC_FIEMAP, psExtentMap);
    if( ret < 0 )
        return VSI_RANGE_STATUS_UNKNOWN;
    if( psExtentMap->fm_mapped_extents == 0 )
        return VSI_RANGE_STATUS_HOLE;
    // In case there is one extent with unknown status, retry after having
    // asked the kernel to sync the file.
    const fiemap_extent* pasExtent = &(psExtentMap->fm_extents[0]);
    if( psExtentMap->fm_mapped_extents == 1 &&
        (pasExtent[0].fe_flags & FIEMAP_EXTENT_UNKNOWN) != 0 )
    {
        psExtentMap->fm_flags = FIEMAP_FLAG_SYNC;
        psExtentMap->fm_start = nOffset;
        psExtentMap->fm_length = nLength;
        psExtentMap->fm_extent_count = 1;
        ret = ioctl(fd, FS_IOC_FIEMAP, psExtentMap);
        if( ret < 0 )
            return VSI_RANGE_STATUS_UNKNOWN;
        if( psExtentMap->fm_mapped_extents == 0 )
            return VSI_RANGE_STATUS_HOLE;
    }
    return VSI_RANGE_STATUS_DATA;
#else
    static bool bMessageEmitted = false;
    if( !bMessageEmitted )
    {
        CPLDebug("VSI",
                 "Sorry: GetExtentStatus() not implemented for "
                 "this operating system");
        bMessageEmitted = true;
    }
    return VSI_RANGE_STATUS_UNKNOWN;
#endif
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIUnixStdioFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                     ~VSIUnixStdioFilesystemHandler()                 */
/************************************************************************/

VSIUnixStdioFilesystemHandler::~VSIUnixStdioFilesystemHandler()
{
    CPLDebug( "VSI",
              "~VSIUnixStdioFilesystemHandler() : nTotalBytesRead = "
              CPL_FRMT_GUIB,
              nTotalBytesRead );

    if( hMutex != nullptr )
        CPLDestroyMutex( hMutex );
    hMutex = nullptr;
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIUnixStdioFilesystemHandler::Open( const char *pszFilename,
                                     const char *pszAccess,
                                     bool bSetError,
                                     CSLConstList /* papszOptions */ )

{
    FILE *fp = VSI_FOPEN64( pszFilename, pszAccess );
    const int nError = errno;

    VSIDebug3( "VSIUnixStdioFilesystemHandler::Open(\"%s\",\"%s\") = %p",
               pszFilename, pszAccess, fp );

    if( fp == nullptr )
    {
        if( bSetError )
        {
            VSIError(VSIE_FileError, "%s: %s", pszFilename, strerror(nError));
        }
        errno = nError;
        return nullptr;
    }

    const bool bReadOnly =
        strcmp(pszAccess, "rb") == 0 || strcmp(pszAccess, "r") == 0;
    const bool bModeAppendReadWrite =
        strcmp(pszAccess, "a+b") == 0 || strcmp(pszAccess, "a+") == 0;
    VSIUnixStdioHandle *poHandle =
        new(std::nothrow) VSIUnixStdioHandle( this, fp, bReadOnly,
                                              bModeAppendReadWrite );
    if( poHandle == nullptr )
    {
        fclose(fp);
        return nullptr;
    }

    errno = nError;

/* -------------------------------------------------------------------- */
/*      If VSI_CACHE is set we want to use a cached reader instead      */
/*      of more direct io on the underlying file.                       */
/* -------------------------------------------------------------------- */
    if( bReadOnly &&
        CPLTestBool( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
    {
        return VSICreateCachedFile( poHandle );
    }

    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Stat( const char * pszFilename,
                                         VSIStatBufL * pStatBuf,
                                         int /* nFlags */ )
{
    return( VSI_STAT64( pszFilename, pStatBuf ) );
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Unlink( const char * pszFilename )

{
    return unlink( pszFilename );
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Rename( const char *oldpath,
                                           const char *newpath )

{
    return rename( oldpath, newpath );
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Mkdir( const char * pszPathname,
                                          long nMode )

{
    return mkdir( pszPathname, static_cast<int>(nMode) );
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Rmdir( const char * pszPathname )

{
    return rmdir( pszPathname );
}

/************************************************************************/
/*                              ReadDirEx()                             */
/************************************************************************/

char **VSIUnixStdioFilesystemHandler::ReadDirEx( const char *pszPath,
                                                 int nMaxFiles )

{
    if( strlen(pszPath) == 0 )
        pszPath = ".";

    CPLStringList oDir;
    DIR *hDir = opendir(pszPath);
    if( hDir != nullptr )
    {
        // We want to avoid returning NULL for an empty list.
        oDir.Assign(static_cast<char**>(CPLCalloc(2, sizeof(char*))));

        struct dirent *psDirEntry = nullptr;
        while( (psDirEntry = readdir(hDir)) != nullptr )
        {
            oDir.AddString( psDirEntry->d_name );
            if( nMaxFiles > 0 && oDir.Count() > nMaxFiles )
                break;
        }

        closedir( hDir );
    }
    else
    {
        // Should we generate an error?
        // For now we'll just return NULL (at the end of the function).
    }

    return oDir.StealList();
}

/************************************************************************/
/*                        GetDiskFreeSpace()                            */
/************************************************************************/

GIntBig VSIUnixStdioFilesystemHandler::GetDiskFreeSpace( const char*
#ifdef HAVE_STATVFS
                                                         pszDirname
#endif
                                                       )
{
    GIntBig nRet = -1;
#ifdef HAVE_STATVFS

#ifdef HAVE_STATVFS64
    struct statvfs64 buf;
    if( statvfs64(pszDirname, &buf) == 0 )
    {
        nRet = static_cast<GIntBig>(buf.f_frsize *
                                    static_cast<GUIntBig>(buf.f_bavail));
    }
#else
    struct statvfs buf;
    if( statvfs(pszDirname, &buf) == 0 )
    {
        nRet = static_cast<GIntBig>(buf.f_frsize *
                                    static_cast<GUIntBig>(buf.f_bavail));
    }
#endif

#endif
    return nRet;
}

/************************************************************************/
/*                      SupportsSparseFiles()                           */
/************************************************************************/

#ifdef __linux
#include <sys/vfs.h>
#endif

int VSIUnixStdioFilesystemHandler::SupportsSparseFiles( const char*
#ifdef __linux
                                                        pszPath
#endif
                                                        )
{
#ifdef __linux
    struct statfs sStatFS;
    if( statfs( pszPath, &sStatFS ) == 0 )
    {
        // Add here any missing filesystem supporting sparse files.
        // See http://en.wikipedia.org/wiki/Comparison_of_file_systems
        switch( static_cast<unsigned>(sStatFS.f_type) )
        {
            // Codes from http://man7.org/linux/man-pages/man2/statfs.2.html
            case 0xef53U:  // ext2, 3, 4
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

            case 0x4d44U: // msdos
                return FALSE;

            case 0x53464846U:  // Windows Subsystem for Linux fs
            {
                static bool bUnknownFSEmitted = false;
                if( !bUnknownFSEmitted )
                {
                    CPLDebug("VSI", "Windows Subsystem for Linux FS is at "
                             "the time of writing not known to support sparse "
                             "files");
                    bUnknownFSEmitted = true;
                }
                return FALSE;
            }

            default:
            {
                static bool bUnknownFSEmitted = false;
                if( !bUnknownFSEmitted )
                {
                    CPLDebug("VSI", "Filesystem with type %X unknown. "
                             "Assuming it does not support sparse files",
                             static_cast<int>(sStatFS.f_type) );
                    bUnknownFSEmitted = true;
                }
                return FALSE;
            }
        }
    }
    return FALSE;
#else
    static bool bMessageEmitted = false;
    if( !bMessageEmitted )
    {
        CPLDebug("VSI",
                 "Sorry: SupportsSparseFiles() not implemented "
                 "for this operating system");
        bMessageEmitted = true;
    }
    return FALSE;
#endif
}

/************************************************************************/
/*                            VSIDIRUnixStdio                           */
/************************************************************************/

struct VSIDIRUnixStdio final: public VSIDIR
{
    CPLString osRootPath{};
    CPLString osBasePath{};
    DIR* m_psDir = nullptr;
    int nRecurseDepth = 0;
    VSIDIREntry entry{};
    std::vector<VSIDIRUnixStdio*> aoStackSubDir{};
    VSIUnixStdioFilesystemHandler* poFS = nullptr;
    std::string m_osFilterPrefix{};
    bool m_bNameAndTypeOnly = false;

    explicit VSIDIRUnixStdio(VSIUnixStdioFilesystemHandler* poFSIn): poFS(poFSIn) {}
    ~VSIDIRUnixStdio();

    const VSIDIREntry* NextDirEntry() override;

    VSIDIRUnixStdio(const VSIDIRUnixStdio&) = delete;
    VSIDIRUnixStdio& operator=(const VSIDIRUnixStdio&) = delete;
};

/************************************************************************/
/*                        ~VSIDIRUnixStdio()                            */
/************************************************************************/

VSIDIRUnixStdio::~VSIDIRUnixStdio()
{
    while( !aoStackSubDir.empty() )
    {
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }
    closedir(m_psDir);
}

/************************************************************************/
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR* VSIUnixStdioFilesystemHandler::OpenDir( const char *pszPath,
                                       int nRecurseDepth,
                                       const char* const * papszOptions)
{
    DIR* psDir = opendir(pszPath);
    if( psDir == nullptr )
    {
        return nullptr;
    }
    VSIDIRUnixStdio* dir = new VSIDIRUnixStdio(this);
    dir->osRootPath = pszPath;
    dir->nRecurseDepth = nRecurseDepth;
    dir->m_psDir = psDir;
    dir->m_osFilterPrefix = CSLFetchNameValueDef(papszOptions, "PREFIX", "");
    dir->m_bNameAndTypeOnly = CPLTestBool(CSLFetchNameValueDef(papszOptions, "NAME_AND_TYPE_ONLY", "NO"));
    return dir;
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry* VSIDIRUnixStdio::NextDirEntry()
{
begin:
    if( VSI_ISDIR(entry.nMode) && nRecurseDepth != 0 )
    {
        CPLString osCurFile(osRootPath);
        if( !osCurFile.empty() )
            osCurFile += '/';
        osCurFile += entry.pszName;
        auto subdir = static_cast<VSIDIRUnixStdio*>(
            poFS->VSIUnixStdioFilesystemHandler::OpenDir(osCurFile,
                nRecurseDepth - 1, nullptr));
        if( subdir )
        {
            subdir->osRootPath = osRootPath;
            subdir->osBasePath = entry.pszName;
            subdir->m_osFilterPrefix = m_osFilterPrefix;
            subdir->m_bNameAndTypeOnly = m_bNameAndTypeOnly;
            aoStackSubDir.push_back(subdir);
        }
        entry.nMode = 0;
    }

    while( !aoStackSubDir.empty() )
    {
        auto l_entry = aoStackSubDir.back()->NextDirEntry();
        if( l_entry )
        {
            return l_entry;
        }
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }

    while( true )
    {
        const auto* psEntry = readdir(m_psDir);
        if( psEntry == nullptr )
        {
            return nullptr;
        }
        // Skip . and ..entries
        if( psEntry->d_name[0] == '.' &&
            (psEntry->d_name[1] == '\0' ||
             (psEntry->d_name[1] == '.' &&
              psEntry->d_name[2] == '\0')) )
        {
            // do nothing
        }
        else
        {
            CPLFree(entry.pszName);
            CPLString osName(osBasePath);
            if( !osName.empty() )
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
            if( !osCurFile.empty() )
                osCurFile += '/';
            osCurFile += entry.pszName;

            if( psEntry->d_type == DT_REG )
                entry.nMode = S_IFREG;
            else if( psEntry->d_type == DT_DIR )
                entry.nMode = S_IFDIR;
            else if( psEntry->d_type == DT_LNK )
                entry.nMode = S_IFLNK;

            const auto StatFile = [&osCurFile, this]()
            {
                VSIStatBufL sStatL;
                if( VSIStatL(osCurFile, &sStatL) == 0 )
                {
                    entry.nMode = sStatL.st_mode;
                    entry.nSize = sStatL.st_size;
                    entry.nMTime = sStatL.st_mtime;
                    entry.bModeKnown = true;
                    entry.bSizeKnown = true;
                    entry.bMTimeKnown = true;
                }
            };

            if( !m_osFilterPrefix.empty() &&
                m_osFilterPrefix.size() > osName.size() )
            {
                if( STARTS_WITH(m_osFilterPrefix.c_str(), osName.c_str()) &&
                    m_osFilterPrefix[osName.size()] == '/' )
                {
                    if( psEntry->d_type == DT_UNKNOWN )
                        StatFile();
                    if( VSI_ISDIR(entry.nMode) )
                    {
                        goto begin;
                    }
                }
                continue;
            }
            if( !m_osFilterPrefix.empty() &&
                !STARTS_WITH(osName.c_str(), m_osFilterPrefix.c_str()) )
            {
                continue;
            }

            if( !m_bNameAndTypeOnly || psEntry->d_type == DT_UNKNOWN )
            {
                StatFile();
            }

            break;
        }
    }

    return &(entry);
}

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                            AddToTotal()                              */
/************************************************************************/

void VSIUnixStdioFilesystemHandler::AddToTotal( vsi_l_offset nBytes )
{
    CPLMutexHolder oHolder(&hMutex);
    nTotalBytesRead += nBytes;
}

#endif

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

void VSIInstallLargeFileHandler()

{
    VSIFileManager::InstallHandler( "", new VSIUnixStdioFilesystemHandler() );
}

#endif  // ndef WIN32

//! @endcond
