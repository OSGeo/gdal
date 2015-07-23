/**********************************************************************
 * $Id$
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

//#define VSI_COUNT_BYTES_READ

#include "cpl_port.h"

#if !defined(WIN32) && !defined(WIN32CE)

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

CPL_CVSID("$Id$");

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

class VSIUnixStdioFilesystemHandler : public VSIFilesystemHandler 
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
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );

#ifdef VSI_COUNT_BYTES_READ
    void             AddToTotal(vsi_l_offset nBytes);
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIUnixStdioHandle                            */
/* ==================================================================== */
/************************************************************************/

class VSIUnixStdioHandle : public VSIVirtualHandle
{
    FILE          *fp;
    vsi_l_offset  nOffset;
    int           bReadOnly;
    int           bLastOpWrite;
    int           bLastOpRead;
    int           bAtEOF;
#ifdef VSI_COUNT_BYTES_READ
    vsi_l_offset  nTotalBytesRead;
    VSIUnixStdioFilesystemHandler *poFS;
#endif
  public:
                      VSIUnixStdioHandle(VSIUnixStdioFilesystemHandler *poFSIn,
                                         FILE* fpIn, int bReadOnlyIn);

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
    virtual int       Truncate( vsi_l_offset nNewSize );
    virtual void     *GetNativeFileDescriptor() { return (void*) (size_t) fileno(fp); }
};


/************************************************************************/
/*                       VSIUnixStdioHandle()                           */
/************************************************************************/

VSIUnixStdioHandle::VSIUnixStdioHandle(
#ifndef VSI_COUNT_BYTES_READ
CPL_UNUSED
#endif
                                       VSIUnixStdioFilesystemHandler *poFSIn,
                                       FILE* fpIn, int bReadOnlyIn) :
    fp(fpIn), nOffset(0), bReadOnly(bReadOnlyIn), bLastOpWrite(FALSE), bLastOpRead(FALSE), bAtEOF(FALSE)
#ifdef VSI_COUNT_BYTES_READ
    , nTotalBytesRead(0), poFS(poFSIn)
#endif
{
}

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

int VSIUnixStdioHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    bAtEOF = FALSE;

    // seeks that do nothing are still surprisingly expensive with MSVCRT.
    // try and short circuit if possible.
    if( nWhence == SEEK_SET && nOffset == this->nOffset )
        return 0;

    // on a read-only file, we can avoid a lseek() system call to be issued
    // if the next position to seek to is within the buffered page
    if( bReadOnly && nWhence == SEEK_SET )
    {
        GIntBig nDiff = (GIntBig)nOffset - (GIntBig)this->nOffset;
        if( nDiff > 0 && nDiff < 4096 )
        {
            GByte abyTemp[4096];
            int nRead = (int)fread(abyTemp, 1, (int)nDiff, fp);
            if( nRead == (int)nDiff )
            {
                this->nOffset = nOffset;
                bLastOpWrite = FALSE;
                bLastOpRead = FALSE;
                return 0;
            }
        }
    }

    const int nResult = VSI_FSEEK64( fp, nOffset, nWhence );
    int nError = errno;

#ifdef VSI_DEBUG

    if( nWhence == SEEK_SET )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB ",SEEK_SET) = %d",
                   fp, nOffset, nResult );
    }
    else if( nWhence == SEEK_END )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB ",SEEK_END) = %d",
                   fp, nOffset, nResult );
    }
    else if( nWhence == SEEK_CUR )
    {
        VSIDebug3( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB ",SEEK_CUR) = %d",
                   fp, nOffset, nResult );
    }
    else
    {
        VSIDebug4( "VSIUnixStdioHandle::Seek(%p," CPL_FRMT_GUIB ",%d-Unknown) = %d",
                   fp, nOffset, nWhence, nResult );
    }

#endif

    if( nResult != -1 )
    {
        if( nWhence == SEEK_SET )
        {
            this->nOffset = nOffset;
        }
        else if( nWhence == SEEK_END )
        {
            this->nOffset = VSI_FTELL64( fp );
        }
        else if( nWhence == SEEK_CUR )
        {
            this->nOffset += nOffset;
        }
    }

    bLastOpWrite = FALSE;
    bLastOpRead = FALSE;

    errno = nError;
    return nResult;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIUnixStdioHandle::Tell()

