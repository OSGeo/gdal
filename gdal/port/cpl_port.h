/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Include file providing low level portability services for CPL.
 *           This should be the first include file for any CPL based code.
 *
 ******************************************************************************
 * Copyright (c) 1998, 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef CPL_BASE_H_INCLUDED
#define CPL_BASE_H_INCLUDED

/**
 * \file cpl_port.h
 *
 * Core portability definitions for CPL.
 *
 */

/* ==================================================================== */
/*      We will use WIN32 as a standard windows define.                 */
/* ==================================================================== */
#if defined(_WIN32) && !defined(WIN32)
#  define WIN32
#endif

#if defined(_WINDOWS) && !defined(WIN32)
#  define WIN32
#endif

/* -------------------------------------------------------------------- */
/*      The following apparently allow you to use strcpy() and other    */
/*      functions judged "unsafe" by microsoft in VS 8 (2005).          */
/* -------------------------------------------------------------------- */
#ifdef _MSC_VER
#  ifndef _CRT_SECURE_NO_DEPRECATE
#    define _CRT_SECURE_NO_DEPRECATE
#  endif
#  ifndef _CRT_NONSTDC_NO_DEPRECATE
#    define _CRT_NONSTDC_NO_DEPRECATE
#  endif
#endif

#include "cpl_config.h"

/* ==================================================================== */
/*      A few sanity checks, mainly to detect problems that sometimes   */
/*      arise with bad configured cross-compilation.                    */
/* ==================================================================== */

#if !defined(SIZEOF_INT) || SIZEOF_INT != 4
#error "Unexpected value for SIZEOF_INT"
#endif

#if !defined(SIZEOF_UNSIGNED_LONG) || (SIZEOF_UNSIGNED_LONG != 4 && SIZEOF_UNSIGNED_LONG != 8)
#error "Unexpected value for SIZEOF_UNSIGNED_LONG"
#endif

#if !defined(SIZEOF_VOIDP) || (SIZEOF_VOIDP != 4 && SIZEOF_VOIDP != 8)
#error "Unexpected value for SIZEOF_VOIDP"
#endif


/* ==================================================================== */
/*      This will disable most WIN32 stuff in a Cygnus build which      */
/*      defines unix to 1.                                              */
/* ==================================================================== */

#ifdef unix
#  undef WIN32
#endif

#if defined(VSI_NEED_LARGEFILE64_SOURCE) && !defined(_LARGEFILE64_SOURCE)
#  define _LARGEFILE64_SOURCE 1
#endif

/* ==================================================================== */
/*      If iconv() is available use extended recoding module.           */
/*      Stub implementation is always compiled in, because it works     */
/*      faster than iconv() for encodings it supports.                  */
/* ==================================================================== */

#if defined(HAVE_ICONV)
#  define CPL_RECODE_ICONV
#endif

#define CPL_RECODE_STUB

/* ==================================================================== */
/*      MinGW stuff                                                     */
/* ==================================================================== */

/* We need __MSVCRT_VERSION__ >= 0x0601 to have "struct __stat64" */
/* Latest versions of mingw32 define it, but with older ones, */
/* we need to define it manually */
#if defined(__MINGW32__)
#ifndef __MSVCRT_VERSION__
#define __MSVCRT_VERSION__ 0x0601
#endif
#endif

/* ==================================================================== */
/*      Standard include files.                                         */
/* ==================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <time.h>

#if defined(HAVE_ERRNO_H)
#  include <errno.h>
#endif

#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

#ifdef HAVE_DIRECT_H
#  include <direct.h>
#endif

#if !defined(WIN32)
#  include <strings.h>
#endif

#if defined(HAVE_LIBDBMALLOC) && defined(HAVE_DBMALLOC_H) && defined(DEBUG)
#  define DBMALLOC
#  include <dbmalloc.h>
#endif

#if !defined(DBMALLOC) && defined(HAVE_DMALLOC_H)
#  define USE_DMALLOC
#  include <dmalloc.h>
#endif

/* ==================================================================== */
/*      Base portability stuff ... this stuff may need to be            */
/*      modified for new platforms.                                     */
/* ==================================================================== */

