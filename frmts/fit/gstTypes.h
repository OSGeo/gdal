/******************************************************************************
 * $Id$
 *
 * Project:  FIT Driver
 * Purpose:  Implement FIT Support - not using the SGI iflFIT library.
 * Author:   Philip Nemec, nemec@keyholecorp.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Keyhole, Inc.
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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.2  2001/07/06 18:46:25  nemec
 * Cleanup files - improve Windows build, make proper copyright notice
 *
 *
 */

#ifndef _gstTypes_h_
#define _gstTypes_h_

#include <stdarg.h>

#if     !defined(TRUE) || ((TRUE) != 1)
#ifdef TRUE
#undef TRUE
#endif
#define TRUE    (1)
#endif
#if     !defined(FALSE) || ((FALSE) != 0)
#ifdef FALSE
#undef FALSE
#endif
#define FALSE   (0)
#endif

typedef int (*gstItemGetFunc)(void *data, int tag, ...);

#if defined(__sgi)
#include <sys/types.h>
typedef uint16_t                        uint16;
typedef int16_t                         int16;
typedef uint32_t                        uint32;
typedef int32_t                         int32;
typedef uint64_t                        uint64;
typedef int64_t                         int64;
#elif defined(__linux__)
#include <sys/types.h>
typedef u_int16_t                       uint16;
typedef int16_t                         int16;
typedef u_int32_t                       uint32;
typedef int32_t                         int32;
typedef u_int64_t                       uint64;
typedef int64_t                         int64;
#define TRUE -1
#define FALSE 0
#elif defined(_WIN32)
typedef unsigned short                  uint16;
typedef short                           int16;
typedef unsigned long                   uint32;
typedef long                            int32;
typedef unsigned __int64                uint64;
typedef __int64                         int64;
#endif

typedef unsigned char                   uchar;

#endif // !_gstTypes_h_
