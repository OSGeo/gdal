/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview support in different drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_priv.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <complex>
#include <condition_variable>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_thread_pool.h"
#include "gdalwarper.h"

// Restrict to 64bit processors because they are guaranteed to have SSE2.
// Could possibly be used too on 32bit, but we would need to check at runtime.
#if defined(__x86_64) || defined(_M_X64)
#define USE_SSE2

#include "gdalsse_priv.h"
#endif

CPL_CVSID("$Id$")

/************************************************************************/
/*                     GDALResampleChunk32R_Near()                      */
/************************************************************************/

template <class T>
static CPLErr
GDALResampleChunk32R_NearT( double dfXRatioDstToSrc,
                            double dfYRatioDstToSrc,
                            GDALDataType eWrkDataType,
                            const T * pChunk,
                            int nChunkXOff, int nChunkXSize,
                            int nChunkYOff,
                            int nDstXOff, int nDstXOff2,
                            int nDstYOff, int nDstYOff2,
                            T** ppDstBuffer )

{
    const int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate buffers.                                               */
/* -------------------------------------------------------------------- */
    *ppDstBuffer = static_cast<T *>(
        VSI_MALLOC3_VERBOSE(nDstXWidth, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(eWrkDataType)));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    T* const pDstBuffer = *ppDstBuffer;

    int* panSrcXOff = static_cast<int *>(
        VSI_MALLOC_VERBOSE(nDstXWidth * sizeof(int)) );

    if( panSrcXOff == nullptr )
    {
        VSIFree(panSrcXOff);
        return CE_Failure;
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
        if( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;

        panSrcXOff[iDstPixel - nDstXOff] = nSrcXOff;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        int   nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        const T * const pSrcScanline =
            pChunk + (static_cast<GPtrDiff_t>(nSrcYOff-nChunkYOff) * nChunkXSize) - nChunkXOff;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        T* pDstScanline = pDstBuffer + (iDstLine - nDstYOff) * nDstXWidth;
        for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
        {
            pDstScanline[iDstPixel] = pSrcScanline[panSrcXOff[iDstPixel]];
        }
    }

    CPLFree( panSrcXOff );

    return CE_None;
}

static CPLErr
GDALResampleChunk32R_Near( double dfXRatioDstToSrc,
                           double dfYRatioDstToSrc,
                           double /* dfSrcXDelta */,
                           double /* dfSrcYDelta */,
                           GDALDataType eWrkDataType,
                           const void * pChunk,
                           const GByte * /* pabyChunkNodataMask_unused */,
                           int nChunkXOff, int nChunkXSize,
                           int nChunkYOff, int /* nChunkYSize */,
                           int nDstXOff, int nDstXOff2,
                           int nDstYOff, int nDstYOff2,
                           GDALRasterBand * /*poOverview*/,
                           void** ppDstBuffer,
                           GDALDataType* peDstBufferDataType,
                           const char * /* pszResampling_unused */,
                           int /* bHasNoData_unused */,
                           float /* fNoDataValue_unused */,
                           GDALColorTable* /* poColorTable_unused */,
                           GDALDataType /* eSrcDataType */,
                           bool /* bPropagateNoData */ )
{
    *peDstBufferDataType = eWrkDataType;
    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<const GByte *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            reinterpret_cast<GByte **>( ppDstBuffer ) );
    else if( eWrkDataType == GDT_UInt16 )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<const GInt16 *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            reinterpret_cast<GInt16 **>( ppDstBuffer ) );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<const float *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            reinterpret_cast<float **>( ppDstBuffer ));

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                          GDALFindBestEntry()                         */
/************************************************************************/

// Find in the color table the entry whose (c1,c2,c3) value is the closest
// (using quadratic distance) to the passed (nR,nG,nB) triplet, ignoring
// transparent entries.
static int GDALFindBestEntry( int nEntryCount, const GDALColorEntry* aEntries,
                              int nR, int nG, int nB )
{
    int nMinDist = std::numeric_limits<int>::max();
    int iBestEntry = 0;
    for( int i = 0; i < nEntryCount; ++i )
    {
        // Ignore transparent entries
        if( aEntries[i].c4 == 0 )
            continue;
        int nDist = (nR - aEntries[i].c1) *  (nR - aEntries[i].c1) +
            (nG - aEntries[i].c2) *  (nG - aEntries[i].c2) +
            (nB - aEntries[i].c3) *  (nB - aEntries[i].c3);
        if( nDist < nMinDist )
        {
            nMinDist = nDist;
            iBestEntry = i;
        }
    }
    return iBestEntry;
}

/************************************************************************/
/*                      ReadColorTableAsArray()                         */
/************************************************************************/

static bool ReadColorTableAsArray( const GDALColorTable* poColorTable,
                                   int& nEntryCount,
                                   GDALColorEntry*& aEntries,
                                   int& nTransparentIdx )
{
    nEntryCount = poColorTable->GetColorEntryCount();
    aEntries = static_cast<GDALColorEntry *>(
        VSI_MALLOC2_VERBOSE(sizeof(GDALColorEntry), nEntryCount) );
    nTransparentIdx = -1;
    if( aEntries == nullptr )
        return false;
    for( int i = 0; i < nEntryCount; ++i )
    {
        poColorTable->GetColorEntryAsRGB(i, &aEntries[i]);
        if( nTransparentIdx < 0 && aEntries[i].c4 == 0 )
            nTransparentIdx = i;
    }
    return true;
}

/************************************************************************/
/*                    GetReplacementValueIfNoData()                     */
/************************************************************************/

static float GetReplacementValueIfNoData(GDALDataType dt, int bHasNoData,
                                         float fNoDataValue)
{
    float fReplacementVal = 0.0f;
    if( bHasNoData )
    {
        if( dt == GDT_Byte )
        {
            if( fNoDataValue == std::numeric_limits<unsigned char>::max() )
                fReplacementVal = static_cast<float>(
                    std::numeric_limits<unsigned char>::max() - 1);
            else
                fReplacementVal = fNoDataValue + 1;
        }
        else if( dt == GDT_UInt16 )
        {
            if( fNoDataValue == std::numeric_limits<GUInt16>::max() )
                fReplacementVal = static_cast<float>(
                    std::numeric_limits<GUInt16>::max() - 1);
            else
                fReplacementVal = fNoDataValue + 1;
        }
        else if( dt == GDT_Int16 )
        {
            if( fNoDataValue == std::numeric_limits<GInt16>::max() )
                fReplacementVal = static_cast<float>(
                    std::numeric_limits<GInt16>::max() - 1);
            else
                fReplacementVal = fNoDataValue + 1;
        }
        else if( dt == GDT_UInt32 )
        {
            // Be careful to limited precision of float
            fReplacementVal = fNoDataValue + 1;
            double dfVal = fNoDataValue;
            if( fReplacementVal >= static_cast<double>(std::numeric_limits<GUInt32>::max() - 128) )
            {
                while( fReplacementVal == fNoDataValue )
                {
                    dfVal -= 1.0;
                    fReplacementVal = static_cast<float>(dfVal);
                }
            }
            else
            {
                while( fReplacementVal == fNoDataValue )
                {
                    dfVal += 1.0;
                    fReplacementVal = static_cast<float>(dfVal);
                }
            }
        }
        else if( dt == GDT_Int32 )
        {
            // Be careful to limited precision of float
            fReplacementVal = fNoDataValue + 1;
            double dfVal = fNoDataValue;
            if( fReplacementVal >= static_cast<double>(std::numeric_limits<GInt32>::max() - 64) )
            {
                while( fReplacementVal == fNoDataValue )
                {
                    dfVal -= 1.0;
                    fReplacementVal = static_cast<float>(dfVal);
                }
            }
            else
            {
                while( fReplacementVal == fNoDataValue )
                {
                    dfVal += 1.0;
                    fReplacementVal = static_cast<float>(dfVal);
                }
            }
        }
        else if( dt == GDT_Float32 || dt == GDT_Float64 )
        {
            if( fNoDataValue == 0 )
            {
                fReplacementVal = std::numeric_limits<float>::min();
            }
            else
            {
                fReplacementVal = static_cast<float>(
                    fNoDataValue + 1e-7 * fNoDataValue);
            }
        }
    }
    return fReplacementVal;
}

/************************************************************************/
/*                             SQUARE()                                 */
/************************************************************************/

template <class T, class Tsquare = T> inline Tsquare SQUARE(T val)
{
    return static_cast<Tsquare>(val) * val;
}

/************************************************************************/
/*                          ComputeIntegerRMS()                         */
/************************************************************************/
// Compute rms = sqrt(sumSquares / weight) in such a way that it is the
// integer that minimizes abs(rms**2 - sumSquares / weight)
template <class T> inline T ComputeIntegerRMS(double sumSquares, double weight)
{
    const double sumDivWeight = sumSquares / weight;
    T rms = static_cast<T>(sqrt(sumDivWeight));

    // Is rms**2 or (rms+1)**2 closest to sumSquares / weight ?
    // Naive version:
    // if( weight * (rms+1)**2 - sumSquares < sumSquares - weight * rms**2 )
    if( 2 * rms * (rms + 1) + 1 < 2 * sumDivWeight )
        rms += 1;
    return rms;
}

template <class T, class Tsum> inline T ComputeIntegerRMS_4values(Tsum)
{
    CPLAssert(false);
    return 0;
}

template<> inline GByte ComputeIntegerRMS_4values<GByte, int>(int sumSquares)
{
    // It has been verified that given the correction on rms below, using
    // sqrt((float)((sumSquares + 1)/ 4)) or sqrt((float)sumSquares * 0.25f)
    // is equivalent, so use the former as it is used twice.
    const int sumSquaresPlusOneDiv4 = (sumSquares + 1) / 4;
    const float sumDivWeight = static_cast<float>(sumSquaresPlusOneDiv4);
    GByte rms = static_cast<GByte>(std::sqrt(sumDivWeight));

    // Is rms**2 or (rms+1)**2 closest to sumSquares / weight ?
    // Naive version:
    // if( weight * (rms+1)**2 - sumSquares < sumSquares - weight * rms**2 )
    // Optimized version for integer case and weight == 4
    if( static_cast<int>(rms) * (rms + 1) < sumSquaresPlusOneDiv4 )
        rms += 1;
    return rms;
}

template<> inline GUInt16 ComputeIntegerRMS_4values<GUInt16, double>(double sumSquares)
{
    const double sumDivWeight = sumSquares * 0.25;
    GUInt16 rms = static_cast<GUInt16>(std::sqrt(sumDivWeight));

    // Is rms**2 or (rms+1)**2 closest to sumSquares / weight ?
    // Naive version:
    // if( weight * (rms+1)**2 - sumSquares < sumSquares - weight * rms**2 )
    // Optimized version for integer case and weight == 4
    if( static_cast<GUInt32>(rms) * (rms + 1) < static_cast<GUInt32>(sumDivWeight + 0.25) )
        rms += 1;
    return rms;
}

#ifdef USE_SSE2

/************************************************************************/
/*                   QuadraticMeanByteSSE2OrAVX2()                      */
/************************************************************************/

#ifdef __SSE4_1__
#define sse2_packus_epi32    _mm_packus_epi32
#else
inline __m128i sse2_packus_epi32(__m128i a, __m128i b)
{
    const auto minus32768_32 = _mm_set1_epi32(-32768);
    const auto minus32768_16 = _mm_set1_epi16(-32768);
    a = _mm_add_epi32(a, minus32768_32);
    b = _mm_add_epi32(b, minus32768_32);
    a = _mm_packs_epi32(a, b);
    a = _mm_sub_epi16(a, minus32768_16);
    return a;
}
#endif

#ifdef __SSSE3__
#define sse2_hadd_epi16     _mm_hadd_epi16
#else
inline __m128i sse2_hadd_epi16(__m128i a, __m128i b)
{
    // Horizontal addition of adjacent pairs
    const auto mask = _mm_set1_epi32(0xFFFF);
    const auto horizLo = _mm_add_epi32(_mm_and_si128(a, mask), _mm_srli_epi32(a, 16));
    const auto horizHi = _mm_add_epi32(_mm_and_si128(b, mask), _mm_srli_epi32(b, 16));

    // Recombine low and high parts
    return _mm_packs_epi32(horizLo, horizHi);
}
#endif


#ifdef __AVX2__
#define DEST_ELTS       16
#define set1_epi16      _mm256_set1_epi16
#define set1_epi32      _mm256_set1_epi32
#define setzero         _mm256_setzero_si256
#define set1_ps         _mm256_set1_ps
#define loadu_int(x)    _mm256_loadu_si256(reinterpret_cast<__m256i const*>(x))
#define unpacklo_epi8   _mm256_unpacklo_epi8
#define unpackhi_epi8   _mm256_unpackhi_epi8
#define madd_epi16      _mm256_madd_epi16
#define add_epi32       _mm256_add_epi32
#define mul_ps          _mm256_mul_ps
#define cvtepi32_ps     _mm256_cvtepi32_ps
#define sqrt_ps         _mm256_sqrt_ps
#define cvttps_epi32    _mm256_cvttps_epi32
#define packs_epi32     _mm256_packs_epi32
#define packus_epi32    _mm256_packus_epi32
#define srli_epi32      _mm256_srli_epi32
#define mullo_epi16     _mm256_mullo_epi16
#define srli_epi16      _mm256_srli_epi16
#define cmpgt_epi16     _mm256_cmpgt_epi16
#define add_epi16       _mm256_add_epi16
#define sub_epi16       _mm256_sub_epi16
#define packus_epi16    _mm256_packus_epi16
/* AVX2 operates on 2 separate 128-bit lanes, so we have to do shuffling */
/* to get the lower 128-bit bits of what would be a true 256-bit vector register */
#define store_lo(x,y)   _mm_storeu_si128(reinterpret_cast<__m128i*>(x), \
                            _mm256_extracti128_si256(_mm256_permute4x64_epi64((y), 0 | (2 << 2)), 0))
#define hadd_epi16      _mm256_hadd_epi16
#define zeroupper()     _mm256_zeroupper()
#else
#define DEST_ELTS       8
#define set1_epi16      _mm_set1_epi16
#define set1_epi32      _mm_set1_epi32
#define setzero         _mm_setzero_si128
#define set1_ps         _mm_set1_ps
#define loadu_int(x)    _mm_loadu_si128(reinterpret_cast<__m128i const*>(x))
#define unpacklo_epi8   _mm_unpacklo_epi8
#define unpackhi_epi8   _mm_unpackhi_epi8
#define madd_epi16      _mm_madd_epi16
#define add_epi32       _mm_add_epi32
#define mul_ps          _mm_mul_ps
#define cvtepi32_ps     _mm_cvtepi32_ps
#define sqrt_ps         _mm_sqrt_ps
#define cvttps_epi32    _mm_cvttps_epi32
#define packs_epi32     _mm_packs_epi32
#define packus_epi32    sse2_packus_epi32
#define srli_epi32      _mm_srli_epi32
#define mullo_epi16     _mm_mullo_epi16
#define srli_epi16      _mm_srli_epi16
#define cmpgt_epi16     _mm_cmpgt_epi16
#define add_epi16       _mm_add_epi16
#define sub_epi16       _mm_sub_epi16
#define packus_epi16    _mm_packus_epi16
#define store_lo(x,y)   _mm_storel_epi64(reinterpret_cast<__m128i*>(x), (y))
#define hadd_epi16      sse2_hadd_epi16
#define zeroupper()     (void)0
#endif

