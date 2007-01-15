/******************************************************************************
 * $Id$
 *
 * Project:  libtiff
 * Purpose:  Sample implementation of "memory" io functions that can be
 *           passed to TIFFClientOpen() or XTIFFClientOpen() to manage 
 *           an in-memory TIFF file. 
 * Author:   Mike Johnson - Banctec AB
 *           Frank Warmerdam, warmerdam@pobox.com
 *
 * While this file is currently GDAL-only, it is intended to be easily
 * migrated back into the libtiff core, or libtiff/contrib tree. 
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 1996 Mike Johnson
 * Copyright (c) 1996 BancTec AB
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Mike Johnson and BancTec may not be used in any advertising or
 * publicity relating to the software.
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
 * $Log$
 * Revision 1.3  2003/07/08 15:49:24  warmerda
 * avoid warnings
 *
 * Revision 1.2  2003/05/12 13:26:43  warmerda
 * Added string.h.
 *
 * Revision 1.1  2002/10/08 23:00:27  warmerda
 * New
 *
 */

#include <string.h>
#include "tif_memio.h"

static void MemIO_ExtendFile( MemIOBuf *miobuf, tsize_t size );

/************************************************************************/
/*                           MemIO_InitBuf()                            */
/*                                                                      */
/*      Initialize a passed in MemIOBuf structure.                      */
/************************************************************************/

void MemIO_InitBuf( MemIOBuf * miobuf, int size, unsigned char *data )

{
    miobuf->data = NULL;
    miobuf->size = 0;
    miobuf->offset = 0;
    miobuf->data_buf_size = 0;
    miobuf->own_buffer = 1;

    if( size > 0 )
    {
        miobuf->data = data;
        miobuf->size = size;
        miobuf->data_buf_size = size;
        miobuf->own_buffer = 0;
    }
}

/************************************************************************/
/*                          MemIO_DeinitBuf()                           */
/*                                                                      */
/*      Clear and free memory buffer.                                   */
/************************************************************************/

void MemIO_DeinitBuf( MemIOBuf *miobuf )

{
    if( miobuf->own_buffer && miobuf->data != NULL )
        _TIFFfree( miobuf->data );
    memset( miobuf, 0, sizeof(MemIOBuf) );
}

/************************************************************************/
/*                           MemIO_SeekProc()                           */
/************************************************************************/

toff_t MemIO_SeekProc( thandle_t fd, toff_t off, int whence )

{
    MemIOBuf *miobuf = (MemIOBuf *) fd;
    toff_t new_off;

    if( whence == SEEK_SET )
        new_off = off;
    else if( whence == SEEK_CUR )
        new_off = miobuf->offset + off;
    else if( whence == SEEK_END )
        new_off = miobuf->size + off;
    else
        return (toff_t) -1;

    if( new_off < 0 )
        return (toff_t) -1;

    if( new_off > (toff_t) miobuf->size )
    {
        MemIO_ExtendFile( miobuf, new_off );
        if( new_off > (toff_t) miobuf->size )
            return (toff_t) -1;
    }
    
    miobuf->offset = new_off;
    
    return miobuf->offset;
}

/************************************************************************/
/*                           MemIO_ReadProc()                           */
/************************************************************************/

tsize_t MemIO_ReadProc( thandle_t fd, tdata_t buf, tsize_t size )

{
    MemIOBuf *miobuf = (MemIOBuf *) fd;
    int      result = 0;

    if( miobuf->offset + size > (toff_t) miobuf->size )
        result = miobuf->size - miobuf->offset;
    else
        result = size;

    memcpy( buf, miobuf->data + miobuf->offset, result );
    miobuf->offset += result;

    return result;
}

/************************************************************************/
/*                          MemIO_WriteProc()                           */
/************************************************************************/

tsize_t MemIO_WriteProc( thandle_t fd, tdata_t buf, tsize_t size )

{
    MemIOBuf *miobuf = (MemIOBuf *) fd;
    int      result = 0;

    if( miobuf->offset + size > (toff_t) miobuf->size )
        MemIO_ExtendFile( miobuf, miobuf->offset + size );

    if( miobuf->offset + size > (toff_t) miobuf->size )
        result = miobuf->size - miobuf->offset;
    else
        result = size;

    memcpy( miobuf->data + miobuf->offset, buf, result );
    miobuf->offset += result;

    return result;
}

/************************************************************************/
/*                           MemIO_SizeProc()                           */
/************************************************************************/

toff_t MemIO_SizeProc( thandle_t fd )

{
    MemIOBuf *miobuf = (MemIOBuf *) fd;

    return miobuf->size;
}

/************************************************************************/
/*                          MemIO_CloseProc()                           */
/************************************************************************/

int MemIO_CloseProc( thandle_t fd )

{
    return 0;
}

/************************************************************************/
/*                           MemIO_MapProc()                            */
/************************************************************************/

int MemIO_MapProc( thandle_t fd, tdata_t *pbase, toff_t *psize )

{
    MemIOBuf *miobuf = (MemIOBuf *) fd;

    *pbase = miobuf->data;
    *psize = miobuf->size;

    return 0;
}

/************************************************************************/
/*                          MemIO_UnmapProc()                           */
/************************************************************************/

void MemIO_UnmapProc( thandle_t fd, tdata_t base, toff_t size )

{
}

/************************************************************************/
/*                          MemIO_ExtendFile()                          */
/************************************************************************/

static void MemIO_ExtendFile( MemIOBuf *miobuf, tsize_t size )

{
    tsize_t new_buf_size;
    tdata_t new_buffer;

    if( size < miobuf->size )
        return;

    if( size < miobuf->data_buf_size )
    {
        miobuf->size = size;
        return;
    }

    new_buf_size = (int) (size * 1.25);
    
    if( miobuf->own_buffer )
        new_buffer = _TIFFrealloc( miobuf->data, new_buf_size );
    else
    {
        new_buffer = _TIFFmalloc( new_buf_size );
        if( new_buffer != NULL )
            memcpy( new_buffer, miobuf->data, miobuf->size );
    }
    
    if( new_buffer == NULL )
        return;

    miobuf->data = new_buffer;
    miobuf->data_buf_size = new_buf_size;
    miobuf->size = size;
}

