/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Inline C++ templates
 * Author:   Phil Vachon, <philippe at cowpig.ca>
 *
 ******************************************************************************
 * Copyright (c) 2009, Phil Vachon, <philippe at cowpig.ca>
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

#ifndef GDAL_PRIV_TEMPLATES_HPP_INCLUDED
#define GDAL_PRIV_TEMPLATES_HPP_INCLUDED

#define SSE_USE_SAME_ROUNDING_AS_NON_SSE

#include <limits>

/************************************************************************/
/*                        GDALGetDataLimits()                           */
/************************************************************************/
/**
 * Compute the limits of values that can be placed in Tout in terms of
 * Tin. Usually used for output clamping, when the output data type's
 * limits are stable relative to the input type (i.e. no roundoff error).
 *
 * @param tMaxValue the returned maximum value
 * @param tMinValue the returned minimum value
 */

template <class Tin, class Tout>
inline void GDALGetDataLimits(Tin &tMaxValue, Tin &tMinValue)
{
    tMaxValue = std::numeric_limits<Tin>::max();
    tMinValue = std::numeric_limits<Tin>::min();

    // Compute the actual minimum value of Tout in terms of Tin.
    if (std::numeric_limits<Tout>::is_signed && std::numeric_limits<Tout>::is_integer)
    {
        // the minimum value is less than zero
        if (std::numeric_limits<Tout>::digits < std::numeric_limits<Tin>::digits ||
                        !std::numeric_limits<Tin>::is_integer)
        {
            // Tout is smaller than Tin, so we need to clamp values in input
            // to the range of Tout's min/max values
            if (std::numeric_limits<Tin>::is_signed)
            {
                tMinValue = static_cast<Tin>(std::numeric_limits<Tout>::min());
            }
            tMaxValue = static_cast<Tin>(std::numeric_limits<Tout>::max());
        }
    }
    else if (std::numeric_limits<Tout>::is_integer)
    {
        // the output is unsigned, so we just need to determine the max
        if (std::numeric_limits<Tout>::digits <= std::numeric_limits<Tin>::digits)
        {
            // Tout is smaller than Tin, so we need to clamp the input values
            // to the range of Tout's max
            tMaxValue = static_cast<Tin>(std::numeric_limits<Tout>::max());
        }
        tMinValue = 0;
    }

}

/************************************************************************/
/*                          GDALClampValue()                            */
/************************************************************************/
/**
 * Clamp values of type T to a specified range
 *
 * @param tValue the value
 * @param tMax the max value
 * @param tMin the min value
 */
template <class T>
inline T GDALClampValue(const T tValue, const T tMax, const T tMin)
{
    return tValue > tMax ? tMax :
           tValue < tMin ? tMin : tValue;
}

/************************************************************************/
/*                          GDALCopyWord()                              */
/************************************************************************/
/**
 * Copy a single word, optionally rounding if appropriate (i.e. going
 * from the float to the integer case). Note that this is the function
 * you should specialize if you're adding a new data type.
 *
 * @param tValueIn value of type Tin; the input value to be converted
 * @param tValueOut value of type Tout; the output value
 */

template <class Tin, class Tout>
inline void GDALCopyWord(const Tin tValueIn, Tout &tValueOut)
{
    Tin tMaxVal, tMinVal;
    GDALGetDataLimits<Tin, Tout>(tMaxVal, tMinVal);
    tValueOut = static_cast<Tout>(GDALClampValue(tValueIn, tMaxVal, tMinVal));
}

template <class Tin>
inline void GDALCopyWord(const Tin tValueIn, float &fValueOut)
{
    fValueOut = (float) tValueIn;
}

template <class Tin>
inline void GDALCopyWord(const Tin tValueIn, double &dfValueOut)
{
    dfValueOut = tValueIn;
}

inline void GDALCopyWord(const double dfValueIn, double &dfValueOut)
{
    dfValueOut = dfValueIn;
}

inline void GDALCopyWord(const float fValueIn, float &fValueOut)
{
    fValueOut = fValueIn;
}

inline void GDALCopyWord(const float fValueIn, double &dfValueOut)
{
    dfValueOut = fValueIn;
}

inline void GDALCopyWord(const double dfValueIn, float &fValueOut)
{
    fValueOut = static_cast<float>(dfValueIn);
}

template <class Tout>
inline void GDALCopyWord(const float fValueIn, Tout &tValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, Tout>(fMaxVal, fMinVal);
    tValueOut = static_cast<Tout>(
        GDALClampValue(fValueIn + 0.5f, fMaxVal, fMinVal));
}

// Just need SSE, but don't bother being too specific
#if (defined(__x86_64) || defined(_M_X64)) && defined(this_is_disabled)

#include <xmmintrin.h>

