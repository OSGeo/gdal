/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Utility functions to find minimum and maximum values in a buffer
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_MINMAX_ELEMENT_INCLUDED
#define GDAL_MINMAX_ELEMENT_INCLUDED

// NOTE: This header requires C++17

// This file may be vendored by other applications than GDAL
// WARNING: if modifying this file, please also update the upstream GDAL version
// at https://github.com/OSGeo/gdal/blob/master/gcore/gdal_minmax_element.hpp

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "gdal.h"

// Just to please cppcheck
#ifndef GDAL_COMPUTE_VERSION
#define GDAL_COMPUTE_VERSION(maj, min, rev)                                    \
    ((maj)*1000000 + (min)*10000 + (rev)*100)
#endif

#ifdef GDAL_COMPILATION
#include "cpl_float.h"
#define GDAL_MINMAXELT_NS gdal
#elif !defined(GDAL_MINMAXELT_NS)
#error "Please define the GDAL_MINMAXELT_NS macro to define the namespace"
#elif GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
#include "cpl_float.h"
#endif

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"
#define GDAL_MINMAX_ELEMENT_USE_SSE2
#else
#if defined(__x86_64) || defined(_M_X64)
#define GDAL_MINMAX_ELEMENT_USE_SSE2
#endif
#ifdef GDAL_MINMAX_ELEMENT_USE_SSE2
// SSE2 header
#include <emmintrin.h>
#if defined(__SSE4_1__) || defined(__AVX__)
#include <smmintrin.h>
#endif
#endif
#endif

#include "gdal_priv_templates.hpp"

