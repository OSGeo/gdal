/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  SSE2 helper
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef GDALSSE_PRIV_H_INCLUDED
#define GDALSSE_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* Could possibly be used too on 32bit, but we would need to check at runtime */
#if (defined(__x86_64) || defined(_M_X64)) && !defined(USE_SSE2_EMULATION)

/* Requires SSE2 */
#include <emmintrin.h>
#include <string.h>

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

#include "gdal_priv_templates.hpp"

static inline __m128i GDALCopyInt16ToXMM(const void* ptr)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    unsigned short s;
    memcpy(&s, ptr, 2);
    return _mm_cvtsi32_si128(s);
#else
    return _mm_cvtsi32_si128(*static_cast<const unsigned short*>(ptr));
#endif
}

static inline __m128i GDALCopyInt32ToXMM(const void* ptr)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    GInt32 i;
    memcpy(&i, ptr, 4);
    return _mm_cvtsi32_si128(i);
#else
    return _mm_cvtsi32_si128(*static_cast<const GInt32*>(ptr));
#endif
}

static inline __m128i GDALCopyInt64ToXMM(const void* ptr)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    GInt64 i;
    memcpy(&i, ptr, 8);
    return _mm_cvtsi64_si128(i);
#else
    return _mm_cvtsi64_si128(*static_cast<const GInt64*>(ptr));
#endif
}

static inline void GDALCopyXMMToInt16(const __m128i xmm, void* pDest)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    GInt16 i = static_cast<GInt16>(_mm_extract_epi16(xmm, 0));
    memcpy(pDest, &i, 2);
#else
    *static_cast<GInt16*>(pDest) = static_cast<GInt16>(_mm_extract_epi16(xmm, 0));
#endif
}

class XMMReg2Double
{
  public:
    __m128d xmm;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
    /* coverity[uninit_member] */
    XMMReg2Double() = default;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    XMMReg2Double(double  val): xmm(_mm_load_sd (&val)) {}
    XMMReg2Double(const XMMReg2Double& other) : xmm(other.xmm) {}

