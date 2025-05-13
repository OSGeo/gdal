/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  SSSE3 specializations
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#include <algorithm>

#if (defined(HAVE_SSSE3_AT_COMPILE_TIME) &&                                    \
     (defined(__x86_64) || defined(_M_X64))) ||                                \
    defined(USE_NEON_OPTIMIZATIONS)

#include "rasterio_ssse3.h"

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"
#else
#include <tmmintrin.h>
#endif

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
/*                     GDALTranspose4x4Int32()                          */
/************************************************************************/

// Consider that the input registers for 4x4 words of size 4 bytes each,
// Return the transposition of this 4x4 matrix
// Considering that in0 = (in00, in01, in02, in03)
// Considering that in1 = (in10, in11, in12, in13)
// Considering that in2 = (in20, in21, in22, in23)
// Considering that in3 = (in30, in31, in32, in33)
// Return          out0 = (in00, in10, in20, in30)
// Return          out1 = (in01, in11, in21, in31)
// Return          out2 = (in02, in12, in22, in32)
// Return          out3 = (in03, in13, in23, in33)
inline void GDALTranspose4x4Int32(__m128i in0, __m128i in1, __m128i in2,
                                  __m128i in3, __m128i &out0, __m128i &out1,
                                  __m128i &out2, __m128i &out3)
{
    __m128i tmp0 = _mm_unpacklo_epi32(in0, in1);  // (in00, in10, in01, in11)
    __m128i tmp1 = _mm_unpackhi_epi32(in0, in1);  // (in02, in12, in03, in13)
    __m128i tmp2 = _mm_unpacklo_epi32(in2, in3);  // (in20, in30, in21, in31)
    __m128i tmp3 = _mm_unpackhi_epi32(in2, in3);  // (in22, in32, in23, in33)

    out0 = _mm_unpacklo_epi64(tmp0, tmp2);  // (in00, in10, in20, in30)
    out1 = _mm_unpackhi_epi64(tmp0, tmp2);  // (in01, in11, in21, in31)
    out2 = _mm_unpacklo_epi64(tmp1, tmp3);  // (in02, in12, in22, in32)
    out3 = _mm_unpackhi_epi64(tmp1, tmp3);  // (in03, in13, in23, in33)
}

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

        GDALTranspose4x4Int32(xmm0, xmm1, xmm2, xmm3, xmm0, xmm1, xmm2, xmm3);

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

/************************************************************************/
/*                               loadu()                                */
/************************************************************************/

inline __m128i loadu(const uint8_t *pSrc, size_t i, size_t srcStride)
{
    return _mm_loadu_si128(
        reinterpret_cast<const __m128i *>(pSrc + i * srcStride));
}

/************************************************************************/
/*                               storeu()                               */
/************************************************************************/

inline void storeu(uint8_t *pDst, size_t i, size_t dstStride, __m128i reg)
{
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + i * dstStride), reg);
}

/************************************************************************/
/*                      GDALInterleave3Byte_SSSE3()                     */
/************************************************************************/

#if (!defined(__GNUC__) || defined(__INTEL_CLANG_COMPILER))

inline __m128i GDAL_mm_or_3_si128(__m128i r0, __m128i r1, __m128i r2)
{
    return _mm_or_si128(_mm_or_si128(r0, r1), r2);
}

