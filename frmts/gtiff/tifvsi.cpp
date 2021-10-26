/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implement system hook functions for libtiff on top of CPL/VSI,
 *           including > 2GB support.  Based on tif_unix.c from libtiff
 *           distribution.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam, warmerdam@pobox.com
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

// TIFF Library UNIX-specific Routines.

#include "cpl_port.h"
#include "tifvsi.h"

#include <assert.h>
#include <string.h>
#include <cerrno>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "cpl_string.h"

// We avoid including xtiffio.h since it drags in the libgeotiff version
// of the VSI functions.

#ifdef RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS
#include "gdal_libgeotiff_symbol_rename.h"
#endif

CPL_CVSID("$Id$")

CPL_C_START
extern TIFF CPL_DLL * XTIFFClientOpen( const char* name, const char* mode,
                                       thandle_t thehandle,
                                       TIFFReadWriteProc, TIFFReadWriteProc,
                                       TIFFSeekProc, TIFFCloseProc,
                                       TIFFSizeProc,
                                       TIFFMapFileProc, TIFFUnmapFileProc );
extern void CPL_DLL XTIFFClose(TIFF *tif);
CPL_C_END

constexpr int BUFFER_SIZE = 65536;

struct GDALTiffHandle;

struct GDALTiffHandleShared
{
    VSILFILE       *fpL;
    bool            bReadOnly;
    bool            bLazyStrileLoading;
    char           *pszName;
    GDALTiffHandle *psActiveHandle; // only used on the parent
    int             nUserCounter;
    bool            bAtEndOfFile;
    vsi_l_offset    nFileLength;
};

struct GDALTiffHandle
{
    bool bFree;

    GDALTiffHandle* psParent; // nullptr for the parent itself
    GDALTiffHandleShared* psShared;

    GByte      *abyWriteBuffer;
    int         nWriteBufferSize;

    // For pseudo-mmap'ed /vsimem/ file
    vsi_l_offset nDataLength;
    void*        pBase;

    // If we pre-cached data (typically from /vsicurl/ )
    int          nCachedRanges;
    void**       ppCachedData;
    vsi_l_offset* panCachedOffsets;
    size_t*       panCachedSizes;
};

static bool GTHFlushBuffer( thandle_t th );

static void SetActiveGTH(GDALTiffHandle* psGTH)
{
    auto psShared = psGTH->psShared;
    if( psShared->psActiveHandle != psGTH )
    {
        if( psShared->psActiveHandle != nullptr )
        {
            GTHFlushBuffer( static_cast<thandle_t>(psShared->psActiveHandle) );
        }
        psShared->psActiveHandle = psGTH;
    }
}

void* VSI_TIFFGetCachedRange( thandle_t th, vsi_l_offset nOffset, size_t nSize )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    for( int i = 0; i < psGTH->nCachedRanges; i++ )
    {
        if( nOffset >= psGTH->panCachedOffsets[i] &&
            nOffset + nSize <=
                psGTH->panCachedOffsets[i] + psGTH->panCachedSizes[i] )
        {
            return static_cast<GByte*>(psGTH->ppCachedData[i]) +
                            (nOffset - psGTH->panCachedOffsets[i]);
        }
        if( nOffset < psGTH->panCachedOffsets[i] )
            break;
    }
    return nullptr;
}

static tsize_t
_tiffReadProc( thandle_t th, tdata_t buf, tsize_t size )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle *>(th);
    //SetActiveGTH(psGTH);

    if( psGTH->nCachedRanges )
    {
        const vsi_l_offset nCurOffset = VSIFTellL( psGTH->psShared->fpL );
        void* data = VSI_TIFFGetCachedRange(th, nCurOffset, static_cast<size_t>(size));
        if( data )
        {
            memcpy(buf, data, size);
            VSIFSeekL( psGTH->psShared->fpL, nCurOffset + size, SEEK_SET );
            return size;
        }
    }

#ifdef DEBUG_VERBOSE_EXTRA
    CPLDebug("GTiff", "Reading %d bytes at offset " CPL_FRMT_GUIB,
             static_cast<int>(size),
             VSIFTellL( psGTH->psShared->fpL ));
#endif
    return VSIFReadL( buf, 1, size, psGTH->psShared->fpL );
}