template<class T> static int QuadraticMeanByteSSE2OrAVX2(
                                                   int nDstXWidth,
                                                   int nChunkXSize,
                                                   const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                                   T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on Byte by
    // processing by group of 8 output pixels, so as to use
    // a single _mm_sqrt_ps() call for 4 output pixels
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto one16 = set1_epi16(1);
    const auto one32 = set1_epi32(1);
    const auto zero = setzero();
    const auto minus32768 = set1_epi16(-32768);

    for( ; iDstPixel < nDstXWidth - (DEST_ELTS-1); iDstPixel += DEST_ELTS )
    {
        // Load 2 * DEST_ELTS bytes from each line
        auto firstLine = loadu_int(pSrcScanlineShifted);
        auto secondLine = loadu_int(pSrcScanlineShifted + nChunkXSize);
        // Extend those Bytes as UInt16s
        auto firstLineLo = unpacklo_epi8(firstLine, zero);
        auto firstLineHi = unpackhi_epi8(firstLine, zero);
        auto secondLineLo = unpacklo_epi8(secondLine, zero);
        auto secondLineHi = unpackhi_epi8(secondLine, zero);

        // Multiplication of 16 bit values and horizontal
        // addition of 32 bit results
        // [ src[2*i+0]^2 + src[2*i+1]^2 for i in range(4) ]
        firstLineLo = madd_epi16(firstLineLo, firstLineLo);
        firstLineHi = madd_epi16(firstLineHi, firstLineHi);
        secondLineLo = madd_epi16(secondLineLo, secondLineLo);
        secondLineHi = madd_epi16(secondLineHi, secondLineHi);

        // Vertical addition
        const auto sumSquaresLo = add_epi32(firstLineLo, secondLineLo);
        const auto sumSquaresHi = add_epi32(firstLineHi, secondLineHi);

        const auto sumSquaresPlusOneDiv4Lo =
            srli_epi32(add_epi32(sumSquaresLo, one32), 2);
        const auto sumSquaresPlusOneDiv4Hi =
            srli_epi32(add_epi32(sumSquaresHi, one32), 2);

        // Take square root and truncate/floor to int32
        const auto rmsLo = cvttps_epi32(sqrt_ps(cvtepi32_ps(sumSquaresPlusOneDiv4Lo)));
        const auto rmsHi = cvttps_epi32(sqrt_ps(cvtepi32_ps(sumSquaresPlusOneDiv4Hi)));

        // Merge back low and high registers with each RMS value
        // as a 16 bit value.
        auto rms = packs_epi32(rmsLo, rmsHi);

        // Round to upper value if it minimizes the
        // error |rms^2 - sumSquares/4|
        // if( 2 * (2 * rms * (rms + 1) + 1) < sumSquares )
        //    rms += 1;
        // which is equivalent to:
        // if( rms * (rms + 1) < (sumSquares+1) / 4 )
        //    rms += 1;
        // And both left and right parts fit on 16 (unsigned) bits
        const auto sumSquaresPlusOneDiv4 = packus_epi32(
            sumSquaresPlusOneDiv4Lo, sumSquaresPlusOneDiv4Hi);
        // cmpgt_epi16 operates on signed int16, but here
        // we have unsigned values, so shift them by -32768 before
        auto mask = cmpgt_epi16(
            add_epi16(sumSquaresPlusOneDiv4, minus32768),
            add_epi16(mullo_epi16(rms, add_epi16(rms, one16)), minus32768));
        // The value of the mask will be -1 when the correction needs to be applied
        rms = sub_epi16(rms, mask);

        // Pack each 16 bit RMS value to 8 bits
        rms = packus_epi16(rms, rms /* could be anything */);
        store_lo(&pDstScanline[iDstPixel], rms);
        pSrcScanlineShifted += 2 * DEST_ELTS;
    }
    zeroupper();

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                      AverageByteSSE2OrAVX2()                         */
/************************************************************************/

template<class T> static int AverageByteSSE2OrAVX2(
                                             int nDstXWidth,
                                             int nChunkXSize,
                                             const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                             T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on Byte by
    // processing by group of 8 output pixels.

    const auto zero = setzero();
    const auto two16 = set1_epi16(2);
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    for( ; iDstPixel < nDstXWidth - (DEST_ELTS-1); iDstPixel += DEST_ELTS )
    {
        // Load 2 * DEST_ELTS bytes from each line
        const auto firstLine = loadu_int(pSrcScanlineShifted);
        const auto secondLine = loadu_int(pSrcScanlineShifted + nChunkXSize);
        // Extend those Bytes as UInt16s
        const auto firstLineLo = unpacklo_epi8(firstLine, zero);
        const auto firstLineHi = unpackhi_epi8(firstLine, zero);
        const auto secondLineLo = unpacklo_epi8(secondLine, zero);
        const auto secondLineHi = unpackhi_epi8(secondLine, zero);

        // Vertical addition
        const auto sumLo = add_epi16(firstLineLo, secondLineLo);
        const auto sumHi = add_epi16(firstLineHi, secondLineHi);

        // Horizontal addition of adjacent pairs, and recombine low and high parts
        const auto sum = hadd_epi16(sumLo, sumHi);

        // average = (sum + 2) / 4
        auto average = srli_epi16(add_epi16(sum, two16), 2);

        // Pack each 16 bit average value to 8 bits
        average = packus_epi16(average, average /* could be anything */);
        store_lo(&pDstScanline[iDstPixel], average);
        pSrcScanlineShifted += 2 * DEST_ELTS;
    }
    zeroupper();

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                     QuadraticMeanUInt16SSE2()                        */
/************************************************************************/

#ifdef __SSE3__
#define sse2_hadd_pd _mm_hadd_pd
#else
inline __m128d sse2_hadd_pd(__m128d a, __m128d b)
{
    auto aLo_bLo = _mm_castps_pd(_mm_movelh_ps(_mm_castpd_ps(a), _mm_castpd_ps(b)));
    auto aHi_bHi = _mm_castps_pd(_mm_movehl_ps(_mm_castpd_ps(b), _mm_castpd_ps(a)));
    return _mm_add_pd(aLo_bLo, aHi_bHi); // (aLo + aHi, bLo + bHi)
}
#endif

inline __m128d SQUARE(__m128d x)
{
    return _mm_mul_pd(x, x);
}

template<class T> static int QuadraticMeanUInt16SSE2(int nDstXWidth,
                                                     int nChunkXSize,
                                                     const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                                     T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on UInt16 by
    // processing by group of 4 output pixels.
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zero = _mm_setzero_si128();
    const auto zeroDot25 = _mm_set1_pd(0.25);
    const auto zeroDot5 = _mm_set1_pd(0.5);

    for( ; iDstPixel < nDstXWidth - 3; iDstPixel += 4 )
    {
        // Load 8 UInt16 from each line
        const auto firstLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted));
        const auto secondLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted + nChunkXSize));

        // Detect if all of the source values fit in 14 bits.
        // because if x < 2^14, then 4 * x^2 < 2^30 which fits in a signed int32
        // and we can do a much faster implementation.
        const auto maskTmp = _mm_srli_epi16(_mm_or_si128(firstLine, secondLine), 14);
        const auto nMaskFitsIn14Bits = _mm_cvtsi128_si64(
            _mm_packus_epi16(maskTmp, maskTmp /* could be anything */));
        if( nMaskFitsIn14Bits == 0 )
        {
            // Multiplication of 16 bit values and horizontal
            // addition of 32 bit results
            const auto firstLineHSumSquare = _mm_madd_epi16(firstLine, firstLine);
            const auto secondLineHSumSquare = _mm_madd_epi16(secondLine, secondLine);
            // Vertical addition
            const auto sumSquares = _mm_add_epi32(firstLineHSumSquare,
                                                  secondLineHSumSquare);
            // In theory we should take sqrt(sumSquares * 0.25f)
            // but given the rounding we do, this is equivalent to
            // sqrt((sumSquares + 1)/4). This has been verified exhaustively for
            // sumSquares <= 4 * 16383^2
            const auto one32 = _mm_set1_epi32(1);
            const auto sumSquaresPlusOneDiv4 =
                _mm_srli_epi32(_mm_add_epi32(sumSquares, one32), 2);
            // Take square root and truncate/floor to int32
            auto rms = _mm_cvttps_epi32(_mm_sqrt_ps(_mm_cvtepi32_ps(sumSquaresPlusOneDiv4)));

            // Round to upper value if it minimizes the
            // error |rms^2 - sumSquares/4|
            // if( 2 * (2 * rms * (rms + 1) + 1) < sumSquares )
            //    rms += 1;
            // which is equivalent to:
            // if( rms * rms + rms < (sumSquares+1) / 4 )
            //    rms += 1;
            auto mask = _mm_cmpgt_epi32(
                sumSquaresPlusOneDiv4,
                _mm_add_epi32(_mm_madd_epi16(rms, rms), rms));
            rms = _mm_sub_epi32(rms, mask);
            // Pack each 32 bit RMS value to 16 bits
            rms = _mm_packs_epi32(rms, rms /* could be anything */);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(&pDstScanline[iDstPixel]), rms);
            pSrcScanlineShifted += 8;
            continue;
        }

        // An approach using _mm_mullo_epi16, _mm_mulhi_epu16 before extending
        // to 32 bit would result in 4 multiplications instead of 8, but mullo/mulhi
        // have a worse throughput than mul_pd.

        // Extend those UInt16s as UInt32s
        const auto firstLineLo = _mm_unpacklo_epi16(firstLine, zero);
        const auto firstLineHi = _mm_unpackhi_epi16(firstLine, zero);
        const auto secondLineLo = _mm_unpacklo_epi16(secondLine, zero);
        const auto secondLineHi = _mm_unpackhi_epi16(secondLine, zero);

        // Multiplication of 32 bit values previously converted to 64 bit double
        const auto firstLineLoLo = SQUARE(_mm_cvtepi32_pd(firstLineLo));
        const auto firstLineLoHi = SQUARE(_mm_cvtepi32_pd(_mm_srli_si128(firstLineLo,8)));
        const auto firstLineHiLo = SQUARE(_mm_cvtepi32_pd(firstLineHi));
        const auto firstLineHiHi = SQUARE(_mm_cvtepi32_pd(_mm_srli_si128(firstLineHi,8)));

        const auto secondLineLoLo = SQUARE(_mm_cvtepi32_pd(secondLineLo));
        const auto secondLineLoHi = SQUARE(_mm_cvtepi32_pd(_mm_srli_si128(secondLineLo,8)));
        const auto secondLineHiLo = SQUARE(_mm_cvtepi32_pd(secondLineHi));
        const auto secondLineHiHi = SQUARE(_mm_cvtepi32_pd(_mm_srli_si128(secondLineHi,8)));

        // Vertical addition of squares
        const auto sumSquaresLoLo = _mm_add_pd(firstLineLoLo, secondLineLoLo);
        const auto sumSquaresLoHi = _mm_add_pd(firstLineLoHi, secondLineLoHi);
        const auto sumSquaresHiLo = _mm_add_pd(firstLineHiLo, secondLineHiLo);
        const auto sumSquaresHiHi = _mm_add_pd(firstLineHiHi, secondLineHiHi);

        // Horizontal addition of squares
        const auto sumSquaresLo = sse2_hadd_pd(sumSquaresLoLo, sumSquaresLoHi);
        const auto sumSquaresHi = sse2_hadd_pd(sumSquaresHiLo, sumSquaresHiHi);

        const auto sumDivWeightLo = _mm_mul_pd(sumSquaresLo, zeroDot25);
        const auto sumDivWeightHi = _mm_mul_pd(sumSquaresHi, zeroDot25);
        // Take square root and truncate/floor to int32
        const auto rmsLo = _mm_cvttpd_epi32(_mm_sqrt_pd(sumDivWeightLo));
        const auto rmsHi = _mm_cvttpd_epi32(_mm_sqrt_pd(sumDivWeightHi));

        // Correctly round rms to minimize | rms^2 - sumSquares / 4 |
        // if( 0.5 < sumDivWeight - (rms * rms + rms) )
        //     rms += 1;
        const auto rmsLoDouble = _mm_cvtepi32_pd(rmsLo);
        const auto rmsHiDouble = _mm_cvtepi32_pd(rmsHi);
        const auto rightLo = _mm_sub_pd(sumDivWeightLo,
                                _mm_add_pd(SQUARE(rmsLoDouble), rmsLoDouble));
        const auto rightHi = _mm_sub_pd(sumDivWeightHi,
                                _mm_add_pd(SQUARE(rmsHiDouble), rmsHiDouble));

        const auto maskLo = _mm_castpd_ps(_mm_cmplt_pd(zeroDot5, rightLo));
        const auto maskHi = _mm_castpd_ps(_mm_cmplt_pd(zeroDot5, rightHi));
        // The value of the mask will be -1 when the correction needs to be applied
        const auto mask = _mm_castps_si128(_mm_shuffle_ps(
            maskLo, maskHi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));

        auto rms = _mm_castps_si128(_mm_movelh_ps(
            _mm_castsi128_ps(rmsLo), _mm_castsi128_ps(rmsHi)));
        // Apply the correction
        rms = _mm_sub_epi32(rms, mask);

        // Pack each 32 bit RMS value to 16 bits
        rms = sse2_packus_epi32(rms, rms /* could be anything */);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&pDstScanline[iDstPixel]), rms);
        pSrcScanlineShifted += 8;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                         AverageUInt16SSE2()                          */
/************************************************************************/

template<class T> static int AverageUInt16SSE2(
                                             int nDstXWidth,
                                             int nChunkXSize,
                                             const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                             T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on UInt16 by
    // processing by group of 8 output pixels.

    const auto mask = _mm_set1_epi32(0xFFFF);
    const auto two = _mm_set1_epi32(2);
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    for( ; iDstPixel < nDstXWidth - 7; iDstPixel += 8 )
    {
        __m128i averageLow;
        // Load 8 UInt16 from each line
        {
        const auto firstLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted));
        const auto secondLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted + nChunkXSize));

        // Horizontal addition and extension to 32 bit
        const auto horizAddFirstLine = _mm_add_epi32(_mm_and_si128(firstLine, mask), _mm_srli_epi32(firstLine, 16));
        const auto horizAddSecondLine = _mm_add_epi32(_mm_and_si128(secondLine, mask), _mm_srli_epi32(secondLine, 16));

        // Vertical addition and average computation
        // average = (sum + 2) >> 2
        const auto sum = _mm_add_epi32(_mm_add_epi32(horizAddFirstLine, horizAddSecondLine), two);
        averageLow = _mm_srli_epi32(sum, 2);
        }
        // Load 8 UInt16 from each line
        __m128i averageHigh;
        {
        const auto firstLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted + 8));
        const auto secondLine = _mm_loadu_si128(reinterpret_cast<__m128i const*>(pSrcScanlineShifted + 8 + nChunkXSize));

        // Horizontal addition and extension to 32 bit
        const auto horizAddFirstLine = _mm_add_epi32(_mm_and_si128(firstLine, mask), _mm_srli_epi32(firstLine, 16));
        const auto horizAddSecondLine = _mm_add_epi32(_mm_and_si128(secondLine, mask), _mm_srli_epi32(secondLine, 16));

        // Vertical addition and average computation
        // average = (sum + 2) >> 2
        const auto sum = _mm_add_epi32(_mm_add_epi32(horizAddFirstLine, horizAddSecondLine), two);
        averageHigh = _mm_srli_epi32(sum, 2);
        }

        // Pack each 32 bit average value to 16 bits
        auto average = sse2_packus_epi32(averageLow, averageHigh);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&pDstScanline[iDstPixel]), average);
        pSrcScanlineShifted += 16;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                      QuadraticMeanFloatSSE2()                        */
/************************************************************************/

inline __m128 SQUARE(__m128 x)
{
    return _mm_mul_ps(x, x);
}

template<class T> static int QuadraticMeanFloatSSE2(int nDstXWidth,
                                                    int nChunkXSize,
                                                    const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                                    T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on Float32 by
    // processing by group of 4 output pixels.
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zeroDot25 = _mm_set1_ps(0.25f);

    for( ; iDstPixel < nDstXWidth - 3; iDstPixel += 4 )
    {
        // Load 8 Float32 from each line, and take their square
        const auto firstLineLo = SQUARE(_mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted)));
        const auto firstLineHi = SQUARE(_mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + 4)));
        const auto secondLineLo = SQUARE(_mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + nChunkXSize)));
        const auto secondLineHi = SQUARE(_mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + 4 + nChunkXSize)));

        // Vertical addition
        const auto sumLo = _mm_add_ps(firstLineLo, secondLineLo);
        const auto sumHi = _mm_add_ps(firstLineHi, secondLineHi);

        // Horizontal addition
        const auto A = _mm_shuffle_ps(sumLo, sumHi, 0 | (2 << 2) | (0 << 4) | (2 << 6));
        const auto B = _mm_shuffle_ps(sumLo, sumHi, 1 | (3 << 2) | (1 << 4) | (3 << 6));
        const auto sumSquares = _mm_add_ps(A, B);

        const auto rms = _mm_sqrt_ps(_mm_mul_ps(sumSquares, zeroDot25));

        // coverity[incompatible_cast]
        _mm_storeu_ps(reinterpret_cast<float*>(&pDstScanline[iDstPixel]), rms);
        pSrcScanlineShifted += 8;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                        AverageFloatSSE2()                            */
/************************************************************************/

template<class T> static int AverageFloatSSE2(int nDstXWidth,
                                              int nChunkXSize,
                                              const T*& CPL_RESTRICT pSrcScanlineShiftedInOut,
                                              T* CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on Float32 by
    // processing by group of 4 output pixels.
    const T* CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zeroDot25 = _mm_set1_ps(0.25f);

    for( ; iDstPixel < nDstXWidth - 3; iDstPixel += 4 )
    {
        // Load 8 Float32 from each line
        const auto firstLineLo = _mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted));
        const auto firstLineHi = _mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + 4));
        const auto secondLineLo = _mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + nChunkXSize));
        const auto secondLineHi = _mm_loadu_ps(reinterpret_cast<float const*>(pSrcScanlineShifted + 4 + nChunkXSize));

        // Vertical addition
        const auto sumLo = _mm_add_ps(firstLineLo, secondLineLo);
        const auto sumHi = _mm_add_ps(firstLineHi, secondLineHi);

        // Horizontal addition
        const auto A = _mm_shuffle_ps(sumLo, sumHi, 0 | (2 << 2) | (0 << 4) | (2 << 6));
        const auto B = _mm_shuffle_ps(sumLo, sumHi, 1 | (3 << 2) | (1 << 4) | (3 << 6));
        const auto sum = _mm_add_ps(A, B);

        const auto average = _mm_mul_ps(sum, zeroDot25);

        // coverity[incompatible_cast]
        _mm_storeu_ps(reinterpret_cast<float*>(&pDstScanline[iDstPixel]), average);
        pSrcScanlineShifted += 8;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

#endif

/************************************************************************/
/*                    GDALResampleChunk32R_Average()                    */
/************************************************************************/

