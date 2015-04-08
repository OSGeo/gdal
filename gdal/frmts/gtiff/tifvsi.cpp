/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implement system hook functions for libtiff on top of CPL/VSI,
 *           including > 2GB support.  Based on tif_unix.c from libtiff
 *           distribution.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam, warmerdam@pobox.com
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

/*
 * TIFF Library UNIX-specific Routines.
 */
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "tifvsi.h"

#include <errno.h>

// We avoid including xtiffio.h since it drags in the libgeotiff version
// of the VSI functions.

#ifdef RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS
#include "gdal_libgeotiff_symbol_rename.h"
#endif

CPL_C_START
extern TIFF CPL_DLL * XTIFFClientOpen(const char* name, const char* mode, 
                                      thandle_t thehandle,
                                      TIFFReadWriteProc, TIFFReadWriteProc,
                                      TIFFSeekProc, TIFFCloseProc,
                                      TIFFSizeProc,
                                      TIFFMapFileProc, TIFFUnmapFileProc);
CPL_C_END

#define BUFFER_SIZE     (65536)

typedef struct
{
    VSILFILE*   fpL;
    int         bAtEndOfFile;
    vsi_l_offset nExpectedPos;
    GByte      *abyWriteBuffer;
    int         nWriteBufferSize;
} GDALTiffHandle;

static tsize_t
_tiffReadProc(thandle_t th, tdata_t buf, tsize_t size)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    return VSIFReadL( buf, 1, size, psGTH->fpL );
}

static int GTHFlushBuffer(thandle_t th)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    int bRet = TRUE;
    if( psGTH->abyWriteBuffer && psGTH->nWriteBufferSize )
    {
        tsize_t nRet = VSIFWriteL( psGTH->abyWriteBuffer, 1, psGTH->nWriteBufferSize, psGTH->fpL );
        bRet = (nRet == psGTH->nWriteBufferSize);
        if( !bRet )
        {
            TIFFErrorExt( th, "_tiffWriteProc", "%s", VSIStrerror( errno ) );
        }
        psGTH->nWriteBufferSize = 0;
    }
    return bRet;
}


static tsize_t
_tiffWriteProc(thandle_t th, tdata_t buf, tsize_t size)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    
    // If we have a write buffer and are at end of file, then accumulate
    // the bytes until the buffer is full
    if( psGTH->bAtEndOfFile && psGTH->abyWriteBuffer )
    {
        const GByte* pabyData = (const GByte*) buf;
        tsize_t nRemainingBytes = size;
        while( TRUE )
        {
            if( psGTH->nWriteBufferSize + nRemainingBytes <= BUFFER_SIZE )
            {
                memcpy( psGTH->abyWriteBuffer + psGTH->nWriteBufferSize,
                        pabyData, nRemainingBytes );
                psGTH->nWriteBufferSize += nRemainingBytes;
                psGTH->nExpectedPos += size;
                return size;
            }

            int nAppendable = BUFFER_SIZE - psGTH->nWriteBufferSize;
            memcpy( psGTH->abyWriteBuffer + psGTH->nWriteBufferSize, pabyData,
                    nAppendable );
            tsize_t nRet = VSIFWriteL( psGTH->abyWriteBuffer, 1, BUFFER_SIZE, psGTH->fpL );
            psGTH->nWriteBufferSize = 0;
            if( nRet != BUFFER_SIZE )
            {
                TIFFErrorExt( th, "_tiffWriteProc", "%s", VSIStrerror( errno ) );
                return 0;
            }

            pabyData += nAppendable;
            nRemainingBytes -= nAppendable;
        }
    }
    
    tsize_t nRet = VSIFWriteL( buf, 1, size, psGTH->fpL );
    if (nRet < size)
    {
        TIFFErrorExt( th, "_tiffWriteProc", "%s", VSIStrerror( errno ) );
    }
    if( psGTH->bAtEndOfFile )
    {
        psGTH->nExpectedPos += nRet;
    }
    return nRet;
}

