/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Inline C++ templates
 * Author:   Phil Vachon, <philippe at cowpig.ca>
 *
 ******************************************************************************
 * Copyright (c) 2009, Phil Vachon, <philippe at cowpig.ca>
 * Copyright (c) 2025, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_PRIV_TEMPLATES_HPP_INCLUDED
#define GDAL_PRIV_TEMPLATES_HPP_INCLUDED

#include "cpl_port.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "cpl_float.h"

// Needs SSE2
#if defined(__x86_64) || defined(_M_X64) || defined(USE_SSE2) ||               \
    defined(USE_NEON_OPTIMIZATIONS)

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"
#else
#include <immintrin.h>
#endif

static inline void GDALCopyXMMToInt32(const __m128i xmm, void *pDest)
{
    int n32 = _mm_cvtsi128_si32(xmm);  // Extract lower 32 bit word
    memcpy(pDest, &n32, sizeof(n32));
}

static inline void GDALCopyXMMToInt64(const __m128i xmm, void *pDest)
{
    _mm_storel_epi64(reinterpret_cast<__m128i *>(pDest), xmm);
}

#if __SSSE3__
#include <tmmintrin.h>
#endif

#if defined(__SSE4_1__) || defined(__AVX__)
#include <smmintrin.h>
#endif

#ifdef __F16C__
#include <immintrin.h>
#endif