/*---------------------------------------------------------------------
 *        types for 16 and 32 bits integers, etc...
 *--------------------------------------------------------------------*/
#if UINT_MAX == 65535
typedef long            GInt32;
typedef unsigned long   GUInt32;
#else
typedef int             GInt32;
typedef unsigned int    GUInt32;
#endif

typedef short           GInt16;
typedef unsigned short  GUInt16;
typedef unsigned char   GByte;
/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h */
#ifndef CPL_GBOOL_DEFINED
#define CPL_GBOOL_DEFINED
typedef int             GBool;
#endif

/* -------------------------------------------------------------------- */
/*      64bit support                                                   */
/* -------------------------------------------------------------------- */

#if defined(WIN32) && defined(_MSC_VER)

#define VSI_LARGE_API_SUPPORTED
typedef __int64          GIntBig;
typedef unsigned __int64 GUIntBig;

#define GINTBIG_MIN     ((GIntBig)(0x80000000) << 32)
#define GINTBIG_MAX     (((GIntBig)(0x7FFFFFFF) << 32) | 0xFFFFFFFFU)
#define GUINTBIG_MAX     (((GUIntBig)(0xFFFFFFFFU) << 32) | 0xFFFFFFFFU)

#elif HAVE_LONG_LONG

typedef long long        GIntBig;
typedef unsigned long long GUIntBig;

#define GINTBIG_MIN     ((GIntBig)(0x80000000) << 32)
#define GINTBIG_MAX     (((GIntBig)(0x7FFFFFFF) << 32) | 0xFFFFFFFFU)
#define GUINTBIG_MAX     (((GUIntBig)(0xFFFFFFFF) << 32) | 0xFFFFFFFFU)

#else

typedef long             GIntBig;
typedef unsigned long    GUIntBig;

#define GINTBIG_MIN     INT_MIN
#define GINTBIG_MAX     INT_MAX
#define GUINTBIG_MAX     UINT_MAX
#endif

#if SIZEOF_VOIDP == 8
typedef GIntBig          GPtrDiff_t;
#else
typedef int              GPtrDiff_t;
#endif

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
  #define CPL_FRMT_GB_WITHOUT_PREFIX     "I64"
#elif HAVE_LONG_LONG
  #define CPL_FRMT_GB_WITHOUT_PREFIX     "ll"
#else
  #define CPL_FRMT_GB_WITHOUT_PREFIX     "l"
#endif

#define CPL_FRMT_GIB     "%" CPL_FRMT_GB_WITHOUT_PREFIX "d"
#define CPL_FRMT_GUIB    "%" CPL_FRMT_GB_WITHOUT_PREFIX "u"

/* Workaround VC6 bug */
#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#define GUINTBIG_TO_DOUBLE(x) (double)(GIntBig)(x)
#else
#define GUINTBIG_TO_DOUBLE(x) (double)(x)
#endif

#ifdef COMPAT_WITH_ICC_CONVERSION_CHECK
#define CPL_INT64_FITS_ON_INT32(x) ((x) >= INT_MIN && (x) <= INT_MAX)
#else
#define CPL_INT64_FITS_ON_INT32(x) (((GIntBig)(int)(x)) == (x))
#endif

/* ==================================================================== */
/*      Other standard services.                                        */
/* ==================================================================== */
#ifdef __cplusplus
#  define CPL_C_START           extern "C" {
#  define CPL_C_END             }
#else
#  define CPL_C_START
#  define CPL_C_END
#endif

#ifndef CPL_DLL
#if defined(_MSC_VER) && !defined(CPL_DISABLE_DLL)
#  define CPL_DLL     __declspec(dllexport)
#else
#  if defined(USE_GCC_VISIBILITY_FLAG)
#    define CPL_DLL     __attribute__ ((visibility("default")))
#  else
#    define CPL_DLL
#  endif
#endif
#endif

