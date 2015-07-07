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

inline void GDALCopyWord(const float fValueIn, short &nValueOut)
{
    float fMaxVal, fMinVal;
    GDALGetDataLimits<float, short>(fMaxVal, fMinVal);
    float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f :
        fValueIn - 0.5f;
    nValueOut = static_cast<short>(
        GDALClampValue(fValue, fMaxVal, fMinVal));
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

#endif // GDAL_PRIV_TEMPLATES_HPP_INCLUDED
