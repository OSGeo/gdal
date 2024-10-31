/******************************************************************************
 * $Id$
 *
 * Project:  CPL
 * Purpose:  Floating point conversion functions. Convert 16- and 24-bit
 *           floating point numbers into the 32-bit IEEE 754 compliant ones.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@remotesensing.org>
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * This code is based on the code from OpenEXR project with the following
 * copyright:
 *
 * Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
 * Digital Ltd. LLC
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *       Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * *       Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * *       Neither the name of Industrial Light & Magic nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef CPL_FLOAT_H_INCLUDED
#define CPL_FLOAT_H_INCLUDED

#include "cpl_port.h"

#ifdef __cplusplus
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#endif

CPL_C_START
GUInt32 CPL_DLL CPLHalfToFloat(GUInt16 iHalf);
GUInt32 CPL_DLL CPLTripleToFloat(GUInt32 iTriple);
CPL_C_END

#ifdef __cplusplus

GUInt16 CPL_DLL CPLFloatToHalf(GUInt32 iFloat32, bool &bHasWarned);

GUInt16 CPL_DLL CPLConvertFloatToHalf(float fFloat32);
float CPL_DLL CPLConvertHalfToFloat(GUInt16 nHalf);

// Define a type `GFloat16`. If the compiler supports it natively (as
// `_Float16`), then this class is a simple wrapper. Otherwise we
// store the values in a `GUInt16` as bit pattern.

//! @cond Doxygen_Suppress
struct GFloat16
{
#ifdef HAVE__FLOAT16

    // How we represent a `GFloat16` internally
    using repr = _Float16;

    // How we compute on `GFloat16` values
    using compute = _Float16;

    static constexpr repr computeToRepr(compute fValue)
    {
        return fValue;
    }

    static constexpr compute reprToCompute(repr rValue)
    {
        return rValue;
    }

    template <typename T> static constexpr repr toRepr(T fValue)
    {
        return static_cast<repr>(fValue);
    }

    template <typename T> static constexpr T fromRepr(repr rValue)
    {
        return static_cast<T>(rValue);
    }

#else

    // How we represent a `GFloat16` internally
    using repr = std::uint16_t;

    // How we compute on `GFloat16` values
    using compute = float;

    static constexpr unsigned float2unsigned(float f)
    {
        // return __builtin_bit_cast(unsigned, f);

        unsigned u{};
        std::memcpy(&u, &f, 4);
        return u;
    }

    static constexpr float unsigned2float(unsigned u)
    {
        // return __builtin_bit_cast(float, u);

        float f{};
        std::memcpy(&f, &u, 4);
        return f;
    }

    // Copied from cpl_float.cpp so that we can inline for performance
    static constexpr std::uint16_t computeToRepr(float fFloat32)
    {
        std::uint32_t iFloat32 = float2unsigned(fFloat32);

        std::uint32_t iSign = (iFloat32 >> 31) & 0x00000001;
        std::uint32_t iExponent = (iFloat32 >> 23) & 0x000000ff;
        std::uint32_t iMantissa = iFloat32 & 0x007fffff;

        if (iExponent == 255)
        {
            if (iMantissa == 0)
            {
                // Positive or negative infinity.
                return static_cast<std::int16_t>((iSign << 15) | 0x7C00);
            }

            // NaN -- preserve sign and significand bits.
            if (iMantissa >> 13)
                return static_cast<std::int16_t>((iSign << 15) | 0x7C00 |
                                                 (iMantissa >> 13));
            return static_cast<std::int16_t>((iSign << 15) | 0x7E00);
        }

        if (iExponent <= 127 - 15)
        {
            // Zero, float32 denormalized number or float32 too small normalized
            // number
            if (13 + 1 + 127 - 15 - iExponent >= 32)
                return static_cast<std::int16_t>(iSign << 15);

            // Return a denormalized number
            return static_cast<std::int16_t>(
                (iSign << 15) |
                ((iMantissa | 0x00800000) >> (13 + 1 + 127 - 15 - iExponent)));
        }

        if (iExponent - (127 - 15) >= 31)
        {
            return static_cast<std::int16_t>((iSign << 15) |
                                             0x7C00);  // Infinity
        }

        // Normalized number.
        iExponent = iExponent - (127 - 15);
        iMantissa = iMantissa >> 13;

        // Assemble sign, exponent and mantissa.
        // coverity[overflow_sink]
        return static_cast<std::int16_t>((iSign << 15) | (iExponent << 10) |
                                         iMantissa);
    }

    // Copied from cpl_float.cpp so that we can inline for performance
    static constexpr float reprToCompute(std::uint16_t iHalf)
    {
        std::uint32_t iSign = (iHalf >> 15) & 0x00000001;
        int iExponent = (iHalf >> 10) & 0x0000001f;
        std::uint32_t iMantissa = iHalf & 0x000003ff;

        if (iExponent == 31)
        {
            if (iMantissa == 0)
            {
                // Positive or negative infinity.
                return unsigned2float((iSign << 31) | 0x7f800000);
            }

            // NaN -- preserve sign and significand bits.
            return unsigned2float((iSign << 31) | 0x7f800000 |
                                  (iMantissa << 13));
        }

        if (iExponent == 0)
        {
            if (iMantissa == 0)
            {
                // Plus or minus zero.
                return unsigned2float(iSign << 31);
            }

            // Denormalized number -- renormalize it.
            while (!(iMantissa & 0x00000400))
            {
                iMantissa <<= 1;
                iExponent -= 1;
            }

            iExponent += 1;
            iMantissa &= ~0x00000400U;
        }

        // Normalized number.
        iExponent = iExponent + (127 - 15);
        iMantissa = iMantissa << 13;

        // Assemble sign, exponent and mantissa.
        /* coverity[overflow_sink] */
        return unsigned2float((iSign << 31) |
                              (static_cast<std::uint32_t>(iExponent) << 23) |
                              iMantissa);
    }

    template <typename T> static constexpr repr toRepr(T fValue)
    {
        return computeToRepr(static_cast<compute>(fValue));
    }

    template <typename T> static constexpr T fromRepr(repr rValue)
    {
        return static_cast<T>(reprToCompute(rValue));
    }

