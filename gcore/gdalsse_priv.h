/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  SSE2 helper
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALSSE_PRIV_H_INCLUDED
#define GDALSSE_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* Could possibly be used too on 32bit, but we would need to check at runtime */
#if (defined(__x86_64) || defined(_M_X64) || defined(USE_SSE2)) &&             \
    !defined(USE_SSE2_EMULATION)

#include <string.h>

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"
#else
/* Requires SSE2 */
#include <emmintrin.h>

#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
#include <smmintrin.h>
#endif
#endif

#include "gdal_priv_templates.hpp"

static inline __m128i GDALCopyInt16ToXMM(const void *ptr)
{
    unsigned short s;
    memcpy(&s, ptr, 2);
    return _mm_cvtsi32_si128(s);
}

static inline __m128i GDALCopyInt32ToXMM(const void *ptr)
{
    GInt32 i;
    memcpy(&i, ptr, 4);
    return _mm_cvtsi32_si128(i);
}

static inline __m128i GDALCopyInt64ToXMM(const void *ptr)
{
#if defined(__i386__) || defined(_M_IX86)
    return _mm_loadl_epi64(static_cast<const __m128i *>(ptr));
#else
    GInt64 i;
    memcpy(&i, ptr, 8);
    return _mm_cvtsi64_si128(i);
#endif
}

#ifndef GDALCopyXMMToInt16_defined
#define GDALCopyXMMToInt16_defined

static inline void GDALCopyXMMToInt16(const __m128i xmm, void *pDest)
{
    GInt16 i = static_cast<GInt16>(_mm_extract_epi16(xmm, 0));
    memcpy(pDest, &i, 2);
}
#endif

class XMMReg4Int;

class XMMReg4Double;

class XMMReg4Float
{
  public:
    __m128 xmm;

    XMMReg4Float()
#if !defined(_MSC_VER)
        : xmm(_mm_undefined_ps())
#endif
    {
    }

    XMMReg4Float(const XMMReg4Float &other) : xmm(other.xmm)
    {
    }

    static inline XMMReg4Float Zero()
    {
        XMMReg4Float reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg4Float Set1(float f)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_set1_ps(f);
        return reg;
    }

