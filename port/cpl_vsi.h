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

/* -------------------------------------------------------------------- */
/*      We need access to ``struct stat''.                              */
/* -------------------------------------------------------------------- */
#ifndef WIN32
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

#define VSI_ISLNK(x)	S_ISLNK(x)
#define VSI_ISREG(x)	S_ISREG(x)
#define VSI_ISDIR(x)	S_ISDIR(x)
#define VSI_ISCHR(x)	S_ISCHR(x)
#define VSI_ISBLK(x)	S_ISBLK(x)

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
