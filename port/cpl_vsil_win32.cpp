/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Win32.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 **********************************************************************
 *
 * $Log$
 * Revision 1.5  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.4  2001/06/21 20:40:31  warmerda
 * *** empty log message ***
 *
 * Revision 1.3  2001/06/11 13:47:07  warmerda
 * initialize HighPart in VSIFTellL()
 *
 * Revision 1.2  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.1  2001/01/03 16:16:59  warmerda
 * New
 *
 */

#include "cpl_vsi.h"

#if defined(WIN32)

CPL_CVSID("$Id$");

#include <windows.h>

/************************************************************************/
/*                              VSIFOpen()                              */
/************************************************************************/

FILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    DWORD dwDesiredAccess, dwCreationDisposition;
    HANDLE hFile;

    if( strchr(pszAccess, '+') != NULL || strchr(pszAccess, 'w') != 0 )
        dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
    else
        dwDesiredAccess = GENERIC_READ;

    if( strstr(pszAccess, "w") != NULL )
        dwCreationDisposition = CREATE_ALWAYS;
    else
        dwCreationDisposition = OPEN_EXISTING;
        
    hFile = CreateFile( pszFilename, dwDesiredAccess, 
                        FILE_SHARE_READ | FILE_SHARE_WRITE, 
                        NULL, dwCreationDisposition, 
                        (dwDesiredAccess == GENERIC_READ) ? 
                        FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL, 
                        NULL );
    
    if( hFile == INVALID_HANDLE_VALUE )
    {
        return NULL;
    }
    else
    {
        return (FILE *) hFile;
    }
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

int VSIFCloseL( FILE * fp )

{
    HANDLE hFile = (HANDLE) fp;

    return CloseHandle( hFile ) ? 0 : -1;
}

/************************************************************************/
/*                             VSIFSeekL()                              */
/************************************************************************/

int VSIFSeekL( FILE * fp, vsi_l_offset nOffset, int nWhence )

{
    HANDLE hFile = (HANDLE) fp;
    GUInt32       dwMoveMethod, dwMoveHigh;
    GUInt32       nMoveLow;
    LARGE_INTEGER li;

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
        
        return -1;
    }
    else
        return 0;
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

vsi_l_offset VSIFTellL( FILE * fp )

{
    HANDLE hFile = (HANDLE) fp;
    DWORD  dwMoveHigh=0, dwMoveLow;
    LARGE_INTEGER   li;

    li.HighPart = 0;
    li.LowPart = SetFilePointer( hFile, 0, (PLONG) &(li.HighPart), 
                                 FILE_CURRENT );

    return li.QuadPart;
}

/************************************************************************/
/*                             VSIRewindL()                             */
/************************************************************************/

void VSIRewindL( FILE * fp )

{
    VSIFSeekL( fp, 0, SEEK_SET );
}

/************************************************************************/
/*                             VSIFFlushL()                             */
/************************************************************************/

void VSIFFlushL( FILE * fp )

{
    HANDLE hFile = (HANDLE) fp;

    FlushFileBuffers( hFile );
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    HANDLE      hFile = (HANDLE) fp;
    DWORD       dwSizeRead;

    if( !ReadFile( hFile, pBuffer, nSize * nCount, &dwSizeRead, NULL ) )
        return 0;
    else
        return dwSizeRead / nSize;
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

size_t VSIFWriteL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    HANDLE      hFile = (HANDLE) fp;
    DWORD       dwSizeWritten;

    if( !WriteFile( hFile, pBuffer, nSize * nCount, &dwSizeWritten, NULL ) )
        return 0;
    else
        return dwSizeWritten / nSize;
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

int VSIFEofL( FILE * fp )

{
    vsi_l_offset       nCur, nEnd;

    nCur = VSIFTell( fp );
    VSIFSeekL( fp, 0, SEEK_END );
    nEnd = VSIFTell( fp );
    VSIFSeekL( fp, nCur, SEEK_SET );

    return (nCur == nEnd);
}

#endif /* defined WIN32 */