#endif

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
    tMaxValue = cpl::NumericLimits<Tin>::max();
    tMinValue = cpl::NumericLimits<Tin>::lowest();

    // Compute the actual minimum value of Tout in terms of Tin.
    if constexpr (cpl::NumericLimits<Tout>::is_signed &&
                  cpl::NumericLimits<Tout>::is_integer)
    {
        // the minimum value is less than zero
        // cppcheck-suppress knownConditionTrueFalse
        if constexpr (cpl::NumericLimits<Tout>::digits <
                          cpl::NumericLimits<Tin>::digits ||
                      !cpl::NumericLimits<Tin>::is_integer)
        {
            // Tout is smaller than Tin, so we need to clamp values in input
            // to the range of Tout's min/max values
            if constexpr (cpl::NumericLimits<Tin>::is_signed)
            {
                tMinValue =
                    static_cast<Tin>(cpl::NumericLimits<Tout>::lowest());
            }
            tMaxValue = static_cast<Tin>(cpl::NumericLimits<Tout>::max());
        }
    }
    else if constexpr (cpl::NumericLimits<Tout>::is_integer)
    {
        // the output is unsigned, so we just need to determine the max
        if constexpr (!std::is_same_v<Tin, Tout> &&
                      cpl::NumericLimits<Tout>::digits <=
                          cpl::NumericLimits<Tin>::digits)
        {
            // Tout is smaller than Tin, so we need to clamp the input values
            // to the range of Tout's max
            tMaxValue = static_cast<Tin>(cpl::NumericLimits<Tout>::max());
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
    return tValue > tMax ? tMax : tValue < tMin ? tMin : tValue;
}

/************************************************************************/
/*                          GDALClampDoubleValue()                            */
/************************************************************************/
/**
 * Clamp double values to a specified range, this uses the same
 * argument ordering as std::clamp, returns TRUE if the value was clamped.
 *
 * @param tValue the value
 * @param tMin the min value
 * @param tMax the max value
 *
 */
template <class T2, class T3>
inline bool GDALClampDoubleValue(double &tValue, const T2 tMin, const T3 tMax)
{
    const double tMin2{static_cast<double>(tMin)};
    const double tMax2{static_cast<double>(tMax)};
    if (tValue > tMax2 || tValue < tMin2)
    {
        tValue = tValue > tMax2 ? tMax2 : tValue < tMin2 ? tMin2 : tValue;
        return true;
    }
    else
    {
        return false;
    }
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
    return dfValue >= static_cast<double>(cpl::NumericLimits<T>::lowest()) &&
           dfValue <= static_cast<double>(cpl::NumericLimits<T>::max());
}

template <> inline bool GDALIsValueInRange<double>(double dfValue)
{
    return !CPLIsNan(dfValue);
}

template <> inline bool GDALIsValueInRange<float>(double dfValue)
{
    return CPLIsInf(dfValue) ||
           (dfValue >=
                -static_cast<double>(std::numeric_limits<float>::max()) &&
            dfValue <= static_cast<double>(std::numeric_limits<float>::max()));
}

template <> inline bool GDALIsValueInRange<GFloat16>(double dfValue)
{
    return CPLIsInf(dfValue) ||
           (dfValue >= -cpl::NumericLimits<GFloat16>::max() &&
            dfValue <= cpl::NumericLimits<GFloat16>::max());
}

template <> inline bool GDALIsValueInRange<int64_t>(double dfValue)
{
    // Values in the range [INT64_MAX - 1023, INT64_MAX - 1]
    // get converted to a double that once cast to int64_t is
    // INT64_MAX + 1, hence the < strict comparison.
    return dfValue >=
               static_cast<double>(cpl::NumericLimits<int64_t>::lowest()) &&
           dfValue < static_cast<double>(cpl::NumericLimits<int64_t>::max());
}

template <> inline bool GDALIsValueInRange<uint64_t>(double dfValue)
{
    // Values in the range [UINT64_MAX - 2047, UINT64_MAX - 1]
    // get converted to a double that once cast to uint64_t is
    // UINT64_MAX + 1, hence the < strict comparison.
    return dfValue >= 0 &&
           dfValue < static_cast<double>(cpl::NumericLimits<uint64_t>::max());
}

/************************************************************************/
/*                         GDALIsValueExactAs()                         */
/************************************************************************/
/**
 * Returns whether a value can be exactly represented on type T.
 *
 * That is static_cast\<double\>(static_cast\<T\>(dfValue)) is legal and is
 * equal to dfValue.
 *
 * Note: for T=float or double, a NaN input leads to true
 *
 * @param dfValue the value
 * @return whether the value can be exactly represented on type T.
 */
template <class T> inline bool GDALIsValueExactAs(double dfValue)
{
    return GDALIsValueInRange<T>(dfValue) &&
           static_cast<double>(static_cast<T>(dfValue)) == dfValue;
}

template <> inline bool GDALIsValueExactAs<float>(double dfValue)
{
    return CPLIsNan(dfValue) ||
           (GDALIsValueInRange<float>(dfValue) &&
            static_cast<double>(static_cast<float>(dfValue)) == dfValue);
}

template <> inline bool GDALIsValueExactAs<GFloat16>(double dfValue)
{
    return CPLIsNan(dfValue) ||
           (GDALIsValueInRange<GFloat16>(dfValue) &&
            static_cast<double>(static_cast<GFloat16>(dfValue)) == dfValue);
}

template <> inline bool GDALIsValueExactAs<double>(double)
{
    return true;
}

/************************************************************************/
/*                          GDALCopyWord()                              */
/************************************************************************/

// Integer input and output: clamp the input

template <class Tin, class Tout> struct sGDALCopyWord
{
    static inline void f(const Tin tValueIn, Tout &tValueOut)
    {
        Tin tMaxVal, tMinVal;
        GDALGetDataLimits<Tin, Tout>(tMaxVal, tMinVal);
        tValueOut =
            static_cast<Tout>(GDALClampValue(tValueIn, tMaxVal, tMinVal));
    }
};

// Integer input and floating point output: simply convert

template <class Tin> struct sGDALCopyWord<Tin, GFloat16>
{
    static inline void f(const Tin tValueIn, GFloat16 &hfValueOut)
    {
        hfValueOut = static_cast<GFloat16>(tValueIn);
    }
};

template <class Tin> struct sGDALCopyWord<Tin, float>
{
    static inline void f(const Tin tValueIn, float &fValueOut)
    {
        fValueOut = static_cast<float>(tValueIn);
    }
};

template <class Tin> struct sGDALCopyWord<Tin, double>
{
    static inline void f(const Tin tValueIn, double &dfValueOut)
    {
        dfValueOut = static_cast<double>(tValueIn);
    }
};

// Floating point input and output, converting between identical types: simply copy

template <> struct sGDALCopyWord<GFloat16, GFloat16>
{
    static inline void f(const GFloat16 hfValueIn, GFloat16 &hfValueOut)
    {
        hfValueOut = hfValueIn;
    }
};

template <> struct sGDALCopyWord<float, float>
{
    static inline void f(const float fValueIn, float &fValueOut)
    {
        fValueOut = fValueIn;
    }
};

template <> struct sGDALCopyWord<double, double>
{
    static inline void f(const double dfValueIn, double &dfValueOut)
    {
        dfValueOut = dfValueIn;
    }
};

// Floating point input and output, converting to a larger type: use implicit conversion

template <> struct sGDALCopyWord<GFloat16, float>
{
    static inline void f(const GFloat16 hfValueIn, float &dfValueOut)
    {
        dfValueOut = hfValueIn;
    }
};

template <> struct sGDALCopyWord<GFloat16, double>
{
    static inline void f(const GFloat16 hfValueIn, double &dfValueOut)
    {
        dfValueOut = hfValueIn;
    }
};

template <> struct sGDALCopyWord<float, double>
{
    static inline void f(const float fValueIn, double &dfValueOut)
    {
        dfValueOut = static_cast<double>(fValueIn);
    }
};

// Floating point input and out, converting to a smaller type: ensure overflow results in infinity

template <> struct sGDALCopyWord<float, GFloat16>
{
    static inline void f(const float fValueIn, GFloat16 &hfValueOut)
    {
        // Our custom implementation when std::float16_t is not
        // available ensures proper behavior.
#if !defined(HAVE_STD_FLOAT16_T)
        if (fValueIn > cpl::NumericLimits<GFloat16>::max())
        {
            hfValueOut = cpl::NumericLimits<GFloat16>::infinity();
            return;
        }
        if (fValueIn < -cpl::NumericLimits<GFloat16>::max())
        {
            hfValueOut = -cpl::NumericLimits<GFloat16>::infinity();
            return;
        }
#endif
        hfValueOut = static_cast<GFloat16>(fValueIn);
    }
};

template <> struct sGDALCopyWord<double, GFloat16>
{
    static inline void f(const double dfValueIn, GFloat16 &hfValueOut)
    {
        // Our custom implementation when std::float16_t is not
        // available ensures proper behavior.
#if !defined(HAVE_STD_FLOAT16_T)
        if (dfValueIn > cpl::NumericLimits<GFloat16>::max())
        {
            hfValueOut = cpl::NumericLimits<GFloat16>::infinity();
            return;
        }
        if (dfValueIn < -cpl::NumericLimits<GFloat16>::max())
        {
            hfValueOut = -cpl::NumericLimits<GFloat16>::infinity();
            return;
        }
#endif
        hfValueOut = static_cast<GFloat16>(dfValueIn);
    }
};

template <> struct sGDALCopyWord<double, float>
{
    static inline void f(const double dfValueIn, float &fValueOut)
    {
#if defined(__x86_64) || defined(_M_X64) || defined(USE_SSE2)
        // We could just write fValueOut = static_cast<float>(dfValueIn);
        // but a sanitizer might complain with values above FLT_MAX
        _mm_store_ss(&fValueOut,
                     _mm_cvtsd_ss(_mm_undefined_ps(), _mm_load_sd(&dfValueIn)));
#else
        if (dfValueIn > static_cast<double>(std::numeric_limits<float>::max()))
        {
            fValueOut = std::numeric_limits<float>::infinity();
            return;
        }
        if (dfValueIn < static_cast<double>(-std::numeric_limits<float>::max()))
        {
            fValueOut = -std::numeric_limits<float>::infinity();
            return;
        }

        fValueOut = static_cast<float>(dfValueIn);
#endif
    }
};

// Floating point input to a small unsigned integer type: nan becomes zero, otherwise round and clamp

template <class Tout> struct sGDALCopyWord<GFloat16, Tout>
{
    static inline void f(const GFloat16 hfValueIn, Tout &tValueOut)
    {
        if (CPLIsNan(hfValueIn))
        {
            tValueOut = 0;
            return;
        }
        GFloat16 hfMaxVal, hfMinVal;
        GDALGetDataLimits<GFloat16, Tout>(hfMaxVal, hfMinVal);
        tValueOut = static_cast<Tout>(
            GDALClampValue(hfValueIn + GFloat16(0.5f), hfMaxVal, hfMinVal));
    }
};

template <class Tout> struct sGDALCopyWord<float, Tout>
{
    static inline void f(const float fValueIn, Tout &tValueOut)
    {
        if (CPLIsNan(fValueIn))
        {
            tValueOut = 0;
            return;
        }
        float fMaxVal, fMinVal;
        GDALGetDataLimits<float, Tout>(fMaxVal, fMinVal);
        tValueOut = static_cast<Tout>(
            GDALClampValue(fValueIn + 0.5f, fMaxVal, fMinVal));
    }
};

template <class Tout> struct sGDALCopyWord<double, Tout>
{
    static inline void f(const double dfValueIn, Tout &tValueOut)
    {
        if (CPLIsNan(dfValueIn))
        {
            tValueOut = 0;
            return;
        }
        double dfMaxVal, dfMinVal;
        GDALGetDataLimits<double, Tout>(dfMaxVal, dfMinVal);
        tValueOut = static_cast<Tout>(
            GDALClampValue(dfValueIn + 0.5, dfMaxVal, dfMinVal));
    }
};

// Floating point input to a large unsigned integer type: nan becomes zero, otherwise round and clamp.
// Avoid roundoff while clamping.

template <> struct sGDALCopyWord<GFloat16, std::uint64_t>
{
    static inline void f(const GFloat16 hfValueIn, std::uint64_t &nValueOut)
    {
        if (!(hfValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (CPLIsInf(hfValueIn))
        {
            nValueOut = cpl::NumericLimits<std::uint64_t>::max();
        }
        else
        {
            nValueOut = static_cast<std::uint64_t>(hfValueIn + GFloat16(0.5f));
        }
    }
};

template <> struct sGDALCopyWord<float, unsigned int>
{
    static inline void f(const float fValueIn, unsigned int &nValueOut)
    {
        if (!(fValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (fValueIn >=
                 static_cast<float>(cpl::NumericLimits<unsigned int>::max()))
        {
            nValueOut = cpl::NumericLimits<unsigned int>::max();
        }
        else
        {
            nValueOut = static_cast<unsigned int>(fValueIn + 0.5f);
        }
    }
};

template <> struct sGDALCopyWord<float, std::uint64_t>
{
    static inline void f(const float fValueIn, std::uint64_t &nValueOut)
    {
        if (!(fValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (fValueIn >=
                 static_cast<float>(cpl::NumericLimits<std::uint64_t>::max()))
        {
            nValueOut = cpl::NumericLimits<std::uint64_t>::max();
        }
        else
        {
            nValueOut = static_cast<std::uint64_t>(fValueIn + 0.5f);
        }
    }
};

template <> struct sGDALCopyWord<double, std::uint64_t>
{
    static inline void f(const double dfValueIn, std::uint64_t &nValueOut)
    {
        if (!(dfValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (dfValueIn >
                 static_cast<double>(cpl::NumericLimits<uint64_t>::max()))
        {
            nValueOut = cpl::NumericLimits<uint64_t>::max();
        }
        else
        {
            nValueOut = static_cast<std::uint64_t>(dfValueIn + 0.5);
        }
    }
};

// Floating point input to a very large unsigned integer type: nan becomes zero, otherwise round and clamp.
// Avoid infinity while clamping when the maximum integer is too large for the floating-point type.
// Avoid roundoff while clamping.

template <> struct sGDALCopyWord<GFloat16, unsigned short>
{
    static inline void f(const GFloat16 hfValueIn, unsigned short &nValueOut)
    {
        if (!(hfValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (CPLIsInf(hfValueIn))
        {
            nValueOut = cpl::NumericLimits<unsigned short>::max();
        }
        else
        {
            nValueOut = static_cast<unsigned short>(hfValueIn + GFloat16(0.5f));
        }
    }
};

template <> struct sGDALCopyWord<GFloat16, unsigned int>
{
    static inline void f(const GFloat16 hfValueIn, unsigned int &nValueOut)
    {
        if (!(hfValueIn > 0))
        {
            nValueOut = 0;
        }
        else if (CPLIsInf(hfValueIn))
        {
            nValueOut = cpl::NumericLimits<unsigned int>::max();
        }
        else
        {
            nValueOut = static_cast<unsigned int>(hfValueIn + GFloat16(0.5f));
        }
    }
};

// Floating point input to a small signed integer type: nan becomes zero, otherwise round and clamp.
// Rounding for signed integers is different than for the unsigned integers above.

template <> struct sGDALCopyWord<GFloat16, signed char>
{
    static inline void f(const GFloat16 hfValueIn, signed char &nValueOut)
    {
        if (CPLIsNan(hfValueIn))
        {
            nValueOut = 0;
            return;
        }
        GFloat16 hfMaxVal, hfMinVal;
        GDALGetDataLimits<GFloat16, signed char>(hfMaxVal, hfMinVal);
        GFloat16 hfValue = hfValueIn >= GFloat16(0.0f)
                               ? hfValueIn + GFloat16(0.5f)
                               : hfValueIn - GFloat16(0.5f);
        nValueOut = static_cast<signed char>(
            GDALClampValue(hfValue, hfMaxVal, hfMinVal));
    }
};

template <> struct sGDALCopyWord<float, signed char>
{
    static inline void f(const float fValueIn, signed char &nValueOut)
    {
        if (CPLIsNan(fValueIn))
        {
            nValueOut = 0;
            return;
        }
        float fMaxVal, fMinVal;
        GDALGetDataLimits<float, signed char>(fMaxVal, fMinVal);
        float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f : fValueIn - 0.5f;
        nValueOut =
            static_cast<signed char>(GDALClampValue(fValue, fMaxVal, fMinVal));
    }
};

template <> struct sGDALCopyWord<float, short>
{
    static inline void f(const float fValueIn, short &nValueOut)
    {
        if (CPLIsNan(fValueIn))
        {
            nValueOut = 0;
            return;
        }
        float fMaxVal, fMinVal;
        GDALGetDataLimits<float, short>(fMaxVal, fMinVal);
        float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f : fValueIn - 0.5f;
        nValueOut =
            static_cast<short>(GDALClampValue(fValue, fMaxVal, fMinVal));
    }
};

template <> struct sGDALCopyWord<double, signed char>
{
    static inline void f(const double dfValueIn, signed char &nValueOut)
    {
        if (CPLIsNan(dfValueIn))
        {
            nValueOut = 0;
            return;
        }
        double dfMaxVal, dfMinVal;
        GDALGetDataLimits<double, signed char>(dfMaxVal, dfMinVal);
        double dfValue = dfValueIn > 0.0 ? dfValueIn + 0.5 : dfValueIn - 0.5;
        nValueOut = static_cast<signed char>(
            GDALClampValue(dfValue, dfMaxVal, dfMinVal));
    }
};

template <> struct sGDALCopyWord<double, short>
{
    static inline void f(const double dfValueIn, short &nValueOut)
    {
        if (CPLIsNan(dfValueIn))
        {
            nValueOut = 0;
            return;
        }
        double dfMaxVal, dfMinVal;
        GDALGetDataLimits<double, short>(dfMaxVal, dfMinVal);
        double dfValue = dfValueIn > 0.0 ? dfValueIn + 0.5 : dfValueIn - 0.5;
        nValueOut =
            static_cast<short>(GDALClampValue(dfValue, dfMaxVal, dfMinVal));
    }
};

template <> struct sGDALCopyWord<double, int>
{
    static inline void f(const double dfValueIn, int &nValueOut)
    {
        if (CPLIsNan(dfValueIn))
        {
            nValueOut = 0;
            return;
        }
        double dfMaxVal, dfMinVal;
        GDALGetDataLimits<double, int>(dfMaxVal, dfMinVal);
        double dfValue = dfValueIn >= 0.0 ? dfValueIn + 0.5 : dfValueIn - 0.5;
        nValueOut =
            static_cast<int>(GDALClampValue(dfValue, dfMaxVal, dfMinVal));
    }
};

// Floating point input to a large signed integer type: nan becomes zero, otherwise round and clamp.
// Rounding for signed integers is different than for the unsigned integers above.
// Avoid roundoff while clamping.

template <> struct sGDALCopyWord<GFloat16, short>
{
    static inline void f(const GFloat16 hfValueIn, short &nValueOut)
    {
        if (CPLIsNan(hfValueIn))
        {
            nValueOut = 0;
        }
        else if (hfValueIn >=
                 static_cast<GFloat16>(cpl::NumericLimits<short>::max()))
        {
            nValueOut = cpl::NumericLimits<short>::max();
        }
        else if (hfValueIn <=
                 static_cast<GFloat16>(cpl::NumericLimits<short>::lowest()))
        {
            nValueOut = cpl::NumericLimits<short>::lowest();
        }
        else
        {
            nValueOut = static_cast<short>(hfValueIn > GFloat16(0.0f)
                                               ? hfValueIn + GFloat16(0.5f)
                                               : hfValueIn - GFloat16(0.5f));
        }
    }
};

template <> struct sGDALCopyWord<float, int>
{
    static inline void f(const float fValueIn, int &nValueOut)
    {
        if (CPLIsNan(fValueIn))
        {
            nValueOut = 0;
        }
        else if (fValueIn >= static_cast<float>(cpl::NumericLimits<int>::max()))
        {
            nValueOut = cpl::NumericLimits<int>::max();
        }
        else if (fValueIn <=
                 static_cast<float>(cpl::NumericLimits<int>::lowest()))
        {
            nValueOut = cpl::NumericLimits<int>::lowest();
        }
        else
        {
            nValueOut = static_cast<int>(fValueIn > 0.0f ? fValueIn + 0.5f
                                                         : fValueIn - 0.5f);
        }
    }
};

template <> struct sGDALCopyWord<float, std::int64_t>
{
    static inline void f(const float fValueIn, std::int64_t &nValueOut)
    {
        if (CPLIsNan(fValueIn))
        {
            nValueOut = 0;
        }
        else if (fValueIn >=
                 static_cast<float>(cpl::NumericLimits<std::int64_t>::max()))
        {
            nValueOut = cpl::NumericLimits<std::int64_t>::max();
        }
        else if (fValueIn <=
                 static_cast<float>(cpl::NumericLimits<std::int64_t>::lowest()))
        {
            nValueOut = cpl::NumericLimits<std::int64_t>::lowest();
        }
        else
        {
            nValueOut = static_cast<std::int64_t>(
                fValueIn > 0.0f ? fValueIn + 0.5f : fValueIn - 0.5f);
        }
    }
};

template <> struct sGDALCopyWord<double, std::int64_t>
{
    static inline void f(const double dfValueIn, std::int64_t &nValueOut)
    {
        if (CPLIsNan(dfValueIn))
        {
            nValueOut = 0;
        }
        else if (dfValueIn >=
                 static_cast<double>(cpl::NumericLimits<std::int64_t>::max()))
        {
            nValueOut = cpl::NumericLimits<std::int64_t>::max();
        }
        else if (dfValueIn <=
                 static_cast<double>(cpl::NumericLimits<std::int64_t>::min()))
        {
            nValueOut = cpl::NumericLimits<std::int64_t>::min();
        }
        else
        {
            nValueOut = static_cast<std::int64_t>(
                dfValueIn > 0.0 ? dfValueIn + 0.5 : dfValueIn - 0.5);
        }
    }
};

// Floating point input to a very large signed integer type: nan becomes zero, otherwise round and clamp.
// Rounding for signed integers is different than for the unsigned integers above.
// Avoid infinity while clamping when the maximum integer is too large for the floating-point type.
// Avoid roundoff while clamping.

template <> struct sGDALCopyWord<GFloat16, int>
{
    static inline void f(const GFloat16 hfValueIn, int &nValueOut)
    {
        if (CPLIsNan(hfValueIn))
        {
            nValueOut = 0;
        }
        else if (CPLIsInf(hfValueIn))
        {
            nValueOut = hfValueIn > GFloat16(0.0f)
                            ? cpl::NumericLimits<int>::max()
                            : cpl::NumericLimits<int>::lowest();
        }
        else
        {
            nValueOut = static_cast<int>(hfValueIn > GFloat16(0.0f)
                                             ? hfValueIn + GFloat16(0.5f)
                                             : hfValueIn - GFloat16(0.5f));
        }
    }
};

template <> struct sGDALCopyWord<GFloat16, std::int64_t>
{
    static inline void f(const GFloat16 hfValueIn, std::int64_t &nValueOut)
    {
        if (CPLIsNan(hfValueIn))
        {
            nValueOut = 0;
        }
        else if (CPLIsInf(hfValueIn))
        {
            nValueOut = hfValueIn > GFloat16(0.0f)
                            ? cpl::NumericLimits<std::int64_t>::max()
                            : cpl::NumericLimits<std::int64_t>::lowest();
        }
        else
        {
            nValueOut = static_cast<std::int64_t>(
                hfValueIn > GFloat16(0.0f) ? hfValueIn + GFloat16(0.5f)
                                           : hfValueIn - GFloat16(0.5f));
        }
    }
};

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
    if constexpr (std::is_same<Tin, Tout>::value)
        tValueOut = tValueIn;
    else
        sGDALCopyWord<Tin, Tout>::f(tValueIn, tValueOut);
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
inline void GDALCopy4Words(const Tin *pValueIn, Tout *const pValueOut)
{
    GDALCopyWord(pValueIn[0], pValueOut[0]);
    GDALCopyWord(pValueIn[1], pValueOut[1]);
    GDALCopyWord(pValueIn[2], pValueOut[2]);
    GDALCopyWord(pValueIn[3], pValueOut[3]);
}

/************************************************************************/
/*                         GDALCopy8Words()                             */
/************************************************************************/
/**
 * Copy 8 packed words to 8 packed words, optionally rounding if appropriate
 * (i.e. going from the float to the integer case).
 *
 * @param pValueIn pointer to 8 input values of type Tin.
 * @param pValueOut pointer to 8 output values of type Tout.
 */

template <class Tin, class Tout>
inline void GDALCopy8Words(const Tin *pValueIn, Tout *const pValueOut)
{
    GDALCopy4Words(pValueIn, pValueOut);
    GDALCopy4Words(pValueIn + 4, pValueOut + 4);
}

// Needs SSE2
#if defined(__x86_64) || defined(_M_X64) || defined(USE_SSE2) ||               \
    defined(USE_NEON_OPTIMIZATIONS)

template <>
inline void GDALCopy4Words(const float *pValueIn, GByte *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    // The following clamping would be useless due to the final saturating
    // packing if we could guarantee the input range in [INT_MIN,INT_MAX]
    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 xmm_max = _mm_set1_ps(255);
    xmm = _mm_add_ps(xmm, p0d5);
    xmm = _mm_min_ps(_mm_max_ps(xmm, p0d5), xmm_max);

    __m128i xmm_i = _mm_cvttps_epi32(xmm);

#if defined(__SSSE3__) || defined(USE_NEON_OPTIMIZATIONS)
    xmm_i = _mm_shuffle_epi8(
        xmm_i, _mm_cvtsi32_si128(0 | (4 << 8) | (8 << 16) | (12 << 24)));
#else
    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
    xmm_i = _mm_packus_epi16(xmm_i, xmm_i);  // Pack int16 to uint8
#endif
    GDALCopyXMMToInt32(xmm_i, pValueOut);
}

static inline __m128 GDALIfThenElse(__m128 mask, __m128 thenVal, __m128 elseVal)
{
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_blendv_ps(elseVal, thenVal, mask);
#else
    return _mm_or_ps(_mm_and_ps(mask, thenVal), _mm_andnot_ps(mask, elseVal));
#endif
}

template <>
inline void GDALCopy4Words(const float *pValueIn, GInt8 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    const __m128 xmm_min = _mm_set1_ps(-128);
    const __m128 xmm_max = _mm_set1_ps(127);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 m0d5 = _mm_set1_ps(-0.5f);
    const __m128 mask = _mm_cmpge_ps(xmm, p0d5);
    // f >= 0.5f ? f + 0.5f : f - 0.5f
    xmm = _mm_add_ps(xmm, GDALIfThenElse(mask, p0d5, m0d5));

    __m128i xmm_i = _mm_cvttps_epi32(xmm);

#if defined(__SSSE3__) || defined(USE_NEON_OPTIMIZATIONS)
    xmm_i = _mm_shuffle_epi8(
        xmm_i, _mm_cvtsi32_si128(0 | (4 << 8) | (8 << 16) | (12 << 24)));
#else
    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);  // Pack int32 to int16
    xmm_i = _mm_packs_epi16(xmm_i, xmm_i);  // Pack int16 to int8
#endif
    GDALCopyXMMToInt32(xmm_i, pValueOut);
}

template <>
inline void GDALCopy4Words(const float *pValueIn, GInt16 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    const __m128 xmm_min = _mm_set1_ps(-32768);
    const __m128 xmm_max = _mm_set1_ps(32767);
    xmm = _mm_min_ps(_mm_max_ps(xmm, xmm_min), xmm_max);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 m0d5 = _mm_set1_ps(-0.5f);
    const __m128 mask = _mm_cmpge_ps(xmm, p0d5);
    // f >= 0.5f ? f + 0.5f : f - 0.5f
    xmm = _mm_add_ps(xmm, GDALIfThenElse(mask, p0d5, m0d5));

    __m128i xmm_i = _mm_cvttps_epi32(xmm);

    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);  // Pack int32 to int16
    GDALCopyXMMToInt64(xmm_i, pValueOut);
}

template <>
inline void GDALCopy4Words(const float *pValueIn, GUInt16 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 xmm_max = _mm_set1_ps(65535);
    xmm = _mm_add_ps(xmm, p0d5);
    xmm = _mm_min_ps(_mm_max_ps(xmm, p0d5), xmm_max);

    __m128i xmm_i = _mm_cvttps_epi32(xmm);

#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    xmm_i = _mm_packus_epi32(xmm_i, xmm_i);  // Pack int32 to uint16
#else
    // Translate to int16 range because _mm_packus_epi32 is SSE4.1 only
    xmm_i = _mm_add_epi32(xmm_i, _mm_set1_epi32(-32768));
    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);  // Pack int32 to int16
    // Translate back to uint16 range (actually -32768==32768 in int16)
    xmm_i = _mm_add_epi16(xmm_i, _mm_set1_epi16(-32768));
#endif
    GDALCopyXMMToInt64(xmm_i, pValueOut);
}

static inline __m128i GDALIfThenElse(__m128i mask, __m128i thenVal,
                                     __m128i elseVal)
{
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_blendv_epi8(elseVal, thenVal, mask);
#else
    return _mm_or_si128(_mm_and_si128(mask, thenVal),
                        _mm_andnot_si128(mask, elseVal));
#endif
}

template <>
inline void GDALCopy4Words(const float *pValueIn, GInt32 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);
    const __m128 xmm_ori = xmm;

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 m0d5 = _mm_set1_ps(-0.5f);
    const __m128 mask = _mm_cmpge_ps(xmm, p0d5);
    // f >= 0.5f ? f + 0.5f : f - 0.5f
    xmm = _mm_add_ps(xmm, GDALIfThenElse(mask, p0d5, m0d5));

    __m128i xmm_i = _mm_cvttps_epi32(xmm);

    const __m128 xmm_min = _mm_set1_ps(-2147483648.0f);
    const __m128 xmm_max = _mm_set1_ps(2147483648.0f);
    const __m128i xmm_i_min = _mm_set1_epi32(INT_MIN);
    const __m128i xmm_i_max = _mm_set1_epi32(INT_MAX);
    xmm_i = GDALIfThenElse(_mm_castps_si128(_mm_cmpge_ps(xmm_ori, xmm_max)),
                           xmm_i_max, xmm_i);
    xmm_i = GDALIfThenElse(_mm_castps_si128(_mm_cmple_ps(xmm_ori, xmm_min)),
                           xmm_i_min, xmm_i);

    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut), xmm_i);
}

// ARM64 has an efficient instruction for Float32 -> Float16
#if !(defined(HAVE__FLOAT16) &&                                                \
      (defined(__aarch64__) && defined(_M_ARM64))) &&                          \
    !(defined(__AVX2__) && defined(__F16C__))

inline __m128i GDALFourFloat32ToFloat16(__m128i xmm)
{
    // Ported from https://github.com/simd-everywhere/simde/blob/51743e7920b6e867678cb50e9c62effe28f70b33/simde/simde-f16.h#L176
    // to SSE2 in a branch-less way

    // clang-format off

    /* This code is CC0, based heavily on code by Fabian Giesen. */
    const __m128i f32u_infinity = _mm_set1_epi32(255 << 23);
    const __m128i f16u_max = _mm_set1_epi32((127 + 16) << 23);
    const __m128i denorm_magic = _mm_set1_epi32(((127 - 15) + (23 - 10) + 1) << 23);

    const auto sign = _mm_and_si128(xmm, _mm_set1_epi32(INT_MIN));
    xmm = _mm_xor_si128(xmm, sign);
    xmm = GDALIfThenElse(
        _mm_cmpgt_epi32(xmm, f16u_max),
        /* result is Inf or NaN (all exponent bits set) */
        GDALIfThenElse(
            _mm_cmpgt_epi32(xmm, f32u_infinity),
                /* NaN->qNaN and Inf->Inf */
               _mm_set1_epi32(0x7e00),
               _mm_set1_epi32(0x7c00)),
        /* (De)normalized number or zero */
        GDALIfThenElse(
            _mm_cmplt_epi32(xmm, _mm_set1_epi32(113 << 23)),
             /* use a magic value to align our 10 mantissa bits at the bottom of
              * the float. as long as FP addition is round-to-nearest-even this
              * just works. */
            _mm_sub_epi32(
               _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(xmm),
                                           _mm_castsi128_ps(denorm_magic))),
               /* and one integer subtract of the bias later,
                * we have our final float! */
               denorm_magic
            ),
            _mm_srli_epi32(
                _mm_add_epi32(
                   /* update exponent, rounding bias part 1 */
                   // (unsigned)-0x37fff001 = ((unsigned)(15-127) << 23) + 0xfff
                  _mm_add_epi32(xmm, _mm_set1_epi32(-0x37fff001)),
                   /* rounding bias part 2, using mant_odd */
                  _mm_and_si128(_mm_srli_epi32(xmm, 13), _mm_set1_epi32(1))),
                13
            )
        )
    );
    xmm = _mm_or_si128(xmm, _mm_srli_epi32(sign, 16));

    // clang-format on
    return xmm;
}

template <>
inline void GDALCopy8Words(const float *pValueIn, GFloat16 *const pValueOut)
{
    __m128i xmm_lo =
        GDALFourFloat32ToFloat16(_mm_castps_si128(_mm_loadu_ps(pValueIn)));
    __m128i xmm_hi =
        GDALFourFloat32ToFloat16(_mm_castps_si128(_mm_loadu_ps(pValueIn + 4)));

#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    auto xmm = _mm_packus_epi32(xmm_lo, xmm_hi);  // Pack int32 to uint16
#else
    // Translate to int16 range because _mm_packus_epi32 is SSE4.1 only
    xmm_lo = _mm_add_epi32(xmm_lo, _mm_set1_epi32(-32768));
    xmm_hi = _mm_add_epi32(xmm_hi, _mm_set1_epi32(-32768));
    auto xmm = _mm_packs_epi32(xmm_lo, xmm_hi);  // Pack int32 to int16
    // Translate back to uint16 range (actually -32768==32768 in int16)
    xmm = _mm_add_epi16(xmm, _mm_set1_epi16(-32768));
#endif
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut), xmm);
}

