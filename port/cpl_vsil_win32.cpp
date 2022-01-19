/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Win32.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

#if defined(WIN32)

#include <windows.h>
#include <winioctl.h> // for FSCTL_SET_SPARSE

#include "cpl_string.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>

/************************************************************************/
/* ==================================================================== */
/*                       VSIWin32FilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

// To avoid aliasing to GetDiskFreeSpace to GetDiskFreeSpaceA on Windows
#ifdef GetDiskFreeSpace
#undef GetDiskFreeSpace
#endif

class VSIWin32FilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIWin32FilesystemHandler)

public:
    // TODO(schwehr): Fix Open call to remove the need for this using call.
    using VSIFilesystemHandler::Open;

    VSIWin32FilesystemHandler() = default;

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError,
                                    CSLConstList /* papszOptions */ ) override;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags ) override;
    virtual int      Unlink( const char *pszFilename ) override;
    virtual int      Rename( const char *oldpath, const char *newpath ) override;
    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual int      Rmdir( const char *pszDirname ) override;
    virtual char   **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;
    virtual int      IsCaseSensitive( const char* pszFilename ) override
                      { (void) pszFilename; return FALSE; }
    virtual GIntBig  GetDiskFreeSpace( const char* pszDirname ) override;
    virtual int SupportsSparseFiles( const char* pszPath ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                            VSIWin32Handle                            */
/* ==================================================================== */
/************************************************************************/

class VSIWin32Handle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIWin32Handle)

  public:
    HANDLE       hFile = nullptr;
    bool         bEOF = false;

    VSIWin32Handle() = default;

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    virtual int       Eof() override;
    virtual int       Flush() override;
    virtual int       Close() override;
    virtual int       Truncate( vsi_l_offset nNewSize ) override;
    virtual void     *GetNativeFileDescriptor() override { return static_cast<void*>(hFile); }
    virtual VSIRangeStatus GetRangeStatus( vsi_l_offset nOffset,
                                           vsi_l_offset nLength ) override;
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

static int ErrnoFromGetLastError(DWORD dwError = 0)
{
    int err = 0;
    if( dwError == 0 )
        dwError = GetLastError();

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
    case ERROR_SHARING_VIOLATION:   /* The process cannot access the file because it is being used by another process. */
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
        err = 0; /* There is no errno equivalent in the errno.h */
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
    LONG       dwMoveMethod, dwMoveHigh;
    GUInt32       nMoveLow;
    LARGE_INTEGER li;

    bEOF = false;

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
    SetFilePointer(hFile, nMoveLow, &dwMoveHigh,
                       dwMoveMethod);

    if( GetLastError() != NO_ERROR )
    {
#ifdef notdef
        LPVOID      lpMsgBuf = nullptr;

        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER
                       | FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPTSTR) &lpMsgBuf, 0, nullptr );

        printf( "[ERROR %d]\n %s\n", GetLastError(), (char *) lpMsgBuf );/*ok*/
        printf( "nOffset=%u, nMoveLow=%u, dwMoveHigh=%u\n",/*ok*/
                static_cast<GUInt32>(nOffset), nMoveLow, dwMoveHigh );
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
    li.LowPart = SetFilePointer( hFile, 0, &(li.HighPart),
                                 FILE_CURRENT );

    return (static_cast<vsi_l_offset>(li.QuadPart));
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIWin32Handle::Flush()