static bool GTHFlushBuffer( thandle_t th )
{
    GDALTiffHandle* psGTH = static_cast<GDALTiffHandle*>(th);
    bool bRet = true;
    if( psGTH->abyWriteBuffer && psGTH->nWriteBufferSize )
    {
        const tsize_t nRet = VSIFWriteL( psGTH->abyWriteBuffer, 1,
                                         psGTH->nWriteBufferSize, psGTH->psShared->fpL );
        bRet = nRet == psGTH->nWriteBufferSize;
        if( !bRet )
        {
            TIFFErrorExt( th, "_tiffWriteProc", "%s", VSIStrerror( errno ) );
        }
        psGTH->nWriteBufferSize = 0;
    }
    return bRet;
}

static tsize_t
_tiffWriteProc( thandle_t th, tdata_t buf, tsize_t size )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle *>( th );
    SetActiveGTH(psGTH);

    // If we have a write buffer and are at end of file, then accumulate
    // the bytes until the buffer is full.
    if( psGTH->psShared->bAtEndOfFile && psGTH->abyWriteBuffer )
    {
        const GByte* pabyData = reinterpret_cast<GByte *>( buf );
        tsize_t nRemainingBytes = size;
        while( true )
        {
            if( psGTH->nWriteBufferSize + nRemainingBytes <= BUFFER_SIZE )
            {
                memcpy( psGTH->abyWriteBuffer + psGTH->nWriteBufferSize,
                        pabyData, nRemainingBytes );
                psGTH->nWriteBufferSize += static_cast<int>(nRemainingBytes);
                if( psGTH->psShared->bAtEndOfFile )
                {
                    psGTH->psShared->nFileLength += size;
                }
                return size;
            }

            int nAppendable = BUFFER_SIZE - psGTH->nWriteBufferSize;
            memcpy( psGTH->abyWriteBuffer + psGTH->nWriteBufferSize, pabyData,
                    nAppendable );
            const tsize_t nRet = VSIFWriteL( psGTH->abyWriteBuffer, 1,
                                             BUFFER_SIZE, psGTH->psShared->fpL );
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

    const tsize_t nRet = VSIFWriteL( buf, 1, size, psGTH->psShared->fpL );
    if( nRet < size )
    {
        TIFFErrorExt( th, "_tiffWriteProc", "%s", VSIStrerror( errno ) );
    }

    if( psGTH->psShared->bAtEndOfFile )
    {
        psGTH->psShared->nFileLength += nRet;
    }
    return nRet;
}

static toff_t
_tiffSeekProc( thandle_t th, toff_t off, int whence )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle *>( th );
    SetActiveGTH(psGTH);

    // Optimization: if we are already at end, then no need to
    // issue a VSIFSeekL().
    if( whence == SEEK_END )
    {
        if( psGTH->psShared->bAtEndOfFile )
        {
            return static_cast<toff_t>( psGTH->psShared->nFileLength );
        }

        if( VSIFSeekL( psGTH->psShared->fpL, off, whence ) != 0 )
        {
            TIFFErrorExt( th, "_tiffSeekProc", "%s", VSIStrerror( errno ) );
            return static_cast<toff_t>( -1 );
        }
        psGTH->psShared->bAtEndOfFile = true;
        psGTH->psShared->nFileLength = VSIFTellL( psGTH->psShared->fpL );
        return static_cast<toff_t>(psGTH->psShared->nFileLength);
    }

    GTHFlushBuffer(th);
    psGTH->psShared->bAtEndOfFile = false;
    psGTH->psShared->nFileLength = 0;

    if( VSIFSeekL( psGTH->psShared->fpL, off, whence ) == 0 )
    {
        return static_cast<toff_t>( VSIFTellL( psGTH->psShared->fpL ) );
    }
    else
    {
        TIFFErrorExt( th, "_tiffSeekProc", "%s", VSIStrerror( errno ) );
        return static_cast<toff_t>(-1);
    }
}

static void FreeGTH(GDALTiffHandle* psGTH)
{
    psGTH->psShared->nUserCounter --;
    if( psGTH->psParent == nullptr )
    {
        assert( psGTH->psShared->nUserCounter == 0 );
        CPLFree(psGTH->psShared->pszName);
        CPLFree(psGTH->psShared);
    }
    else
    {
        if( psGTH->psShared->psActiveHandle == psGTH )
            psGTH->psShared->psActiveHandle = nullptr;
    }
    CPLFree(psGTH->abyWriteBuffer);
    CPLFree(psGTH->ppCachedData);
    CPLFree(psGTH->panCachedOffsets);
    CPLFree(psGTH->panCachedSizes);
    CPLFree(psGTH);
}

static int
_tiffCloseProc( thandle_t th )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    SetActiveGTH(psGTH);
    GTHFlushBuffer(th);
    if( psGTH->bFree )
        FreeGTH(psGTH);
    return 0;
}