#endif

template <>
inline void GDALCopy4Words(const double *pValueIn, float *const pValueOut)
{
    const __m128d val01 = _mm_loadu_pd(pValueIn);
    const __m128d val23 = _mm_loadu_pd(pValueIn + 2);
    const __m128 val01_s = _mm_cvtpd_ps(val01);
    const __m128 val23_s = _mm_cvtpd_ps(val23);
    const __m128 val = _mm_movelh_ps(val01_s, val23_s);
    _mm_storeu_ps(pValueOut, val);
}

template <>
inline void GDALCopy4Words(const double *pValueIn, GByte *const pValueOut)
{
    const __m128d p0d5 = _mm_set1_pd(0.5);
    const __m128d xmm_max = _mm_set1_pd(255);

    __m128d val01 = _mm_loadu_pd(pValueIn);
    __m128d val23 = _mm_loadu_pd(pValueIn + 2);
    val01 = _mm_add_pd(val01, p0d5);
    val01 = _mm_min_pd(_mm_max_pd(val01, p0d5), xmm_max);
    val23 = _mm_add_pd(val23, p0d5);
    val23 = _mm_min_pd(_mm_max_pd(val23, p0d5), xmm_max);

    const __m128i val01_u32 = _mm_cvttpd_epi32(val01);
    const __m128i val23_u32 = _mm_cvttpd_epi32(val23);

    // Merge 4 int32 values into a single register
    auto xmm_i = _mm_castpd_si128(_mm_shuffle_pd(
        _mm_castsi128_pd(val01_u32), _mm_castsi128_pd(val23_u32), 0));

#if defined(__SSSE3__) || defined(USE_NEON_OPTIMIZATIONS)
    xmm_i = _mm_shuffle_epi8(
        xmm_i, _mm_cvtsi32_si128(0 | (4 << 8) | (8 << 16) | (12 << 24)));
#else
    xmm_i = _mm_packs_epi32(xmm_i, xmm_i);   // Pack int32 to int16
    xmm_i = _mm_packus_epi16(xmm_i, xmm_i);  // Pack int16 to uint8
#endif
    GDALCopyXMMToInt32(xmm_i, pValueOut);
}

