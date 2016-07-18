/******************************************************************************
 * Project:  GDAL
 * Purpose:  AVX2 emulation with SSE2
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

typedef struct
{
    __m128i low;
    __m128i high;
} __m256i;

static inline __m256i _mm256_set1_epi8(char c)
{
    __m256i reg;
    reg.low = _mm_set1_epi8(c);
    reg.high = _mm_set1_epi8(c);
    return reg;
}

static inline __m256i _mm256_setzero_si256()
{
    __m256i reg;
    reg.low = _mm_setzero_si128();
    reg.high = _mm_setzero_si128();
    return reg;
}

static inline __m256i _mm256_load_si256(__m256i const * p)
{
    __m256i reg;
    reg.low = _mm_load_si128((__m128i const*)p);
    reg.high = _mm_load_si128((__m128i const*)((char*)p+16));
    return reg;
}

static inline void _mm256_store_si256(__m256i * p, __m256i reg)
{
    _mm_store_si128((__m128i*)p, reg.low);
    _mm_store_si128((__m128i*)((char*)p+16), reg.high);
}

static inline void _mm256_storeu_si256(__m256i * p, __m256i reg)
{
    _mm_storeu_si128((__m128i*)p, reg.low);
    _mm_storeu_si128((__m128i*)((char*)p+16), reg.high);
}

static inline __m256i _mm256_cmpeq_epi8(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_cmpeq_epi8(r1.low, r2.low);
    reg.high = _mm_cmpeq_epi8(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_sad_epu8(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_sad_epu8(r1.low, r2.low);
    reg.high = _mm_sad_epu8(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_add_epi32(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_add_epi32(r1.low, r2.low);
    reg.high = _mm_add_epi32(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_andnot_si256(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_andnot_si128(r1.low, r2.low);
    reg.high = _mm_andnot_si128(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_and_si256(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_and_si128(r1.low, r2.low);
    reg.high = _mm_and_si128(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_or_si256(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_or_si128(r1.low, r2.low);
    reg.high = _mm_or_si128(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_min_epu8(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_min_epu8(r1.low, r2.low);
    reg.high = _mm_min_epu8(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_max_epu8(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_max_epu8(r1.low, r2.low);
    reg.high = _mm_max_epu8(r1.high, r2.high);
    return reg;
}

static inline __m128i _mm256_extracti128_si256(__m256i reg, int index)
{
    return (index == 0) ? reg.low : reg.high;
}

static inline __m256i _mm256_cvtepu8_epi16(__m128i reg128)
{
    __m256i reg;
    reg.low = _mm_unpacklo_epi8(reg128, _mm_setzero_si128());
    reg.high = _mm_unpacklo_epi8(_mm_shuffle_epi32(reg128, 2 | (3 << 2)),
                                 _mm_setzero_si128());
    return reg;
}

static inline __m256i _mm256_madd_epi16(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_madd_epi16(r1.low, r2.low);
    reg.high = _mm_madd_epi16(r1.high, r2.high);
    return reg;
}

#ifdef __SSE4_1__
#include <smmintrin.h>
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

static inline __m128i _mm_min_epu16 (__m128i x, __m128i y)
{
    const __m128i mask = GDALAVX2Emul_mm_cmple_epu16(x, y);
    return GDALAVX2Emul_mm_ternary(mask, x, y);
}

static inline __m128i _mm_max_epu16 (__m128i x, __m128i y)
{
    const __m128i mask = GDALAVX2Emul_mm_cmple_epu16(x, y);
    return GDALAVX2Emul_mm_ternary(mask, y, x);
}


#if defined(__GNUC__)
#define GDALAVX2EMUL_ALIGNED_16(x) x __attribute__ ((aligned (16)))
#elif defined(_MSC_VER)
#define GDALAVX2EMUL_ALIGNED_16(x) __declspec(align(16)) x
#else
#error "unsupported compiler"
#endif

static inline __m128i _mm_mullo_epi32 (__m128i x, __m128i y)
{
    GDALAVX2EMUL_ALIGNED_16(unsigned int x_scalar[4]);
    GDALAVX2EMUL_ALIGNED_16(unsigned int y_scalar[4]);
    _mm_store_si128( (__m128i*)x_scalar, x );
    _mm_store_si128( (__m128i*)y_scalar, y );
    x_scalar[0] = x_scalar[0] * y_scalar[0];
    x_scalar[1] = x_scalar[1] * y_scalar[1];
    x_scalar[2] = x_scalar[2] * y_scalar[2];
    x_scalar[3] = x_scalar[3] * y_scalar[3];
    return _mm_load_si128(  (__m128i*)x_scalar );
}
#endif


static inline __m256i _mm256_min_epu16(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_min_epu16(r1.low, r2.low);
    reg.high = _mm_min_epu16(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_max_epu16(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_max_epu16(r1.low, r2.low);
    reg.high = _mm_max_epu16(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_cvtepu16_epi32(__m128i reg128)
{
    __m256i reg;
    reg.low = _mm_unpacklo_epi16(reg128, _mm_setzero_si128());
    reg.high = _mm_unpacklo_epi16(_mm_shuffle_epi32(reg128, 2 | (3 << 2)),
                                  _mm_setzero_si128());
    return reg;
}

static inline __m256i _mm256_cvtepu16_epi64(__m128i reg128)
{
    __m256i reg;
    reg.low = _mm_unpacklo_epi32(_mm_unpacklo_epi16(reg128,
                                                    _mm_setzero_si128()),
                                 _mm_setzero_si128());
    reg.high = _mm_unpacklo_epi32(_mm_unpacklo_epi16(
                                     _mm_srli_si128(reg128, 4),
                                                    _mm_setzero_si128()),
                                     _mm_setzero_si128());
    return reg;
}

static inline __m256i _mm256_cvtepu32_epi64(__m128i reg128)
{
    __m256i reg;
    reg.low = _mm_unpacklo_epi32(reg128, _mm_setzero_si128());
    reg.high = _mm_unpacklo_epi32(_mm_shuffle_epi32(reg128, 2 | (3 << 2)),
                                  _mm_setzero_si128());
    return reg;
}

static inline __m256i _mm256_mullo_epi32(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_mullo_epi32(r1.low, r2.low);
    reg.high = _mm_mullo_epi32(r1.high, r2.high);
    return reg;
}

static inline __m256i _mm256_add_epi64(__m256i r1, __m256i r2)
{
    __m256i reg;
    reg.low = _mm_add_epi64(r1.low, r2.low);
    reg.high = _mm_add_epi64(r1.high, r2.high);
    return reg;
}

#endif /* GDAL_AVX2_EMULATION_H_INCLUDED */