    static inline XMMReg2Double Zero()
    {
        XMMReg2Double reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg2Double Load1ValHighAndLow(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const float* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2ValAligned(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2ValAligned(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned char* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const short* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned short* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Equals(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpeq_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double NotEquals(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpneq_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double Greater(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_cmpgt_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double And(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_and_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    static inline XMMReg2Double Ternary(const XMMReg2Double& cond, const XMMReg2Double& true_expr, const XMMReg2Double& false_expr)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_or_pd(_mm_and_pd (cond.xmm, true_expr.xmm), _mm_andnot_pd(cond.xmm, false_expr.xmm));
        return reg;
    }

    static inline XMMReg2Double Min(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.xmm = _mm_min_pd(expr1.xmm, expr2.xmm);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double* ptr)
    {
        xmm =  _mm_load1_pd(ptr);
    }

    inline void nsLoad2Val(const double* ptr)
    {
        xmm = _mm_loadu_pd(ptr);
    }

    inline void nsLoad2ValAligned(const double* ptr)
    {
        xmm = _mm_load_pd(ptr);
    }

    inline void nsLoad2Val(const float* ptr)
    {
        xmm = _mm_cvtps_pd(_mm_castsi128_ps(GDALCopyInt64ToXMM(ptr)));
    }

    inline void nsLoad2Val(const unsigned char* ptr)
    {
        __m128i xmm_i = GDALCopyInt16ToXMM(ptr);
#ifdef __SSE4_1__
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const short* ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#ifdef __SSE4_1__
        xmm_i = _mm_cvtepi16_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi16(xmm_i,xmm_i); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|b|b|a|a */
        xmm_i = _mm_srai_epi32(xmm_i, 16);       /* 0|0|0|0|b|b|a|a --> 0|0|0|0|sign(b)|b|sign(a)|a */
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const unsigned short* ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#ifdef __SSE4_1__
        xmm_i = _mm_cvtepu16_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi16(xmm_i,_mm_setzero_si128()); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|0|b|0|a */
#endif
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    static inline void Load4Val(const unsigned char* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
#ifdef __SSE4_1__
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
#else
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
        low.xmm = _mm_cvtepi32_pd(xmm_i);
        high.xmm =  _mm_cvtepi32_pd(_mm_shuffle_epi32(xmm_i,_MM_SHUFFLE(3,2,3,2)));
    }

    static inline void Load4Val(const short* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const unsigned short* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const double* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const float* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        __m128 temp1 = _mm_loadu_ps(ptr);
        __m128 temp2 = _mm_shuffle_ps(temp1, temp1, _MM_SHUFFLE(3,2,3,2));
        low.xmm = _mm_cvtps_pd(temp1);
        high.xmm = _mm_cvtps_pd(temp2);
    }

    inline void Zeroize()
    {
        xmm = _mm_setzero_pd();
    }

    inline XMMReg2Double& operator= (const XMMReg2Double& other)
    {
        xmm = other.xmm;
        return *this;
    }

    inline XMMReg2Double& operator+= (const XMMReg2Double& other)
    {
        xmm = _mm_add_pd(xmm, other.xmm);
        return *this;
    }

    inline XMMReg2Double& operator*= (const XMMReg2Double& other)
    {
        xmm = _mm_mul_pd(xmm, other.xmm);
        return *this;
    }

    inline XMMReg2Double operator+ (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_add_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator- (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_sub_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator* (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_mul_pd(xmm, other.xmm);
        return ret;
    }

    inline XMMReg2Double operator/ (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.xmm = _mm_div_pd(xmm, other.xmm);
        return ret;
    }

    inline double GetHorizSum() const
    {
        __m128d xmm2;
        xmm2 = _mm_shuffle_pd(xmm,xmm,_MM_SHUFFLE2(0,1)); /* transfer high word into low word of xmm2 */
        return _mm_cvtsd_f64(_mm_add_sd(xmm, xmm2));
    }

    inline void Store2Val(double* ptr) const
    {
        _mm_storeu_pd(ptr, xmm);
    }

    inline void Store2ValAligned(double* ptr) const
    {
        _mm_store_pd(ptr, xmm);
    }

    inline void Store2Val(float* ptr) const
    {
        __m128i xmm_i = _mm_castps_si128( _mm_cvtpd_ps(xmm) );
        GDALCopyXMMToInt64(xmm_i, reinterpret_cast<GInt64*>(ptr));
    }

    inline void Store2Val(unsigned char* ptr) const
    {
        __m128i tmp = _mm_cvttpd_epi32(_mm_add_pd(xmm, _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        tmp = _mm_packs_epi32(tmp, tmp);
        tmp = _mm_packus_epi16(tmp, tmp);
        GDALCopyXMMToInt16(tmp, reinterpret_cast<GInt16*>(ptr));
    }

    inline void Store2Val(unsigned short* ptr) const
    {
        __m128i tmp = _mm_cvttpd_epi32(_mm_add_pd(xmm, _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        // X X X X 0 B 0 A --> X X X X A A B A
        tmp = _mm_shufflelo_epi16(tmp, 0 | (2 << 2));
        GDALCopyXMMToInt32(tmp, reinterpret_cast<GInt32*>(ptr));
    }

    inline void StoreMask(unsigned char* ptr) const
    {
        _mm_storeu_si128( reinterpret_cast<__m128i*>(ptr), _mm_castpd_si128(xmm) );
    }

    inline operator double () const
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

    XMMReg2Double() {}
    XMMReg2Double(double  val)  { low = val; high = 0.0; }
    XMMReg2Double(const XMMReg2Double& other) : low(other.low), high(other.high) {}

    static inline XMMReg2Double Zero()
    {
        XMMReg2Double reg;
        reg.Zeroize();
        return reg;
    }

    static inline XMMReg2Double Load1ValHighAndLow(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    static inline XMMReg2Double Equals(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
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

    static inline XMMReg2Double NotEquals(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
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

    static inline XMMReg2Double Greater(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
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

    static inline XMMReg2Double And(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
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

    static inline XMMReg2Double Ternary(const XMMReg2Double& cond, const XMMReg2Double& true_expr, const XMMReg2Double& false_expr)
    {
        XMMReg2Double reg;
        if( cond.low )
            reg.low = true_expr.low;
        else
            reg.low = false_expr.low;
        if( cond.high )
            reg.high = true_expr.high;
        else
            reg.high = false_expr.high;
        return reg;
    }

    static inline XMMReg2Double Min(const XMMReg2Double& expr1, const XMMReg2Double& expr2)
    {
        XMMReg2Double reg;
        reg.low = (expr1.low < expr2.low) ? expr1.low : expr2.low;
        reg.high = (expr1.high < expr2.high) ? expr1.high : expr2.high;
        return reg;
    }

    static inline XMMReg2Double Load2Val(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2ValAligned(const double* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2ValAligned(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const float* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned char* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const short* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    static inline XMMReg2Double Load2Val(const unsigned short* ptr)
    {
        XMMReg2Double reg;
        reg.nsLoad2Val(ptr);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double* ptr)
    {
        low = ptr[0];
        high = ptr[0];
    }

    inline void nsLoad2Val(const double* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2ValAligned(const double* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const float* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const unsigned char* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const short* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    inline void nsLoad2Val(const unsigned short* ptr)
    {
        low = ptr[0];
        high = ptr[1];
    }

    static inline void Load4Val(const unsigned char* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.low = ptr[0];
        low.high = ptr[1];
        high.low = ptr[2];
        high.high = ptr[3];
    }

    static inline void Load4Val(const short* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const unsigned short* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const double* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    static inline void Load4Val(const float* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        low.nsLoad2Val(ptr);
        high.nsLoad2Val(ptr+2);
    }

    inline void Zeroize()
    {
        low = 0.0;
        high = 0.0;
    }

    inline XMMReg2Double& operator= (const XMMReg2Double& other)
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    inline XMMReg2Double& operator+= (const XMMReg2Double& other)
    {
        low += other.low;
        high += other.high;
        return *this;
    }

    inline XMMReg2Double& operator*= (const XMMReg2Double& other)
    {
        low *= other.low;
        high *= other.high;
        return *this;
    }

    inline XMMReg2Double operator+ (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.low = low + other.low;
        ret.high = high + other.high;
        return ret;
    }

    inline XMMReg2Double operator- (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.low = low - other.low;
        ret.high = high - other.high;
        return ret;
    }

    inline XMMReg2Double operator* (const XMMReg2Double& other) const
    {
        XMMReg2Double ret;
        ret.low = low * other.low;
        ret.high = high * other.high;
        return ret;
    }

    inline XMMReg2Double operator/ (const XMMReg2Double& other) const
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

    inline void Store2Val(double* ptr) const
    {
        ptr[0] = low;
        ptr[1] = high;
    }

    inline void Store2ValAligned(double* ptr) const
    {
        ptr[0] = low;
        ptr[1] = high;
    }

    inline void Store2Val(float* ptr) const
    {
        ptr[0] = low;
        ptr[1] = high;
    }

    void Store2Val(unsigned char* ptr) const
    {
        ptr[0] = (unsigned char)(low + 0.5);
        ptr[1] = (unsigned char)(high + 0.5);
    }

    void Store2Val(unsigned short* ptr) const
    {
        ptr[0] = (GUInt16)(low + 0.5);
        ptr[1] = (GUInt16)(high + 0.5);
    }

    inline void StoreMask(unsigned char* ptr) const
    {
        memcpy(ptr, &low, 8);
        memcpy(ptr + 8, &high, 8);
    }

    inline operator double () const
    {
        return low;
    }
};

#endif /*  defined(__x86_64) || defined(_M_X64) */

#ifdef __AVX__

#include <immintrin.h>

class XMMReg4Double
{
  public:
    __m256d ymm;

    XMMReg4Double(): ymm(_mm256_setzero_pd()) {}
    XMMReg4Double(const XMMReg4Double& other) : ymm(other.ymm) {}

    static inline XMMReg4Double Zero()
    {
        XMMReg4Double reg;
        reg.Zeroize();
        return reg;
    }

    inline void Zeroize()
    {
        ymm = _mm256_setzero_pd();
    }

    static inline XMMReg4Double Load1ValHighAndLow(const double* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad1ValHighAndLow(ptr);
        return reg;
    }

    inline void nsLoad1ValHighAndLow(const double* ptr)
    {
        ymm = _mm256_set1_pd(*ptr);
    }

    static inline XMMReg4Double Load4Val(const unsigned char* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const unsigned char* ptr)
    {
        __m128i xmm_i = GDALCopyInt32ToXMM(ptr);
        xmm_i = _mm_cvtepu8_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(xmm_i);
    }

    static inline XMMReg4Double Load4Val(const short* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const short* ptr)
    {
        __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm_i = _mm_cvtepi16_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(xmm_i);
    }

    static inline XMMReg4Double Load4Val(const unsigned short* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const unsigned short* ptr)
    {
        __m128i xmm_i = GDALCopyInt64ToXMM(ptr);
        xmm_i = _mm_cvtepu16_epi32(xmm_i);
        ymm = _mm256_cvtepi32_pd(xmm_i); // ok to use signed conversion since we are in the ushort range, so cannot be interpreted as negative int32
    }

    static inline XMMReg4Double Load4Val(const double* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const double* ptr)
    {
        ymm = _mm256_loadu_pd(ptr);
    }

    static inline XMMReg4Double Load4ValAligned(const double* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4ValAligned(ptr);
        return reg;
    }

    inline void nsLoad4ValAligned(const double* ptr)
    {
        ymm = _mm256_load_pd(ptr);
    }

    static inline XMMReg4Double Load4Val(const float* ptr)
    {
        XMMReg4Double reg;
        reg.nsLoad4Val(ptr);
        return reg;
    }

    inline void nsLoad4Val(const float* ptr)
    {
        ymm = _mm256_cvtps_pd( _mm_loadu_ps(ptr) );
    }

    static inline XMMReg4Double Equals(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.ymm =  _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_EQ_OQ);
        return reg;
    }

    static inline XMMReg4Double NotEquals(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.ymm =  _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_NEQ_OQ);
        return reg;
    }

    static inline XMMReg4Double Greater(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.ymm =  _mm256_cmp_pd(expr1.ymm, expr2.ymm, _CMP_GT_OQ);
        return reg;
    }

    static inline XMMReg4Double And(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_and_pd(expr1.ymm, expr2.ymm);
        return reg;
    }

    static inline XMMReg4Double Ternary(const XMMReg4Double& cond, const XMMReg4Double& true_expr, const XMMReg4Double& false_expr)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_or_pd(_mm256_and_pd (cond.ymm, true_expr.ymm), _mm256_andnot_pd(cond.ymm, false_expr.ymm));
        return reg;
    }

    static inline XMMReg4Double Min(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.ymm = _mm256_min_pd(expr1.ymm, expr2.ymm);
        return reg;
    }

    inline XMMReg4Double& operator= (const XMMReg4Double& other)
    {
        ymm = other.ymm;
        return *this;
    }

    inline XMMReg4Double& operator+= (const XMMReg4Double& other)
    {
        ymm = _mm256_add_pd(ymm, other.ymm);
        return *this;
    }

    inline XMMReg4Double& operator*= (const XMMReg4Double& other)
    {
        ymm = _mm256_mul_pd(ymm, other.ymm);
        return *this;
    }

    inline XMMReg4Double operator+ (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_add_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator- (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_sub_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator* (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_mul_pd(ymm, other.ymm);
        return ret;
    }

    inline XMMReg4Double operator/ (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.ymm = _mm256_div_pd(ymm, other.ymm);
        return ret;
    }

    void AddToLow( const XMMReg2Double& other )
    {
         __m256d ymm2 = _mm256_setzero_pd();
         ymm2 = _mm256_insertf128_pd( ymm2, other.xmm, 0);
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

    inline void Store4Val(unsigned char* ptr) const
    {
        __m128i xmm_i = _mm256_cvttpd_epi32 (_mm256_add_pd(ymm, _mm256_set1_pd(0.5)));
        //xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
        //xmm_i = _mm_packus_epi16(xmm_i, xmm_i);  // Pack int16 to uint8
        xmm_i = _mm_shuffle_epi8(xmm_i, _mm_cvtsi32_si128(0 | (4 << 8) | (8 << 16) | (12 << 24))); //  SSSE3
        GDALCopyXMMToInt32(xmm_i, reinterpret_cast<GInt32*>(ptr));
    }

    inline void Store4Val(unsigned short* ptr) const
    {
        __m128i xmm_i = _mm256_cvttpd_epi32 (_mm256_add_pd(ymm, _mm256_set1_pd(0.5)));
        xmm_i = _mm_packus_epi32(xmm_i, xmm_i);   // Pack uint32 to uint16
        GDALCopyXMMToInt64(xmm_i, reinterpret_cast<GInt64*>(ptr));
    }

    inline void Store4Val(float* ptr) const
    {
        _mm_storeu_ps(ptr, _mm256_cvtpd_ps (ymm));
    }

    inline void Store4Val(double* ptr) const
    {
        _mm256_storeu_pd(ptr, ymm);
    }

    inline void StoreMask(unsigned char* ptr) const
    {
        _mm256_storeu_si256( reinterpret_cast<__m256i*>(ptr), _mm256_castpd_si256(ymm) );
    }
};

#else

class XMMReg4Double
{
  public:
    XMMReg2Double low, high;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
    /* coverity[uninit_member] */
    XMMReg4Double() = default;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    XMMReg4Double(const XMMReg4Double& other) : low(other.low), high(other.high) {}

    static inline XMMReg4Double Zero()
    {
        XMMReg4Double reg;
        reg.low.Zeroize();
        reg.high.Zeroize();
        return reg;
    }

    static inline XMMReg4Double Load1ValHighAndLow(const double* ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad1ValHighAndLow(ptr);
        reg.high = reg.low;
        return reg;
    }

    static inline XMMReg4Double Load4Val(const unsigned char* ptr)
    {
        XMMReg4Double reg;
        XMMReg2Double::Load4Val(ptr, reg.low, reg.high);
        return reg;
    }

    static inline XMMReg4Double Load4Val(const short* ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr+2);
        return reg;
    }

    static inline XMMReg4Double Load4Val(const unsigned short* ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr+2);
        return reg;
    }

    static inline XMMReg4Double Load4Val(const double* ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2Val(ptr);
        reg.high.nsLoad2Val(ptr+2);
        return reg;
    }

    static inline XMMReg4Double Load4ValAligned(const double* ptr)
    {
        XMMReg4Double reg;
        reg.low.nsLoad2ValAligned(ptr);
        reg.high.nsLoad2ValAligned(ptr+2);
        return reg;
    }

    static inline XMMReg4Double Load4Val(const float* ptr)
    {
        XMMReg4Double reg;
        XMMReg2Double::Load4Val(ptr, reg.low, reg.high);
        return reg;
    }

    static inline XMMReg4Double Equals(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Equals(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Equals(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double NotEquals(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::NotEquals(expr1.low, expr2.low);
        reg.high = XMMReg2Double::NotEquals(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double Greater(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Greater(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Greater(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double And(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::And(expr1.low, expr2.low);
        reg.high = XMMReg2Double::And(expr1.high, expr2.high);
        return reg;
    }

    static inline XMMReg4Double Ternary(const XMMReg4Double& cond, const XMMReg4Double& true_expr, const XMMReg4Double& false_expr)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Ternary(cond.low, true_expr.low, false_expr.low);
        reg.high = XMMReg2Double::Ternary(cond.high, true_expr.high, false_expr.high);
        return reg;
    }

    static inline XMMReg4Double Min(const XMMReg4Double& expr1, const XMMReg4Double& expr2)
    {
        XMMReg4Double reg;
        reg.low = XMMReg2Double::Min(expr1.low, expr2.low);
        reg.high = XMMReg2Double::Min(expr1.high, expr2.high);
        return reg;
    }

    inline XMMReg4Double& operator= (const XMMReg4Double& other)
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    inline XMMReg4Double& operator+= (const XMMReg4Double& other)
    {
        low += other.low;
        high += other.high;
        return *this;
    }

    inline XMMReg4Double& operator*= (const XMMReg4Double& other)
    {
        low *= other.low;
        high *= other.high;
        return *this;
    }

    inline XMMReg4Double operator+ (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.low = low + other.low;
        ret.high = high + other.high;
        return ret;
    }

    inline XMMReg4Double operator- (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.low = low - other.low;
        ret.high = high - other.high;
        return ret;
    }

    inline XMMReg4Double operator* (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.low = low * other.low;
        ret.high = high * other.high;
        return ret;
    }

    inline XMMReg4Double operator/ (const XMMReg4Double& other) const
    {
        XMMReg4Double ret;
        ret.low = low / other.low;
        ret.high = high / other.high;
        return ret;
    }

    void AddToLow( const XMMReg2Double& other )
    {
        low += other;
    }

    inline double GetHorizSum() const
    {
        return (low + high).GetHorizSum();
    }

    inline void Store4Val(unsigned char* ptr) const
    {
#ifdef USE_SSE2_EMULATION
        low.Store2Val(ptr);
        high.Store2Val(ptr+2);
#else
        __m128i tmpLow = _mm_cvttpd_epi32(_mm_add_pd(low.xmm, _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        __m128i tmpHigh = _mm_cvttpd_epi32(_mm_add_pd(high.xmm, _mm_set1_pd(0.5))); /* Convert the 2 double values to 2 integers */
        auto tmp = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(tmpLow), _mm_castsi128_ps(tmpHigh), _MM_SHUFFLE(1, 0, 1, 0)));
        tmp = _mm_packs_epi32(tmp, tmp);
        tmp = _mm_packus_epi16(tmp, tmp);
        GDALCopyXMMToInt32(tmp, reinterpret_cast<GInt32*>(ptr));
#endif
    }

    inline void Store4Val(unsigned short* ptr) const
    {
#if 1
        low.Store2Val(ptr);
        high.Store2Val(ptr+2);
#else
        __m128i xmm0 = _mm_cvtpd_epi32 (low.xmm);
        __m128i xmm1 = _mm_cvtpd_epi32 (high.xmm);
        xmm0 = _mm_or_si128(xmm0, _mm_slli_si128(xmm1, 8));
#if __SSE4_1__
        xmm0 = _mm_packus_epi32(xmm0, xmm0);   // Pack uint32 to uint16
#else
        xmm0 = _mm_add_epi32( xmm0, _mm_set1_epi32(-32768) );
        xmm0 = _mm_packs_epi32( xmm0, xmm0 );
        xmm0 = _mm_sub_epi16( xmm0, _mm_set1_epi16(-32768) );
#endif
        GDALCopyXMMToInt64(xmm0, (GInt64*)ptr);
#endif
    }

    inline void Store4Val(float* ptr) const
    {
        low.Store2Val(ptr);
        high.Store2Val(ptr+2);
    }

    inline void Store4Val(double* ptr) const
    {
        low.Store2Val(ptr);
        high.Store2Val(ptr+2);
    }

    inline void StoreMask(unsigned char* ptr) const
    {
        low.StoreMask(ptr);
        high.StoreMask(ptr+16);
    }

};

#endif

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDALSSE_PRIV_H_INCLUDED */