static toff_t
_tiffSizeProc( thandle_t th )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    SetActiveGTH(psGTH);

    if( psGTH->psShared->bAtEndOfFile )
    {
        return static_cast<toff_t>( psGTH->psShared->nFileLength );
    }

    const vsi_l_offset old_off = VSIFTellL( psGTH->psShared->fpL );
    CPL_IGNORE_RET_VAL(VSIFSeekL( psGTH->psShared->fpL, 0, SEEK_END ));

    const toff_t file_size = static_cast<toff_t>(VSIFTellL( psGTH->psShared->fpL ));
    CPL_IGNORE_RET_VAL(VSIFSeekL( psGTH->psShared->fpL, old_off, SEEK_SET ));

    return file_size;
}

static int
_tiffMapProc( thandle_t th, tdata_t* pbase , toff_t* psize )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    //SetActiveGTH(psGTH);

    if( psGTH->pBase )
    {
        *pbase = psGTH->pBase;
        *psize = static_cast<toff_t>(psGTH->nDataLength);
        return 1;
    }
    return 0;
}

static void
_tiffUnmapProc( thandle_t /* th */, tdata_t /* base */, toff_t /* size */ )
{}

VSILFILE* VSI_TIFFGetVSILFile( thandle_t th )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle *>( th );
    SetActiveGTH(psGTH);
    VSI_TIFFFlushBufferedWrite(th);
    return psGTH->psShared->fpL;
}

int VSI_TIFFFlushBufferedWrite( thandle_t th )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    SetActiveGTH(psGTH);
    psGTH->psShared->bAtEndOfFile = false;
    return GTHFlushBuffer(th);
}

int VSI_TIFFHasCachedRanges( thandle_t th )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    return psGTH->nCachedRanges != 0;
}

toff_t VSI_TIFFSeek(TIFF* tif, toff_t off, int whence )
{
    thandle_t th = TIFFClientdata( tif );
    return _tiffSeekProc(th, off, whence);
}

int VSI_TIFFWrite( TIFF* tif, const void* buffer, size_t buffersize )
{
    thandle_t th = TIFFClientdata( tif );
    return static_cast<size_t>(
        _tiffWriteProc( th, const_cast<tdata_t>(buffer), buffersize )) == buffersize;
}

void VSI_TIFFSetCachedRanges( thandle_t th, int nRanges,
                              void ** ppData,
                              const vsi_l_offset* panOffsets,
                              const size_t* panSizes )
{
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );
    psGTH->nCachedRanges = nRanges;
    if( nRanges )
    {
        psGTH->ppCachedData =
            static_cast<void**>(CPLRealloc(psGTH->ppCachedData,
                                                nRanges * sizeof(void*)));
        memcpy(psGTH->ppCachedData, ppData,
            nRanges * sizeof(void*));

        psGTH->panCachedOffsets =
            static_cast<vsi_l_offset*>(CPLRealloc(psGTH->panCachedOffsets,
                                                nRanges * sizeof(vsi_l_offset)));
        memcpy(psGTH->panCachedOffsets, panOffsets,
            nRanges * sizeof(vsi_l_offset));

        psGTH->panCachedSizes =
            static_cast<size_t*>(CPLRealloc(psGTH->panCachedSizes,
                                                nRanges * sizeof(size_t)));
        memcpy(psGTH->panCachedSizes, panSizes,
            nRanges * sizeof(size_t));
    }
}

static bool IsReadOnly(const char* mode)
{
    bool bReadOnly = true;
    for( int i = 0; mode[i] != '\0'; i++ )
    {
        if( mode[i] == 'w'
            || mode[i] == '+'
            || mode[i] == 'a' )
        {
            bReadOnly = false;
        }
    }
    return bReadOnly;
}

static void InitializeWriteBuffer(GDALTiffHandle* psGTH, const char* pszMode)
{
    // No need to buffer on /vsimem/
    const bool bReadOnly = IsReadOnly(pszMode);
    bool bAllocBuffer = !bReadOnly;
    if( STARTS_WITH(psGTH->psShared->pszName, "/vsimem/") )
    {
        if( bReadOnly &&
            CPLTestBool(CPLGetConfigOption("GTIFF_USE_MMAP", "NO")) )
        {
            psGTH->nDataLength = 0;
            psGTH->pBase =
                VSIGetMemFileBuffer(psGTH->psShared->pszName, &psGTH->nDataLength, FALSE);
        }
        bAllocBuffer = false;
    }

    psGTH->abyWriteBuffer =
        bAllocBuffer ? static_cast<GByte *>( VSIMalloc(BUFFER_SIZE) ) : nullptr;
    psGTH->nWriteBufferSize = 0;

}