template <>
inline void GDALCopy4Words(const float *pValueIn, double *const pValueOut)
{
    const __m128 valIn = _mm_loadu_ps(pValueIn);
    _mm_storeu_pd(pValueOut, _mm_cvtps_pd(valIn));
    _mm_storeu_pd(pValueOut + 2, _mm_cvtps_pd(_mm_movehl_ps(valIn, valIn)));
}

#ifdef __F16C__
template <>
inline void GDALCopy4Words(const GFloat16 *pValueIn, float *const pValueOut)
{
    __m128i xmm = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(pValueIn));
    _mm_storeu_ps(pValueOut, _mm_cvtph_ps(xmm));
}

template <>
inline void GDALCopy4Words(const float *pValueIn, GFloat16 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);
    GDALCopyXMMToInt64(_mm_cvtps_ph(xmm, _MM_FROUND_TO_NEAREST_INT), pValueOut);
}

template <>
inline void GDALCopy4Words(const GFloat16 *pValueIn, double *const pValueOut)
{
    float tmp[4];
    GDALCopy4Words(pValueIn, tmp);
    GDALCopy4Words(tmp, pValueOut);
}

template <>
inline void GDALCopy4Words(const double *pValueIn, GFloat16 *const pValueOut)
{
    float tmp[4];
    GDALCopy4Words(pValueIn, tmp);
    GDALCopy4Words(tmp, pValueOut);
}

