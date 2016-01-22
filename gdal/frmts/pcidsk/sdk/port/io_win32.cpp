/******************************************************************************
 *
 * Purpose:  Implementation of IO interface using Win32 API.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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

#include "pcidsk_io.h"
#include "pcidsk_exception.h"
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>

using namespace PCIDSK;

class Win32IOInterface : public IOInterfaces
{
    virtual void   *Open( std::string filename, std::string access ) const;
    virtual uint64  Seek( void *io_handle, uint64 offset, int whence ) const;
    virtual uint64  Tell( void *io_handle ) const;
    virtual uint64  Read( void *buffer, uint64 size, uint64 nmemb, void *io_handle ) const;
    virtual uint64  Write( const void *buffer, uint64 size, uint64 nmemb, void *io_handle ) const;
    virtual int     Eof( void *io_handle ) const;
    virtual int     Flush( void *io_handle ) const;
    virtual int     Close( void *io_handle ) const;

    const char     *LastError() const;
};

typedef struct {
    HANDLE hFile;
    uint64 offset;
} FileInfo;

/************************************************************************/
/*                       GetDefaultIOInterfaces()                       */
/************************************************************************/

const IOInterfaces *PCIDSK::GetDefaultIOInterfaces()
{
    static Win32IOInterface singleton_win32_interface;

    return &singleton_win32_interface;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

void *
Win32IOInterface::Open( std::string filename, std::string access ) const

{
    DWORD dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes;
    HANDLE hFile;

    if( strchr(access.c_str(),'+') != NULL || strchr(access.c_str(),'w') != 0 )
        dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
    else
        dwDesiredAccess = GENERIC_READ;

    if( strstr(access.c_str(), "w") != NULL )
        dwCreationDisposition = CREATE_ALWAYS;
    else
        dwCreationDisposition = OPEN_EXISTING;

    dwFlagsAndAttributes = (dwDesiredAccess == GENERIC_READ) ? 
                FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL, 
    
    hFile = CreateFileA(filename.c_str(), dwDesiredAccess, 
                        FILE_SHARE_READ | FILE_SHARE_WRITE, 
                        NULL, dwCreationDisposition,  dwFlagsAndAttributes, NULL);

    if( hFile == INVALID_HANDLE_VALUE )
    {
        ThrowPCIDSKException( "Open(%s,%s) failed:\n%s", 
                              filename.c_str(), access.c_str(), LastError() );
    }
    
    FileInfo *fi = new FileInfo();
    fi->hFile = hFile;
    fi->offset = 0;

    return fi;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

uint64 
Win32IOInterface::Seek( void *io_handle, uint64 offset, int whence ) const

{
    FileInfo *fi = (FileInfo *) io_handle;
    uint32       dwMoveMethod, dwMoveHigh;
    uint32       nMoveLow;
    LARGE_INTEGER li;

    // seeks that do nothing are still surprisingly expensive with MSVCRT.
    // try and short circuit if possible.
    if( whence == SEEK_SET && offset == fi->offset )
        return 0;

    switch(whence)
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

    li.QuadPart = offset;
    nMoveLow = li.LowPart;
    dwMoveHigh = li.HighPart;

    SetLastError( 0 );
    SetFilePointer(fi->hFile, (LONG) nMoveLow, (PLONG)&dwMoveHigh,
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
        ThrowPCIDSKException( "Seek(%d,%d): %s (%d)", 
                              (int) offset, whence, 
                              LastError(), GetLastError() );
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Update our offset.                                              */
/* -------------------------------------------------------------------- */
    if( whence == SEEK_SET )
        fi->offset = offset;
    else if( whence == SEEK_END )
    {
        LARGE_INTEGER   li;

        li.HighPart = 0;
        li.LowPart = SetFilePointer( fi->hFile, 0, (PLONG) &(li.HighPart), 
                                     FILE_CURRENT );
        fi->offset = li.QuadPart;
    }
    else if( whence == SEEK_CUR )
        fi->offset += offset;

    return 0;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

uint64 Win32IOInterface::Tell( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    return fi->offset;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

uint64 Win32IOInterface::Read( void *buffer, uint64 size, uint64 nmemb, 
                               void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    errno = 0;

    DWORD       dwSizeRead;
    size_t      result;

    if( !ReadFile(fi->hFile, buffer, (DWORD)(size*nmemb), &dwSizeRead, NULL) )
    {
        result = 0;
    }
    else if( size == 0 )
        result = 0;
    else
        result = (size_t) (dwSizeRead / size);

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Read(%d): %s", 
                              (int) size * nmemb,
                              LastError() );

    fi->offset += size*result;

    return result;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

uint64 Win32IOInterface::Write( const void *buffer, uint64 size, uint64 nmemb, 
                                void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    errno = 0;

    DWORD       dwSizeRead;
    size_t      result;

    if( !WriteFile(fi->hFile, buffer, (DWORD)(size*nmemb), &dwSizeRead, NULL) )
    {
        result = 0;
    }
    else if( size == 0 )
        result = 0;
    else
        result = (size_t) (dwSizeRead / size);

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Write(%d): %s", 
                                   (int) size * nmemb,
                                   LastError() );

    fi->offset += size*result;

    return result;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int Win32IOInterface::Eof( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;
    uint64       nCur, nEnd;

    nCur = Tell( io_handle );
    Seek( io_handle, 0, SEEK_END );
    nEnd = Tell( io_handle );
    Seek( io_handle, nCur, SEEK_SET );

    return (nCur == nEnd);
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int Win32IOInterface::Flush( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    FlushFileBuffers( fi->hFile );

    return 0;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int Win32IOInterface::Close( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    int result = CloseHandle( fi->hFile ) ? 0 : -1;
    delete fi;

    return result;
}

/************************************************************************/
/*                             LastError()                              */
/*                                                                      */
/*      Return a string representation of the last error.               */
/************************************************************************/

const char *Win32IOInterface::LastError() const

{
    return "";
}
