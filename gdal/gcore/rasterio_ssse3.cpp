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

#include "cpl_port.h"

CPL_CVSID("$Id$")

#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && ( defined(__x86_64) || defined(_M_X64) )

#include <tmmintrin.h>
#include "gdal_priv_templates.hpp"

void GDALUnrolledCopy_GByte_3_1_SSSE3( GByte* CPL_RESTRICT pDest,
                                             const GByte* CPL_RESTRICT pSrc,
                                             GInt64 nIters );

void GDALUnrolledCopy_GByte_3_1_SSSE3( GByte* CPL_RESTRICT pDest,
                                             const GByte* CPL_RESTRICT pSrc,
                                             GInt64 nIters )
{
    decltype(nIters) i;
    const __m128i xmm_shuffle0 = _mm_set_epi8(-1  ,-1  ,-1  ,-1,
                                              -1  ,-1  ,-1  ,-1,
                                              -1  ,-1  ,15  ,12,
                                              9   ,6   ,3   ,0);
    const __m128i xmm_shuffle1 = _mm_set_epi8(-1  ,-1  ,-1  ,-1,
                                              -1  ,14  ,11  ,8,
                                              5   ,2   ,-1  ,-1,
                                              -1  ,-1  ,-1  ,-1);
    const __m128i xmm_shuffle2 = _mm_set_epi8(13  ,10  ,7   ,4,
                                              1   ,-1  ,-1  ,-1,
                                              -1  ,-1  ,-1  ,-1,
                                              -1  ,-1  ,-1  ,-1);
    // If we were sure that there would always be 2 trailing bytes, we could
    // check against nIters - 15
    for ( i = 0; i < nIters - 16; i += 16 )
    {
        __m128i xmm0 = _mm_loadu_si128( reinterpret_cast<__m128i const*>(pSrc + 0) );
        __m128i xmm1 = _mm_loadu_si128( reinterpret_cast<__m128i const*>(pSrc + 16) );
        __m128i xmm2 = _mm_loadu_si128( reinterpret_cast<__m128i const*>(pSrc + 32) );

        // From LSB to MSB:
        // 0,x,x,1,x,x,2,x,x,3,x,x,4,x,x,5 --> 0,1,2,3,4,5,0,0,0,0,0,0,0,0,0
        xmm0 = _mm_shuffle_epi8(xmm0, xmm_shuffle0);
        // x,x,6,x,x,7,x,x,8,x,x,9,x,x,10,x --> 0,0,0,0,0,0,6,7,8,9,10,0,0,0,0,0
        xmm1 = _mm_shuffle_epi8(xmm1, xmm_shuffle1);
        // x,11,x,x,12,x,x,13,x,x,14,x,x,15,x,x --> 0,0,0,0,0,0,0,0,0,0,0,11,12,13,14,15
        xmm2 = _mm_shuffle_epi8(xmm2, xmm_shuffle2);
        xmm0 = _mm_or_si128(xmm0, xmm1);
        xmm0 = _mm_or_si128(xmm0, xmm2);

        _mm_storeu_si128( reinterpret_cast<__m128i*> (pDest + i), xmm0);

        pSrc += 3 * 16;
    }
    for( ; i < nIters; i++ )
    {
        pDest[i] = *pSrc;
        pSrc += 3;
    }
}

#endif // HAVE_SSSE3_AT_COMPILE_TIME
