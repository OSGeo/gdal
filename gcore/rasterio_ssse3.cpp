/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  SSSE3 specializations
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"

#if defined(HAVE_SSSE3_AT_COMPILE_TIME) &&                                     \
    (defined(__x86_64) || defined(_M_X64))

#include "rasterio_ssse3.h"

#include <tmmintrin.h>
#include "gdal_priv_templates.hpp"

void GDALUnrolledCopy_GByte_3_1_SSSE3(GByte *CPL_RESTRICT pDest,
                                      const GByte *CPL_RESTRICT pSrc,
                                      GPtrDiff_t nIters)
{
    decltype(nIters) i;
    const __m128i xmm_shuffle0 = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                              -1, -1, 15, 12, 9, 6, 3, 0);
    const __m128i xmm_shuffle1 = _mm_set_epi8(-1, -1, -1, -1, -1, 14, 11, 8, 5,
                                              2, -1, -1, -1, -1, -1, -1);
    const __m128i xmm_shuffle2 = _mm_set_epi8(13, 10, 7, 4, 1, -1, -1, -1, -1,
                                              -1, -1, -1, -1, -1, -1, -1);
    // If we were sure that there would always be 2 trailing bytes, we could
    // check against nIters - 15
    for (i = 0; i < nIters - 16; i += 16)
    {
        __m128i xmm0 =
            _mm_loadu_si128(reinterpret_cast<__m128i const *>(pSrc + 0));
        __m128i xmm1 =
            _mm_loadu_si128(reinterpret_cast<__m128i const *>(pSrc + 16));
        __m128i xmm2 =
            _mm_loadu_si128(reinterpret_cast<__m128i const *>(pSrc + 32));

        // From LSB to MSB:
        // 0,x,x,1,x,x,2,x,x,3,x,x,4,x,x,5 --> 0,1,2,3,4,5,0,0,0,0,0,0,0,0,0
        xmm0 = _mm_shuffle_epi8(xmm0, xmm_shuffle0);
        // x,x,6,x,x,7,x,x,8,x,x,9,x,x,10,x --> 0,0,0,0,0,0,6,7,8,9,10,0,0,0,0,0
        xmm1 = _mm_shuffle_epi8(xmm1, xmm_shuffle1);
        // x,11,x,x,12,x,x,13,x,x,14,x,x,15,x,x -->
        // 0,0,0,0,0,0,0,0,0,0,0,11,12,13,14,15
        xmm2 = _mm_shuffle_epi8(xmm2, xmm_shuffle2);
        xmm0 = _mm_or_si128(xmm0, xmm1);
        xmm0 = _mm_or_si128(xmm0, xmm2);

        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDest + i), xmm0);

        pSrc += 3 * 16;
    }
    for (; i < nIters; i++)
    {
        pDest[i] = *pSrc;
        pSrc += 3;
    }
}

/************************************************************************/
/*                  GDALDeinterleave3Byte_SSSE3()                       */
/************************************************************************/

#if defined(__GNUC__) && !defined(__clang__)
// GCC autovectorizer does an excellent job
__attribute__((optimize("tree-vectorize"))) void GDALDeinterleave3Byte_SSSE3(
    const GByte *CPL_RESTRICT pabySrc, GByte *CPL_RESTRICT pabyDest0,
    GByte *CPL_RESTRICT pabyDest1, GByte *CPL_RESTRICT pabyDest2, size_t nIters)
{
    for (size_t i = 0; i < nIters; ++i)
    {
        pabyDest0[i] = pabySrc[3 * i + 0];
        pabyDest1[i] = pabySrc[3 * i + 1];
        pabyDest2[i] = pabySrc[3 * i + 2];
    }
}
#else
void GDALDeinterleave3Byte_SSSE3(const GByte *CPL_RESTRICT pabySrc,
                                 GByte *CPL_RESTRICT pabyDest0,
                                 GByte *CPL_RESTRICT pabyDest1,
                                 GByte *CPL_RESTRICT pabyDest2, size_t nIters)
{
    size_t i = 0;
    for (; i + 15 < nIters; i += 16)
    {
        __m128i xmm0 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 3 * i + 0));
        __m128i xmm1 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 3 * i + 16));
        __m128i xmm2 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 3 * i + 32));
        auto xmm0_new =
            _mm_shuffle_epi8(xmm0, _mm_set_epi8(-1, -1, -1, -1, 11, 8, 5, 2, 10,
                                                7, 4, 1, 9, 6, 3, 0));
        auto xmm1_new = _mm_shuffle_epi8(
            _mm_alignr_epi8(xmm1, xmm0, 12),
            _mm_set_epi8(-1, -1, -1, -1, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0));
        auto xmm2_new = _mm_shuffle_epi8(
            _mm_alignr_epi8(xmm2, xmm1, 8),
            _mm_set_epi8(-1, -1, -1, -1, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0));
        auto xmm3_new =
            _mm_shuffle_epi8(xmm2, _mm_set_epi8(-1, -1, -1, -1, 15, 12, 9, 6,
                                                14, 11, 8, 5, 13, 10, 7, 4));

        __m128i xmm01lo =
            _mm_unpacklo_epi32(xmm0_new, xmm1_new);  // W0 W4 W1 W5
        __m128i xmm01hi = _mm_unpackhi_epi32(xmm0_new, xmm1_new);  // W2 W6 -  -
        __m128i xmm23lo =
            _mm_unpacklo_epi32(xmm2_new, xmm3_new);  // W8 WC W9 WD
        __m128i xmm23hi = _mm_unpackhi_epi32(xmm2_new, xmm3_new);  // WA WE -  -
        xmm0_new = _mm_unpacklo_epi64(xmm01lo, xmm23lo);  // W0 W4 W8 WC
        xmm1_new = _mm_unpackhi_epi64(xmm01lo, xmm23lo);  // W1 W5 W9 WD
        xmm2_new = _mm_unpacklo_epi64(xmm01hi, xmm23hi);  // W2 W6 WA WE
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest0 + i), xmm0_new);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest1 + i), xmm1_new);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest2 + i), xmm2_new);
    }
