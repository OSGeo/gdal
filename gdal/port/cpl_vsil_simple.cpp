/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Alternatve simplified implementation VSI*L File API that just
 *           uses plain VSI API and/or posix calls.  This module isn't
 *           normally built into GDAL.  It is for simple packages like
 *           dgnlib.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_vsi.h"

CPL_CVSID("$Id$");

#ifdef WIN32
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <dirent.h>
#endif

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

int VSIMkdir( const char *pszPathname, long mode )

{
#ifdef WIN32
    return mkdir( pszPathname );
#else
    return mkdir( pszPathname, mode );
#endif
}

/************************************************************************/
/*                             VSIUnlink()                              */
/*************************a***********************************************/

int VSIUnlink( const char * pszFilename )

{
    return unlink( pszFilename );
}

/************************************************************************/
/*                             VSIRename()                              */
/************************************************************************/

int VSIRename( const char * oldpath, const char * newpath )

{
    return rename( oldpath, newpath );
}

/************************************************************************/
/*                              VSIRmdir()                              */
/************************************************************************/

int VSIRmdir( const char * pszDirname )

{
    return rmdir( pszDirname );
}

/************************************************************************/
/*                              VSIStatL()                              */
/************************************************************************/

int VSIStatL( const char * pszFilename, VSIStatBufL *psStatBuf )

{
    return( VSIStat( pszFilename, (VSIStatBuf *) psStatBuf ) );
}

/************************************************************************/
/*                             VSIFOpenL()                              */
/************************************************************************/

FILE *VSIFOpenL( const char * pszFilename, const char * pszAccess )

{
    return VSIFOpen( pszFilename, pszAccess );
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

int VSIFCloseL( FILE * fp )

{
    return VSIFClose( fp );
}

/************************************************************************/
/*                             VSIFSeekL()                              */
/************************************************************************/

int VSIFSeekL( FILE * fp, vsi_l_offset nOffset, int nWhence )

{
    return VSIFSeek(fp, static_cast<int>(nOffset), nWhence);
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

vsi_l_offset VSIFTellL( FILE * fp )

{
    return static_cast<vsi_l_offset>(VSIFTell(fp));
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

int VSIFFlushL( FILE * fp )

{
    VSIFFlush( fp );
    return 0;
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

size_t VSIFReadL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    return VSIFRead( pBuffer, nSize, nCount, fp );
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

size_t VSIFWriteL( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    return VSIFWrite( pBuffer, nSize, nCount, fp );
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

int VSIFEofL( FILE * fp )

{
    return VSIFEof( fp );
}