// ICC autovectorizer doesn't do a good job at generating good SSE code,
// at least with icx 2024.0.2.20231213, but it nicely unrolls the below loop.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void
GDALInterleave3Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                          uint8_t *CPL_RESTRICT pDst, size_t nIters)
{
    size_t i = 0;
    constexpr size_t VALS_PER_ITER = 16;

    if (nIters >= VALS_PER_ITER)
    {
        // clang-format off
        constexpr char X = -1;
        // How to dispatch 16 values of row=0 onto 3x16 bytes
        const __m128i xmm_shuffle00 = _mm_setr_epi8(0, X, X,
                                                    1, X, X,
                                                    2, X, X,
                                                    3, X, X,
                                                    4, X, X,
                                                    5);
        const __m128i xmm_shuffle01 = _mm_setr_epi8(   X, X,
                                                    6, X, X,
                                                    7, X, X,
                                                    8, X, X,
                                                    9, X, X,
                                                    10,X);
        const __m128i xmm_shuffle02 = _mm_setr_epi8(       X,
                                                    11, X, X,
                                                    12, X, X,
                                                    13, X, X,
                                                    14, X, X,
                                                    15, X, X);

        // How to dispatch 16 values of row=1 onto 3x16 bytes
        const __m128i xmm_shuffle10 = _mm_setr_epi8(X, 0, X,
                                                    X, 1, X,
                                                    X, 2, X,
                                                    X, 3, X,
                                                    X, 4, X,
                                                    X);
        const __m128i xmm_shuffle11 = _mm_setr_epi8(   5, X,
                                                    X, 6, X,
                                                    X, 7, X,
                                                    X, 8, X,
                                                    X, 9, X,
                                                    X,10);
        const __m128i xmm_shuffle12 = _mm_setr_epi8(       X,
                                                    X, 11, X,
                                                    X, 12, X,
                                                    X, 13, X,
                                                    X, 14, X,
                                                    X, 15, X);

        // How to dispatch 16 values of row=2 onto 3x16 bytes
        const __m128i xmm_shuffle20 = _mm_setr_epi8(X, X, 0,
                                                    X, X, 1,
                                                    X, X, 2,
                                                    X, X, 3,
                                                    X, X, 4,
                                                    X);
        const __m128i xmm_shuffle21 = _mm_setr_epi8(   X, 5,
                                                    X, X, 6,
                                                    X, X, 7,
                                                    X, X, 8,
                                                    X, X, 9,
                                                    X, X);
        const __m128i xmm_shuffle22 = _mm_setr_epi8(      10,
                                                    X, X, 11,
                                                    X, X, 12,
                                                    X, X, 13,
                                                    X, X, 14,
                                                    X, X, 15);
        // clang-format on

        for (; i + VALS_PER_ITER <= nIters; i += VALS_PER_ITER)
        {
#define LOAD(x) __m128i xmm##x = loadu(pSrc + i, x, nIters)
            LOAD(0);
            LOAD(1);
            LOAD(2);

#define SHUFFLE(x, y) _mm_shuffle_epi8(xmm##y, xmm_shuffle##y##x)
#define COMBINE_3(x)                                                           \
    GDAL_mm_or_3_si128(SHUFFLE(x, 0), SHUFFLE(x, 1), SHUFFLE(x, 2))

#define STORE(x)                                                               \
    storeu(pDst, 3 * (i / VALS_PER_ITER) + x, VALS_PER_ITER, COMBINE_3(x))
            STORE(0);
            STORE(1);
            STORE(2);
#undef LOAD
#undef COMBINE_3
#undef SHUFFLE
#undef STORE
        }
    }

    for (; i < nIters; ++i)
    {
#define INTERLEAVE(x) pDst[3 * i + x] = pSrc[i + x * nIters]
        INTERLEAVE(0);
        INTERLEAVE(1);
        INTERLEAVE(2);
#undef INTERLEAVE
    }
}

#else

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("tree-vectorize")))
#endif
#if defined(__GNUC__)
__attribute__((noinline))
#endif
#if defined(__clang__) && !defined(__INTEL_CLANG_COMPILER)
// clang++ -O2 -fsanitize=undefined fails to vectorize, ignore that warning
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpass-failed"
#endif
static void
GDALInterleave3Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                          uint8_t *CPL_RESTRICT pDst, size_t nIters)
{
#if defined(__clang__) && !defined(__INTEL_CLANG_COMPILER)
#pragma clang loop vectorize(enable)
#endif
    for (size_t i = 0; i < nIters; ++i)
    {
        pDst[3 * i + 0] = pSrc[i + 0 * nIters];
        pDst[3 * i + 1] = pSrc[i + 1 * nIters];
        pDst[3 * i + 2] = pSrc[i + 2 * nIters];
    }
}
#if defined(__clang__) && !defined(__INTEL_CLANG_COMPILER)
#pragma clang diagnostic pop
#endif

#endif

/************************************************************************/
/*                      GDALInterleave5Byte_SSSE3()                     */
/************************************************************************/

inline __m128i GDAL_mm_or_5_si128(__m128i r0, __m128i r1, __m128i r2,
                                  __m128i r3, __m128i r4)
{
    return _mm_or_si128(
        _mm_or_si128(_mm_or_si128(r0, r1), _mm_or_si128(r2, r3)), r4);
}

