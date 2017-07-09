/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Unix platforms with fseek64()
 *           and ftell64() such as IRIX.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

class VSIUnixStdioFilesystemHandler CPL_FINAL : public VSIFilesystemHandler
{
#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset  nTotalBytesRead;
    CPLMutex     *hMutex;
#endif

public:
                              VSIUnixStdioFilesystemHandler();
#ifdef VSI_COUNT_BYTES_READ
    virtual                  ~VSIUnixStdioFilesystemHandler();
#endif

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;
    virtual int     Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                          int nFlags ) override;
    virtual int     Unlink( const char *pszFilename ) override;
    virtual int     Rename( const char *oldpath, const char *newpath ) override;
    virtual int     Mkdir( const char *pszDirname, long nMode ) override;
    virtual int     Rmdir( const char *pszDirname ) override;
    virtual char  **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;
    virtual GIntBig GetDiskFreeSpace( const char* pszDirname ) override;
    virtual int SupportsSparseFiles( const char* pszPath ) override;

#ifdef VSI_COUNT_BYTES_READ
    void             AddToTotal(vsi_l_offset nBytes);
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIUnixStdioHandle                            */
/* ==================================================================== */
/************************************************************************/

class VSIUnixStdioHandle CPL_FINAL : public VSIVirtualHandle
{
    FILE          *fp;
    vsi_l_offset  m_nOffset;
    bool          bReadOnly;
    bool          bLastOpWrite;
    bool          bLastOpRead;
    bool          bAtEOF;
    // In a+ mode, disable any optimization since the behaviour of the file
    // pointer on Mac and other BSD system is to have a seek() to the end of
    // file and thus a call to our Seek(0, SEEK_SET) before a read will be a
    // no-op.
    bool          bModeAppendReadWrite;
#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset  nTotalBytesRead;
    VSIUnixStdioFilesystemHandler *poFS;
#endif
  public:
                   VSIUnixStdioHandle( VSIUnixStdioFilesystemHandler *poFSIn,
                                       FILE* fpIn, bool bReadOnlyIn,
                                       bool bModeAppendReadWriteIn );

    virtual int    Seek( vsi_l_offset nOffsetIn, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    virtual size_t Write( const void *pBuffer, size_t nSize,
                          size_t nMemb ) override;
    virtual int    Eof() override;
    virtual int    Flush() override;
    virtual int    Close() override;
    virtual int    Truncate( vsi_l_offset nNewSize ) override;
    virtual void  *GetNativeFileDescriptor() override {
        return reinterpret_cast<void *>(static_cast<size_t>(fileno(fp))); }
    virtual VSIRangeStatus GetRangeStatus( vsi_l_offset nOffset,
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
    m_nOffset(0),
    bReadOnly(bReadOnlyIn),
    bLastOpWrite(false),
    bLastOpRead(false),
    bAtEOF(false),
    bModeAppendReadWrite(bModeAppendReadWriteIn)
#ifdef VSI_COUNT_BYTES_READ
    ,
    nTotalBytesRead(0),
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

/************************************************************************/
/*                      VSIUnixStdioFilesystemHandler()                 */
/************************************************************************/

VSIUnixStdioFilesystemHandler::VSIUnixStdioFilesystemHandler()
#ifdef VSI_COUNT_BYTES_READ
     : nTotalBytesRead(0),
       hMutex(NULL)
#endif
{}

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

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIUnixStdioFilesystemHandler::Open( const char *pszFilename,
                                     const char *pszAccess,
                                     bool bSetError )

{
    FILE *fp = VSI_FOPEN64( pszFilename, pszAccess );
    const int nError = errno;

    VSIDebug3( "VSIUnixStdioFilesystemHandler::Open(\"%s\",\"%s\") = %p",
               pszFilename, pszAccess, fp );

    if( fp == NULL )
    {
        if( bSetError )
        {
            VSIError(VSIE_FileError, "%s: %s", pszFilename, strerror(nError));
        }
        errno = nError;
        return NULL;
    }

    const bool bReadOnly =
        strcmp(pszAccess, "rb") == 0 || strcmp(pszAccess, "r") == 0;
    const bool bModeAppendReadWrite =
        strcmp(pszAccess, "a+b") == 0 || strcmp(pszAccess, "a+") == 0;
    VSIUnixStdioHandle *poHandle =
        new(std::nothrow) VSIUnixStdioHandle( this, fp, bReadOnly,
                                              bModeAppendReadWrite );
    if( poHandle == NULL )
    {
        fclose(fp);
        return NULL;
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
    if( hDir != NULL )
    {
        // We want to avoid returning NULL for an empty list.
        oDir.Assign(static_cast<char**>(CPLCalloc(2, sizeof(char*))));

        struct dirent *psDirEntry = NULL;
        while( (psDirEntry = readdir(hDir)) != NULL )
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
        switch( sStatFS.f_type )
        {
            // Codes from http://man7.org/linux/man-pages/man2/statfs.2.html
            case 0xef53:  // ext2, 3, 4
            case 0x52654973:  // reiser
            case 0x58465342:  // xfs
            case 0x3153464a:  // jfs
            case 0x5346544e:  // ntfs
            case 0x9123683e:  // brfs
            // nfs: NFS < 4.2 supports creating sparse files (but reading them
            // not efficiently).
            case 0x6969:
            case 0x01021994:  // tmpfs
                return TRUE;

            case 0x4d44: // msdos
                return FALSE;

            default:
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