template <class T, class Tsum, GDALDataType eWrkDataType>
static CPLErr
GDALResampleChunk32R_AverageT( double dfXRatioDstToSrc,
                               double dfYRatioDstToSrc,
                               double dfSrcXDelta,
                               double dfSrcYDelta,
                               const T* pChunk,
                               const GByte * pabyChunkNodataMask,
                               int nChunkXOff, int nChunkXSize,
                               int nChunkYOff, int nChunkYSize,
                               int nDstXOff, int nDstXOff2,
                               int nDstYOff, int nDstYOff2,
                               GDALRasterBand * poOverview,
                               void** ppDstBuffer,
                               const char * pszResampling,
                               int bHasNoData,  // TODO(schwehr): bool.
                               float fNoDataValue,
                               GDALColorTable* poColorTable,
                               bool bPropagateNoData )
{
    // AVERAGE_BIT2GRAYSCALE
    const bool bBit2Grayscale =
        CPL_TO_BOOL( STARTS_WITH_CI( pszResampling, "AVERAGE_BIT2G" ) );
    const bool bQuadraticMean =
        CPL_TO_BOOL( EQUAL( pszResampling, "RMS" ) );
    if( bBit2Grayscale )
        poColorTable = nullptr;

    T tNoDataValue;
    if( !bHasNoData )
        tNoDataValue = 0;
    else
        tNoDataValue = static_cast<T>(fNoDataValue);
    const T tReplacementVal = static_cast<T>(GetReplacementValueIfNoData(
        poOverview->GetRasterDataType(), bHasNoData, fNoDataValue));

    int nChunkRightXOff = nChunkXOff + nChunkXSize;
    int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate buffers.                                               */
/* -------------------------------------------------------------------- */
    *ppDstBuffer = static_cast<T *>(
        VSI_MALLOC3_VERBOSE(nDstXWidth, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(eWrkDataType)));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    T* const pDstBuffer = static_cast<T*>(*ppDstBuffer);

    struct PrecomputedXValue
    {
        int nLeftXOffShifted;
        int nRightXOffShifted;
        double dfLeftWeight;
        double dfRightWeight;
        double dfTotalWeightFullLine;
    };
    PrecomputedXValue* pasSrcX = static_cast<PrecomputedXValue *>(
        VSI_MALLOC_VERBOSE(nDstXWidth * sizeof(PrecomputedXValue) ) );

    if( pasSrcX == nullptr )
    {
        VSIFree(pasSrcX);
        return CE_Failure;
    }

    int nEntryCount = 0;
    GDALColorEntry* aEntries = nullptr;
    int nTransparentIdx = -1;

    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount, aEntries,
                               nTransparentIdx) )
    {
        VSIFree(pasSrcX);
        return CE_Failure;
    }

    // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
    // it as nodata value
    if( bHasNoData && fNoDataValue >= 0.0f && tNoDataValue < nEntryCount )
    {
        if( aEntries == nullptr )
        {
            CPLError(CE_Failure, CPLE_ObjectNull, "No aEntries.");
            VSIFree(pasSrcX);
            return CE_Failure;
        }
        aEntries[static_cast<int>(tNoDataValue)].c4 = 0;
    }
    // Or if we have no explicit nodata, but a color table entry that is
    // transparent, consider it as the nodata value
    else if( !bHasNoData && nTransparentIdx >= 0 )
    {
        bHasNoData = TRUE;
        tNoDataValue = static_cast<T>(nTransparentIdx);
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    bool bSrcXSpacingIsTwo = true;
    int nLastSrcXOff2 = -1;
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
        // Apply some epsilon to avoid numerical precision issues
        int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
        double dfSrcXOff2 = dfSrcXDelta + (iDstPixel+1)* dfXRatioDstToSrc;
        int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));

        if( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;
        if( nSrcXOff2 == nSrcXOff )
            nSrcXOff2 ++;
        if( nSrcXOff2 > nChunkRightXOff )
            nSrcXOff2 = nChunkRightXOff;

        pasSrcX[iDstPixel - nDstXOff].nLeftXOffShifted = nSrcXOff - nChunkXOff;
        pasSrcX[iDstPixel - nDstXOff].nRightXOffShifted = nSrcXOff2 - nChunkXOff;
        pasSrcX[iDstPixel - nDstXOff].dfLeftWeight = ( nSrcXOff2 == nSrcXOff + 1 ) ? 1.0 :  1 - (dfSrcXOff - nSrcXOff);
        pasSrcX[iDstPixel - nDstXOff].dfRightWeight = 1 - (nSrcXOff2 - dfSrcXOff2);
        pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine = pasSrcX[iDstPixel - nDstXOff].dfLeftWeight;
        if( nSrcXOff + 1 < nSrcXOff2 )
        {
            pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine += nSrcXOff2 - nSrcXOff - 2;
            pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine += pasSrcX[iDstPixel - nDstXOff].dfRightWeight;
        }

        if( nSrcXOff2 - nSrcXOff != 2 ||
            (nLastSrcXOff2 >= 0 && nLastSrcXOff2 != nSrcXOff) )
        {
            bSrcXSpacingIsTwo = false;
        }
        nLastSrcXOff2 = nSrcXOff2;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        double dfSrcYOff2 = dfSrcYDelta + (iDstLine+1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
        if( nSrcYOff2 == nSrcYOff )
            ++nSrcYOff2;
        if( nSrcYOff2 > nChunkBottomYOff )
            nSrcYOff2 = nChunkBottomYOff;

        T* const pDstScanline = pDstBuffer + (iDstLine - nDstYOff) * nDstXWidth;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        if( poColorTable == nullptr )
        {
            if( bSrcXSpacingIsTwo && nSrcYOff2 == nSrcYOff + 2 &&
                pabyChunkNodataMask == nullptr )
            {
              if( eWrkDataType == GDT_Byte || eWrkDataType == GDT_UInt16 )
              {
                // Optimized case : no nodata, overview by a factor of 2 and
                // regular x and y src spacing.
                const T* pSrcScanlineShifted =
                    pChunk + pasSrcX[0].nLeftXOffShifted +
                    static_cast<GPtrDiff_t>(nSrcYOff - nChunkYOff) * nChunkXSize;
                int iDstPixel = 0;
#ifdef USE_SSE2
                if( bQuadraticMean && eWrkDataType == GDT_Byte )
                {
                    iDstPixel = QuadraticMeanByteSSE2OrAVX2(nDstXWidth,
                                                            nChunkXSize,
                                                            pSrcScanlineShifted,
                                                            pDstScanline);
                }
                else if( bQuadraticMean /* && eWrkDataType == GDT_UInt16 */ )
                {
                    iDstPixel = QuadraticMeanUInt16SSE2(nDstXWidth,
                                                        nChunkXSize,
                                                        pSrcScanlineShifted,
                                                        pDstScanline);
                }
                else if( /* !bQuadraticMean && */ eWrkDataType == GDT_Byte )
                {
                    iDstPixel = AverageByteSSE2OrAVX2(nDstXWidth,
                                                      nChunkXSize,
                                                      pSrcScanlineShifted,
                                                      pDstScanline);
                }
                else /* if( !bQuadraticMean && eWrkDataType == GDT_UInt16 ) */
                {
                    iDstPixel = AverageUInt16SSE2(nDstXWidth,
                                                  nChunkXSize,
                                                  pSrcScanlineShifted,
                                                  pDstScanline);
                }
#endif
                for( ; iDstPixel < nDstXWidth; ++iDstPixel )
                {
                    Tsum nTotal = 0;
                    T nVal;
                    if( bQuadraticMean )
                        nTotal =
                            SQUARE<Tsum>(pSrcScanlineShifted[0])
                            + SQUARE<Tsum>(pSrcScanlineShifted[1])
                            + SQUARE<Tsum>(pSrcScanlineShifted[nChunkXSize])
                            + SQUARE<Tsum>(pSrcScanlineShifted[1+nChunkXSize]);
                    else
                        nTotal =
                            pSrcScanlineShifted[0]
                            + pSrcScanlineShifted[1]
                            + pSrcScanlineShifted[nChunkXSize]
                            + pSrcScanlineShifted[1+nChunkXSize];

                    constexpr int nTotalWeight = 4;
                    if( bQuadraticMean )
                        nVal = ComputeIntegerRMS_4values<T>(nTotal);
                    else
                        nVal = static_cast<T>((nTotal + nTotalWeight/2) / nTotalWeight);

                    // No need to compare nVal against tNoDataValue as we are
                    // in a case where pabyChunkNodataMask == nullptr implies
                    // the absence of nodata value.
                    pDstScanline[iDstPixel] = nVal;
                    pSrcScanlineShifted += 2;
                }
              }
              else
              {
                CPLAssert( eWrkDataType == GDT_Float32 );
                const T* pSrcScanlineShifted =
                    pChunk + pasSrcX[0].nLeftXOffShifted +
                    static_cast<GPtrDiff_t>(nSrcYOff - nChunkYOff) * nChunkXSize;
                int iDstPixel = 0;
#ifdef USE_SSE2
                if( bQuadraticMean )
                {
                    iDstPixel = QuadraticMeanFloatSSE2(nDstXWidth,
                                                       nChunkXSize,
                                                       pSrcScanlineShifted,
                                                       pDstScanline);
                }
                else
                {
                    iDstPixel = AverageFloatSSE2(nDstXWidth,
                                                 nChunkXSize,
                                                 pSrcScanlineShifted,
                                                 pDstScanline);
                }
#endif

                for( ; iDstPixel < nDstXWidth; ++iDstPixel )
                {
                    float nTotal = 0;
                    T nVal;
                    if( bQuadraticMean )
                        nTotal =
                            SQUARE<float>(pSrcScanlineShifted[0])
                            + SQUARE<float>(pSrcScanlineShifted[1])
                            + SQUARE<float>(pSrcScanlineShifted[nChunkXSize])
                            + SQUARE<float>(pSrcScanlineShifted[1+nChunkXSize]);
                    else
                        nTotal = static_cast<float>(
                            pSrcScanlineShifted[0]
                            + pSrcScanlineShifted[1]
                            + pSrcScanlineShifted[nChunkXSize]
                            + pSrcScanlineShifted[1+nChunkXSize]);

                    if( bQuadraticMean )
                        nVal = static_cast<T>(std::sqrt(nTotal * 0.25f));
                    else
                        nVal = static_cast<T>(nTotal * 0.25f);

                    // No need to compare nVal against tNoDataValue as we are
                    // in a case where pabyChunkNodataMask == nullptr implies
                    // the absence of nodata value.
                    pDstScanline[iDstPixel] = nVal;
                    pSrcScanlineShifted += 2;
                }
              }
            }
            else
            {
                const double dfBottomWeight =
                    (nSrcYOff + 1 == nSrcYOff2) ? 1.0 : 1.0 - (dfSrcYOff - nSrcYOff);
                const double dfTopWeight = 1.0 - (nSrcYOff2 - dfSrcYOff2);
                nSrcYOff -= nChunkYOff;
                nSrcYOff2 -= nChunkYOff;

                double dfTotalWeightFullColumn = dfBottomWeight;
                if( nSrcYOff + 1 < nSrcYOff2 )
                {
                    dfTotalWeightFullColumn += nSrcYOff2 - nSrcYOff - 2;
                    dfTotalWeightFullColumn += dfTopWeight;
                }

                for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
                {
                    const int nSrcXOff = pasSrcX[iDstPixel].nLeftXOffShifted;
                    const int nSrcXOff2 = pasSrcX[iDstPixel].nRightXOffShifted;

                    double dfTotal = 0;
                    double dfTotalWeight = 0;
                    if( pabyChunkNodataMask == nullptr )
                    {
                        auto pChunkShifted = pChunk +
                                static_cast<GPtrDiff_t>(nSrcYOff) *nChunkXSize;
                        int nCounterY = nSrcYOff2 - nSrcYOff - 1;
                        double dfWeightY = dfBottomWeight;
                        while( true )
                        {
                            double dfTotalLine;
                            if( bQuadraticMean )
                            {
                                // Left pixel
                                {
                                    const T val = pChunkShifted[nSrcXOff];
                                    dfTotalLine = SQUARE<double>(val) * pasSrcX[iDstPixel].dfLeftWeight;
                                }

                                if( nSrcXOff + 1 < nSrcXOff2 )
                                {
                                    // Middle pixels
                                    for( int iX = nSrcXOff + 1; iX + 1 < nSrcXOff2; ++iX )
                                    {
                                        const T val = pChunkShifted[iX];
                                        dfTotalLine += SQUARE<double>(val);
                                    }

                                    // Right pixel
                                    {
                                        const T val = pChunkShifted[nSrcXOff2 - 1];
                                        dfTotalLine += SQUARE<double>(val) * pasSrcX[iDstPixel].dfRightWeight;
                                    }
                                }
                            }
                            else
                            {
                                // Left pixel
                                {
                                    const T val = pChunkShifted[nSrcXOff];
                                    dfTotalLine = val * pasSrcX[iDstPixel].dfLeftWeight;
                                }

                                if( nSrcXOff + 1 < nSrcXOff2 )
                                {
                                    // Middle pixels
                                    for( int iX = nSrcXOff + 1; iX + 1 < nSrcXOff2; ++iX )
                                    {
                                        const T val = pChunkShifted[iX];
                                        dfTotalLine += val;
                                    }

                                    // Right pixel
                                    {
                                        const T val = pChunkShifted[nSrcXOff2 - 1];
                                        dfTotalLine += val * pasSrcX[iDstPixel].dfRightWeight;
                                    }
                                }
                            }

                            dfTotal += dfTotalLine * dfWeightY;
                            --nCounterY;
                            if( nCounterY < 0 )
                                break;
                            pChunkShifted += nChunkXSize;
                            dfWeightY = (nCounterY == 0) ? dfTopWeight : 1.0;
                        }

                        dfTotalWeight = pasSrcX[iDstPixel].dfTotalWeightFullLine * dfTotalWeightFullColumn;
                    }
                    else
                    {
                        GPtrDiff_t nCount = 0;
                        for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                        {
                            const auto pChunkShifted = pChunk +
                                static_cast<GPtrDiff_t>(iY) *nChunkXSize;

                            double dfTotalLine = 0;
                            double dfTotalWeightLine = 0;
                            // Left pixel
                            {
                                const int iX = nSrcXOff;
                                const T val = pChunkShifted[iX];
                                if( pabyChunkNodataMask[iX + iY *nChunkXSize] )
                                {
                                    nCount ++;
                                    const double dfWeightX = pasSrcX[iDstPixel].dfLeftWeight;
                                    dfTotalWeightLine = dfWeightX;
                                    if( bQuadraticMean )
                                        dfTotalLine = SQUARE<double>(val) * dfWeightX;
                                    else
                                        dfTotalLine = val * dfWeightX;
                                }
                            }

                            if( nSrcXOff + 1 < nSrcXOff2 )
                            {
                                // Middle pixels
                                for( int iX = nSrcXOff + 1; iX + 1 < nSrcXOff2; ++iX )
                                {
                                    const T val = pChunkShifted[iX];
                                    if( pabyChunkNodataMask[iX + iY *nChunkXSize] )
                                    {
                                        nCount ++;
                                        dfTotalWeightLine += 1;
                                        if( bQuadraticMean )
                                            dfTotalLine += SQUARE<double>(val);
                                        else
                                            dfTotalLine += val;
                                    }
                                }

                                // Right pixel
                                {
                                    const int iX = nSrcXOff2 - 1;
                                    const T val = pChunkShifted[iX];
                                    if( pabyChunkNodataMask[iX + iY *nChunkXSize] )
                                    {
                                        nCount ++;
                                        const double dfWeightX = pasSrcX[iDstPixel].dfRightWeight;
                                        dfTotalWeightLine += dfWeightX;
                                        if( bQuadraticMean )
                                            dfTotalLine += SQUARE<double>(val) * dfWeightX;
                                        else
                                            dfTotalLine += val * dfWeightX;
                                    }
                                }
                            }

                            const double dfWeightY =
                                (iY == nSrcYOff) ? dfBottomWeight :
                                (iY + 1 == nSrcYOff2) ? dfTopWeight :
                                1.0;
                            dfTotal += dfTotalLine * dfWeightY;
                            dfTotalWeight += dfTotalWeightLine * dfWeightY;
                        }

                        if( nCount == 0 ||
                            (bPropagateNoData && nCount <
                                static_cast<GPtrDiff_t>(nSrcYOff2 - nSrcYOff) * (nSrcXOff2 - nSrcXOff)))
                        {
                            pDstScanline[iDstPixel] = tNoDataValue;
                            continue;
                        }
                    }
                    if( eWrkDataType == GDT_Byte ||
                             eWrkDataType == GDT_UInt16)
                    {
                        T nVal;
                        if( bQuadraticMean )
                            nVal = ComputeIntegerRMS<T>(dfTotal, dfTotalWeight);
                        else
                            nVal = static_cast<T>(dfTotal / dfTotalWeight + 0.5);
                        if( bHasNoData && nVal == tNoDataValue )
                            nVal = tReplacementVal;
                        pDstScanline[iDstPixel] = nVal;
                    }
                    else
                    {
                        T nVal;
                        if( bQuadraticMean )
                            nVal = static_cast<T>(sqrt(dfTotal / dfTotalWeight));
                        else
                            nVal = static_cast<T>(dfTotal / dfTotalWeight);
                        if( bHasNoData && nVal == tNoDataValue )
                            nVal = tReplacementVal;
                        pDstScanline[iDstPixel] = nVal;
                    }
                }
            }
        }
        else
        {
            nSrcYOff -= nChunkYOff;
            nSrcYOff2 -= nChunkYOff;

            for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
            {
                const int nSrcXOff = pasSrcX[iDstPixel].nLeftXOffShifted;
                const int nSrcXOff2 = pasSrcX[iDstPixel].nRightXOffShifted;

                GPtrDiff_t nTotalR = 0;
                GPtrDiff_t nTotalG = 0;
                GPtrDiff_t nTotalB = 0;
                GPtrDiff_t nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const T val = pChunk[iX + static_cast<GPtrDiff_t>(iY) *nChunkXSize];
                        const unsigned nVal = static_cast<unsigned>(val);
                        if( nVal < static_cast<unsigned>(nEntryCount) && aEntries[nVal].c4 )
                        {
                            if( bQuadraticMean )
                            {
                                nTotalR += SQUARE<int>(aEntries[nVal].c1);
                                nTotalG += SQUARE<int>(aEntries[nVal].c2);
                                nTotalB += SQUARE<int>(aEntries[nVal].c3);
                                ++nCount;
                            }
                            else
                            {
                                nTotalR += aEntries[nVal].c1;
                                nTotalG += aEntries[nVal].c2;
                                nTotalB += aEntries[nVal].c3;
                                ++nCount;
                            }
                        }
                    }
                }

                if( nCount == 0 ||
                    (bPropagateNoData && nCount <
                        static_cast<GPtrDiff_t>(nSrcYOff2 - nSrcYOff) * (nSrcXOff2 - nSrcXOff)) )
                {
                    pDstScanline[iDstPixel] = tNoDataValue;
                }
                else
                {
                    int nR, nG, nB;
                    if( bQuadraticMean )
                    {
                        nR = static_cast<int>(sqrt(nTotalR / nCount) + 0.5);
                        nG = static_cast<int>(sqrt(nTotalG / nCount) + 0.5);
                        nB = static_cast<int>(sqrt(nTotalB / nCount) + 0.5);
                    }
                    else
                    {
                        nR = static_cast<int>((nTotalR + nCount / 2) / nCount);
                        nG = static_cast<int>((nTotalG + nCount / 2) / nCount);
                        nB = static_cast<int>((nTotalB + nCount / 2) / nCount);
                    }
                    pDstScanline[iDstPixel] = static_cast<T>(GDALFindBestEntry(
                        nEntryCount, aEntries, nR, nG, nB));
                }
            }
        }
    }

    CPLFree( aEntries );
    CPLFree( pasSrcX );

    return CE_None;
}

static CPLErr
GDALResampleChunk32R_Average( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                              double dfSrcXDelta,
                              double dfSrcYDelta,
                              GDALDataType eWrkDataType,
                              const void * pChunk,
                              const GByte * pabyChunkNodataMask,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, int nChunkYSize,
                              int nDstXOff, int nDstXOff2,
                              int nDstYOff, int nDstYOff2,
                              GDALRasterBand * poOverview,
                              void** ppDstBuffer,
                              GDALDataType* peDstBufferDataType,
                              const char * pszResampling,
                              int bHasNoData, float fNoDataValue,
                              GDALColorTable* poColorTable,
                              GDALDataType /* eSrcDataType */,
                              bool bPropagateNoData )
{
    if( eWrkDataType == GDT_Byte )
    {
        *peDstBufferDataType = eWrkDataType;
        return GDALResampleChunk32R_AverageT<GByte, int, GDT_Byte>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<const GByte *>( pChunk ),
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            ppDstBuffer,
            pszResampling,
            bHasNoData, fNoDataValue,
            poColorTable,
            bPropagateNoData );
    }
    else if( eWrkDataType == GDT_UInt16  )
    {
        *peDstBufferDataType = eWrkDataType;
        if( EQUAL(pszResampling, "RMS") )
        {
            // Use double as accumulation type, because UInt32 could overflow
            return GDALResampleChunk32R_AverageT<GUInt16, double, GDT_UInt16>(
                dfXRatioDstToSrc, dfYRatioDstToSrc,
                dfSrcXDelta, dfSrcYDelta,
                static_cast<const GUInt16 *>( pChunk ),
                pabyChunkNodataMask,
                nChunkXOff, nChunkXSize,
                nChunkYOff, nChunkYSize,
                nDstXOff, nDstXOff2,
                nDstYOff, nDstYOff2,
                poOverview,
                ppDstBuffer,
                pszResampling,
                bHasNoData, fNoDataValue,
                poColorTable,
                bPropagateNoData );
        }
        else
        {
            return GDALResampleChunk32R_AverageT<GUInt16, GUInt32, GDT_UInt16>(
                dfXRatioDstToSrc, dfYRatioDstToSrc,
                dfSrcXDelta, dfSrcYDelta,
                static_cast<const GUInt16 *>( pChunk ),
                pabyChunkNodataMask,
                nChunkXOff, nChunkXSize,
                nChunkYOff, nChunkYSize,
                nDstXOff, nDstXOff2,
                nDstYOff, nDstYOff2,
                poOverview,
                ppDstBuffer,
                pszResampling,
                bHasNoData, fNoDataValue,
                poColorTable,
                bPropagateNoData );
        }
    }
    else if( eWrkDataType == GDT_Float32 )
    {
        *peDstBufferDataType = eWrkDataType;
        return GDALResampleChunk32R_AverageT<float, double, GDT_Float32>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<const float *>( pChunk ),
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            ppDstBuffer,
            pszResampling,
            bHasNoData, fNoDataValue,
            poColorTable,
            bPropagateNoData );
    }

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                    GDALResampleChunk32R_Gauss()                      */
/************************************************************************/

static CPLErr
GDALResampleChunk32R_Gauss( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                            double /* dfSrcXDelta */,
                            double /* dfSrcYDelta */,
                            GDALDataType /* eWrkDataType */,
                            const void * pChunk,
                            const GByte * pabyChunkNodataMask,
                            int nChunkXOff, int nChunkXSize,
                            int nChunkYOff, int nChunkYSize,
                            int nDstXOff, int nDstXOff2,
                            int nDstYOff, int nDstYOff2,
                            GDALRasterBand * poOverview,
                            void** ppDstBuffer,
                            GDALDataType* peDstBufferDataType,
                            const char * /* pszResampling */,
                            int bHasNoData, float fNoDataValue,
                            GDALColorTable* poColorTable,
                            GDALDataType /* eSrcDataType */,
                            bool /* bPropagateNoData */ )

{
    const float * const pafChunk = static_cast<const float *>( pChunk );

    *ppDstBuffer =
        VSI_MALLOC3_VERBOSE(nDstXOff2 - nDstXOff, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(GDT_Float32));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    *peDstBufferDataType = GDT_Float32;
    float* const pafDstBuffer = static_cast<float*>(*ppDstBuffer);

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    int nGaussMatrixDim = 3;
    const int *panGaussMatrix;
    constexpr int anGaussMatrix3x3[] ={
        1, 2, 1,
        2, 4, 2,
        1, 2, 1
    };
    constexpr int anGaussMatrix5x5[] = {
        1, 4, 6, 4, 1,
        4, 16, 24, 16, 4,
        6, 24, 36, 24, 6,
        4, 16, 24, 16, 4,
        1, 4, 6, 4, 1};
    constexpr int anGaussMatrix7x7[] = {
        1, 6, 15, 20, 15, 6, 1,
        6, 36, 90, 120, 90, 36, 6,
        15, 90, 225, 300, 225, 90, 15,
        20, 120, 300, 400, 300, 120, 20,
        15, 90, 225, 300, 225, 90, 15,
        6, 36, 90, 120, 90, 36, 6,
        1, 6, 15, 20, 15, 6, 1};

    const int nOXSize = poOverview->GetXSize();
    const int nOYSize = poOverview->GetYSize();
    const int nResYFactor = static_cast<int>(0.5 + dfYRatioDstToSrc);

    // matrix for gauss filter
    if(nResYFactor <= 2 )
    {
        panGaussMatrix = anGaussMatrix3x3;
        nGaussMatrixDim=3;
    }
    else if( nResYFactor <= 4 )
    {
        panGaussMatrix = anGaussMatrix5x5;
        nGaussMatrixDim=5;
    }
    else
    {
        panGaussMatrix = anGaussMatrix7x7;
        nGaussMatrixDim=7;
    }

#ifdef DEBUG_OUT_OF_BOUND_ACCESS
    int* panGaussMatrixDup = static_cast<int*>(
        CPLMalloc(sizeof(int) * nGaussMatrixDim * nGaussMatrixDim));
    memcpy(panGaussMatrixDup, panGaussMatrix,
           sizeof(int) * nGaussMatrixDim * nGaussMatrixDim);
    panGaussMatrix = panGaussMatrixDup;
#endif

    if( !bHasNoData )
        fNoDataValue = 0.0f;

    int nEntryCount = 0;
    GDALColorEntry* aEntries = nullptr;
    int nTransparentIdx = -1;
    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount, aEntries,
                               nTransparentIdx) )
    {
        return CE_Failure;
    }

    // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
    // it as nodata value.
    if( bHasNoData && fNoDataValue >= 0.0f && fNoDataValue < nEntryCount )
    {
        if( aEntries == nullptr )
        {
            CPLError(CE_Failure, CPLE_ObjectNull, "No aEntries");
            return CE_Failure;
        }
        aEntries[static_cast<int>(fNoDataValue)].c4 = 0;
    }
    // Or if we have no explicit nodata, but a color table entry that is
    // transparent, consider it as the nodata value.
    else if( !bHasNoData && nTransparentIdx >= 0 )
    {
        fNoDataValue = static_cast<float>(nTransparentIdx);
    }

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    const int nDstXWidth = nDstXOff2 - nDstXOff;

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        int nSrcYOff2 =
            static_cast<int>(0.5 + (iDstLine+1) * dfYRatioDstToSrc) + 1;

        if( nSrcYOff < nChunkYOff )
        {
            nSrcYOff = nChunkYOff;
            nSrcYOff2++;
        }

        const int iSizeY = nSrcYOff2 - nSrcYOff;
        nSrcYOff = nSrcYOff + iSizeY/2 - nGaussMatrixDim/2;
        nSrcYOff2 = nSrcYOff + nGaussMatrixDim;

        if( nSrcYOff2 > nChunkBottomYOff ||
            (dfYRatioDstToSrc > 1 && iDstLine == nOYSize-1) )
        {
            nSrcYOff2 = std::min(nChunkBottomYOff,
                                 nSrcYOff + nGaussMatrixDim);
        }

        int nYShiftGaussMatrix = 0;
        if(nSrcYOff < nChunkYOff)
        {
            nYShiftGaussMatrix = -(nSrcYOff - nChunkYOff);
            nSrcYOff = nChunkYOff;
        }

        const float * const pafSrcScanline =
            pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        const GByte *pabySrcScanlineNodataMask = nullptr;
        if( pabyChunkNodataMask != nullptr )
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask + ((nSrcYOff-nChunkYOff) * nChunkXSize);

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        float* const pafDstScanline = pafDstBuffer + (iDstLine - nDstYOff) * nDstXWidth;
        for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
        {
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 =
                static_cast<int>(0.5 + (iDstPixel+1) * dfXRatioDstToSrc) + 1;

            if( nSrcXOff < nChunkXOff )
            {
                nSrcXOff = nChunkXOff;
                nSrcXOff2++;
            }

            const int iSizeX = nSrcXOff2 - nSrcXOff;
            nSrcXOff = nSrcXOff + iSizeX/2 - nGaussMatrixDim/2;
            nSrcXOff2 = nSrcXOff + nGaussMatrixDim;

            if( nSrcXOff2 > nChunkRightXOff ||
                (dfXRatioDstToSrc > 1 && iDstPixel == nOXSize-1) )
            {
                nSrcXOff2 = std::min(nChunkRightXOff,
                                     nSrcXOff + nGaussMatrixDim);
            }

            int nXShiftGaussMatrix = 0;
            if(nSrcXOff < nChunkXOff)
            {
                nXShiftGaussMatrix = -(nSrcXOff - nChunkXOff);
                nSrcXOff = nChunkXOff;
            }

            if( poColorTable == nullptr )
            {
                double dfTotal = 0.0;
                GInt64 nCount = 0;
                const int *panLineWeight = panGaussMatrix +
                    nYShiftGaussMatrix * nGaussMatrixDim + nXShiftGaussMatrix;

                for( int j=0, iY = nSrcYOff;
                     iY < nSrcYOff2;
                     ++iY, ++j, panLineWeight += nGaussMatrixDim )
                {
                    for( int i=0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i )
                    {
                        const double val =
                            pafSrcScanline[iX-nChunkXOff+static_cast<GPtrDiff_t>(iY-nSrcYOff)
                                           * nChunkXSize];
                        if( pabySrcScanlineNodataMask == nullptr ||
                            pabySrcScanlineNodataMask[iX - nChunkXOff
                                                      +static_cast<GPtrDiff_t>(iY - nSrcYOff)
                                                      * nChunkXSize] )
                        {
                            const int nWeight = panLineWeight[i];
                            dfTotal += val * nWeight;
                            nCount += nWeight;
                        }
                    }
                }

                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>(dfTotal / nCount);
                }
            }
            else
            {
                GInt64 nTotalR = 0;
                GInt64 nTotalG = 0;
                GInt64 nTotalB = 0;
                GInt64 nTotalWeight = 0;
                const int *panLineWeight =
                    panGaussMatrix + nYShiftGaussMatrix * nGaussMatrixDim +
                    nXShiftGaussMatrix;

                for( int j=0, iY = nSrcYOff; iY < nSrcYOff2;
                        ++iY, ++j, panLineWeight += nGaussMatrixDim )
                {
                    for( int i=0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i )
                    {
                        const double val =
                            pafSrcScanline[iX - nChunkXOff +
                                           static_cast<GPtrDiff_t>(iY-nSrcYOff) * nChunkXSize];
                        int nVal = static_cast<int>(val);
                        if( nVal >= 0 && nVal < nEntryCount &&
                            aEntries[nVal].c4 )
                        {
                            const int nWeight = panLineWeight[i];
                            nTotalR += aEntries[nVal].c1 * nWeight;
                            nTotalG += aEntries[nVal].c2 * nWeight;
                            nTotalB += aEntries[nVal].c3 * nWeight;
                            nTotalWeight += nWeight;
                        }
                    }
                }

                if( nTotalWeight == 0 )
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    const int nR =
                        static_cast<int>((nTotalR + nTotalWeight / 2) / nTotalWeight);
                    const int nG =
                        static_cast<int>((nTotalG + nTotalWeight / 2) / nTotalWeight);
                    const int nB =
                        static_cast<int>((nTotalB + nTotalWeight / 2) / nTotalWeight);
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>( GDALFindBestEntry(
                            nEntryCount, aEntries, nR, nG, nB ) );
                }
            }
        }
    }

    CPLFree( aEntries );