// ARM64 has an efficient instruction for Float16 -> Float32/Float64
#elif !(defined(HAVE__FLOAT16) && (defined(__aarch64__) && defined(_M_ARM64)))

// Convert 4 float16 values to 4 float 32 values
// xmm must contain 4 float16 values stored in 32 bit each (with upper 16 bits at zero)
static inline __m128i GDALFourFloat16ToFloat32(__m128i xmm)
{
    // Ported from https://github.com/simd-everywhere/simde/blob/51743e7920b6e867678cb50e9c62effe28f70b33/simde/simde-f16.h#L242C4-L242C68
    // to SSE2 in a branch-less way

    /* This code is CC0, based heavily on code by Fabian Giesen. */
    const auto denorm_magic =
        _mm_castsi128_ps(_mm_set1_epi32((128 - 15) << 23));
    const auto shifted_exp =
        _mm_set1_epi32(0x7c00 << 13); /* exponent mask after shift */

    // Shift exponent and mantissa bits to their position in a float32
    auto f32u = _mm_slli_epi32(_mm_and_si128(xmm, _mm_set1_epi32(0x7fff)), 13);
    // Extract the (shifted) exponent
    const auto exp = _mm_and_si128(shifted_exp, f32u);
    // Adjust the exponent
    const auto exp_adjustment = _mm_set1_epi32((127 - 15) << 23);
    f32u = _mm_add_epi32(f32u, exp_adjustment);

    const auto is_inf_nan = _mm_cmpeq_epi32(exp, shifted_exp); /* Inf/NaN? */
    // When is_inf_nan is true: extra exponent adjustment
    const auto f32u_inf_nan = _mm_add_epi32(f32u, exp_adjustment);

    const auto is_denormal =
        _mm_cmpeq_epi32(exp, _mm_setzero_si128()); /* Zero/Denormal? */
    // When is_denormal is true:
    auto f32u_denormal = _mm_add_epi32(f32u, _mm_set1_epi32(1 << 23));
    f32u_denormal = _mm_castps_si128(
        _mm_sub_ps(_mm_castsi128_ps(f32u_denormal), denorm_magic));

    f32u = GDALIfThenElse(is_inf_nan, f32u_inf_nan, f32u);
    f32u = GDALIfThenElse(is_denormal, f32u_denormal, f32u);

    // Re-apply sign bit
    f32u = _mm_or_si128(
        f32u, _mm_slli_epi32(_mm_and_si128(xmm, _mm_set1_epi32(0x8000)), 16));
    return f32u;
}

