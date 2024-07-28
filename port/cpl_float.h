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
#include <limits>
#endif

CPL_C_START
GUInt32 CPL_DLL CPLHalfToFloat(GUInt16 iHalf);
GUInt32 CPL_DLL CPLTripleToFloat(GUInt32 iTriple);
CPL_C_END

#ifdef __cplusplus

// std::numeric_limits does not work for _Float16, thus we define
// GDALNumericLimits which does.

template <typename T> struct GDALNumericLimits : std::numeric_limits<T>
{
};

#ifdef SIZEOF__FLOAT16
template <> struct GDALNumericLimits<_Float16>
{
    static constexpr bool has_denorm = true;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_signed = true;

    static constexpr int digits = 11;

    static constexpr _Float16 epsilon()
    {
        return static_cast<_Float16>(6.103515625e-5);
    }

    static constexpr _Float16 min()
    {
        return static_cast<_Float16>(6.103515625e-5);
    }

    static constexpr _Float16 lowest()
    {
        return -65504;
    }

    static constexpr _Float16 max()
    {
        return 65504;
    }

    static constexpr _Float16 infinity()
    {
        return static_cast<_Float16>(1.0 / 0.0);
    }

    static constexpr _Float16 quiet_NaN()
    {
        return static_cast<_Float16>(0.0 / 0.0);
    }
};
#endif

GUInt16 CPL_DLL CPLFloatToHalf(GUInt32 iFloat32, bool &bHasWarned);

GUInt16 CPL_DLL CPLConvertFloatToHalf(float fFloat32);
float CPL_DLL CPLConvertHalfToFloat(GUInt16 nHalf);
#endif

#endif  // CPL_FLOAT_H_INCLUDED
