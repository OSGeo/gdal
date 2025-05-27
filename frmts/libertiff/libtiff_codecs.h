/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  GeoTIFF thread safe reader using libertiff library.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <cinttypes>

#include "tiff_common.h"

// Use code from internal libtiff for LZW, PackBits and LERC codecs
#define TIFFInitLZW LIBERTIFF_TIFFInitLZW
#define TIFFInitPackBits LIBERTIFF_TIFFInitPackBits
#define TIFFInitLERC LIBERTIFF_TIFFInitLERC
#define _TIFFmallocExt LIBERTIFF_TIFFmallocExt
#define _TIFFreallocExt LIBERTIFF_TIFFreallocExt
#define _TIFFcallocExt LIBERTIFF_TIFFcallocExt
#define _TIFFfreeExt LIBERTIFF_TIFFfreeExt
#define _TIFFmemset LIBERTIFF_TIFFmemset
#define _TIFFmemcpy LIBERTIFF_TIFFmemcpy
#define TIFFPredictorInit LIBERTIFF_TIFFPredictorInit
#define TIFFPredictorCleanup LIBERTIFF_TIFFPredictorCleanup
#define _TIFFSetDefaultCompressionState LIBERTIFF_TIFFSetDefaultCompressionState
#define TIFFFlushData1 LIBERTIFF_TIFFFlushData1_dummy
#define TIFFWarningExtR LIBERTIFF_TIFFWarningExtR
#define TIFFErrorExtR LIBERTIFF_TIFFErrorExtR
#define TIFFTileRowSize LIBERTIFF_TIFFTileRowSize_dummy
#define TIFFScanlineSize LIBERTIFF_TIFFScanlineSize_dummy
#define TIFFSetField LIBERTIFF_TIFFSetField_dummy
#define _TIFFMergeFields LIBERTIFF_TIFFMergeFields_dummy
#define register
extern "C"
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#define LZW_READ_ONLY
#include "tif_lzw.c"

#define PACKBITS_READ_ONLY
#include "tif_packbits.c"

#ifdef LERC_SUPPORT
#define LERC_READ_ONLY
#include "tif_lerc.c"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    void *LIBERTIFF_TIFFmallocExt(TIFF *, tmsize_t s)
    {
        return malloc(s);
    }

    void *LIBERTIFF_TIFFreallocExt(TIFF *, void *p, tmsize_t s)
    {
        return realloc(p, s);
    }

    void *LIBERTIFF_TIFFcallocExt(TIFF *, tmsize_t nmemb, tmsize_t siz)
    {
        return calloc(nmemb, siz);
    }

    void LIBERTIFF_TIFFfreeExt(TIFF *, void *ptr)
    {
        free(ptr);
    }

    void LIBERTIFF_TIFFmemset(void *ptr, int v, tmsize_t s)
    {
        memset(ptr, v, s);
    }

    void LIBERTIFF_TIFFmemcpy(void *d, const void *s, tmsize_t c)
    {
        memcpy(d, s, c);
    }

    void LIBERTIFF_TIFFSetDefaultCompressionState(TIFF *)
    {
    }

    int LIBERTIFF_TIFFSetField_dummy(TIFF *, uint32_t, ...)
    {
        return 0;
    }

    int LIBERTIFF_TIFFMergeFields_dummy(TIFF *, const TIFFField[], uint32_t)
    {
        return 1;
    }

    int LIBERTIFF_TIFFPredictorInit(TIFF *)
    {
        return 0;
    }

    int LIBERTIFF_TIFFPredictorCleanup(TIFF *)
    {
        return 0;
    }

    void LIBERTIFF_TIFFWarningExtR(TIFF *, const char *pszModule,
                                   const char *fmt, ...)
    {
        char *pszModFmt =
            gdal::tiff_common::PrepareTIFFErrorFormat(pszModule, fmt);
        va_list ap;
        va_start(ap, fmt);
        CPLErrorV(CE_Warning, CPLE_AppDefined, pszModFmt, ap);
        va_end(ap);
        CPLFree(pszModFmt);
    }

    void LIBERTIFF_TIFFErrorExtR(TIFF *, const char *pszModule, const char *fmt,
                                 ...)
    {
        char *pszModFmt =
            gdal::tiff_common::PrepareTIFFErrorFormat(pszModule, fmt);
        va_list ap;
        va_start(ap, fmt);
        CPLErrorV(CE_Failure, CPLE_AppDefined, pszModFmt, ap);
        va_end(ap);
        CPLFree(pszModFmt);
    }
}
#undef isTiled