/* Should optional (normally private) interfaces be exported? */
#ifdef CPL_OPTIONAL_APIS
#  define CPL_ODLL CPL_DLL
#else
#  define CPL_ODLL
#endif

#ifndef CPL_STDCALL
#if defined(_MSC_VER) && !defined(CPL_DISABLE_STDCALL)
#  define CPL_STDCALL     __stdcall
#else
#  define CPL_STDCALL
#endif
#endif

#ifdef _MSC_VER
#  define FORCE_CDECL  __cdecl
#else
#  define FORCE_CDECL 
#endif

/* TODO : support for other compilers needed */
#if (defined(__GNUC__) && !defined(__NO_INLINE__)) || defined(_MSC_VER)
#define HAS_CPL_INLINE  1
#define CPL_INLINE __inline
#elif defined(__SUNPRO_CC)
#define HAS_CPL_INLINE  1
#define CPL_INLINE inline
#else
#define CPL_INLINE
#endif

#ifndef NULL
#  define NULL  0
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE  1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

#ifndef ABS
#  define ABS(x)        ((x<0) ? (-1*(x)) : x)
#endif

#ifndef M_PI
# define M_PI		3.14159265358979323846
/* 3.1415926535897932384626433832795 */
#endif

/* -------------------------------------------------------------------- */
/*      Macro to test equality of two floating point values.            */
/*      We use fabs() function instead of ABS() macro to avoid side     */
/*      effects.                                                        */
/* -------------------------------------------------------------------- */
#ifndef CPLIsEqual
#  define CPLIsEqual(x,y) (fabs((x) - (y)) < 0.0000000000001)
#endif

/* -------------------------------------------------------------------- */
/*      Provide macros for case insensitive string comparisons.         */
/* -------------------------------------------------------------------- */
#ifndef EQUAL

#if defined(AFL_FRIENDLY) && defined(__GNUC__)

static inline int CPL_afl_friendly_memcmp(const void* ptr1, const void* ptr2, size_t len)
        __attribute__((always_inline));

static inline int CPL_afl_friendly_memcmp(const void* ptr1, const void* ptr2, size_t len)
{
    const unsigned char* bptr1 = (const unsigned char*)ptr1;
    const unsigned char* bptr2 = (const unsigned char*)ptr2;
    while( len-- )
    {
        unsigned char b1 = *(bptr1++);
        unsigned char b2 = *(bptr2++);
        if( b1 != b2 ) return b1 - b2;
    }
    return 0;
}

static inline int CPL_afl_friendly_strcmp(const char* ptr1, const char* ptr2)
        __attribute__((always_inline));

static inline int CPL_afl_friendly_strcmp(const char* ptr1, const char* ptr2)
{
    const unsigned char* usptr1 = (const unsigned char*)ptr1;
    const unsigned char* usptr2 = (const unsigned char*)ptr2;
    while( 1 )
    {
        unsigned char ch1 = *(usptr1++);
        unsigned char ch2 = *(usptr2++);
        if( ch1 == 0 || ch1 != ch2 ) return ch1 - ch2;
    }
}

static inline int CPL_afl_friendly_strncmp(const char* ptr1, const char* ptr2, size_t len)
        __attribute__((always_inline));

static inline int CPL_afl_friendly_strncmp(const char* ptr1, const char* ptr2, size_t len)
{
    const unsigned char* usptr1 = (const unsigned char*)ptr1;
    const unsigned char* usptr2 = (const unsigned char*)ptr2;
    while( len -- )
    {
        unsigned char ch1 = *(usptr1++);
        unsigned char ch2 = *(usptr2++);
        if( ch1 == 0 || ch1 != ch2 ) return ch1 - ch2;
    }
    return 0;
}

static inline int CPL_afl_friendly_strcasecmp(const char* ptr1, const char* ptr2)
        __attribute__((always_inline));

