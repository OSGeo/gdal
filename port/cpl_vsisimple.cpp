/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * cpl_vsisimple.cpp
 *
 * This is a simple implementation (direct to Posix) of the Virtual System
 * Interface (VSI).  See gdal_vsi.h.
 *
 * TODO:
 *  - add some assertions to ensure that arguments are widely legal.  For
 *    instance validation of access strings to fopen().
 * 
 * $Log$
 * Revision 1.12  2002/06/15 03:10:22  aubin
 * remove debug test for 64bit compile
 *
 * Revision 1.11  2002/06/15 00:07:23  aubin
 * mods to enable 64bit file i/o
 *
 * Revision 1.10  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.9  2001/04/30 18:19:06  warmerda
 * avoid stat on macos_pre10
 *
 * Revision 1.8  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.7  2001/01/03 05:33:17  warmerda
 * added VSIFlush
 *
 * Revision 1.6  2000/12/14 18:29:48  warmerda
 * added VSIMkdir
 *
 * Revision 1.5  2000/01/26 19:06:29  warmerda
 * fix up mkdir/unlink for windows
 *
 * Revision 1.4  2000/01/25 03:11:03  warmerda
 * added unlink and mkdir
 *
 * Revision 1.3  1998/12/14 04:50:33  warmerda
 * Avoid C++ comments so it will be C compilable as well.
 *
 * Revision 1.2  1998/12/04 21:42:57  danmo
 * Added #ifndef WIN32 arounf #include <unistd.h>
 *
 * Revision 1.1  1998/12/03 18:26:03  warmerda
 * New
 *
 */

#include "cpl_vsi.h"

CPL_CVSID("$Id$");

/* for stat() */

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#endif
#include <sys/stat.h>

/************************************************************************/
/*                              VSIFOpen()                              */
/************************************************************************/

FILE *VSIFOpen( const char * pszFilename, const char * pszAccess )

{
    return( fopen( (char *) pszFilename, (char *) pszAccess ) );
}

/************************************************************************/
/*                             VSIFClose()                              */
/************************************************************************/

int VSIFClose( FILE * fp )

{
    return( fclose(fp) );
}

/************************************************************************/
/*                              VSIFSeek()                              */
/************************************************************************/

int VSIFSeek( FILE * fp, long nOffset, int nWhence )

{
    return( fseek( fp, nOffset, nWhence ) );
}

/************************************************************************/
/*                              VSIFTell()                              */
/************************************************************************/

long VSIFTell( FILE * fp )

{
    return( ftell( fp ) );
}

/************************************************************************/
/*                             VSIRewind()                              */
/************************************************************************/

void VSIRewind( FILE * fp )

{
    rewind( fp );
}

/************************************************************************/
/*                              VSIFRead()                              */
/************************************************************************/

size_t VSIFRead( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    return( fread( pBuffer, nSize, nCount, fp ) );
}

/************************************************************************/
/*                             VSIFWrite()                              */
/************************************************************************/

size_t VSIFWrite( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    return( fwrite( pBuffer, nSize, nCount, fp ) );
}

/************************************************************************/
/*                             VSIFFlush()                              */
/************************************************************************/

void VSIFFlush( FILE * fp )

{
    fflush( fp );
}

/************************************************************************/
/*                              VSIFGets()                              */
/************************************************************************/

char *VSIFGets( char *pszBuffer, int nBufferSize, FILE * fp )

{
    return( fgets( pszBuffer, nBufferSize, fp ) );
}

/************************************************************************/
/*                              VSIFGetc()                              */
/************************************************************************/

int VSIFGetc( FILE * fp )

{
    return( fgetc( fp ) );
}

/************************************************************************/
/*                             VSIUngetc()                              */
/************************************************************************/

int VSIUngetc( int c, FILE * fp )

{
    return( ungetc( c, fp ) );
}

/************************************************************************/
/*                             VSIFPrintf()                             */
/*                                                                      */
/*      This is a little more complicated than just calling             */
/*      fprintf() because of the variable arguments.  Instead we        */
/*      have to use vfprintf().                                         */
/************************************************************************/

int     VSIFPrintf( FILE * fp, const char * pszFormat, ... )

{
    va_list     args;
    int         nReturn;

    va_start( args, pszFormat );
    nReturn = vfprintf( fp, pszFormat, args );
    va_end( args );

    return( nReturn );
}

/************************************************************************/
/*                              VSIFEof()                               */
/************************************************************************/

int VSIFEof( FILE * fp )

{
    return( feof( fp ) );
}

/************************************************************************/
/*                              VSIFPuts()                              */
/************************************************************************/

int VSIFPuts( const char * pszString, FILE * fp )

{
    return fputs( pszString, fp );
}

/************************************************************************/
/*                              VSIFPutc()                              */
/************************************************************************/

int VSIFPutc( int nChar, FILE * fp )

{
    return( fputc( nChar, fp ) );
}

/************************************************************************/
/*                             VSICalloc()                              */
/************************************************************************/

void *VSICalloc( size_t nCount, size_t nSize )

{
    return( calloc( nCount, nSize ) );
}

/************************************************************************/
/*                             VSIMalloc()                              */
/************************************************************************/

void *VSIMalloc( size_t nSize )

{
    return( malloc( nSize ) );
}

/************************************************************************/
/*                             VSIRealloc()                             */
/************************************************************************/

void * VSIRealloc( void * pData, size_t nNewSize )

{
    return( realloc( pData, nNewSize ) );
}

/************************************************************************/
/*                              VSIFree()                               */
/************************************************************************/

void VSIFree( void * pData )

{
    if( pData != NULL )
        free( pData );
}

/************************************************************************/
/*                             VSIStrdup()                              */
/************************************************************************/

char *VSIStrdup( const char * pszString )

{
    return( strdup( pszString ) );
}

/************************************************************************/
/*                              VSIStat()                               */
/************************************************************************/

int VSIStat( const char * pszFilename, VSIStatBuf * pStatBuf )

{
#if defined(macos_pre10)
    return -1;
#else
#ifdef VSI_LARGE_API_SUPPORTED
    return( stat64( pszFilename, pStatBuf ) );
#else
    return( stat( pszFilename, pStatBuf ) );
#endif
#endif
}

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

int VSIMkdir( const char *pszPathname, long mode )

{
#ifdef WIN32
    return mkdir( pszPathname );
#elif defined(macos_pre10)
    return -1;
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
/*                              VSIRmdir()                              */
/************************************************************************/

int VSIRmdir( const char * pszFilename )

{
    return rmdir( pszFilename );
}