static TIFF* VSI_TIFFOpen_common(GDALTiffHandle* psGTH, const char* pszMode)
{
    InitializeWriteBuffer(psGTH, pszMode);

    TIFF *tif =
        XTIFFClientOpen( psGTH->psShared->pszName,
                         pszMode,
                         reinterpret_cast<thandle_t>(psGTH),
                         _tiffReadProc, _tiffWriteProc,
                         _tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
                         _tiffMapProc, _tiffUnmapProc );
    if( tif == nullptr )
        FreeGTH(psGTH);

    return tif;
}

// Open a TIFF file for read/writing.
TIFF* VSI_TIFFOpen( const char* name, const char* mode,
                    VSILFILE* fpL )
{

    if( VSIFSeekL(fpL, 0, SEEK_SET) < 0 )
        return nullptr;

    GDALTiffHandle* psGTH = static_cast<GDALTiffHandle *>(
        CPLCalloc(1, sizeof(GDALTiffHandle)) );
    psGTH->bFree = true;
    psGTH->psParent = nullptr;
    psGTH->psShared = static_cast<GDALTiffHandleShared *>(
        CPLCalloc(1, sizeof(GDALTiffHandleShared)) );
    psGTH->psShared->bReadOnly = (strchr(mode, '+') == nullptr);
    psGTH->psShared->bLazyStrileLoading = (strchr(mode, 'D') != nullptr);
    psGTH->psShared->pszName = CPLStrdup(name);
    psGTH->psShared->fpL = fpL;
    psGTH->psShared->psActiveHandle = psGTH;
    psGTH->psShared->nFileLength = 0;
    psGTH->psShared->bAtEndOfFile = false;
    psGTH->psShared->nUserCounter = 1;

    return VSI_TIFFOpen_common(psGTH, mode);
}

TIFF* VSI_TIFFOpenChild( TIFF* parent )
{
    GDALTiffHandle* psGTHParent =
        reinterpret_cast<GDALTiffHandle*>(TIFFClientdata(parent));

    GDALTiffHandle* psGTH = static_cast<GDALTiffHandle *>(
        CPLCalloc(1, sizeof(GDALTiffHandle)) );
    psGTH->bFree = true;
    psGTH->psParent = psGTHParent;
    psGTH->psShared = psGTHParent->psShared;
    psGTH->psShared->nUserCounter ++;

    SetActiveGTH(psGTH);
    VSIFSeekL( psGTH->psShared->fpL, 0, SEEK_SET );
    psGTH->psShared->bAtEndOfFile = false;

    const char* mode =
        psGTH->psShared->bReadOnly && psGTH->psShared->bLazyStrileLoading ? "rDO" :
        psGTH->psShared->bReadOnly ? "r" :
        psGTH->psShared->bLazyStrileLoading ? "r+D" : "r+";
    return VSI_TIFFOpen_common(psGTH, mode);
}

// Re-open a TIFF handle (seeking to the appropriate directory is then needed)
TIFF* VSI_TIFFReOpen( TIFF* tif )
{
    thandle_t th = TIFFClientdata( tif );
    GDALTiffHandle* psGTH = reinterpret_cast<GDALTiffHandle*>( th );

    // Disable freeing of psGTH in _tiffCloseProc(), which could be called
    // if XTIFFClientOpen() fails, or obviously by XTIFFClose()
    psGTH->bFree = false;

    const char* mode =
        psGTH->psShared->bReadOnly && psGTH->psShared->bLazyStrileLoading ? "rDO" :
        psGTH->psShared->bReadOnly ? "r" :
        psGTH->psShared->bLazyStrileLoading ? "r+D" : "r+";

    SetActiveGTH(psGTH);
    VSIFSeekL( psGTH->psShared->fpL, 0, SEEK_SET );
    psGTH->psShared->bAtEndOfFile = false;

    TIFF* newHandle = XTIFFClientOpen( psGTH->psShared->pszName,
                         mode,
                         reinterpret_cast<thandle_t>(psGTH),
                         _tiffReadProc, _tiffWriteProc,
                         _tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
                         _tiffMapProc, _tiffUnmapProc );
    if( newHandle != nullptr )
        XTIFFClose(tif);

    psGTH->bFree = true;

    return newHandle;
}