{
    /* Nothing needed to offer the same guarantee as POSIX fflush() */
    /* FlushFileBuffers() would be closer to fsync() */
    /* See http://trac.osgeo.org/gdal/ticket/5556 */

    // Add this as a hack to make ogr_mitab_30 and _31 tests pass
    if( CPLTestBool(CPLGetConfigOption("VSI_FLUSH", "FALSE")) )
        FlushFileBuffers(hFile);
    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIWin32Handle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    DWORD dwSizeRead = 0;
    size_t nResult = 0;

    if( !ReadFile( hFile, pBuffer, static_cast<DWORD>(nSize*nCount), &dwSizeRead, nullptr ) )
    {
        nResult = 0;
        errno = ErrnoFromGetLastError();
    }
    else if( nSize == 0 )
        nResult = 0;
    else
        nResult = dwSizeRead / nSize;

    if( nResult != nCount )
        bEOF = true;

    return nResult;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIWin32Handle::Write( const void *pBuffer, size_t nSize, size_t nCount)

{
    DWORD dwSizeWritten = 0;
    size_t nResult = 0;

    if( !WriteFile(hFile, pBuffer,
                   static_cast<DWORD>(nSize*nCount),&dwSizeWritten,nullptr) )
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
    Seek( 0, SEEK_END );
    if( nNewSize > Tell() )
    {
        // Enable sparse files if growing size
        DWORD dwTemp;
        DeviceIoControl(hFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &dwTemp, nullptr);
    }
    Seek( nNewSize, SEEK_SET );
    BOOL bRes = SetEndOfFile( hFile );
    Seek( nCur, SEEK_SET );

    if (bRes)
        return 0;
    else
        return -1;
}

/************************************************************************/
/*                           GetRangeStatus()                           */
/************************************************************************/

VSIRangeStatus VSIWin32Handle::GetRangeStatus( vsi_l_offset
#ifdef FSCTL_QUERY_ALLOCATED_RANGES
                                                        nOffset
#endif
                                               ,vsi_l_offset
#ifdef FSCTL_QUERY_ALLOCATED_RANGES
                                                        nLength
#endif
                                              )
{
    // Not available on mingw includes
#ifdef FSCTL_QUERY_ALLOCATED_RANGES
    FILE_ALLOCATED_RANGE_BUFFER sQueryRange;
    FILE_ALLOCATED_RANGE_BUFFER asOutputRange[1];
    DWORD nOutputBytes = 0;

    sQueryRange.FileOffset.QuadPart = nOffset;
    sQueryRange.Length.QuadPart = nOffset + nLength;

    if( !DeviceIoControl(hFile, FSCTL_QUERY_ALLOCATED_RANGES,
                        &sQueryRange, sizeof(sQueryRange),
                        asOutputRange, sizeof(asOutputRange),
                        &nOutputBytes, nullptr) )
    {
        if( GetLastError() == ERROR_MORE_DATA )
        {
            return VSI_RANGE_STATUS_DATA;
        }
        else
        {
            return VSI_RANGE_STATUS_UNKNOWN;
        }
    }

    if( nOutputBytes )
        return VSI_RANGE_STATUS_DATA;
    else
        return VSI_RANGE_STATUS_HOLE;
#else
    return VSI_RANGE_STATUS_UNKNOWN;
#endif
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIWin32FilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          CPLGetWineVersion()                         */
/************************************************************************/

const char* CPLGetWineVersion(); // also used by cpl_aws.cpp

const char* CPLGetWineVersion()
{
    HMODULE hntdll = GetModuleHandle("ntdll.dll");
    if( hntdll == nullptr )
    {
        CPLDebug("CPLGetWineVersion", "Can't get handle to ntdll.dll.");
        return nullptr;
    }

    const char * (CDECL *pwine_get_version)(void);
    const auto ret = GetProcAddress(hntdll, "wine_get_version");
    static_assert(sizeof(pwine_get_version) == sizeof(ret),
                  "sizeof(pwine_get_version) == sizeof(ret)");
    memcpy(&pwine_get_version, &ret, sizeof(ret));
    if( pwine_get_version == nullptr )
    {
        return nullptr;
    }

    return pwine_get_version();
}

/************************************************************************/
/*                           VSIWin32StrlenW()                          */
/************************************************************************/

static size_t VSIWin32StrlenW(const wchar_t* pwszString)
{
    size_t nLen = 0;
    while( pwszString[nLen] != 0 )
        nLen ++;
    return nLen;
}

/************************************************************************/
/*                        VSIWin32TryLongFilename()                     */
/************************************************************************/

static void VSIWin32TryLongFilename(wchar_t*& pwszFilename)
{
    size_t nLen = VSIWin32StrlenW(pwszFilename);
    if( pwszFilename[0] != 0 &&
        pwszFilename[1] == ':' &&
        (pwszFilename[2] == '\\' || pwszFilename[2] == '/' ) )
    {
        pwszFilename = static_cast<wchar_t *>(
            CPLRealloc( pwszFilename, (4 + nLen + 1) * sizeof(wchar_t)));
        memmove( pwszFilename + 4, pwszFilename, (nLen+1) * sizeof(wchar_t));
    }
    else
    {
        // TODO(schwehr): 32768 should be a symbolic constant.
        wchar_t* pwszCurDir =
            static_cast<wchar_t *>(CPLMalloc(32768 * sizeof(wchar_t)));
        DWORD nCurDirLen = GetCurrentDirectoryW( 32768, pwszCurDir );
        CPLAssert(nCurDirLen < 32768);
        pwszFilename = static_cast<wchar_t *>(
            CPLRealloc(pwszFilename,
                       (4 + nCurDirLen + 1 + nLen + 1) * sizeof(wchar_t)));
        int nOffset = 0;
        if( pwszFilename[0] == '.' &&
            (pwszFilename[1] == '/' || pwszFilename[1] == '\\') )
            nOffset = 2;
        /* \\$\c:\a\b ..\c --> \\$\c:\a\c */
        while( pwszFilename[nOffset + 0] == '.' && pwszFilename[nOffset + 1] == '.' &&
              (pwszFilename[nOffset + 2] == '/' || pwszFilename[nOffset + 2] == '\\') )
        {
            DWORD nCurDirLenBefore = nCurDirLen;
            while( nCurDirLen > 0 && pwszCurDir[nCurDirLen-1] != '\\' )
                nCurDirLen --;
            if( nCurDirLen <= 2 )
            {
                nCurDirLen = nCurDirLenBefore;
                break;
            }
            nCurDirLen --;
            nOffset += 3;
        }
        memmove( pwszFilename + 4 + nCurDirLen + 1,
                 pwszFilename + nOffset, (nLen-nOffset+1) * sizeof(wchar_t) );
        memmove( pwszFilename + 4, pwszCurDir, nCurDirLen * sizeof(wchar_t) );
        pwszFilename[ 4 + nCurDirLen ] = '\\';
        CPLFree(pwszCurDir);
    }
    pwszFilename[0] = '\\';
    pwszFilename[1] = '\\';
    pwszFilename[2] = '?';
    pwszFilename[3] = '\\';

    for(size_t i = 4; pwszFilename[i] != 0; i++)
    {
        if( pwszFilename[i] == '/' )
            pwszFilename[i] = '\\';
    }
#if notdef
    CPLString osFilename;
    for(size_t i = 0; pwszFilename[i] != 0; i++)
        osFilename += (char)pwszFilename[i];
    CPLDebug("VSI", "Trying %s", osFilename.c_str());
#endif
}

/************************************************************************/
/*                         VSIWin32IsLongFilename()                     */
/************************************************************************/

static bool VSIWin32IsLongFilename( const wchar_t* pwszFilename )
{
    return (pwszFilename[0] == '\\' && pwszFilename[1] == '\\' &&
            pwszFilename[2] == '?' && pwszFilename[3] == '\\');
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *VSIWin32FilesystemHandler::Open( const char *pszFilename,
                                                   const char *pszAccess,
                                                   bool bSetError,
                                                   CSLConstList /* papszOptions */ )

{
    DWORD dwDesiredAccess;
    DWORD dwCreationDisposition;
    DWORD dwFlagsAndAttributes;
    HANDLE hFile;

    // GENERICs are used instead of FILE_GENERIC_READ.
    dwDesiredAccess = GENERIC_READ;
    if (strchr(pszAccess, '+') != nullptr || strchr(pszAccess, 'w') != nullptr)
        dwDesiredAccess |= GENERIC_WRITE;

    // Append mode only makes sense on files and pipes, have to use FILE_ access
    // these are very different from the GENERICs
    // Append is read and write but not overwrite data (only append data)
    if (strchr(pszAccess, 'a') != nullptr )
    {
        dwDesiredAccess =
            FILE_GENERIC_READ | (FILE_GENERIC_WRITE ^ FILE_WRITE_DATA);

        // Wine < 1.7.4 doesn't work properly without FILE_WRITE_DATA bit
        // (it refuses to write at all), so we'd better re-add it even if the
        // resulting semantics isn't completely conformant.
        // See https://bugs.winehq.org/show_bug.cgi?id=33232
        const char* pszWineVersion = CPLGetWineVersion();
        if( pszWineVersion != nullptr )
        {
            int nVersion = atoi(pszWineVersion) * 10000;
            const char* pszDot = strchr(pszWineVersion, '.');
            if( pszDot )
            {
                nVersion += atoi(pszDot + 1) * 100;
                pszDot = strchr(pszDot + 1, '.');
                if( pszDot )
                {
                    nVersion += atoi(pszDot + 1);
                }
            }
            if( nVersion < 1 * 10000 + 7 * 100 + 4 )
            {
#if DEBUG_VERBOSE
                CPLDebug("VSI",
                         "Wine %s detected. Append mode needs FILE_WRITE_DATA",
                         pszWineVersion);
#endif
                dwDesiredAccess |= FILE_WRITE_DATA;
            }
        }
    }

    if( strstr(pszAccess, "w") != nullptr )
        dwCreationDisposition = CREATE_ALWAYS;
    else if( strstr(pszAccess, "a") != nullptr )
        dwCreationDisposition = OPEN_ALWAYS;
    else
        dwCreationDisposition = OPEN_EXISTING;

    dwFlagsAndAttributes = (dwDesiredAccess == GENERIC_READ) ?
        FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL;

/* -------------------------------------------------------------------- */
/*      On Win32 consider treating the filename as utf-8 and            */
/*      converting to wide characters to open.                          */
/* -------------------------------------------------------------------- */
    DWORD nLastError = 0;
    bool bShared = CPLTestBool(CPLGetConfigOption( "GDAL_SHARED_FILE", "YES" ) );
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        hFile = CreateFileW( pwszFilename, dwDesiredAccess,
                            bShared ? FILE_SHARE_READ | FILE_SHARE_WRITE : 0,
                            nullptr, dwCreationDisposition,  dwFlagsAndAttributes,
                            nullptr );
        if ( hFile == INVALID_HANDLE_VALUE &&
            !VSIWin32IsLongFilename(pwszFilename) )
        {
            nLastError = GetLastError();
#ifdef notdef
            switch( nLastError )
            {
                    case ERROR_FILE_NOT_FOUND:      CPLDebug("VSI", "ERROR_FILE_NOT_FOUND"); break;
                    case ERROR_PATH_NOT_FOUND:      CPLDebug("VSI", "ERROR_PATH_NOT_FOUND"); break;
                    case ERROR_INVALID_DRIVE:       CPLDebug("VSI", "ERROR_INVALID_DRIVE"); break;
                    case ERROR_NO_MORE_FILES:       CPLDebug("VSI", "ERROR_NO_MORE_FILES"); break;
                    case ERROR_BAD_PATHNAME:        CPLDebug("VSI", "ERROR_BAD_PATHNAME"); break;
                    case ERROR_BAD_NETPATH:         CPLDebug("VSI", "ERROR_BAD_NETPATH"); break;
                    case ERROR_FILENAME_EXCED_RANGE: CPLDebug("VSI", "ERROR_FILENAME_EXCED_RANGE"); break;
                    case ERROR_SHARING_VIOLATION:   CPLDebug("VSI", "ERROR_SHARING_VIOLATION"); break;
                    default:  CPLDebug("VSI", "other error %d", nLastError); break;
            }
#endif
        }
        if( nLastError == ERROR_PATH_NOT_FOUND ||
            nLastError == ERROR_FILENAME_EXCED_RANGE )
        {
            VSIWin32TryLongFilename(pwszFilename);
            nLastError = 0;
            hFile = CreateFileW( pwszFilename, dwDesiredAccess,
                            bShared ? FILE_SHARE_READ | FILE_SHARE_WRITE : 0,
                            nullptr, dwCreationDisposition,  dwFlagsAndAttributes,
                            nullptr );
        }
        CPLFree( pwszFilename );
    }
    else
    {
        hFile = CreateFile( pszFilename, dwDesiredAccess,
                            bShared ? FILE_SHARE_READ | FILE_SHARE_WRITE : 0,
                            nullptr, dwCreationDisposition,  dwFlagsAndAttributes,
                            nullptr );
    }

    if( hFile == INVALID_HANDLE_VALUE )
    {
        nLastError = GetLastError();
        const int nError = ErrnoFromGetLastError(nLastError);
        if( bSetError && nError != 0 )
        {
            VSIError(VSIE_FileError, "%s: %s", pszFilename,
                     (nLastError == ERROR_SHARING_VIOLATION) ?
                        "file used by other process": strerror(nError));
        }
        errno = nError;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a VSI file handle.                                       */
/* -------------------------------------------------------------------- */
    VSIWin32Handle *poHandle = new VSIWin32Handle;

    poHandle->hFile = hFile;

    if (strchr(pszAccess, 'a') != nullptr)
        poHandle->Seek(0, SEEK_END);

/* -------------------------------------------------------------------- */
/*      If VSI_CACHE is set we want to use a cached reader instead      */
/*      of more direct io on the underlying file.                       */
/* -------------------------------------------------------------------- */
    if( (EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb"))
        && CPLTestBool( CPLGetConfigOption( "VSI_CACHE", "FALSE" ) ) )
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

#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        bool bTryOtherStatImpl = false;
        int nResult = 0;
        if( VSIWin32IsLongFilename(pwszFilename) )
        {
            bTryOtherStatImpl = true;
            nResult = -1;
        }
        else
        {
            nResult = _wstat64( pwszFilename, pStatBuf );
            if( nResult < 0 )
            {
                DWORD nLastError = GetLastError();
                if( nLastError == ERROR_PATH_NOT_FOUND ||
                    nLastError == ERROR_FILENAME_EXCED_RANGE )
                {
                    VSIWin32TryLongFilename(pwszFilename);
                    bTryOtherStatImpl = true;
                }
            }
        }

        if( bTryOtherStatImpl )
        {
            // _wstat64 doesn't like \\?\ paths, so do our poor-man
            // stat like.
            // nResult = _wstat64( pwszFilename, pStatBuf );

            VSIVirtualHandle* poHandle = Open( pszFilename, "rb");
            if( poHandle != nullptr )
            {
                nResult = 0;
                memset( pStatBuf, 0, sizeof(VSIStatBufL) );
                CPL_IGNORE_RET_VAL(poHandle->Seek(0, SEEK_END));
                pStatBuf->st_mode = S_IFREG;
                pStatBuf->st_size = poHandle->Tell();
                poHandle->Close();
                delete poHandle;
            }
            else
                nResult = -1;
        }

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
#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        const int nResult = _wunlink( pwszFilename );
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
#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszOldPath =
            CPLRecodeToWChar( oldpath, CPL_ENC_UTF8, CPL_ENC_UCS2 );
        wchar_t *pwszNewPath =
            CPLRecodeToWChar( newpath, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        const int nResult = _wrename( pwszOldPath, pwszNewPath );
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
#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszPathname, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        const int nResult = _wmkdir( pwszFilename );
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
#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszPathname, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        const int nResult = _wrmdir( pwszFilename );
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

char **VSIWin32FilesystemHandler::ReadDirEx( const char *pszPath,
                                             int nMaxFiles )

{
#if defined(_MSC_VER) || __MSVCRT_VERSION__ >= 0x0601
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
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
                if( nMaxFiles > 0 && oDir.Count() > nMaxFiles )
                    break;
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
                if( nMaxFiles > 0 && oDir.Count() > nMaxFiles )
                    break;
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
/*                        GetDiskFreeSpace()                            */
/************************************************************************/

GIntBig VSIWin32FilesystemHandler::GetDiskFreeSpace( const char* pszDirname )
{
    GIntBig nRet = -1;
    ULARGE_INTEGER nFreeBytesAvailable;
    if( GetDiskFreeSpaceEx(pszDirname, &nFreeBytesAvailable, nullptr, nullptr) )
    {
        nRet = static_cast<GIntBig>(nFreeBytesAvailable.QuadPart);
    }
    return nRet;
}

/************************************************************************/
/*                      SupportsSparseFiles()                           */
/************************************************************************/

int VSIWin32FilesystemHandler::SupportsSparseFiles( const char* pszPath )
{
    CPLString osPath(pszPath);
    DWORD dwVolFlags = 0;
    if( CPLIsFilenameRelative(pszPath) )
    {
        char* pszTmp = CPLGetCurrentDir();
        osPath = pszTmp;
        CPLFree(pszTmp);
    }
    if( osPath.size() >= 3 && osPath[1] == ':' &&
        (osPath[2] == '/' || osPath[2] == '\\') )
    {
        osPath.resize(3);
    }

    GetVolumeInformation(osPath.c_str(), nullptr, 0, nullptr,
                         nullptr, &dwVolFlags, nullptr, 0);
    return (dwVolFlags & FILE_SUPPORTS_SPARSE_FILES);
}

/************************************************************************/
/*                     VSIInstallLargeFileHandler()                     */
/************************************************************************/

void VSIInstallLargeFileHandler()

{
    VSIFileManager::InstallHandler( "", new VSIWin32FilesystemHandler );
}

#endif /* def WIN32 */
