/**********************************************************************
 * $Id$
 *
 * Name:     cpl_atomic_ops.h
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

#ifndef _CPL_ATOMIC_OPS_INCLUDED
#define _CPL_ATOMIC_OPS_INCLUDED

#include "cpl_port.h"

CPL_C_START

/** Add a value to a pointed integer in a thread and SMP-safe way
  * and return the resulting value of the operation.
  *
  * This function, which in most cases is implemented by a few
  * efficient machine instructions, guarantees that the value pointed
  * by ptr will be incremented in a thread and SMP-safe way.
  * The variables for this function must be aligned on a 32-bit boundary.
  *
  * Depending on the platforms, this function can also act as a
  * memory barrier, but this should not be assumed.
  *
  * Current platforms/architectures where an efficient implementation
  * exists are MacOSX, MS Windows, i386/x86_64 with GCC and platforms
  * supported by GCC 4.1 or higher. For other platforms supporting
  * the pthread library, and when GDAL is configured with thread-support,
  * the atomicity will be done with a mutex, but with
  * reduced efficiently. For the remaining platforms, a simple addition
  * with no locking will be done...
  *
  * @param ptr a pointer to an integer to increment
  * @param increment the amount to add to the pointed integer
  * @return the pointed value AFTER the result of the addition
  */
int CPL_DLL CPLAtomicAdd(volatile int* ptr, int increment);

/** Increment of 1 the pointed integer in a thread and SMP-safe way
  * and return the resulting value of the operation.
  *
  * @see CPLAtomicAdd for the details and guarantees of this atomic
  *      operation
  *
  * @param ptr a pointer to an integer to increment
  * @return the pointed value AFTER the opeation: *ptr + 1
  */
#define CPLAtomicInc(ptr) CPLAtomicAdd(ptr, 1)

/** Decrement of 1 the pointed integer in a thread and SMP-safe way
  * and return the resulting value of the operation.
  *
  * @see CPLAtomicAdd for the details and guarantees of this atomic
  *      operation
  *
  * @param ptr a pointer to an integer to decrement
  * @return the pointed value AFTER the opeation: *ptr - 1
  */
#define CPLAtomicDec(ptr) CPLAtomicAdd(ptr, -1)

CPL_C_END

#endif /* _CPL_ATOMIC_OPS_INCLUDED */
