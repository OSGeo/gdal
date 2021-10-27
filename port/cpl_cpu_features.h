/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Prototypes, and definitions for of CPU features detection
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef CPL_CPU_FEATURES_H
#define CPL_CPU_FEATURES_H

#include "cpl_port.h"
#include "cpl_string.h"

//! @cond Doxygen_Suppress

#ifdef HAVE_SSE_AT_COMPILE_TIME
#if (defined(_M_X64) || defined(__x86_64))
#define HAVE_INLINE_SSE
static bool inline CPLHaveRuntimeSSE() { return true; }
#else
bool CPLHaveRuntimeSSE();
#endif
#endif

#ifdef HAVE_SSSE3_AT_COMPILE_TIME
#if __SSSE3__
#define HAVE_INLINE_SSSE3
static bool inline CPLHaveRuntimeSSSE3()
{
#ifdef DEBUG
    if( !CPLTestBool(CPLGetConfigOption("GDAL_USE_SSSE3", "YES")) )
        return false;
#endif
    return true;
}
#else
#if defined(__GNUC__) && !defined(DEBUG)
extern bool bCPLHasSSSE3;
static bool inline CPLHaveRuntimeSSSE3() { return bCPLHasSSSE3; }
#else
bool CPLHaveRuntimeSSSE3();
#endif
#endif
#endif

#ifdef HAVE_AVX_AT_COMPILE_TIME
#if __AVX__
#define HAVE_INLINE_AVX
static bool inline CPLHaveRuntimeAVX() { return true; }
#elif defined(__GNUC__)
extern bool bCPLHasAVX;
static bool inline CPLHaveRuntimeAVX() { return bCPLHasAVX; }
#else
bool CPLHaveRuntimeAVX();
#endif
#endif

//! @endcond

#endif // CPL_CPU_FEATURES_H