namespace GDAL_MINMAXELT_NS
{
namespace detail
{

template <class T> struct is_floating_point
{
    static constexpr bool value = std::is_floating_point_v<T>
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
                                  || std::is_same_v<T, GFloat16>
#endif
        ;
};

template <class T>
constexpr bool is_floating_point_v = is_floating_point<T>::value;

/************************************************************************/
/*                            compScalar()                              */
/************************************************************************/

template <class T, bool IS_MAX> inline static bool compScalar(T x, T y)
{
    if constexpr (IS_MAX)
        return x > y;
    else
        return x < y;
}

template <typename T> inline static bool IsNan(T x)
{
    // We need to write `using std::isnan` instead of directly using
    // `std::isnan` because `std::isnan` only supports the types
    // `float` and `double`. The `isnan` for `cpl::Float16` is found in the
    // `cpl` namespace via argument-dependent lookup
    // <https://en.cppreference.com/w/cpp/language/adl>.
    using std::isnan;
    return isnan(x);
}

template <class T> inline static bool compEqual(T x, T y)
{
    return x == y;
}

// On Intel/Neon, we do comparisons on uint16_t instead of casting to float for
// faster execution.
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0) &&                      \
    defined(GDAL_MINMAX_ELEMENT_USE_SSE2)
template <> bool IsNan<GFloat16>(GFloat16 x)
{
    uint16_t iX;
    memcpy(&iX, &x, sizeof(x));
    // Check that the 5 bits of the exponent are set and the mantissa is not zero.
    return (iX & 0x7fff) > 0x7c00;
}

template <> bool compEqual<GFloat16>(GFloat16 x, GFloat16 y)
{
    uint16_t iX, iY;
    memcpy(&iX, &x, sizeof(x));
    memcpy(&iY, &y, sizeof(y));
    // Given our usage where y cannot be NaN we can skip the IsNan tests
    assert(!IsNan(y));
    return (iX == iY ||
            // Also check +0 == -0
            ((iX | iY) == (1 << 15)));
}

template <> bool compScalar<GFloat16, true>(GFloat16 x, GFloat16 y)
{
    uint16_t iX, iY;
    memcpy(&iX, &x, sizeof(x));
    memcpy(&iY, &y, sizeof(y));
    bool ret;
    if (IsNan(x) || IsNan(y))
    {
        ret = false;
    }
    else if (!(iX >> 15))
    {
        // +0 considered > -0. We don't really care
        ret = (iY >> 15) || iX > iY;
    }
    else
    {
        ret = (iY >> 15) && iX < iY;
    }
    return ret;
}

template <> bool compScalar<GFloat16, false>(GFloat16 x, GFloat16 y)
{
    return compScalar<GFloat16, true>(y, x);
}
#endif

/************************************************************************/
/*                 extremum_element_with_nan_generic()                  */
/************************************************************************/

template <class T, bool IS_MAX>
inline size_t extremum_element_with_nan_generic(const T *v, size_t size)
{
    if (size == 0)
        return 0;
    size_t idx_of_extremum = 0;
    auto extremum = v[0];
    bool extremum_is_nan = IsNan(extremum);
    size_t i = 1;
    for (; i < size; ++i)
    {
        if (compScalar<T, IS_MAX>(v[i], extremum) ||
            (extremum_is_nan && !IsNan(v[i])))
        {
            extremum = v[i];
            idx_of_extremum = i;
            extremum_is_nan = false;
        }
    }
    return idx_of_extremum;
}

/************************************************************************/
/*                 extremum_element_with_nan_generic()                  */
/************************************************************************/

template <class T, bool IS_MAX>
inline size_t extremum_element_with_nan_generic(const T *v, size_t size,
                                                T noDataValue)
{
    if (IsNan(noDataValue))
        return extremum_element_with_nan_generic<T, IS_MAX>(v, size);
    if (size == 0)
        return 0;
    size_t idx_of_extremum = 0;
    auto extremum = v[0];
    bool extremum_is_nan_or_nodata =
        IsNan(extremum) || compEqual(extremum, noDataValue);
    size_t i = 1;
    for (; i < size; ++i)
    {
        if (!compEqual(v[i], noDataValue) &&
            (compScalar<T, IS_MAX>(v[i], extremum) ||
             (extremum_is_nan_or_nodata && !IsNan(v[i]))))
        {
            extremum = v[i];
            idx_of_extremum = i;
            extremum_is_nan_or_nodata = false;
        }
    }
    return idx_of_extremum;
}

/************************************************************************/
/*                       extremum_element_generic()                     */
/************************************************************************/

template <class T, bool IS_MAX>
inline size_t extremum_element_generic(const T *buffer, size_t size,
                                       bool bHasNoData, T noDataValue)
{
    if (bHasNoData)
    {
        if constexpr (is_floating_point_v<T>)
        {
            if (IsNan(noDataValue))
            {
                if constexpr (IS_MAX)
                {
                    return std::max_element(buffer, buffer + size,
                                            [](T a, T b) {
                                                return IsNan(b)   ? false
                                                       : IsNan(a) ? true
                                                                  : a < b;
                                            }) -
                           buffer;
                }
                else
                {
                    return std::min_element(buffer, buffer + size,
                                            [](T a, T b) {
                                                return IsNan(b)   ? true
                                                       : IsNan(a) ? false
                                                                  : a < b;
                                            }) -
                           buffer;
                }
            }
            else
            {
                if constexpr (IS_MAX)
                {
                    return std::max_element(buffer, buffer + size,
                                            [noDataValue](T a, T b)
                                            {
                                                return IsNan(b)   ? false
                                                       : IsNan(a) ? true
                                                       : (b == noDataValue)
                                                           ? false
                                                       : (a == noDataValue)
                                                           ? true
                                                           : a < b;
                                            }) -
                           buffer;
                }
                else
                {
                    return std::min_element(buffer, buffer + size,
                                            [noDataValue](T a, T b)
                                            {
                                                return IsNan(b)   ? true
                                                       : IsNan(a) ? false
                                                       : (b == noDataValue)
                                                           ? true
                                                       : (a == noDataValue)
                                                           ? false
                                                           : a < b;
                                            }) -
                           buffer;
                }
            }
        }
        else
        {
            if constexpr (IS_MAX)
            {
                return std::max_element(buffer, buffer + size,
                                        [noDataValue](T a, T b) {
                                            return (b == noDataValue)   ? false
                                                   : (a == noDataValue) ? true
                                                                        : a < b;
                                        }) -
                       buffer;
            }
            else
            {
                return std::min_element(buffer, buffer + size,
                                        [noDataValue](T a, T b) {
                                            return (b == noDataValue)   ? true
                                                   : (a == noDataValue) ? false
                                                                        : a < b;
                                        }) -
                       buffer;
            }
        }
    }
    else
    {
        if constexpr (is_floating_point_v<T>)
        {
            if constexpr (IS_MAX)
            {
                return std::max_element(buffer, buffer + size,
                                        [](T a, T b) {
                                            return IsNan(b)   ? false
                                                   : IsNan(a) ? true
                                                              : a < b;
                                        }) -
                       buffer;
            }
            else
            {
                return std::min_element(buffer, buffer + size,
                                        [](T a, T b) {
                                            return IsNan(b)   ? true
                                                   : IsNan(a) ? false
                                                              : a < b;
                                        }) -
                       buffer;
            }
        }
        else
        {
            if constexpr (IS_MAX)
            {
                return std::max_element(buffer, buffer + size) - buffer;
            }
            else
            {
                return std::min_element(buffer, buffer + size) - buffer;
            }
        }
    }
}

#ifdef GDAL_MINMAX_ELEMENT_USE_SSE2

/************************************************************************/
/*                     extremum_element_sse2()                          */
/************************************************************************/

static inline int8_t Shift8(uint8_t x)
{
    return static_cast<int8_t>(x + std::numeric_limits<int8_t>::min());
}

static inline int16_t Shift16(uint16_t x)
{
    return static_cast<int16_t>(x + std::numeric_limits<int16_t>::min());
}

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static inline int32_t Shift32(uint32_t x)
{
    x += static_cast<uint32_t>(std::numeric_limits<int32_t>::min());
    int32_t ret;
    memcpy(&ret, &x, sizeof(x));
    return ret;
}

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static inline int64_t Shift64(uint64_t x)
{
    x += static_cast<uint64_t>(std::numeric_limits<int64_t>::min());
    int64_t ret;
    memcpy(&ret, &x, sizeof(x));
    return ret;
}

// Return a _mm128[i|d] register with all its elements set to x
template <class T> static inline auto set1(T x)
{
    if constexpr (std::is_same_v<T, uint8_t>)
        return _mm_set1_epi8(Shift8(x));
    else if constexpr (std::is_same_v<T, int8_t>)
        return _mm_set1_epi8(x);
    else if constexpr (std::is_same_v<T, uint16_t>)
        return _mm_set1_epi16(Shift16(x));
    else if constexpr (std::is_same_v<T, int16_t>)
        return _mm_set1_epi16(x);
    else if constexpr (std::is_same_v<T, uint32_t>)
        return _mm_set1_epi32(Shift32(x));
    else if constexpr (std::is_same_v<T, int32_t>)
        return _mm_set1_epi32(x);
    else if constexpr (std::is_same_v<T, uint64_t>)
        return _mm_set1_epi64x(Shift64(x));
    else if constexpr (std::is_same_v<T, int64_t>)
        return _mm_set1_epi64x(x);
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
    else if constexpr (std::is_same_v<T, GFloat16>)
    {
        int16_t iX;
        memcpy(&iX, &x, sizeof(x));
        return _mm_set1_epi16(iX);
    }
#endif
    else if constexpr (std::is_same_v<T, float>)
        return _mm_set1_ps(x);
    else
        return _mm_set1_pd(x);
}

// Return a _mm128[i|d] register with all its elements set to x
template <class T> static inline auto set1_unshifted(T x)
{
    if constexpr (std::is_same_v<T, uint8_t>)
    {
        int8_t xSigned;
        memcpy(&xSigned, &x, sizeof(xSigned));
        return _mm_set1_epi8(xSigned);
    }
    else if constexpr (std::is_same_v<T, int8_t>)
        return _mm_set1_epi8(x);
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        int16_t xSigned;
        memcpy(&xSigned, &x, sizeof(xSigned));
        return _mm_set1_epi16(xSigned);
    }
    else if constexpr (std::is_same_v<T, int16_t>)
        return _mm_set1_epi16(x);
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        int32_t xSigned;
        memcpy(&xSigned, &x, sizeof(xSigned));
        return _mm_set1_epi32(xSigned);
    }
    else if constexpr (std::is_same_v<T, int32_t>)
        return _mm_set1_epi32(x);
    else if constexpr (std::is_same_v<T, uint64_t>)
    {
        int64_t xSigned;
        memcpy(&xSigned, &x, sizeof(xSigned));
        return _mm_set1_epi64x(xSigned);
    }
    else if constexpr (std::is_same_v<T, int64_t>)
        return _mm_set1_epi64x(x);
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
    else if constexpr (std::is_same_v<T, GFloat16>)
    {
        int16_t iX;
        memcpy(&iX, &x, sizeof(x));
        return _mm_set1_epi16(iX);
    }
