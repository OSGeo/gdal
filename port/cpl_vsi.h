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
 * cpl_vsi.h
 *
 * Include file defining the Virtual System Interface (VSI) functions.  This
 * should normally be included by all translators using VSI functions for
 * accessing system services.  It is also used by the GDAL core, and can be
 * used by higher level applications which adhere to VSI use.
 *
 * Most VSI functions are direct analogs of Posix C library functions.
 * VSI exists to allow ``hooking'' these functions to provide application
 * specific checking, io redirection and so on. 
 * 
 * $Log$
 * Revision 1.5  1999/05/23 02:43:57  warmerda
 * Added documentation block.
 *
 * Revision 1.4  1999/02/25 04:48:11  danmo
 * Added VSIStat() macros specific to _WIN32 (for MSVC++)
 *
 * Revision 1.3  1999/01/28 18:31:25  warmerda
 * Test on _WIN32 rather than WIN32.  It seems to be more reliably defined.
 *
 * Revision 1.2  1998/12/04 21:42:57  danmo
 * Added #ifndef WIN32 arounf #include <unistd.h>
 *
 * Revision 1.1  1998/12/03 18:26:02  warmerda
 * New
 *
 */

#ifndef CPL_VSI_H_INCLUDED
#define CPL_VSI_H_INCLUDED

#include "cpl_port.h"
/**
 * \file cpl_vsi.h
 *
 * Standard C Covers
 *
 * The VSI functions are intended to be hookable aliases for Standard C
 * I/O, memory allocation and other system functions. They are intended
 * to allow virtualization of disk I/O so that non file data sources
 * can be made to appear as files, and so that additional error trapping
 * and reporting can be interested.  The memory access API is aliased
 * so that special application memory management services can be used.
 *
 * Is is intended that each of these functions retains exactly the same
 * calling pattern as the original Standard C functions they relate to.
 * This means we don't have to provide custom documentation, and also means
 * that the default implementation is very simple.
 */


/* -------------------------------------------------------------------- */
/*      We need access to ``struct stat''.                              */
/* -------------------------------------------------------------------- */
#ifndef _WIN32
#  include <unistd.h>
#endif
#include <sys/stat.h>

CPL_C_START

/* ==================================================================== */
/*      stdio file access functions.                                    */
/* ==================================================================== */

FILE CPL_DLL *	VSIFOpen( const char *, const char * );
int CPL_DLL 	VSIFClose( FILE * );
int CPL_DLL     VSIFSeek( FILE *, long, int );
long CPL_DLL	VSIFTell( FILE * );
void CPL_DLL    VSIRewind( FILE * );

size_t CPL_DLL	VSIFRead( void *, size_t, size_t, FILE * );
size_t CPL_DLL  VSIFWrite( void *, size_t, size_t, FILE * );
char CPL_DLL   *VSIFGets( char *, int, FILE * );
int CPL_DLL     VSIFPuts( const char *, FILE * );
int CPL_DLL     VSIFPrintf( FILE *, const char *, ... );

int CPL_DLL     VSIFGetc( FILE * );
int CPL_DLL     VSIFPutc( int, FILE * );
int CPL_DLL     VSIUngetc( int, FILE * );
int CPL_DLL	VSIFEof( FILE * );

/* ==================================================================== */
/*      VSIStat() related.                                              */
/* ==================================================================== */

typedef struct stat VSIStatBuf;
int CPL_DLL	VSIStat( const char *, VSIStatBuf * );

#ifdef _WIN32
#  define VSI_ISLNK(x)	( 0 )            /* N/A on Windows */
#  define VSI_ISREG(x)	((x) & S_IFREG)
#  define VSI_ISDIR(x)	((x) & S_IFDIR)
#  define VSI_ISCHR(x)	((x) & S_IFCHR)
#  define VSI_ISBLK(x)	( 0 )            /* N/A on Windows */
#else
#  define VSI_ISLNK(x)	S_ISLNK(x)
#  define VSI_ISREG(x)	S_ISREG(x)
#  define VSI_ISDIR(x)	S_ISDIR(x)
#  define VSI_ISCHR(x)	S_ISCHR(x)
#  define VSI_ISBLK(x)	S_ISBLK(x)
#endif

/* ==================================================================== */
/*      Memory allocation                                               */
/* ==================================================================== */

void CPL_DLL   *VSICalloc( size_t, size_t );
void CPL_DLL   *VSIMalloc( size_t );
void CPL_DLL	VSIFree( void * );
void CPL_DLL   *VSIRealloc( void *, size_t );
char CPL_DLL   *VSIStrdup( const char * );

CPL_C_END

#endif /* ndef CPL_VSI_H_INCLUDED */