static void GDALInterleave5Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                                      uint8_t *CPL_RESTRICT pDst, size_t nIters)
{
    size_t i = 0;
    constexpr size_t VALS_PER_ITER = 16;

    if (nIters >= VALS_PER_ITER)
    {
        // clang-format off
        constexpr char X = -1;
        // How to dispatch 16 values of row=0 onto 5x16 bytes
        const __m128i xmm_shuffle00 = _mm_setr_epi8(0, X, X, X, X,
                                                    1, X, X, X, X,
                                                    2, X, X, X, X,
                                                    3);
        const __m128i xmm_shuffle01 = _mm_setr_epi8(   X, X, X, X,
                                                    4, X, X, X, X,
                                                    5, X, X, X, X,
                                                    6, X);
        const __m128i xmm_shuffle02 = _mm_setr_epi8(      X, X, X,
                                                    7, X, X, X, X,
                                                    8, X, X, X, X,
                                                    9, X, X);
        const __m128i xmm_shuffle03 = _mm_setr_epi8(          X, X,
                                                    10, X, X, X, X,
                                                    11, X, X, X, X,
                                                    12, X, X, X);
        const __m128i xmm_shuffle04 = _mm_setr_epi8(             X,
                                                    13, X, X, X, X,
                                                    14, X, X, X, X,
                                                    15, X, X, X, X);

        // How to dispatch 16 values of row=1 onto 5x16 bytes
        const __m128i xmm_shuffle10 = _mm_setr_epi8(X, 0, X, X, X,
                                                    X, 1, X, X, X,
                                                    X, 2, X, X, X,
                                                    X);
        const __m128i xmm_shuffle11 = _mm_setr_epi8(   3, X, X, X,
                                                    X, 4, X, X, X,
                                                    X, 5, X, X, X,
                                                    X, 6);
        const __m128i xmm_shuffle12 = _mm_setr_epi8(      X, X, X,
                                                    X, 7, X, X, X,
                                                    X, 8, X, X, X,
                                                    X, 9, X);
        const __m128i xmm_shuffle13 = _mm_setr_epi8(          X, X,
                                                    X, 10, X, X, X,
                                                    X, 11, X, X, X,
                                                    X, 12, X, X);
        const __m128i xmm_shuffle14 = _mm_setr_epi8(             X,
                                                    X, 13, X, X, X,
                                                    X, 14, X, X, X,
                                                    X, 15, X, X, X);

        // How to dispatch 16 values of row=2 onto 5x16 bytes
        const __m128i xmm_shuffle20 = _mm_setr_epi8(X, X, 0, X, X,
                                                    X, X, 1, X, X,
                                                    X, X, 2, X, X,
                                                    X);
        const __m128i xmm_shuffle21 = _mm_setr_epi8(   X, 3, X, X,
                                                    X, X, 4, X, X,
                                                    X, X, 5, X, X,
                                                    X, X);
        const __m128i xmm_shuffle22 = _mm_setr_epi8(      6, X, X,
                                                    X, X, 7, X, X,
                                                    X, X, 8, X, X,
                                                    X, X, 9);
        const __m128i xmm_shuffle23 = _mm_setr_epi8(          X, X,
                                                    X, X, 10, X, X,
                                                    X, X, 11, X, X,
                                                    X, X, 12, X);
        const __m128i xmm_shuffle24 = _mm_setr_epi8(             X,
                                                    X, X, 13, X, X,
                                                    X, X, 14, X, X,
                                                    X, X, 15, X, X);

        // How to dispatch 16 values of row=3 onto 5x16 bytes
        const __m128i xmm_shuffle30 = _mm_setr_epi8(X, X, X, 0, X,
                                                    X, X, X, 1, X,
                                                    X, X, X, 2, X,
                                                    X);
        const __m128i xmm_shuffle31 = _mm_setr_epi8(   X, X, 3, X,
                                                    X, X, X, 4, X,
                                                    X, X, X, 5, X,
                                                    X, X);
        const __m128i xmm_shuffle32 = _mm_setr_epi8(      X, 6, X,
                                                    X, X, X, 7, X,
                                                    X, X, X, 8, X,
                                                    X, X, X);
        const __m128i xmm_shuffle33 = _mm_setr_epi8(          9, X,
                                                    X, X, X, 10, X,
                                                    X, X, X, 11, X,
                                                    X, X, X, 12);
        const __m128i xmm_shuffle34 = _mm_setr_epi8(             X,
                                                    X, X, X, 13, X,
                                                    X, X, X, 14, X,
                                                    X, X, X, 15, X);

        // How to dispatch 16 values of row=4 onto 5x16 bytes
        const __m128i xmm_shuffle40 = _mm_setr_epi8(X, X, X, X, 0,
                                                    X, X, X, X, 1,
                                                    X, X, X, X, 2,
                                                    X);
        const __m128i xmm_shuffle41 = _mm_setr_epi8(   X, X, X, 3,
                                                    X, X, X, X, 4,
                                                    X, X, X, X, 5,
                                                    X, X);
        const __m128i xmm_shuffle42 = _mm_setr_epi8(      X, X, 6,
                                                    X, X, X, X, 7,
                                                    X, X, X, X, 8,
                                                    X, X, X);
        const __m128i xmm_shuffle43 = _mm_setr_epi8(         X,  9,
                                                    X, X, X, X, 10,
                                                    X, X, X, X, 11,
                                                    X, X, X, X);
        const __m128i xmm_shuffle44 = _mm_setr_epi8(            12,
                                                    X, X, X, X, 13,
                                                    X, X, X, X, 14,
                                                    X, X, X, X, 15);
        // clang-format on

        for (; i + VALS_PER_ITER <= nIters; i += VALS_PER_ITER)
        {
#define LOAD(x) __m128i xmm##x = loadu(pSrc + i, x, nIters)
            LOAD(0);
            LOAD(1);
            LOAD(2);
            LOAD(3);
            LOAD(4);

#define SHUFFLE(x, y) _mm_shuffle_epi8(xmm##y, xmm_shuffle##y##x)
#define COMBINE_5(x)                                                           \
    GDAL_mm_or_5_si128(SHUFFLE(x, 0), SHUFFLE(x, 1), SHUFFLE(x, 2),            \
                       SHUFFLE(x, 3), SHUFFLE(x, 4))

#define STORE(x)                                                               \
    storeu(pDst, 5 * (i / VALS_PER_ITER) + x, VALS_PER_ITER, COMBINE_5(x))
            STORE(0);
            STORE(1);
            STORE(2);
            STORE(3);
            STORE(4);
#undef LOAD
#undef COMBINE_5
#undef SHUFFLE
#undef STORE
        }
    }

    for (; i < nIters; ++i)
    {
#define INTERLEAVE(x) pDst[5 * i + x] = pSrc[i + x * nIters]
        INTERLEAVE(0);
        INTERLEAVE(1);
        INTERLEAVE(2);
        INTERLEAVE(3);
        INTERLEAVE(4);
#undef INTERLEAVE
    }
}

