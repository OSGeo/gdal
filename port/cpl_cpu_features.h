/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Prototypes, and definitions for of CPU features detection
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_CPU_FEATURES_H
#define CPL_CPU_FEATURES_H

#include "cpl_port.h"
#include "cpl_string.h"

//! @cond Doxygen_Suppress

#ifdef HAVE_SSE_AT_COMPILE_TIME
#if (defined(_M_X64) || defined(__x86_64))
#define HAVE_INLINE_SSE

static bool inline CPLHaveRuntimeSSE()
{
    return true;
}
#else
bool CPLHaveRuntimeSSE();
#endif
#endif

#ifdef USE_NEON_OPTIMIZATIONS
static bool inline CPLHaveRuntimeSSSE3()
{
    return true;
}
#elif defined(HAVE_SSSE3_AT_COMPILE_TIME)
#if __SSSE3__
#define HAVE_INLINE_SSSE3

static bool inline CPLHaveRuntimeSSSE3()
{
#ifdef DEBUG
    if (!CPLTestBool(CPLGetConfigOption("GDAL_USE_SSSE3", "YES")))
        return false;
#endif
    return true;
}
#else
#if defined(__GNUC__) && !defined(DEBUG)
extern bool bCPLHasSSSE3;

static bool inline CPLHaveRuntimeSSSE3()
{
    return bCPLHasSSSE3;
}
#else
bool CPLHaveRuntimeSSSE3();
#endif
#endif
#endif

#ifdef HAVE_AVX_AT_COMPILE_TIME
#if __AVX__
#define HAVE_INLINE_AVX

static bool inline CPLHaveRuntimeAVX()
{
    return true;
}
#elif defined(__GNUC__)
extern bool bCPLHasAVX;

static bool inline CPLHaveRuntimeAVX()
{
    return bCPLHasAVX;
}
#else
bool CPLHaveRuntimeAVX();
#endif
#endif

//! @endcond

#endif  // CPL_CPU_FEATURES_H
