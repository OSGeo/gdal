/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Win32.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$");

#if defined(WIN32)

#include <windows.h>
#include "cpl_string.h"


#if !defined(WIN32CE)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#else
#  include <wce_io.h>
#  include <wce_errno.h>
#  include <wce_stdio.h>
#  include <wce_stat.h>
#  include "cpl_win32ce_api.h"
#endif


/************************************************************************/
/* ==================================================================== */
/*                       VSIWin32FilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

class VSIWin32FilesystemHandler : public VSIFilesystemHandler 
{
public:
    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );
    virtual int      IsCaseSensitive( const char* pszFilename )
                      { (void) pszFilename; return FALSE; }
};

/************************************************************************/
/* ==================================================================== */
/*                            VSIWin32Handle                            */
/* ==================================================================== */
/************************************************************************/

class VSIWin32Handle : public VSIVirtualHandle
{
  public:
    HANDLE       hFile;
    int          bEOF;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
    virtual int       Truncate( vsi_l_offset nNewSize );
};

/************************************************************************/
/*                      ErrnoFromGetLastError()                         */
/*                                                                      */
/* Private function translating Windows API error codes to POSIX errno. */
/*                                                                      */
/* TODO: If the function is going to be public CPL function, then       */
/* replace the switch with array of (Win32 code, errno code) tuples and */
/* complement it with missing codes.                                    */ 
/************************************************************************/

