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

#ifdef HAVE_AVX_AT_COMPILE_TIME
#include <immintrin.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*         GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX()     */
/************************************************************************/

#define GDAL_mm256_load1_ps(x) _mm256_set_ps(x, x, x, x, x, x, x, x)

CPLErr
GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX(
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
    const __m256 ymm_small = GDAL_mm256_load1_ps(fEpsilon);
    const __m256 ymm_x = GDAL_mm256_load1_ps(fXPoint);
    const __m256 ymm_y = GDAL_mm256_load1_ps(fYPoint);
    __m256 ymm_nominator = _mm256_setzero_ps();
    __m256 ymm_denominator = _mm256_setzero_ps();
    int mask = 0;

#undef LOOP_SIZE
#if defined(__x86_64) || defined(_M_X64)
    /* This would also work in 32bit mode, but there are only 8 XMM registers */
    /* whereas we have 16 for 64bit */
#define LOOP_SIZE   16
    size_t nPointsRound = (nPoints / LOOP_SIZE) * LOOP_SIZE;
    for ( i = 0; i < nPointsRound; i += LOOP_SIZE )
    {
        __m256 ymm_rx = _mm256_sub_ps(_mm256_load_ps(pafX + i), ymm_x);            /* rx = pafX[i] - fXPoint */
        __m256 ymm_rx_8 = _mm256_sub_ps(_mm256_load_ps(pafX + i + 8), ymm_x);
        __m256 ymm_ry = _mm256_sub_ps(_mm256_load_ps(pafY + i), ymm_y);            /* ry = pafY[i] - fYPoint */
        __m256 ymm_ry_8 = _mm256_sub_ps(_mm256_load_ps(pafY + i + 8), ymm_y);
        __m256 ymm_r2 = _mm256_add_ps(_mm256_mul_ps(ymm_rx, ymm_rx),               /* r2 = rx * rx + ry * ry */
                                   _mm256_mul_ps(ymm_ry, ymm_ry));
        __m256 ymm_r2_8 = _mm256_add_ps(_mm256_mul_ps(ymm_rx_8, ymm_rx_8),
                                     _mm256_mul_ps(ymm_ry_8, ymm_ry_8));
        __m256 ymm_invr2 = _mm256_rcp_ps(ymm_r2);                               /* invr2 = 1.0f / r2 */
        __m256 ymm_invr2_8 = _mm256_rcp_ps(ymm_r2_8);
        ymm_nominator = _mm256_add_ps(ymm_nominator,                            /* nominator += invr2 * pafZ[i] */
                            _mm256_mul_ps(ymm_invr2, _mm256_load_ps(pafZ + i)));
        ymm_nominator = _mm256_add_ps(ymm_nominator,
                            _mm256_mul_ps(ymm_invr2_8, _mm256_load_ps(pafZ + i + 8)));
        ymm_denominator = _mm256_add_ps(ymm_denominator, ymm_invr2);           /* denominator += invr2 */
        ymm_denominator = _mm256_add_ps(ymm_denominator, ymm_invr2_8);
        mask = _mm256_movemask_ps(_mm256_cmp_ps(ymm_r2, ymm_small, _CMP_LT_OS)) |           /* if( r2 < fEpsilon) */
              (_mm256_movemask_ps(_mm256_cmp_ps(ymm_r2_8, ymm_small, _CMP_LT_OS)) << 8);
        if( mask )
            break;
    }
#else
#define LOOP_SIZE   8
    size_t nPointsRound = (nPoints / LOOP_SIZE) * LOOP_SIZE;
    for ( i = 0; i < nPointsRound; i += LOOP_SIZE )
    {
        __m256 ymm_rx = _mm256_sub_ps(_mm256_load_ps((float*)pafX + i), ymm_x);           /* rx = pafX[i] - fXPoint */
        __m256 ymm_ry = _mm256_sub_ps(_mm256_load_ps((float*)pafY + i), ymm_y);           /* ry = pafY[i] - fYPoint */
        __m256 ymm_r2 = _mm256_add_ps(_mm256_mul_ps(ymm_rx, ymm_rx),              /* r2 = rx * rx + ry * ry */
                                   _mm256_mul_ps(ymm_ry, ymm_ry));
        __m256 ymm_invr2 = _mm256_rcp_ps(ymm_r2);                              /* invr2 = 1.0f / r2 */
        ymm_nominator = _mm256_add_ps(ymm_nominator,                           /* nominator += invr2 * pafZ[i] */
                            _mm256_mul_ps(ymm_invr2, _mm256_load_ps((float*)pafZ + i)));
        ymm_denominator = _mm256_add_ps(ymm_denominator, ymm_invr2);           /* denominator += invr2 */
        mask = _mm256_movemask_ps(_mm256_cmp_ps(ymm_r2, ymm_small, _CMP_LT_OS));            /* if( r2 < fEpsilon) */
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
#undef LOOP_SIZE

    /* Get back nominator and denominator values for YMM registers */
    float afNominator[8], afDenominator[8];
    _mm256_storeu_ps(afNominator, ymm_nominator);
    _mm256_storeu_ps(afDenominator, ymm_denominator);

    float fNominator = afNominator[0] + afNominator[1] +
                       afNominator[2] + afNominator[3] +
                       afNominator[4] + afNominator[5] +
                       afNominator[6] + afNominator[7];
    float fDenominator = afDenominator[0] + afDenominator[1] +
                         afDenominator[2] + afDenominator[3] +
                         afDenominator[4] + afDenominator[5] +
                         afDenominator[6] + afDenominator[7];

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

#endif