#ifdef DEBUG_OUT_OF_BOUND_ACCESS
    CPLFree( panGaussMatrixDup );
#endif

    return CE_None;
}

/************************************************************************/
/*                    GDALResampleChunk32R_Mode()                       */
/************************************************************************/

static CPLErr
GDALResampleChunk32R_Mode( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                           double dfSrcXDelta,
                           double dfSrcYDelta,
                           GDALDataType /* eWrkDataType */,
                           const void * pChunk,
                           const GByte * pabyChunkNodataMask,
                           int nChunkXOff, int nChunkXSize,
                           int nChunkYOff, int nChunkYSize,
                           int nDstXOff, int nDstXOff2,
                           int nDstYOff, int nDstYOff2,
                           GDALRasterBand * /* poOverview */,
                           void** ppDstBuffer,
                           GDALDataType* peDstBufferDataType,
                           const char * /* pszResampling */,
                           int bHasNoData, float fNoDataValue,
                           GDALColorTable* poColorTable,
                           GDALDataType eSrcDataType,
                           bool /* bPropagateNoData */ )

{
    const float * const pafChunk = static_cast<const float *>( pChunk );

    const int nDstXSize = nDstXOff2 - nDstXOff;
    *ppDstBuffer =
        VSI_MALLOC3_VERBOSE(nDstXSize, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(GDT_Float32));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    *peDstBufferDataType = GDT_Float32;
    float* const pafDstBuffer = static_cast<float*>(*ppDstBuffer);

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */

    if( !bHasNoData )
        fNoDataValue = 0.0f;
    int nEntryCount = 0;
    GDALColorEntry* aEntries = nullptr;
    int nTransparentIdx = -1;
    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount,
                               aEntries, nTransparentIdx) )
    {
        return CE_Failure;
    }

    size_t nMaxNumPx = 0;
    float *pafVals = nullptr;
    int *panSums = nullptr;

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    std::vector<int> anVals(256, 0);

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && dfSrcYOff - nSrcYOff > 0.9 &&
            nSrcYOff < nChunkBottomYOff)
            nSrcYOff ++;
#endif
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        double dfSrcYOff2 = dfSrcYDelta + (iDstLine+1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && nSrcYOff2 - dfSrcYOff2 > 0.9 &&
            nSrcYOff2 > nChunkYOff)
            nSrcYOff2 --;
#endif
        if( nSrcYOff2 == nSrcYOff )
            ++nSrcYOff2;
        if( nSrcYOff2 > nChunkBottomYOff )
            nSrcYOff2 = nChunkBottomYOff;

        const float * const pafSrcScanline =
            pafChunk + (static_cast<GPtrDiff_t>(nSrcYOff-nChunkYOff) * nChunkXSize);
        const GByte *pabySrcScanlineNodataMask = nullptr;
        if( pabyChunkNodataMask != nullptr )
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask + static_cast<GPtrDiff_t>(nSrcYOff-nChunkYOff) * nChunkXSize;

        float* const pafDstScanline = pafDstBuffer + (iDstLine - nDstYOff) * nDstXSize;
/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
        {
            double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
            // Apply some epsilon to avoid numerical precision issues
            int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a tiny
            // participation in the resulting pixel
            if( dfXRatioDstToSrc > 1 && dfSrcXOff - nSrcXOff > 0.9 &&
                nSrcXOff < nChunkRightXOff)
                nSrcXOff ++;
#endif
            if( nSrcXOff < nChunkXOff )
                nSrcXOff = nChunkXOff;

            double dfSrcXOff2 = dfSrcXDelta + (iDstPixel+1)* dfXRatioDstToSrc;
            int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a tiny
            // participation in the resulting pixel
            if( dfXRatioDstToSrc > 1 && nSrcXOff2 - dfSrcXOff2 > 0.9 &&
                nSrcXOff2 > nChunkXOff)
                nSrcXOff2 --;
#endif
            if( nSrcXOff2 == nSrcXOff )
                nSrcXOff2 ++;
            if( nSrcXOff2 > nChunkRightXOff )
                nSrcXOff2 = nChunkRightXOff;

            if( eSrcDataType != GDT_Byte || nEntryCount > 256 )
            {
                // Not sure how much sense it makes to run a majority
                // filter on floating point data, but here it is for the sake
                // of compatibility. It won't look right on RGB images by the
                // nature of the filter.

                if( nSrcYOff2 - nSrcYOff <= 0 ||
                    nSrcXOff2 - nSrcXOff <= 0 ||
                    nSrcYOff2 - nSrcYOff > INT_MAX / (nSrcXOff2 - nSrcXOff) ||
                    static_cast<size_t>(nSrcYOff2-nSrcYOff)*
                        static_cast<size_t>(nSrcXOff2-nSrcXOff) >
                            std::numeric_limits<size_t>::max() / sizeof(float) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big downsampling factor");
                    CPLFree( aEntries );
                    CPLFree( pafVals );
                    CPLFree( panSums );
                    return CE_Failure;
                }
                const size_t nNumPx = static_cast<size_t>(nSrcYOff2-nSrcYOff)*
                                      static_cast<size_t>(nSrcXOff2-nSrcXOff);
                size_t iMaxInd = 0;
                size_t iMaxVal = 0;
                bool biMaxValdValid = false;

                if( pafVals == nullptr || nNumPx > nMaxNumPx )
                {
                    float* pafValsNew = static_cast<float *>(
                        VSI_REALLOC_VERBOSE(pafVals, nNumPx * sizeof(float)) );
                    int* panSumsNew = static_cast<int *>(
                        VSI_REALLOC_VERBOSE(panSums, nNumPx * sizeof(int)) );
                    if( pafValsNew != nullptr )
                        pafVals = pafValsNew;
                    if( panSumsNew != nullptr )
                        panSums = panSumsNew;
                    if( pafValsNew == nullptr || panSumsNew == nullptr )
                    {
                        CPLFree( aEntries );
                        CPLFree( pafVals );
                        CPLFree( panSums );
                        return CE_Failure;
                    }
                    nMaxNumPx = nNumPx;
                }

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    const GPtrDiff_t iTotYOff = static_cast<GPtrDiff_t>(iY-nSrcYOff)*nChunkXSize-nChunkXOff;
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        if( pabySrcScanlineNodataMask == nullptr ||
                            pabySrcScanlineNodataMask[iX+iTotYOff] )
                        {
                            const float fVal = pafSrcScanline[iX+iTotYOff];
                            size_t i = 0;  // Used after for.

                            // Check array for existing entry.
                            for( ; i < iMaxInd; ++i )
                                if( pafVals[i] == fVal
                                    && ++panSums[i] > panSums[iMaxVal] )
                                {
                                    iMaxVal = i;
                                    biMaxValdValid = true;
                                    break;
                                }

                            // Add to arr if entry not already there.
                            if( i == iMaxInd )
                            {
                                pafVals[iMaxInd] = fVal;
                                panSums[iMaxInd] = 1;

                                if( !biMaxValdValid )
                                {
                                    iMaxVal = iMaxInd;
                                    biMaxValdValid = true;
                                }

                                ++iMaxInd;
                            }
                        }
                    }
                }

                if( !biMaxValdValid )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] = pafVals[iMaxVal];
            }
            else // if( eSrcDataType == GDT_Byte && nEntryCount < 256 )
            {
                // So we go here for a paletted or non-paletted byte band.
                // The input values are then between 0 and 255.
                int nMaxVal = 0;
                int iMaxInd = -1;

                // The cost of this zeroing might be high. Perhaps we should just
                // use the above generic case, and go to this one if the number
                // of source pixels is large enough
                std::fill(anVals.begin(), anVals.end(), 0);

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    const GPtrDiff_t iTotYOff =
                        static_cast<GPtrDiff_t>(iY - nSrcYOff) * nChunkXSize - nChunkXOff;
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const float val = pafSrcScanline[iX+iTotYOff];
                        if( bHasNoData == FALSE || val != fNoDataValue )
                        {
                            int nVal = static_cast<int>(val);
                            if( ++anVals[nVal] > nMaxVal)
                            {
                                // Sum the density.
                                // Is it the most common value so far?
                                iMaxInd = nVal;
                                nMaxVal = anVals[nVal];
                            }
                        }
                    }
                }

                if( iMaxInd == -1 )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>(iMaxInd);
            }
        }
    }

    CPLFree( aEntries );
    CPLFree( pafVals );
    CPLFree( panSums );

    return CE_None;
}

/************************************************************************/
/*                  GDALResampleConvolutionHorizontal()                 */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionHorizontal(
    const T* pChunk, const double* padfWeights, int nSrcPixelCount )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;  // Used after for.
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        dfVal1 += pChunk[i] * padfWeights[i];
        dfVal1 += pChunk[i+1] * padfWeights[i+1];
        dfVal2 += pChunk[i+2] * padfWeights[i+2];
        dfVal2 += pChunk[i+3] * padfWeights[i+3];
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal1 += pChunk[i] * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template<class T> static inline void GDALResampleConvolutionHorizontalWithMask(
    const T* pChunk, const GByte* pabyMask,
    const double* padfWeights, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum)
{
    dfVal = 0;
    dfWeightSum = 0;
    int i = 0;
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        const double dfWeight0 = padfWeights[i] * pabyMask[i];
        const double dfWeight1 = padfWeights[i+1] * pabyMask[i+1];
        const double dfWeight2 = padfWeights[i+2] * pabyMask[i+2];
        const double dfWeight3 = padfWeights[i+3] * pabyMask[i+3];
        dfVal += pChunk[i] * dfWeight0;
        dfVal += pChunk[i+1] * dfWeight1;
        dfVal += pChunk[i+2] * dfWeight2;
        dfVal += pChunk[i+3] * dfWeight3;
        dfWeightSum += dfWeight0 + dfWeight1 + dfWeight2 + dfWeight3;
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        const double dfWeight = padfWeights[i] * pabyMask[i];
        dfVal += pChunk[i] * dfWeight;
        dfWeightSum += dfWeight;
    }
}

template<class T> static inline void GDALResampleConvolutionHorizontal_3rows(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeights, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    double dfVal5 = 0.0;
    double dfVal6 = 0.0;
    int i = 0;  // Used after for.
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        dfVal1 += pChunkRow1[i] * padfWeights[i];
        dfVal1 += pChunkRow1[i+1] * padfWeights[i+1];
        dfVal2 += pChunkRow1[i+2] * padfWeights[i+2];
        dfVal2 += pChunkRow1[i+3] * padfWeights[i+3];
        dfVal3 += pChunkRow2[i] * padfWeights[i];
        dfVal3 += pChunkRow2[i+1] * padfWeights[i+1];
        dfVal4 += pChunkRow2[i+2] * padfWeights[i+2];
        dfVal4 += pChunkRow2[i+3] * padfWeights[i+3];
        dfVal5 += pChunkRow3[i] * padfWeights[i];
        dfVal5 += pChunkRow3[i+1] * padfWeights[i+1];
        dfVal6 += pChunkRow3[i+2] * padfWeights[i+2];
        dfVal6 += pChunkRow3[i+3] * padfWeights[i+3];
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal1 += pChunkRow1[i] * padfWeights[i];
        dfVal3 += pChunkRow2[i] * padfWeights[i];
        dfVal5 += pChunkRow3[i] * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
    dfRes3 = dfVal5 + dfVal6;
}

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeights, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeights, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCount4_3rows(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeights,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeights, 4,
        dfRes1, dfRes2, dfRes3 );
}

/************************************************************************/
/*                  GDALResampleConvolutionVertical()                   */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionVertical(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;
    int j = 0;
    for( ; i + 3 < nSrcLineCount; i+=4, j+=4*nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal1 += pChunk[j + nStride] * padfWeights[i+1];
        dfVal2 += pChunk[j + 2 * nStride] * padfWeights[i+2];
        dfVal2 += pChunk[j + 3 * nStride] * padfWeights[i+3];
    }
    for( ; i < nSrcLineCount; ++i, j += nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template<class T> static inline void GDALResampleConvolutionVertical_2cols(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount,
    double& dfRes1, double& dfRes2 )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    int i = 0;
    int j = 0;
    for(;i+3<nSrcLineCount;i+=4, j+=4*nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal3 += pChunk[j+1] * padfWeights[i];
        dfVal1 += pChunk[j + nStride] * padfWeights[i+1];
        dfVal3 += pChunk[j+1 + nStride] * padfWeights[i+1];
        dfVal2 += pChunk[j + 2 * nStride] * padfWeights[i+2];
        dfVal4 += pChunk[j+1 + 2 * nStride] * padfWeights[i+2];
        dfVal2 += pChunk[j + 3 * nStride] * padfWeights[i+3];
        dfVal4 += pChunk[j+1 + 3 * nStride] * padfWeights[i+3];
    }
    for( ; i < nSrcLineCount; ++i, j += nStride )
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal3 += pChunk[j+1] * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
}

#ifdef USE_SSE2

#ifdef __AVX__
/************************************************************************/
/*             GDALResampleConvolutionVertical_16cols<T>                */
/************************************************************************/

template<class T> static inline void GDALResampleConvolutionVertical_16cols(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount,
    float* afDest )
{
    int i = 0;
    int j = 0;
    XMMReg4Double v_acc0 = XMMReg4Double::Zero();
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    XMMReg4Double v_acc3 = XMMReg4Double::Zero();
    for(;i+3<nSrcLineCount;i+=4, j+=4*nStride)
    {
        XMMReg4Double w0 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+0);
        XMMReg4Double w1 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+1);
        XMMReg4Double w2 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+2);
        XMMReg4Double w3 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+3);
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+ 0+0*nStride) * w0;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+ 4+0*nStride) * w0;
        v_acc2 += XMMReg4Double::Load4Val(pChunk+j+ 8+0*nStride) * w0;
        v_acc3 += XMMReg4Double::Load4Val(pChunk+j+12+0*nStride) * w0;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+ 0+1*nStride) * w1;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+ 4+1*nStride) * w1;
        v_acc2 += XMMReg4Double::Load4Val(pChunk+j+ 8+1*nStride) * w1;
        v_acc3 += XMMReg4Double::Load4Val(pChunk+j+12+1*nStride) * w1;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+ 0+2*nStride) * w2;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+ 4+2*nStride) * w2;
        v_acc2 += XMMReg4Double::Load4Val(pChunk+j+ 8+2*nStride) * w2;
        v_acc3 += XMMReg4Double::Load4Val(pChunk+j+12+2*nStride) * w2;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+ 0+3*nStride) * w3;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+ 4+3*nStride) * w3;
        v_acc2 += XMMReg4Double::Load4Val(pChunk+j+ 8+3*nStride) * w3;
        v_acc3 += XMMReg4Double::Load4Val(pChunk+j+12+3*nStride) * w3;
    }
    for( ; i < nSrcLineCount; ++i, j += nStride )
    {
        XMMReg4Double w = XMMReg4Double::Load1ValHighAndLow(padfWeights+i);
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+ 0) * w;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+ 4) * w;
        v_acc2 += XMMReg4Double::Load4Val(pChunk+j+ 8) * w;
        v_acc3 += XMMReg4Double::Load4Val(pChunk+j+12) * w;
    }
    v_acc0.Store4Val(afDest);
    v_acc1.Store4Val(afDest+4);
    v_acc2.Store4Val(afDest+8);
    v_acc3.Store4Val(afDest+12);
}

#else


/************************************************************************/
/*              GDALResampleConvolutionVertical_8cols<T>                */
/************************************************************************/

template<class T> static inline void GDALResampleConvolutionVertical_8cols(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount,
    float* afDest )
{
    int i = 0;
    int j = 0;
    XMMReg4Double v_acc0 = XMMReg4Double::Zero();
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    for(;i+3<nSrcLineCount;i+=4, j+=4*nStride)
    {
        XMMReg4Double w0 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+0);
        XMMReg4Double w1 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+1);
        XMMReg4Double w2 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+2);
        XMMReg4Double w3 = XMMReg4Double::Load1ValHighAndLow(padfWeights+i+3);
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+0+0*nStride) * w0;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+4+0*nStride) * w0;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+0+1*nStride) * w1;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+4+1*nStride) * w1;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+0+2*nStride) * w2;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+4+2*nStride) * w2;
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+0+3*nStride) * w3;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+4+3*nStride) * w3;
    }
    for( ; i < nSrcLineCount; ++i, j += nStride )
    {
        XMMReg4Double w = XMMReg4Double::Load1ValHighAndLow(padfWeights+i);
        v_acc0 += XMMReg4Double::Load4Val(pChunk+j+0) * w;
        v_acc1 += XMMReg4Double::Load4Val(pChunk+j+4) * w;
    }
    v_acc0.Store4Val(afDest);
    v_acc1.Store4Val(afDest+4);
}

#endif // __AVX__

/************************************************************************/
/*              GDALResampleConvolutionHorizontalSSE2<T>                */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionHorizontalSSE2(
    const T* pChunk, const double* padfWeightsAligned, int nSrcPixelCount )
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    int i = 0;  // Used after for.
    for( ; i + 7 < nSrcPixelCount; i += 8 )
    {
        // Retrieve the pixel & accumulate
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunk+i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunk+i+4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i+4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;
    }

    v_acc1 += v_acc2;

    double dfVal = v_acc1.GetHorizSum();
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal += pChunk[i] * padfWeightsAligned[i];
    }
    return dfVal;
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal<GByte>                */
/************************************************************************/

template<> inline double GDALResampleConvolutionHorizontal<GByte>(
    const GByte* pChunk, const double* padfWeightsAligned, int nSrcPixelCount )
{
    return GDALResampleConvolutionHorizontalSSE2( pChunk, padfWeightsAligned,
                                                  nSrcPixelCount );
}

template<> inline double GDALResampleConvolutionHorizontal<GUInt16>(
    const GUInt16* pChunk, const double* padfWeightsAligned,
    int nSrcPixelCount )
{
    return GDALResampleConvolutionHorizontalSSE2( pChunk, padfWeightsAligned,
                                                  nSrcPixelCount) ;
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontalWithMaskSSE2<T>        */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontalWithMaskSSE2(
    const T* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum )
{
    int i = 0;  // Used after for.
    XMMReg4Double v_acc = XMMReg4Double::Zero();
    XMMReg4Double v_acc_weight = XMMReg4Double::Zero();
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        const XMMReg4Double v_pixels = XMMReg4Double::Load4Val(pChunk+i);
        const XMMReg4Double v_mask = XMMReg4Double::Load4Val(pabyMask+i);
        XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        v_weight *= v_mask;
        v_acc += v_pixels * v_weight;
        v_acc_weight += v_weight;
    }

    dfVal = v_acc.GetHorizSum();
    dfWeightSum = v_acc_weight.GetHorizSum();
    for( ; i < nSrcPixelCount; ++i )
    {
        const double dfWeight = padfWeightsAligned[i] * pabyMask[i];
        dfVal += pChunk[i] * dfWeight;
        dfWeightSum += dfWeight;
    }
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontalWithMask<GByte>        */
/************************************************************************/

template<> inline void GDALResampleConvolutionHorizontalWithMask<GByte>(
    const GByte* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum)
{
    GDALResampleConvolutionHorizontalWithMaskSSE2(pChunk, pabyMask,
                                                  padfWeightsAligned,
                                                  nSrcPixelCount,
                                                  dfVal, dfWeightSum);
}

template<> inline void GDALResampleConvolutionHorizontalWithMask<GUInt16>(
    const GUInt16* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum )
{
    GDALResampleConvolutionHorizontalWithMaskSSE2( pChunk, pabyMask,
                                                   padfWeightsAligned,
                                                   nSrcPixelCount,
                                                   dfVal, dfWeightSum );
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal_3rows_SSE2<T>         */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontal_3rows_SSE2(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero(),
                  v_acc2 = XMMReg4Double::Zero(),
                  v_acc3 = XMMReg4Double::Zero();
    int i = 0;
    for( ; i + 7 < nSrcPixelCount; i += 8 )
    {
        // Retrieve the pixel & accumulate.
        XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1+i);
        XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow1+i+4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i+4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc1 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow2+i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2+i+4);
        v_acc2 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow3+i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow3+i+4);
        v_acc3 += v_pixels1 * v_weight1;
        v_acc3 += v_pixels2 * v_weight2;
    }

    dfRes1 = v_acc1.GetHorizSum();
    dfRes2 = v_acc2.GetHorizSum();
    dfRes3 = v_acc3.GetHorizSum();
    for( ; i < nSrcPixelCount; ++i )
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal_3rows<GByte>          */
/************************************************************************/

