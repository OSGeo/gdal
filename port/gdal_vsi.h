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
 * gdal_vsi.h
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
 * Revision 1.1  1998/10/18 06:13:05  warmerda
 * Initial implementation.
 *
 */

#ifndef GDAL_VSI_H_INCLUDED
#define GDAL_VSI_H_INCLUDED

#include "gdal_base.h"

GDAL_C_START

/* ==================================================================== */
/*      stdio file access functions.                                    */
/* ==================================================================== */

FILE GDAL_DLL *	VSIFOpen( const char *, const char * );
int GDAL_DLL 	VSIFClose( FILE * );
int GDAL_DLL    VSIFSeek( FILE *, long, int );
long GDAL_DLL	VSIFTell( FILE * );
void GDAL_DLL   VSIRewind( FILE * );

size_t GDAL_DLL	VSIFRead( void *, size_t, size_t, FILE * );
size_t GDAL_DLL VSIFWrite( void *, size_t, size_t, FILE * );

int GDAL_DLL    VSIFPrintf( FILE *, const char *, ... );

/* ==================================================================== */
/*      Memory allocation                                               */
/* ==================================================================== */

void GDAL_DLL	*VSICalloc( size_t, size_t );
void GDAL_DLL   *VSIMalloc( size_t );
void GDAL_DLL	VSIFree( void * );
void GDAL_DLL   *VSIRealloc( void *, size_t );



GDAL_C_END

#endif /* ndef GDAL_VSI_H_INCLUDED */