#if defined(__clang__)
#pragma clang loop vectorize(disable)
#endif
    for (; i < nIters; ++i)
    {
        pabyDest0[i] = pabySrc[3 * i + 0];
        pabyDest1[i] = pabySrc[3 * i + 1];
        pabyDest2[i] = pabySrc[3 * i + 2];
    }
}
#endif

/************************************************************************/
/*                  GDALDeinterleave4Byte_SSSE3()                       */
/************************************************************************/

#if !defined(__GNUC__) || defined(__clang__)
void GDALDeinterleave4Byte_SSSE3(const GByte *CPL_RESTRICT pabySrc,
                                 GByte *CPL_RESTRICT pabyDest0,
                                 GByte *CPL_RESTRICT pabyDest1,
                                 GByte *CPL_RESTRICT pabyDest2,
                                 GByte *CPL_RESTRICT pabyDest3, size_t nIters)
{
    const __m128i shuffle_mask =
        _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
    size_t i = 0;
    for (; i + 15 < nIters; i += 16)
    {
        __m128i xmm0 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 4 * i + 0));
        __m128i xmm1 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 4 * i + 16));
        __m128i xmm2 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 4 * i + 32));
        __m128i xmm3 = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pabySrc + 4 * i + 48));
        xmm0 = _mm_shuffle_epi8(xmm0, shuffle_mask);  // W0 W1 W2 W3
        xmm1 = _mm_shuffle_epi8(xmm1, shuffle_mask);  // W4 W5 W6 W7
        xmm2 = _mm_shuffle_epi8(xmm2, shuffle_mask);  // W8 W9 WA WB
        xmm3 = _mm_shuffle_epi8(xmm3, shuffle_mask);  // WC WD WE WF

        __m128i xmm01lo = _mm_unpacklo_epi32(xmm0, xmm1);  // W0 W4 W1 W5
        __m128i xmm01hi = _mm_unpackhi_epi32(xmm0, xmm1);  // W2 W6 W3 W7
        __m128i xmm23lo = _mm_unpacklo_epi32(xmm2, xmm3);  // W8 WC W9 WD
        __m128i xmm23hi = _mm_unpackhi_epi32(xmm2, xmm3);  // WA WE WB WF
        xmm0 = _mm_unpacklo_epi64(xmm01lo, xmm23lo);       // W0 W4 W8 WC
        xmm1 = _mm_unpackhi_epi64(xmm01lo, xmm23lo);       // W1 W5 W9 WD
        xmm2 = _mm_unpacklo_epi64(xmm01hi, xmm23hi);       // W2 W6 WA WE
        xmm3 = _mm_unpackhi_epi64(xmm01hi, xmm23hi);       // W3 W7 WB WF

        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest0 + i), xmm0);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest1 + i), xmm1);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest2 + i), xmm2);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDest3 + i), xmm3);
    }
#if defined(__clang__)
#pragma clang loop vectorize(disable)
#endif
    for (; i < nIters; ++i)
    {
        pabyDest0[i] = pabySrc[4 * i + 0];
        pabyDest1[i] = pabySrc[4 * i + 1];
        pabyDest2[i] = pabySrc[4 * i + 2];
        pabyDest3[i] = pabySrc[4 * i + 3];
    }
}
#endif

/************************************************************************/
/*                  GDALDeinterleave3UInt16_SSSE3()                     */
/************************************************************************/

#if (defined(__GNUC__) && !defined(__clang__)) ||                              \
    defined(__INTEL_CLANG_COMPILER)
#if !defined(__INTEL_CLANG_COMPILER)
// GCC autovectorizer does an excellent job
__attribute__((optimize("tree-vectorize")))
#endif
void GDALDeinterleave3UInt16_SSSE3(const GUInt16* CPL_RESTRICT panSrc,
                                  GUInt16* CPL_RESTRICT panDest0,
                                  GUInt16* CPL_RESTRICT panDest1,
                                  GUInt16* CPL_RESTRICT panDest2,
                                  size_t nIters)
{
    for (size_t i = 0; i < nIters; ++i)
    {
        panDest0[i] = panSrc[3 * i + 0];
        panDest1[i] = panSrc[3 * i + 1];
        panDest2[i] = panSrc[3 * i + 2];
    }
}
#endif

/************************************************************************/
/*                  GDALDeinterleave4UInt16_SSSE3()                     */
/************************************************************************/

#if (defined(__GNUC__) && !defined(__clang__)) ||                              \
    defined(__INTEL_CLANG_COMPILER)
#if !defined(__INTEL_CLANG_COMPILER)
// GCC autovectorizer does an excellent job
__attribute__((optimize("tree-vectorize")))
#endif
void GDALDeinterleave4UInt16_SSSE3(const GUInt16* CPL_RESTRICT panSrc,
                                  GUInt16* CPL_RESTRICT panDest0,
                                  GUInt16* CPL_RESTRICT panDest1,
                                  GUInt16* CPL_RESTRICT panDest2,
                                  GUInt16* CPL_RESTRICT panDest3,
                                  size_t nIters)
{
    for (size_t i = 0; i < nIters; ++i)
    {
        panDest0[i] = panSrc[4 * i + 0];
        panDest1[i] = panSrc[4 * i + 1];
        panDest2[i] = panSrc[4 * i + 2];
        panDest3[i] = panSrc[4 * i + 3];
    }
}
#endif

#endif  // HAVE_SSSE3_AT_COMPILE_TIME