{
#ifdef notdef
    vsi_l_offset    nOffset = VSI_FTELL64( fp );
    int             nError = errno;

    VSIDebug2( "VSIUnixStdioHandle::Tell(%p) = %ld", fp, (long)nOffset );

    errno = nError;
    return nOffset;
#endif
    return nOffset;
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
    if( bLastOpWrite )
        VSI_FSEEK64( fp, nOffset, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Perform the read.                                               */
/* -------------------------------------------------------------------- */
    size_t  nResult = fread( pBuffer, nSize, nCount, fp );
    int     nError = errno;

    VSIDebug4( "VSIUnixStdioHandle::Read(%p,%ld,%ld) = %ld", 
               fp, (long)nSize, (long)nCount, (long)nResult );

    errno = nError;

/* -------------------------------------------------------------------- */
/*      Update current offset.                                          */
/* -------------------------------------------------------------------- */

#ifdef VSI_COUNT_BYTES_READ
    nTotalBytesRead += nSize * nResult;
#endif

    nOffset += nSize * nResult;
    bLastOpWrite = FALSE;
    bLastOpRead = TRUE;

    if (nResult != nCount)
    {
        errno = 0;
        vsi_l_offset nNewOffset = VSI_FTELL64( fp );
        if( errno == 0 ) /* ftell() can fail if we are end of file with a pipe */
            nOffset = nNewOffset;
        else
            CPLDebug("VSI", "%s", VSIStrerror(errno));
        bAtEOF = feof(fp);
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
    if( bLastOpRead )
        VSI_FSEEK64( fp, nOffset, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Perform the write.                                              */
/* -------------------------------------------------------------------- */
    size_t  nResult = fwrite( pBuffer, nSize, nCount, fp );
    int     nError = errno;

    VSIDebug4( "VSIUnixStdioHandle::Write(%p,%ld,%ld) = %ld", 
               fp, (long)nSize, (long)nCount, (long)nResult );

    errno = nError;

/* -------------------------------------------------------------------- */
/*      Update current offset.                                          */
/* -------------------------------------------------------------------- */
    nOffset += nSize * nResult;
    bLastOpWrite = TRUE;
    bLastOpRead = FALSE;
    
    return nResult;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIUnixStdioHandle::Eof()

{
    if( bAtEOF )
        return 1;
    else
        return 0;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

int VSIUnixStdioHandle::Truncate( vsi_l_offset nNewSize )
{
    fflush(fp);
    int nRet = VSI_FTRUNCATE64(fileno(fp), nNewSize);
    return nRet;
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
     : nTotalBytesRead(0), hMutex(NULL)
#endif
{
}

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                     ~VSIUnixStdioFilesystemHandler()                 */
/************************************************************************/

VSIUnixStdioFilesystemHandler::~VSIUnixStdioFilesystemHandler()
{
    CPLDebug( "VSI",
              "~VSIUnixStdioFilesystemHandler() : nTotalBytesRead = " CPL_FRMT_GUIB,
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
                                     const char *pszAccess )

{
    FILE    *fp = VSI_FOPEN64( pszFilename, pszAccess );
    int     nError = errno;
    
    VSIDebug3( "VSIUnixStdioFilesystemHandler::Open(\"%s\",\"%s\") = %p",
               pszFilename, pszAccess, fp );

    if( fp == NULL )
    {
        errno = nError;
        return NULL;
    }

    int bReadOnly = strcmp(pszAccess, "rb") == 0 || strcmp(pszAccess, "r") == 0;
    VSIUnixStdioHandle *poHandle = new VSIUnixStdioHandle(this, fp, bReadOnly );

    errno = nError;

/* -------------------------------------------------------------------- */
/*      If VSI_CACHE is set we want to use a cached reader instead      */
/*      of more direct io on the underlying file.                       */
/* -------------------------------------------------------------------- */
    if( bReadOnly
        && CSLTestBoolean( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
    {
        return VSICreateCachedFile( poHandle );
    }
    else
    {
        return poHandle;
    }
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Stat( const char * pszFilename,
                                         VSIStatBufL * pStatBuf,
                                         CPL_UNUSED int nFlags)
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
    return mkdir( pszPathname, nMode );
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIUnixStdioFilesystemHandler::Rmdir( const char * pszPathname )

{
    return rmdir( pszPathname );
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSIUnixStdioFilesystemHandler::ReadDir( const char *pszPath )

{
    DIR           *hDir;
    struct dirent *psDirEntry;
    CPLStringList  oDir;

    if (strlen(pszPath) == 0)
        pszPath = ".";

    if ( (hDir = opendir(pszPath)) != NULL )
    {
        // we want to avoid returning NULL for an empty list.
        oDir.Assign( (char**) CPLCalloc(2,sizeof(char*)) );

        while( (psDirEntry = readdir(hDir)) != NULL )
            oDir.AddString( psDirEntry->d_name );

        closedir( hDir );
    }
    else
    {
        /* Should we generate an error???  
         * For now we'll just return NULL (at the end of the function)
         */
    }

    return oDir.StealList();
}

#ifdef VSI_COUNT_BYTES_READ
/************************************************************************/
/*                            AddToTotal()                              */
/************************************************************************/

void VSIUnixStdioFilesystemHandler::AddToTotal(vsi_l_offset nBytes)
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

#endif /* ndef WIN32 */