template <class Tout>
inline void GDALCopyWordSSE(const float fValueIn, Tout &tValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, Tout>(fMaxVal, fMinVal);
    __m128 xmm = _mm_set_ss(fValueIn);
#ifdef SSE_USE_SAME_ROUNDING_AS_NON_SSE
    __m128 mask = _mm_cmpge_ss(xmm, _mm_set_ss(0.f));
    __m128 p0d5 = _mm_set_ss(0.5f);
    __m128 m0d5 = _mm_set_ss(-0.5f);
    xmm = _mm_add_ss(xmm, _mm_or_ps(_mm_and_ps(mask, p0d5), _mm_andnot_ps(mask, m0d5)));
#endif
    __m128 xmm_min = _mm_set_ss(fMinVal);
    __m128 xmm_max = _mm_set_ss(fMaxVal);
    xmm = _mm_min_ss(_mm_max_ss(xmm, xmm_min), xmm_max);
#ifdef SSE_USE_SAME_ROUNDING_AS_NON_SSE
    tValueOut = (Tout)_mm_cvttss_si32(xmm);
#else
    tValueOut = (Tout)_mm_cvtss_si32(xmm);
#endif
}

inline void GDALCopyWord(const float fValueIn, GByte &tValueOut)
{
    GDALCopyWordSSE(fValueIn, tValueOut);
}

inline void GDALCopyWord(const float fValueIn, GInt16 &tValueOut)
{
    GDALCopyWordSSE(fValueIn, tValueOut);
}

inline void GDALCopyWord(const float fValueIn, GUInt16 &tValueOut)
{
    GDALCopyWordSSE(fValueIn, tValueOut);
}

#else

inline void GDALCopyWord(const float fValueIn, short &nValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, short>(fMaxVal, fMinVal);
    float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f :
        fValueIn - 0.5f;
    nValueOut = static_cast<short>(
        GDALClampValue(fValue, fMaxVal, fMinVal));
}

#endif //  defined(__x86_64) || defined(_M_X64)

template <class Tout>
inline void GDALCopyWord(const double dfValueIn, Tout &tValueOut)
{
    double dfMaxVal, dfMinVal;
    GDALGetDataLimits<double, Tout>(dfMaxVal, dfMinVal);
    tValueOut = static_cast<Tout>(
        GDALClampValue(dfValueIn + 0.5, dfMaxVal, dfMinVal));
}

inline void GDALCopyWord(const double dfValueIn, int &nValueOut)
{
    double dfMaxVal, dfMinVal;
    GDALGetDataLimits<double, int>(dfMaxVal, dfMinVal);
    double dfValue = dfValueIn >= 0.0 ? dfValueIn + 0.5 :
        dfValueIn - 0.5;
    nValueOut = static_cast<int>(
        GDALClampValue(dfValue, dfMaxVal, dfMinVal));
}

inline void GDALCopyWord(const double dfValueIn, short &nValueOut)
{
    double dfMaxVal, dfMinVal;
    GDALGetDataLimits<double, short>(dfMaxVal, dfMinVal);
    double dfValue = dfValueIn > 0.0 ? dfValueIn + 0.5 :
        dfValueIn - 0.5;
    nValueOut = static_cast<short>(
        GDALClampValue(dfValue, dfMaxVal, dfMinVal));
}

// Roundoff occurs for Float32 -> int32 for max/min. Overload GDALCopyWord
// specifically for this case.
inline void GDALCopyWord(const float fValueIn, int &nValueOut)
{
    if (fValueIn >= static_cast<float>(std::numeric_limits<int>::max()))
    {
        nValueOut = std::numeric_limits<int>::max();
    }
    else if (fValueIn <= static_cast<float>(std::numeric_limits<int>::min()))
    {
        nValueOut = std::numeric_limits<int>::min();
    }
    else
    {
        nValueOut = static_cast<int>(fValueIn > 0.0f ? 
            fValueIn + 0.5f : fValueIn - 0.5f);
    }
}

// Roundoff occurs for Float32 -> uint32 for max. Overload GDALCopyWord
// specifically for this case.
inline void GDALCopyWord(const float fValueIn, unsigned int &nValueOut)
{
    if (fValueIn >= static_cast<float>(std::numeric_limits<unsigned int>::max()))
    {
        nValueOut = std::numeric_limits<unsigned int>::max();
    }
    else if (fValueIn <= static_cast<float>(std::numeric_limits<unsigned int>::min()))
    {
        nValueOut = std::numeric_limits<unsigned int>::min();
    }
    else
    {
        nValueOut = static_cast<unsigned int>(fValueIn + 0.5f);
    }
}

#ifdef notdef
/************************************************************************/
/*                         GDALCopy2Words()                             */
/************************************************************************/
/**
 * Copy 2 words, optionally rounding if appropriate (i.e. going
 * from the float to the integer case).
 *
 * @param pValueIn pointer to 2 input values of type Tin.
 * @param pValueOut pointer to 2 output values of type Tout.
 */

template <class Tin, class Tout>
inline void GDALCopy2Words(const Tin* pValueIn, Tout* const &pValueOut)
{
    GDALCopyWord(pValueIn[0], pValueOut[0]);
    GDALCopyWord(pValueIn[1], pValueOut[1]);
}