template <>
inline void GDALCopy8Words(const GFloat16 *pValueIn, float *const pValueOut)
{
    __m128i xmm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pValueIn));
    const auto xmm_0 =
        GDALFourFloat16ToFloat32(_mm_unpacklo_epi16(xmm, _mm_setzero_si128()));
    const auto xmm_1 =
        GDALFourFloat16ToFloat32(_mm_unpackhi_epi16(xmm, _mm_setzero_si128()));
    _mm_storeu_ps(pValueOut + 0, _mm_castsi128_ps(xmm_0));
    _mm_storeu_ps(pValueOut + 4, _mm_castsi128_ps(xmm_1));
}

template <>
inline void GDALCopy8Words(const GFloat16 *pValueIn, double *const pValueOut)
{
    __m128i xmm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pValueIn));
    const auto xmm_0 = _mm_castsi128_ps(
        GDALFourFloat16ToFloat32(_mm_unpacklo_epi16(xmm, _mm_setzero_si128())));
    const auto xmm_1 = _mm_castsi128_ps(
        GDALFourFloat16ToFloat32(_mm_unpackhi_epi16(xmm, _mm_setzero_si128())));
    _mm_storeu_pd(pValueOut + 0, _mm_cvtps_pd(xmm_0));
    _mm_storeu_pd(pValueOut + 2, _mm_cvtps_pd(_mm_movehl_ps(xmm_0, xmm_0)));
    _mm_storeu_pd(pValueOut + 4, _mm_cvtps_pd(xmm_1));
    _mm_storeu_pd(pValueOut + 6, _mm_cvtps_pd(_mm_movehl_ps(xmm_1, xmm_1)));
}