#endif

  private:
    repr rValue;

  public:
    constexpr compute get() const
    {
        return reprToCompute(rValue);
    }

    GFloat16() = default;
    GFloat16(const GFloat16 &) = default;
    GFloat16(GFloat16 &&) = default;
    GFloat16 &operator=(const GFloat16 &) = default;
    GFloat16 &operator=(GFloat16 &&) = default;

    // Constructors and conversion operators

#ifdef HAVE__FLOAT16
    // cppcheck-suppress missingExplicitConstructor
    constexpr GFloat16(_Float16 hfValue) : rValue(hfValue)
    {
    }

    constexpr operator _Float16() const
    {
        return rValue;
    }
#endif

#define GDAL_DEFINE_CONVERSION(TYPE)                                           \
                                                                               \
    /* cppcheck-suppress missingExplicitConstructor */                         \
    constexpr GFloat16(TYPE fValue) : rValue(toRepr(fValue))                   \
    {                                                                          \
    }                                                                          \
                                                                               \
    constexpr operator TYPE() const                                            \
    {                                                                          \
        return fromRepr<TYPE>(rValue);                                         \
    }

    GDAL_DEFINE_CONVERSION(float)
    GDAL_DEFINE_CONVERSION(double)
    GDAL_DEFINE_CONVERSION(char)
    GDAL_DEFINE_CONVERSION(signed char)
    GDAL_DEFINE_CONVERSION(short)
    GDAL_DEFINE_CONVERSION(int)
    GDAL_DEFINE_CONVERSION(long)
    GDAL_DEFINE_CONVERSION(long long)
    GDAL_DEFINE_CONVERSION(unsigned char)
    GDAL_DEFINE_CONVERSION(unsigned short)
    GDAL_DEFINE_CONVERSION(unsigned int)
    GDAL_DEFINE_CONVERSION(unsigned long)
    GDAL_DEFINE_CONVERSION(unsigned long long)

#undef GDAL_DEFINE_CONVERSION

    // Arithmetic operators

    friend constexpr GFloat16 operator+(GFloat16 x)
    {
        return +x.get();
    }

    friend constexpr GFloat16 operator-(GFloat16 x)
    {
        return -x.get();
    }

#define GDAL_DEFINE_ARITHOP(OP)                                                \
                                                                               \
    friend constexpr GFloat16 operator OP(GFloat16 x, GFloat16 y)              \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend constexpr double operator OP(double x, GFloat16 y)                  \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr float operator OP(float x, GFloat16 y)                    \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr GFloat16 operator OP(int x, GFloat16 y)                   \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr double operator OP(GFloat16 x, double y)                  \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend constexpr float operator OP(GFloat16 x, float y)                    \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend constexpr GFloat16 operator OP(GFloat16 x, int y)                   \
    {                                                                          \
        return x.get() OP y;                                                   \
    }

    GDAL_DEFINE_ARITHOP(+)
    GDAL_DEFINE_ARITHOP(-)
    GDAL_DEFINE_ARITHOP(*)
    GDAL_DEFINE_ARITHOP(/)

#undef GDAL_DEFINE_ARITHOP

    // Comparison operators

#define GDAL_DEFINE_COMPARISON(OP)                                             \
                                                                               \
    friend constexpr bool operator OP(GFloat16 x, GFloat16 y)                  \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(float x, GFloat16 y)                     \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(double x, GFloat16 y)                    \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(int x, GFloat16 y)                       \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(GFloat16 x, float y)                     \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(GFloat16 x, double y)                    \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend constexpr bool operator OP(GFloat16 x, int y)                       \
    {                                                                          \
        return x.get() OP y;                                                   \
    }

    GDAL_DEFINE_COMPARISON(==)
    GDAL_DEFINE_COMPARISON(!=)
    GDAL_DEFINE_COMPARISON(<)
    GDAL_DEFINE_COMPARISON(>)
    GDAL_DEFINE_COMPARISON(<=)
    GDAL_DEFINE_COMPARISON(>=)