#endif
    else if constexpr (std::is_same_v<T, float>)
        return _mm_set1_ps(x);
    else
        return _mm_set1_pd(x);
}

// Load as many values of type T at a _mm128[i|d] register can contain from x
template <class T> static inline auto loadv(const T *x)
{
    if constexpr (std::is_same_v<T, float>)
        return _mm_loadu_ps(x);
    else if constexpr (std::is_same_v<T, double>)
        return _mm_loadu_pd(x);
    else
        return _mm_loadu_si128(reinterpret_cast<const __m128i *>(x));
}

inline __m128i IsNanGFloat16(__m128i x)
{
    // (iX & 0x7fff) > 0x7c00
    return _mm_cmpgt_epi16(_mm_and_si128(x, _mm_set1_epi16(0x7fff)),
                           _mm_set1_epi16(0x7c00));
}

template <class T, class SSE_T>
static inline SSE_T blendv(SSE_T a, SSE_T b, SSE_T mask)
{
    if constexpr (std::is_same_v<T, float>)
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_blendv_ps(a, b, mask);
#else
        return _mm_or_ps(_mm_andnot_ps(mask, a), _mm_and_ps(mask, b));
#endif
    }
    else if constexpr (std::is_same_v<T, double>)
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_blendv_pd(a, b, mask);
#else
        return _mm_or_pd(_mm_andnot_pd(mask, a), _mm_and_pd(mask, b));
#endif
    }
    else
    {
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
        return _mm_blendv_epi8(a, b, mask);
#else
        return _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, b));
#endif
    }
}

inline __m128i cmpgt_ph(__m128i x, __m128i y)
{
#ifdef slow
    GFloat16 vx[8], vy[8];
    int16_t res[8];
    _mm_storeu_si128(reinterpret_cast<__m128i *>(vx), x);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(vy), y);
    for (int i = 0; i < 8; ++i)
    {
        res[i] = vx[i] > vy[i] ? -1 : 0;
    }
    return _mm_loadu_si128(reinterpret_cast<const __m128i *>(res));
