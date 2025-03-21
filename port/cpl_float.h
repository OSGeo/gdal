/******************************************************************************
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
#ifdef HAVE_STD_FLOAT16_T
#include <stdfloat>
#endif
#endif

CPL_C_START
GUInt32 CPL_DLL CPLHalfToFloat(GUInt16 iHalf);
GUInt32 CPL_DLL CPLTripleToFloat(GUInt32 iTriple);
CPL_C_END

#ifdef __cplusplus

GUInt16 CPL_DLL CPLFloatToHalf(GUInt32 iFloat32, bool &bHasWarned);

GUInt16 CPL_DLL CPLConvertFloatToHalf(float fFloat32);
float CPL_DLL CPLConvertHalfToFloat(GUInt16 nHalf);

namespace cpl
{

// We define our own version of `std::numeric_limits` so that we can
// specialize it for `cpl::Float16` if necessary. Specializing
// `std::numeric_limits` doesn't always work because some libraries
// use `std::numeric_limits`, and one cannot specialize a type
// template after it has been used.
template <typename T> struct NumericLimits : std::numeric_limits<T>
{
};

#ifndef HAVE_STD_FLOAT16_T

// Define a type `cpl::Float16`. If the compiler supports it natively
// (as `_Float16`), then this class is a simple wrapper. Otherwise we
// store the values in a `GUInt16` as bit pattern.

//! @cond Doxygen_Suppress
struct Float16
{
    struct make_from_bits_and_value
    {
    };

#ifdef HAVE__FLOAT16

    // How we represent a `Float16` internally
    using repr = _Float16;

    // How we compute on `Float16` values
    using compute = _Float16;

    // Create a Float16 in a constexpr manner. Since we can't convert
    // bits in a constexpr function, we need to take both the bit
    // pattern and a float value as input, and can then choose which
    // of the two to use.
    constexpr Float16(make_from_bits_and_value, CPL_UNUSED std::uint16_t bits,
                      float fValue)
        : rValue(repr(fValue))
    {
    }

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

#else  // #ifndef HAVE__FLOAT16

    // How we represent a `Float16` internally
    using repr = std::uint16_t;

    // How we compute on `Float16` values
    using compute = float;

    // Create a Float16 in a constexpr manner. Since we can't convert
    // bits in a constexpr function, we need to take both the bit
    // pattern and a float value as input, and can then choose which
    // of the two to use.
    constexpr Float16(make_from_bits_and_value, std::uint16_t bits,
                      CPL_UNUSED float fValue)
        : rValue(bits)
    {
    }

    static unsigned float2unsigned(float f)
    {
        unsigned u;
        std::memcpy(&u, &f, 4);
        return u;
    }

    static float unsigned2float(unsigned u)
    {
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }

    // Copied from cpl_float.cpp so that we can inline for performance
    static std::uint16_t computeToRepr(float fFloat32)
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
    static float reprToCompute(std::uint16_t iHalf)
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

    template <typename T> static repr toRepr(T fValue)
    {
        return computeToRepr(static_cast<compute>(fValue));
    }

    template <typename T> static T fromRepr(repr rValue)
    {
        return static_cast<T>(reprToCompute(rValue));
    }

#endif  // #ifndef HAVE__FLOAT16

  private:
    repr rValue;

  public:
    compute get() const
    {
        return reprToCompute(rValue);
    }

    Float16() = default;
    Float16(const Float16 &) = default;
    Float16(Float16 &&) = default;
    Float16 &operator=(const Float16 &) = default;
    Float16 &operator=(Float16 &&) = default;

    // Constructors and conversion operators

#ifdef HAVE__FLOAT16
    // cppcheck-suppress noExplicitConstructor
    constexpr Float16(_Float16 hfValue) : rValue(hfValue)
    {
    }

    constexpr operator _Float16() const
    {
        return rValue;
    }
#endif

    // cppcheck-suppress-macro noExplicitConstructor
#define GDAL_DEFINE_CONVERSION(TYPE)                                           \
                                                                               \
    Float16(TYPE fValue) : rValue(toRepr(fValue))                              \
    {                                                                          \
    }                                                                          \
                                                                               \
    operator TYPE() const                                                      \
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

    friend Float16 operator+(Float16 x)
    {
        return +x.get();
    }

    friend Float16 operator-(Float16 x)
    {
        return -x.get();
    }

#define GDAL_DEFINE_ARITHOP(OP)                                                \
                                                                               \
    friend Float16 operator OP(Float16 x, Float16 y)                           \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend double operator OP(double x, Float16 y)                             \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend float operator OP(float x, Float16 y)                               \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend Float16 operator OP(int x, Float16 y)                               \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend double operator OP(Float16 x, double y)                             \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend float operator OP(Float16 x, float y)                               \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend Float16 operator OP(Float16 x, int y)                               \
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
    friend bool operator OP(Float16 x, Float16 y)                              \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend bool operator OP(float x, Float16 y)                                \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(double x, Float16 y)                               \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(int x, Float16 y)                                  \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(Float16 x, float y)                                \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(Float16 x, double y)                               \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(Float16 x, int y)                                  \
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

    friend bool isfinite(Float16 x)
    {
        using std::isfinite;
        return isfinite(float(x));
    }

    friend bool isinf(Float16 x)
    {
        using std::isinf;
        return isinf(float(x));
    }

    friend bool isnan(Float16 x)
    {
        using std::isnan;
        return isnan(float(x));
    }

    friend bool isnormal(Float16 x)
    {
        using std::isnormal;
        return isnormal(float(x));
    }

    friend bool signbit(Float16 x)
    {
        using std::signbit;
        return signbit(float(x));
    }

    friend Float16 abs(Float16 x)
    {
        using std::abs;
        return Float16(abs(float(x)));
    }

    friend Float16 cbrt(Float16 x)
    {
        using std::cbrt;
        return Float16(cbrt(float(x)));
    }

    friend Float16 ceil(Float16 x)
    {
        using std::ceil;
        return Float16(ceil(float(x)));
    }

    friend Float16 copysign(Float16 x, Float16 y)
    {
        using std::copysign;
        return Float16(copysign(float(x), float(y)));
    }

    friend Float16 fabs(Float16 x)
    {
        using std::fabs;
        return Float16(fabs(float(x)));
    }

    friend Float16 floor(Float16 x)
    {
        using std::floor;
        return Float16(floor(float(x)));
    }

    friend Float16 fmax(Float16 x, Float16 y)
    {
        using std::fmax;
        return Float16(fmax(float(x), float(y)));
    }

    friend Float16 fmin(Float16 x, Float16 y)
    {
        using std::fmin;
        return Float16(fmin(float(x), float(y)));
    }

    friend Float16 hypot(Float16 x, Float16 y)
    {
        using std::hypot;
        return Float16(hypot(float(x), float(y)));
    }

    friend Float16 max(Float16 x, Float16 y)
    {
        using std::max;
        return Float16(max(float(x), float(y)));
    }

    friend Float16 min(Float16 x, Float16 y)
    {
        using std::min;
        return Float16(min(float(x), float(y)));
    }

    // Adapted from the LLVM Project, under the Apache License v2.0
    friend Float16 nextafter(Float16 x, Float16 y)
    {
        if (isnan(x))
            return x;
        if (isnan(y))
            return y;
        if (x == y)
            return y;

        std::uint16_t bits;
        if (x != Float16(0))
        {
            std::memcpy(&bits, &x.rValue, 2);
            if ((x < y) == (x > Float16(0)))
                ++bits;
            else
                --bits;
        }
        else
        {
            bits = (signbit(y) << 15) | 0x0001;
        }

        Float16 r;
        std::memcpy(&r.rValue, &bits, 2);

        return r;
    }

    friend Float16 pow(Float16 x, Float16 y)
    {
        using std::pow;
        return Float16(pow(float(x), float(y)));
    }

    friend Float16 pow(Float16 x, int n)
    {
        using std::pow;
        return Float16(pow(float(x), n));
    }

    friend Float16 round(Float16 x)
    {
        using std::round;
        return Float16(round(float(x)));
    }

    friend Float16 sqrt(Float16 x)
    {
        using std::sqrt;
        return Float16(sqrt(float(x)));
    }
};

template <> struct NumericLimits<Float16>
{
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr bool has_denorm = true;
    static constexpr bool is_iec559 = true;

    static constexpr int digits = 11;
    static constexpr int digits10 = 3;
    static constexpr int max_digits10 = 5;
    static constexpr int radix = 2;

    static constexpr Float16 epsilon()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0x1400, 0.000977f);
    }

    static constexpr Float16 min()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0x0001, 6.0e-8f);
    }

    static constexpr Float16 lowest()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0xfbff, -65504.0f);
    }

    static constexpr Float16 max()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0x7bff, +65504.0f);
    }

    static constexpr Float16 infinity()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0x7c00,
                       std::numeric_limits<float>::infinity());
    }

    static constexpr Float16 quiet_NaN()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0x7e00,
                       std::numeric_limits<float>::quiet_NaN());
    }

    static constexpr Float16 signaling_NaN()
    {
        return Float16(Float16::make_from_bits_and_value{}, 0xfe00,
                       std::numeric_limits<float>::signaling_NaN());
    }
};

//! @endcond

#endif  // #ifndef HAVE_STD_FLOAT16_T

}  // namespace cpl

#ifdef HAVE_STD_FLOAT16_T
using GFloat16 = std::float16_t;
#else
using GFloat16 = cpl::Float16;
#endif

// Define some GDAL wrappers. Their C equivalents are defined in `cpl_port.h`.
// (These wrappers are not necessary any more in C++, one can always
// call `isnan` etc directly.)

template <typename T> constexpr int CPLIsNan(T x)
{
    // We need to write `using std::isnan` instead of directly using
    // `std::isnan` because `std::isnan` only supports the types
    // `float` and `double`. The `isnan` for `cpl::Float16` is found in the
    // `cpl` namespace via argument-dependent lookup
    // <https://en.cppreference.com/w/cpp/language/adl>.
    using std::isnan;
    return isnan(x);
}

template <typename T> constexpr int CPLIsInf(T x)
{
    using std::isinf;
    return isinf(x);
}

template <typename T> constexpr int CPLIsFinite(T x)
{
    using std::isfinite;
    return isfinite(x);
}

#endif  // #ifdef __cplusplus

double CPL_DLL CPLGreatestCommonDivisor(double x, double y);

#endif  // CPL_FLOAT_H_INCLUDED