    static inline XMMReg4Float Load4Val(const float *ptr)
    {
        XMMReg4Float reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    static inline XMMReg4Float Load4Val(const unsigned char *ptr)
    {
        XMMReg4Float reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    static inline XMMReg4Float Load4Val(const short *ptr)
    {
        XMMReg4Float reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    static inline XMMReg4Float Load4Val(const unsigned short *ptr)
    {
        XMMReg4Float reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    static inline XMMReg4Float Load4Val(const int *ptr)
    {
        XMMReg4Float reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    static inline XMMReg4Float Equals(const XMMReg4Float &expr1,
                                      const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_cmpeq_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float NotEquals(const XMMReg4Float &expr1,
                                         const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_cmpneq_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float Lesser(const XMMReg4Float &expr1,
                                      const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_cmplt_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float Greater(const XMMReg4Float &expr1,
                                       const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_cmpgt_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float And(const XMMReg4Float &expr1,
                                   const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_and_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float Ternary(const XMMReg4Float &cond,
                                       const XMMReg4Float &true_expr,
                                       const XMMReg4Float &false_expr)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_or_ps(_mm_and_ps(cond.xmm, true_expr.xmm),
                            _mm_andnot_ps(cond.xmm, false_expr.xmm));
        return reg;
    }

    static inline XMMReg4Float Min(const XMMReg4Float &expr1,
                                   const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_min_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Float Max(const XMMReg4Float &expr1,
                                   const XMMReg4Float &expr2)
    {
        XMMReg4Float reg;
        reg.xmm = _mm_max_ps(expr1.xmm, expr2.xmm);
        return reg;
    }

    inline void nsLoad4Val(const float *ptr)
    {
        xmm = _mm_loadu_ps(ptr);
    }

    static inline void Load16Val(const float *ptr, XMMReg4Float &r0,
                                 XMMReg4Float &r1, XMMReg4Float &r2,
                                 XMMReg4Float &r3)
    {
        r0.nsLoad4Val(ptr);
        r1.nsLoad4Val(ptr + 4);
        r2.nsLoad4Val(ptr + 8);
        r3.nsLoad4Val(ptr + 12);
    }

    inline void nsLoad4Val(const int *ptr)
    {
        const __m128i xmm_i =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        xmm = _mm_cvtepi32_ps(xmm_i);
    }

    static inline void Load16Val(const int *ptr, XMMReg4Float &r0,
                                 XMMReg4Float &r1, XMMReg4Float &r2,
                                 XMMReg4Float &r3)
    {
        r0.nsLoad4Val(ptr);
        r1.nsLoad4Val(ptr + 4);
        r2.nsLoad4Val(ptr + 8);
        r3.nsLoad4Val(ptr + 12);
    }

    static inline __m128i cvtepu8_epi32(__m128i x)
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_cvtepu8_epi32(x);
#else
        return _mm_unpacklo_epi16(_mm_unpacklo_epi8(x, _mm_setzero_si128()),
                                  _mm_setzero_si128());
#endif
    }

    inline void nsLoad4Val(const unsigned char *ptr)
    {
        const __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
        xmm = _mm_cvtepi32_ps(cvtepu8_epi32(xmm_i));
    }

    static inline void Load8Val(const unsigned char *ptr, XMMReg4Float &r0,
                                XMMReg4Float &r1)
    {
        const __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        r0.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(xmm_i));
        r1.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(_mm_srli_si128(xmm_i, 4)));
    }

    static inline void Load16Val(const unsigned char *ptr, XMMReg4Float &r0,
                                 XMMReg4Float &r1, XMMReg4Float &r2,
                                 XMMReg4Float &r3)
    {
        const __m128i xmm_i =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        r0.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(xmm_i));
        r1.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(_mm_srli_si128(xmm_i, 4)));
        r2.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(_mm_srli_si128(xmm_i, 8)));
        r3.xmm = _mm_cvtepi32_ps(cvtepu8_epi32(_mm_srli_si128(xmm_i, 12)));
    }

    static inline __m128i cvtepi16_epi32(__m128i x)
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_cvtepi16_epi32(x);
#else
        /* 0|0|0|0|b|b|a|a --> 0|0|0|0|sign(b)|b|sign(a)|a */
        return _mm_srai_epi32(
            /* 0|0|0|0|0|0|b|a --> 0|0|0|0|b|b|a|a */
            _mm_unpacklo_epi16(x, x), 16);
#endif
    }

    inline void nsLoad4Val(const short *ptr)
    {
        const __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm = _mm_cvtepi32_ps(cvtepi16_epi32(xmm_i));
    }

    static inline void Load8Val(const short *ptr, XMMReg4Float &r0,
                                XMMReg4Float &r1)
    {
        const __m128i xmm_i =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        r0.xmm = _mm_cvtepi32_ps(cvtepi16_epi32(xmm_i));
        r1.xmm = _mm_cvtepi32_ps(cvtepi16_epi32(_mm_srli_si128(xmm_i, 8)));
    }

    static inline void Load16Val(const short *ptr, XMMReg4Float &r0,
                                 XMMReg4Float &r1, XMMReg4Float &r2,
                                 XMMReg4Float &r3)
    {
        Load8Val(ptr, r0, r1);
        Load8Val(ptr + 8, r2, r3);
    }

    static inline __m128i cvtepu16_epi32(__m128i x)
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_cvtepu16_epi32(x);
#else
        return _mm_unpacklo_epi16(
            x, _mm_setzero_si128()); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|0|b|0|a */
#endif
    }

    inline void nsLoad4Val(const unsigned short *ptr)
    {
        const __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm = _mm_cvtepi32_ps(cvtepu16_epi32(xmm_i));
    }

    static inline void Load8Val(const unsigned short *ptr, XMMReg4Float &r0,
                                XMMReg4Float &r1)
    {
        const __m128i xmm_i =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        r0.xmm = _mm_cvtepi32_ps(cvtepu16_epi32(xmm_i));
        r1.xmm = _mm_cvtepi32_ps(cvtepu16_epi32(_mm_srli_si128(xmm_i, 8)));
    }

    static inline void Load16Val(const unsigned short *ptr, XMMReg4Float &r0,
                                 XMMReg4Float &r1, XMMReg4Float &r2,
                                 XMMReg4Float &r3)
    {
        Load8Val(ptr, r0, r1);
        Load8Val(ptr + 8, r2, r3);
    }

    inline void Zeroize()
    {
        xmm = _mm_setzero_ps();
    }

    inline XMMReg4Float &operator=(const XMMReg4Float &other)
    {
        xmm = other.xmm;
        return *this;
    }

    inline XMMReg4Float &operator+=(const XMMReg4Float &other)
    {
        xmm = _mm_add_ps(xmm, other.xmm);
        return *this;
    }

    inline XMMReg4Float &operator-=(const XMMReg4Float &other)
    {
        xmm = _mm_sub_ps(xmm, other.xmm);
        return *this;
    }

    inline XMMReg4Float &operator*=(const XMMReg4Float &other)
    {
        xmm = _mm_mul_ps(xmm, other.xmm);
        return *this;
    }

    inline XMMReg4Float operator+(const XMMReg4Float &other) const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_add_ps(xmm, other.xmm);
        return ret;
    }

    inline XMMReg4Float operator-(const XMMReg4Float &other) const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_sub_ps(xmm, other.xmm);
        return ret;
    }

    inline XMMReg4Float operator*(const XMMReg4Float &other) const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_mul_ps(xmm, other.xmm);
        return ret;
    }

    inline XMMReg4Float operator/(const XMMReg4Float &other) const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_div_ps(xmm, other.xmm);
        return ret;
    }

    inline XMMReg4Float inverse() const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_div_ps(_mm_set1_ps(1.0f), xmm);
        return ret;
    }

    inline XMMReg4Int truncate_to_int() const;

    inline XMMReg4Double cast_to_double() const;

    inline void Store4Val(float *ptr) const
    {
        _mm_storeu_ps(ptr, xmm);
    }

    inline void Store4ValAligned(float *ptr) const
    {
        _mm_store_ps(ptr, xmm);
    }

    inline operator float() const
    {
        return _mm_cvtss_f32(xmm);
    }
};

class XMMReg4Int
{
  public:
    __m128i xmm;

    XMMReg4Int()
#if !defined(_MSC_VER)
        : xmm(_mm_undefined_si128())
#endif
    {
    }

    XMMReg4Int(const XMMReg4Int &other) : xmm(other.xmm)
    {
    }

    static inline XMMReg4Int Zero()
    {
        XMMReg4Int reg;
        reg.xmm = _mm_setzero_si128();
        return reg;
    }

    static inline XMMReg4Int Set1(int i)
    {
        XMMReg4Int reg;
        reg.xmm = _mm_set1_epi32(i);
        return reg;
    }

    static inline XMMReg4Int Load4Val(const int *ptr)
    {
        XMMReg4Int reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const int *ptr)
    {
        xmm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
    }

