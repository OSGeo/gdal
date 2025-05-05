/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Floating point conversion functions. Convert 16- and 24-bit
 *           floating point numbers into the 32-bit IEEE 754 compliant ones.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@remotesensing.org>
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

#include "cpl_float.h"
#include "cpl_error.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <optional>

/************************************************************************/
/*                           HalfToFloat()                              */
/*                                                                      */
/*  16-bit floating point number to 32-bit one.                         */
/************************************************************************/

GUInt32 CPLHalfToFloat(GUInt16 iHalf)
{

    GUInt32 iSign = (iHalf >> 15) & 0x00000001;
    int iExponent = (iHalf >> 10) & 0x0000001f;
    GUInt32 iMantissa = iHalf & 0x000003ff;

    if (iExponent == 0)
    {
        if (iMantissa == 0)
        {
            /* --------------------------------------------------------------------
             */
            /*      Plus or minus zero. */
            /* --------------------------------------------------------------------
             */

            return iSign << 31;
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*      Denormalized number -- renormalize it. */
            /* --------------------------------------------------------------------
             */

            while (!(iMantissa & 0x00000400))
            {
                iMantissa <<= 1;
                iExponent -= 1;
            }

            iExponent += 1;
            iMantissa &= ~0x00000400U;
        }
    }
    else if (iExponent == 31)
    {
        if (iMantissa == 0)
        {
            /* --------------------------------------------------------------------
             */
            /*       Positive or negative infinity. */
            /* --------------------------------------------------------------------
             */

            return (iSign << 31) | 0x7f800000;
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*       NaN -- preserve sign and significand bits. */
            /* --------------------------------------------------------------------
             */

            return (iSign << 31) | 0x7f800000 | (iMantissa << 13);
        }
    }

    /* -------------------------------------------------------------------- */
    /*       Normalized number.                                             */
    /* -------------------------------------------------------------------- */

    iExponent = iExponent + (127 - 15);
    iMantissa = iMantissa << 13;

    /* -------------------------------------------------------------------- */
    /*       Assemble sign, exponent and mantissa.                          */
    /* -------------------------------------------------------------------- */

    /* coverity[overflow_sink] */
    return (iSign << 31) | (static_cast<GUInt32>(iExponent) << 23) | iMantissa;
}

/************************************************************************/
/*                           TripleToFloat()                            */
/*                                                                      */
/*  24-bit floating point number to 32-bit one.                         */
/************************************************************************/

GUInt32 CPLTripleToFloat(GUInt32 iTriple)
{

    GUInt32 iSign = (iTriple >> 23) & 0x00000001;
    int iExponent = (iTriple >> 16) & 0x0000007f;
    GUInt32 iMantissa = iTriple & 0x0000ffff;

    if (iExponent == 0)
    {
        if (iMantissa == 0)
        {
            /* --------------------------------------------------------------------
             */
            /*      Plus or minus zero. */
            /* --------------------------------------------------------------------
             */

            return iSign << 31;
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*      Denormalized number -- renormalize it. */
            /* --------------------------------------------------------------------
             */

            while (!(iMantissa & 0x00010000))
            {
                iMantissa <<= 1;
                iExponent -= 1;
            }

            iExponent += 1;
            iMantissa &= ~0x00010000U;
        }
    }
    else if (iExponent == 127)
    {
        if (iMantissa == 0)
        {
            /* --------------------------------------------------------------------
             */
            /*       Positive or negative infinity. */
            /* --------------------------------------------------------------------
             */

            return (iSign << 31) | 0x7f800000;
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*       NaN -- preserve sign and significand bits. */
            /* --------------------------------------------------------------------
             */

            return (iSign << 31) | 0x7f800000 | (iMantissa << 7);
        }
    }

    /* -------------------------------------------------------------------- */
    /*       Normalized number.                                             */
    /* -------------------------------------------------------------------- */

    iExponent = iExponent + (127 - 63);
    iMantissa = iMantissa << 7;

    /* -------------------------------------------------------------------- */
    /*       Assemble sign, exponent and mantissa.                          */
    /* -------------------------------------------------------------------- */

    /* coverity[overflow_sink] */
    return (iSign << 31) | (static_cast<GUInt32>(iExponent) << 23) | iMantissa;
}

/************************************************************************/
/*                            FloatToHalf()                             */
/************************************************************************/

GUInt16 CPLFloatToHalf(GUInt32 iFloat32, bool &bHasWarned)
{
    GUInt32 iSign = (iFloat32 >> 31) & 0x00000001;
    GUInt32 iExponent = (iFloat32 >> 23) & 0x000000ff;
    GUInt32 iMantissa = iFloat32 & 0x007fffff;

    if (iExponent == 255)
    {
        if (iMantissa == 0)
        {
            /* --------------------------------------------------------------------
             */
            /*       Positive or negative infinity. */
            /* --------------------------------------------------------------------
             */

            return static_cast<GUInt16>((iSign << 15) | 0x7C00);
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*       NaN -- preserve sign and significand bits. */
            /* --------------------------------------------------------------------
             */
            if (iMantissa >> 13)
                return static_cast<GUInt16>((iSign << 15) | 0x7C00 |
                                            (iMantissa >> 13));

            return static_cast<GUInt16>((iSign << 15) | 0x7E00);
        }
    }

    if (iExponent <= 127 - 15)
    {
        // Zero, float32 denormalized number or float32 too small normalized
        // number
        if (13 + 1 + 127 - 15 - iExponent >= 32)
            return static_cast<GUInt16>(iSign << 15);

        // Return a denormalized number
        return static_cast<GUInt16>(
            (iSign << 15) |
            ((iMantissa | 0x00800000) >> (13 + 1 + 127 - 15 - iExponent)));
    }
    if (iExponent - (127 - 15) >= 31)
    {
        if (!bHasWarned)
        {
            bHasWarned = true;
            float fVal = 0.0f;
            memcpy(&fVal, &iFloat32, 4);
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Value %.8g is beyond range of float16. Converted to %sinf",
                fVal, (fVal > 0) ? "+" : "-");
        }
        return static_cast<GUInt16>((iSign << 15) | 0x7C00);  // Infinity
    }

    /* -------------------------------------------------------------------- */
    /*       Normalized number.                                             */
    /* -------------------------------------------------------------------- */

    iExponent = iExponent - (127 - 15);
    iMantissa = iMantissa >> 13;

    /* -------------------------------------------------------------------- */
    /*       Assemble sign, exponent and mantissa.                          */
    /* -------------------------------------------------------------------- */

    // coverity[overflow_sink]
    return static_cast<GUInt16>((iSign << 15) | (iExponent << 10) | iMantissa);
}