static inline int CPL_afl_friendly_strcasecmp(const char* ptr1, const char* ptr2)
{
    const unsigned char* usptr1 = (const unsigned char*)ptr1;
    const unsigned char* usptr2 = (const unsigned char*)ptr2;
    while( 1 )
    {
        unsigned char ch1 = *(usptr1++);
        unsigned char ch2 = *(usptr2++);
        ch1 = (unsigned char)toupper(ch1);
        ch2 = (unsigned char)toupper(ch2);
        if( ch1 == 0 || ch1 != ch2 ) return ch1 - ch2;
    }
}

static inline int CPL_afl_friendly_strncasecmp(const char* ptr1, const char* ptr2, size_t len)
        __attribute__((always_inline));

static inline int CPL_afl_friendly_strncasecmp(const char* ptr1, const char* ptr2, size_t len)
{
    const unsigned char* usptr1 = (const unsigned char*)ptr1;
    const unsigned char* usptr2 = (const unsigned char*)ptr2;
    while( len-- )
    {
        unsigned char ch1 = *(usptr1++);
        unsigned char ch2 = *(usptr2++);
        ch1 = (unsigned char)toupper(ch1);
        ch2 = (unsigned char)toupper(ch2);
        if( ch1 == 0 || ch1 != ch2 ) return ch1 - ch2;
    }
    return 0;
}

static inline char* CPL_afl_friendly_strstr(const char* haystack, const char* needle)
        __attribute__((always_inline));

static inline char* CPL_afl_friendly_strstr(const char* haystack, const char* needle)
{
    const char* ptr_haystack = haystack;
    while( 1 )
    {
        const char* ptr_haystack2 = ptr_haystack;
        const char* ptr_needle = needle;
        while( 1 )
        {
            char ch1 = *(ptr_haystack2++);
            char ch2 = *(ptr_needle++);
            if( ch2 == 0 )
                return (char*)ptr_haystack;
            if( ch1 != ch2 )
                break;
        }
        if( *ptr_haystack == 0 )
            return NULL;
        ptr_haystack ++;
    }
}

#undef strcmp
#undef strncmp
#define memcmp CPL_afl_friendly_memcmp
#define strcmp CPL_afl_friendly_strcmp
#define strncmp CPL_afl_friendly_strncmp
#define strcasecmp CPL_afl_friendly_strcasecmp
#define strncasecmp CPL_afl_friendly_strncasecmp
#define strstr CPL_afl_friendly_strstr

#endif /* defined(AFL_FRIENDLY) && defined(__GNUC__) */

#  if defined(WIN32)
#    define STRCASECMP(a,b)         (stricmp(a,b))
#    define STRNCASECMP(a,b,n)      (strnicmp(a,b,n))
#  else
#    define STRCASECMP(a,b)         (strcasecmp(a,b))
#    define STRNCASECMP(a,b,n)      (strncasecmp(a,b,n))
#  endif
#  define EQUALN(a,b,n)           (STRNCASECMP(a,b,n)==0)
#  define EQUAL(a,b)              (STRCASECMP(a,b)==0)
#endif

#ifndef STARTS_WITH_CI
#define STARTS_WITH(a,b)               (strncmp(a,b,strlen(b)) == 0)
#define STARTS_WITH_CI(a,b)            EQUALN(a,b,strlen(b))
#endif

#ifndef CPL_THREADLOCAL
#  define CPL_THREADLOCAL
#endif

/* -------------------------------------------------------------------- */
/*      Handle isnan() and isinf().  Note that isinf() and isnan()      */
/*      are supposed to be macros according to C99, defined in math.h   */
/*      Some systems (i.e. Tru64) don't have isinf() at all, so if       */
/*      the macro is not defined we just assume nothing is infinite.    */
/*      This may mean we have no real CPLIsInf() on systems with isinf()*/
/*      function but no corresponding macro, but I can live with        */
/*      that since it isn't that important a test.                      */
/* -------------------------------------------------------------------- */
#ifdef _MSC_VER
#  include <float.h>
#  define CPLIsNan(x) _isnan(x)
#  define CPLIsInf(x) (!_isnan(x) && !_finite(x))
#  define CPLIsFinite(x) _finite(x)
#else
#  define CPLIsNan(x) isnan(x)
#  ifdef isinf 
#    define CPLIsInf(x) isinf(x)
#    define CPLIsFinite(x) (!isnan(x) && !isinf(x))
#  elif defined(__sun__)
#    include <ieeefp.h>
#    define CPLIsInf(x)    (!finite(x) && !isnan(x))
#    define CPLIsFinite(x) finite(x)
#  else
#    define CPLIsInf(x)    FALSE
#    define CPLIsFinite(x) (!isnan(x))
#  endif
#endif