#endif  // __F16C__

#ifdef __AVX2__

#include <immintrin.h>

template <>
inline void GDALCopy8Words(const double *pValueIn, float *const pValueOut)
{
    const __m256d val0123 = _mm256_loadu_pd(pValueIn);
    const __m256d val4567 = _mm256_loadu_pd(pValueIn + 4);
    const __m256 val0123_s = _mm256_castps128_ps256(_mm256_cvtpd_ps(val0123));
    const __m256 val4567_s = _mm256_castps128_ps256(_mm256_cvtpd_ps(val4567));
    const __m256 val =
        _mm256_permute2f128_ps(val0123_s, val4567_s, 0 | (2 << 4));
    _mm256_storeu_ps(pValueOut, val);
}

template <>
inline void GDALCopy8Words(const float *pValueIn, double *const pValueOut)
{
    const __m256 valIn = _mm256_loadu_ps(pValueIn);
    _mm256_storeu_pd(pValueOut, _mm256_cvtps_pd(_mm256_castps256_ps128(valIn)));
    _mm256_storeu_pd(pValueOut + 4,
                     _mm256_cvtps_pd(_mm256_castps256_ps128(
                         _mm256_permute2f128_ps(valIn, valIn, 1))));
}

#ifdef __F16C__

template <>
inline void GDALCopy8Words(const GFloat16 *pValueIn, float *const pValueOut)
{
    __m128i xmm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pValueIn));
    _mm256_storeu_ps(pValueOut, _mm256_cvtph_ps(xmm));
}

