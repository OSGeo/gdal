/**********************************************************************
 * $Id$
 *
 * Name:     cpl_atomic_ops.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Atomic operation functions.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_atomic_ops.h"

#if defined(__MACH__) && defined(__APPLE__)

#include <libkern/OSAtomic.h>

int CPLAtomicAdd(volatile int* ptr, int increment)
{
  return OSAtomicAdd32(increment, (int*)(ptr));
}

#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))

#include <windows.h>

int CPLAtomicAdd(volatile int* ptr, int increment)
{
#if defined(_MSC_VER) && (_MSC_VER <= 1200)
  return InterlockedExchangeAdd((LONG*)(ptr), (LONG)(increment)) + increment;
#else
  return InterlockedExchangeAdd((volatile LONG*)(ptr), (LONG)(increment)) + increment;
#endif
}

#elif defined(__MINGW32__) && defined(__i386__)

#include <windows.h>

int CPLAtomicAdd(volatile int* ptr, int increment)
{
  return InterlockedExchangeAdd((LONG*)(ptr), (LONG)(increment)) + increment;
}

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

int CPLAtomicAdd(volatile int* ptr, int increment)
{
  int temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  return temp + increment;
}

#elif defined(HAVE_GCC_ATOMIC_BUILTINS)
/* Starting with GCC 4.1.0, built-in functions for atomic memory access are provided. */
/* see http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html */
/* We use a ./configure test to determine whether this builtins are available */
/* as it appears that the GCC 4.1 version used on debian etch is broken when linking */
/* such instructions... */
int CPLAtomicAdd(volatile int* ptr, int increment)
{
  if (increment > 0)
    return __sync_add_and_fetch(ptr, increment);
  else
    return __sync_sub_and_fetch(ptr, -increment);
}

#elif !defined(CPL_MULTIPROC_PTHREAD)
#warning "Needs real lock API to implement properly atomic increment"

/* Dummy implementation */
int CPLAtomicAdd(volatile int* ptr, int increment)
{
    (*ptr) += increment;
    return *ptr;
}
#else

#include "cpl_multiproc.h"

static void *hAtomicOpMutex = NULL;

/* Slow, but safe, implemenation using a mutex */
int CPLAtomicAdd(volatile int* ptr, int increment)
{
    CPLMutexHolder oMutex(&hAtomicOpMutex);
    (*ptr) += increment;
    return *ptr;
}

#endif

#ifndef HAS_CPL_INLINE

int CPLAtomicInc(volatile int* ptr)
{
    return CPLAtomicAdd(ptr, 1);
}

int CPLAtomicDec(volatile int* ptr)
{
    return CPLAtomicAdd(ptr, -1);
}

#endif