/*---------------------------------------------------------------------
 *                         CPL_LSB and CPL_MSB
 * Only one of these 2 macros should be defined and specifies the byte 
 * ordering for the current platform.  
 * This should be defined in the Makefile, but if it is not then
 * the default is CPL_LSB (Intel ordering, LSB first).
 *--------------------------------------------------------------------*/
#if defined(WORDS_BIGENDIAN) && !defined(CPL_MSB) && !defined(CPL_LSB)
#  define CPL_MSB
#endif

#if ! ( defined(CPL_LSB) || defined(CPL_MSB) )
#define CPL_LSB
#endif

#if defined(CPL_LSB)
#  define CPL_IS_LSB 1
#else
#  define CPL_IS_LSB 0
#endif

#ifdef __cplusplus

extern "C++" {

template <bool b> struct CPLStaticAssert {};
template<> struct CPLStaticAssert<true>
{
    static void my_function() {}
};

} /* extern "C++" */

#define CPL_STATIC_ASSERT(x) CPLStaticAssert<x>::my_function()
#define CPL_STATIC_ASSERT_IF_AVAILABLE(x) CPL_STATIC_ASSERT(x)

#else  /* __cplusplus */

#define CPL_STATIC_ASSERT_IF_AVAILABLE(x) 

#endif  /* __cplusplus */

/*---------------------------------------------------------------------
 *        Little endian <==> big endian byte swap macros.
 *--------------------------------------------------------------------*/

#define CPL_SWAP16(x) \
        ((GUInt16)( \
            (((GUInt16)(x) & 0x00ffU) << 8) | \
            (((GUInt16)(x) & 0xff00U) >> 8) ))

#define CPL_SWAP16PTR(x) \
{                                                                 \
    GByte       byTemp, *_pabyDataT = (GByte *) (x);              \
    CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 2); \
                                                                  \
    byTemp = _pabyDataT[0];                                       \
    _pabyDataT[0] = _pabyDataT[1];                                \
    _pabyDataT[1] = byTemp;                                       \
}                                                                    

#define CPL_SWAP32(x) \
        ((GUInt32)( \
            (((GUInt32)(x) & (GUInt32)0x000000ffUL) << 24) | \
            (((GUInt32)(x) & (GUInt32)0x0000ff00UL) <<  8) | \
            (((GUInt32)(x) & (GUInt32)0x00ff0000UL) >>  8) | \
            (((GUInt32)(x) & (GUInt32)0xff000000UL) >> 24) ))

#define CPL_SWAP32PTR(x) \
{                                                                 \
    GByte       byTemp, *_pabyDataT = (GByte *) (x);              \
    CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 4);  \
                                                                  \
    byTemp = _pabyDataT[0];                                       \
    _pabyDataT[0] = _pabyDataT[3];                                \
    _pabyDataT[3] = byTemp;                                       \
    byTemp = _pabyDataT[1];                                       \
    _pabyDataT[1] = _pabyDataT[2];                                \
    _pabyDataT[2] = byTemp;                                       \
}                                                                    