template<> inline void GDALResampleConvolutionHorizontal_3rows<GByte>(
    const GByte* pChunkRow1, const GByte* pChunkRow2, const GByte* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

template<> inline void GDALResampleConvolutionHorizontal_3rows<GUInt16>(
    const GUInt16* pChunkRow1, const GUInt16* pChunkRow2,
    const GUInt16* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

/************************************************************************/
/*     GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2<T>   */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3)
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    XMMReg4Double v_acc3 = XMMReg4Double::Zero();
    int i = 0;  // Use after for.
    for( ; i + 3 < nSrcPixelCount; i += 4)
    {
        // Retrieve the pixel & accumulate.
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1+i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2+i);
        const XMMReg4Double v_pixels3 = XMMReg4Double::Load4Val(pChunkRow3+i);
        const XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);

        v_acc1 += v_pixels1 * v_weight;
        v_acc2 += v_pixels2 * v_weight;
        v_acc3 += v_pixels3 * v_weight;
    }

    dfRes1 = v_acc1.GetHorizSum();
    dfRes2 = v_acc2.GetHorizSum();
    dfRes3 = v_acc3.GetHorizSum();

    for( ; i < nSrcPixelCount; ++i )
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*     GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>    */
/************************************************************************/

template<> inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>(
    const GByte* pChunkRow1, const GByte* pChunkRow2, const GByte* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

template<> inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GUInt16>(
    const GUInt16* pChunkRow1, const GUInt16* pChunkRow2,
    const GUInt16* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

/************************************************************************/
/*     GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2<T>       */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeightsAligned,
    double& dfRes1, double& dfRes2, double& dfRes3)
{
    const XMMReg4Double v_weight =
        XMMReg4Double::Load4ValAligned(padfWeightsAligned);

    // Retrieve the pixel & accumulate.
    const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1);
    const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2);
    const XMMReg4Double v_pixels3 = XMMReg4Double::Load4Val(pChunkRow3);

    XMMReg4Double v_acc1 = v_pixels1 * v_weight;
    XMMReg4Double v_acc2 = v_pixels2 * v_weight;
    XMMReg4Double v_acc3 = v_pixels3 * v_weight;

    dfRes1 = v_acc1.GetHorizSum();
    dfRes2 = v_acc2.GetHorizSum();
    dfRes3 = v_acc3.GetHorizSum();
}

/************************************************************************/
/*       GDALResampleConvolutionHorizontalPixelCount4_3rows<GByte>      */
/************************************************************************/

template<> inline void
GDALResampleConvolutionHorizontalPixelCount4_3rows<GByte>(
    const GByte* pChunkRow1, const GByte* pChunkRow2, const GByte* pChunkRow3,
    const double* padfWeightsAligned,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned,
        dfRes1, dfRes2, dfRes3 );
}

template<> inline void
GDALResampleConvolutionHorizontalPixelCount4_3rows<GUInt16>(
    const GUInt16* pChunkRow1, const GUInt16* pChunkRow2,
    const GUInt16* pChunkRow3,
    const double* padfWeightsAligned,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned,
        dfRes1, dfRes2, dfRes3 );
}

#endif  // USE_SSE2

/************************************************************************/
/*                   GDALResampleChunk32R_Convolution()                 */
/************************************************************************/

template<class T> static CPLErr
GDALResampleChunk32R_ConvolutionT( double dfXRatioDstToSrc,
                                   double dfYRatioDstToSrc,
                                   double dfSrcXDelta,
                                   double dfSrcYDelta,
                                   const T * pChunk, int nBands,
                                   const GByte * pabyChunkNodataMask,
                                   int nChunkXOff, int nChunkXSize,
                                   int nChunkYOff, int nChunkYSize,
                                   int nDstXOff, int nDstXOff2,
                                   int nDstYOff, int nDstYOff2,
                                   GDALRasterBand * poDstBand,
                                   void* pDstBuffer,
                                   int bHasNoData,
                                   float fNoDataValue,
                                   FilterFuncType pfnFilterFunc,
                                   FilterFunc4ValuesType pfnFilterFunc4Values,
                                   int nKernelRadius,
                                   bool bKernelWithNegativeWeights,
                                   float fMaxVal )

{
    if( !bHasNoData )
        fNoDataValue = 0.0f;
    const auto dstDataType = poDstBand->GetRasterDataType();
    const int nDstDataTypeSize = GDALGetDataTypeSizeBytes(dstDataType);
    const float fReplacementVal = GetReplacementValueIfNoData(
        dstDataType, bHasNoData, fNoDataValue);
    // cppcheck-suppress unreadVariable
    const int isIntegerDT = GDALDataTypeIsInteger(dstDataType);
    const auto nNodataValueInt64 = static_cast<GInt64>(fNoDataValue);

    // TODO: we should have some generic function to do this.
    float fDstMin = -std::numeric_limits<float>::max();
    float fDstMax = std::numeric_limits<float>::max();
    if( dstDataType == GDT_Byte )
    {
        fDstMin = std::numeric_limits<GByte>::min();
        fDstMax = std::numeric_limits<GByte>::max();
    }
    else if( dstDataType == GDT_UInt16 )
    {
        fDstMin = std::numeric_limits<GUInt16>::min();
        fDstMax = std::numeric_limits<GUInt16>::max();
    }
    else if( dstDataType == GDT_Int16 )
    {
        fDstMin = std::numeric_limits<GInt16>::min();
        fDstMax = std::numeric_limits<GInt16>::max();
    }
    else if( dstDataType == GDT_UInt32 )
    {
        fDstMin = static_cast<float>(std::numeric_limits<GUInt32>::min());
        fDstMax = static_cast<float>(std::numeric_limits<GUInt32>::max());
    }
    else if( dstDataType == GDT_Int32 )
    {
        // cppcheck-suppress unreadVariable
        fDstMin = static_cast<float>(std::numeric_limits<GInt32>::min());
        // cppcheck-suppress unreadVariable
        fDstMax = static_cast<float>(std::numeric_limits<GInt32>::max());
    }

    auto replaceValIfNodata =
        [bHasNoData, isIntegerDT, fDstMin, fDstMax, nNodataValueInt64,
         fNoDataValue, fReplacementVal](float fVal)
    {
        if( !bHasNoData )
            return fVal;

        // Clamp value before comparing to nodata: this is only needed for
        // kernels with negative weights (Lanczos)
        float fClamped = fVal;
        if( fClamped < fDstMin )
            fClamped = fDstMin;
        else if( fClamped > fDstMax )
            fClamped = fDstMax;
        if( isIntegerDT )
        {
            if( nNodataValueInt64 == static_cast<GInt64>(std::round(fClamped)) )
            {
                // Do not use the nodata value
                return fReplacementVal;
            }
        }
        else if( fNoDataValue == fClamped )
        {
            // Do not use the nodata value
            return fReplacementVal;
        }
        return fClamped;
    };

/* -------------------------------------------------------------------- */
/*      Allocate work buffers.                                          */
/* -------------------------------------------------------------------- */
    const int nDstXSize = nDstXOff2 - nDstXOff;
    float* pafWrkScanline = nullptr;
    if( dstDataType != GDT_Float32 )
    {
        pafWrkScanline = static_cast<float*>(
            VSI_MALLOC2_VERBOSE(nDstXSize, sizeof(float)));
        if( pafWrkScanline == nullptr )
            return CE_Failure;
    }

    const double dfXScale = 1.0 / dfXRatioDstToSrc;
    const double dfXScaleWeight = ( dfXScale >= 1.0 ) ? 1.0 : dfXScale;
    const double dfXScaledRadius = nKernelRadius / dfXScaleWeight;
    const double dfYScale = 1.0 / dfYRatioDstToSrc;
    const double dfYScaleWeight = ( dfYScale >= 1.0 ) ? 1.0 : dfYScale;
    const double dfYScaledRadius = nKernelRadius / dfYScaleWeight;

    // Temporary array to store result of horizontal filter.
    double* padfHorizontalFiltered = static_cast<double*>(
        VSI_MALLOC3_VERBOSE(nChunkYSize, nDstXSize, sizeof(double) * nBands) );

    // To store convolution coefficients.
    double* padfWeights = static_cast<double *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            static_cast<int>(
                2 + 2 * std::max(dfXScaledRadius, dfYScaledRadius) +
                0.5) * sizeof(double) ) );

    GByte* pabyChunkNodataMaskHorizontalFiltered = nullptr;
    if( pabyChunkNodataMask )
        pabyChunkNodataMaskHorizontalFiltered = static_cast<GByte*>(
            VSI_MALLOC2_VERBOSE(nChunkYSize, nDstXSize) );
    if( padfHorizontalFiltered == nullptr ||
        padfWeights == nullptr ||
        (pabyChunkNodataMask != nullptr &&
         pabyChunkNodataMaskHorizontalFiltered == nullptr) )
    {
        VSIFree(pafWrkScanline);
        VSIFree(padfHorizontalFiltered);
        VSIFreeAligned(padfWeights);
        VSIFree(pabyChunkNodataMaskHorizontalFiltered);
        return CE_Failure;
    }

/* ==================================================================== */
/*      First pass: horizontal filter                                   */
/* ==================================================================== */
    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
#ifdef USE_SSE2
    bool bSrcPixelCountLess8 = dfXScaledRadius < 4;
#endif
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        const double dfSrcPixel =
            (iDstPixel + 0.5) * dfXRatioDstToSrc + dfSrcXDelta;
        int nSrcPixelStart =
            static_cast<int>(floor(dfSrcPixel - dfXScaledRadius + 0.5));
        if( nSrcPixelStart < nChunkXOff )
            nSrcPixelStart = nChunkXOff;
        int nSrcPixelStop =
            static_cast<int>(dfSrcPixel + dfXScaledRadius + 0.5);
        if( nSrcPixelStop > nChunkRightXOff )
            nSrcPixelStop = nChunkRightXOff;
#if 0
        if( nSrcPixelStart < nChunkXOff && nChunkXOff > 0 )
        {
            printf( "truncated iDstPixel = %d\n", iDstPixel );/*ok*/
        }
        if( nSrcPixelStop > nChunkRightXOff && nChunkRightXOff < nSrcWidth )
        {
            printf( "truncated iDstPixel = %d\n", iDstPixel );/*ok*/
        }
#endif
        const int nSrcPixelCount = nSrcPixelStop - nSrcPixelStart;
        double dfWeightSum = 0.0;

        // Compute convolution coefficients.
        int nSrcPixel = nSrcPixelStart;
        double dfX = dfXScaleWeight * (nSrcPixel - dfSrcPixel + 0.5);
        for( ; nSrcPixel + 3 < nSrcPixelStop; nSrcPixel+=4)
        {
            padfWeights[nSrcPixel - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+1 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+2 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+3 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcPixel - nSrcPixelStart);
        }
        for( ; nSrcPixel < nSrcPixelStop; ++nSrcPixel, dfX += dfXScaleWeight)
        {
            const double dfWeight = pfnFilterFunc(dfX);
            padfWeights[nSrcPixel - nSrcPixelStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        const int nHeight = nChunkYSize * nBands;
        if( pabyChunkNodataMask == nullptr )
        {
            if( dfWeightSum != 0 )
            {
                const double dfInvWeightSum = 1.0 / dfWeightSum;
                for( int i = 0; i < nSrcPixelCount; ++i )
                    padfWeights[i] *= dfInvWeightSum;
            }
            int iSrcLineOff = 0;
#ifdef USE_SSE2
            if( nSrcPixelCount == 4 )
            {
                for( ; iSrcLineOff+2 < nHeight; iSrcLineOff +=3 )
                {
                    const GPtrDiff_t j =
                        static_cast<GPtrDiff_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontalPixelCount4_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize,
                        padfWeights, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal1;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 1) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal2;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 2) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal3;
                }
            }
            else if( bSrcPixelCountLess8 )
            {
                for( ; iSrcLineOff+2 < nHeight; iSrcLineOff +=3 )
                {
                    const GPtrDiff_t j =
                        static_cast<GPtrDiff_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize,
                        padfWeights, nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal1;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 1) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal2;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 2) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal3;
                }
            }
            else
#endif
            {
                for( ; iSrcLineOff+2 < nHeight; iSrcLineOff +=3 )
                {
                    const GPtrDiff_t j =
                        static_cast<GPtrDiff_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontal_3rows(
                        pChunk + j,
                        pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize,
                        padfWeights, nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal1;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 1) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal2;
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) + 2) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal3;
                }
            }
            for( ; iSrcLineOff < nHeight; ++iSrcLineOff )
            {
                const GPtrDiff_t j =
                    static_cast<GPtrDiff_t>(iSrcLineOff) * nChunkXSize + (nSrcPixelStart - nChunkXOff);
                const double dfVal =
                    GDALResampleConvolutionHorizontal(pChunk + j,
                                                padfWeights, nSrcPixelCount);
                padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) * nDstXSize +
                                       iDstPixel - nDstXOff] = dfVal;
            }
        }
        else
        {
            for( int iSrcLineOff = 0; iSrcLineOff < nHeight; ++iSrcLineOff )
            {
                const GPtrDiff_t j =
                    static_cast<GPtrDiff_t>(iSrcLineOff) * nChunkXSize + (nSrcPixelStart - nChunkXOff);

                if( bKernelWithNegativeWeights )
                {
                    int nConsecutiveValid = 0;
                    int nMaxConsecutiveValid = 0;
                    for( int k = 0; k < nSrcPixelCount; k++ )
                    {
                        if( pabyChunkNodataMask[j + k] )
                            nConsecutiveValid ++;
                        else if( nConsecutiveValid )
                        {
                            nMaxConsecutiveValid = std::max(nMaxConsecutiveValid,
                                                            nConsecutiveValid);
                            nConsecutiveValid = 0;
                        }
                    }
                    nMaxConsecutiveValid = std::max(nMaxConsecutiveValid,
                                                    nConsecutiveValid);
                    if( nMaxConsecutiveValid < nSrcPixelCount / 2 )
                    {
                        const size_t nTempOffset =
                            static_cast<size_t>(iSrcLineOff) * nDstXSize + iDstPixel - nDstXOff;
                        padfHorizontalFiltered[nTempOffset] = 0.0;
                        pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 0;
                        continue;
                    }
                }

                double dfVal = 0.0;
                GDALResampleConvolutionHorizontalWithMask(
                    pChunk + j, pabyChunkNodataMask + j,
                    padfWeights, nSrcPixelCount,
                    dfVal, dfWeightSum );
                const size_t nTempOffset =
                    static_cast<size_t>(iSrcLineOff) * nDstXSize + iDstPixel - nDstXOff;
                if( dfWeightSum > 0.0 )
                {
                    padfHorizontalFiltered[nTempOffset] = dfVal / dfWeightSum;
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 1;
                }
                else
                {
                    padfHorizontalFiltered[nTempOffset] = 0.0;
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 0;
                }
            }
        }
    }

/* ==================================================================== */
/*      Second pass: vertical filter                                    */
/* ==================================================================== */
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;

    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        float* const pafDstScanline = pafWrkScanline ? pafWrkScanline:
            static_cast<float*>(pDstBuffer) + (iDstLine - nDstYOff)  * nDstXSize;

        const double dfSrcLine =
            (iDstLine + 0.5) * dfYRatioDstToSrc + dfSrcYDelta;
        int nSrcLineStart =
            static_cast<int>(floor(dfSrcLine - dfYScaledRadius + 0.5));
        int nSrcLineStop = static_cast<int>(dfSrcLine + dfYScaledRadius + 0.5);
        if( nSrcLineStart < nChunkYOff )
            nSrcLineStart = nChunkYOff;
        if( nSrcLineStop > nChunkBottomYOff )
            nSrcLineStop = nChunkBottomYOff;
#if 0
        if( nSrcLineStart < nChunkYOff &&
            nChunkYOff > 0 )
        {
            printf( "truncated iDstLine = %d\n", iDstLine );/*ok*/
        }
        if( nSrcLineStop > nChunkBottomYOff && nChunkBottomYOff < nSrcHeight )
        {
            printf( "truncated iDstLine = %d\n", iDstLine );/*ok*/
        }
#endif
        const int nSrcLineCount = nSrcLineStop - nSrcLineStart;
        double dfWeightSum = 0.0;

        // Compute convolution coefficients.
        int nSrcLine = nSrcLineStart;  // Used after for.
        double dfY = dfYScaleWeight * (nSrcLine - dfSrcLine + 0.5);
        for( ;
             nSrcLine + 3 < nSrcLineStop;
             nSrcLine += 4, dfY += 4 * dfYScaleWeight)
        {
            padfWeights[nSrcLine - nSrcLineStart] = dfY;
            padfWeights[nSrcLine+1 - nSrcLineStart] = dfY + dfYScaleWeight;
            padfWeights[nSrcLine+2 - nSrcLineStart] = dfY + 2 * dfYScaleWeight;
            padfWeights[nSrcLine+3 - nSrcLineStart] = dfY + 3 * dfYScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcLine - nSrcLineStart);
        }
        for( ; nSrcLine < nSrcLineStop; ++nSrcLine, dfY += dfYScaleWeight )
        {
            const double dfWeight = pfnFilterFunc(dfY);
            padfWeights[nSrcLine - nSrcLineStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        if( pabyChunkNodataMask == nullptr )
        {
            if( dfWeightSum != 0 )
            {
                const double dfInvWeightSum = 1.0 / dfWeightSum;
                for( int i = 0; i < nSrcLineCount; ++i )
                    padfWeights[i] *= dfInvWeightSum;
            }
        }

        if( pabyChunkNodataMask == nullptr )
        {
            int iFilteredPixelOff = 0;  // Used after for.
            // j used after for.
            size_t j = (nSrcLineStart - nChunkYOff) * static_cast<size_t>(nDstXSize);
#ifdef USE_SSE2

#ifdef __AVX__
            for( ;
                 iFilteredPixelOff+15 < nDstXSize;
                 iFilteredPixelOff += 16, j += 16 )
            {
                GDALResampleConvolutionVertical_16cols(
                    padfHorizontalFiltered + j, nDstXSize, padfWeights,
                    nSrcLineCount, pafDstScanline + iFilteredPixelOff );
                if( bHasNoData )
                {
                    for( int k = 0; k < 16; k++ )
                    {
                        pafDstScanline[iFilteredPixelOff + k] =
                            replaceValIfNodata(pafDstScanline[iFilteredPixelOff + k]);
                    }
                }
            }
#else
            for( ;
                 iFilteredPixelOff+7 < nDstXSize;
                 iFilteredPixelOff += 8, j += 8 )
            {
                GDALResampleConvolutionVertical_8cols(
                    padfHorizontalFiltered + j, nDstXSize, padfWeights,
                    nSrcLineCount, pafDstScanline + iFilteredPixelOff );
                if( bHasNoData )
                {
                    for( int k = 0; k < 8; k++ )
                    {
                        pafDstScanline[iFilteredPixelOff + k] =
                            replaceValIfNodata(pafDstScanline[iFilteredPixelOff + k]);
                    }
                }
            }
#endif

            for( ; iFilteredPixelOff < nDstXSize; iFilteredPixelOff++, j++ )
            {
                const float fVal = static_cast<float>(
                    GDALResampleConvolutionVertical(
                        padfHorizontalFiltered + j,
                        nDstXSize, padfWeights, nSrcLineCount ));
                pafDstScanline[iFilteredPixelOff] = replaceValIfNodata(fVal);
            }
#else
            for( ;
                 iFilteredPixelOff+1 < nDstXSize;
                 iFilteredPixelOff += 2, j += 2 )
            {
                double dfVal1 = 0.0;
                double dfVal2 = 0.0;
                GDALResampleConvolutionVertical_2cols(
                    padfHorizontalFiltered + j, nDstXSize, padfWeights,
                    nSrcLineCount, dfVal1, dfVal2 );
                pafDstScanline[iFilteredPixelOff] = replaceValIfNodata(
                    static_cast<float>(dfVal1));
                pafDstScanline[iFilteredPixelOff+1] = replaceValIfNodata(
                    static_cast<float>(dfVal2));
            }
            if( iFilteredPixelOff < nDstXSize )
            {
                const double dfVal =
                    GDALResampleConvolutionVertical(
                        padfHorizontalFiltered + j,
                        nDstXSize, padfWeights, nSrcLineCount );
                pafDstScanline[iFilteredPixelOff] = replaceValIfNodata(
                    static_cast<float>(dfVal));
            }
#endif
        }
        else
        {
            for( int iFilteredPixelOff = 0;
                 iFilteredPixelOff < nDstXSize;
                 ++iFilteredPixelOff )
            {
                double dfVal = 0.0;
                dfWeightSum = 0.0;
                size_t j = (nSrcLineStart - nChunkYOff) * static_cast<size_t>(nDstXSize)
                             + iFilteredPixelOff;
                if( bKernelWithNegativeWeights )
                {
                    int nConsecutiveValid = 0;
                    int nMaxConsecutiveValid = 0;
                    for(int i = 0; i < nSrcLineCount; ++i, j += nDstXSize)
                    {
                        const double dfWeight =
                            padfWeights[i]
                            * pabyChunkNodataMaskHorizontalFiltered[j];
                        if( pabyChunkNodataMaskHorizontalFiltered[j] )
                        {
                            nConsecutiveValid ++;
                        }
                        else if( nConsecutiveValid )
                        {
                            nMaxConsecutiveValid = std::max(
                                nMaxConsecutiveValid, nConsecutiveValid);
                            nConsecutiveValid = 0;
                        }
                        dfVal += padfHorizontalFiltered[j] * dfWeight;
                        dfWeightSum += dfWeight;
                    }
                    nMaxConsecutiveValid = std::max(nMaxConsecutiveValid,
                                                    nConsecutiveValid);
                    if( nMaxConsecutiveValid < nSrcLineCount / 2 )
                    {
                        pafDstScanline[iFilteredPixelOff] = fNoDataValue;
                        continue;
                    }
                }
                else
                {
                    for(int i = 0; i < nSrcLineCount; ++i, j += nDstXSize)
                    {
                        const double dfWeight =
                            padfWeights[i]
                            * pabyChunkNodataMaskHorizontalFiltered[j];
                        dfVal += padfHorizontalFiltered[j] * dfWeight;
                        dfWeightSum += dfWeight;
                    }
                }
                if( dfWeightSum > 0.0 )
                {
                    pafDstScanline[iFilteredPixelOff] = replaceValIfNodata(
                        static_cast<float>(dfVal / dfWeightSum));
                }
                else
                {
                    pafDstScanline[iFilteredPixelOff] = fNoDataValue;
                }
            }
        }

        if( fMaxVal != 0.0f )
        {
            for( int i = 0; i < nDstXSize; ++i )
            {
                if( pafDstScanline[i] > fMaxVal )
                    pafDstScanline[i] = fMaxVal;
            }
        }

        if( pafWrkScanline )
        {
            GDALCopyWords(pafWrkScanline, GDT_Float32, 4,
                          static_cast<GByte*>(pDstBuffer) +
                            static_cast<size_t>(iDstLine - nDstYOff) *
                                nDstXSize * nDstDataTypeSize,
                          dstDataType, nDstDataTypeSize,
                          nDstXSize);
        }
    }

    VSIFree(pafWrkScanline);
    VSIFreeAligned( padfWeights );
    VSIFree( padfHorizontalFiltered );
    VSIFree( pabyChunkNodataMaskHorizontalFiltered );

    return CE_None;
}