#else
    const auto x_is_negative = _mm_srai_epi16(x, 15);
    const auto y_is_negative = _mm_srai_epi16(y, 15);
    return _mm_andnot_si128(
        // only x can be NaN given how we use this method
        IsNanGFloat16(x),
        blendv<int16_t>(_mm_or_si128(y_is_negative, _mm_cmpgt_epi16(x, y)),
                        _mm_and_si128(y_is_negative, _mm_cmpgt_epi16(y, x)),
                        x_is_negative));
#endif
}

inline __m128i cmpgt_epi64(__m128i x, __m128i y)
{
#if defined(__SSE4_2__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_cmpgt_epi64(x, y);
#else
    auto tmp = _mm_and_si128(_mm_sub_epi64(y, x), _mm_cmpeq_epi32(x, y));
    tmp = _mm_or_si128(tmp, _mm_cmpgt_epi32(x, y));
    // Replicate the 2 odd-indexed (hi) 32-bit word into the 2 even-indexed (lo)
    // ones
    return _mm_shuffle_epi32(tmp, _MM_SHUFFLE(3, 3, 1, 1));
#endif
}

// Return a __m128i register with bits set when x[i] < y[i] when !IS_MAX
// or x[i] > y[i] when IS_MAX
template <class T, bool IS_MAX, class SSE_T>
static inline __m128i comp(SSE_T x, SSE_T y)
{
    if constexpr (IS_MAX)
    {
        if constexpr (std::is_same_v<T, uint8_t>)
            return _mm_cmpgt_epi8(
                _mm_add_epi8(x,
                             _mm_set1_epi8(std::numeric_limits<int8_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int8_t>)
            return _mm_cmpgt_epi8(x, y);
        else if constexpr (std::is_same_v<T, uint16_t>)
            return _mm_cmpgt_epi16(
                _mm_add_epi16(
                    x, _mm_set1_epi16(std::numeric_limits<int16_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int16_t>)
            return _mm_cmpgt_epi16(x, y);
        else if constexpr (std::is_same_v<T, uint32_t>)
            return _mm_cmpgt_epi32(
                _mm_add_epi32(
                    x, _mm_set1_epi32(std::numeric_limits<int32_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int32_t>)
            return _mm_cmpgt_epi32(x, y);
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return cmpgt_epi64(
                _mm_add_epi64(
                    x, _mm_set1_epi64x(std::numeric_limits<int64_t>::min())),
                y);
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return cmpgt_epi64(x, y);
        }
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        else if constexpr (std::is_same_v<T, GFloat16>)
        {
            return cmpgt_ph(x, y);
        }
#endif
        else if constexpr (std::is_same_v<T, float>)
            return _mm_castps_si128(_mm_cmpgt_ps(x, y));
        else
            return _mm_castpd_si128(_mm_cmpgt_pd(x, y));
    }
    else
    {
        if constexpr (std::is_same_v<T, uint8_t>)
            return _mm_cmplt_epi8(
                _mm_add_epi8(x,
                             _mm_set1_epi8(std::numeric_limits<int8_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int8_t>)
            return _mm_cmplt_epi8(x, y);
        else if constexpr (std::is_same_v<T, uint16_t>)
            return _mm_cmplt_epi16(
                _mm_add_epi16(
                    x, _mm_set1_epi16(std::numeric_limits<int16_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int16_t>)
            return _mm_cmplt_epi16(x, y);
        else if constexpr (std::is_same_v<T, uint32_t>)
            return _mm_cmplt_epi32(
                _mm_add_epi32(
                    x, _mm_set1_epi32(std::numeric_limits<int32_t>::min())),
                y);
        else if constexpr (std::is_same_v<T, int32_t>)
            return _mm_cmplt_epi32(x, y);
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return cmpgt_epi64(
                y, _mm_add_epi64(x, _mm_set1_epi64x(
                                        std::numeric_limits<int64_t>::min())));
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return cmpgt_epi64(y, x);
        }
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        else if constexpr (std::is_same_v<T, GFloat16>)
        {
            return cmpgt_ph(y, x);
        }
#endif
        else if constexpr (std::is_same_v<T, float>)
            return _mm_castps_si128(_mm_cmplt_ps(x, y));
        else
            return _mm_castpd_si128(_mm_cmplt_pd(x, y));
    }
}

template <class T, class SSE_T> static inline SSE_T compeq(SSE_T a, SSE_T b);

template <> __m128i compeq<uint8_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi8(a, b);
}

template <> __m128i compeq<int8_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi8(a, b);
}

template <> __m128i compeq<uint16_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi16(a, b);
}

template <> __m128i compeq<int16_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi16(a, b);
}

template <> __m128i compeq<uint32_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi32(a, b);
}

template <> __m128i compeq<int32_t>(__m128i a, __m128i b)
{
    return _mm_cmpeq_epi32(a, b);
}

template <> __m128i compeq<uint64_t>(__m128i a, __m128i b)
{
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_cmpeq_epi64(a, b);
#else
    auto tmp = _mm_cmpeq_epi32(a, b);
    // The shuffle swaps hi-lo 32-bit words
    return _mm_and_si128(tmp, _mm_shuffle_epi32(tmp, _MM_SHUFFLE(2, 3, 0, 1)));
#endif
}

template <>
#if defined(__INTEL_CLANG_COMPILER) &&                                         \
    !(defined(__SSE4_1__) || defined(__AVX__))
// ICC 2024 has a bug with the following code when -fiopenmp is enabled.
// Disabling inlining works around it...
__attribute__((noinline))
#endif
__m128i
compeq<int64_t>(__m128i a, __m128i b)
{
    return compeq<uint64_t>(a, b);
}

template <> __m128 compeq<float>(__m128 a, __m128 b)
{
    return _mm_cmpeq_ps(a, b);
}

template <> __m128d compeq<double>(__m128d a, __m128d b)
{
    return _mm_cmpeq_pd(a, b);
}

#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
template <> __m128i compeq<GFloat16>(__m128i a, __m128i b)
{
    // !isnan(a) && !isnan(b) && (a == b || (a|b) == 0x8000)
    return _mm_andnot_si128(
        IsNanGFloat16(a),  // b cannot be NaN given how we use this method
        _mm_or_si128(_mm_cmpeq_epi16(a, b),
                     _mm_cmpeq_epi16(
                         _mm_or_si128(a, b),
                         _mm_set1_epi16(std::numeric_limits<int16_t>::min()))));
}
#endif

// Using SSE2
template <class T, bool IS_MAX, bool HAS_NODATA>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
size_t
extremum_element_sse2(const T *v, size_t size, T noDataValue)
{
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> ||
                  std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t> ||
                  std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t> ||
                  std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> ||
                  is_floating_point_v<T>);
    if (size == 0)
        return 0;
    size_t idx_of_extremum = 0;
    T extremum = v[0];
    [[maybe_unused]] bool extremum_is_invalid = false;
    if constexpr (is_floating_point_v<T>)
    {
        if constexpr (HAS_NODATA)
        {
            if (IsNan(noDataValue))
            {
                return extremum_element_sse2<T, IS_MAX, false>(
                    v, size, static_cast<T>(0));
            }
        }
        extremum_is_invalid = IsNan(extremum);
    }
    if constexpr (HAS_NODATA)
    {
        if (compEqual(extremum, noDataValue))
            extremum_is_invalid = true;
    }
    size_t i = 1;

    constexpr size_t VALS_PER_REG = sizeof(set1(extremum)) / sizeof(extremum);
    constexpr int LOOP_UNROLLING = 4;
    // If changing the value, then we need to adjust the number of sse_valX
    // loading in the loop.
    static_assert(LOOP_UNROLLING == 4);
    constexpr size_t VALS_PER_ITER = VALS_PER_REG * LOOP_UNROLLING;

    const auto update = [v, noDataValue, &extremum, &idx_of_extremum,
                         &extremum_is_invalid](size_t idx)
    {
        if constexpr (HAS_NODATA)
        {
            if (compEqual(v[idx], noDataValue))
                return;
            if (extremum_is_invalid)
            {
                if constexpr (is_floating_point_v<T>)
                {
                    if (IsNan(v[idx]))
                        return;
                }
                extremum = v[idx];
                idx_of_extremum = idx;
                extremum_is_invalid = false;
                return;
            }
        }
        else
        {
            CPL_IGNORE_RET_VAL(noDataValue);
        }
        if (compScalar<T, IS_MAX>(v[idx], extremum))
        {
            extremum = v[idx];
            idx_of_extremum = idx;
            extremum_is_invalid = false;
        }
        else if constexpr (is_floating_point_v<T>)
        {
            if (extremum_is_invalid && !IsNan(v[idx]))
            {
                extremum = v[idx];
                idx_of_extremum = idx;
                extremum_is_invalid = false;
            }
        }
    };

    for (; i < VALS_PER_ITER && i < size; ++i)
    {
        update(i);
    }

    [[maybe_unused]] auto sse_neutral = set1_unshifted(static_cast<T>(0));
    [[maybe_unused]] auto sse_nodata = set1_unshifted(noDataValue);
    if constexpr (HAS_NODATA || is_floating_point_v<T>)
    {
        for (; i < size && extremum_is_invalid; ++i)
        {
            update(i);
        }
        if (!extremum_is_invalid)
        {
            for (; i < size && (i % VALS_PER_ITER) != 0; ++i)
            {
                update(i);
            }
            sse_neutral = set1_unshifted(extremum);
        }
    }

    auto sse_extremum = set1(extremum);

    [[maybe_unused]] size_t hits = 0;
    const auto sse_iter_count = (size / VALS_PER_ITER) * VALS_PER_ITER;
    for (; i < sse_iter_count; i += VALS_PER_ITER)
    {
        // A bit of loop unrolling to save 3/4 of slow movemask operations.
        auto sse_val0 = loadv(v + i + 0 * VALS_PER_REG);
        auto sse_val1 = loadv(v + i + 1 * VALS_PER_REG);
        auto sse_val2 = loadv(v + i + 2 * VALS_PER_REG);
        auto sse_val3 = loadv(v + i + 3 * VALS_PER_REG);

        if constexpr (HAS_NODATA)
        {
            // Replace all components that are at the nodata value by a
            // neutral value (current minimum)
            const auto replaceNoDataByNeutral =
                [sse_neutral, sse_nodata](auto sse_val)
            {
                const auto eq_nodata = compeq<T>(sse_val, sse_nodata);
                return blendv<T>(sse_val, sse_neutral, eq_nodata);
            };

            sse_val0 = replaceNoDataByNeutral(sse_val0);
            sse_val1 = replaceNoDataByNeutral(sse_val1);
            sse_val2 = replaceNoDataByNeutral(sse_val2);
            sse_val3 = replaceNoDataByNeutral(sse_val3);
        }

        if (_mm_movemask_epi8(_mm_or_si128(
                _mm_or_si128(comp<T, IS_MAX>(sse_val0, sse_extremum),
                             comp<T, IS_MAX>(sse_val1, sse_extremum)),
                _mm_or_si128(comp<T, IS_MAX>(sse_val2, sse_extremum),
                             comp<T, IS_MAX>(sse_val3, sse_extremum)))) != 0)
        {
            if constexpr (!std::is_same_v<T, int8_t> &&
                          !std::is_same_v<T, uint8_t>)
            {
                // The above tests excluding int8_t/uint8_t is due to the fact
                // with those small ranges of values we will quickly converge
                // to the minimum, so no need to do the below "smart" test.

                if (++hits == size / 16)
                {
                    // If we have an almost sorted array, then using this code path
                    // will hurt performance. Arbitrary give up if we get here
                    // more than 1. / 16 of the size of the array.
                    // fprintf(stderr, "going to non-vector path\n");
                    break;
                }
            }
            for (size_t j = 0; j < VALS_PER_ITER; j++)
            {
                update(i + j);
            }

            sse_extremum = set1(extremum);
            if constexpr (HAS_NODATA)
            {
                sse_neutral = set1_unshifted(extremum);
            }
        }
    }
    for (; i < size; ++i)
    {
        update(i);
    }
    return idx_of_extremum;
}

/************************************************************************/
/*                         extremum_element()                           */
/************************************************************************/

template <class T, bool IS_MAX>
inline size_t extremum_element(const T *buffer, size_t size, T noDataValue)
{
    return extremum_element_sse2<T, IS_MAX, true>(buffer, size, noDataValue);
}

template <class T, bool IS_MAX>
inline size_t extremum_element(const T *buffer, size_t size)
{
    return extremum_element_sse2<T, IS_MAX, false>(buffer, size,
                                                   static_cast<T>(0));
}

#else

template <class T, bool IS_MAX>
inline size_t extremum_element(const T *buffer, size_t size, T noDataValue)
{
    if constexpr (is_floating_point_v<T>)
        return extremum_element_with_nan_generic<T, IS_MAX>(buffer, size,
                                                            noDataValue);
    else
        return extremum_element_generic<T, IS_MAX>(buffer, size, true,
                                                   noDataValue);
}

template <class T, bool IS_MAX>
inline size_t extremum_element(const T *buffer, size_t size)
{
    if constexpr (is_floating_point_v<T>)
        return extremum_element_with_nan_generic<T, IS_MAX>(buffer, size);
    else
        return extremum_element_generic<T, IS_MAX>(buffer, size, false,
                                                   static_cast<T>(0));
}

#endif

/************************************************************************/
/*                            extremum_element()                        */
/************************************************************************/

template <class T, bool IS_MAX>
inline size_t extremum_element(const T *buffer, size_t size, bool bHasNoData,
                               T noDataValue)
{
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0) &&                      \
    !defined(GDAL_MINMAX_ELEMENT_USE_SSE2)
    if constexpr (std::is_same_v<T, GFloat16>)
    {
        if (bHasNoData)
            return extremum_element_with_nan_generic<T, IS_MAX>(buffer, size,
                                                                noDataValue);
        else
            return extremum_element_with_nan_generic<T, IS_MAX>(buffer, size);
    }
    else
#endif
        if (bHasNoData)
        return extremum_element<T, IS_MAX>(buffer, size, noDataValue);
    else
        return extremum_element<T, IS_MAX>(buffer, size);
}

template <class T, class NODATAType>
inline bool IsValueExactAs([[maybe_unused]] NODATAType noDataValue)
{
    if constexpr (std::is_same_v<T, NODATAType>)
        return true;
    else
        return GDALIsValueExactAs<T>(static_cast<double>(noDataValue));
}

template <bool IS_MAX, class NODATAType>
size_t extremum_element(const void *buffer, size_t nElts, GDALDataType eDT,
                        bool bHasNoData, NODATAType noDataValue)
{
    switch (eDT)
    {
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 7, 0)
        case GDT_Int8:
        {
            using T = int8_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#endif
        case GDT_Byte:
        {
            using T = uint8_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Int16:
        {
            using T = int16_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt16:
        {
            using T = uint16_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Int32:
        {
            using T = int32_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt32:
        {
            using T = uint32_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 5, 0)
        case GDT_Int64:
        {
            using T = int64_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt64:
        {
            using T = uint64_t;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#endif
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        case GDT_Float16:
        {
            using T = GFloat16;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : static_cast<T>(0));
        }
#endif
        case GDT_Float32:
        {
            using T = float;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Float64:
        {
            using T = double;
            bHasNoData = bHasNoData && IsValueExactAs<T>(noDataValue);
            return extremum_element<T, IS_MAX>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_CInt16:
        case GDT_CInt32:
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        case GDT_CFloat16:
#endif
        case GDT_CFloat32:
        case GDT_CFloat64:
        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
    CPLError(CE_Failure, CPLE_NotSupported,
             "%s not supported for this data type.", __FUNCTION__);
    return 0;
}

}  // namespace detail

/************************************************************************/
/*                            max_element()                             */
/************************************************************************/

/** Return the index of the element where the maximum value is hit.
 *
 * If it is hit in several locations, it is not specified which one will be
 * returned.
 *
 * @param buffer Vector of nElts elements of type eDT.
 * @param nElts Number of elements in buffer.
 * @param eDT Data type of the elements of buffer.
 * @param bHasNoData Whether noDataValue is valid.
 * @param noDataValue Nodata value, only taken into account if bHasNoData == true
 *
 * @since GDAL 3.11
 */
template <class NODATAType>
inline size_t max_element(const void *buffer, size_t nElts, GDALDataType eDT,
                          bool bHasNoData, NODATAType noDataValue)
{
    return detail::extremum_element<true>(buffer, nElts, eDT, bHasNoData,
                                          noDataValue);
}

/************************************************************************/
/*                            min_element()                             */
/************************************************************************/

/** Return the index of the element where the minimum value is hit.
 *
 * If it is hit in several locations, it is not specified which one will be
 * returned.
 *
 * @param buffer Vector of nElts elements of type eDT.
 * @param nElts Number of elements in buffer.
 * @param eDT Data type of the elements of buffer.
 * @param bHasNoData Whether noDataValue is valid.
 * @param noDataValue Nodata value, only taken into account if bHasNoData == true
 *
 * @since GDAL 3.11
 */
template <class NODATAType>
inline size_t min_element(const void *buffer, size_t nElts, GDALDataType eDT,
                          bool bHasNoData, NODATAType noDataValue)
{
    return detail::extremum_element<false>(buffer, nElts, eDT, bHasNoData,
                                           noDataValue);
}

namespace detail
{

#ifdef NOT_EFFICIENT

/************************************************************************/
/*                         minmax_element()                             */
/************************************************************************/

template <class T>
std::pair<size_t, size_t> minmax_element(const T *v, size_t size, T noDataValue)
{
    static_assert(!(std::is_floating_point_v<T>));
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
    static_assert(!std::is_same_v<T, GFloat16>);
#endif
    if (size == 0)
        return std::pair(0, 0);
    size_t idx_of_min = 0;
    size_t idx_of_max = 0;
    T vmin = v[0];
    T vmax = v[0];
    bool extremum_is_nodata = vmin == noDataValue;
    size_t i = 1;
    for (; i < size; ++i)
    {
        if (v[i] != noDataValue && (v[i] < vmin || extremum_is_nodata))
        {
            vmin = v[i];
            idx_of_min = i;
            extremum_is_nodata = false;
        }
        if (v[i] != noDataValue && (v[i] > vmax || extremum_is_nodata))
        {
            vmax = v[i];
            idx_of_max = i;
            extremum_is_nodata = false;
        }
    }
    return std::pair(idx_of_min, idx_of_max);
}

template <class T>
std::pair<size_t, size_t> minmax_element(const T *v, size_t size)
{
    static_assert(!(std::is_floating_point_v<T>));
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
    static_assert(!std::is_same_v<T, GFloat16>);
#endif
    if (size == 0)
        return std::pair(0, 0);
    size_t idx_of_min = 0;
    size_t idx_of_max = 0;
    T vmin = v[0];
    T vmax = v[0];
    size_t i = 1;
    for (; i < size; ++i)
    {
        if (v[i] < vmin)
        {
            vmin = v[i];
            idx_of_min = i;
        }
        if (v[i] > vmax)
        {
            vmax = v[i];
            idx_of_max = i;
        }
    }
    return std::pair(idx_of_min, idx_of_max);
}

template <class T>
inline std::pair<size_t, size_t> minmax_element_with_nan(const T *v,
                                                         size_t size)
{
    if (size == 0)
        return std::pair(0, 0);
    size_t idx_of_min = 0;
    size_t idx_of_max = 0;
    T vmin = v[0];
    T vmax = v[0];
    size_t i = 1;
    if (IsNan(v[0]))
    {
        for (; i < size; ++i)
        {
            if (!IsNan(v[i]))
            {
                vmin = v[i];
                idx_of_min = i;
                vmax = v[i];
                idx_of_max = i;
                break;
            }
        }
    }
    for (; i < size; ++i)
    {
        if (v[i] < vmin)
        {
            vmin = v[i];
            idx_of_min = i;
        }
        if (v[i] > vmax)
        {
            vmax = v[i];
            idx_of_max = i;
        }
    }
    return std::pair(idx_of_min, idx_of_max);
}

template <>
std::pair<size_t, size_t> minmax_element<float>(const float *v, size_t size)
{
    return minmax_element_with_nan<float>(v, size);
}

template <>
std::pair<size_t, size_t> minmax_element<double>(const double *v, size_t size)
{
    return minmax_element_with_nan<double>(v, size);
}

template <class T>
inline std::pair<size_t, size_t> minmax_element(const T *buffer, size_t size,
                                                bool bHasNoData, T noDataValue)
{
    if (bHasNoData)
    {
        return minmax_element<T>(buffer, size, noDataValue);
    }
    else
    {
        return minmax_element<T>(buffer, size);
    }
}
#else

/************************************************************************/
/*                         minmax_element()                             */
/************************************************************************/

template <class T>
inline std::pair<size_t, size_t> minmax_element(const T *buffer, size_t size,
                                                bool bHasNoData, T noDataValue)
{
#ifdef NOT_EFFICIENT
    if (bHasNoData)
    {
        return minmax_element<T>(buffer, size, noDataValue);
    }
    else
    {
        return minmax_element<T>(buffer, size);
        //auto [imin, imax] = std::minmax_element(buffer, buffer + size);
        //return std::pair(imin - buffer, imax - buffer);
    }
#else

#if !defined(GDAL_MINMAX_ELEMENT_USE_SSE2)
    if constexpr (!std::is_floating_point_v<T>
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
                  && !std::is_same_v<T, GFloat16>
#endif
    )
    {
        if (!bHasNoData)
        {
            auto [min_iter, max_iter] =
                std::minmax_element(buffer, buffer + size);
            return std::pair(min_iter - buffer, max_iter - buffer);
        }
    }
#endif

    // Using separately min and max is more efficient than computing them
    // within the same loop
    return std::pair(
        extremum_element<T, false>(buffer, size, bHasNoData, noDataValue),
        extremum_element<T, true>(buffer, size, bHasNoData, noDataValue));

#endif
}
#endif

}  // namespace detail

/************************************************************************/
/*                          minmax_element()                            */
/************************************************************************/

/** Return the index of the elements where the minimum and maximum values are hit.
 *
 * If they are hit in several locations, it is not specified which one will be
 * returned (contrary to std::minmax_element).
 *
 * @param buffer Vector of nElts elements of type eDT.
 * @param nElts Number of elements in buffer.
 * @param eDT Data type of the elements of buffer.
 * @param bHasNoData Whether noDataValue is valid.
 * @param noDataValue Nodata value, only taken into account if bHasNoData == true
 *
 * @since GDAL 3.11
 */
template <class NODATAType>
inline std::pair<size_t, size_t>
minmax_element(const void *buffer, size_t nElts, GDALDataType eDT,
               bool bHasNoData, NODATAType noDataValue)
{
    switch (eDT)
    {
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 7, 0)
        case GDT_Int8:
        {
            using T = int8_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#endif
        case GDT_Byte:
        {
            using T = uint8_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Int16:
        {
            using T = int16_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt16:
        {
            using T = uint16_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Int32:
        {
            using T = int32_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt32:
        {
            using T = uint32_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 5, 0)
        case GDT_Int64:
        {
            using T = int64_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_UInt64:
        {
            using T = uint64_t;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
#endif
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        case GDT_Float16:
        {
            using T = GFloat16;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : static_cast<T>(0));
        }
#endif
        case GDT_Float32:
        {
            using T = float;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_Float64:
        {
            using T = double;
            bHasNoData = bHasNoData && detail::IsValueExactAs<T>(noDataValue);
            return detail::minmax_element<T>(
                static_cast<const T *>(buffer), nElts, bHasNoData,
                bHasNoData ? static_cast<T>(noDataValue) : 0);
        }
        case GDT_CInt16:
        case GDT_CInt32:
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 11, 0)
        case GDT_CFloat16:
#endif
        case GDT_CFloat32:
        case GDT_CFloat64:
        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
    CPLError(CE_Failure, CPLE_NotSupported,
             "%s not supported for this data type.", __FUNCTION__);
    return std::pair(0, 0);
}

}  // namespace GDAL_MINMAXELT_NS

#endif  // GDAL_MINMAX_ELEMENT_INCLUDED