#define CPL_SWAP64PTR(x) \
{                                                                 \
    GByte       byTemp, *_pabyDataT = (GByte *) (x);              \
    CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 8); \
                                                                  \
    byTemp = _pabyDataT[0];                                       \
    _pabyDataT[0] = _pabyDataT[7];                                \
    _pabyDataT[7] = byTemp;                                       \
    byTemp = _pabyDataT[1];                                       \
    _pabyDataT[1] = _pabyDataT[6];                                \
    _pabyDataT[6] = byTemp;                                       \
    byTemp = _pabyDataT[2];                                       \
    _pabyDataT[2] = _pabyDataT[5];                                \
    _pabyDataT[5] = byTemp;                                       \
    byTemp = _pabyDataT[3];                                       \
    _pabyDataT[3] = _pabyDataT[4];                                \
    _pabyDataT[4] = byTemp;                                       \
}                                                                    


/* Until we have a safe 64 bits integer data type defined, we'll replace
 * this version of the CPL_SWAP64() macro with a less efficient one.
 */
/*
#define CPL_SWAP64(x) \
        ((uint64)( \
            (uint64)(((uint64)(x) & (uint64)0x00000000000000ffULL) << 56) | \
            (uint64)(((uint64)(x) & (uint64)0x000000000000ff00ULL) << 40) | \
            (uint64)(((uint64)(x) & (uint64)0x0000000000ff0000ULL) << 24) | \
            (uint64)(((uint64)(x) & (uint64)0x00000000ff000000ULL) << 8) | \
            (uint64)(((uint64)(x) & (uint64)0x000000ff00000000ULL) >> 8) | \
            (uint64)(((uint64)(x) & (uint64)0x0000ff0000000000ULL) >> 24) | \
            (uint64)(((uint64)(x) & (uint64)0x00ff000000000000ULL) >> 40) | \
            (uint64)(((uint64)(x) & (uint64)0xff00000000000000ULL) >> 56) ))
*/

#define CPL_SWAPDOUBLE(p) CPL_SWAP64PTR(p)

#ifdef CPL_MSB
#  define CPL_MSBWORD16(x)      (x)
#  define CPL_LSBWORD16(x)      CPL_SWAP16(x)
#  define CPL_MSBWORD32(x)      (x)
#  define CPL_LSBWORD32(x)      CPL_SWAP32(x)
#  define CPL_MSBPTR16(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 2)
#  define CPL_LSBPTR16(x)       CPL_SWAP16PTR(x)
#  define CPL_MSBPTR32(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 4)
#  define CPL_LSBPTR32(x)       CPL_SWAP32PTR(x)
#  define CPL_MSBPTR64(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 8)
#  define CPL_LSBPTR64(x)       CPL_SWAP64PTR(x)
#else
#  define CPL_LSBWORD16(x)      (x)
#  define CPL_MSBWORD16(x)      CPL_SWAP16(x)
#  define CPL_LSBWORD32(x)      (x)
#  define CPL_MSBWORD32(x)      CPL_SWAP32(x)
#  define CPL_LSBPTR16(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 2)
#  define CPL_MSBPTR16(x)       CPL_SWAP16PTR(x)
#  define CPL_LSBPTR32(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 4)
#  define CPL_MSBPTR32(x)       CPL_SWAP32PTR(x)
#  define CPL_LSBPTR64(x)       CPL_STATIC_ASSERT_IF_AVAILABLE(sizeof(*(x)) == 1 || sizeof(*(x)) == 8)
#  define CPL_MSBPTR64(x)       CPL_SWAP64PTR(x)
#endif

/** Return a Int16 from the 2 bytes ordered in LSB order at address x */
#define CPL_LSBINT16PTR(x)    ((*(GByte*)(x)) | (*(((GByte*)(x))+1) << 8))

/** Return a Int32 from the 4 bytes ordered in LSB order at address x */
#define CPL_LSBINT32PTR(x)    ((*(GByte*)(x)) | (*(((GByte*)(x))+1) << 8) | \
                              (*(((GByte*)(x))+2) << 16) | (*(((GByte*)(x))+3) << 24))

/** Return a signed Int16 from the 2 bytes ordered in LSB order at address x */
#define CPL_LSBSINT16PTR(x) ((GInt16) CPL_LSBINT16PTR(x))