static int ErrnoFromGetLastError()
{
    int err = 0;
    DWORD dwError = GetLastError();
    
    switch( dwError )
    {
    case NO_ERROR:
        err = 0;
        break;
    case ERROR_FILE_NOT_FOUND:      /* Cannot find the file specified. */
    case ERROR_PATH_NOT_FOUND:      /* Cannot find the path specified. */
    case ERROR_INVALID_DRIVE:       /* Cannot find the drive specified. */
    case ERROR_NO_MORE_FILES:       /* There are no more files. */
    case ERROR_BAD_PATHNAME:        /* The specified path is invalid. */
    case ERROR_BAD_NETPATH:         /* The network path was not found. */
    case ERROR_FILENAME_EXCED_RANGE:/* The filename or extension is too long. */
        err = ENOENT;
        break;
    case ERROR_TOO_MANY_OPEN_FILES: /* The system cannot open the file. */
        err = EMFILE;
        break;
    case ERROR_ACCESS_DENIED:       /* Access denied. */
    case ERROR_CURRENT_DIRECTORY:   /* The directory cannot be removed. */
    case ERROR_WRITE_PROTECT:       /* The media is write protected. */
    case ERROR_LOCK_VIOLATION:      /* Another process has locked a portion of the file. */
    case ERROR_WRONG_DISK:          /* The wrong diskette is in the drive. */
    case ERROR_SHARING_BUFFER_EXCEEDED: /* Too many files opened for sharing. */
    case ERROR_DRIVE_LOCKED:        /* The disk is in use or locked by another process. */
    case ERROR_LOCK_FAILED:         /* Unable to lock a region of a file. */
    case ERROR_SEEK_ON_DEVICE:      /* The file pointer cannot be set on the specified device or file. */
        err = EACCES;
        break;
    case ERROR_INVALID_HANDLE:      /* The handle is invalid. */
    case ERROR_INVALID_TARGET_HANDLE: /* The target internal file identifier is incorrect. */
    case ERROR_DIRECT_ACCESS_HANDLE:  /* Operation other than raw disk I/O not permitted. */
        err = EBADF;
        break;
    case ERROR_ARENA_TRASHED:       /* The storage control blocks were destroyed. */
    case ERROR_NOT_ENOUGH_MEMORY:   /* Not enough storage is available. */
    case ERROR_INVALID_BLOCK:       /* The storage control block address is invalid. */
    case ERROR_NOT_ENOUGH_QUOTA:    /* Not enough quota is available to process this command. */
        err = ENOMEM;
        break;
    case ERROR_BAD_ENVIRONMENT:     /* The environment is incorrect. */
        err = E2BIG;
        break;
    case ERROR_INVALID_ACCESS:      /* The access code is invalid. */
    case ERROR_INVALID_DATA:        /* The data is invalid. */
        err = EINVAL;
        break;
    case ERROR_NOT_SAME_DEVICE:     /* The system cannot move the file to a different disk drive. */
        err = EXDEV;
        break;
    case ERROR_DIR_NOT_EMPTY:       /* The directory is not empty. */
        err = ENOTEMPTY;
        break;
    case ERROR_FILE_EXISTS:         /* The file exists. */
    case ERROR_ALREADY_EXISTS:      /* Cannot create a file when that file already exists. */
        err = EEXIST;
        break;
    case ERROR_DISK_FULL:           /* There is not enough space on the disk. */
        err = ENOSPC;
        break;
    case ERROR_HANDLE_EOF:          /* Reached the end of the file. */
        err = 0; /* There is no errno quivalent, in the errno.h */
        break;
    default:
        err = 0;
    }
    CPLAssert( 0 <= err );

    return err;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIWin32Handle::Close()

{
    return CloseHandle( hFile ) ? 0 : -1;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIWin32Handle::Seek( vsi_l_offset nOffset, int nWhence )

{
    GUInt32       dwMoveMethod, dwMoveHigh;
    GUInt32       nMoveLow;
    LARGE_INTEGER li;

    bEOF = FALSE;

    switch(nWhence)
    {
        case SEEK_CUR:
            dwMoveMethod = FILE_CURRENT;
            break;
        case SEEK_END:
            dwMoveMethod = FILE_END;
            break;
        case SEEK_SET:
        default:
            dwMoveMethod = FILE_BEGIN;
            break;
    }

    li.QuadPart = nOffset;
    nMoveLow = li.LowPart;
    dwMoveHigh = li.HighPart;

    SetLastError( 0 );
    SetFilePointer(hFile, (LONG) nMoveLow, (PLONG)&dwMoveHigh,
                       dwMoveMethod);

    if( GetLastError() != NO_ERROR )
    {
#ifdef notdef
        LPVOID      lpMsgBuf = NULL;
        
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER 
                       | FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), 
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       (LPTSTR) &lpMsgBuf, 0, NULL );
 
        printf( "[ERROR %d]\n %s\n", GetLastError(), (char *) lpMsgBuf );
        printf( "nOffset=%u, nMoveLow=%u, dwMoveHigh=%u\n", 
                (GUInt32) nOffset, nMoveLow, dwMoveHigh );
#endif
        errno = ErrnoFromGetLastError();
        return -1;
    }
    else
        return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIWin32Handle::Tell()

{
    LARGE_INTEGER   li;

    li.HighPart = 0;
    li.LowPart = SetFilePointer( hFile, 0, (PLONG) &(li.HighPart), 
                                 FILE_CURRENT );

    return (static_cast<vsi_l_offset>(li.QuadPart));
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIWin32Handle::Flush()

{
    FlushFileBuffers( hFile );
    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIWin32Handle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    DWORD       dwSizeRead;
    size_t      nResult;

    if( !ReadFile( hFile, pBuffer, (DWORD)(nSize*nCount), &dwSizeRead, NULL ) )
    {
        nResult = 0;
        errno = ErrnoFromGetLastError();
    }
    else if( nSize == 0 )
        nResult = 0;
    else
        nResult = dwSizeRead / nSize;

    if( nResult != nCount )
        bEOF = TRUE;

    return nResult;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIWin32Handle::Write( const void *pBuffer, size_t nSize, size_t nCount)

{
    DWORD       dwSizeWritten;
    size_t      nResult;

    if( !WriteFile(hFile, (void *)pBuffer,
                   (DWORD)(nSize*nCount),&dwSizeWritten,NULL) )
    {
        nResult = 0;
        errno = ErrnoFromGetLastError();
    }
    else if( nSize == 0)
        nResult = 0;
    else
        nResult = dwSizeWritten / nSize;

    return nResult;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIWin32Handle::Eof()

{
    return bEOF;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

int VSIWin32Handle::Truncate( vsi_l_offset nNewSize )
{
    vsi_l_offset nCur = Tell();
    Seek( nNewSize, SEEK_SET );
    BOOL bRes = SetEndOfFile( hFile );
    Seek( nCur, SEEK_SET );

    if (bRes)
        return 0;
    else
        return -1;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIWin32FilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *VSIWin32FilesystemHandler::Open( const char *pszFilename, 
                                                   const char *pszAccess )

{
    DWORD dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes;
    HANDLE hFile;

    if( strchr(pszAccess, '+') != NULL ||
        strchr(pszAccess, 'w') != 0 ||
        strchr(pszAccess, 'a') != 0 )
        dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
    else
        dwDesiredAccess = GENERIC_READ;

    if( strstr(pszAccess, "w") != NULL )
        dwCreationDisposition = CREATE_ALWAYS;
    else
        dwCreationDisposition = OPEN_EXISTING;

    dwFlagsAndAttributes = (dwDesiredAccess == GENERIC_READ) ? 
        FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL;
    
/* -------------------------------------------------------------------- */
/*      On Win32 consider treating the filename as utf-8 and            */
/*      converting to wide characters to open.                          */
/* -------------------------------------------------------------------- */
#ifndef WIN32CE
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename = 
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        hFile = CreateFileW( pwszFilename, dwDesiredAccess, 
                            FILE_SHARE_READ | FILE_SHARE_WRITE, 
                            NULL, dwCreationDisposition,  dwFlagsAndAttributes,
                            NULL );
        CPLFree( pwszFilename );
    }
    else
    {
        hFile = CreateFile( pszFilename, dwDesiredAccess, 
                            FILE_SHARE_READ | FILE_SHARE_WRITE, 
                            NULL, dwCreationDisposition,  dwFlagsAndAttributes,
                            NULL );
    }

/* -------------------------------------------------------------------- */
/*      On WinCE we only support plain ascii filenames.                 */
/* -------------------------------------------------------------------- */
#else /* def WIN32CE */
    hFile = CE_CreateFileA( pszFilename, dwDesiredAccess, 
                        FILE_SHARE_READ | FILE_SHARE_WRITE, 
                        NULL, dwCreationDisposition,  dwFlagsAndAttributes, NULL );
#endif

    if( hFile == INVALID_HANDLE_VALUE )
    {
        errno = ErrnoFromGetLastError();
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a VSI file handle.                                       */
/* -------------------------------------------------------------------- */
    VSIWin32Handle *poHandle = new VSIWin32Handle;
    
    poHandle->hFile = hFile;
    poHandle->bEOF = FALSE;
    
    if (strchr(pszAccess, 'a') != 0)
        poHandle->Seek(0, SEEK_END);
    
/* -------------------------------------------------------------------- */
/*      If VSI_CACHE is set we want to use a cached reader instead      */
/*      of more direct io on the underlying file.                       */
/* -------------------------------------------------------------------- */
    if( (EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb"))
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

int VSIWin32FilesystemHandler::Stat( const char * pszFilename, 
                                     VSIStatBufL * pStatBuf,
                                     int nFlags )

{
    (void) nFlags;

#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        int nResult;
        wchar_t *pwszFilename = 
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        nResult = _wstat64( pwszFilename, pStatBuf );
        CPLFree( pwszFilename );

        return nResult;
    }
    else
#endif
    {
        return( VSI_STAT64( pszFilename, pStatBuf ) );
    }
}

/************************************************************************/

/*                               Unlink()                               */
/************************************************************************/

int VSIWin32FilesystemHandler::Unlink( const char * pszFilename )

{
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        int nResult;
        wchar_t *pwszFilename = 
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        nResult = _wunlink( pwszFilename );
        CPLFree( pwszFilename );
        return nResult;
    }
    else
#endif
    {
        return unlink( pszFilename );
    }
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIWin32FilesystemHandler::Rename( const char *oldpath,
                                           const char *newpath )

{
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        int nResult;
        wchar_t *pwszOldPath = 
            CPLRecodeToWChar( oldpath, CPL_ENC_UTF8, CPL_ENC_UCS2 );
        wchar_t *pwszNewPath = 
            CPLRecodeToWChar( newpath, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        nResult = _wrename( pwszOldPath, pwszNewPath );
        CPLFree( pwszOldPath );
        CPLFree( pwszNewPath );
        return nResult;
    }
    else
#endif
    {
        return rename( oldpath, newpath );
    }
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIWin32FilesystemHandler::Mkdir( const char * pszPathname,
                                      long nMode )

{
    (void) nMode;
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        int nResult;
        wchar_t *pwszFilename = 
            CPLRecodeToWChar( pszPathname, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        nResult = _wmkdir( pwszFilename );
        CPLFree( pwszFilename );
        return nResult;
    }
    else
#endif
    {
        return mkdir( pszPathname );
    }
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIWin32FilesystemHandler::Rmdir( const char * pszPathname )

{
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        int nResult;
        wchar_t *pwszFilename = 
            CPLRecodeToWChar( pszPathname, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        nResult = _wrmdir( pwszFilename );
        CPLFree( pwszFilename );
        return nResult;
    }
    else
#endif
    {
        return rmdir( pszPathname );
    }
}

/************************************************************************/
/*                              ReadDir()                               */
/************************************************************************/

char **VSIWin32FilesystemHandler::ReadDir( const char *pszPath )

{
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        struct _wfinddata_t c_file;
        intptr_t hFile;
        char    *pszFileSpec;
        CPLStringList oDir;
        
        if (strlen(pszPath) == 0)
            pszPath = ".";
        
        pszFileSpec = CPLStrdup(CPLSPrintf("%s\\*.*", pszPath));
        wchar_t *pwszFileSpec = 
            CPLRecodeToWChar( pszFileSpec, CPL_ENC_UTF8, CPL_ENC_UCS2 );
        
        if ( (hFile = _wfindfirst( pwszFileSpec, &c_file )) != -1L )
        {
            do
            {
                oDir.AddStringDirectly(
                    CPLRecodeFromWChar(c_file.name,CPL_ENC_UCS2,CPL_ENC_UTF8));
            } while( _wfindnext( hFile, &c_file ) == 0 );
            
            _findclose( hFile );
        }
        else
        {
            /* Should we generate an error???  
             * For now we'll just return NULL (at the end of the function)
             */
        }

        CPLFree(pszFileSpec);
        CPLFree(pwszFileSpec);

        return oDir.StealList();
    }
    else
#endif
    {
        struct _finddata_t c_file;
        intptr_t hFile;
        char    *pszFileSpec;
        CPLStringList oDir;
        
        if (strlen(pszPath) == 0)
            pszPath = ".";
        
        pszFileSpec = CPLStrdup(CPLSPrintf("%s\\*.*", pszPath));
        
        if ( (hFile = _findfirst( pszFileSpec, &c_file )) != -1L )
        {
            do
            {
                oDir.AddString(c_file.name);
            } while( _findnext( hFile, &c_file ) == 0 );
            
            _findclose( hFile );
        }
        else
        {
            /* Should we generate an error???  
             * For now we'll just return NULL (at the end of the function)
             */
        }

        CPLFree(pszFileSpec);
        
        return oDir.StealList();
    }
}

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

void VSIInstallLargeFileHandler()

{
    VSIFileManager::InstallHandler( "", new VSIWin32FilesystemHandler );
}

#endif /* def WIN32 */


