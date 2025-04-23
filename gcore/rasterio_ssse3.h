/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  SSSE3 specializations
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RASTERIO_SSSE3_H_INCLUDED
#define RASTERIO_SSSE3_H_INCLUDED

#include "cpl_port.h"

#if defined(HAVE_SSSE3_AT_COMPILE_TIME) &&                                     \
    (defined(__x86_64) || defined(_M_X64) || defined(USE_NEON_OPTIMIZATIONS))

void GDALUnrolledCopy_GByte_3_1_SSSE3(GByte *CPL_RESTRICT pDest,
                                      const GByte *CPL_RESTRICT pSrc,
                                      GPtrDiff_t nIters);

void GDALDeinterleave3Byte_SSSE3(const GByte *CPL_RESTRICT pabySrc,
                                 GByte *CPL_RESTRICT pabyDest0,
                                 GByte *CPL_RESTRICT pabyDest1,
                                 GByte *CPL_RESTRICT pabyDest2, size_t nIters);

#if !defined(__GNUC__) || defined(__clang__)
// GCC excluded because the auto-vectorized SSE2 code is good enough
void GDALDeinterleave4Byte_SSSE3(const GByte *CPL_RESTRICT pabySrc,
                                 GByte *CPL_RESTRICT pabyDest0,
                                 GByte *CPL_RESTRICT pabyDest1,
                                 GByte *CPL_RESTRICT pabyDest2,
                                 GByte *CPL_RESTRICT pabyDest3, size_t nIters);
#endif

#if (defined(__GNUC__) && !defined(__clang__)) ||                              \
    defined(__INTEL_CLANG_COMPILER)
// Restricted to GCC/ICC only as only verified with it that it can properly
// auto-vectorize
void GDALDeinterleave3UInt16_SSSE3(const GUInt16 *CPL_RESTRICT panSrc,
                                   GUInt16 *CPL_RESTRICT panDest0,
                                   GUInt16 *CPL_RESTRICT panDest1,
                                   GUInt16 *CPL_RESTRICT panDest2,
                                   size_t nIters);

void GDALDeinterleave4UInt16_SSSE3(const GUInt16 *CPL_RESTRICT panSrc,
                                   GUInt16 *CPL_RESTRICT panDest0,
                                   GUInt16 *CPL_RESTRICT panDest1,
                                   GUInt16 *CPL_RESTRICT panDest2,
                                   GUInt16 *CPL_RESTRICT panDest3,
                                   size_t nIters);
#endif

void GDALTranspose2D_Byte_SSSE3(const uint8_t *CPL_RESTRICT pSrc,
                                uint8_t *CPL_RESTRICT pDst, size_t nSrcWidth,
                                size_t nSrcHeight);

#endif

#endif /* RASTERIO_SSSE3_H_INCLUDED */