/** Return a unsigned Int16 from the 2 bytes ordered in LSB order at address x */
#define CPL_LSBUINT16PTR(x) ((GUInt16)CPL_LSBINT16PTR(x))

/** Return a signed Int32 from the 4 bytes ordered in LSB order at address x */
#define CPL_LSBSINT32PTR(x) ((GInt32) CPL_LSBINT32PTR(x))

/** Return a unsigned Int32 from the 4 bytes ordered in LSB order at address x */
#define CPL_LSBUINT32PTR(x) ((GUInt32)CPL_LSBINT32PTR(x))


/* Utility macro to explicitly mark intentionally unreferenced parameters. */
#ifndef UNREFERENCED_PARAM 
#  ifdef UNREFERENCED_PARAMETER /* May be defined by Windows API */
#    define UNREFERENCED_PARAM(param) UNREFERENCED_PARAMETER(param)
#  else
#    define UNREFERENCED_PARAM(param) ((void)param)
#  endif /* UNREFERENCED_PARAMETER */
#endif /* UNREFERENCED_PARAM */

/***********************************************************************
 * Define CPL_CVSID() macro.  It can be disabled during a build by
 * defining DISABLE_CVSID in the compiler options.
 *
 * The cvsid_aw() function is just there to prevent reports of cpl_cvsid()
 * being unused.
 */

#ifndef DISABLE_CVSID
#if defined(__GNUC__) && __GNUC__ >= 4
#  define CPL_CVSID(string)     static const char cpl_cvsid[] __attribute__((used)) = string;
#else
#  define CPL_CVSID(string)     static const char cpl_cvsid[] = string; \
static const char *cvsid_aw() { return( cvsid_aw() ? NULL : cpl_cvsid ); }
#endif
#else
#  define CPL_CVSID(string)
#endif

/* Null terminated variadic */
/* We exclude mingw64 4.6 which seems to be broken regarding this */
#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(DOXYGEN_SKIP) && !(defined(__MINGW64__) && __GNUC__ == 4 && __GNUC_MINOR__ == 6)
#   define CPL_NULL_TERMINATED     __attribute__((__sentinel__))
#else
#   define CPL_NULL_TERMINATED
#endif

#if defined(__GNUC__) && __GNUC__ >= 3 && !defined(DOXYGEN_SKIP)
#define CPL_PRINT_FUNC_FORMAT( format_idx, arg_idx )  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#define CPL_SCAN_FUNC_FORMAT( format_idx, arg_idx )  __attribute__((__format__ (__scanf__, format_idx, arg_idx)))
#else
#define CPL_PRINT_FUNC_FORMAT( format_idx, arg_idx )
#define CPL_SCAN_FUNC_FORMAT( format_idx, arg_idx )
#endif

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(DOXYGEN_SKIP)
#define CPL_WARN_UNUSED_RESULT                        __attribute__((warn_unused_result))
#else
#define CPL_WARN_UNUSED_RESULT
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#  define CPL_UNUSED __attribute((__unused__))
#else
/* TODO: add cases for other compilers */
#  define CPL_UNUSED
#endif

#if defined(__GNUC__) && __GNUC__ >= 3 && !defined(DOXYGEN_SKIP)
#define CPL_NO_RETURN                                __attribute__((noreturn))
#else
#define CPL_NO_RETURN
#endif

/* Clang __has_attribute */
#ifndef __has_attribute
  #define __has_attribute(x) 0  // Compatibility with non-clang compilers.
#endif

#if ((defined(__GNUC__) && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))) || __has_attribute(returns_nonnull)) && !defined(DOXYGEN_SKIP)
#  define CPL_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#  define CPL_RETURNS_NONNULL
#endif


#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(DOXYGEN_SKIP)
#define CPL_RESTRICT __restrict__
#else
#define CPL_RESTRICT
#endif

/* Helper to remove the copy and assignment constructors so that the compiler
   will not generate the default versions.

   Must be placed in the private section of a class and should be at the end.
*/
#ifdef __cplusplus
#if 1