/************************************************************************/
/*                      GDALTranspose2D_Byte_SSSE3()                    */
/************************************************************************/

// Given r = (b00, b01, b02, b03,
//            b10, b11, b12, b13,
//            b20, b21, b22, b23,
//            b30, b31, b32, b33)
// Return    (b00, b10, b20, b30,
//            b01, b11, b21, b31,
//            b02, b12, b22, b32,
//            b03, b13, b22, b33)
inline void GDALReorderForTranspose4x4(__m128i &r)
{
    const __m128i shuffle_mask =
        _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);

    r = _mm_shuffle_epi8(r, shuffle_mask);
}

// Transpose the 16x16 byte values contained in the 16 SSE registers
inline void GDALTranspose16x16ByteBlock_SSSE3(
    __m128i &r00, __m128i &r01, __m128i &r02, __m128i &r03, __m128i &r04,
    __m128i &r05, __m128i &r06, __m128i &r07, __m128i &r08, __m128i &r09,
    __m128i &r10, __m128i &r11, __m128i &r12, __m128i &r13, __m128i &r14,
    __m128i &r15)
{
    __m128i tmp00, tmp01, tmp02, tmp03;
    __m128i tmp10, tmp11, tmp12, tmp13;
    __m128i tmp20, tmp21, tmp22, tmp23;
    __m128i tmp30, tmp31, tmp32, tmp33;

    GDALTranspose4x4Int32(r00, r01, r02, r03, tmp00, tmp01, tmp02, tmp03);
    GDALTranspose4x4Int32(r04, r05, r06, r07, tmp10, tmp11, tmp12, tmp13);
    GDALTranspose4x4Int32(r08, r09, r10, r11, tmp20, tmp21, tmp22, tmp23);
    GDALTranspose4x4Int32(r12, r13, r14, r15, tmp30, tmp31, tmp32, tmp33);

    GDALReorderForTranspose4x4(tmp00);
    GDALReorderForTranspose4x4(tmp01);
    GDALReorderForTranspose4x4(tmp02);
    GDALReorderForTranspose4x4(tmp03);
    GDALReorderForTranspose4x4(tmp10);
    GDALReorderForTranspose4x4(tmp11);
    GDALReorderForTranspose4x4(tmp12);
    GDALReorderForTranspose4x4(tmp13);
    GDALReorderForTranspose4x4(tmp20);
    GDALReorderForTranspose4x4(tmp21);
    GDALReorderForTranspose4x4(tmp22);
    GDALReorderForTranspose4x4(tmp23);
    GDALReorderForTranspose4x4(tmp30);
    GDALReorderForTranspose4x4(tmp31);
    GDALReorderForTranspose4x4(tmp32);
    GDALReorderForTranspose4x4(tmp33);

    GDALTranspose4x4Int32(tmp00, tmp10, tmp20, tmp30, r00, r01, r02, r03);
    GDALTranspose4x4Int32(tmp01, tmp11, tmp21, tmp31, r04, r05, r06, r07);
    GDALTranspose4x4Int32(tmp02, tmp12, tmp22, tmp32, r08, r09, r10, r11);
    GDALTranspose4x4Int32(tmp03, tmp13, tmp23, tmp33, r12, r13, r14, r15);
}