#undef GDAL_DEFINE_COMPARISON

    // Standard math functions

    friend constexpr bool isfinite(GFloat16 x)
    {
        using std::isfinite;
        return isfinite(float(x));
    }

    friend constexpr bool isinf(GFloat16 x)
    {
        using std::isinf;
        return isinf(float(x));
    }

    friend constexpr bool isnan(GFloat16 x)
    {
        using std::isnan;
        return isnan(float(x));
    }

    friend constexpr GFloat16 abs(GFloat16 x)
    {
        using std::abs;
        return GFloat16(abs(float(x)));
    }

    friend constexpr GFloat16 cbrt(GFloat16 x)
    {
        using std::cbrt;
        return GFloat16(cbrt(float(x)));
    }

    friend constexpr GFloat16 ceil(GFloat16 x)
    {
        using std::ceil;
        return GFloat16(ceil(float(x)));
    }

    friend constexpr GFloat16 fabs(GFloat16 x)
    {
        using std::fabs;
        return GFloat16(fabs(float(x)));
    }

    friend constexpr GFloat16 floor(GFloat16 x)
    {
        using std::floor;
        return GFloat16(floor(float(x)));
    }

    friend constexpr GFloat16 round(GFloat16 x)
    {
        using std::round;
        return GFloat16(round(float(x)));
    }

    friend constexpr GFloat16 sqrt(GFloat16 x)
    {
        using std::sqrt;
        return GFloat16(sqrt(float(x)));
    }

    friend constexpr GFloat16 fmax(GFloat16 x, GFloat16 y)
    {
        using std::fmax;
        return GFloat16(fmax(float(x), float(y)));
    }

    friend constexpr GFloat16 fmin(GFloat16 x, GFloat16 y)
    {
        using std::fmin;
        return GFloat16(fmin(float(x), float(y)));
    }

    friend constexpr GFloat16 hypot(GFloat16 x, GFloat16 y)
    {
        using std::hypot;
        return GFloat16(hypot(float(x), float(y)));
    }

    friend constexpr GFloat16 max(GFloat16 x, GFloat16 y)
    {
        using std::max;
        return GFloat16(max(float(x), float(y)));
    }

    friend constexpr GFloat16 min(GFloat16 x, GFloat16 y)
    {
        using std::min;
        return GFloat16(min(float(x), float(y)));
    }

    friend constexpr GFloat16 pow(GFloat16 x, GFloat16 y)
    {
        using std::pow;
        return GFloat16(pow(float(x), float(y)));
    }

    friend constexpr GFloat16 pow(GFloat16 x, int n)
    {
        using std::pow;
        return GFloat16(pow(float(x), n));
    }
};

//! @endcond

// Define some GDAL wrappers. Their C equivalents are defined in `cpl_port.h`.

template <typename T> constexpr int CPLIsNan(T x)
{
    using std::isnan, ::isnan;
    return isnan(x);
}

template <typename T> constexpr int CPLIsInf(T x)
{
    using std::isinf, ::isinf;
    return isinf(x);
}

template <typename T> constexpr int CPLIsFinite(T x)
{
    using std::isfinite, ::isfinite;
    return isfinite(x);
}

// std::numeric_limits does not work for _Float16, thus we define
// GDALNumericLimits which does.
//
// std::numeric_limits<_Float16> does not lead a compile-time error.
// Instead, it silently returns wrong values (all zeros), at least
// with GCC on Ubuntu 24.04.

template <typename T> struct GDALNumericLimits : std::numeric_limits<T>
{
};

template <> struct GDALNumericLimits<GFloat16>
{
    static constexpr bool has_denorm = true;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_signed = true;

    static constexpr int digits = 11;

    static constexpr GFloat16 epsilon()
    {
        // return GFloat16FromBits(0x1400);  // 0.000977
        return GFloat16(0.000977f);
    }

    static constexpr GFloat16 min()
    {
        // return GFloat16FromBits(0x0001);  // 6.0e-8
        return GFloat16(6.0e-8f);
    }

    static constexpr GFloat16 lowest()
    {
        // return GFloat16FromBits(0xfbff);  // -65504
        return GFloat16(-65504.0f);
    }

    static constexpr GFloat16 max()
    {
        // return GFloat16FromBits(0x7bff);  // +65504
        return GFloat16(+65504.0f);
    }

    static constexpr GFloat16 infinity()
    {
        // return GFloat16FromBits(0x7c00);  // inf
        return GFloat16(std::numeric_limits<float>::infinity());
    }

    static constexpr GFloat16 quiet_NaN()
    {
        // return GFloat16FromBits(0x7e00);  // nan
        return GFloat16(std::numeric_limits<float>::quiet_NaN());
    }
};

#endif

#endif  // CPL_FLOAT_H_INCLUDED