GUInt16 CPLConvertFloatToHalf(float fFloat32)
{
    GUInt32 nFloat32;
    std::memcpy(&nFloat32, &fFloat32, sizeof nFloat32);
    bool bHasWarned = true;
    return CPLFloatToHalf(nFloat32, bHasWarned);
}

float CPLConvertHalfToFloat(GUInt16 nHalf)
{
    GUInt32 nFloat32 = CPLHalfToFloat(nHalf);
    float fFloat32;
    std::memcpy(&fFloat32, &nFloat32, sizeof fFloat32);
    return fFloat32;
}

namespace
{

template <typename T> struct Fraction
{
    using value_type = T;

    T num;
    T denom;
};

/** Approximate a floating point number as a fraction, using the method describe
 * in Richards, Ian (1981). Continued Fractions Without Tears. Mathematics
 * Magazine, Vol. 54, No. 4. https://doi.org/10.2307/2689627
 *
 * If the fraction cannot be approximated within the specified error tolerance
 * in a certain amount of iterations, a warning will be raised and  std::nullopt
 * will be returned.
 *
 * @param x the number to approximate as a fraction
 * @param err the maximum allowable absolute error in the approximation
 *
 * @return the approximated value, or std::nullopt
 *
*/
std::optional<Fraction<std::uint64_t>> FloatToFraction(double x, double err)
{
    using inttype = std::uint64_t;
    constexpr int MAX_ITER = 1000;

    const double sign = std::signbit(x) ? -1 : 1;

    double g(std::abs(x));
    inttype a(0);
    inttype b(1);
    inttype c(1);
    inttype d(0);

    Fraction<std::uint64_t> ret;

    for (int i = 0; i < MAX_ITER; i++)
    {
        if (!(g >= 0 &&
              g <= static_cast<double>(std::numeric_limits<inttype>::max())))
        {
            break;
        }
        const inttype s = static_cast<inttype>(std::floor(g));
        ret.num = a + s * c;
        ret.denom = b + s * d;

        a = c;
        b = d;
        c = ret.num;
        d = ret.denom;
        g = 1.0 / (g - static_cast<double>(s));

        const double approx = sign * static_cast<double>(ret.num) /
                              static_cast<double>(ret.denom);

        if (std::abs(approx - x) < err)
        {
            return ret;
        }
    }

    CPLError(CE_Warning, CPLE_AppDefined,
             "Failed to approximate %g as a fraction with error < %g in %d "
             "iterations",
             x, err, MAX_ITER);
    return std::nullopt;
}
}  // namespace

/** Return the largest value by which two input values can be
 *  divided, with the result being an integer. If no suitable
 *  value can be found, zero will be returned.
 */
double CPLGreatestCommonDivisor(double a, double b)
{
    if (a == 0 || !std::isfinite(a) || b == 0 || !std::isfinite(b))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Input values must be finite non-null values");
        return 0;
    }

    if (a == b)
    {
        return a;
    }

    // Check if one resolution is an integer factor of the other.
    // This is fast and succeeds in some cases where the method below fails.
    if (a > b && std::abs(std::round(a / b) - a / b) < 1e-8)
    {
        return b;
    }
    if (b > a && std::abs(std::round(b / a) - b / a) < 1e-8)
    {
        return a;
    }

    const auto approx_a = FloatToFraction(a, 1e-10);
    if (!approx_a.has_value())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not approximate resolution %.18g as a fraction", a);
        return 0;
    }

    const auto approx_b = FloatToFraction(b, 1e-10);
    if (!approx_b.has_value())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not approximate resolution %.18g as a fraction", b);
        return 0;
    }

    const double sign = std::signbit(a) ? -1 : 1;

    const auto &frac_a = approx_a.value();
    const auto &frac_b = approx_b.value();

    const auto common_denom = std::lcm(frac_a.denom, frac_b.denom);

    const auto num_a = static_cast<std::uint64_t>(
        frac_a.num * std::round(common_denom / frac_a.denom));
    const auto num_b = static_cast<std::uint64_t>(
        frac_b.num * std::round(common_denom / frac_b.denom));

    const auto common_num = std::gcd(num_a, num_b);

    // coverity[divide_by_zero]
    const auto common = sign * static_cast<double>(common_num) /
                        static_cast<double>(common_denom);

    const auto disaggregation_factor = std::max(a / common, b / common);
    if (disaggregation_factor > 10000)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Common resolution between %.18g and %.18g calculated at "
                 "%.18g which "
                 "would cause excessive disaggregation",
                 a, b, common);
        return 0;
    }

    return common;
}