template <>
inline void GDALCopy8Words(const float *pValueIn, GFloat16 *const pValueOut)
{
    __m256 ymm = _mm256_loadu_ps(pValueIn);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut),
                     _mm256_cvtps_ph(ymm, _MM_FROUND_TO_NEAREST_INT));
}

template <>
inline void GDALCopy8Words(const GFloat16 *pValueIn, double *const pValueOut)
{
    __m128i xmm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pValueIn));
    const auto ymm = _mm256_cvtph_ps(xmm);
    _mm256_storeu_pd(pValueOut, _mm256_cvtps_pd(_mm256_extractf128_ps(ymm, 0)));
    _mm256_storeu_pd(pValueOut + 4,
                     _mm256_cvtps_pd(_mm256_extractf128_ps(ymm, 1)));
}

template <>
inline void GDALCopy8Words(const double *pValueIn, GFloat16 *const pValueOut)
{
    __m256d ymm0 = _mm256_loadu_pd(pValueIn);
    __m256d ymm1 = _mm256_loadu_pd(pValueIn + 4);
    __m256 ymm = _mm256_set_m128(_mm256_cvtpd_ps(ymm1), _mm256_cvtpd_ps(ymm0));
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut),
                     _mm256_cvtps_ph(ymm, _MM_FROUND_TO_NEAREST_INT));
}

#endif

template <>
inline void GDALCopy8Words(const float *pValueIn, GByte *const pValueOut)
{
    __m256 ymm = _mm256_loadu_ps(pValueIn);

    const __m256 p0d5 = _mm256_set1_ps(0.5f);
    const __m256 ymm_max = _mm256_set1_ps(255);
    ymm = _mm256_add_ps(ymm, p0d5);
    ymm = _mm256_min_ps(_mm256_max_ps(ymm, p0d5), ymm_max);

    __m256i ymm_i = _mm256_cvttps_epi32(ymm);

    ymm_i = _mm256_packus_epi32(ymm_i, ymm_i);  // Pack int32 to uint16
    ymm_i = _mm256_permute4x64_epi64(ymm_i, 0 | (2 << 2));  // AVX2

    __m128i xmm_i = _mm256_castsi256_si128(ymm_i);
    xmm_i = _mm_packus_epi16(xmm_i, xmm_i);
    GDALCopyXMMToInt64(xmm_i, pValueOut);
}

template <>
inline void GDALCopy8Words(const float *pValueIn, GUInt16 *const pValueOut)
{
    __m256 ymm = _mm256_loadu_ps(pValueIn);

    const __m256 p0d5 = _mm256_set1_ps(0.5f);
    const __m256 ymm_max = _mm256_set1_ps(65535);
    ymm = _mm256_add_ps(ymm, p0d5);
    ymm = _mm256_min_ps(_mm256_max_ps(ymm, p0d5), ymm_max);

    __m256i ymm_i = _mm256_cvttps_epi32(ymm);

    ymm_i = _mm256_packus_epi32(ymm_i, ymm_i);  // Pack int32 to uint16
    ymm_i = _mm256_permute4x64_epi64(ymm_i, 0 | (2 << 2));  // AVX2

    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut),
                     _mm256_castsi256_si128(ymm_i));
}
#else
template <>
inline void GDALCopy8Words(const float *pValueIn, GUInt16 *const pValueOut)
{
    __m128 xmm = _mm_loadu_ps(pValueIn);
    __m128 xmm1 = _mm_loadu_ps(pValueIn + 4);

    const __m128 p0d5 = _mm_set1_ps(0.5f);
    const __m128 xmm_max = _mm_set1_ps(65535);
    xmm = _mm_add_ps(xmm, p0d5);
    xmm1 = _mm_add_ps(xmm1, p0d5);
    xmm = _mm_min_ps(_mm_max_ps(xmm, p0d5), xmm_max);
    xmm1 = _mm_min_ps(_mm_max_ps(xmm1, p0d5), xmm_max);

    __m128i xmm_i = _mm_cvttps_epi32(xmm);
    __m128i xmm1_i = _mm_cvttps_epi32(xmm1);

#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    xmm_i = _mm_packus_epi32(xmm_i, xmm1_i);  // Pack int32 to uint16
#else
    // Translate to int16 range because _mm_packus_epi32 is SSE4.1 only
    xmm_i = _mm_add_epi32(xmm_i, _mm_set1_epi32(-32768));
    xmm1_i = _mm_add_epi32(xmm1_i, _mm_set1_epi32(-32768));
    xmm_i = _mm_packs_epi32(xmm_i, xmm1_i);  // Pack int32 to int16
    // Translate back to uint16 range (actually -32768==32768 in int16)
    xmm_i = _mm_add_epi16(xmm_i, _mm_set1_epi16(-32768));
#endif
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pValueOut), xmm_i);
}
#endif

// ARM64 has an efficient instruction for Float64 -> Float16
#if !(defined(HAVE__FLOAT16) &&                                                \
      (defined(__aarch64__) && defined(_M_ARM64))) &&                          \
    !(defined(__AVX2__) && defined(__F16C__))
template <>
inline void GDALCopy8Words(const double *pValueIn, GFloat16 *const pValueOut)
{
    float fVal[8];
    GDALCopy8Words(pValueIn, fVal);
    GDALCopy8Words(fVal, pValueOut);
}
#endif

#endif  //  defined(__x86_64) || defined(_M_X64)

#endif  // GDAL_PRIV_TEMPLATES_HPP_INCLUDED
