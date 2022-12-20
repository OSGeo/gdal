/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  SSSE3 specializations
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef RASTERIO_SSSE3_H_INCLUDED
#define RASTERIO_SSSE3_H_INCLUDED

#include "cpl_port.h"

#if defined(HAVE_SSSE3_AT_COMPILE_TIME) &&                                     \
    (defined(__x86_64) || defined(_M_X64))

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

#endif

#endif /* RASTERIO_SSSE3_H_INCLUDED */
