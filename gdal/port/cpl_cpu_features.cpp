/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPU features detection
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

#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_cpu_features.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

#define CPUID_SSSE3_ECX_BIT     9
#define CPUID_OSXSAVE_ECX_BIT   27
#define CPUID_AVX_ECX_BIT       28

#define CPUID_SSE_EDX_BIT       25

#define BIT_XMM_STATE           (1 << 1)
#define BIT_YMM_STATE           (2 << 1)

#define REG_EAX                 0
#define REG_EBX                 1
#define REG_ECX                 2
#define REG_EDX                 3

#if defined(__GNUC__)
#if defined(__x86_64)
#define GCC_CPUID(level, a, b, c, d)            \
  __asm__ ("xchgq %%rbx, %q1\n"                 \
           "cpuid\n"                            \
           "xchgq %%rbx, %q1"                   \
       : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
       : "0" (level))
#else
#define GCC_CPUID(level, a, b, c, d)            \
  __asm__ ("xchgl %%ebx, %1\n"                  \
           "cpuid\n"                            \
           "xchgl %%ebx, %1"                    \
       : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
       : "0" (level))
#endif

#define CPL_CPUID(level, array) GCC_CPUID(level, array[0], array[1], array[2], array[3])

#elif defined(_MSC_VER) && defined(_M_IX86) && _MSC_VER <= 1310
static void inline __cpuid( int cpuinfo[4], int level )
{
    __asm
    {
        push   ebx
        push   esi

        mov    esi,cpuinfo
        mov    eax, level
        cpuid
        mov    dword ptr [esi], eax
        mov    dword ptr [esi+4], ebx
        mov    dword ptr [esi+8], ecx
        mov    dword ptr [esi+0Ch], edx

        pop    esi
        pop    ebx
    }
}

#define CPL_CPUID(level, array) __cpuid(array, level)

#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))

#include <intrin.h>
#define CPL_CPUID(level, array) __cpuid(array, level)

#endif

#if defined(HAVE_SSE_AT_COMPILE_TIME) && !defined(HAVE_INLINE_SSE)

/************************************************************************/
/*                          CPLHaveRuntimeSSE()                         */
/************************************************************************/

bool CPLHaveRuntimeSSE()
{
    int cpuinfo[4] = { 0, 0, 0, 0 };
    CPL_CPUID(1, cpuinfo);
    return (cpuinfo[REG_EDX] & (1 << CPUID_SSE_EDX_BIT)) != 0;
}

#endif

#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && !defined(HAVE_INLINE_SSSE3)

/************************************************************************/
/*                         CPLHaveRuntimeSSSE3()                        */
/************************************************************************/

bool CPLHaveRuntimeSSSE3()
{
#ifdef DEBUG
    if( !CPLTestBool(CPLGetConfigOption("GDAL_USE_SSSE3", "YES")) )
        return false;
#endif
    int cpuinfo[4] = { 0, 0, 0, 0 };
    CPL_CPUID(1, cpuinfo);
    return (cpuinfo[REG_ECX] & (1 << CPUID_SSSE3_ECX_BIT)) != 0;
}

#endif

#if defined(HAVE_AVX_AT_COMPILE_TIME) && !defined(HAVE_INLINE_AVX)

/************************************************************************/
/*                          CPLHaveRuntimeAVX()                         */
/************************************************************************/

#if defined(__GNUC__) && (defined(__i386__) ||defined(__x86_64))

bool CPLHaveRuntimeAVX()
{
    int cpuinfo[4] = { 0, 0, 0, 0 };
    CPL_CPUID(1, cpuinfo);

    // Check OSXSAVE feature.
    if( (cpuinfo[REG_ECX] & (1 << CPUID_OSXSAVE_ECX_BIT)) == 0 )
    {
        return false;
    }

    // Check AVX feature.
    if( (cpuinfo[REG_ECX] & (1 << CPUID_AVX_ECX_BIT)) == 0 )
    {
        return false;
    }

    // Issue XGETBV and check the XMM and YMM state bit.
    unsigned int nXCRLow;
    unsigned int nXCRHigh;
    __asm__ ("xgetbv" : "=a" (nXCRLow), "=d" (nXCRHigh) : "c" (0));
    if( (nXCRLow & ( BIT_XMM_STATE | BIT_YMM_STATE )) !=
                ( BIT_XMM_STATE | BIT_YMM_STATE ) )
    {
        return false;
    }

    return true;
}

#elif defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 160040219) && (defined(_M_IX86) || defined(_M_X64))
// _xgetbv available only in Visual Studio 2010 SP1 or later

bool CPLHaveRuntimeAVX()
{
    int cpuinfo[4] = { 0, 0, 0, 0 };
    CPL_CPUID(1, cpuinfo);

    // Check OSXSAVE feature.
    if( (cpuinfo[REG_ECX] & (1 << CPUID_OSXSAVE_ECX_BIT)) == 0 )
    {
        return false;
    }

    // Check AVX feature.
    if( (cpuinfo[REG_ECX] & (1 << CPUID_AVX_ECX_BIT)) == 0 )
    {
        return false;
    }

    // Issue XGETBV and check the XMM and YMM state bit.
    unsigned __int64 xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
    if( (xcrFeatureMask & ( BIT_XMM_STATE | BIT_YMM_STATE )) !=
                          ( BIT_XMM_STATE | BIT_YMM_STATE ) )
    {
        return false;
    }

    return true;
}

#else

int CPLHaveRuntimeAVX()
{
    return false;
}

#endif

#endif // defined(HAVE_AVX_AT_COMPILE_TIME) && !defined(CPLHaveRuntimeAVX)

//! @endcond
