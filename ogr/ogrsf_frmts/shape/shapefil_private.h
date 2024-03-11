#ifndef SHAPEFILE_PRIVATE_H_INCLUDED
#define SHAPEFILE_PRIVATE_H_INCLUDED

/******************************************************************************
 *
 * Project:  Shapelib
 * Purpose:  Private include file for Shapelib.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2012-2016, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
 ******************************************************************************
 *
 */

#ifdef __cplusplus
#define STATIC_CAST(type, x) static_cast<type>(x)
#define REINTERPRET_CAST(type, x) reinterpret_cast<type>(x)
#define CONST_CAST(type, x) const_cast<type>(x)
#define SHPLIB_NULLPTR nullptr
#else
#define STATIC_CAST(type, x) ((type)(x))
#define REINTERPRET_CAST(type, x) ((type)(x))
#define CONST_CAST(type, x) ((type)(x))
#define SHPLIB_NULLPTR NULL
#endif

#if !defined(SHP_BIG_ENDIAN)
#if defined(CPL_MSB)
#define SHP_BIG_ENDIAN 1
#elif (defined(__GNUC__) && __GNUC__ >= 5) ||                                  \
    (defined(__GNUC__) && defined(__GNUC_MINOR__) && __GNUC__ == 4 &&          \
     __GNUC_MINOR__ >= 6)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SHP_BIG_ENDIAN 1
#endif
#elif defined(__GLIBC__)
#if __BYTE_ORDER == __BIG_ENDIAN
#define SHP_BIG_ENDIAN 1
#endif
#elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#define SHP_BIG_ENDIAN 1
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#elif defined(__sparc) || defined(__sparc__) || defined(_POWER) ||             \
    defined(__powerpc__) || defined(__ppc__) || defined(__hpux) ||             \
    defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#define SHP_BIG_ENDIAN 1
#endif
#endif

#include "shapefil.h"
#include <stdint.h>
#include <stdlib.h>

/************************************************************************/
/*        Little endian <==> big endian byte swap macros.               */
/************************************************************************/

#if (defined(__GNUC__) && __GNUC__ >= 5) ||                                    \
    (defined(__GNUC__) && defined(__GNUC_MINOR__) && __GNUC__ == 4 &&          \
     __GNUC_MINOR__ >= 8)
#define _SHP_SWAP32(x)                                                         \
    STATIC_CAST(uint32_t, __builtin_bswap32(STATIC_CAST(uint32_t, x)))
#define _SHP_SWAP64(x)                                                         \
    STATIC_CAST(uint64_t, __builtin_bswap64(STATIC_CAST(uint64_t, x)))
#elif defined(_MSC_VER)
#define _SHP_SWAP32(x)                                                         \
    STATIC_CAST(uint32_t, _byteswap_ulong(STATIC_CAST(uint32_t, x)))
#define _SHP_SWAP64(x)                                                         \
    STATIC_CAST(uint64_t, _byteswap_uint64(STATIC_CAST(uint64_t, x)))
#else
#define _SHP_SWAP32(x)                                                         \
    STATIC_CAST(uint32_t,                                                      \
                ((STATIC_CAST(uint32_t, x) & 0x000000ffU) << 24) |             \
                    ((STATIC_CAST(uint32_t, x) & 0x0000ff00U) << 8) |          \
                    ((STATIC_CAST(uint32_t, x) & 0x00ff0000U) >> 8) |          \
                    ((STATIC_CAST(uint32_t, x) & 0xff000000U) >> 24))
#define _SHP_SWAP64(x)                                                         \
    ((STATIC_CAST(uint64_t, _SHP_SWAP32(STATIC_CAST(uint32_t, x))) << 32) |    \
     (STATIC_CAST(uint64_t, _SHP_SWAP32(STATIC_CAST(                           \
                                uint32_t, STATIC_CAST(uint64_t, x) >> 32)))))

#endif

/* in-place uint32_t* swap */
#define SHP_SWAP32(p)                                                          \
    *REINTERPRET_CAST(uint32_t *, p) =                                         \
        _SHP_SWAP32(*REINTERPRET_CAST(uint32_t *, p))
/* in-place uint64_t* swap */
#define SHP_SWAP64(p)                                                          \
    *REINTERPRET_CAST(uint64_t *, p) =                                         \
        _SHP_SWAP64(*REINTERPRET_CAST(uint64_t *, p))
/* in-place double* swap */
#define SHP_SWAPDOUBLE(x)                                                      \
    do                                                                         \
    {                                                                          \
        uint64_t _n64;                                                         \
        void *_lx = x;                                                         \
        memcpy(&_n64, _lx, 8);                                                 \
        _n64 = _SHP_SWAP64(_n64);                                              \
        memcpy(_lx, &_n64, 8);                                                 \
    } while (0)
/* copy double* swap*/
#define SHP_SWAPDOUBLE_CPY(dst, src)                                           \
    do                                                                         \
    {                                                                          \
        uint64_t _n64;                                                         \
        const void *_ls = src;                                                 \
        void *_ld = dst;                                                       \
        memcpy(&_n64, _ls, 8);                                                 \
        _n64 = _SHP_SWAP64(_n64);                                              \
        memcpy(_ld, &_n64, 8);                                                 \
    } while (0)
#endif /* ndef SHAPEFILE_PRIVATE_H_INCLUDED */