static CPLErr GDALResampleChunk32R_Convolution(
    double dfXRatioDstToSrc, double dfYRatioDstToSrc,
    double dfSrcXDelta,
    double dfSrcYDelta,
    GDALDataType eWrkDataType,
    const void * pChunk,
    const GByte * pabyChunkNodataMask,
    int nChunkXOff, int nChunkXSize,
    int nChunkYOff, int nChunkYSize,
    int nDstXOff, int nDstXOff2,
    int nDstYOff, int nDstYOff2,
    GDALRasterBand * poOverview,
    void** ppDstBuffer,
    GDALDataType* peDstBufferDataType,
    const char * pszResampling,
    int bHasNoData, float fNoDataValue,
    GDALColorTable* /* poColorTable_unused */,
    GDALDataType /* eSrcDataType */,
    bool /* bPropagateNoData */ )
{
    GDALResampleAlg eResample;
    bool bKernelWithNegativeWeights = false;
    if( EQUAL(pszResampling, "BILINEAR") )
        eResample = GRA_Bilinear;
    else if( EQUAL(pszResampling, "CUBIC") )
        eResample = GRA_Cubic;
    else if( EQUAL(pszResampling, "CUBICSPLINE") )
        eResample = GRA_CubicSpline;
    else if( EQUAL(pszResampling, "LANCZOS") )
    {
        eResample = GRA_Lanczos;
        bKernelWithNegativeWeights = true;
    }
    else
    {
        CPLAssert(false);
        return CE_Failure;
    }
    const int nKernelRadius = GWKGetFilterRadius(eResample);
    FilterFuncType pfnFilterFunc = GWKGetFilterFunc(eResample);
    const FilterFunc4ValuesType pfnFilterFunc4Values =
        GWKGetFilterFunc4Values(eResample);

    float fMaxVal = 0.f;
    // Cubic, etc... can have overshoots, so make sure we clamp values to the
    // maximum value if NBITS is set.
    const char* pszNBITS =
        poOverview->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    GDALDataType eBandDT = poOverview->GetRasterDataType();
    if( eResample != GRA_Bilinear && pszNBITS != nullptr &&
        (eBandDT == GDT_Byte || eBandDT == GDT_UInt16 ||
         eBandDT == GDT_UInt32) )
    {
        int nBits = atoi(pszNBITS);
        if( nBits == GDALGetDataTypeSize(eBandDT) )
            nBits = 0;
        if( nBits > 0 && nBits < 32 )
            fMaxVal = static_cast<float>((1U << nBits) -1);
    }

    *ppDstBuffer =
        VSI_MALLOC3_VERBOSE(nDstXOff2 - nDstXOff,
                            nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(eBandDT));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    *peDstBufferDataType = eBandDT;

    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_ConvolutionT<GByte>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<const GByte *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            *ppDstBuffer,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            bKernelWithNegativeWeights,
            fMaxVal );
    else if( eWrkDataType == GDT_UInt16 )
        return GDALResampleChunk32R_ConvolutionT<GUInt16>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<const GUInt16 *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            *ppDstBuffer,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            bKernelWithNegativeWeights,
            fMaxVal );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_ConvolutionT<float>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<const float *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            *ppDstBuffer,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            bKernelWithNegativeWeights,
            fMaxVal );

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                       GDALResampleChunkC32R()                        */
/************************************************************************/

static CPLErr
GDALResampleChunkC32R( int nSrcWidth, int nSrcHeight,
                       const float * pafChunk, int nChunkYOff, int nChunkYSize,
                       int nDstYOff, int nDstYOff2,
                       GDALRasterBand * poOverview,
                       void** ppDstBuffer,
                       GDALDataType* peDstBufferDataType,
                       const char * pszResampling )

{
    enum Method
    {
        NEAR,
        AVERAGE,
        AVERAGE_MAGPHASE,
        RMS,
    };

    Method eMethod = NEAR;
    if( STARTS_WITH_CI(pszResampling, "NEAR") )
    {
        eMethod = NEAR;
    }
    else if( EQUAL(pszResampling, "AVERAGE_MAGPHASE") )
    {
        eMethod = AVERAGE_MAGPHASE;
    }
    else if( EQUAL(pszResampling, "RMS") )
    {
        eMethod = RMS;
    }
    else if( STARTS_WITH_CI(pszResampling, "AVER") )
    {
        eMethod = AVERAGE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported resampling method %s for GDALResampleChunkC32R",
                 pszResampling);
        return CE_Failure;
    }

    const int nOXSize = poOverview->GetXSize();
    *ppDstBuffer =
        VSI_MALLOC3_VERBOSE(nOXSize,
                            nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(GDT_CFloat32));
    if( *ppDstBuffer == nullptr )
    {
        return CE_Failure;
    }
    float* const pafDstBuffer = static_cast<float*>(*ppDstBuffer);
    *peDstBufferDataType = GDT_CFloat32;

    const int nOYSize = poOverview->GetYSize();
    const double dfXRatioDstToSrc = static_cast<double>(nSrcWidth) / nOXSize;
    const double dfYRatioDstToSrc = static_cast<double>(nSrcHeight) / nOYSize;

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        int nSrcYOff2 = static_cast<int>(0.5 + (iDstLine+1) * dfYRatioDstToSrc);
        if( nSrcYOff2 == nSrcYOff )
            nSrcYOff2 ++;

        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
        {
            if( nSrcYOff == nSrcHeight && nSrcHeight - 1 >= nChunkYOff )
                nSrcYOff = nSrcHeight - 1;
            nSrcYOff2 = nSrcHeight;
        }
        if( nSrcYOff2 > nChunkYOff + nChunkYSize )
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        const float * const pafSrcScanline =
            pafChunk + ((nSrcYOff - nChunkYOff) * nSrcWidth) * 2;
        float* const pafDstScanline =
            pafDstBuffer + (iDstLine - nDstYOff) * 2 * nOXSize;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = 0; iDstPixel < nOXSize; ++iDstPixel )
        {
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 = static_cast<int>(
                0.5 + (iDstPixel+1) * dfXRatioDstToSrc);
            if( nSrcXOff2 == nSrcXOff )
                nSrcXOff2 ++;
            if( nSrcXOff2 > nSrcWidth || iDstPixel == nOXSize-1 )
            {
                if( nSrcXOff == nSrcWidth && nSrcWidth - 1 >= 0 )
                    nSrcXOff = nSrcWidth - 1;
                nSrcXOff2 = nSrcWidth;
            }

            if( eMethod == NEAR )
            {
                pafDstScanline[iDstPixel*2] = pafSrcScanline[nSrcXOff*2];
                pafDstScanline[iDstPixel*2+1] = pafSrcScanline[nSrcXOff*2+1];
            }
            else if( eMethod == AVERAGE_MAGPHASE )
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                double dfTotalM = 0.0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const double dfR =
                            pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2];
                        const double dfI =
                            pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2+1];
                        dfTotalR += dfR;
                        dfTotalI += dfI;
                        dfTotalM += std::hypot(dfR, dfI);
                        ++nCount;
                    }
                }

                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixel*2  ] =
                        static_cast<float>(dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] =
                        static_cast<float>(dfTotalI/nCount);
                    const double dfM = std::hypot(pafDstScanline[iDstPixel*2],
                                                  pafDstScanline[iDstPixel*2+1]);
                    const double dfDesiredM = dfTotalM / nCount;
                    double dfRatio = 1.0;
                    if( dfM != 0.0 )
                        dfRatio = dfDesiredM / dfM;

                    pafDstScanline[iDstPixel*2] *=
                        static_cast<float>(dfRatio);
                    pafDstScanline[iDstPixel*2+1] *=
                        static_cast<float>(dfRatio);
                }
            }
            else if( eMethod == RMS )
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const double dfR = pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2];
                        const double dfI = pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2+1];

                        dfTotalR += SQUARE(dfR);
                        dfTotalI += SQUARE(dfI);

                        ++nCount;
                    }
                }

                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    /* compute RMS */
                    pafDstScanline[iDstPixel*2  ] = static_cast<float>(sqrt(dfTotalR/nCount));
                    pafDstScanline[iDstPixel*2+1] = static_cast<float>(sqrt(dfTotalI/nCount));
                }
            }
            else if( eMethod == AVERAGE )
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        // TODO(schwehr): Maybe use std::complex?
                        dfTotalR +=
                            pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2];
                        dfTotalI +=
                            pafSrcScanline[iX*2+static_cast<GPtrDiff_t>(iY-nSrcYOff)*nSrcWidth*2+1];
                        ++nCount;
                    }
                }

                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixel*2  ] =
                        static_cast<float>(dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] =
                        static_cast<float>(dfTotalI/nCount);
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                  GDALRegenerateCascadingOverviews()                  */
/*                                                                      */
/*      Generate a list of overviews in order from largest to           */
/*      smallest, computing each from the next larger.                  */
/************************************************************************/

static CPLErr
GDALRegenerateCascadingOverviews(
    GDALRasterBand *poSrcBand, int nOverviews, GDALRasterBand **papoOvrBands,
    const char * pszResampling,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      First, we must put the overviews in order from largest to       */
/*      smallest.                                                       */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nOverviews-1; ++i )
    {
        for( int j = 0; j < nOverviews - i - 1; ++j )
        {
            if( papoOvrBands[j]->GetXSize()
                * static_cast<float>( papoOvrBands[j]->GetYSize() ) <
                papoOvrBands[j+1]->GetXSize()
                * static_cast<float>( papoOvrBands[j+1]->GetYSize() ) )
            {
                GDALRasterBand *poTempBand = papoOvrBands[j];
                papoOvrBands[j] = papoOvrBands[j+1];
                papoOvrBands[j+1] = poTempBand;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Count total pixels so we can prepare appropriate scaled         */
/*      progress functions.                                             */
/* -------------------------------------------------------------------- */
    double dfTotalPixels = 0.0;

    for( int i = 0; i < nOverviews; ++i )
    {
        dfTotalPixels += papoOvrBands[i]->GetXSize()
            * static_cast<double>( papoOvrBands[i]->GetYSize() );
    }

/* -------------------------------------------------------------------- */
/*      Generate all the bands.                                         */
/* -------------------------------------------------------------------- */
    double dfPixelsProcessed = 0.0;

    for( int i = 0; i < nOverviews; ++i )
    {
        GDALRasterBand *poBaseBand = poSrcBand;
        if( i != 0 )
            poBaseBand = papoOvrBands[i-1];

        double dfPixels =
            papoOvrBands[i]->GetXSize()
            * static_cast<double>( papoOvrBands[i]->GetYSize() );

        void *pScaledProgressData =
            GDALCreateScaledProgress(
            dfPixelsProcessed / dfTotalPixels,
            (dfPixelsProcessed + dfPixels) / dfTotalPixels,
            pfnProgress, pProgressData );

        const CPLErr eErr =
            GDALRegenerateOverviews(
                poBaseBand,
                1,
                reinterpret_cast<GDALRasterBandH *>( papoOvrBands ) + i,
                pszResampling,
                GDALScaledProgress,
                pScaledProgressData );
        GDALDestroyScaledProgress( pScaledProgressData );

        if( eErr != CE_None )
            return eErr;

        dfPixelsProcessed += dfPixels;

        // Only do the bit2grayscale promotion on the base band.
        if( STARTS_WITH_CI( pszResampling,
                            "AVERAGE_BIT2G" /* AVERAGE_BIT2GRAYSCALE */) )
            pszResampling = "AVERAGE";
    }

    return CE_None;
}

/************************************************************************/
/*                    GDALGetResampleFunction()                         */
/************************************************************************/

GDALResampleFunction GDALGetResampleFunction( const char* pszResampling,
                                              int* pnRadius )
{
    if( pnRadius ) *pnRadius = 0;
    if( STARTS_WITH_CI(pszResampling, "NEAR") )
        return GDALResampleChunk32R_Near;
    else if( STARTS_WITH_CI(pszResampling, "AVER") || EQUAL(pszResampling, "RMS") )
        return GDALResampleChunk32R_Average;
    else if( STARTS_WITH_CI(pszResampling, "GAUSS") )
    {
        if( pnRadius ) *pnRadius = 1;
        return GDALResampleChunk32R_Gauss;
    }
    else if( STARTS_WITH_CI(pszResampling, "MODE") )
        return GDALResampleChunk32R_Mode;
    else if( EQUAL(pszResampling,"CUBIC") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Cubic);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"CUBICSPLINE") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_CubicSpline);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"LANCZOS") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Lanczos);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"BILINEAR") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Bilinear);
        return GDALResampleChunk32R_Convolution;
    }
    else
    {
       CPLError(
           CE_Failure, CPLE_AppDefined,
           "GDALGetResampleFunction: Unsupported resampling method \"%s\".",
           pszResampling );
        return nullptr;
    }
}

/************************************************************************/
/*                      GDALGetOvrWorkDataType()                        */
/************************************************************************/

GDALDataType GDALGetOvrWorkDataType( const char* pszResampling,
                                     GDALDataType eSrcDataType )
{
    if( (STARTS_WITH_CI(pszResampling, "NEAR") ||
         STARTS_WITH_CI(pszResampling, "AVER") ||
         EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) &&
        eSrcDataType == GDT_Byte )
    {
        return GDT_Byte;
    }
    else if( (STARTS_WITH_CI(pszResampling, "NEAR") ||
         STARTS_WITH_CI(pszResampling, "AVER") ||
         EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) &&
        eSrcDataType == GDT_UInt16 )
    {
        return GDT_UInt16;
    }

    return GDT_Float32;
}


namespace {
// Structure to hold a pointer to free with CPLFree()
struct PointerHolder
{
    void* ptr = nullptr;

    explicit PointerHolder(void* ptrIn): ptr(ptrIn) {}
    ~PointerHolder() { CPLFree(ptr); }
    PointerHolder(const PointerHolder&) = delete;
    PointerHolder& operator=(const PointerHolder&) = delete;
};
}

/************************************************************************/
/*                      GDALRegenerateOverviews()                       */
/************************************************************************/

/**
 * \brief Generate downsampled overviews.
 *
 * This function will generate one or more overview images from a base image
 * using the requested downsampling algorithm.  Its primary use is for
 * generating overviews via GDALDataset::BuildOverviews(), but it can also be
 * used to generate downsampled images in one file from another outside the
 * overview architecture.
 *
 * The output bands need to exist in advance.
 *
 * The full set of resampling algorithms is documented in
 * GDALDataset::BuildOverviews().
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset
 * metadata) so that only a given RGB triplet (in case of a RGB image) will be
 * considered as the nodata value and not each value of the triplet
 * independently per band.
 *
 * Starting with GDAL 3.2, the GDAL_NUM_THREADS configuration option can be set
 * to "ALL_CPUS" or a integer value to specify the number of threads to use for
 * overview computation.
 *
 * @param hSrcBand the source (base level) band.
 * @param nOverviewCount the number of downsampled bands being generated.
 * @param pahOvrBands the list of downsampled bands to be generated.
 * @param pszResampling Resampling algorithm (e.g. "AVERAGE").
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */
CPLErr
GDALRegenerateOverviews( GDALRasterBandH hSrcBand,
                         int nOverviewCount, GDALRasterBandH *pahOvrBands,
                         const char * pszResampling,
                         GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALRasterBand *poSrcBand = GDALRasterBand::FromHandle( hSrcBand );
    GDALRasterBand **papoOvrBands =
        reinterpret_cast<GDALRasterBand **>( pahOvrBands );

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn
        = GDALGetResampleFunction(pszResampling, &nKernelRadius);

    if( pfnResampleFn == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Check color tables...                                           */
/* -------------------------------------------------------------------- */
    GDALColorTable* poColorTable = nullptr;

    if( (STARTS_WITH_CI(pszResampling, "AVER")
         || EQUAL(pszResampling, "RMS")
         || STARTS_WITH_CI(pszResampling, "MODE")
         || STARTS_WITH_CI(pszResampling, "GAUSS")) &&
        poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
    {
        poColorTable = poSrcBand->GetColorTable();
        if( poColorTable != nullptr )
        {
            if( poColorTable->GetPaletteInterpretation() != GPI_RGB )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Computing overviews on palette index raster bands "
                    "with a palette whose color interpretation is not RGB "
                    "will probably lead to unexpected results." );
                poColorTable = nullptr;
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Computing overviews on palette index raster bands "
                      "without a palette will probably lead to unexpected "
                      "results." );
        }
    }
    // Not ready yet
    else if( (EQUAL(pszResampling, "CUBIC") ||
              EQUAL(pszResampling, "CUBICSPLINE") ||
              EQUAL(pszResampling, "LANCZOS") ||
              EQUAL(pszResampling, "BILINEAR") )
        && poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Computing %s overviews on palette index raster bands "
                  "will probably lead to unexpected results.", pszResampling );
    }

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.

    GDALRasterBand* poMaskBand = nullptr;
    bool bUseNoDataMask = false;
    bool bCanUseCascaded = true;

    if( !STARTS_WITH_CI(pszResampling, "NEAR") )
    {
        int nMaskFlags;
        // Special case if we are the alpha band. We want it to be considered
        // as the mask band to avoid alpha=0 to be taken into account in average
        // computation.
        if( poSrcBand->GetColorInterpretation() == GCI_AlphaBand )
        {
            poMaskBand = poSrcBand;
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
        }
        // Same as above for mask band. I'd wish we had a better way of conveying this !
        else if( CPLTestBool(CPLGetConfigOption(
                    "GDAL_REGENERATED_BAND_IS_MASK", "NO")) )
        {
            poMaskBand = poSrcBand;
            nMaskFlags = GMF_PER_DATASET;
        }
        else
        {
            poMaskBand = poSrcBand->GetMaskBand();
            nMaskFlags = poSrcBand->GetMaskFlags();
            bCanUseCascaded = (nMaskFlags == GMF_NODATA ||
                               nMaskFlags == GMF_ALL_VALID);
        }

        bUseNoDataMask = (nMaskFlags & GMF_ALL_VALID) == 0;
    }

/* -------------------------------------------------------------------- */
/*      If we are operating on multiple overviews, and using            */
/*      averaging, lets do them in cascading order to reduce the        */
/*      amount of computation.                                          */
/* -------------------------------------------------------------------- */

    // In case the mask made be computed from another band of the dataset,
    // we can't use cascaded generation, as the computation of the overviews
    // of the band used for the mask band may not have yet occurred (#3033).
    if( (STARTS_WITH_CI(pszResampling, "AVER") |
         STARTS_WITH_CI(pszResampling, "GAUSS") ||
         EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) && nOverviewCount > 1
         && bCanUseCascaded )
        return GDALRegenerateCascadingOverviews( poSrcBand,
                                                 nOverviewCount, papoOvrBands,
                                                 pszResampling,
                                                 pfnProgress,
                                                 pProgressData );