    static inline XMMReg4Int Equals(const XMMReg4Int &expr1,
                                    const XMMReg4Int &expr2)
    {
        XMMReg4Int reg;
        reg.xmm = _mm_cmpeq_epi32(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg4Int Ternary(const XMMReg4Int &cond,
                                     const XMMReg4Int &true_expr,
                                     const XMMReg4Int &false_expr)
    {
        XMMReg4Int reg;
        reg.xmm = _mm_or_si128(_mm_and_si128(cond.xmm, true_expr.xmm),
                               _mm_andnot_si128(cond.xmm, false_expr.xmm));
        return reg;
    }

    inline XMMReg4Int &operator+=(const XMMReg4Int &other)
    {
        xmm = _mm_add_epi32(xmm, other.xmm);
        return *this;
    }

    inline XMMReg4Int &operator-=(const XMMReg4Int &other)
    {
        xmm = _mm_sub_epi32(xmm, other.xmm);
        return *this;
    }

    inline XMMReg4Int operator+(const XMMReg4Int &other) const
    {
        XMMReg4Int ret;
        ret.xmm = _mm_add_epi32(xmm, other.xmm);
        return ret;
    }

    inline XMMReg4Int operator-(const XMMReg4Int &other) const
    {
        XMMReg4Int ret;
        ret.xmm = _mm_sub_epi32(xmm, other.xmm);
        return ret;
    }

    XMMReg4Double cast_to_double() const;

    XMMReg4Float to_float() const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_cvtepi32_ps(xmm);
        return ret;
    }
};

inline XMMReg4Int XMMReg4Float::truncate_to_int() const
{
    XMMReg4Int ret;
    ret.xmm = _mm_cvttps_epi32(xmm);
    return ret;
}

class XMMReg8Byte
{
  public:
    __m128i xmm;

    XMMReg8Byte()
#if !defined(_MSC_VER)
        : xmm(_mm_undefined_si128())
#endif
    {
    }

    XMMReg8Byte(const XMMReg8Byte &other) : xmm(other.xmm)
    {
    }

    static inline XMMReg8Byte Zero()
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_setzero_si128();
        return reg;
    }

    static inline XMMReg8Byte Set1(char i)
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_set1_epi8(i);
        return reg;
    }

    static inline XMMReg8Byte Equals(const XMMReg8Byte &expr1,
                                     const XMMReg8Byte &expr2)
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_cmpeq_epi8(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg8Byte Or(const XMMReg8Byte &expr1,
                                 const XMMReg8Byte &expr2)
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_or_si128(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg8Byte Ternary(const XMMReg8Byte &cond,
                                      const XMMReg8Byte &true_expr,
                                      const XMMReg8Byte &false_expr)
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_or_si128(_mm_and_si128(cond.xmm, true_expr.xmm),
                               _mm_andnot_si128(cond.xmm, false_expr.xmm));
        return reg;
    }

    inline XMMReg8Byte operator+(const XMMReg8Byte &other) const
    {
        XMMReg8Byte ret;
        ret.xmm = _mm_add_epi8(xmm, other.xmm);
        return ret;
    }

    inline XMMReg8Byte operator-(const XMMReg8Byte &other) const
    {
        XMMReg8Byte ret;
        ret.xmm = _mm_sub_epi8(xmm, other.xmm);
        return ret;
    }

    static inline XMMReg8Byte Pack(const XMMReg4Int &r0, const XMMReg4Int &r1)
    {
        XMMReg8Byte reg;
        reg.xmm = _mm_packs_epi32(r0.xmm, r1.xmm);
        reg.xmm = _mm_packus_epi16(reg.xmm, reg.xmm);
        return reg;
    }

    inline void Store8Val(unsigned char *ptr) const
    {
        GDALCopyXMMToInt64(xmm, reinterpret_cast<GInt64 *>(ptr));
    }
};

class XMMReg2Double
{
  public:
    __m128d xmm;

    XMMReg2Double()
#if !defined(_MSC_VER)
        : xmm(_mm_undefined_pd())
#endif
    {
    }

    XMMReg2Double(double val) : xmm(_mm_load_sd(&val))
    {
    }

    XMMReg2Double(const XMMReg2Double &other) : xmm(other.xmm)
    {
    }

