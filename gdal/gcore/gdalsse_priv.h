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

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* Could possibly be used too on 32bit, but we would need to check at runtime */
#if (defined(__x86_64) || defined(_M_X64)) && !defined(USE_SSE2_EMULATION)

/* Requires SSE2 */
#include <emmintrin.h>
#include <string.h>

class XMMReg2Double
{
  public:
    __m128d xmm;

    XMMReg2Double() {}
    XMMReg2Double(double  val)  { xmm = _mm_load_sd (&val); }
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

    inline void nsLoad2ValAligned(const double* pval)
    {
        xmm = _mm_load_pd(pval);
    }

    inline void nsLoad2Val(const float* pval)
    {
        __m128 temp1 = _mm_load_ss(pval);
        __m128 temp2 = _mm_load_ss(pval + 1);
        temp1 = _mm_shuffle_ps(temp1, temp2, _MM_SHUFFLE(1,0,1,0));
        temp1 = _mm_shuffle_ps(temp1, temp1, _MM_SHUFFLE(3,3,2,0));
        xmm = _mm_cvtps_pd(temp1);
    }

    inline void nsLoad2Val(const unsigned char* ptr)
    {
        __m128i xmm_i = _mm_cvtsi32_si128(*(unsigned short*)(ptr));
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const short* ptr)
    {
        int i;
        memcpy(&i, ptr, 4);
        __m128i xmm_i = _mm_cvtsi32_si128(i);
        xmm_i = _mm_unpacklo_epi16(xmm_i,xmm_i); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|b|b|a|a */
        xmm_i = _mm_srai_epi32(xmm_i, 16);       /* 0|0|0|0|b|b|a|a --> 0|0|0|0|sign(b)|b|sign(a)|a */
        xmm = _mm_cvtepi32_pd(xmm_i);
    }

    inline void nsLoad2Val(const unsigned short* ptr)
    {
        int i;
        memcpy(&i, ptr, 4);
        __m128i xmm_i = _mm_cvtsi32_si128(i);
        xmm_i = _mm_unpacklo_epi16(xmm_i,xmm_i); /* 0|0|0|0|0|0|b|a --> 0|0|0|0|b|b|a|a */
        xmm_i = _mm_srli_epi32(xmm_i, 16);       /* 0|0|0|0|b|b|a|a --> 0|0|0|0|0|b|0|a */
        xmm = _mm_cvtepi32_pd(xmm_i);
    }
    
    static inline void Load4Val(const unsigned char* ptr, XMMReg2Double& low, XMMReg2Double& high)
    {
        __m128i xmm_i = _mm_cvtsi32_si128(*(int*)(ptr));
        xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
        xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
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

    inline void AddLowAndHigh()
    {
        __m128d xmm2;
        xmm2 = _mm_shuffle_pd(xmm,xmm,_MM_SHUFFLE2(0,1)); /* transfer high word into low word of xmm2 */
        xmm = _mm_add_pd(xmm, xmm2);
    }
    
    inline void Store2Double(double* pval) const
    {
        _mm_storeu_pd(pval, xmm);
    }
    
    inline void Store2DoubleAligned(double* pval) const
    {
        _mm_store_pd(pval, xmm);
    }

    void Store2Val(unsigned short* ptr) const
    {
        __m128i tmp = _mm_cvtpd_epi32(xmm); /* Convert the 2 double values to 2 integers */
        ptr[0] = (GUInt16)_mm_extract_epi16(tmp, 0);
        ptr[1] = (GUInt16)_mm_extract_epi16(tmp, 2);
    }
    
    inline operator double () const
    {
        double val;
        _mm_store_sd(&val, xmm);
        return val;
    }
};

#else

#warning "Software emulation of SSE2 !"

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
        reg.low = (expr1.low < expr2.low) ? expr1.low : expr2.high;
        reg.high = (expr1.high < expr2.high) ? expr1.high : expr2.low;
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

    inline void nsLoad1ValHighAndLow(const double* pval)
    {
        low = pval[0];
        high = pval[0];
    }
    
    inline void nsLoad2Val(const double* pval)
    {
        low = pval[0];
        high = pval[1];
    }

    inline void nsLoad2ValAligned(const double* pval)
    {
        low = pval[0];
        high = pval[1];
    }

    inline void nsLoad2Val(const float* pval)
    {
        low = pval[0];
        high = pval[1];
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

    inline void AddLowAndHigh()
    {
        double add = low + high;
        low = add;
        high = add;
    }

    inline void Store2Double(double* pval) const
    {
        pval[0] = low;
        pval[1] = high;
    }
    
    inline void Store2DoubleAligned(double* pval) const
    {
        pval[0] = low;
        pval[1] = high;
    }

    void Store2Val(unsigned short* ptr) const
    {
        ptr[0] = (GUInt16)low;
        ptr[1] = (GUInt16)high;
    }
    
    inline operator double () const
    {
        return low;
    }
};

#endif /*  defined(__x86_64) || defined(_M_X64) */

class XMMReg4Double
{
  public:
    XMMReg2Double low, high;

    XMMReg4Double() {}
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

    inline void AddLowAndHigh()
    {
        low = low + high;
        low.AddLowAndHigh();
    }

    inline XMMReg2Double& GetLow()
    {
        return low;
    }

    inline XMMReg2Double& GetHigh()
    {
        return high;
    }

    void Store4Val(unsigned short* ptr) const
    {
        low.Store2Val(ptr);
        high.Store2Val(ptr+2);
    }
};

#endif /* GDALSSE_PRIV_H_INCLUDED */