#if __cplusplus >= 201103L
#define CPL_FINAL final
#define CPL_DISALLOW_COPY_ASSIGN(ClassName) \
    ClassName( const ClassName & ) = delete; \
    ClassName &operator=( const ClassName & ) = delete;
#else
#define CPL_FINAL
#define CPL_DISALLOW_COPY_ASSIGN(ClassName) \
    ClassName( const ClassName & ); \
    ClassName &operator=( const ClassName & );
#endif

#else
#define CPL_FINAL
#define CPL_DISALLOW_COPY_ASSIGN(ClassName)
#endif
#endif /* __cplusplus */

#if !defined(DOXYGEN_SKIP)
#if defined(__has_extension)
  #if __has_extension(attribute_deprecated_with_message)
    /* Clang extension */
    #define CPL_WARN_DEPRECATED(x)                       __attribute__ ((deprecated(x)))
  #else
    #define CPL_WARN_DEPRECATED(x)
  #endif
#elif defined(__GNUC__)
    #define CPL_WARN_DEPRECATED(x)                       __attribute__ ((deprecated))
#else
  #define CPL_WARN_DEPRECATED(x)
#endif
#endif

#if defined(GDAL_COMPILATION) && !defined(DONT_DEPRECATE_SPRINTF)
#define CPL_WARN_DEPRECATED_IF_GDAL_COMPILATION(x) CPL_WARN_DEPRECATED(x)
#else
#define CPL_WARN_DEPRECATED_IF_GDAL_COMPILATION(x)
#endif

#if !defined(_MSC_VER) && !defined(__APPLE__)
CPL_C_START
#ifdef WARN_STANDARD_PRINTF
int vsnprintf(char *str, size_t size, const char* fmt, va_list args) CPL_WARN_DEPRECATED("Use CPLvsnprintf() instead");
int snprintf(char *str, size_t size, const char* fmt, ...) CPL_PRINT_FUNC_FORMAT(3,4) CPL_WARN_DEPRECATED("Use CPLsnprintf() instead");
int sprintf(char *str, const char* fmt, ...) CPL_PRINT_FUNC_FORMAT(2, 3) CPL_WARN_DEPRECATED("Use CPLsnprintf() instead");
#elif defined(GDAL_COMPILATION) && !defined(DONT_DEPRECATE_SPRINTF)
int sprintf(char *str, const char* fmt, ...) CPL_PRINT_FUNC_FORMAT(2, 3) CPL_WARN_DEPRECATED("Use snprintf() or CPLsnprintf() instead");
#endif
CPL_C_END
#endif /* !defined(_MSC_VER) && !defined(__APPLE__) */

#if defined(MAKE_SANITIZE_HAPPY) || !(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
#define CPL_CPU_REQUIRES_ALIGNED_ACCESS
#define CPL_IS_DOUBLE_A_INT(d)  ( (d) >= INT_MIN && (d) <= INT_MAX && (double)(int)(d) == (d) )
#else
/* This is technically unspecified behaviour if the double is out of range, but works OK on x86 */
#define CPL_IS_DOUBLE_A_INT(d)  ( (double)(int)(d) == (d) )
#endif

#ifdef __cplusplus
/* The size of C style arrays. */
#define CPL_ARRAYSIZE(array) \
  ((sizeof(array) / sizeof(*(array))) / \
  static_cast<size_t>(!(sizeof(array) % sizeof(*(array)))))

extern "C++" {
template<class T> static void CPL_IGNORE_RET_VAL(T) {}
inline static bool CPL_TO_BOOL(int x) { return x != FALSE; }
} /* extern "C++" */

#endif  /* __cplusplus */

#if (((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) || (defined(__clang__) && __clang_major__ >= 3)) && !defined(_MSC_VER))
#define HAVE_GCC_DIAGNOSTIC_PUSH
#endif

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER)) 
#define HAVE_GCC_SYSTEM_HEADER
#endif

#endif /* ndef CPL_BASE_H_INCLUDED */