/* -------------------------------------------------------------------- */
/*      Setup one horizontal swath to read from the raw buffer.         */
/* -------------------------------------------------------------------- */
    int nFRXBlockSize = 0;
    int nFRYBlockSize = 0;
    poSrcBand->GetBlockSize( &nFRXBlockSize, &nFRYBlockSize );

    int nFullResYChunk = 0;
    if( nFRYBlockSize < 16 || nFRYBlockSize > 256 )
        nFullResYChunk = 64;
    else
        nFullResYChunk = nFRYBlockSize;

    // Only configurable for debug / testing
    const char* pszChunkYSize = CPLGetConfigOption("GDAL_OVR_CHUNKYSIZE", nullptr);
    if( pszChunkYSize )
    {
        // coverity[tainted_data]
        nFullResYChunk = atoi(pszChunkYSize);
    }

    const GDALDataType eSrcDataType = poSrcBand->GetRasterDataType();
    const GDALDataType eWrkDataType = GDALDataTypeIsComplex( eSrcDataType )?
        GDT_CFloat32 :
        GDALGetOvrWorkDataType( pszResampling, eSrcDataType );

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    int nMaxOvrFactor = 1;
    for( int iOverview = 0; iOverview < nOverviewCount; ++iOverview )
    {
        const int nDstWidth = papoOvrBands[iOverview]->GetXSize();
        const int nDstHeight = papoOvrBands[iOverview]->GetYSize();
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nWidth) / nDstWidth + 0.5) );
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nHeight) / nDstHeight + 0.5) );
    }
    // Make sure that round(nChunkYOff / nMaxOvrFactor) < round((nChunkYOff + nFullResYChunk) / nMaxOvrFactor)
    nFullResYChunk = std::max(nFullResYChunk, 2 * nMaxOvrFactor);
    const int nMaxChunkYSizeQueried =
        nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;

    int bHasNoData = FALSE;
    const float fNoDataValue =
        static_cast<float>( poSrcBand->GetNoDataValue(&bHasNoData) );
    const bool bPropagateNoData =
        CPLTestBool( CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO") );

    // Structure describing a resampling job
    struct OvrJob
    {
        // Buffers to free when job is finished
        std::shared_ptr<PointerHolder> oSrcMaskBufferHolder{};
        std::shared_ptr<PointerHolder> oSrcBufferHolder{};
        std::unique_ptr<PointerHolder> oDstBufferHolder{};

        // Input parameters of pfnResampleFn
        GDALResampleFunction pfnResampleFn = nullptr;
        double dfXRatioDstToSrc{};
        double dfYRatioDstToSrc{};
        GDALDataType eWrkDataType = GDT_Unknown;
        const void * pChunk = nullptr;
        const GByte * pabyChunkNodataMask = nullptr;
        int nWidth = 0;
        int nHeight = 0;
        int nChunkYOff= 0;
        int nChunkYSize = 0;
        int nDstWidth = 0;
        int nDstYOff = 0;
        int nDstYOff2 = 0;
        GDALRasterBand* poDstBand = nullptr;
        const char * pszResampling = nullptr;
        int bHasNoData = 0;
        float fNoDataValue = 0.0f;
        GDALColorTable* poColorTable = nullptr;
        GDALDataType eSrcDataType = GDT_Unknown;
        bool bPropagateNoData = false;

        // Output values of resampling function
        CPLErr eErr = CE_Failure;
        void* pDstBuffer = nullptr;
        GDALDataType eDstBufferDataType = GDT_Unknown;

        // Synchronization
        bool                    bFinished = false;
        std::mutex              mutex{};
        std::condition_variable cv{};
    };

    // Thread function to resample
    const auto JobResampleFunc = [](void* pData)
    {
        OvrJob* poJob = static_cast<OvrJob*>(pData);

        if( poJob->eWrkDataType != GDT_CFloat32 )
        {
            poJob->eErr = poJob->pfnResampleFn(
                poJob->dfXRatioDstToSrc,
                poJob->dfYRatioDstToSrc,
                0.0, 0.0,
                poJob->eWrkDataType,
                poJob->pChunk,
                poJob->pabyChunkNodataMask,
                0,
                poJob->nWidth,
                poJob->nChunkYOff,
                poJob->nChunkYSize,
                0, poJob->nDstWidth,
                poJob->nDstYOff,
                poJob->nDstYOff2,
                poJob->poDstBand,
                &(poJob->pDstBuffer),
                &(poJob->eDstBufferDataType),
                poJob->pszResampling,
                poJob->bHasNoData,
                poJob->fNoDataValue,
                poJob->poColorTable,
                poJob->eSrcDataType,
                poJob->bPropagateNoData);
        }
        else
        {
            poJob->eErr = GDALResampleChunkC32R(
                poJob->nWidth,
                poJob->nHeight,
                static_cast<const float*>(poJob->pChunk),
                poJob->nChunkYOff,
                poJob->nChunkYSize,
                poJob->nDstYOff,
                poJob->nDstYOff2,
                poJob->poDstBand,
                &(poJob->pDstBuffer),
                &(poJob->eDstBufferDataType),
                poJob->pszResampling);
        }

        poJob->oDstBufferHolder.reset(new PointerHolder(poJob->pDstBuffer));

        {
            std::lock_guard<std::mutex> guard(poJob->mutex);
            poJob->bFinished = true;
            poJob->cv.notify_one();
        }
    };

    // Function to write resample data to target band
    const auto WriteJobData = [](const OvrJob* poJob)
    {
        return poJob->poDstBand->RasterIO( GF_Write,
                                            0,
                                            poJob->nDstYOff,
                                            poJob->nDstWidth,
                                            poJob->nDstYOff2 - poJob->nDstYOff,
                                            poJob->pDstBuffer,
                                            poJob->nDstWidth,
                                            poJob->nDstYOff2 - poJob->nDstYOff,
                                            poJob->eDstBufferDataType,
                                            0, 0, nullptr );
    };

    // Wait for completion of oldest job and serialize it
    const auto WaitAndFinalizeOldestJob = [WriteJobData](
                        std::list<std::unique_ptr<OvrJob>>& jobList)
    {
        auto poOldestJob = jobList.front().get();
        {
            std::unique_lock<std::mutex> oGuard(poOldestJob->mutex);
            while( !poOldestJob->bFinished )
            {
                poOldestJob->cv.wait(oGuard);
            }
        }
        CPLErr l_eErr = poOldestJob->eErr;
        if( l_eErr == CE_None )
        {
            l_eErr = WriteJobData(poOldestJob);
        }

        jobList.pop_front();
        return l_eErr;
    };

    // Queue of jobs
    std::list<std::unique_ptr<OvrJob>> jobList;

    GByte *pabyChunkNodataMask = nullptr;
    void *pChunk = nullptr;

    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");
    const int nThreads = std::max(1, std::min(128,
            EQUAL(pszThreads, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszThreads)));
    auto poThreadPool = nThreads > 1 ? GDALGetGlobalThreadPool(nThreads) : nullptr;
    auto poJobQueue = poThreadPool ? poThreadPool->CreateJobQueue() :
                            std::unique_ptr<CPLJobQueue>(nullptr);

/* -------------------------------------------------------------------- */
/*      Loop over image operating on chunks.                            */
/* -------------------------------------------------------------------- */
    int nChunkYOff = 0;
    CPLErr eErr = CE_None;

    for( nChunkYOff = 0;
         nChunkYOff < nHeight && eErr == CE_None;
         nChunkYOff += nFullResYChunk )
    {
        if( !pfnProgress( nChunkYOff / static_cast<double>( nHeight ),
                          nullptr, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if( nFullResYChunk + nChunkYOff > nHeight )
            nFullResYChunk = nHeight - nChunkYOff;

        int nChunkYOffQueried = nChunkYOff - nKernelRadius * nMaxOvrFactor;
        int nChunkYSizeQueried =
            nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;
        if( nChunkYOffQueried < 0 )
        {
            nChunkYSizeQueried += nChunkYOffQueried;
            nChunkYOffQueried = 0;
        }
        if( nChunkYOffQueried + nChunkYSizeQueried > nHeight )
            nChunkYSizeQueried = nHeight - nChunkYOffQueried;

        // Avoid accumulating too many tasks and exhaust RAM
        // Try to complete already finished jobs
        while( eErr == CE_None && !jobList.empty() )
        {
            auto poOldestJob = jobList.front().get();
            {
                std::lock_guard<std::mutex> oGuard(poOldestJob->mutex);
                if( !poOldestJob->bFinished )
                {
                    break;
                }
            }
            eErr = poOldestJob->eErr;
            if( eErr == CE_None )
            {
                eErr = WriteJobData(poOldestJob);
            }

            jobList.pop_front();
        }

        // And in case we have saturated the number of threads,
        // wait for completion of tasks to go below the threshold.
        while( eErr == CE_None &&
                jobList.size() >= static_cast<size_t>(nThreads) )
        {
            eErr = WaitAndFinalizeOldestJob(jobList);
        }

        // (Re)allocate buffers if needed
        if( pChunk == nullptr )
        {
            pChunk = VSI_MALLOC3_VERBOSE(
                GDALGetDataTypeSizeBytes(eWrkDataType), nMaxChunkYSizeQueried, nWidth );
        }
        if( bUseNoDataMask && pabyChunkNodataMask == nullptr )
        {
            pabyChunkNodataMask = static_cast<GByte*>(
                VSI_MALLOC2_VERBOSE( nMaxChunkYSizeQueried, nWidth ));
        }

        if( pChunk == nullptr || (bUseNoDataMask && pabyChunkNodataMask == nullptr))
        {
            CPLFree(pChunk);
            CPLFree(pabyChunkNodataMask);
            return CE_Failure;
        }

        // Read chunk.
        if( eErr == CE_None )
            eErr = poSrcBand->RasterIO(
                GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried,
                pChunk, nWidth, nChunkYSizeQueried, eWrkDataType,
                0, 0, nullptr );
        if( eErr == CE_None && bUseNoDataMask )
            eErr = poMaskBand->RasterIO(
                GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried,
                pabyChunkNodataMask, nWidth, nChunkYSizeQueried, GDT_Byte,
                0, 0, nullptr );

        // Special case to promote 1bit data to 8bit 0/255 values.
        if( EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE") )
        {
            if( eWrkDataType == GDT_Float32 )
            {
                float* pafChunk = static_cast<float*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if( eWrkDataType == GDT_Byte )
            {
                GByte* pabyChunk = static_cast<GByte*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 255;
                }
            }
            else if( eWrkDataType == GDT_UInt16 )
            {
                GUInt16* pasChunk = static_cast<GUInt16*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pasChunk[i] == 1 )
                        pasChunk[i] = 255;
                }
            }
            else {
                CPLAssert(false);
            }
        }
        else if( EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE_MINISWHITE") )
        {
            if( eWrkDataType == GDT_Float32 )
            {
                float* pafChunk = static_cast<float*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 0.0;
                    else if( pafChunk[i] == 0.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if( eWrkDataType == GDT_Byte )
            {
                GByte* pabyChunk = static_cast<GByte*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 0;
                    else if( pabyChunk[i] == 0 )
                        pabyChunk[i] = 255;
                }
            }
            else if( eWrkDataType == GDT_UInt16 )
            {
                GUInt16* pasChunk = static_cast<GUInt16*>(pChunk);
                for( GPtrDiff_t i = 0; i < static_cast<GPtrDiff_t>(nChunkYSizeQueried)*nWidth; i++)
                {
                    if( pasChunk[i] == 1 )
                        pasChunk[i] = 0;
                    else if( pasChunk[i] == 0 )
                        pasChunk[i] = 255;
                }
            }
            else {
                CPLAssert(false);
            }
        }

        std::shared_ptr<PointerHolder> oSrcBufferHolder(
            new PointerHolder(poJobQueue ? pChunk : nullptr));
        std::shared_ptr<PointerHolder> oSrcMaskBufferHolder(
            new PointerHolder(poJobQueue ? pabyChunkNodataMask : nullptr));

        for( int iOverview = 0;
             iOverview < nOverviewCount && eErr == CE_None;
             ++iOverview )
        {
            GDALRasterBand* poDstBand = papoOvrBands[iOverview];
            const int nDstWidth = poDstBand->GetXSize();
            const int nDstHeight = poDstBand->GetYSize();

            const double dfXRatioDstToSrc =
                static_cast<double>(nWidth) / nDstWidth;
            const double dfYRatioDstToSrc =
                static_cast<double>(nHeight) / nDstHeight;

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
            int nDstYOff = static_cast<int>(0.5 + nChunkYOff/dfYRatioDstToSrc);
            if( nDstYOff == nDstHeight )
                continue;
            int nDstYOff2 = static_cast<int>(
                0.5 + (nChunkYOff+nFullResYChunk)/dfYRatioDstToSrc);

            if( nChunkYOff + nFullResYChunk == nHeight )
                nDstYOff2 = nDstHeight;
#if DEBUG_VERBOSE
            CPLDebug(
                "GDAL",
                "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)",
                0, nChunkYOffQueried, nWidth, nChunkYSizeQueried,
                0, nDstYOff, nDstWidth, nDstYOff2 - nDstYOff );
#endif

            auto poJob = std::unique_ptr<OvrJob>(new OvrJob());
            poJob->pfnResampleFn = pfnResampleFn;
            poJob->dfXRatioDstToSrc = dfXRatioDstToSrc;
            poJob->dfYRatioDstToSrc = dfYRatioDstToSrc;
            poJob->eWrkDataType = eWrkDataType;
            poJob->pChunk = pChunk;
            poJob->pabyChunkNodataMask = pabyChunkNodataMask;
            poJob->nWidth = nWidth;
            poJob->nHeight = nHeight;
            poJob->nChunkYOff = nChunkYOffQueried;
            poJob->nChunkYSize = nChunkYSizeQueried;
            poJob->nDstWidth = nDstWidth;
            poJob->nDstYOff = nDstYOff;
            poJob->nDstYOff2 = nDstYOff2;
            poJob->poDstBand = poDstBand;
            poJob->pszResampling = pszResampling;
            poJob->bHasNoData = bHasNoData;
            poJob->fNoDataValue = fNoDataValue;
            poJob->poColorTable = poColorTable;
            poJob->eSrcDataType = eSrcDataType;
            poJob->bPropagateNoData = bPropagateNoData;

            if( poJobQueue )
            {
                poJob->oSrcMaskBufferHolder = oSrcMaskBufferHolder;
                poJob->oSrcBufferHolder = oSrcBufferHolder;
                poJobQueue->SubmitJob(JobResampleFunc, poJob.get());
                jobList.emplace_back(std::move(poJob));
            }
            else
            {
                JobResampleFunc(poJob.get());
                eErr = poJob->eErr;
                if( eErr == CE_None )
                {
                    eErr = WriteJobData(poJob.get());
                }
            }
        }

        if( poJobQueue )
        {
            pChunk = nullptr;
            pabyChunkNodataMask = nullptr;
        }
    }

    VSIFree( pChunk );
    VSIFree( pabyChunkNodataMask );

    // Wait for all pending jobs to complete
    while( eErr == CE_None && !jobList.empty() )
    {
        eErr = WaitAndFinalizeOldestJob(jobList);
    }

/* -------------------------------------------------------------------- */
/*      Renormalized overview mean / stddev if needed.                  */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && EQUAL(pszResampling,"AVERAGE_MP") )
    {
        GDALOverviewMagnitudeCorrection(
            poSrcBand,
            nOverviewCount,
            reinterpret_cast<GDALRasterBandH *>( papoOvrBands ),
            GDALDummyProgress, nullptr );
    }

/* -------------------------------------------------------------------- */
/*      It can be important to flush out data to overviews.             */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0;
         eErr == CE_None && iOverview < nOverviewCount;
         ++iOverview )
    {
        eErr = papoOvrBands[iOverview]->FlushCache();
    }

    if( eErr == CE_None )
        pfnProgress( 1.0, nullptr, pProgressData );

    return eErr;
}

/************************************************************************/
/*            GDALRegenerateOverviewsMultiBand()                        */
/************************************************************************/

/**
 * \brief Variant of GDALRegenerateOverviews, specially dedicated for generating
 * compressed pixel-interleaved overviews (JPEG-IN-TIFF for example)
 *
 * This function will generate one or more overview images from a base
 * image using the requested downsampling algorithm.  Its primary use
 * is for generating overviews via GDALDataset::BuildOverviews(), but it
 * can also be used to generate downsampled images in one file from another
 * outside the overview architecture.
 *
 * The output bands need to exist in advance and share the same characteristics
 * (type, dimensions)
 *
 * The resampling algorithms supported for the moment are "NEAREST", "AVERAGE",
 * "RMS", "GAUSS", "CUBIC", "CUBICSPLINE", "LANCZOS" and "BILINEAR"
 *
 * It does not support color tables or complex data types.
 *
 * The pseudo-algorithm used by the function is :
 *    for each overview
 *       iterate on lines of the source by a step of deltay
 *           iterate on columns of the source  by a step of deltax
 *               read the source data of size deltax * deltay for all the bands
 *               generate the corresponding overview block for all the bands
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset
 * metadata) so that only a given RGB triplet (in case of a RGB image) will be
 * considered as the nodata value and not each value of the triplet
 * independently per band.
 *
 * Starting with GDAL 3.2, the GDAL_NUM_THREADS configuration option can be set
 * to "ALL_CPUS" or a integer value to specify the number of threads to use for
 * overview computation.
 *
 * @param nBands the number of bands, size of papoSrcBands and size of
 *               first dimension of papapoOverviewBands
 * @param papoSrcBands the list of source bands to downsample
 * @param nOverviews the number of downsampled overview levels being generated.
 * @param papapoOverviewBands bidimension array of bands. First dimension is
 *                            indexed by nBands. Second dimension is indexed by
 *                            nOverviews.
 * @param pszResampling Resampling algorithm ("NEAREST", "AVERAGE", "RMS",
 * "GAUSS", "CUBIC", "CUBICSPLINE", "LANCZOS" or "BILINEAR").
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr
GDALRegenerateOverviewsMultiBand( int nBands, GDALRasterBand** papoSrcBands,
                                  int nOverviews,
                                  GDALRasterBand*** papapoOverviewBands,
                                  const char * pszResampling,
                                  GDALProgressFunc pfnProgress,
                                  void * pProgressData )
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    // Sanity checks.
    if( !STARTS_WITH_CI(pszResampling, "NEAR") &&
        !EQUAL(pszResampling, "RMS") &&
        !EQUAL(pszResampling, "AVERAGE") &&
        !EQUAL(pszResampling, "GAUSS") &&
        !EQUAL(pszResampling, "CUBIC") &&
        !EQUAL(pszResampling, "CUBICSPLINE") &&
        !EQUAL(pszResampling, "LANCZOS") &&
        !EQUAL(pszResampling, "BILINEAR") )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "GDALRegenerateOverviewsMultiBand: pszResampling='%s' "
            "not supported", pszResampling );
        return CE_Failure;
    }

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn =
        GDALGetResampleFunction(pszResampling, &nKernelRadius);
    if( pfnResampleFn == nullptr )
        return CE_Failure;

    const int nToplevelSrcWidth = papoSrcBands[0]->GetXSize();
    const int nToplevelSrcHeight = papoSrcBands[0]->GetYSize();
    GDALDataType eDataType = papoSrcBands[0]->GetRasterDataType();
    for( int iBand = 1; iBand < nBands; ++iBand )
    {
        if( papoSrcBands[iBand]->GetXSize() != nToplevelSrcWidth ||
            papoSrcBands[iBand]->GetYSize() != nToplevelSrcHeight )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same dimensions" );
            return CE_Failure;
        }
        if( papoSrcBands[iBand]->GetRasterDataType() != eDataType )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same data type" );
            return CE_Failure;
        }
    }

    for( int iOverview = 0; iOverview < nOverviews; ++iOverview )
    {
        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        const int nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();
        for( int iBand = 1; iBand < nBands; ++iBand )
        {
            if( papapoOverviewBands[iBand][iOverview]->GetXSize() !=
                nDstWidth ||
                papapoOverviewBands[iBand][iOverview]->GetYSize()
                != nDstHeight )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "of the same level must have the same dimensions" );
                return CE_Failure;
            }
            if( papapoOverviewBands[iBand][iOverview]->GetRasterDataType() !=
                eDataType )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "must have the same data type as the source bands" );
                return CE_Failure;
            }
        }
    }

    // First pass to compute the total number of pixels to read.
    double dfTotalPixelCount = 0;
    for( int iOverview = 0; iOverview < nOverviews; ++iOverview )
    {
        int nSrcWidth = nToplevelSrcWidth;
        int nSrcHeight = nToplevelSrcHeight;
        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        // Try to use previous level of overview as the source to compute
        // the next level.
        if( iOverview > 0 &&
            papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth )
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
        }

        dfTotalPixelCount += static_cast<double>(nSrcWidth) * nSrcHeight;
    }

    const GDALDataType eWrkDataType =
        GDALGetOvrWorkDataType(pszResampling, eDataType);

    // I'd wish we had a better way of conveying this !
    const bool bIsMask = CPLTestBool(CPLGetConfigOption(
                    "GDAL_REGENERATED_BAND_IS_MASK", "NO"));

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.
    const bool bUseNoDataMask =
        !STARTS_WITH_CI(pszResampling, "NEAR") &&
        (bIsMask || (papoSrcBands[0]->GetMaskFlags() & GMF_ALL_VALID) == 0);

    int* const pabHasNoData = static_cast<int *>(
        VSI_MALLOC_VERBOSE(nBands * sizeof(int)) );
    float* const pafNoDataValue = static_cast<float *>(
        VSI_MALLOC_VERBOSE(nBands * sizeof(float)) );
    if( pabHasNoData == nullptr || pafNoDataValue == nullptr )
    {
        CPLFree(pabHasNoData);
        CPLFree(pafNoDataValue);
        return CE_Failure;
    }

    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        pabHasNoData[iBand] = FALSE;
        pafNoDataValue[iBand] = static_cast<float>(
            papoSrcBands[iBand]->GetNoDataValue(&pabHasNoData[iBand]) );
    }
    const bool bPropagateNoData =
        CPLTestBool( CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO") );

    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");
    const int nThreads = std::max(1, std::min(128,
            EQUAL(pszThreads, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszThreads)));
    auto poThreadPool = nThreads > 1 ? GDALGetGlobalThreadPool(nThreads) : nullptr;
    auto poJobQueue = poThreadPool ? poThreadPool->CreateJobQueue() :
                            std::unique_ptr<CPLJobQueue>(nullptr);

    // Only configurable for debug / testing
    const int nChunkMaxSize =
        atoi(CPLGetConfigOption("GDAL_OVR_CHUNK_MAX_SIZE", "10485760"));

    // Second pass to do the real job.
    double dfCurPixelCount = 0;
    CPLErr eErr = CE_None;
    for( int iOverview = 0;
         iOverview < nOverviews && eErr == CE_None;
         ++iOverview )
    {
        int iSrcOverview = -1;  // -1 means the source bands.

        int nDstChunkXSize = 0;
        int nDstChunkYSize = 0;
        papapoOverviewBands[0][iOverview]->GetBlockSize( &nDstChunkXSize,
                                                         &nDstChunkYSize );

        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        const int nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();

        // Try to use previous level of overview as the source to compute
        // the next level.
        int nSrcWidth = nToplevelSrcWidth;
        int nSrcHeight = nToplevelSrcHeight;
        if( iOverview > 0 &&
            papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth )
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
            iSrcOverview = iOverview - 1;
        }

        const double dfXRatioDstToSrc =
            static_cast<double>(nSrcWidth) / nDstWidth;
        const double dfYRatioDstToSrc =
            static_cast<double>(nSrcHeight) / nDstHeight;

        int nOvrFactor = std::max( static_cast<int>(0.5 + dfXRatioDstToSrc),
                                   static_cast<int>(0.5 + dfYRatioDstToSrc) );
        if( nOvrFactor == 0 ) nOvrFactor = 1;

        // Try to extend the chunk size so that the memory needed to acquire
        // source pixels goes up to 10 MB.
        // This can help for drivers that support multi-threaded reading
        const int nFullResYChunk =
            2 + static_cast<int>(nDstChunkYSize * dfYRatioDstToSrc);
        const int nFullResYChunkQueried =
            nFullResYChunk + 2 * nKernelRadius * nOvrFactor;
        while( nDstChunkXSize < nDstWidth )
        {
            const int nFullResXChunk =
                2 + static_cast<int>(2 * nDstChunkXSize * dfXRatioDstToSrc);

            const int nFullResXChunkQueried =
                nFullResXChunk + 2 * nKernelRadius * nOvrFactor;

            if( static_cast<GIntBig>(nFullResXChunkQueried) *
                  nFullResYChunkQueried * nBands *
                    GDALGetDataTypeSizeBytes(eWrkDataType) > nChunkMaxSize )
            {
                break;
            }

            nDstChunkXSize *= 2;
        }
        nDstChunkXSize = std::min(nDstChunkXSize, nDstWidth);

        const int nFullResXChunk =
            2 + static_cast<int>(nDstChunkXSize * dfXRatioDstToSrc);
        const int nFullResXChunkQueried =
            nFullResXChunk + 2 * nKernelRadius * nOvrFactor;

        // Structure describing a resampling job
        struct OvrJob
        {
            // Buffers to free when job is finished
            std::shared_ptr<PointerHolder> oSrcMaskBufferHolder{};
            std::unique_ptr<PointerHolder> oSrcBufferHolder{};
            std::unique_ptr<PointerHolder> oDstBufferHolder{};

            // Input parameters of pfnResampleFn
            GDALResampleFunction pfnResampleFn = nullptr;
            double dfXRatioDstToSrc{};
            double dfYRatioDstToSrc{};
            GDALDataType eWrkDataType = GDT_Unknown;
            const void * pChunk = nullptr;
            const GByte * pabyChunkNodataMask = nullptr;
            int nChunkXOff = 0;
            int nChunkXSize = 0;
            int nChunkYOff = 0;
            int nChunkYSize = 0;
            int nDstXOff = 0;
            int nDstXOff2 = 0;
            int nDstYOff = 0;
            int nDstYOff2 = 0;
            GDALRasterBand* poOverview = nullptr;
            const char * pszResampling = nullptr;
            int bHasNoData = 0;
            float fNoDataValue = 0.0f;
            GDALDataType eSrcDataType = GDT_Unknown;
            bool bPropagateNoData = false;

            // Output values of resampling function
            CPLErr eErr = CE_Failure;
            void* pDstBuffer = nullptr;
            GDALDataType eDstBufferDataType = GDT_Unknown;

            // Synchronization
            bool                    bFinished = false;
            std::mutex              mutex{};
            std::condition_variable cv{};
        };

        // Thread function to resample
        const auto JobResampleFunc = [](void* pData)
        {
            OvrJob* poJob = static_cast<OvrJob*>(pData);

            poJob->eErr = poJob->pfnResampleFn(
                poJob->dfXRatioDstToSrc,
                poJob->dfYRatioDstToSrc,
                0.0, 0.0,
                poJob->eWrkDataType,
                poJob->pChunk,
                poJob->pabyChunkNodataMask,
                poJob->nChunkXOff,
                poJob->nChunkXSize,
                poJob->nChunkYOff,
                poJob->nChunkYSize,
                poJob->nDstXOff,
                poJob->nDstXOff2,
                poJob->nDstYOff,
                poJob->nDstYOff2,
                poJob->poOverview,
                &(poJob->pDstBuffer),
                &(poJob->eDstBufferDataType),
                poJob->pszResampling,
                poJob->bHasNoData,
                poJob->fNoDataValue,
                nullptr,
                poJob->eSrcDataType,
                poJob->bPropagateNoData);

            poJob->oDstBufferHolder.reset(new PointerHolder(poJob->pDstBuffer));

            {
                std::lock_guard<std::mutex> guard(poJob->mutex);
                poJob->bFinished = true;
                poJob->cv.notify_one();
            }
        };

        // Function to write resample data to target band
        const auto WriteJobData = [](const OvrJob* poJob)
        {
            return poJob->poOverview->RasterIO(
                                GF_Write,
                                poJob->nDstXOff,
                                poJob->nDstYOff,
                                poJob->nDstXOff2 - poJob->nDstXOff,
                                poJob->nDstYOff2 - poJob->nDstYOff,
                                poJob->pDstBuffer,
                                poJob->nDstXOff2 - poJob->nDstXOff,
                                poJob->nDstYOff2 - poJob->nDstYOff,
                                poJob->eDstBufferDataType,
                                0, 0, nullptr );
        };

        // Wait for completion of oldest job and serialize it
        const auto WaitAndFinalizeOldestJob = [WriteJobData](
                            std::list<std::unique_ptr<OvrJob>>& jobList)
        {
            auto poOldestJob = jobList.front().get();
            {
                std::unique_lock<std::mutex> oGuard(poOldestJob->mutex);
                while( !poOldestJob->bFinished )
                {
                    poOldestJob->cv.wait(oGuard);
                }
            }
            CPLErr l_eErr = poOldestJob->eErr;
            if( l_eErr == CE_None )
            {
                l_eErr = WriteJobData(poOldestJob);
            }

            jobList.pop_front();
            return l_eErr;
        };

        // Queue of jobs
        std::list<std::unique_ptr<OvrJob>> jobList;

        std::vector<void*> apaChunk(nBands);
        GByte* pabyChunkNoDataMask = nullptr;

        int nDstYOff = 0;
        // Iterate on destination overview, block by block.
        for( nDstYOff = 0;
             nDstYOff < nDstHeight && eErr == CE_None;
             nDstYOff += nDstChunkYSize )
        {
            int nDstYCount;
            if( nDstYOff + nDstChunkYSize <= nDstHeight )
                nDstYCount = nDstChunkYSize;
            else
                nDstYCount = nDstHeight - nDstYOff;

            int nChunkYOff =
                static_cast<int>(nDstYOff * dfYRatioDstToSrc);
            int nChunkYOff2 =
                static_cast<int>(
                    ceil((nDstYOff + nDstYCount) * dfYRatioDstToSrc) );
            if( nChunkYOff2 > nSrcHeight || nDstYOff + nDstYCount == nDstHeight)
                nChunkYOff2 = nSrcHeight;
            int nYCount = nChunkYOff2 - nChunkYOff;
            CPLAssert(nYCount <= nFullResYChunk);

            int nChunkYOffQueried = nChunkYOff - nKernelRadius * nOvrFactor;
            int nChunkYSizeQueried = nYCount + 2 * nKernelRadius * nOvrFactor;
            if( nChunkYOffQueried < 0 )
            {
                nChunkYSizeQueried += nChunkYOffQueried;
                nChunkYOffQueried = 0;
            }
            if( nChunkYSizeQueried + nChunkYOffQueried > nSrcHeight )
                nChunkYSizeQueried = nSrcHeight - nChunkYOffQueried;
            CPLAssert(nChunkYSizeQueried <= nFullResYChunkQueried);

            if( !pfnProgress( dfCurPixelCount / dfTotalPixelCount,
                              nullptr, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }

            int nDstXOff = 0;
            // Iterate on destination overview, block by block.
            for( nDstXOff = 0;
                 nDstXOff < nDstWidth && eErr == CE_None;
                 nDstXOff += nDstChunkXSize )
            {
                int nDstXCount = 0;
                if( nDstXOff + nDstChunkXSize <= nDstWidth )
                    nDstXCount = nDstChunkXSize;
                else
                    nDstXCount = nDstWidth - nDstXOff;

                int nChunkXOff =
                    static_cast<int>(nDstXOff * dfXRatioDstToSrc);
                int nChunkXOff2 =
                    static_cast<int>(
                        ceil((nDstXOff + nDstXCount) * dfXRatioDstToSrc) );
                if( nChunkXOff2 > nSrcWidth ||
                    nDstXOff + nDstXCount == nDstWidth )
                    nChunkXOff2 = nSrcWidth;
                const int nXCount = nChunkXOff2 - nChunkXOff;
                CPLAssert(nXCount <= nFullResXChunk);

                int nChunkXOffQueried = nChunkXOff - nKernelRadius * nOvrFactor;
                int nChunkXSizeQueried =
                    nXCount + 2 * nKernelRadius * nOvrFactor;
                if( nChunkXOffQueried < 0 )
                {
                    nChunkXSizeQueried += nChunkXOffQueried;
                    nChunkXOffQueried = 0;
                }
                if( nChunkXSizeQueried + nChunkXOffQueried > nSrcWidth )
                    nChunkXSizeQueried = nSrcWidth - nChunkXOffQueried;
                CPLAssert(nChunkXSizeQueried <= nFullResXChunkQueried);
#if DEBUG_VERBOSE
                CPLDebug(
                    "GDAL",
                    "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)",
                    nChunkXOffQueried, nChunkYOffQueried, nChunkXSizeQueried, nChunkYSizeQueried,
                    nDstXOff, nDstYOff, nDstXCount, nDstYCount );
#endif

                // Avoid accumulating too many tasks and exhaust RAM

                // Try to complete already finished jobs
                while( eErr == CE_None && !jobList.empty() )
                {
                    auto poOldestJob = jobList.front().get();
                    {
                        std::lock_guard<std::mutex> oGuard(poOldestJob->mutex);
                        if( !poOldestJob->bFinished )
                        {
                            break;
                        }
                    }
                    eErr = poOldestJob->eErr;
                    if( eErr == CE_None )
                    {
                        eErr = WriteJobData(poOldestJob);
                    }

                    jobList.pop_front();
                }

                // And in case we have saturated the number of threads,
                // wait for completion of tasks to go below the threshold.
                while( eErr == CE_None &&
                       jobList.size() >= static_cast<size_t>(nThreads) )
                {
                    eErr = WaitAndFinalizeOldestJob(jobList);
                }

                // (Re)allocate buffers if needed
                for( int iBand = 0; iBand < nBands; ++iBand )
                {
                    if( apaChunk[iBand] == nullptr )
                    {
                        apaChunk[iBand] = VSI_MALLOC3_VERBOSE(
                            nFullResXChunkQueried,
                            nFullResYChunkQueried,
                            GDALGetDataTypeSizeBytes(eWrkDataType) );
                        if( apaChunk[iBand] == nullptr )
                        {
                            eErr = CE_Failure;
                        }
                    }
                }
                if( bUseNoDataMask && pabyChunkNoDataMask == nullptr  )
                {
                    pabyChunkNoDataMask = static_cast<GByte *>(
                        VSI_MALLOC2_VERBOSE( nFullResXChunkQueried,
                                            nFullResYChunkQueried ) );
                    if( pabyChunkNoDataMask == nullptr )
                    {
                        eErr = CE_Failure;
                    }
                }

                // Read the source buffers for all the bands.
                for( int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand )
                {
                    GDALRasterBand* poSrcBand = nullptr;
                    if( iSrcOverview == -1 )
                        poSrcBand = papoSrcBands[iBand];
                    else
                        poSrcBand = papapoOverviewBands[iBand][iSrcOverview];
                    eErr = poSrcBand->RasterIO(
                        GF_Read,
                        nChunkXOffQueried, nChunkYOffQueried,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        apaChunk[iBand],
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        eWrkDataType, 0, 0, nullptr );
                }

                if( bUseNoDataMask && eErr == CE_None )
                {
                    GDALRasterBand* poSrcBand = nullptr;
                    if( iSrcOverview == -1 )
                        poSrcBand = papoSrcBands[0];
                    else
                        poSrcBand = papapoOverviewBands[0][iSrcOverview];
                    auto poMaskBand = bIsMask ? poSrcBand : poSrcBand->GetMaskBand();
                    eErr = poMaskBand->RasterIO(
                        GF_Read,
                        nChunkXOffQueried, nChunkYOffQueried,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        pabyChunkNoDataMask,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        GDT_Byte, 0, 0, nullptr );
                }

                std::shared_ptr<PointerHolder> oSrcMaskBufferHolder(
                    new PointerHolder(poJobQueue ? pabyChunkNoDataMask : nullptr));

                // Compute the resulting overview block.
                for( int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand )
                {
                    auto poJob = std::unique_ptr<OvrJob>(new OvrJob());
                    poJob->pfnResampleFn = pfnResampleFn;
                    poJob->dfXRatioDstToSrc = dfXRatioDstToSrc;
                    poJob->dfYRatioDstToSrc = dfYRatioDstToSrc;
                    poJob->eWrkDataType = eWrkDataType;
                    poJob->pChunk = apaChunk[iBand];
                    poJob->pabyChunkNodataMask = pabyChunkNoDataMask;
                    poJob->nChunkXOff = nChunkXOffQueried;
                    poJob->nChunkXSize = nChunkXSizeQueried;
                    poJob->nChunkYOff = nChunkYOffQueried;
                    poJob->nChunkYSize = nChunkYSizeQueried;
                    poJob->nDstXOff = nDstXOff;
                    poJob->nDstXOff2 = nDstXOff + nDstXCount;
                    poJob->nDstYOff = nDstYOff;
                    poJob->nDstYOff2 = nDstYOff + nDstYCount;
                    poJob->poOverview = papapoOverviewBands[iBand][iOverview];
                    poJob->pszResampling = pszResampling;
                    poJob->bHasNoData = pabHasNoData[iBand];
                    poJob->fNoDataValue = pafNoDataValue[iBand];
                    poJob->eSrcDataType = eDataType;
                    poJob->bPropagateNoData = bPropagateNoData;

                    if( poJobQueue )
                    {
                        poJob->oSrcMaskBufferHolder = oSrcMaskBufferHolder;
                        poJob->oSrcBufferHolder.reset(
                            new PointerHolder(apaChunk[iBand]));
                        apaChunk[iBand] = nullptr;

                        poJobQueue->SubmitJob(JobResampleFunc, poJob.get());
                        jobList.emplace_back(std::move(poJob));
                    }
                    else
                    {
                        JobResampleFunc(poJob.get());
                        eErr = poJob->eErr;
                        if( eErr == CE_None )
                        {
                            eErr = WriteJobData(poJob.get());
                        }
                    }
                }

                if( poJobQueue )
                    pabyChunkNoDataMask = nullptr;
            }

            dfCurPixelCount += static_cast<double>(nYCount) * nSrcWidth;
        }

        // Wait for all pending jobs to complete
        while( eErr == CE_None && !jobList.empty() )
        {
            eErr = WaitAndFinalizeOldestJob(jobList);
        }

        // Flush the data to overviews.
        for( int iBand = 0; iBand < nBands; ++iBand )
        {
            CPLFree(apaChunk[iBand]);
            papapoOverviewBands[iBand][iOverview]->FlushCache();
        }
        CPLFree(pabyChunkNoDataMask);
    }

    CPLFree(pabHasNoData);
    CPLFree(pafNoDataValue);

    if( eErr == CE_None )
        pfnProgress( 1.0, nullptr, pProgressData );

    return eErr;
}

/************************************************************************/
/*                        GDALComputeBandStats()                        */
/************************************************************************/

/** Undocumented
 * @param hSrcBand undocumented.
 * @param nSampleStep undocumented.
 * @param pdfMean undocumented.
 * @param pdfStdDev undocumented.
 * @param pfnProgress undocumented.
 * @param pProgressData undocumented.
 * @return undocumented
 */
CPLErr CPL_STDCALL
GDALComputeBandStats( GDALRasterBandH hSrcBand,
                      int nSampleStep,
                      double *pdfMean, double *pdfStdDev,
                      GDALProgressFunc pfnProgress,
                      void *pProgressData )

{
    VALIDATE_POINTER1( hSrcBand, "GDALComputeBandStats", CE_Failure );

    GDALRasterBand *poSrcBand = GDALRasterBand::FromHandle( hSrcBand );

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    if( nSampleStep >= nHeight || nSampleStep < 1 )
        nSampleStep = 1;

    GDALDataType eWrkType = GDT_Unknown;
    float *pafData = nullptr;
    GDALDataType eType = poSrcBand->GetRasterDataType();
    const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
    if( bComplex )
    {
        pafData = static_cast<float *>(
            VSI_MALLOC_VERBOSE(nWidth * 2 * sizeof(float)) );
        eWrkType = GDT_CFloat32;
    }
    else
    {
        pafData = static_cast<float *>(
            VSI_MALLOC_VERBOSE(nWidth * sizeof(float)) );
        eWrkType = GDT_Float32;
    }

    if( nWidth == 0 || pafData == nullptr )
    {
        VSIFree(pafData);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Loop over all sample lines.                                     */
/* -------------------------------------------------------------------- */
    double dfSum = 0.0;
    double dfSum2 = 0.0;
    int iLine = 0;
    GIntBig nSamples = 0;

    do
    {
        if( !pfnProgress( iLine / static_cast<double>( nHeight ),
                          nullptr, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }

        const CPLErr eErr =
            poSrcBand->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                                 pafData, nWidth, 1, eWrkType,
                                 0, 0, nullptr );
        if( eErr != CE_None )
        {
            CPLFree( pafData );
            return eErr;
        }

        for( int iPixel = 0; iPixel < nWidth; ++iPixel )
        {
            float fValue = 0.0f;

            if( bComplex )
            {
                // Compute the magnitude of the complex value.
                fValue = std::hypot(pafData[iPixel*2],
                                    pafData[iPixel*2+1]);
            }
            else
            {
                fValue = pafData[iPixel];
            }

            dfSum  += fValue;
            dfSum2 += fValue * fValue;
        }

        nSamples += nWidth;
        iLine += nSampleStep;
    } while( iLine < nHeight );

    if( !pfnProgress( 1.0, nullptr, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        CPLFree( pafData );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Produce the result values.                                      */
/* -------------------------------------------------------------------- */
    if( pdfMean != nullptr )
        *pdfMean = dfSum / nSamples;

    if( pdfStdDev != nullptr )
    {
        const double dfMean = dfSum / nSamples;

        *pdfStdDev = sqrt((dfSum2 / nSamples) - (dfMean * dfMean));
    }

    CPLFree( pafData );

    return CE_None;
}

/************************************************************************/
/*                  GDALOverviewMagnitudeCorrection()                   */
/*                                                                      */
/*      Correct the mean and standard deviation of the overviews of     */
/*      the given band to match the base layer approximately.           */
/************************************************************************/

/** Undocumented
 * @param hBaseBand undocumented.
 * @param nOverviewCount undocumented.
 * @param pahOverviews undocumented.
 * @param pfnProgress undocumented.
 * @param pProgressData undocumented.
 * @return undocumented
 */
CPLErr
GDALOverviewMagnitudeCorrection( GDALRasterBandH hBaseBand,
                                 int nOverviewCount,
                                 GDALRasterBandH *pahOverviews,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData )

{
    VALIDATE_POINTER1( hBaseBand, "GDALOverviewMagnitudeCorrection",
                       CE_Failure );

/* -------------------------------------------------------------------- */
/*      Compute mean/stddev for source raster.                          */
/* -------------------------------------------------------------------- */
    double dfOrigMean = 0.0;
    double dfOrigStdDev = 0.0;
    {
        const CPLErr eErr
            = GDALComputeBandStats( hBaseBand, 2, &dfOrigMean, &dfOrigStdDev,
                                    pfnProgress, pProgressData );

        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Loop on overview bands.                                         */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; iOverview < nOverviewCount; ++iOverview )
    {
        GDALRasterBand *poOverview = GDALRasterBand::FromHandle(
            pahOverviews[iOverview]);
        double  dfOverviewMean, dfOverviewStdDev;

        const CPLErr eErr =
            GDALComputeBandStats( pahOverviews[iOverview], 1,
                                  &dfOverviewMean, &dfOverviewStdDev,
                                  pfnProgress, pProgressData );

        if( eErr != CE_None )
            return eErr;

        double dfGain = 1.0;
        if( dfOrigStdDev >= 0.0001 )
            dfGain = dfOrigStdDev / dfOverviewStdDev;

/* -------------------------------------------------------------------- */
/*      Apply gain and offset.                                          */
/* -------------------------------------------------------------------- */
        const int nWidth = poOverview->GetXSize();
        const int nHeight = poOverview->GetYSize();

        GDALDataType eWrkType = GDT_Unknown;
        float *pafData = nullptr;
        const GDALDataType eType = poOverview->GetRasterDataType();
        const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
        if( bComplex )
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, 2 * sizeof(float)) );
            eWrkType = GDT_CFloat32;
        }
        else
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, sizeof(float)) );
            eWrkType = GDT_Float32;
        }

        if( pafData == nullptr )
        {
            return CE_Failure;
        }

        for( int iLine = 0; iLine < nHeight; ++iLine )
        {
            if( !pfnProgress( iLine / static_cast<double>( nHeight ),
                              nullptr, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                CPLFree( pafData );
                return CE_Failure;
            }

            if( poOverview->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                                      pafData, nWidth, 1, eWrkType,
                                      0, 0, nullptr ) != CE_None )
            {
                CPLFree( pafData );
                return CE_Failure;
            }

            for( int iPixel = 0; iPixel < nWidth; ++iPixel )
            {
                if( bComplex )
                {
                    pafData[iPixel*2] *= static_cast<float>( dfGain );
                    pafData[iPixel*2+1] *= static_cast<float>( dfGain );
                }
                else
                {
                    pafData[iPixel] = static_cast<float>(
                        (pafData[iPixel] - dfOverviewMean)
                        * dfGain + dfOrigMean );
                }
            }

            if( poOverview->RasterIO( GF_Write, 0, iLine, nWidth, 1,
                                      pafData, nWidth, 1, eWrkType,
                                      0, 0, nullptr ) != CE_None )
            {
                CPLFree( pafData );
                return CE_Failure;
            }
        }

        if( !pfnProgress( 1.0, nullptr, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }

        CPLFree( pafData );
    }

    return CE_None;
}
