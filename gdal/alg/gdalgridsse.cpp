/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Implementation of GDAL scattered data gridder.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdalgrid.h"
#include "gdalgrid_priv.h"

#ifdef HAVE_SSE_AT_COMPILE_TIME
#include <xmmintrin.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                          CPLHaveRuntimeSSE()                         */
/************************************************************************/

#define CPUID_SSE_EDX_BIT     25

#if (defined(_M_X64) || defined(__x86_64))

int CPLHaveRuntimeSSE()
{
    return TRUE;
}

#elif defined(__GNUC__) && defined(__i386__)

int CPLHaveRuntimeSSE()
{
    int cpuinfo[4] = {0,0,0,0};
    GCC_CPUID(1, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
    return (cpuinfo[3] & (1 << CPUID_SSE_EDX_BIT)) != 0;
}

#elif defined(_MSC_VER) && defined(_M_IX86)

#if _MSC_VER <= 1310
static void inline __cpuid(int cpuinfo[4], int level)
{
    __asm 
    {
        push   ebx
        push   esi

        mov    esi,cpuinfo
        mov    eax,level  
        cpuid  
        mov    dword ptr [esi], eax
        mov    dword ptr [esi+4],ebx  
        mov    dword ptr [esi+8],ecx  
        mov    dword ptr [esi+0Ch],edx 

        pop    esi
        pop    ebx
    }
}
#else
#include <intrin.h>
#endif

int CPLHaveRuntimeSSE()
{
    int cpuinfo[4] = {0,0,0,0};
    __cpuid(cpuinfo, 1);
    return (cpuinfo[3] & (1 << CPUID_SSE_EDX_BIT)) != 0;
}

#else

int CPLHaveRuntimeSSE()
{
    return FALSE;
}
#endif

/************************************************************************/
/*         GDALGridInverseDistanceToAPower2NoSmoothingNoSearchSSE()     */
/************************************************************************/

CPLErr
GDALGridInverseDistanceToAPower2NoSmoothingNoSearchSSE(
                                        const void *poOptions,
                                        GUInt32 nPoints,
                                        CPL_UNUSED const double *unused_padfX,
                                        CPL_UNUSED const double *unused_padfY,
                                        CPL_UNUSED const double *unused_padfZ,
                                        double dfXPoint, double dfYPoint,
                                        double *pdfValue,
                                        void* hExtraParamsIn )
{
    size_t i = 0;
    GDALGridExtraParameters* psExtraParams = (GDALGridExtraParameters*) hExtraParamsIn;
    const float* pafX = psExtraParams->pafX;
    const float* pafY = psExtraParams->pafY;
    const float* pafZ = psExtraParams->pafZ;

    const float fEpsilon = 0.0000000000001f;
    const float fXPoint = (float)dfXPoint;
    const float fYPoint = (float)dfYPoint;
    const __m128 xmm_small = _mm_load1_ps((float*)&fEpsilon);
    const __m128 xmm_x = _mm_load1_ps((float*)&fXPoint);
    const __m128 xmm_y = _mm_load1_ps((float*)&fYPoint);
    __m128 xmm_nominator = _mm_setzero_ps();
    __m128 xmm_denominator = _mm_setzero_ps();
    int mask = 0;

#if defined(__x86_64) || defined(_M_X64)
    /* This would also work in 32bit mode, but there are only 8 XMM registers */
    /* whereas we have 16 for 64bit */
#define LOOP_SIZE   8
    size_t nPointsRound = (nPoints / LOOP_SIZE) * LOOP_SIZE;
    for ( i = 0; i < nPointsRound; i += LOOP_SIZE )
    {
        __m128 xmm_rx = _mm_sub_ps(_mm_load_ps(pafX + i), xmm_x);            /* rx = pafX[i] - fXPoint */
        __m128 xmm_rx_4 = _mm_sub_ps(_mm_load_ps(pafX + i + 4), xmm_x);
        __m128 xmm_ry = _mm_sub_ps(_mm_load_ps(pafY + i), xmm_y);            /* ry = pafY[i] - fYPoint */
        __m128 xmm_ry_4 = _mm_sub_ps(_mm_load_ps(pafY + i + 4), xmm_y);
        __m128 xmm_r2 = _mm_add_ps(_mm_mul_ps(xmm_rx, xmm_rx),               /* r2 = rx * rx + ry * ry */
                                   _mm_mul_ps(xmm_ry, xmm_ry));
        __m128 xmm_r2_4 = _mm_add_ps(_mm_mul_ps(xmm_rx_4, xmm_rx_4),
                                     _mm_mul_ps(xmm_ry_4, xmm_ry_4));
        __m128 xmm_invr2 = _mm_rcp_ps(xmm_r2);                               /* invr2 = 1.0f / r2 */
        __m128 xmm_invr2_4 = _mm_rcp_ps(xmm_r2_4);
        xmm_nominator = _mm_add_ps(xmm_nominator,                            /* nominator += invr2 * pafZ[i] */
                            _mm_mul_ps(xmm_invr2, _mm_load_ps(pafZ + i)));
        xmm_nominator = _mm_add_ps(xmm_nominator,
                            _mm_mul_ps(xmm_invr2_4, _mm_load_ps(pafZ + i + 4)));
        xmm_denominator = _mm_add_ps(xmm_denominator, xmm_invr2);           /* denominator += invr2 */
        xmm_denominator = _mm_add_ps(xmm_denominator, xmm_invr2_4);
        mask = _mm_movemask_ps(_mm_cmplt_ps(xmm_r2, xmm_small)) |           /* if( r2 < fEpsilon) */
              (_mm_movemask_ps(_mm_cmplt_ps(xmm_r2_4, xmm_small)) << 4);
        if( mask )
            break;
    }
#else
#define LOOP_SIZE   4
    size_t nPointsRound = (nPoints / LOOP_SIZE) * LOOP_SIZE;
    for ( i = 0; i < nPointsRound; i += LOOP_SIZE )
    {
        __m128 xmm_rx = _mm_sub_ps(_mm_load_ps((float*)pafX + i), xmm_x);           /* rx = pafX[i] - fXPoint */
        __m128 xmm_ry = _mm_sub_ps(_mm_load_ps((float*)pafY + i), xmm_y);           /* ry = pafY[i] - fYPoint */
        __m128 xmm_r2 = _mm_add_ps(_mm_mul_ps(xmm_rx, xmm_rx),              /* r2 = rx * rx + ry * ry */
                                   _mm_mul_ps(xmm_ry, xmm_ry));
        __m128 xmm_invr2 = _mm_rcp_ps(xmm_r2);                              /* invr2 = 1.0f / r2 */
        xmm_nominator = _mm_add_ps(xmm_nominator,                           /* nominator += invr2 * pafZ[i] */
                            _mm_mul_ps(xmm_invr2, _mm_load_ps((float*)pafZ + i)));
        xmm_denominator = _mm_add_ps(xmm_denominator, xmm_invr2);           /* denominator += invr2 */
        mask = _mm_movemask_ps(_mm_cmplt_ps(xmm_r2, xmm_small));            /* if( r2 < fEpsilon) */
        if( mask )
            break;
    }
#endif

    /* Find which i triggered r2 < fEpsilon */
    if( mask )
    {
        for(int j = 0; j < LOOP_SIZE; j++ )
        {
            if( mask & (1 << j) )
            {
                (*pdfValue) = (pafZ)[i + j];
                return CE_None;
            }
        }
    }

    /* Get back nominator and denominator values for XMM registers */
    float afNominator[4], afDenominator[4];
    _mm_storeu_ps(afNominator, xmm_nominator);
    _mm_storeu_ps(afDenominator, xmm_denominator);

    float fNominator = afNominator[0] + afNominator[1] +
                       afNominator[2] + afNominator[3];
    float fDenominator = afDenominator[0] + afDenominator[1] +
                         afDenominator[2] + afDenominator[3];

    /* Do the few remaining loop iterations */
    for ( ; i < nPoints; i++ )
    {
        const float fRX = pafX[i] - fXPoint;
        const float fRY = pafY[i] - fYPoint;
        const float fR2 =
            fRX * fRX + fRY * fRY;

        // If the test point is close to the grid node, use the point
        // value directly as a node value to avoid singularity.
        if ( fR2 < 0.0000000000001 )
        {
            break;
        }
        else
        {
            const float fInvR2 = 1.0f / fR2;
            fNominator += fInvR2 * pafZ[i];
            fDenominator += fInvR2;
        }
    }

    if( i != nPoints )
    {
        (*pdfValue) = pafZ[i];
    }
    else
    if ( fDenominator == 0.0 )
    {
        (*pdfValue) =
            ((GDALGridInverseDistanceToAPowerOptions*)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = fNominator / fDenominator;

    return CE_None;
}


#endif /* HAVE_SSE_AT_COMPILE_TIME */