static toff_t
_tiffSeekProc(thandle_t th, toff_t off, int whence)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;

    /* Optimization: if we are already at end, then no need to */
    /* issue a VSIFSeekL() */
    if( whence == SEEK_END )
    {
        if( psGTH->bAtEndOfFile )
        {
            return (toff_t) psGTH->nExpectedPos;
        }

        if( VSIFSeekL( psGTH->fpL, off, whence ) != 0 )
        {
            TIFFErrorExt( th, "_tiffSeekProc", "%s", VSIStrerror( errno ) );
            return (toff_t) -1;
        }
        psGTH->bAtEndOfFile = TRUE;
        psGTH->nExpectedPos = VSIFTellL( psGTH->fpL );
        return (toff_t) (psGTH->nExpectedPos);
    }

    GTHFlushBuffer(th);
    psGTH->bAtEndOfFile = FALSE;
    psGTH->nExpectedPos = 0;
    
    if( VSIFSeekL( psGTH->fpL, off, whence ) == 0 )
        return (toff_t) VSIFTellL( psGTH->fpL );
    else
    {
        TIFFErrorExt( th, "_tiffSeekProc", "%s", VSIStrerror( errno ) );
        return (toff_t) -1;
    }
}

static int
_tiffCloseProc(thandle_t th)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    GTHFlushBuffer(th);
    CPLFree(psGTH->abyWriteBuffer);
    CPLFree(psGTH);
    return 0;
}

static toff_t
_tiffSizeProc(thandle_t th)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    vsi_l_offset  old_off;
    toff_t        file_size;
    
    if( psGTH->bAtEndOfFile )
    {
        return (toff_t) psGTH->nExpectedPos;
    }

    old_off = VSIFTellL( psGTH->fpL );
    VSIFSeekL( psGTH->fpL, 0, SEEK_END );
    
    file_size = (toff_t) VSIFTellL( psGTH->fpL );
    VSIFSeekL( psGTH->fpL, old_off, SEEK_SET );

    return file_size;
}

static int
_tiffMapProc(thandle_t th, tdata_t* pbase, toff_t* psize)
{
	(void) th; (void) pbase; (void) psize;
	return (0);
}

static void
_tiffUnmapProc(thandle_t th, tdata_t base, toff_t size)
{
	(void) th; (void) base; (void) size;
}

VSILFILE* VSI_TIFFGetVSILFile(thandle_t th)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    VSI_TIFFFlushBufferedWrite(th);
    return psGTH->fpL;
}

int VSI_TIFFFlushBufferedWrite(thandle_t th)
{
    GDALTiffHandle* psGTH = (GDALTiffHandle*) th;
    psGTH->bAtEndOfFile = FALSE;
    return GTHFlushBuffer(th);
}

/*
 * Open a TIFF file for read/writing.
 */
TIFF* VSI_TIFFOpen(const char* name, const char* mode,
                   VSILFILE* fpL)
{
    int           i, a_out;
    char          access[32];
    TIFF          *tif;
    int           bAllocBuffer = FALSE;

    a_out = 0;
    access[0] = '\0';
    for( i = 0; mode[i] != '\0'; i++ )
    {
        if( mode[i] == 'r'
            || mode[i] == 'w'
            || mode[i] == '+'
            || mode[i] == 'a' )
        {
            access[a_out++] = mode[i];
            access[a_out] = '\0';
        }
        if( mode[i] == 'w'
            || mode[i] == '+'
            || mode[i] == 'a' )
            bAllocBuffer = TRUE;
    }

    // No need to buffer on /vsimem/
    if( strncmp(name, "/vsimem/", strlen("/vsimem/")) == 0 )
        bAllocBuffer = FALSE;

    strcat( access, "b" );

    VSIFSeekL(fpL, 0, SEEK_SET);
    
    GDALTiffHandle* psGTH = (GDALTiffHandle*) CPLMalloc(sizeof(GDALTiffHandle));
    psGTH->fpL = fpL;
    psGTH->nExpectedPos = 0;
    psGTH->bAtEndOfFile = FALSE;
    psGTH->abyWriteBuffer = (bAllocBuffer) ? (GByte*)VSIMalloc(BUFFER_SIZE) : NULL;
    psGTH->nWriteBufferSize = 0;

    tif = XTIFFClientOpen(name, mode,
                          (thandle_t) psGTH,
                          _tiffReadProc, _tiffWriteProc,
                          _tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
                          _tiffMapProc, _tiffUnmapProc);
    if( tif == NULL )
        CPLFree(psGTH);

    return tif;
}