inline void GDALTranspose2D16x16Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                                           uint8_t *CPL_RESTRICT pDst,
                                           size_t srcStride, size_t dstStride)
{
#define LOAD(x) __m128i r##x = loadu(pSrc, x, srcStride)
    LOAD(0);
    LOAD(1);
    LOAD(2);
    LOAD(3);
    LOAD(4);
    LOAD(5);
    LOAD(6);
    LOAD(7);
    LOAD(8);
    LOAD(9);
    LOAD(10);
    LOAD(11);
    LOAD(12);
    LOAD(13);
    LOAD(14);
    LOAD(15);
#undef LOAD

    GDALTranspose16x16ByteBlock_SSSE3(r0, r1, r2, r3, r4, r5, r6, r7, r8, r9,
                                      r10, r11, r12, r13, r14, r15);

#define STORE(x) storeu(pDst, x, dstStride, r##x)
    STORE(0);
    STORE(1);
    STORE(2);
    STORE(3);
    STORE(4);
    STORE(5);
    STORE(6);
    STORE(7);
    STORE(8);
    STORE(9);
    STORE(10);
    STORE(11);
    STORE(12);
    STORE(13);
    STORE(14);
    STORE(15);
#undef STORE
}

void GDALTranspose2D_Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                                uint8_t *CPL_RESTRICT pDst, size_t nSrcWidth,
                                size_t nSrcHeight)
{
    if (nSrcHeight == 3)
    {
        GDALInterleave3Byte_SSSE3(pSrc, pDst, nSrcWidth);
    }
    else if (nSrcHeight == 5)
    {
        GDALInterleave5Byte_SSSE3(pSrc, pDst, nSrcWidth);
    }
    else
    {
        constexpr size_t blocksize = 16;
        for (size_t i = 0; i < nSrcHeight; i += blocksize)
        {
            const size_t max_k = std::min(i + blocksize, nSrcHeight);
            for (size_t j = 0; j < nSrcWidth; j += blocksize)
            {
                // transpose the block beginning at [i,j]
                const size_t max_l = std::min(j + blocksize, nSrcWidth);
                if (max_k - i == blocksize && max_l - j == blocksize)
                {
                    GDALTranspose2D16x16Byte_SSSE3(&pSrc[j + i * nSrcWidth],
                                                   &pDst[i + j * nSrcHeight],
                                                   nSrcWidth, nSrcHeight);
                }
                else
                {
                    for (size_t k = i; k < max_k; ++k)
                    {
                        for (size_t l = j; l < max_l; ++l)
                        {
                            GDALCopyWord(pSrc[l + k * nSrcWidth],
                                         pDst[k + l * nSrcHeight]);
                        }
                    }
                }
            }
        }
    }
}

#endif  // HAVE_SSSE3_AT_COMPILE_TIME
