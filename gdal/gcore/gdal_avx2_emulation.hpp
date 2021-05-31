/******************************************************************************
 * Project:  GDAL
 * Purpose:  AVX2 emulation with SSE2 + a few SSE4.1 emulation
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

#ifndef GDAL_AVX2_EMULATION_H_INCLUDED
#define GDAL_AVX2_EMULATION_H_INCLUDED

#include <emmintrin.h>

#ifdef __SSE4_1__
#include <smmintrin.h>

#define GDALmm_min_epu16   _mm_min_epu16
#define GDALmm_max_epu16   _mm_max_epu16
#define GDALmm_mullo_epi32 _mm_mullo_epi32
#define GDALmm_cvtepu8_epi16  _mm_cvtepu8_epi16
#define GDALmm_cvtepu16_epi32 _mm_cvtepu16_epi32
#define GDALmm_cvtepu16_epi64 _mm_cvtepu16_epi64
#define GDALmm_cvtepu32_epi64 _mm_cvtepu32_epi64

#else
// Emulation of SSE4.1 _mm_min_epu16 and _mm_max_epu16 with SSE2 only

static inline __m128i GDALAVX2Emul_mm_cmple_epu16 (__m128i x, __m128i y)
{
    return _mm_cmpeq_epi16(_mm_subs_epu16(x, y), _mm_setzero_si128() );
}

static inline __m128i GDALAVX2Emul_mm_ternary(__m128i mask,
                                      __m128i then_reg,
                                      __m128i else_reg)
{
    return _mm_or_si128(_mm_and_si128(mask, then_reg),
                        _mm_andnot_si128(mask, else_reg));
}

static inline __m128i GDALmm_min_epu16 (__m128i x, __m128i y)
{
    const __m128i mask = GDALAVX2Emul_mm_cmple_epu16(x, y);
    return GDALAVX2Emul_mm_ternary(mask, x, y);
}

static inline __m128i GDALmm_max_epu16 (__m128i x, __m128i y)
{
    const __m128i mask = GDALAVX2Emul_mm_cmple_epu16(x, y);
    return GDALAVX2Emul_mm_ternary(mask, y, x);
}

static inline __m128i GDALmm_mullo_epi32 (__m128i x, __m128i y)
{
    const __m128i mul02 = _mm_shuffle_epi32(_mm_mul_epu32(x, y), 2 << 2);
    const __m128i mul13 = _mm_shuffle_epi32(_mm_mul_epu32(_mm_srli_si128(x, 4),
                                                          _mm_srli_si128(y, 4)),
                                            2 << 2);
    return _mm_unpacklo_epi32(mul02, mul13);;
}

static inline __m128i GDALmm_cvtepu8_epi16 (__m128i x)
{
    return _mm_unpacklo_epi8(x, _mm_setzero_si128());
}

static inline __m128i GDALmm_cvtepu16_epi32 (__m128i x)
{
    return _mm_unpacklo_epi16(x, _mm_setzero_si128());
}

static inline __m128i GDALmm_cvtepu16_epi64 (__m128i x)
{
    return _mm_unpacklo_epi32(_mm_unpacklo_epi16(x, _mm_setzero_si128()),
                              _mm_setzero_si128());
}

static inline __m128i GDALmm_cvtepu32_epi64 (__m128i x)
{
    return _mm_unpacklo_epi32(x, _mm_setzero_si128());
}

#endif // __SSE4_1__


#ifdef __AVX2__

#include <immintrin.h>

typedef __m256i GDALm256i;

#define GDALmm256_set1_epi8             _mm256_set1_epi8
#define GDALmm256_set1_epi16            _mm256_set1_epi16
#define GDALmm256_set1_epi32            _mm256_set1_epi32
#define GDALmm256_setzero_si256         _mm256_setzero_si256
#define GDALmm256_load_si256            _mm256_load_si256
#define GDALmm256_store_si256           _mm256_store_si256
#define GDALmm256_storeu_si256          _mm256_storeu_si256
#define GDALmm256_cmpeq_epi8            _mm256_cmpeq_epi8
#define GDALmm256_sad_epu8              _mm256_sad_epu8
#define GDALmm256_add_epi32             _mm256_add_epi32
#define GDALmm256_andnot_si256          _mm256_andnot_si256
#define GDALmm256_and_si256             _mm256_and_si256
#define GDALmm256_or_si256              _mm256_or_si256
#define GDALmm256_min_epu8              _mm256_min_epu8
#define GDALmm256_max_epu8              _mm256_max_epu8
#define GDALmm256_extracti128_si256     _mm256_extracti128_si256
#define GDALmm256_cvtepu8_epi16         _mm256_cvtepu8_epi16
#define GDALmm256_madd_epi16            _mm256_madd_epi16
#define GDALmm256_min_epu16             _mm256_min_epu16
#define GDALmm256_max_epu16             _mm256_max_epu16
#define GDALmm256_cvtepu16_epi32        _mm256_cvtepu16_epi32
#define GDALmm256_cvtepu16_epi64        _mm256_cvtepu16_epi64
#define GDALmm256_cvtepu32_epi64        _mm256_cvtepu32_epi64
#define GDALmm256_mullo_epi32           _mm256_mullo_epi32
#define GDALmm256_add_epi64             _mm256_add_epi64
#define GDALmm256_add_epi16             _mm256_add_epi16
#define GDALmm256_sub_epi16             _mm256_sub_epi16
#define GDALmm256_min_epi16             _mm256_min_epi16
#define GDALmm256_max_epi16             _mm256_max_epi16
#define GDALmm256_srli_epi16            _mm256_srli_epi16
#define GDALmm256_srli_epi32            _mm256_srli_epi32
#define GDALmm256_srli_epi64            _mm256_srli_epi64
#define GDALmm256_set1_epi64x           _mm256_set1_epi64x

#else

typedef struct
{
    __m128i low;
    __m128i high;
} GDALm256i;

static inline GDALm256i GDALmm256_set1_epi8(char c)
{
    GDALm256i reg;
    reg.low = _mm_set1_epi8(c);
    reg.high = _mm_set1_epi8(c);
    return reg;
}

static inline GDALm256i GDALmm256_set1_epi16(short s)
{
    GDALm256i reg;
    reg.low = _mm_set1_epi16(s);
    reg.high = _mm_set1_epi16(s);
    return reg;
}

static inline GDALm256i GDALmm256_set1_epi32(int i)
{
    GDALm256i reg;
    reg.low = _mm_set1_epi32(i);
    reg.high = _mm_set1_epi32(i);
    return reg;
}

static inline GDALm256i GDALmm256_set1_epi64x(long long i)
{
    GDALm256i reg;
    reg.low = _mm_set1_epi64x(i);
    reg.high = _mm_set1_epi64x(i);
    return reg;
}

static inline GDALm256i GDALmm256_setzero_si256()
{
    GDALm256i reg;
    reg.low = _mm_setzero_si128();
    reg.high = _mm_setzero_si128();
    return reg;
}

static inline GDALm256i GDALmm256_load_si256(GDALm256i const * p)
{
    GDALm256i reg;
    reg.low = _mm_load_si128(reinterpret_cast<__m128i const*>(p));
    reg.high = _mm_load_si128(reinterpret_cast<__m128i const*>(reinterpret_cast<const char*>(p)+16));
    return reg;
}

static inline void GDALmm256_store_si256(GDALm256i * p, GDALm256i reg)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(p), reg.low);
    _mm_store_si128(reinterpret_cast<__m128i*>(reinterpret_cast<char*>(p)+16), reg.high);
}

static inline void GDALmm256_storeu_si256(GDALm256i * p, GDALm256i reg)
{
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), reg.low);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(reinterpret_cast<char*>(p)+16), reg.high);
}

#define DEFINE_BINARY_MM256(mm256name, mm128name) \
static inline GDALm256i mm256name(GDALm256i r1, GDALm256i r2) \
{ \
    GDALm256i reg; \
    reg.low = mm128name(r1.low, r2.low); \
    reg.high = mm128name(r1.high, r2.high); \
    return reg; \
}

DEFINE_BINARY_MM256(GDALmm256_cmpeq_epi8, _mm_cmpeq_epi8)
DEFINE_BINARY_MM256(GDALmm256_sad_epu8, _mm_sad_epu8)
DEFINE_BINARY_MM256(GDALmm256_add_epi32, _mm_add_epi32)
DEFINE_BINARY_MM256(GDALmm256_andnot_si256, _mm_andnot_si128)
DEFINE_BINARY_MM256(GDALmm256_and_si256, _mm_and_si128)
DEFINE_BINARY_MM256(GDALmm256_or_si256, _mm_or_si128)
DEFINE_BINARY_MM256(GDALmm256_min_epu8, _mm_min_epu8)
DEFINE_BINARY_MM256(GDALmm256_max_epu8, _mm_max_epu8)
DEFINE_BINARY_MM256(GDALmm256_madd_epi16, _mm_madd_epi16)
DEFINE_BINARY_MM256(GDALmm256_min_epu16, GDALmm_min_epu16)
DEFINE_BINARY_MM256(GDALmm256_max_epu16, GDALmm_max_epu16)
DEFINE_BINARY_MM256(GDALmm256_mullo_epi32, GDALmm_mullo_epi32)
DEFINE_BINARY_MM256(GDALmm256_add_epi64, _mm_add_epi64)
DEFINE_BINARY_MM256(GDALmm256_add_epi16, _mm_add_epi16)
DEFINE_BINARY_MM256(GDALmm256_sub_epi16, _mm_sub_epi16)
DEFINE_BINARY_MM256(GDALmm256_min_epi16, _mm_min_epi16)
DEFINE_BINARY_MM256(GDALmm256_max_epi16, _mm_max_epi16)

static inline __m128i GDALmm256_extracti128_si256(GDALm256i reg, int index)
{
    return (index == 0) ? reg.low : reg.high;
}

#define DEFINE_CVTE_MM256(mm256name, mm128name) \
static inline GDALm256i mm256name(__m128i x) \
{ \
    GDALm256i reg; \
    reg.low = mm128name(x); \
    reg.high = mm128name(_mm_srli_si128(x, 8)); \
    return reg; \
}

DEFINE_CVTE_MM256(GDALmm256_cvtepu8_epi16, GDALmm_cvtepu8_epi16)
DEFINE_CVTE_MM256(GDALmm256_cvtepu16_epi32, GDALmm_cvtepu16_epi32)
DEFINE_CVTE_MM256(GDALmm256_cvtepu16_epi64, GDALmm_cvtepu16_epi64)
DEFINE_CVTE_MM256(GDALmm256_cvtepu32_epi64, GDALmm_cvtepu32_epi64)

static inline GDALm256i GDALmm256_srli_epi16(GDALm256i reg, int imm)
{
    GDALm256i ret;
    ret.low = _mm_srli_epi16(reg.low, imm);
    ret.high = _mm_srli_epi16(reg.high, imm);
    return ret;
}

static inline GDALm256i GDALmm256_srli_epi32(GDALm256i reg, int imm)
{
    GDALm256i ret;
    ret.low = _mm_srli_epi32(reg.low, imm);
    ret.high = _mm_srli_epi32(reg.high, imm);
    return ret;
}

static inline GDALm256i GDALmm256_srli_epi64(GDALm256i reg, int imm)
{
    GDALm256i ret;
    ret.low = _mm_srli_epi64(reg.low, imm);
    ret.high = _mm_srli_epi64(reg.high, imm);
    return ret;
}

#endif

#endif /* GDAL_AVX2_EMULATION_H_INCLUDED */
