/******************************************************************************
 * $Id$
 *
 * Project:  libtiff
 * Purpose:  Sample declarations of "memory" io functions that can be
 *           passed to TIFFClientOpen() or XTIFFClientOpen() to manage 
 *           an in-memory TIFF file. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * While this file is currently GDAL-only, it is intended to be easily
 * migrated back into the libtiff core, or libtiff/contrib tree. 
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef TIF_MEMIO_H_INCLUDED
#define TIF_MEMIO_H_INCLUDED

#include "tiffio.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    unsigned char *data;  /* allocated with _TIFFmalloc() */
    tsize_t        size;  /* "file" size in bytes */
    tsize_t        data_buf_size; /* in bytes - may be larger than used */
    toff_t         offset; /* current file offset from start of file. */
    int            own_buffer; /* non-zero if 'data' is owned by MemIOBuf. */
} MemIOBuf;

void  MemIO_InitBuf( MemIOBuf *, int size, unsigned char *data );
void  MemIO_DeinitBuf( MemIOBuf * );

tsize_t MemIO_ReadProc( thandle_t fd, tdata_t buf, tsize_t size );
tsize_t MemIO_WriteProc( thandle_t fd, tdata_t buf, tsize_t size );
toff_t  MemIO_SeekProc( thandle_t fd, toff_t off, int whence );
int     MemIO_CloseProc( thandle_t fd );
toff_t  MemIO_SizeProc( thandle_t fd );
int     MemIO_MapProc( thandle_t fd, tdata_t *pbase, toff_t *psize );
void    MemIO_UnmapProc( thandle_t fd, tdata_t base, toff_t size );

#if defined(__cplusplus)
}
#endif

#endif /* ndef TIF_MEMIO_H_INCLUDED */    