// Just need SSE, but don't bother being too specific
#if defined(__x86_64) || defined(_M_X64)

#include <xmmintrin.h>

template <class Tout>
inline void GDALCopy2WordsSSE(const float* pValueIn, Tout* const &pValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, Tout>(fMaxVal, fMinVal);
    __m128 xmm = _mm_set_ps(0, 0, pValueIn[1], pValueIn[0]);
    __m128 xmm_min = _mm_set_ps(0, 0, fMinVal, fMinVal);
    __m128 xmm_max = _mm_set_ps(0, 0, fMaxVal, fMaxVal);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);
    pValueOut[0] = _mm_cvtss_si32(xmm);
    pValueOut[1] = _mm_cvtss_si32(_mm_shuffle_ps(xmm, xmm, _MM_SHUFFLE(0, 0, 0, 1)));
}

inline void GDALCopy2Words(const float* pValueIn, GByte* const &pValueOut)
{
    GDALCopy2WordsSSE(pValueIn, pValueOut);
}

inline void GDALCopy2Words(const float* pValueIn, GInt16* const &pValueOut)
{
    GDALCopy2WordsSSE(pValueIn, pValueOut);
}

inline void GDALCopy2Words(const float* pValueIn, GUInt16* const &pValueOut)
{
    GDALCopy2WordsSSE(pValueIn, pValueOut);
}
#endif //  defined(__x86_64) || defined(_M_X64)

#endif

/************************************************************************/
/*                         GDALCopy4Words()                             */
/************************************************************************/
/**
 * Copy 4 words, optionally rounding if appropriate (i.e. going
 * from the float to the integer case).
 *
 * @param pValueIn pointer to 4 input values of type Tin.
 * @param pValueOut pointer to 4 output values of type Tout.
 */

template <class Tin, class Tout>
inline void GDALCopy4Words(const Tin* pValueIn, Tout* const &pValueOut)
{
    GDALCopyWord(pValueIn[0], pValueOut[0]);
    GDALCopyWord(pValueIn[1], pValueOut[1]);
    GDALCopyWord(pValueIn[2], pValueOut[2]);
    GDALCopyWord(pValueIn[3], pValueOut[3]);
}

// Needs SSE2 for _mm_cvtps_epi32 and store operations
#if defined(__x86_64) || defined(_M_X64)

#include <emmintrin.h>

template <class Tout>
inline void GDALCopy4WordsSSE(const float* pValueIn, Tout* const &pValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, Tout>(fMaxVal, fMinVal);
    __m128 xmm = _mm_loadu_ps(pValueIn);
#ifdef SSE_USE_SAME_ROUNDING_AS_NON_SSE
    __m128 p0d5 = _mm_set1_ps(0.5f);
    __m128 m0d5 = _mm_set1_ps(-0.5f);
    //__m128 mask = _mm_cmpge_ps(xmm, _mm_set1_ps(0.f));
    __m128 mask = _mm_cmpge_ps(xmm, p0d5);
    xmm = _mm_add_ps(xmm, _mm_or_ps(_mm_and_ps(mask, p0d5), _mm_andnot_ps(mask, m0d5))); /* f >= 0.5f ? f + 0.5f : f - 0.5f */
#endif
    __m128 xmm_min = _mm_set1_ps(fMinVal);
    __m128 xmm_max = _mm_set1_ps(fMaxVal);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);
#ifdef SSE_USE_SAME_ROUNDING_AS_NON_SSE
    __m128i xmm_i = _mm_cvttps_epi32 (xmm);
#else
    __m128i xmm_i = _mm_cvtps_epi32(xmm);
#endif
#if 0
    int aTemp[4];
    _mm_storeu_si128 ( (__m128i *)aTemp, xmm_i);
    pValueOut[0] = (Tout)aTemp[0];
    pValueOut[1] = (Tout)aTemp[1];
    pValueOut[2] = (Tout)aTemp[2];
    pValueOut[3] = (Tout)aTemp[3];
#else
    pValueOut[0] = (Tout)_mm_extract_epi16(xmm_i, 0);
    pValueOut[1] = (Tout)_mm_extract_epi16(xmm_i, 2);
    pValueOut[2] = (Tout)_mm_extract_epi16(xmm_i, 4);
    pValueOut[3] = (Tout)_mm_extract_epi16(xmm_i, 6);
#endif
}

inline void GDALCopy4Words(const float* pValueIn, GByte* const &pValueOut)
{
    GDALCopy4WordsSSE(pValueIn, pValueOut);
}

inline void GDALCopy4Words(const float* pValueIn, GInt16* const &pValueOut)
{
    GDALCopy4WordsSSE(pValueIn, pValueOut);
}

inline void GDALCopy4Words(const float* pValueIn, GUInt16* const &pValueOut)
{
    GDALCopy4WordsSSE(pValueIn, pValueOut);
}
#endif //  defined(__x86_64) || defined(_M_X64)

#endif // GDAL_PRIV_TEMPLATES_HPP_INCLUDED
