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
 * Revision 1.10  2004/03/10 18:17:47  warmerda
 * Hopefully corrected type casting warnings without breaking anything.
 *
 * Revision 1.9  2003/05/27 20:45:33  warmerda
 * added VSI IO debugging stuff
 *
 * Revision 1.8  2002/06/17 14:10:14  warmerda
 * no stat64 on Win32
 *
 * Revision 1.7  2002/06/17 14:00:16  warmerda
 * segregate VSIStatL() and VSIStatBufL.
 *
 * Revision 1.6  2002/06/12 02:11:58  warmerda
 * Removed unused variables.
 *
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

typedef struct {
    HANDLE       hFile;
    vsi_l_offset nLastOffset;
} VSIWin32File;

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

    VSIDebug3( "VSIFOpenL(%s,%s) = %p", pszFilename, pszAccess, hFile );
    
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

    VSIDebug1( "VSIFCloseL(%p)", fp );

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

#ifdef VSI_DEBUG
    if( nWhence == SEEK_SET )
    {
        VSIDebug3( "VSIFSeekL(%p,%d:%lu,SEEK_SET)", 
                   fp, (int) dwMoveHigh, (unsigned long) nMoveLow );
    }
    else if( nWhence == SEEK_END )
    {
        VSIDebug3( "VSIFSeekL(%p,%d:%lu,SEEK_END)",
                   fp, (int) dwMoveHigh, (unsigned long) nMoveLow );
    }
    else if( nWhence == SEEK_CUR )
    {
        VSIDebug3( "VSIFSeekL(%p,%d:%lu,SEEK_CUR)",
                   fp, (int) dwMoveHigh, (unsigned long) nMoveLow );
    }
    else
    {
        VSIDebug4( "VSIFSeekL(%p,%d:%lu,%d-Unknown)",
                   fp, (int) dwMoveHigh, (unsigned long) nMoveLow,
                   nWhence );
    }
#endif 

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
    LARGE_INTEGER   li;

    li.HighPart = 0;
    li.LowPart = SetFilePointer( hFile, 0, (PLONG) &(li.HighPart), 
                                 FILE_CURRENT );

    VSIDebug3( "VSIFTellL(%p) = %ld:%ld", fp, li.HighPart, li.LowPart );

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

    VSIDebug1( "VSIFFlushL(%p)", fp );

    FlushFileBuffers( hFile );
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    HANDLE      hFile = (HANDLE) fp;
    DWORD       dwSizeRead;
    size_t      nResult;

    if( !ReadFile( hFile, pBuffer, (DWORD)(nSize*nCount), &dwSizeRead, NULL ) )
        nResult = 0;
    else
        nResult = dwSizeRead / nSize;

    VSIDebug3( "VSIFReadL(%p,%ld) = %ld", 
               fp, (long) nSize * nCount, (long) dwSizeRead );

    return nResult;
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

size_t VSIFWriteL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    HANDLE      hFile = (HANDLE) fp;
    DWORD       dwSizeWritten;
    size_t      nResult;

    if( !WriteFile(hFile,pBuffer,(DWORD)(nSize*nCount),&dwSizeWritten,NULL) )
        nResult = 0;
    else
        nResult = dwSizeWritten / nSize;

    VSIDebug3( "VSIFWriteL(%p,%ld) = %ld", 
               fp, (long) nSize * nCount, (long) dwSizeWritten );

    return nResult;
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

