/******************************************************************************
 *
 * Purpose:  Primary include file for PCIDSK SDK.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#ifndef PCIDSK_CONFIG_H_INCLUDED
#define PCIDSK_CONFIG_H_INCLUDED

// The PCIMAJORVERSION define is something we use internally at PCI Geomatics.
#ifdef PCIMAJORVERSION
#define CPL_UNUSED
#else
#include "cpl_port.h"
#endif

// Compatibility hack for non-C++11 compilers
#if !(__cplusplus >= 201103L || _MSC_VER >= 1500)
#define override
#endif

namespace PCIDSK {

    typedef unsigned char  uint8;
    typedef unsigned char  uchar;

#ifndef _PCI_TYPES
    typedef int            int32;
    typedef unsigned int   uint32;
    typedef short          int16;
    typedef unsigned short uint16;

#if defined(_MSC_VER)
    typedef __int64          int64;
    typedef unsigned __int64 uint64;
#else
    #if defined(PCIMAJORVERSION) && BUILDABI == 64
        typedef long               int64;
        typedef unsigned long      uint64;
    #else // BUILDABI
        typedef long long          int64;
        typedef unsigned long long uint64;
    #endif // BUILDABI
#endif

#endif // _PCI_TYPES

}

#ifdef _MSC_VER
# ifdef LIBPCIDSK_EXPORTS
#  define PCIDSK_DLL     __declspec(dllexport)
# else
#  define PCIDSK_DLL
# endif
#else
#  define PCIDSK_DLL
#endif

#if defined(__MSVCRT__) || defined(_MSC_VER)
  #define PCIDSK_FRMT_64_WITHOUT_PREFIX     "I64"
#else
  #if defined(PCIMAJORVERSION) && BUILDABI == 64
  #define PCIDSK_FRMT_64_WITHOUT_PREFIX     "l"
  #else // BUILDABI
  #define PCIDSK_FRMT_64_WITHOUT_PREFIX     "ll"
  #endif // BUILDABI
#endif

// #define MISSING_VSNPRINTF

/**
 * Versioning in the PCIDSK SDK
 * The version number for the PCIDSK SDK is to be used as follows:
 *  <ul>
 *  <li> If minor changes to the underlying fundamental classes are made,
 *          but no linkage-breaking changes are made, increment the minor
 *          number.
 *  <li> If major changes are made to the underlying interfaces that will
 *          break linkage, increment the major number.
 *  </ul>
 */
#define PCIDSK_SDK_MAJOR_VERSION    0
#define PCIDSK_SDK_MINOR_VERSION    1

#ifndef PCIDSK_DEFAULT_TILE_SIZE
#define PCIDSK_DEFAULT_TILE_SIZE 256
#endif

#if defined(__GNUC__) && __GNUC__ >= 3 && !defined(DOXYGEN_SKIP)
#define PCIDSK_PRINT_FUNC_FORMAT( format_idx, arg_idx )  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
#define PCIDSK_PRINT_FUNC_FORMAT( format_idx, arg_idx )
#endif

#ifndef GDAL_PCIDSK_DRIVER
#if defined(PCIDSK_INTERNAL) && !defined(ALIAS_CPLSNPRINTF_AS_SNPRINTF)
#include <stdlib.h>
extern "C" double CPLAtof(const char*);
extern "C" int CPLsnprintf(char *str, size_t size, const char* fmt, ...) PCIDSK_PRINT_FUNC_FORMAT(3,4);
#else
#define CPLAtof atof
#ifdef _MSC_VER
#define snprintf _snprintf
#define CPLsnprintf _snprintf
#else
#define CPLsnprintf snprintf
#endif
#endif
#endif

#if defined(__MSVCRT__) || defined(_MSC_VER)
  #define PCIDSK_FRMT_INT64     "%I64d"
  #define PCIDSK_FRMT_UINT64    "%I64u"
#else
  #if defined(PCIMAJORVERSION) && BUILDABI == 64
  #define PCIDSK_FRMT_INT64     "%ld"
  #define PCIDSK_FRMT_UINT64    "%lu"
  #else // BUILDABI
  #define PCIDSK_FRMT_INT64     "%lld"
  #define PCIDSK_FRMT_UINT64    "%llu"
  #endif // BUILDABI
#endif

#endif // PCIDSK_CONFIG_H_INCLUDED