    static inline XMMReg2Double Set1(double d)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_set1_pd(d);
        return reg;
    }

    static inline XMMReg2Double Zero()
    {
        XMMReg2Double reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg2Double Load1ValHighAndLow(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const float *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2ValAligned(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2ValAligned(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned char *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const short *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned short *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const int *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Equals(const XMMReg2Double &expr1,
                                       const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpeq_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double NotEquals(const XMMReg2Double &expr1,
                                          const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpneq_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double Greater(const XMMReg2Double &expr1,
                                        const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpgt_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double And(const XMMReg2Double &expr1,
                                    const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_and_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double Ternary(const XMMReg2Double &cond,
                                        const XMMReg2Double &true_expr,
                                        const XMMReg2Double &false_expr)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_or_pd(_mm_and_pd(cond.xmm, true_expr.xmm),
                            _mm_andnot_pd(cond.xmm, false_expr.xmm));
        return reg;
    }

    static inline XMMReg2Double Min(const XMMReg2Double &expr1,
                                    const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_min_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double *ptr)
    {
        xmm = _mm_load1_pd(ptr);
    }

    inline void nsLoad2Val(const double *ptr)
    {
        xmm = _mm_loadu_pd(ptr);
    }

    inline void nsLoad2ValAligned(const double *ptr)
    {
        xmm = _mm_load_pd(ptr);
    }

    inline void nsLoad2Val(const float *ptr)
    {
        xmm = _mm_cvtps_pd(_mm_castsi128_ps(GDALCopyInt64ToXMM(ptr)));
    }

    inline void nsLoad2Val(const int *ptr)
    {
        xmm = _mm_cvtepi32_pd(GDALCopyInt64ToXMM(ptr));
    }

    inline void nsLoad2Val(const unsigned char *ptr)
    {
        __m128i xmm_i = GDALCopyInt16ToXMM(ptr);
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const short *ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        xmm_i = _mm_cvtepi16_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi16(
            xmm_i, xmm_i); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|b|b|a|a */
        xmm_i = _mm_srai_epi32(
            xmm_i, 16); /* 0|0|0|0|b|b|a|a --> 0|0|0|0|sign(b)|b|sign(a)|a */
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const unsigned short *ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        xmm_i = _mm_cvtepu16_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi16(
            xmm_i,
            _mm_setzero_si128()); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|0|b|0|a */
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    static inline void Load4Val(const unsigned char *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
        low.xmm = _mm_cvtepi32_pd(xmm_i);
        high.xmm =
            _mm_cvtepi32_pd(_mm_shuffle_epi32(xmm_i, _MM_SHUFFLE(3, 2, 3, 2)));
    }

    static inline void Load4Val(const short *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const unsigned short *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const double *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const float *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        __m128 temp1 = _mm_loadu_ps(ptr);
        __m128 temp2 = _mm_shuffle_ps(temp1, temp1, _MM_SHUFFLE(3, 2, 3, 2));
        low.xmm = _mm_cvtps_pd(temp1);
        high.xmm = _mm_cvtps_pd(temp2);
    }

    inline void Zeroize()
    {
        xmm = _mm_setzero_pd();
    }

    inline XMMReg2Double &operator=(const XMMReg2Double &other)
    {
        xmm = other.xmm;
        return *this;
    }

    inline XMMReg2Double &operator+=(const XMMReg2Double &other)
    {
        xmm = _mm_add_pd(xmm, other.xmm);
        return *this;
    }

    inline XMMReg2Double &operator*=(const XMMReg2Double &other)
    {
        xmm = _mm_mul_pd(xmm, other.xmm);
        return *this;
    }

    inline XMMReg2Double operator+(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_add_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator-(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_sub_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator*(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_mul_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator/(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_div_pd(xmm, other.xmm);
        return ret;
    }

    inline double GetHorizSum() const
    {
        __m128d xmm2;
        xmm2 = _mm_shuffle_pd(
            xmm, xmm,
            _MM_SHUFFLE2(0, 1)); /* transfer high word into low word of xmm2 */
        return _mm_cvtsd_f64(_mm_add_sd(xmm, xmm2));
    }

    inline void Store2Val(double *ptr) const
    {
        _mm_storeu_pd(ptr, xmm);
    }

    inline void Store2ValAligned(double *ptr) const
    {
        _mm_store_pd(ptr, xmm);
    }

    inline void Store2Val(float *ptr) const
    {
        __m128i xmm_i = _mm_castps_si128(_mm_cvtpd_ps(xmm));
        GDALCopyXMMToInt64(xmm_i, reinterpret_cast<GInt64 *>(ptr));
    }

    inline void Store2Val(unsigned char *ptr) const
    {
        __m128i tmp = _mm_cvttpd_epi32(_mm_add_pd(
            xmm,
            _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        tmp = _mm_packs_epi32(tmp, tmp);
        tmp = _mm_packus_epi16(tmp, tmp);
        GDALCopyXMMToInt16(tmp, reinterpret_cast<GInt16 *>(ptr));
    }

    inline void Store2Val(unsigned short *ptr) const
    {
        __m128i tmp = _mm_cvttpd_epi32(_mm_add_pd(
            xmm,
            _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        // X X X X 0 B 0 A --> X X X X A A B A
        tmp = _mm_shufflelo_epi16(tmp, 0 | (2 << 2));
        GDALCopyXMMToInt32(tmp, reinterpret_cast<GInt32 *>(ptr));
    }

    inline void StoreMask(unsigned char *ptr) const
    {
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr),
                         _mm_castpd_si128(xmm));
    }

    inline operator double() const
    {
        return _mm_cvtsd_f64(xmm);
    }
};

#else

#ifndef NO_WARN_USE_SSE2_EMULATION
#warning "Software emulation of SSE2 !"
#endif

class XMMReg2Double
{
  public:
    double low;
    double high;

    // cppcheck-suppress uninitMemberVar
    XMMReg2Double() = default;

    explicit XMMReg2Double(double val)
    {
        low = val;
        high = 0.0;
    }

    XMMReg2Double(const XMMReg2Double &other) : low(other.low), high(other.high)
    {
    }

    static inline XMMReg2Double Zero()
    {
        XMMReg2Double reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg2Double Set1(double d)
    {
        XMMReg2Double reg;
        reg.low = d;
        reg.high = d;
        return reg;
    }

    static inline XMMReg2Double Load1ValHighAndLow(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    static inline XMMReg2Double Equals(const XMMReg2Double &expr1,
                                       const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;

        if (expr1.low == expr2.low)
            memset(&(reg.low), 0xFF, sizeof(double));
        else
            reg.low = 0;

        if (expr1.high == expr2.high)
            memset(&(reg.high), 0xFF, sizeof(double));
        else
            reg.high = 0;

        return reg;
    }

    static inline XMMReg2Double NotEquals(const XMMReg2Double &expr1,
                                          const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;

        if (expr1.low != expr2.low)
            memset(&(reg.low), 0xFF, sizeof(double));
        else
            reg.low = 0;

        if (expr1.high != expr2.high)
            memset(&(reg.high), 0xFF, sizeof(double));
        else
            reg.high = 0;

        return reg;
    }

    static inline XMMReg2Double Greater(const XMMReg2Double &expr1,
                                        const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;

        if (expr1.low > expr2.low)
            memset(&(reg.low), 0xFF, sizeof(double));
        else
            reg.low = 0;

        if (expr1.high > expr2.high)
            memset(&(reg.high), 0xFF, sizeof(double));
        else
            reg.high = 0;

        return reg;
    }

    static inline XMMReg2Double And(const XMMReg2Double &expr1,
                                    const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        int low1[2], high1[2];
        int low2[2], high2[2];
        memcpy(low1, &expr1.low, sizeof(double));
        memcpy(high1, &expr1.high, sizeof(double));
        memcpy(low2, &expr2.low, sizeof(double));
        memcpy(high2, &expr2.high, sizeof(double));
        low1[0] &= low2[0];
        low1[1] &= low2[1];
        high1[0] &= high2[0];
        high1[1] &= high2[1];
        memcpy(&reg.low, low1, sizeof(double));
        memcpy(&reg.high, high1, sizeof(double));
        return reg;
    }

    static inline XMMReg2Double Ternary(const XMMReg2Double &cond,
                                        const XMMReg2Double &true_expr,
                                        const XMMReg2Double &false_expr)
    {
        XMMReg2Double reg;
        if (cond.low != 0)
            reg.low = true_expr.low;
        else
            reg.low = false_expr.low;
        if (cond.high != 0)
            reg.high = true_expr.high;
        else
            reg.high = false_expr.high;
        return reg;
    }

    static inline XMMReg2Double Min(const XMMReg2Double &expr1,
                                    const XMMReg2Double &expr2)
    {
        XMMReg2Double reg;
        reg.low = (expr1.low < expr2.low) ? expr1.low : expr2.low;
        reg.high = (expr1.high < expr2.high) ? expr1.high : expr2.high;
        return reg;
    }

    static inline XMMReg2Double Load2Val(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2ValAligned(const double *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2ValAligned(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const float *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned char *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const short *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned short *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const int *ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double *ptr)
    {
        low = ptr[0];
        high = ptr[0];
    }

    inline void nsLoad2Val(const double *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2ValAligned(const double *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const float *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const unsigned char *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const short *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const unsigned short *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const int *ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    static inline void Load4Val(const unsigned char *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.low = ptr[0];
        low.high = ptr[1];
        high.low = ptr[2];
        high.high = ptr[3];
    }

    static inline void Load4Val(const short *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const unsigned short *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const double *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    static inline void Load4Val(const float *ptr, XMMReg2Double &low,
                                XMMReg2Double &high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr + 2);
    }

    inline void Zeroize()
    {
        low = 0.0;
        high = 0.0;
    }

    inline XMMReg2Double &operator=(const XMMReg2Double &other)
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    inline XMMReg2Double &operator+=(const XMMReg2Double &other)
    {
        low += other.low;
        high += other.high;
        return *this;
    }

    inline XMMReg2Double &operator*=(const XMMReg2Double &other)
    {
        low *= other.low;
        high *= other.high;
        return *this;
    }

    inline XMMReg2Double operator+(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.low = low + other.low;
        ret.high = high + other.high;
        return ret;
    }

    inline XMMReg2Double operator-(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.low = low - other.low;
        ret.high = high - other.high;
        return ret;
    }

    inline XMMReg2Double operator*(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.low = low * other.low;
        ret.high = high * other.high;
        return ret;
    }

    inline XMMReg2Double operator/(const XMMReg2Double &other) const
    {
        XMMReg2Double ret;
        ret.low = low / other.low;
        ret.high = high / other.high;
        return ret;
    }

    inline double GetHorizSum() const
    {
        return low + high;
    }

    inline void Store2Val(double *ptr) const
    {
        ptr[0] = low;
        ptr[1] = high;
    }

    inline void Store2ValAligned(double *ptr) const
    {
        ptr[0] = low;
        ptr[1] = high;
    }

    inline void Store2Val(float *ptr) const
    {
        ptr[0] = static_cast<float>(low);
        ptr[1] = static_cast<float>(high);
    }

    void Store2Val(unsigned char *ptr) const
    {
        ptr[0] = static_cast<unsigned char>(low + 0.5);
        ptr[1] = static_cast<unsigned char>(high + 0.5);
    }

    void Store2Val(unsigned short *ptr) const
    {
        ptr[0] = static_cast<GUInt16>(low + 0.5);
        ptr[1] = static_cast<GUInt16>(high + 0.5);
    }

    inline void StoreMask(unsigned char *ptr) const
    {
        memcpy(ptr, &low, 8);
        memcpy(ptr + 8, &high, 8);
    }

    inline operator double() const
    {
        return low;
    }
};

#endif /*  defined(__x86_64) || defined(_M_X64) */

#if defined(__AVX__) && !defined(USE_SSE2_EMULATION)

#include <immintrin.h>

class XMMReg4Double
{
  public:
    __m256d ymm;

    XMMReg4Double() : ymm(_mm256_setzero_pd())
    {
    }

    XMMReg4Double(const XMMReg4Double &other) : ymm(other.ymm)
    {
    }

    static inline XMMReg4Double Zero()
    {
        XMMReg4Double reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg4Double Set1(double d)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_set1_pd(d);
        return reg;
    }

    inline void Zeroize()
    {
        ymm = _mm256_setzero_pd();
    }

    static inline XMMReg4Double Load1ValHighAndLow(const double *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double *ptr)
    {
        ymm = _mm256_set1_pd(*ptr);
    }

    static inline XMMReg4Double Load4Val(const unsigned char *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const unsigned char *ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(xmm_i);
    }

    static inline void Load8Val(const unsigned char *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        const __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        const __m128i xmm_i_low = _mm_cvtepu8_epi32(xmm_i);
        low.ymm = _mm256_cvtepi32_pd(xmm_i_low);
        const __m128i xmm_i_high = _mm_cvtepu8_epi32(_mm_srli_si128(xmm_i, 4));
        high.ymm = _mm256_cvtepi32_pd(xmm_i_high);
    }

    static inline XMMReg4Double Load4Val(const short *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const short *ptr)
    {
        __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm_i = _mm_cvtepi16_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(xmm_i);
    }

    static inline void Load8Val(const short *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low.nsLoad4Val(ptr);
        high.nsLoad4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const unsigned short *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const unsigned short *ptr)
    {
        __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm_i = _mm_cvtepu16_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(
            xmm_i);  // ok to use signed conversion since we are in the ushort
                     // range, so cannot be interpreted as negative int32
    }

    static inline void Load8Val(const unsigned short *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low.nsLoad4Val(ptr);
        high.nsLoad4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const double *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const double *ptr)
    {
        ymm = _mm256_loadu_pd(ptr);
    }

    static inline void Load8Val(const double *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low.nsLoad4Val(ptr);
        high.nsLoad4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4ValAligned(const double *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4ValAligned(ptr);
        return reg;
    }

    inline void nsLoad4ValAligned(const double *ptr)
    {
        ymm = _mm256_load_pd(ptr);
    }

    static inline XMMReg4Double Load4Val(const float *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const float *ptr)
    {
        ymm = _mm256_cvtps_pd(_mm_loadu_ps(ptr));
    }

    static inline void Load8Val(const float *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low.nsLoad4Val(ptr);
        high.nsLoad4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const int *ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const int *ptr)
    {
        ymm = _mm256_cvtepi32_pd(
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr)));
    }

    static inline void Load8Val(const int *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low.nsLoad4Val(ptr);
        high.nsLoad4Val(ptr + 4);
    }

    static inline XMMReg4Double Equals(const XMMReg4Double &expr1,
                                       const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_EQ_OQ);
        return reg;
    }

    static inline XMMReg4Double NotEquals(const XMMReg4Double &expr1,
                                          const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_NEQ_OQ);
        return reg;
    }

    static inline XMMReg4Double Greater(const XMMReg4Double &expr1,
                                        const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_GT_OQ);
        return reg;
    }

    static inline XMMReg4Double And(const XMMReg4Double &expr1,
                                    const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_and_pd(expr1.ymm, expr2.ymm);
        return reg;
    }

    static inline XMMReg4Double Ternary(const XMMReg4Double &cond,
                                        const XMMReg4Double &true_expr,
                                        const XMMReg4Double &false_expr)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_or_pd(_mm256_and_pd(cond.ymm, true_expr.ymm),
                               _mm256_andnot_pd(cond.ymm, false_expr.ymm));
        return reg;
    }

    static inline XMMReg4Double Min(const XMMReg4Double &expr1,
                                    const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_min_pd(expr1.ymm, expr2.ymm);
        return reg;
    }

    inline XMMReg4Double &operator=(const XMMReg4Double &other)
    {
        ymm = other.ymm;
        return *this;
    }

    inline XMMReg4Double &operator+=(const XMMReg4Double &other)
    {
        ymm = _mm256_add_pd(ymm, other.ymm);
        return *this;
    }

    inline XMMReg4Double &operator*=(const XMMReg4Double &other)
    {
        ymm = _mm256_mul_pd(ymm, other.ymm);
        return *this;
    }

    inline XMMReg4Double operator+(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_add_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator-(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_sub_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator*(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_mul_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator/(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_div_pd(ymm, other.ymm);
        return ret;
    }

    void AddToLow(const XMMReg2Double &other)
    {
        __m256d ymm2 = _mm256_setzero_pd();
        ymm2 = _mm256_insertf128_pd(ymm2, other.xmm, 0);
        ymm = _mm256_add_pd(ymm, ymm2);
    }

    inline double GetHorizSum() const
    {
        __m256d ymm_tmp1, ymm_tmp2;
        ymm_tmp2 = _mm256_hadd_pd(ymm, ymm);
        ymm_tmp1 = _mm256_permute2f128_pd(ymm_tmp2, ymm_tmp2, 1);
        ymm_tmp1 = _mm256_add_pd(ymm_tmp1, ymm_tmp2);
        return _mm_cvtsd_f64(_mm256_castpd256_pd128(ymm_tmp1));
    }

    inline XMMReg4Double approx_inv_sqrt(const XMMReg4Double &one,
                                         const XMMReg4Double &half) const
    {
        __m256d reg = ymm;
        __m256d reg_half = _mm256_mul_pd(reg, half.ymm);
        // Compute rough approximation of 1 / sqrt(b) with _mm_rsqrt_ps
        reg = _mm256_cvtps_pd(_mm_rsqrt_ps(_mm256_cvtpd_ps(reg)));
        // And perform one step of Newton-Raphson approximation to improve it
        // approx_inv_sqrt_x = approx_inv_sqrt_x*(1.5 -
        //                            0.5*x*approx_inv_sqrt_x*approx_inv_sqrt_x);
        const __m256d one_and_a_half = _mm256_add_pd(one.ymm, half.ymm);
        reg = _mm256_mul_pd(
            reg,
            _mm256_sub_pd(one_and_a_half,
                          _mm256_mul_pd(reg_half, _mm256_mul_pd(reg, reg))));
        XMMReg4Double ret;
        ret.ymm = reg;
        return ret;
    }

    inline XMMReg4Float cast_to_float() const
    {
        XMMReg4Float ret;
        ret.xmm = _mm256_cvtpd_ps(ymm);
        return ret;
    }

    inline void Store4Val(unsigned char *ptr) const
    {
        __m128i xmm_i =
            _mm256_cvttpd_epi32(_mm256_add_pd(ymm, _mm256_set1_pd(0.5)));
        // xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
        // xmm_i = _mm_packus_epi16(xmm_i, xmm_i);  // Pack int16 to uint8
        xmm_i =
            _mm_shuffle_epi8(xmm_i, _mm_cvtsi32_si128(0 | (4 << 8) | (8 << 16) |
                                                      (12 << 24)));  //  SSSE3
        GDALCopyXMMToInt32(xmm_i, reinterpret_cast<GInt32 *>(ptr));
    }

    inline void Store4Val(unsigned short *ptr) const
    {
        __m128i xmm_i =
            _mm256_cvttpd_epi32(_mm256_add_pd(ymm, _mm256_set1_pd(0.5)));
        xmm_i = _mm_packus_epi32(xmm_i, xmm_i);  // Pack uint32 to uint16
        GDALCopyXMMToInt64(xmm_i, reinterpret_cast<GInt64 *>(ptr));
    }

    inline void Store4Val(float *ptr) const
    {
        _mm_storeu_ps(ptr, _mm256_cvtpd_ps(ymm));
    }

    inline void Store4Val(double *ptr) const
    {
        _mm256_storeu_pd(ptr, ymm);
    }

    inline void StoreMask(unsigned char *ptr) const
    {
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr),
                            _mm256_castpd_si256(ymm));
    }
};

inline XMMReg4Double XMMReg4Float::cast_to_double() const
{
    XMMReg4Double ret;
    ret.ymm = _mm256_cvtps_pd(xmm);
    return ret;
}

inline XMMReg4Double XMMReg4Int::cast_to_double() const
{
    XMMReg4Double ret;
    ret.ymm = _mm256_cvtepi32_pd(xmm);
    return ret;
}

#else

class XMMReg4Double
{
  public:
    XMMReg2Double low, high;

    XMMReg4Double() : low(XMMReg2Double()), high(XMMReg2Double())
    {
    }

    XMMReg4Double(const XMMReg4Double &other) : low(other.low), high(other.high)
    {
    }

    static inline XMMReg4Double Zero()
    {
        XMMReg4Double reg;
        reg.low.Zeroize();
        reg.high.Zeroize();
        return reg;
    }

    static inline XMMReg4Double Set1(double d)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Set1(d);
        reg.high = XMMReg2Double::Set1(d);
        return reg;
    }

    static inline XMMReg4Double Load1ValHighAndLow(const double *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad1ValHighAndLow(ptr);
        reg.high = reg.low;
        return reg;
    }

    static inline XMMReg4Double Load4Val(const unsigned char *ptr)
    {
        XMMReg4Double reg;
        XMMReg2Double::Load4Val(ptr, reg.low, reg.high);
        return reg;
    }

    static inline void Load8Val(const unsigned char *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const short *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr + 2);
        return reg;
    }

    static inline void Load8Val(const short *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const unsigned short *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr + 2);
        return reg;
    }

    static inline void Load8Val(const unsigned short *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const int *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr + 2);
        return reg;
    }

    static inline void Load8Val(const int *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4Val(const double *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr + 2);
        return reg;
    }

    static inline void Load8Val(const double *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Load4ValAligned(const double *ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2ValAligned(ptr);
        reg.high.nsLoad2ValAligned(ptr + 2);
        return reg;
    }

    static inline XMMReg4Double Load4Val(const float *ptr)
    {
        XMMReg4Double reg;
        XMMReg2Double::Load4Val(ptr, reg.low, reg.high);
        return reg;
    }

    static inline void Load8Val(const float *ptr, XMMReg4Double &low,
                                XMMReg4Double &high)
    {
        low = Load4Val(ptr);
        high = Load4Val(ptr + 4);
    }

    static inline XMMReg4Double Equals(const XMMReg4Double &expr1,
                                       const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Equals(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Equals(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double NotEquals(const XMMReg4Double &expr1,
                                          const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::NotEquals(expr1.low, expr2.low);
        reg.high = XMMReg2Double::NotEquals(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double Greater(const XMMReg4Double &expr1,
                                        const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Greater(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Greater(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double And(const XMMReg4Double &expr1,
                                    const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::And(expr1.low, expr2.low);
        reg.high = XMMReg2Double::And(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double Ternary(const XMMReg4Double &cond,
                                        const XMMReg4Double &true_expr,
                                        const XMMReg4Double &false_expr)
    {
        XMMReg4Double reg;
        reg.low =
            XMMReg2Double::Ternary(cond.low, true_expr.low, false_expr.low);
        reg.high =
            XMMReg2Double::Ternary(cond.high, true_expr.high, false_expr.high);
        return reg;
    }

    static inline XMMReg4Double Min(const XMMReg4Double &expr1,
                                    const XMMReg4Double &expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Min(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Min(expr1.high, expr2.high);
        return reg;
    }

    inline XMMReg4Double &operator=(const XMMReg4Double &other)
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    inline XMMReg4Double &operator+=(const XMMReg4Double &other)
    {
        low += other.low;
        high += other.high;
        return *this;
    }

    inline XMMReg4Double &operator*=(const XMMReg4Double &other)
    {
        low *= other.low;
        high *= other.high;
        return *this;
    }

    inline XMMReg4Double operator+(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.low = low + other.low;
        ret.high = high + other.high;
        return ret;
    }

    inline XMMReg4Double operator-(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.low = low - other.low;
        ret.high = high - other.high;
        return ret;
    }

    inline XMMReg4Double operator*(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.low = low * other.low;
        ret.high = high * other.high;
        return ret;
    }

    inline XMMReg4Double operator/(const XMMReg4Double &other) const
    {
        XMMReg4Double ret;
        ret.low = low / other.low;
        ret.high = high / other.high;
        return ret;
    }

    void AddToLow(const XMMReg2Double &other)
    {
        low += other;
    }

    inline double GetHorizSum() const
    {
        return (low + high).GetHorizSum();
    }

#if !defined(USE_SSE2_EMULATION)
    inline XMMReg4Double approx_inv_sqrt(const XMMReg4Double &one,
                                         const XMMReg4Double &half) const
    {
        __m128d reg0 = low.xmm;
        __m128d reg1 = high.xmm;
        __m128d reg0_half = _mm_mul_pd(reg0, half.low.xmm);
        __m128d reg1_half = _mm_mul_pd(reg1, half.low.xmm);
        // Compute rough approximation of 1 / sqrt(b) with _mm_rsqrt_ps
        reg0 = _mm_cvtps_pd(_mm_rsqrt_ps(_mm_cvtpd_ps(reg0)));
        reg1 = _mm_cvtps_pd(_mm_rsqrt_ps(_mm_cvtpd_ps(reg1)));
        // And perform one step of Newton-Raphson approximation to improve it
        // approx_inv_sqrt_x = approx_inv_sqrt_x*(1.5 -
        //                            0.5*x*approx_inv_sqrt_x*approx_inv_sqrt_x);
        const __m128d one_and_a_half = _mm_add_pd(one.low.xmm, half.low.xmm);
        reg0 = _mm_mul_pd(
            reg0, _mm_sub_pd(one_and_a_half,
                             _mm_mul_pd(reg0_half, _mm_mul_pd(reg0, reg0))));
        reg1 = _mm_mul_pd(
            reg1, _mm_sub_pd(one_and_a_half,
                             _mm_mul_pd(reg1_half, _mm_mul_pd(reg1, reg1))));
        XMMReg4Double ret;
        ret.low.xmm = reg0;
        ret.high.xmm = reg1;
        return ret;
    }

    inline XMMReg4Float cast_to_float() const
    {
        XMMReg4Float ret;
        ret.xmm = _mm_castsi128_ps(
            _mm_unpacklo_epi64(_mm_castps_si128(_mm_cvtpd_ps(low.xmm)),
                               _mm_castps_si128(_mm_cvtpd_ps(high.xmm))));
        return ret;
    }
#endif

    inline void Store4Val(unsigned char *ptr) const
    {
#ifdef USE_SSE2_EMULATION
        low.Store2Val(ptr);
        high.Store2Val(ptr + 2);
#else
        __m128i tmpLow = _mm_cvttpd_epi32(_mm_add_pd(
            low.xmm,
            _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        __m128i tmpHigh = _mm_cvttpd_epi32(_mm_add_pd(
            high.xmm,
            _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        auto tmp = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(tmpLow),
                                                   _mm_castsi128_ps(tmpHigh),
                                                   _MM_SHUFFLE(1, 0, 1, 0)));
        tmp = _mm_packs_epi32(tmp, tmp);
        tmp = _mm_packus_epi16(tmp, tmp);
        GDALCopyXMMToInt32(tmp, reinterpret_cast<GInt32 *>(ptr));
#endif
    }

    inline void Store4Val(unsigned short *ptr) const
    {
#if 1
        low.Store2Val(ptr);
        high.Store2Val(ptr + 2);
#else
        __m128i xmm0 = _mm_cvtpd_epi32(low.xmm);
        __m128i xmm1 = _mm_cvtpd_epi32(high.xmm);
        xmm0 = _mm_or_si128(xmm0, _mm_slli_si128(xmm1, 8));
#if __SSE4_1__
        xmm0 = _mm_packus_epi32(xmm0, xmm0);  // Pack uint32 to uint16
#else
        xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(-32768));
        xmm0 = _mm_packs_epi32(xmm0, xmm0);
        xmm0 = _mm_sub_epi16(xmm0, _mm_set1_epi16(-32768));
#endif
        GDALCopyXMMToInt64(xmm0, (GInt64 *)ptr);
#endif
    }

    inline void Store4Val(float *ptr) const
    {
        low.Store2Val(ptr);
        high.Store2Val(ptr + 2);
    }

    inline void Store4Val(double *ptr) const
    {
        low.Store2Val(ptr);
        high.Store2Val(ptr + 2);
    }

    inline void StoreMask(unsigned char *ptr) const
    {
        low.StoreMask(ptr);
        high.StoreMask(ptr + 16);
    }
};

#if !defined(USE_SSE2_EMULATION)
inline XMMReg4Double XMMReg4Float::cast_to_double() const
{
    XMMReg4Double ret;
    ret.low.xmm = _mm_cvtps_pd(xmm);
    ret.high.xmm = _mm_cvtps_pd(
        _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(xmm), 8)));
    return ret;
}

inline XMMReg4Double XMMReg4Int::cast_to_double() const
{
    XMMReg4Double ret;
    ret.low.xmm = _mm_cvtepi32_pd(xmm);
    ret.high.xmm = _mm_cvtepi32_pd(_mm_srli_si128(xmm, 8));
    return ret;
}
#endif

#endif

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDALSSE_PRIV_H_INCLUDED */
