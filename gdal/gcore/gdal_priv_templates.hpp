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
        /* coverity[same_on_both_sides] */
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
/*                         GDALIsValueInRange()                         */
/************************************************************************/
/**
 * Returns whether a value is in the type range.
 * NaN is considered not to be in type range.
 *
 * @param dfValue the value
 * @return whether the value is in the type range.
 */
template <class T> inline bool GDALIsValueInRange(double dfValue)
{
    return dfValue >= std::numeric_limits<T>::min() &&
           dfValue <= std::numeric_limits<T>::max();
}

template <> inline bool GDALIsValueInRange<double>(double dfValue)
{
    return !CPLIsNan(dfValue);
}

template <> inline bool GDALIsValueInRange<float>(double dfValue)
{
    return CPLIsInf(dfValue) ||
           (dfValue >= -std::numeric_limits<float>::max() &&
            dfValue <= std::numeric_limits<float>::max());
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
    if( dfValueIn > std::numeric_limits<float>::max() )
    {
        fValueOut = std::numeric_limits<float>::infinity();
        return;
    }
    if( dfValueIn < -std::numeric_limits<float>::max() )
    {
        fValueOut = -std::numeric_limits<float>::infinity();
        return;
    }

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

inline void GDALCopyWord(const float fValueIn, short &nValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, short>(fMaxVal, fMinVal);
    float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f :
        fValueIn - 0.5f;
    nValueOut = static_cast<short>(
        GDALClampValue(fValue, fMaxVal, fMinVal));
}

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

/************************************************************************/
/*                         GDALCopy4Words()                             */
/************************************************************************/
/**
 * Copy 4 packed words to 4 packed words, optionally rounding if appropriate
 * (i.e. going from the float to the integer case).
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

// Needs SSE2
// _mm_cvtsi128_si64 doesn't work gcc 3.4
#if (defined(__x86_64) || defined(_M_X64)) && !(defined(__GNUC__) && __GNUC__ < 4)

#include <emmintrin.h>

static inline void GDALCopyXMMToInt32(const __m128i xmm, void* pDest)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    int n32 = _mm_cvtsi128_si32 (xmm);     // Extract lower 32 bit word
    memcpy(pDest, &n32, sizeof(n32));
#else
    *(int*)pDest = _mm_cvtsi128_si32 (xmm);
#endif
}

static inline void GDALCopyXMMToInt64(const __m128i xmm, void* pDest)
{
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
    GInt64 n64 = _mm_cvtsi128_si64 (xmm);   // Extract lower 64 bit word
    memcpy(pDest, &n64, sizeof(n64));
#else
    *(GInt64*)pDest = _mm_cvtsi128_si64 (xmm);
#endif
}

#if __SSE4_1__
#include <smmintrin.h>
#endif

inline void GDALCopy4Words(const float* pValueIn, GByte* const &pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    // The following clamping would be useless due to the final saturating
    // packing if we could guarantee the input range in [INT_MIN,INT_MAX]
    const __m128 xmm_min = _mm_set1_ps(0);
    const __m128 xmm_max = _mm_set1_ps(255);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    xmm = _mm_add_ps(xmm, p0d5);

    __m128i xmm_i = _mm_cvttps_epi32 (xmm);

    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
    xmm_i = _mm_packus_epi16(xmm_i, xmm_i);  // Pack int16 to uint8
    GDALCopyXMMToInt32(xmm_i, pValueOut);
}

inline void GDALCopy4Words(const float* pValueIn, GInt16* const &pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    const __m128 xmm_min = _mm_set1_ps(-32768);
    const __m128 xmm_max = _mm_set1_ps(32767);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 m0d5 = _mm_set1_ps(-0.5f);
    const __m128 mask = _mm_cmpge_ps(xmm, p0d5);
    // f >= 0.5f ? f + 0.5f : f - 0.5f
    xmm = _mm_add_ps(xmm, _mm_or_ps(_mm_and_ps(mask, p0d5),
                                    _mm_andnot_ps(mask, m0d5)));

    __m128i xmm_i = _mm_cvttps_epi32 (xmm);

    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
    GDALCopyXMMToInt64(xmm_i, pValueOut);
}

inline void GDALCopy4Words(const float* pValueIn, GUInt16* const &pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    const __m128 xmm_min = _mm_set1_ps(0);
    const __m128 xmm_max = _mm_set1_ps(65535);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);

    xmm = _mm_add_ps(xmm, _mm_set1_ps(0.5f));

    __m128i xmm_i = _mm_cvttps_epi32 (xmm);

#if __SSE4_1__
     xmm_i = _mm_packus_epi32(xmm_i, xmm_i);   // Pack int32 to uint16
#else
    // Translate to int16 range because _mm_packus_epi32 is SSE4.1 only
    xmm_i = _mm_add_epi32(xmm_i, _mm_set1_epi32(-32768));
    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
    // Translate back to uint16 range (actually -32768==32768 in int16)
    xmm_i = _mm_add_epi16(xmm_i, _mm_set1_epi16(-32768));
#endif
    GDALCopyXMMToInt64(xmm_i, pValueOut);
}

#ifdef notdef_because_slightly_slower_than_default_implementation
inline void GDALCopy4Words(const double* pValueIn, float* const &pValueOut)
{
    __m128d float_posmax = _mm_set1_pd(std::numeric_limits<float>::max());
    __m128d float_negmax = _mm_set1_pd(-std::numeric_limits<float>::max());
    __m128d float_posinf = _mm_set1_pd(std::numeric_limits<float>::infinity());
    __m128d float_neginf = _mm_set1_pd(-std::numeric_limits<float>::infinity());
    __m128d val01 = _mm_loadu_pd(pValueIn);
    __m128d val23 = _mm_loadu_pd(pValueIn+2);
    __m128d mask_max = _mm_cmpge_pd( val01, float_posmax );
    __m128d mask_max23 = _mm_cmpge_pd( val23, float_posmax );
    val01 = _mm_or_pd(_mm_and_pd(mask_max, float_posinf), _mm_andnot_pd(mask_max, val01));
    val23 = _mm_or_pd(_mm_and_pd(mask_max23, float_posinf), _mm_andnot_pd(mask_max23, val23));
    __m128d mask_min = _mm_cmple_pd( val01, float_negmax );
    __m128d mask_min23 = _mm_cmple_pd( val23, float_negmax );
    val01 = _mm_or_pd(_mm_and_pd(mask_min, float_neginf), _mm_andnot_pd(mask_min, val01));
    val23 = _mm_or_pd(_mm_and_pd(mask_min23, float_neginf), _mm_andnot_pd(mask_min23, val23));
    __m128 val01_s =  _mm_cvtpd_ps ( val01);
    __m128 val23_s =  _mm_cvtpd_ps ( val23);
    __m128i val01_i = _mm_castps_si128(val01_s);
    __m128i val23_i = _mm_castps_si128(val23_s);
    GDALCopyXMMToInt64(val01_i, pValueOut);
    GDALCopyXMMToInt64(val23_i, pValueOut+2);
}
#endif

#endif //  defined(__x86_64) || defined(_M_X64)

#endif // GDAL_PRIV_TEMPLATES_HPP_INCLUDED
