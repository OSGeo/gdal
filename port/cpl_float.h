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

namespace cpl
{

// Define a type `CPLFloat16`. If the compiler supports it natively (as
// `_Float16`), then this class is a simple wrapper. Otherwise we
// store the values in a `GUInt16` as bit pattern.

//! @cond Doxygen_Suppress
struct CPLFloat16
{
    struct make_from_bits_and_value
    {
    };

#ifdef HAVE__FLOAT16

    // How we represent a `CPLFloat16` internally
    using repr = _Float16;

    // How we compute on `CPLFloat16` values
    using compute = _Float16;

    // Create a CPLFloat16 in a constexpr manner. Since we can't convert
    // bits in a constexpr function, we need to take both the bit
    // pattern and a float value as input, and can then choose which
    // of the two to use.
    constexpr CPLFloat16(make_from_bits_and_value,
                         CPL_UNUSED std::uint16_t bits, float fValue)
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

#else

    // How we represent a `CPLFloat16` internally
    using repr = std::uint16_t;

    // How we compute on `CPLFloat16` values
    using compute = float;

    // Create a CPLFloat16 in a constexpr manner. Since we can't convert
    // bits in a constexpr function, we need to take both the bit
    // pattern and a float value as input, and can then choose which
    // of the two to use.
    constexpr CPLFloat16(make_from_bits_and_value, std::uint16_t bits,
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

#endif

  private:
    repr rValue;

  public:
    compute get() const
    {
        return reprToCompute(rValue);
    }

    CPLFloat16() = default;
    CPLFloat16(const CPLFloat16 &) = default;
    CPLFloat16(CPLFloat16 &&) = default;
    CPLFloat16 &operator=(const CPLFloat16 &) = default;
    CPLFloat16 &operator=(CPLFloat16 &&) = default;

    // Constructors and conversion operators

#ifdef HAVE__FLOAT16
    // cppcheck-suppress noExplicitConstructor
    constexpr CPLFloat16(_Float16 hfValue) : rValue(hfValue)
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
    CPLFloat16(TYPE fValue) : rValue(toRepr(fValue))                           \
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

    friend CPLFloat16 operator+(CPLFloat16 x)
    {
        return +x.get();
    }

    friend CPLFloat16 operator-(CPLFloat16 x)
    {
        return -x.get();
    }

#define GDAL_DEFINE_ARITHOP(OP)                                                \
                                                                               \
    friend CPLFloat16 operator OP(CPLFloat16 x, CPLFloat16 y)                  \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend double operator OP(double x, CPLFloat16 y)                          \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend float operator OP(float x, CPLFloat16 y)                            \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend CPLFloat16 operator OP(int x, CPLFloat16 y)                         \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend double operator OP(CPLFloat16 x, double y)                          \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend float operator OP(CPLFloat16 x, float y)                            \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend CPLFloat16 operator OP(CPLFloat16 x, int y)                         \
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
    friend bool operator OP(CPLFloat16 x, CPLFloat16 y)                        \
    {                                                                          \
        return x.get() OP y.get();                                             \
    }                                                                          \
                                                                               \
    friend bool operator OP(float x, CPLFloat16 y)                             \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(double x, CPLFloat16 y)                            \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(int x, CPLFloat16 y)                               \
    {                                                                          \
        return x OP y.get();                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(CPLFloat16 x, float y)                             \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(CPLFloat16 x, double y)                            \
    {                                                                          \
        return x.get() OP y;                                                   \
    }                                                                          \
                                                                               \
    friend bool operator OP(CPLFloat16 x, int y)                               \
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

    friend bool isfinite(CPLFloat16 x)
    {
        using std::isfinite;
        return isfinite(float(x));
    }

    friend bool isinf(CPLFloat16 x)
    {
        using std::isinf;
        return isinf(float(x));
    }

    friend bool isnan(CPLFloat16 x)
    {
        using std::isnan;
        return isnan(float(x));
    }

    friend CPLFloat16 abs(CPLFloat16 x)
    {
        using std::abs;
        return CPLFloat16(abs(float(x)));
    }

    friend CPLFloat16 cbrt(CPLFloat16 x)
    {
        using std::cbrt;
        return CPLFloat16(cbrt(float(x)));
    }

    friend CPLFloat16 ceil(CPLFloat16 x)
    {
        using std::ceil;
        return CPLFloat16(ceil(float(x)));
    }

    friend CPLFloat16 fabs(CPLFloat16 x)
    {
        using std::fabs;
        return CPLFloat16(fabs(float(x)));
    }

    friend CPLFloat16 floor(CPLFloat16 x)
    {
        using std::floor;
        return CPLFloat16(floor(float(x)));
    }

    friend CPLFloat16 round(CPLFloat16 x)
    {
        using std::round;
        return CPLFloat16(round(float(x)));
    }

    friend CPLFloat16 sqrt(CPLFloat16 x)
    {
        using std::sqrt;
        return CPLFloat16(sqrt(float(x)));
    }

    friend CPLFloat16 fmax(CPLFloat16 x, CPLFloat16 y)
    {
        using std::fmax;
        return CPLFloat16(fmax(float(x), float(y)));
    }

    friend CPLFloat16 fmin(CPLFloat16 x, CPLFloat16 y)
    {
        using std::fmin;
        return CPLFloat16(fmin(float(x), float(y)));
    }

    friend CPLFloat16 hypot(CPLFloat16 x, CPLFloat16 y)
    {
        using std::hypot;
        return CPLFloat16(hypot(float(x), float(y)));
    }

    friend CPLFloat16 max(CPLFloat16 x, CPLFloat16 y)
    {
        using std::max;
        return CPLFloat16(max(float(x), float(y)));
    }

    friend CPLFloat16 min(CPLFloat16 x, CPLFloat16 y)
    {
        using std::min;
        return CPLFloat16(min(float(x), float(y)));
    }

    friend CPLFloat16 pow(CPLFloat16 x, CPLFloat16 y)
    {
        using std::pow;
        return CPLFloat16(pow(float(x), float(y)));
    }

    friend CPLFloat16 pow(CPLFloat16 x, int n)
    {
        using std::pow;
        return CPLFloat16(pow(float(x), n));
    }
};

//! @endcond
}  // namespace cpl

using GFloat16 = cpl::CPLFloat16;

// Define some GDAL wrappers. Their C equivalents are defined in `cpl_port.h`.
// (These wrappers are not necessary any mroe in C++, one can always
// call `isnan` etc directly.)

template <typename T> constexpr int CPLIsNan(T x)
{
    // We need to write `using std::isnan` instead of directly using
    // `std::isnan` because `std::isnan` only supports the types
    // `float` and `double`. The `isnan` for `CPLFloat16` is found in
    // the `cpl` namespace via argument-dependent lookup
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

namespace std
{

//! @cond Doxygen_Suppress
template <> struct numeric_limits<cpl::CPLFloat16>
{
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr bool has_denorm = true;

    static constexpr int digits = 11;
    static constexpr int digits10 = 3;
    static constexpr int max_digits10 = 5;
    static constexpr int radix = 2;

    static constexpr cpl::CPLFloat16 epsilon()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0x1400, 0.000977f);
    }

    static constexpr cpl::CPLFloat16 min()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0x0001, 6.0e-8f);
    }

    static constexpr cpl::CPLFloat16 lowest()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0xfbff, -65504.0f);
    }

    static constexpr cpl::CPLFloat16 max()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0x7bff, +65504.0f);
    }

    static constexpr cpl::CPLFloat16 infinity()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0x7c00, std::numeric_limits<float>::infinity());
    }

    static constexpr cpl::CPLFloat16 quiet_NaN()
    {
        return cpl::CPLFloat16(cpl::CPLFloat16::make_from_bits_and_value{},
                               0x7e00, std::numeric_limits<float>::quiet_NaN());
    }
};

//! @endcond

}  // namespace std

#endif

#endif  // CPL_FLOAT_H_INCLUDED
