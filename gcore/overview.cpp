
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
 * SPDX-License-Identifier: MIT
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
#include "cpl_float.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "cpl_worker_thread_pool.h"
#include "gdal.h"
#include "gdal_thread_pool.h"
#include "gdalwarper.h"
#include "gdal_vrt.h"
#include "vrtdataset.h"

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"

#if (!defined(__aarch64__) && !defined(_M_ARM64))
#define ARM_V7
#endif

#define USE_SSE2

#include "gdalsse_priv.h"

// Restrict to 64bit processors because they are guaranteed to have SSE2,
// or if __AVX2__ is defined.
#elif defined(__x86_64) || defined(_M_X64) || defined(__AVX2__)
#define USE_SSE2

#include "gdalsse_priv.h"

#ifdef __SSE3__
#include <pmmintrin.h>
#endif
#ifdef __SSSE3__
#include <tmmintrin.h>
#endif
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif

#endif

// To be included after above USE_SSE2 and include gdalsse_priv.h
// to avoid build issue on Windows x86
#include "gdal_priv_templates.hpp"

/************************************************************************/
/*                       GDALResampleChunk_Near()                       */
/************************************************************************/

template <class T>
static CPLErr GDALResampleChunk_NearT(const GDALOverviewResampleArgs &args,
                                      const T *pChunk, T **ppDstBuffer)

{
    const double dfXRatioDstToSrc = args.dfXRatioDstToSrc;
    const double dfYRatioDstToSrc = args.dfYRatioDstToSrc;
    const GDALDataType eWrkDataType = args.eWrkDataType;
    const int nChunkXOff = args.nChunkXOff;
    const int nChunkXSize = args.nChunkXSize;
    const int nChunkYOff = args.nChunkYOff;
    const int nDstXOff = args.nDstXOff;
    const int nDstXOff2 = args.nDstXOff2;
    const int nDstYOff = args.nDstYOff;
    const int nDstYOff2 = args.nDstYOff2;
    const int nDstXWidth = nDstXOff2 - nDstXOff;

    /* -------------------------------------------------------------------- */
    /*      Allocate buffers.                                               */
    /* -------------------------------------------------------------------- */
    *ppDstBuffer = static_cast<T *>(
        VSI_MALLOC3_VERBOSE(nDstXWidth, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(eWrkDataType)));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }
    T *const pDstBuffer = *ppDstBuffer;

    int *panSrcXOff =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(nDstXWidth, sizeof(int)));

    if (panSrcXOff == nullptr)
    {
        return CE_Failure;
    }

    /* ==================================================================== */
    /*      Precompute inner loop constants.                                */
    /* ==================================================================== */
    for (int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel)
    {
        int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
        if (nSrcXOff < nChunkXOff)
            nSrcXOff = nChunkXOff;

        panSrcXOff[iDstPixel - nDstXOff] = nSrcXOff;
    }

    /* ==================================================================== */
    /*      Loop over destination scanlines.                                */
    /* ==================================================================== */
    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if (nSrcYOff < nChunkYOff)
            nSrcYOff = nChunkYOff;

        const T *const pSrcScanline =
            pChunk +
            (static_cast<size_t>(nSrcYOff - nChunkYOff) * nChunkXSize) -
            nChunkXOff;

        /* --------------------------------------------------------------------
         */
        /*      Loop over destination pixels */
        /* --------------------------------------------------------------------
         */
        T *pDstScanline =
            pDstBuffer + static_cast<size_t>(iDstLine - nDstYOff) * nDstXWidth;
        for (int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel)
        {
            pDstScanline[iDstPixel] = pSrcScanline[panSrcXOff[iDstPixel]];
        }
    }

    CPLFree(panSrcXOff);

    return CE_None;
}

static CPLErr GDALResampleChunk_Near(const GDALOverviewResampleArgs &args,
                                     const void *pChunk, void **ppDstBuffer,
                                     GDALDataType *peDstBufferDataType)
{
    *peDstBufferDataType = args.eWrkDataType;
    switch (args.eWrkDataType)
    {
        // For nearest resampling, as no computation is done, only the
        // size of the data type matters.
        case GDT_UInt8:
        case GDT_Int8:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 1);
            return GDALResampleChunk_NearT(
                args, static_cast<const uint8_t *>(pChunk),
                reinterpret_cast<uint8_t **>(ppDstBuffer));
        }

        case GDT_Int16:
        case GDT_UInt16:
        case GDT_Float16:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 2);
            return GDALResampleChunk_NearT(
                args, static_cast<const uint16_t *>(pChunk),
                reinterpret_cast<uint16_t **>(ppDstBuffer));
        }

        case GDT_CInt16:
        case GDT_CFloat16:
        case GDT_Int32:
        case GDT_UInt32:
        case GDT_Float32:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 4);
            return GDALResampleChunk_NearT(
                args, static_cast<const uint32_t *>(pChunk),
                reinterpret_cast<uint32_t **>(ppDstBuffer));
        }

        case GDT_CInt32:
        case GDT_CFloat32:
        case GDT_Int64:
        case GDT_UInt64:
        case GDT_Float64:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 8);
            return GDALResampleChunk_NearT(
                args, static_cast<const uint64_t *>(pChunk),
                reinterpret_cast<uint64_t **>(ppDstBuffer));
        }

        case GDT_CFloat64:
        {
            return GDALResampleChunk_NearT(
                args, static_cast<const std::complex<double> *>(pChunk),
                reinterpret_cast<std::complex<double> **>(ppDstBuffer));
        }

        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
    CPLAssert(false);
    return CE_Failure;
}

namespace
{

// Find in the color table the entry whose RGB value is the closest
// (using quadratic distance) to the test color, ignoring transparent entries.
int BestColorEntry(const std::vector<GDALColorEntry> &entries,
                   const GDALColorEntry &test)
{
    int nMinDist = std::numeric_limits<int>::max();
    size_t bestEntry = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const GDALColorEntry &entry = entries[i];
        // Ignore transparent entries
        if (entry.c4 == 0)
            continue;

        int nDist = ((test.c1 - entry.c1) * (test.c1 - entry.c1)) +
                    ((test.c2 - entry.c2) * (test.c2 - entry.c2)) +
                    ((test.c3 - entry.c3) * (test.c3 - entry.c3));
        if (nDist < nMinDist)
        {
            nMinDist = nDist;
            bestEntry = i;
        }
    }
    return static_cast<int>(bestEntry);
}

std::vector<GDALColorEntry> ReadColorTable(const GDALColorTable &table,
                                           int &transparentIdx)
{
    std::vector<GDALColorEntry> entries(table.GetColorEntryCount());

    transparentIdx = -1;
    int i = 0;
    for (auto &entry : entries)
    {
        table.GetColorEntryAsRGB(i, &entry);
        if (transparentIdx < 0 && entry.c4 == 0)
            transparentIdx = i;
        ++i;
    }
    return entries;
}

}  // unnamed  namespace

/************************************************************************/
/*                               SQUARE()                               */
/************************************************************************/

template <class T, class Tsquare = T> inline Tsquare SQUARE(T val)
{
    return static_cast<Tsquare>(val) * val;
}

/************************************************************************/
/*                         ComputeIntegerRMS()                          */
/************************************************************************/
// Compute rms = sqrt(sumSquares / weight) in such a way that it is the
// integer that minimizes abs(rms**2 - sumSquares / weight)
template <class T, class Twork>
inline T ComputeIntegerRMS(double sumSquares, double weight)
{
    const double sumDivWeight = sumSquares / weight;
    T rms = static_cast<T>(sqrt(sumDivWeight));

    // Is rms**2 or (rms+1)**2 closest to sumSquares / weight ?
    // Naive version:
    // if( weight * (rms+1)**2 - sumSquares < sumSquares - weight * rms**2 )
    if (static_cast<double>(static_cast<Twork>(2) * rms * (rms + 1) + 1) <
        2 * sumDivWeight)
        rms += 1;
    return rms;
}

template <class T, class Tsum> inline T ComputeIntegerRMS_4values(Tsum)
{
    CPLAssert(false);
    return 0;
}

template <> inline GByte ComputeIntegerRMS_4values<GByte, int>(int sumSquares)
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
    if (static_cast<int>(rms) * (rms + 1) < sumSquaresPlusOneDiv4)
        rms += 1;
    return rms;
}

template <>
inline GUInt16 ComputeIntegerRMS_4values<GUInt16, double>(double sumSquares)
{
    const double sumDivWeight = sumSquares * 0.25;
    GUInt16 rms = static_cast<GUInt16>(std::sqrt(sumDivWeight));

    // Is rms**2 or (rms+1)**2 closest to sumSquares / weight ?
    // Naive version:
    // if( weight * (rms+1)**2 - sumSquares < sumSquares - weight * rms**2 )
    // Optimized version for integer case and weight == 4
    if (static_cast<GUInt32>(rms) * (rms + 1) <
        static_cast<GUInt32>(sumDivWeight + 0.25))
        rms += 1;
    return rms;
}

#ifdef USE_SSE2

/************************************************************************/
/*                    QuadraticMeanByteSSE2OrAVX2()                     */
/************************************************************************/

#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
#define sse2_packus_epi32 _mm_packus_epi32
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

#if defined(__SSSE3__) || defined(USE_NEON_OPTIMIZATIONS)
#define sse2_hadd_epi16 _mm_hadd_epi16
#else
inline __m128i sse2_hadd_epi16(__m128i a, __m128i b)
{
    // Horizontal addition of adjacent pairs
    const auto mask = _mm_set1_epi32(0xFFFF);
    const auto horizLo =
        _mm_add_epi32(_mm_and_si128(a, mask), _mm_srli_epi32(a, 16));
    const auto horizHi =
        _mm_add_epi32(_mm_and_si128(b, mask), _mm_srli_epi32(b, 16));

    // Recombine low and high parts
    return _mm_packs_epi32(horizLo, horizHi);
}
#endif

#ifdef __AVX2__

#define set1_epi16 _mm256_set1_epi16
#define set1_epi32 _mm256_set1_epi32
#define setzero _mm256_setzero_si256
#define set1_ps _mm256_set1_ps
#define loadu_int(x) _mm256_loadu_si256(reinterpret_cast<__m256i const *>(x))
#define unpacklo_epi8 _mm256_unpacklo_epi8
#define unpackhi_epi8 _mm256_unpackhi_epi8
#define madd_epi16 _mm256_madd_epi16
#define add_epi32 _mm256_add_epi32
#define mul_ps _mm256_mul_ps
#define cvtepi32_ps _mm256_cvtepi32_ps
#define sqrt_ps _mm256_sqrt_ps
#define cvttps_epi32 _mm256_cvttps_epi32
#define packs_epi32 _mm256_packs_epi32
#define packus_epi32 _mm256_packus_epi32
#define srli_epi32 _mm256_srli_epi32
#define mullo_epi16 _mm256_mullo_epi16
#define srli_epi16 _mm256_srli_epi16
#define cmpgt_epi16 _mm256_cmpgt_epi16
#define add_epi16 _mm256_add_epi16
#define sub_epi16 _mm256_sub_epi16
#define packus_epi16 _mm256_packus_epi16

/* AVX2 operates on 2 separate 128-bit lanes, so we have to do shuffling */
/* to get the lower 128-bit bits of what would be a true 256-bit vector register
 */

inline __m256i FIXUP_LANES(__m256i x)
{
    return _mm256_permute4x64_epi64(x, _MM_SHUFFLE(3, 1, 2, 0));
}

#define store_lo(x, y)                                                         \
    _mm_storeu_si128(reinterpret_cast<__m128i *>(x),                           \
                     _mm256_extracti128_si256(FIXUP_LANES(y), 0))
#define storeu_int(x, y)                                                       \
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(x), FIXUP_LANES(y))
#define hadd_epi16 _mm256_hadd_epi16
#else
#define set1_epi16 _mm_set1_epi16
#define set1_epi32 _mm_set1_epi32
#define setzero _mm_setzero_si128
#define set1_ps _mm_set1_ps
#define loadu_int(x) _mm_loadu_si128(reinterpret_cast<__m128i const *>(x))
#define unpacklo_epi8 _mm_unpacklo_epi8
#define unpackhi_epi8 _mm_unpackhi_epi8
#define madd_epi16 _mm_madd_epi16
#define add_epi32 _mm_add_epi32
#define mul_ps _mm_mul_ps
#define cvtepi32_ps _mm_cvtepi32_ps
#define sqrt_ps _mm_sqrt_ps
#define cvttps_epi32 _mm_cvttps_epi32
#define packs_epi32 _mm_packs_epi32
#define packus_epi32 sse2_packus_epi32
#define srli_epi32 _mm_srli_epi32
#define mullo_epi16 _mm_mullo_epi16
#define srli_epi16 _mm_srli_epi16
#define cmpgt_epi16 _mm_cmpgt_epi16
#define add_epi16 _mm_add_epi16
#define sub_epi16 _mm_sub_epi16
#define packus_epi16 _mm_packus_epi16
#define store_lo(x, y) _mm_storel_epi64(reinterpret_cast<__m128i *>(x), (y))
#define storeu_int(x, y) _mm_storeu_si128(reinterpret_cast<__m128i *>(x), (y))
#define hadd_epi16 sse2_hadd_epi16
#endif

template <class T>
static int
#if defined(__GNUC__)
    __attribute__((noinline))
#endif
    QuadraticMeanByteSSE2OrAVX2(int nDstXWidth, int nChunkXSize,
                                const T *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                                T *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on Byte by
    // processing by group of 8 output pixels, so as to use
    // a single _mm_sqrt_ps() call for 4 output pixels
    const T *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto one16 = set1_epi16(1);
    const auto one32 = set1_epi32(1);
    const auto zero = setzero();
    const auto minus32768 = set1_epi16(-32768);

    constexpr int DEST_ELTS = static_cast<int>(sizeof(zero)) / 2;
    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
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
        const auto rmsLo =
            cvttps_epi32(sqrt_ps(cvtepi32_ps(sumSquaresPlusOneDiv4Lo)));
        const auto rmsHi =
            cvttps_epi32(sqrt_ps(cvtepi32_ps(sumSquaresPlusOneDiv4Hi)));

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
        const auto sumSquaresPlusOneDiv4 =
            packus_epi32(sumSquaresPlusOneDiv4Lo, sumSquaresPlusOneDiv4Hi);
        // cmpgt_epi16 operates on signed int16, but here
        // we have unsigned values, so shift them by -32768 before
        const auto mask = cmpgt_epi16(
            add_epi16(sumSquaresPlusOneDiv4, minus32768),
            add_epi16(mullo_epi16(rms, add_epi16(rms, one16)), minus32768));
        // The value of the mask will be -1 when the correction needs to be
        // applied
        rms = sub_epi16(rms, mask);

        // Pack each 16 bit RMS value to 8 bits
        rms = packus_epi16(rms, rms /* could be anything */);
        store_lo(&pDstScanline[iDstPixel], rms);
        pSrcScanlineShifted += 2 * DEST_ELTS;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                       AverageByteSSE2OrAVX2()                        */
/************************************************************************/

static int
AverageByteSSE2OrAVX2(int nDstXWidth, int nChunkXSize,
                      const GByte *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                      GByte *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on Byte by
    // processing by group of 16 output pixels for SSE2, or 32 for AVX2

    const auto zero = setzero();
    const auto two16 = set1_epi16(2);
    const GByte *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    constexpr int DEST_ELTS = static_cast<int>(sizeof(zero)) / 2;
    int iDstPixel = 0;
    for (; iDstPixel < nDstXWidth - (2 * DEST_ELTS - 1);
         iDstPixel += 2 * DEST_ELTS)
    {
        decltype(setzero()) average0;
        {
            // Load 2 * DEST_ELTS bytes from each line
            const auto firstLine = loadu_int(pSrcScanlineShifted);
            const auto secondLine =
                loadu_int(pSrcScanlineShifted + nChunkXSize);
            // Extend those Bytes as UInt16s
            const auto firstLineLo = unpacklo_epi8(firstLine, zero);
            const auto firstLineHi = unpackhi_epi8(firstLine, zero);
            const auto secondLineLo = unpacklo_epi8(secondLine, zero);
            const auto secondLineHi = unpackhi_epi8(secondLine, zero);

            // Vertical addition
            const auto sumLo = add_epi16(firstLineLo, secondLineLo);
            const auto sumHi = add_epi16(firstLineHi, secondLineHi);

            // Horizontal addition of adjacent pairs, and recombine low and high
            // parts
            const auto sum = hadd_epi16(sumLo, sumHi);

            // average = (sum + 2) / 4
            average0 = srli_epi16(add_epi16(sum, two16), 2);

            pSrcScanlineShifted += 2 * DEST_ELTS;
        }

        decltype(setzero()) average1;
        {
            // Load 2 * DEST_ELTS bytes from each line
            const auto firstLine = loadu_int(pSrcScanlineShifted);
            const auto secondLine =
                loadu_int(pSrcScanlineShifted + nChunkXSize);
            // Extend those Bytes as UInt16s
            const auto firstLineLo = unpacklo_epi8(firstLine, zero);
            const auto firstLineHi = unpackhi_epi8(firstLine, zero);
            const auto secondLineLo = unpacklo_epi8(secondLine, zero);
            const auto secondLineHi = unpackhi_epi8(secondLine, zero);

            // Vertical addition
            const auto sumLo = add_epi16(firstLineLo, secondLineLo);
            const auto sumHi = add_epi16(firstLineHi, secondLineHi);

            // Horizontal addition of adjacent pairs, and recombine low and high
            // parts
            const auto sum = hadd_epi16(sumLo, sumHi);

            // average = (sum + 2) / 4
            average1 = srli_epi16(add_epi16(sum, two16), 2);

            pSrcScanlineShifted += 2 * DEST_ELTS;
        }

        // Pack each 16 bit average value to 8 bits
        const auto average = packus_epi16(average0, average1);
        storeu_int(&pDstScanline[iDstPixel], average);
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                      QuadraticMeanUInt16SSE2()                       */
/************************************************************************/

#ifdef __SSE3__
#define sse2_hadd_pd _mm_hadd_pd
#else
inline __m128d sse2_hadd_pd(__m128d a, __m128d b)
{
    auto aLo_bLo =
        _mm_castps_pd(_mm_movelh_ps(_mm_castpd_ps(a), _mm_castpd_ps(b)));
    auto aHi_bHi =
        _mm_castps_pd(_mm_movehl_ps(_mm_castpd_ps(b), _mm_castpd_ps(a)));
    return _mm_add_pd(aLo_bLo, aHi_bHi);  // (aLo + aHi, bLo + bHi)
}
#endif

inline __m128d SQUARE_PD(__m128d x)
{
    return _mm_mul_pd(x, x);
}

#ifdef __AVX2__

inline __m256d SQUARE_PD(__m256d x)
{
    return _mm256_mul_pd(x, x);
}

inline __m256d FIXUP_LANES(__m256d x)
{
    return _mm256_permute4x64_pd(x, _MM_SHUFFLE(3, 1, 2, 0));
}

inline __m256 FIXUP_LANES(__m256 x)
{
    return _mm256_castpd_ps(FIXUP_LANES(_mm256_castps_pd(x)));
}

#endif

static int
QuadraticMeanUInt16SSE2(int nDstXWidth, int nChunkXSize,
                        const uint16_t *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                        uint16_t *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on UInt16 by
    // processing by group of 4 output pixels.
    const uint16_t *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zero = _mm_setzero_si128();

#ifdef __AVX2__
    const auto zeroDot25 = _mm256_set1_pd(0.25);
    const auto zeroDot5 = _mm256_set1_pd(0.5);

    // The first four 0's could be anything, as we only take the bottom
    // 128 bits.
    const auto permutation = _mm256_set_epi32(0, 0, 0, 0, 6, 4, 2, 0);
#else
    const auto zeroDot25 = _mm_set1_pd(0.25);
    const auto zeroDot5 = _mm_set1_pd(0.5);
#endif

    constexpr int DEST_ELTS =
        static_cast<int>(sizeof(zero) / sizeof(uint16_t)) / 2;
    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
    {
        // Load 8 UInt16 from each line
        const auto firstLine = _mm_loadu_si128(
            reinterpret_cast<__m128i const *>(pSrcScanlineShifted));
        const auto secondLine =
            _mm_loadu_si128(reinterpret_cast<__m128i const *>(
                pSrcScanlineShifted + nChunkXSize));

        // Detect if all of the source values fit in 14 bits.
        // because if x < 2^14, then 4 * x^2 < 2^30 which fits in a signed int32
        // and we can do a much faster implementation.
        const auto maskTmp =
            _mm_srli_epi16(_mm_or_si128(firstLine, secondLine), 14);
#if defined(__i386__) || defined(_M_IX86)
        uint64_t nMaskFitsIn14Bits = 0;
        _mm_storel_epi64(
            reinterpret_cast<__m128i *>(&nMaskFitsIn14Bits),
            _mm_packus_epi16(maskTmp, maskTmp /* could be anything */));
#else
        const auto nMaskFitsIn14Bits = _mm_cvtsi128_si64(
            _mm_packus_epi16(maskTmp, maskTmp /* could be anything */));
#endif
        if (nMaskFitsIn14Bits == 0)
        {
            // Multiplication of 16 bit values and horizontal
            // addition of 32 bit results
            const auto firstLineHSumSquare =
                _mm_madd_epi16(firstLine, firstLine);
            const auto secondLineHSumSquare =
                _mm_madd_epi16(secondLine, secondLine);
            // Vertical addition
            const auto sumSquares =
                _mm_add_epi32(firstLineHSumSquare, secondLineHSumSquare);
            // In theory we should take sqrt(sumSquares * 0.25f)
            // but given the rounding we do, this is equivalent to
            // sqrt((sumSquares + 1)/4). This has been verified exhaustively for
            // sumSquares <= 4 * 16383^2
            const auto one32 = _mm_set1_epi32(1);
            const auto sumSquaresPlusOneDiv4 =
                _mm_srli_epi32(_mm_add_epi32(sumSquares, one32), 2);
            // Take square root and truncate/floor to int32
            auto rms = _mm_cvttps_epi32(
                _mm_sqrt_ps(_mm_cvtepi32_ps(sumSquaresPlusOneDiv4)));

            // Round to upper value if it minimizes the
            // error |rms^2 - sumSquares/4|
            // if( 2 * (2 * rms * (rms + 1) + 1) < sumSquares )
            //    rms += 1;
            // which is equivalent to:
            // if( rms * rms + rms < (sumSquares+1) / 4 )
            //    rms += 1;
            auto mask =
                _mm_cmpgt_epi32(sumSquaresPlusOneDiv4,
                                _mm_add_epi32(_mm_madd_epi16(rms, rms), rms));
            rms = _mm_sub_epi32(rms, mask);
            // Pack each 32 bit RMS value to 16 bits
            rms = _mm_packs_epi32(rms, rms /* could be anything */);
            _mm_storel_epi64(
                reinterpret_cast<__m128i *>(&pDstScanline[iDstPixel]), rms);
            pSrcScanlineShifted += 2 * DEST_ELTS;
            continue;
        }

        // An approach using _mm_mullo_epi16, _mm_mulhi_epu16 before extending
        // to 32 bit would result in 4 multiplications instead of 8, but
        // mullo/mulhi have a worse throughput than mul_pd.

        // Extend those UInt16s as UInt32s
        const auto firstLineLo = _mm_unpacklo_epi16(firstLine, zero);
        const auto firstLineHi = _mm_unpackhi_epi16(firstLine, zero);
        const auto secondLineLo = _mm_unpacklo_epi16(secondLine, zero);
        const auto secondLineHi = _mm_unpackhi_epi16(secondLine, zero);

#ifdef __AVX2__
        // Multiplication of 32 bit values previously converted to 64 bit double
        const auto firstLineLoDbl = SQUARE_PD(_mm256_cvtepi32_pd(firstLineLo));
        const auto firstLineHiDbl = SQUARE_PD(_mm256_cvtepi32_pd(firstLineHi));
        const auto secondLineLoDbl =
            SQUARE_PD(_mm256_cvtepi32_pd(secondLineLo));
        const auto secondLineHiDbl =
            SQUARE_PD(_mm256_cvtepi32_pd(secondLineHi));

        // Vertical addition of squares
        const auto sumSquaresLo =
            _mm256_add_pd(firstLineLoDbl, secondLineLoDbl);
        const auto sumSquaresHi =
            _mm256_add_pd(firstLineHiDbl, secondLineHiDbl);

        // Horizontal addition of squares
        const auto sumSquares =
            FIXUP_LANES(_mm256_hadd_pd(sumSquaresLo, sumSquaresHi));

        const auto sumDivWeight = _mm256_mul_pd(sumSquares, zeroDot25);

        // Take square root and truncate/floor to int32
        auto rms = _mm256_cvttpd_epi32(_mm256_sqrt_pd(sumDivWeight));
        const auto rmsDouble = _mm256_cvtepi32_pd(rms);
        const auto right = _mm256_sub_pd(
            sumDivWeight, _mm256_add_pd(SQUARE_PD(rmsDouble), rmsDouble));

        auto mask =
            _mm256_castpd_ps(_mm256_cmp_pd(zeroDot5, right, _CMP_LT_OS));
        // Extract 32-bit from each of the 4 64-bit masks
        // mask = FIXUP_LANES(_mm256_shuffle_ps(mask, mask,
        // _MM_SHUFFLE(2,0,2,0)));
        mask = _mm256_permutevar8x32_ps(mask, permutation);
        const auto maskI = _mm_castps_si128(_mm256_extractf128_ps(mask, 0));

        // Apply the correction
        rms = _mm_sub_epi32(rms, maskI);

        // Pack each 32 bit RMS value to 16 bits
        rms = _mm_packus_epi32(rms, rms /* could be anything */);
#else
        // Multiplication of 32 bit values previously converted to 64 bit double
        const auto firstLineLoLo = SQUARE_PD(_mm_cvtepi32_pd(firstLineLo));
        const auto firstLineLoHi =
            SQUARE_PD(_mm_cvtepi32_pd(_mm_srli_si128(firstLineLo, 8)));
        const auto firstLineHiLo = SQUARE_PD(_mm_cvtepi32_pd(firstLineHi));
        const auto firstLineHiHi =
            SQUARE_PD(_mm_cvtepi32_pd(_mm_srli_si128(firstLineHi, 8)));

        const auto secondLineLoLo = SQUARE_PD(_mm_cvtepi32_pd(secondLineLo));
        const auto secondLineLoHi =
            SQUARE_PD(_mm_cvtepi32_pd(_mm_srli_si128(secondLineLo, 8)));
        const auto secondLineHiLo = SQUARE_PD(_mm_cvtepi32_pd(secondLineHi));
        const auto secondLineHiHi =
            SQUARE_PD(_mm_cvtepi32_pd(_mm_srli_si128(secondLineHi, 8)));

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
        const auto rightLo = _mm_sub_pd(
            sumDivWeightLo, _mm_add_pd(SQUARE_PD(rmsLoDouble), rmsLoDouble));
        const auto rightHi = _mm_sub_pd(
            sumDivWeightHi, _mm_add_pd(SQUARE_PD(rmsHiDouble), rmsHiDouble));

        const auto maskLo = _mm_castpd_ps(_mm_cmplt_pd(zeroDot5, rightLo));
        const auto maskHi = _mm_castpd_ps(_mm_cmplt_pd(zeroDot5, rightHi));
        // The value of the mask will be -1 when the correction needs to be
        // applied
        const auto mask = _mm_castps_si128(_mm_shuffle_ps(
            maskLo, maskHi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));

        auto rms = _mm_castps_si128(
            _mm_movelh_ps(_mm_castsi128_ps(rmsLo), _mm_castsi128_ps(rmsHi)));
        // Apply the correction
        rms = _mm_sub_epi32(rms, mask);

        // Pack each 32 bit RMS value to 16 bits
        rms = sse2_packus_epi32(rms, rms /* could be anything */);
#endif

        _mm_storel_epi64(reinterpret_cast<__m128i *>(&pDstScanline[iDstPixel]),
                         rms);
        pSrcScanlineShifted += 2 * DEST_ELTS;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                         AverageUInt16SSE2()                          */
/************************************************************************/

static int
AverageUInt16SSE2(int nDstXWidth, int nChunkXSize,
                  const uint16_t *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                  uint16_t *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on UInt16 by
    // processing by group of 8 output pixels.

    const auto mask = _mm_set1_epi32(0xFFFF);
    const auto two = _mm_set1_epi32(2);
    const uint16_t *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    constexpr int DEST_ELTS = static_cast<int>(sizeof(mask) / sizeof(uint16_t));
    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
    {
        __m128i averageLow;
        // Load 8 UInt16 from each line
        {
            const auto firstLine = _mm_loadu_si128(
                reinterpret_cast<__m128i const *>(pSrcScanlineShifted));
            const auto secondLine =
                _mm_loadu_si128(reinterpret_cast<__m128i const *>(
                    pSrcScanlineShifted + nChunkXSize));

            // Horizontal addition and extension to 32 bit
            const auto horizAddFirstLine = _mm_add_epi32(
                _mm_and_si128(firstLine, mask), _mm_srli_epi32(firstLine, 16));
            const auto horizAddSecondLine =
                _mm_add_epi32(_mm_and_si128(secondLine, mask),
                              _mm_srli_epi32(secondLine, 16));

            // Vertical addition and average computation
            // average = (sum + 2) >> 2
            const auto sum = _mm_add_epi32(
                _mm_add_epi32(horizAddFirstLine, horizAddSecondLine), two);
            averageLow = _mm_srli_epi32(sum, 2);
        }
        // Load 8 UInt16 from each line
        __m128i averageHigh;
        {
            const auto firstLine =
                _mm_loadu_si128(reinterpret_cast<__m128i const *>(
                    pSrcScanlineShifted + DEST_ELTS));
            const auto secondLine =
                _mm_loadu_si128(reinterpret_cast<__m128i const *>(
                    pSrcScanlineShifted + DEST_ELTS + nChunkXSize));

            // Horizontal addition and extension to 32 bit
            const auto horizAddFirstLine = _mm_add_epi32(
                _mm_and_si128(firstLine, mask), _mm_srli_epi32(firstLine, 16));
            const auto horizAddSecondLine =
                _mm_add_epi32(_mm_and_si128(secondLine, mask),
                              _mm_srli_epi32(secondLine, 16));

            // Vertical addition and average computation
            // average = (sum + 2) >> 2
            const auto sum = _mm_add_epi32(
                _mm_add_epi32(horizAddFirstLine, horizAddSecondLine), two);
            averageHigh = _mm_srli_epi32(sum, 2);
        }

        // Pack each 32 bit average value to 16 bits
        auto average = sse2_packus_epi32(averageLow, averageHigh);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(&pDstScanline[iDstPixel]),
                         average);
        pSrcScanlineShifted += 2 * DEST_ELTS;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                       QuadraticMeanFloatSSE2()                       */
/************************************************************************/

#if !defined(ARM_V7)

#ifdef __SSE3__
#define sse2_hadd_ps _mm_hadd_ps
#else
inline __m128 sse2_hadd_ps(__m128 a, __m128 b)
{
    auto aEven_bEven = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
    auto aOdd_bOdd = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
    return _mm_add_ps(aEven_bEven, aOdd_bOdd);  // (aEven + aOdd, bEven + bOdd)
}
#endif

#ifdef __AVX2__
#define set1_ps _mm256_set1_ps
#define loadu_ps _mm256_loadu_ps
#define andnot_ps _mm256_andnot_ps
#define and_ps _mm256_and_ps
#define max_ps _mm256_max_ps
#define shuffle_ps _mm256_shuffle_ps
#define div_ps _mm256_div_ps
#define cmpeq_ps(x, y) _mm256_cmp_ps((x), (y), _CMP_EQ_OQ)
#define mul_ps _mm256_mul_ps
#define add_ps _mm256_add_ps
#define hadd_ps _mm256_hadd_ps
#define sqrt_ps _mm256_sqrt_ps
#define or_ps _mm256_or_ps
#define unpacklo_ps _mm256_unpacklo_ps
#define unpackhi_ps _mm256_unpackhi_ps
#define storeu_ps _mm256_storeu_ps
#define blendv_ps _mm256_blendv_ps

inline __m256 SQUARE_PS(__m256 x)
{
    return _mm256_mul_ps(x, x);
}

#else

#define set1_ps _mm_set1_ps
#define loadu_ps _mm_loadu_ps
#define andnot_ps _mm_andnot_ps
#define and_ps _mm_and_ps
#define max_ps _mm_max_ps
#define shuffle_ps _mm_shuffle_ps
#define div_ps _mm_div_ps
#define cmpeq_ps _mm_cmpeq_ps
#define mul_ps _mm_mul_ps
#define add_ps _mm_add_ps
#define hadd_ps sse2_hadd_ps
#define sqrt_ps _mm_sqrt_ps
#define or_ps _mm_or_ps
#define unpacklo_ps _mm_unpacklo_ps
#define unpackhi_ps _mm_unpackhi_ps
#define storeu_ps _mm_storeu_ps

inline __m128 blendv_ps(__m128 a, __m128 b, __m128 mask)
{
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_blendv_ps(a, b, mask);
#else
    return _mm_or_ps(_mm_andnot_ps(mask, a), _mm_and_ps(mask, b));
#endif
}

inline __m128 SQUARE_PS(__m128 x)
{
    return _mm_mul_ps(x, x);
}

inline __m128 FIXUP_LANES(__m128 x)
{
    return x;
}

#endif

static int
#if defined(__GNUC__)
    __attribute__((noinline))
#endif
    QuadraticMeanFloatSSE2(int nDstXWidth, int nChunkXSize,
                           const float *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                           float *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for RMS on Float32 by
    // processing by group of output pixels.
    const float *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto minus_zero = set1_ps(-0.0f);
    const auto zeroDot25 = set1_ps(0.25f);
    const auto one = set1_ps(1.0f);
    const auto infv = set1_ps(std::numeric_limits<float>::infinity());
    constexpr int DEST_ELTS = static_cast<int>(sizeof(one) / sizeof(float));

    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
    {
        // Load 2*DEST_ELTS Float32 from each line
        auto firstLineLo = loadu_ps(pSrcScanlineShifted);
        auto firstLineHi = loadu_ps(pSrcScanlineShifted + DEST_ELTS);
        auto secondLineLo = loadu_ps(pSrcScanlineShifted + nChunkXSize);
        auto secondLineHi =
            loadu_ps(pSrcScanlineShifted + DEST_ELTS + nChunkXSize);

        // Take the absolute value
        firstLineLo = andnot_ps(minus_zero, firstLineLo);
        firstLineHi = andnot_ps(minus_zero, firstLineHi);
        secondLineLo = andnot_ps(minus_zero, secondLineLo);
        secondLineHi = andnot_ps(minus_zero, secondLineHi);

        auto firstLineEven =
            shuffle_ps(firstLineLo, firstLineHi, _MM_SHUFFLE(2, 0, 2, 0));
        auto firstLineOdd =
            shuffle_ps(firstLineLo, firstLineHi, _MM_SHUFFLE(3, 1, 3, 1));
        auto secondLineEven =
            shuffle_ps(secondLineLo, secondLineHi, _MM_SHUFFLE(2, 0, 2, 0));
        auto secondLineOdd =
            shuffle_ps(secondLineLo, secondLineHi, _MM_SHUFFLE(3, 1, 3, 1));

        // Compute the maximum of each DEST_ELTS value to RMS-average
        const auto maxV = max_ps(max_ps(firstLineEven, firstLineOdd),
                                 max_ps(secondLineEven, secondLineEven));

        // Normalize each value by the maximum of the DEST_ELTS ones.
        // This step is important to avoid that the square evaluates to infinity
        // for sufficiently big input.
        auto invMax = div_ps(one, maxV);
        // Deal with 0 being the maximum to correct division by zero
        // note: comparing to -0 leads to identical results as to comparing with
        // 0
        invMax = andnot_ps(cmpeq_ps(maxV, minus_zero), invMax);

        firstLineEven = mul_ps(firstLineEven, invMax);
        firstLineOdd = mul_ps(firstLineOdd, invMax);
        secondLineEven = mul_ps(secondLineEven, invMax);
        secondLineOdd = mul_ps(secondLineOdd, invMax);

        // Compute squares
        firstLineEven = SQUARE_PS(firstLineEven);
        firstLineOdd = SQUARE_PS(firstLineOdd);
        secondLineEven = SQUARE_PS(secondLineEven);
        secondLineOdd = SQUARE_PS(secondLineOdd);

        const auto sumSquares = add_ps(add_ps(firstLineEven, firstLineOdd),
                                       add_ps(secondLineEven, secondLineOdd));

        auto rms = mul_ps(maxV, sqrt_ps(mul_ps(sumSquares, zeroDot25)));

        // Deal with infinity being the maximum
        const auto maskIsInf = cmpeq_ps(maxV, infv);
        rms = blendv_ps(rms, infv, maskIsInf);

        rms = FIXUP_LANES(rms);

        storeu_ps(&pDstScanline[iDstPixel], rms);
        pSrcScanlineShifted += DEST_ELTS * 2;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                          AverageFloatSSE2()                          */
/************************************************************************/

static int AverageFloatSSE2(int nDstXWidth, int nChunkXSize,
                            const float *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                            float *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on Float32 by
    // processing by group of output pixels.
    const float *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zeroDot25 = _mm_set1_ps(0.25f);
    constexpr int DEST_ELTS =
        static_cast<int>(sizeof(zeroDot25) / sizeof(float));

    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
    {
        // Load 2 * DEST_ELTS Float32 from each line
        const auto firstLineLo =
            _mm_mul_ps(_mm_loadu_ps(pSrcScanlineShifted), zeroDot25);
        const auto firstLineHi = _mm_mul_ps(
            _mm_loadu_ps(pSrcScanlineShifted + DEST_ELTS), zeroDot25);
        const auto secondLineLo = _mm_mul_ps(
            _mm_loadu_ps(pSrcScanlineShifted + nChunkXSize), zeroDot25);
        const auto secondLineHi = _mm_mul_ps(
            _mm_loadu_ps(pSrcScanlineShifted + DEST_ELTS + nChunkXSize),
            zeroDot25);

        // Vertical addition
        const auto tmpLo = _mm_add_ps(firstLineLo, secondLineLo);
        const auto tmpHi = _mm_add_ps(firstLineHi, secondLineHi);

        // Horizontal addition
        const auto average = sse2_hadd_ps(tmpLo, tmpHi);

        _mm_storeu_ps(&pDstScanline[iDstPixel], average);
        pSrcScanlineShifted += DEST_ELTS * 2;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

/************************************************************************/
/*                         AverageDoubleSSE2()                          */
/************************************************************************/

static int
AverageDoubleSSE2(int nDstXWidth, int nChunkXSize,
                  const double *&CPL_RESTRICT pSrcScanlineShiftedInOut,
                  double *CPL_RESTRICT pDstScanline)
{
    // Optimized implementation for average on Float64 by
    // processing by group of output pixels.
    const double *CPL_RESTRICT pSrcScanlineShifted = pSrcScanlineShiftedInOut;

    int iDstPixel = 0;
    const auto zeroDot25 = _mm_set1_pd(0.25);
    constexpr int DEST_ELTS =
        static_cast<int>(sizeof(zeroDot25) / sizeof(double));

    for (; iDstPixel < nDstXWidth - (DEST_ELTS - 1); iDstPixel += DEST_ELTS)
    {
        // Load 4 * DEST_ELTS Float64 from each line
        const auto firstLine0 = _mm_mul_pd(
            _mm_loadu_pd(pSrcScanlineShifted + 0 * DEST_ELTS), zeroDot25);
        const auto firstLine1 = _mm_mul_pd(
            _mm_loadu_pd(pSrcScanlineShifted + 1 * DEST_ELTS), zeroDot25);
        const auto secondLine0 = _mm_mul_pd(
            _mm_loadu_pd(pSrcScanlineShifted + 0 * DEST_ELTS + nChunkXSize),
            zeroDot25);
        const auto secondLine1 = _mm_mul_pd(
            _mm_loadu_pd(pSrcScanlineShifted + 1 * DEST_ELTS + nChunkXSize),
            zeroDot25);

        // Vertical addition
        const auto tmp0 = _mm_add_pd(firstLine0, secondLine0);
        const auto tmp1 = _mm_add_pd(firstLine1, secondLine1);

        // Horizontal addition
        const auto average0 = sse2_hadd_pd(tmp0, tmp1);

        _mm_storeu_pd(&pDstScanline[iDstPixel + 0], average0);
        pSrcScanlineShifted += DEST_ELTS * 2;
    }

    pSrcScanlineShiftedInOut = pSrcScanlineShifted;
    return iDstPixel;
}

#endif

#endif

/************************************************************************/
/*                   GDALResampleChunk_AverageOrRMS()                   */
/************************************************************************/

template <class T, class Tsum, GDALDataType eWrkDataType, bool bQuadraticMean>
static CPLErr
GDALResampleChunk_AverageOrRMS_T(const GDALOverviewResampleArgs &args,
                                 const T *pChunk, void **ppDstBuffer)
{
    const double dfXRatioDstToSrc = args.dfXRatioDstToSrc;
    const double dfYRatioDstToSrc = args.dfYRatioDstToSrc;
    const double dfSrcXDelta = args.dfSrcXDelta;
    const double dfSrcYDelta = args.dfSrcYDelta;
    const GByte *pabyChunkNodataMask = args.pabyChunkNodataMask;
    const int nChunkXOff = args.nChunkXOff;
    const int nChunkYOff = args.nChunkYOff;
    const int nChunkXSize = args.nChunkXSize;
    const int nChunkYSize = args.nChunkYSize;
    const int nDstXOff = args.nDstXOff;
    const int nDstXOff2 = args.nDstXOff2;
    const int nDstYOff = args.nDstYOff;
    const int nDstYOff2 = args.nDstYOff2;
    const char *pszResampling = args.pszResampling;
    bool bHasNoData = args.bHasNoData;
    const double dfNoDataValue = args.dfNoDataValue;
    const GDALColorTable *const poColorTable =
        !bQuadraticMean &&
                // AVERAGE_BIT2GRAYSCALE
                CPL_TO_BOOL(STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2G"))
            ? nullptr
            : args.poColorTable;
    const bool bPropagateNoData = args.bPropagateNoData;

    T tNoDataValue = (!bHasNoData) ? 0 : static_cast<T>(dfNoDataValue);
    const T tReplacementVal =
        bHasNoData ? static_cast<T>(GDALGetNoDataReplacementValue(
                         args.eOvrDataType, dfNoDataValue))
                   : 0;

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    const int nDstXWidth = nDstXOff2 - nDstXOff;

    /* -------------------------------------------------------------------- */
    /*      Allocate buffers.                                               */
    /* -------------------------------------------------------------------- */
    *ppDstBuffer = static_cast<T *>(
        VSI_MALLOC3_VERBOSE(nDstXWidth, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(eWrkDataType)));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }
    T *const pDstBuffer = static_cast<T *>(*ppDstBuffer);

    struct PrecomputedXValue
    {
        int nLeftXOffShifted;
        int nRightXOffShifted;
        double dfLeftWeight;
        double dfRightWeight;
        double dfTotalWeightFullLine;
    };

    PrecomputedXValue *pasSrcX = static_cast<PrecomputedXValue *>(
        VSI_MALLOC2_VERBOSE(nDstXWidth, sizeof(PrecomputedXValue)));

    if (pasSrcX == nullptr)
    {
        return CE_Failure;
    }

    std::vector<GDALColorEntry> colorEntries;

    if (poColorTable)
    {
        int nTransparentIdx = -1;
        colorEntries = ReadColorTable(*poColorTable, nTransparentIdx);

        // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
        // it as nodata value
        if (bHasNoData && dfNoDataValue >= 0.0 &&
            tNoDataValue < colorEntries.size())
            colorEntries[static_cast<int>(tNoDataValue)].c4 = 0;

        // Or if we have no explicit nodata, but a color table entry that is
        // transparent, consider it as the nodata value
        else if (!bHasNoData && nTransparentIdx >= 0)
        {
            bHasNoData = true;
            tNoDataValue = static_cast<T>(nTransparentIdx);
        }
    }

    /* ==================================================================== */
    /*      Precompute inner loop constants.                                */
    /* ==================================================================== */
    bool bSrcXSpacingIsTwo = true;
    int nLastSrcXOff2 = -1;
    for (int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel)
    {
        const double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
        // Apply some epsilon to avoid numerical precision issues
        int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
        const double dfSrcXOff2 =
            dfSrcXDelta + (iDstPixel + 1) * dfXRatioDstToSrc;
        int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));

        if (nSrcXOff < nChunkXOff)
            nSrcXOff = nChunkXOff;
        if (nSrcXOff2 == nSrcXOff)
            nSrcXOff2++;
        if (nSrcXOff2 > nChunkRightXOff)
            nSrcXOff2 = nChunkRightXOff;

        pasSrcX[iDstPixel - nDstXOff].nLeftXOffShifted = nSrcXOff - nChunkXOff;
        pasSrcX[iDstPixel - nDstXOff].nRightXOffShifted =
            nSrcXOff2 - nChunkXOff;
        pasSrcX[iDstPixel - nDstXOff].dfLeftWeight =
            (nSrcXOff2 == nSrcXOff + 1) ? 1.0 : 1 - (dfSrcXOff - nSrcXOff);
        pasSrcX[iDstPixel - nDstXOff].dfRightWeight =
            1 - (nSrcXOff2 - dfSrcXOff2);
        pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine =
            pasSrcX[iDstPixel - nDstXOff].dfLeftWeight;
        if (nSrcXOff + 1 < nSrcXOff2)
        {
            pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine +=
                nSrcXOff2 - nSrcXOff - 2;
            pasSrcX[iDstPixel - nDstXOff].dfTotalWeightFullLine +=
                pasSrcX[iDstPixel - nDstXOff].dfRightWeight;
        }

        if (nSrcXOff2 - nSrcXOff != 2 ||
            (nLastSrcXOff2 >= 0 && nLastSrcXOff2 != nSrcXOff))
        {
            bSrcXSpacingIsTwo = false;
        }
        nLastSrcXOff2 = nSrcXOff2;
    }

    /* ==================================================================== */
    /*      Loop over destination scanlines.                                */
    /* ==================================================================== */
    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        const double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
        if (nSrcYOff < nChunkYOff)
            nSrcYOff = nChunkYOff;

        const double dfSrcYOff2 =
            dfSrcYDelta + (iDstLine + 1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
        if (nSrcYOff2 == nSrcYOff)
            ++nSrcYOff2;
        if (nSrcYOff2 > nChunkBottomYOff)
            nSrcYOff2 = nChunkBottomYOff;

        T *const pDstScanline =
            pDstBuffer + static_cast<size_t>(iDstLine - nDstYOff) * nDstXWidth;

        /* --------------------------------------------------------------------
         */
        /*      Loop over destination pixels */
        /* --------------------------------------------------------------------
         */
        if (poColorTable == nullptr)
        {
            if (bSrcXSpacingIsTwo && nSrcYOff2 == nSrcYOff + 2 &&
                pabyChunkNodataMask == nullptr)
            {
                if constexpr (eWrkDataType == GDT_UInt8 ||
                              eWrkDataType == GDT_UInt16)
                {
                    // Optimized case : no nodata, overview by a factor of 2 and
                    // regular x and y src spacing.
                    const T *pSrcScanlineShifted =
                        pChunk + pasSrcX[0].nLeftXOffShifted +
                        static_cast<size_t>(nSrcYOff - nChunkYOff) *
                            nChunkXSize;
                    int iDstPixel = 0;
#ifdef USE_SSE2
                    if constexpr (eWrkDataType == GDT_UInt8)
                    {
                        if constexpr (bQuadraticMean)
                        {
                            iDstPixel = QuadraticMeanByteSSE2OrAVX2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                        else
                        {
                            iDstPixel = AverageByteSSE2OrAVX2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                    }
                    else
                    {
                        static_assert(eWrkDataType == GDT_UInt16);
                        if constexpr (bQuadraticMean)
                        {
                            iDstPixel = QuadraticMeanUInt16SSE2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                        else
                        {
                            iDstPixel = AverageUInt16SSE2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                    }
#endif
                    for (; iDstPixel < nDstXWidth; ++iDstPixel)
                    {
                        Tsum nTotal = 0;
                        T nVal;
                        if constexpr (bQuadraticMean)
                            nTotal =
                                SQUARE<Tsum>(pSrcScanlineShifted[0]) +
                                SQUARE<Tsum>(pSrcScanlineShifted[1]) +
                                SQUARE<Tsum>(pSrcScanlineShifted[nChunkXSize]) +
                                SQUARE<Tsum>(
                                    pSrcScanlineShifted[1 + nChunkXSize]);
                        else
                            nTotal = pSrcScanlineShifted[0] +
                                     pSrcScanlineShifted[1] +
                                     pSrcScanlineShifted[nChunkXSize] +
                                     pSrcScanlineShifted[1 + nChunkXSize];

                        constexpr int nTotalWeight = 4;
                        if constexpr (bQuadraticMean)
                            nVal = ComputeIntegerRMS_4values<T>(nTotal);
                        else
                            nVal = static_cast<T>((nTotal + nTotalWeight / 2) /
                                                  nTotalWeight);

                        // No need to compare nVal against tNoDataValue as we
                        // are in a case where pabyChunkNodataMask == nullptr
                        // implies the absence of nodata value.
                        pDstScanline[iDstPixel] = nVal;
                        pSrcScanlineShifted += 2;
                    }
                }
                else
                {
                    static_assert(eWrkDataType == GDT_Float32 ||
                                  eWrkDataType == GDT_Float64);
                    const T *pSrcScanlineShifted =
                        pChunk + pasSrcX[0].nLeftXOffShifted +
                        static_cast<size_t>(nSrcYOff - nChunkYOff) *
                            nChunkXSize;
                    int iDstPixel = 0;
#if defined(USE_SSE2) && !defined(ARM_V7)
                    if constexpr (eWrkDataType == GDT_Float32)
                    {
                        static_assert(std::is_same_v<T, float>);
                        if constexpr (bQuadraticMean)
                        {
                            iDstPixel = QuadraticMeanFloatSSE2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                        else
                        {
                            iDstPixel = AverageFloatSSE2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                    }
                    else
                    {
                        if constexpr (!bQuadraticMean)
                        {
                            iDstPixel = AverageDoubleSSE2(
                                nDstXWidth, nChunkXSize, pSrcScanlineShifted,
                                pDstScanline);
                        }
                    }
#endif

                    for (; iDstPixel < nDstXWidth; ++iDstPixel)
                    {
                        T nVal;

                        if constexpr (bQuadraticMean)
                        {
                            // Avoid issues with large values by renormalizing
                            const auto max = std::max(
                                {std::fabs(pSrcScanlineShifted[0]),
                                 std::fabs(pSrcScanlineShifted[1]),
                                 std::fabs(pSrcScanlineShifted[nChunkXSize]),
                                 std::fabs(
                                     pSrcScanlineShifted[1 + nChunkXSize])});
                            if (max == 0)
                            {
                                nVal = 0;
                            }
                            else if (std::isinf(max))
                            {
                                // If there is at least one infinity value,
                                // then just summing, and taking the abs
                                // value will give the expected result:
                                // * +inf if all values are +inf
                                // * +inf if all values are -inf
                                // * NaN otherwise
                                nVal = std::fabs(
                                    pSrcScanlineShifted[0] +
                                    pSrcScanlineShifted[1] +
                                    pSrcScanlineShifted[nChunkXSize] +
                                    pSrcScanlineShifted[1 + nChunkXSize]);
                            }
                            else
                            {
                                const auto inv_max = static_cast<T>(1.0) / max;
                                nVal =
                                    max *
                                    std::sqrt(
                                        static_cast<T>(0.25) *
                                        (SQUARE(pSrcScanlineShifted[0] *
                                                inv_max) +
                                         SQUARE(pSrcScanlineShifted[1] *
                                                inv_max) +
                                         SQUARE(
                                             pSrcScanlineShifted[nChunkXSize] *
                                             inv_max) +
                                         SQUARE(
                                             pSrcScanlineShifted[1 +
                                                                 nChunkXSize] *
                                             inv_max)));
                            }
                        }
                        else
                        {
                            constexpr auto weight = static_cast<T>(0.25);
                            // Multiply each value by weight to avoid
                            // potential overflow
                            nVal =
                                (weight * pSrcScanlineShifted[0] +
                                 weight * pSrcScanlineShifted[1] +
                                 weight * pSrcScanlineShifted[nChunkXSize] +
                                 weight * pSrcScanlineShifted[1 + nChunkXSize]);
                        }

                        // No need to compare nVal against tNoDataValue as we
                        // are in a case where pabyChunkNodataMask == nullptr
                        // implies the absence of nodata value.
                        pDstScanline[iDstPixel] = nVal;
                        pSrcScanlineShifted += 2;
                    }
                }
            }
            else
            {
                const double dfBottomWeight =
                    (nSrcYOff + 1 == nSrcYOff2) ? 1.0
                                                : 1.0 - (dfSrcYOff - nSrcYOff);
                const double dfTopWeight = 1.0 - (nSrcYOff2 - dfSrcYOff2);
                nSrcYOff -= nChunkYOff;
                nSrcYOff2 -= nChunkYOff;

                double dfTotalWeightFullColumn = dfBottomWeight;
                if (nSrcYOff + 1 < nSrcYOff2)
                {
                    dfTotalWeightFullColumn += nSrcYOff2 - nSrcYOff - 2;
                    dfTotalWeightFullColumn += dfTopWeight;
                }

                for (int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel)
                {
                    const int nSrcXOff = pasSrcX[iDstPixel].nLeftXOffShifted;
                    const int nSrcXOff2 = pasSrcX[iDstPixel].nRightXOffShifted;

                    double dfTotal = 0;
                    double dfTotalWeight = 0;
                    [[maybe_unused]] double dfMulFactor = 1.0;
                    [[maybe_unused]] double dfInvMulFactor = 1.0;
                    constexpr bool bUseMulFactor =
                        (eWrkDataType == GDT_Float32 ||
                         eWrkDataType == GDT_Float64);
                    if (pabyChunkNodataMask == nullptr)
                    {
                        if constexpr (bUseMulFactor)
                        {
                            if constexpr (bQuadraticMean)
                            {
                                T mulFactor = 0;
                                auto pChunkShifted =
                                    pChunk +
                                    static_cast<size_t>(nSrcYOff) * nChunkXSize;

                                for (int iY = nSrcYOff; iY < nSrcYOff2;
                                     ++iY, pChunkShifted += nChunkXSize)
                                {
                                    for (int iX = nSrcXOff; iX < nSrcXOff2;
                                         ++iX)
                                        mulFactor = std::max(
                                            mulFactor,
                                            std::fabs(pChunkShifted[iX]));
                                }
                                dfMulFactor = double(mulFactor);
                                dfInvMulFactor =
                                    dfMulFactor > 0 &&
                                            std::isfinite(dfMulFactor)
                                        ? 1.0 / dfMulFactor
                                        : 1.0;
                            }
                            else
                            {
                                dfMulFactor = (nSrcYOff2 - nSrcYOff) *
                                              (nSrcXOff2 - nSrcXOff);
                                dfInvMulFactor = 1.0 / dfMulFactor;
                            }
                        }

                        auto pChunkShifted =
                            pChunk +
                            static_cast<size_t>(nSrcYOff) * nChunkXSize;
                        int nCounterY = nSrcYOff2 - nSrcYOff - 1;
                        double dfWeightY = dfBottomWeight;
                        while (true)
                        {
                            double dfTotalLine;
                            if constexpr (bQuadraticMean)
                            {
                                // Left pixel
                                {
                                    const T val = pChunkShifted[nSrcXOff];
                                    dfTotalLine =
                                        SQUARE(double(val) * dfInvMulFactor) *
                                        pasSrcX[iDstPixel].dfLeftWeight;
                                }

                                if (nSrcXOff + 1 < nSrcXOff2)
                                {
                                    // Middle pixels
                                    for (int iX = nSrcXOff + 1;
                                         iX < nSrcXOff2 - 1; ++iX)
                                    {
                                        const T val = pChunkShifted[iX];
                                        dfTotalLine += SQUARE(double(val) *
                                                              dfInvMulFactor);
                                    }

                                    // Right pixel
                                    {
                                        const T val =
                                            pChunkShifted[nSrcXOff2 - 1];
                                        dfTotalLine +=
                                            SQUARE(double(val) *
                                                   dfInvMulFactor) *
                                            pasSrcX[iDstPixel].dfRightWeight;
                                    }
                                }
                            }
                            else
                            {
                                // Left pixel
                                {
                                    const T val = pChunkShifted[nSrcXOff];
                                    dfTotalLine =
                                        double(val) * dfInvMulFactor *
                                        pasSrcX[iDstPixel].dfLeftWeight;
                                }

                                if (nSrcXOff + 1 < nSrcXOff2)
                                {
                                    // Middle pixels
                                    for (int iX = nSrcXOff + 1;
                                         iX < nSrcXOff2 - 1; ++iX)
                                    {
                                        const T val = pChunkShifted[iX];
                                        dfTotalLine +=
                                            double(val) * dfInvMulFactor;
                                    }

                                    // Right pixel
                                    {
                                        const T val =
                                            pChunkShifted[nSrcXOff2 - 1];
                                        dfTotalLine +=
                                            double(val) * dfInvMulFactor *
                                            pasSrcX[iDstPixel].dfRightWeight;
                                    }
                                }
                            }

                            dfTotal += dfTotalLine * dfWeightY;
                            --nCounterY;
                            if (nCounterY < 0)
                                break;
                            pChunkShifted += nChunkXSize;
                            dfWeightY = (nCounterY == 0) ? dfTopWeight : 1.0;
                        }

                        dfTotalWeight =
                            pasSrcX[iDstPixel].dfTotalWeightFullLine *
                            dfTotalWeightFullColumn;
                    }
                    else
                    {
                        size_t nCount = 0;
                        for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                        {
                            const auto pChunkShifted =
                                pChunk + static_cast<size_t>(iY) * nChunkXSize;

                            double dfTotalLine = 0;
                            double dfTotalWeightLine = 0;
                            // Left pixel
                            {
                                const int iX = nSrcXOff;
                                const T val = pChunkShifted[iX];
                                if (pabyChunkNodataMask
                                        [iX +
                                         static_cast<size_t>(iY) * nChunkXSize])
                                {
                                    nCount++;
                                    const double dfWeightX =
                                        pasSrcX[iDstPixel].dfLeftWeight;
                                    dfTotalWeightLine = dfWeightX;
                                    if constexpr (bQuadraticMean)
                                        dfTotalLine =
                                            SQUARE(double(val)) * dfWeightX;
                                    else
                                        dfTotalLine = double(val) * dfWeightX;
                                }
                            }

                            if (nSrcXOff < nSrcXOff2 - 1)
                            {
                                // Middle pixels
                                for (int iX = nSrcXOff + 1; iX < nSrcXOff2 - 1;
                                     ++iX)
                                {
                                    const T val = pChunkShifted[iX];
                                    if (pabyChunkNodataMask
                                            [iX + static_cast<size_t>(iY) *
                                                      nChunkXSize])
                                    {
                                        nCount++;
                                        dfTotalWeightLine += 1;
                                        if constexpr (bQuadraticMean)
                                            dfTotalLine += SQUARE(double(val));
                                        else
                                            dfTotalLine += double(val);
                                    }
                                }

                                // Right pixel
                                {
                                    const int iX = nSrcXOff2 - 1;
                                    const T val = pChunkShifted[iX];
                                    if (pabyChunkNodataMask
                                            [iX + static_cast<size_t>(iY) *
                                                      nChunkXSize])
                                    {
                                        nCount++;
                                        const double dfWeightX =
                                            pasSrcX[iDstPixel].dfRightWeight;
                                        dfTotalWeightLine += dfWeightX;
                                        if constexpr (bQuadraticMean)
                                            dfTotalLine +=
                                                SQUARE(double(val)) * dfWeightX;
                                        else
                                            dfTotalLine +=
                                                double(val) * dfWeightX;
                                    }
                                }
                            }

                            const double dfWeightY =
                                (iY == nSrcYOff)        ? dfBottomWeight
                                : (iY + 1 == nSrcYOff2) ? dfTopWeight
                                                        : 1.0;
                            dfTotal += dfTotalLine * dfWeightY;
                            dfTotalWeight += dfTotalWeightLine * dfWeightY;
                        }

                        if (nCount == 0 ||
                            (bPropagateNoData &&
                             nCount <
                                 static_cast<size_t>(nSrcYOff2 - nSrcYOff) *
                                     (nSrcXOff2 - nSrcXOff)))
                        {
                            pDstScanline[iDstPixel] = tNoDataValue;
                            continue;
                        }
                    }
                    if constexpr (eWrkDataType == GDT_UInt8)
                    {
                        T nVal;
                        if constexpr (bQuadraticMean)
                            nVal = ComputeIntegerRMS<T, int>(dfTotal,
                                                             dfTotalWeight);
                        else
                            nVal =
                                static_cast<T>(dfTotal / dfTotalWeight + 0.5);
                        if (bHasNoData && nVal == tNoDataValue)
                            nVal = tReplacementVal;
                        pDstScanline[iDstPixel] = nVal;
                    }
                    else if constexpr (eWrkDataType == GDT_UInt16)
                    {
                        T nVal;
                        if constexpr (bQuadraticMean)
                            nVal = ComputeIntegerRMS<T, uint64_t>(
                                dfTotal, dfTotalWeight);
                        else
                            nVal =
                                static_cast<T>(dfTotal / dfTotalWeight + 0.5);
                        if (bHasNoData && nVal == tNoDataValue)
                            nVal = tReplacementVal;
                        pDstScanline[iDstPixel] = nVal;
                    }
                    else
                    {
                        T nVal;
                        if constexpr (bQuadraticMean)
                        {
                            if constexpr (bUseMulFactor)
                                nVal = static_cast<T>(
                                    dfMulFactor *
                                    sqrt(dfTotal / dfTotalWeight));
                            else
                                nVal = static_cast<T>(
                                    sqrt(dfTotal / dfTotalWeight));
                        }
                        else
                        {
                            if constexpr (bUseMulFactor)
                                nVal = static_cast<T>(
                                    dfMulFactor * (dfTotal / dfTotalWeight));
                            else
                                nVal = static_cast<T>(dfTotal / dfTotalWeight);
                        }
                        if (bHasNoData && nVal == tNoDataValue)
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

            for (int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel)
            {
                const int nSrcXOff = pasSrcX[iDstPixel].nLeftXOffShifted;
                const int nSrcXOff2 = pasSrcX[iDstPixel].nRightXOffShifted;

                uint64_t nTotalR = 0;
                uint64_t nTotalG = 0;
                uint64_t nTotalB = 0;
                size_t nCount = 0;

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        const T val =
                            pChunk[iX + static_cast<size_t>(iY) * nChunkXSize];
                        // cppcheck-suppress unsignedLessThanZero
                        if (val < 0 || val >= colorEntries.size())
                            continue;
                        const size_t idx = static_cast<size_t>(val);
                        const auto &entry = colorEntries[idx];
                        if (entry.c4)
                        {
                            if constexpr (bQuadraticMean)
                            {
                                nTotalR += SQUARE<int>(entry.c1);
                                nTotalG += SQUARE<int>(entry.c2);
                                nTotalB += SQUARE<int>(entry.c3);
                                ++nCount;
                            }
                            else
                            {
                                nTotalR += entry.c1;
                                nTotalG += entry.c2;
                                nTotalB += entry.c3;
                                ++nCount;
                            }
                        }
                    }
                }

                if (nCount == 0 ||
                    (bPropagateNoData &&
                     nCount < static_cast<size_t>(nSrcYOff2 - nSrcYOff) *
                                  (nSrcXOff2 - nSrcXOff)))
                {
                    pDstScanline[iDstPixel] = tNoDataValue;
                }
                else
                {
                    GDALColorEntry color;
                    if constexpr (bQuadraticMean)
                    {
                        color.c1 =
                            static_cast<short>(sqrt(nTotalR / nCount) + 0.5);
                        color.c2 =
                            static_cast<short>(sqrt(nTotalG / nCount) + 0.5);
                        color.c3 =
                            static_cast<short>(sqrt(nTotalB / nCount) + 0.5);
                    }
                    else
                    {
                        color.c1 =
                            static_cast<short>((nTotalR + nCount / 2) / nCount);
                        color.c2 =
                            static_cast<short>((nTotalG + nCount / 2) / nCount);
                        color.c3 =
                            static_cast<short>((nTotalB + nCount / 2) / nCount);
                    }
                    pDstScanline[iDstPixel] =
                        static_cast<T>(BestColorEntry(colorEntries, color));
                }
            }
        }
    }

    CPLFree(pasSrcX);

    return CE_None;
}

template <bool bQuadraticMean>
static CPLErr
GDALResampleChunk_AverageOrRMSInternal(const GDALOverviewResampleArgs &args,
                                       const void *pChunk, void **ppDstBuffer,
                                       GDALDataType *peDstBufferDataType)
{
    *peDstBufferDataType = args.eWrkDataType;
    switch (args.eWrkDataType)
    {
        case GDT_UInt8:
        {
            return GDALResampleChunk_AverageOrRMS_T<GByte, int, GDT_UInt8,
                                                    bQuadraticMean>(
                args, static_cast<const GByte *>(pChunk), ppDstBuffer);
        }

        case GDT_UInt16:
        {
            if constexpr (bQuadraticMean)
            {
                // Use double as accumulation type, because UInt32 could overflow
                return GDALResampleChunk_AverageOrRMS_T<
                    GUInt16, double, GDT_UInt16, bQuadraticMean>(
                    args, static_cast<const GUInt16 *>(pChunk), ppDstBuffer);
            }
            else
            {
                return GDALResampleChunk_AverageOrRMS_T<
                    GUInt16, GUInt32, GDT_UInt16, bQuadraticMean>(
                    args, static_cast<const GUInt16 *>(pChunk), ppDstBuffer);
            }
        }

        case GDT_Float32:
        {
            return GDALResampleChunk_AverageOrRMS_T<float, double, GDT_Float32,
                                                    bQuadraticMean>(
                args, static_cast<const float *>(pChunk), ppDstBuffer);
        }

        case GDT_Float64:
        {
            return GDALResampleChunk_AverageOrRMS_T<double, double, GDT_Float64,
                                                    bQuadraticMean>(
                args, static_cast<const double *>(pChunk), ppDstBuffer);
        }

        default:
            break;
    }

    CPLAssert(false);
    return CE_Failure;
}

static CPLErr
GDALResampleChunk_AverageOrRMS(const GDALOverviewResampleArgs &args,
                               const void *pChunk, void **ppDstBuffer,
                               GDALDataType *peDstBufferDataType)
{
    if (EQUAL(args.pszResampling, "RMS"))
        return GDALResampleChunk_AverageOrRMSInternal<true>(
            args, pChunk, ppDstBuffer, peDstBufferDataType);
    else
        return GDALResampleChunk_AverageOrRMSInternal<false>(
            args, pChunk, ppDstBuffer, peDstBufferDataType);
}

/************************************************************************/
/*                      GDALResampleChunk_Gauss()                       */
/************************************************************************/

static CPLErr GDALResampleChunk_Gauss(const GDALOverviewResampleArgs &args,
                                      const void *pChunk, void **ppDstBuffer,
                                      GDALDataType *peDstBufferDataType)

{
    const double dfXRatioDstToSrc = args.dfXRatioDstToSrc;
    const double dfYRatioDstToSrc = args.dfYRatioDstToSrc;
    const GByte *pabyChunkNodataMask = args.pabyChunkNodataMask;
    const int nChunkXOff = args.nChunkXOff;
    const int nChunkXSize = args.nChunkXSize;
    const int nChunkYOff = args.nChunkYOff;
    const int nChunkYSize = args.nChunkYSize;
    const int nDstXOff = args.nDstXOff;
    const int nDstXOff2 = args.nDstXOff2;
    const int nDstYOff = args.nDstYOff;
    const int nDstYOff2 = args.nDstYOff2;
    const bool bHasNoData = args.bHasNoData;
    double dfNoDataValue = args.dfNoDataValue;
    const GDALColorTable *poColorTable = args.poColorTable;

    const double *const padfChunk = static_cast<const double *>(pChunk);

    *ppDstBuffer =
        VSI_MALLOC3_VERBOSE(nDstXOff2 - nDstXOff, nDstYOff2 - nDstYOff,
                            GDALGetDataTypeSizeBytes(GDT_Float64));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }
    *peDstBufferDataType = GDT_Float64;
    double *const padfDstBuffer = static_cast<double *>(*ppDstBuffer);

    /* -------------------------------------------------------------------- */
    /*      Create the filter kernel and allocate scanline buffer.          */
    /* -------------------------------------------------------------------- */
    int nGaussMatrixDim = 3;
    const int *panGaussMatrix;
    constexpr int anGaussMatrix3x3[] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
    constexpr int anGaussMatrix5x5[] = {1,  4, 6,  4,  1,  4, 16, 24, 16,
                                        4,  6, 24, 36, 24, 6, 4,  16, 24,
                                        16, 4, 1,  4,  6,  4, 1};
    constexpr int anGaussMatrix7x7[] = {
        1,   6,  15, 20,  15,  6,   1,   6,  36, 90,  120, 90,  36,
        6,   15, 90, 225, 300, 225, 90,  15, 20, 120, 300, 400, 300,
        120, 20, 15, 90,  225, 300, 225, 90, 15, 6,   36,  90,  120,
        90,  36, 6,  1,   6,   15,  20,  15, 6,  1};

    const int nOXSize = args.nOvrXSize;
    const int nOYSize = args.nOvrYSize;
    const int nResYFactor = static_cast<int>(0.5 + dfYRatioDstToSrc);

    // matrix for gauss filter
    if (nResYFactor <= 2)
    {
        panGaussMatrix = anGaussMatrix3x3;
        nGaussMatrixDim = 3;
    }
    else if (nResYFactor <= 4)
    {
        panGaussMatrix = anGaussMatrix5x5;
        nGaussMatrixDim = 5;
    }
    else
    {
        panGaussMatrix = anGaussMatrix7x7;
        nGaussMatrixDim = 7;
    }

#ifdef DEBUG_OUT_OF_BOUND_ACCESS
    int *panGaussMatrixDup = static_cast<int *>(
        CPLMalloc(sizeof(int) * nGaussMatrixDim * nGaussMatrixDim));
    memcpy(panGaussMatrixDup, panGaussMatrix,
           sizeof(int) * nGaussMatrixDim * nGaussMatrixDim);
    panGaussMatrix = panGaussMatrixDup;
#endif

    if (!bHasNoData)
        dfNoDataValue = 0.0;

    std::vector<GDALColorEntry> colorEntries;
    int nTransparentIdx = -1;
    if (poColorTable)
        colorEntries = ReadColorTable(*poColorTable, nTransparentIdx);

    // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
    // it as nodata value.
    if (bHasNoData && dfNoDataValue >= 0.0 &&
        dfNoDataValue < colorEntries.size())
        colorEntries[static_cast<int>(dfNoDataValue)].c4 = 0;

    // Or if we have no explicit nodata, but a color table entry that is
    // transparent, consider it as the nodata value.
    else if (!bHasNoData && nTransparentIdx >= 0)
    {
        dfNoDataValue = nTransparentIdx;
    }

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    const int nDstXWidth = nDstXOff2 - nDstXOff;

    /* ==================================================================== */
    /*      Loop over destination scanlines.                                */
    /* ==================================================================== */
    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        int nSrcYOff2 =
            static_cast<int>(0.5 + (iDstLine + 1) * dfYRatioDstToSrc) + 1;

        if (nSrcYOff < nChunkYOff)
        {
            nSrcYOff = nChunkYOff;
            nSrcYOff2++;
        }

        const int iSizeY = nSrcYOff2 - nSrcYOff;
        nSrcYOff = nSrcYOff + iSizeY / 2 - nGaussMatrixDim / 2;
        nSrcYOff2 = nSrcYOff + nGaussMatrixDim;

        if (nSrcYOff2 > nChunkBottomYOff ||
            (dfYRatioDstToSrc > 1 && iDstLine == nOYSize - 1))
        {
            nSrcYOff2 = std::min(nChunkBottomYOff, nSrcYOff + nGaussMatrixDim);
        }

        int nYShiftGaussMatrix = 0;
        if (nSrcYOff < nChunkYOff)
        {
            nYShiftGaussMatrix = -(nSrcYOff - nChunkYOff);
            nSrcYOff = nChunkYOff;
        }

        const double *const padfSrcScanline =
            padfChunk + ((nSrcYOff - nChunkYOff) * nChunkXSize);
        const GByte *pabySrcScanlineNodataMask = nullptr;
        if (pabyChunkNodataMask != nullptr)
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask + ((nSrcYOff - nChunkYOff) * nChunkXSize);

        /* --------------------------------------------------------------------
         */
        /*      Loop over destination pixels */
        /* --------------------------------------------------------------------
         */
        double *const padfDstScanline =
            padfDstBuffer + (iDstLine - nDstYOff) * nDstXWidth;
        for (int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel)
        {
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 =
                static_cast<int>(0.5 + (iDstPixel + 1) * dfXRatioDstToSrc) + 1;

            if (nSrcXOff < nChunkXOff)
            {
                nSrcXOff = nChunkXOff;
                nSrcXOff2++;
            }

            const int iSizeX = nSrcXOff2 - nSrcXOff;
            nSrcXOff = nSrcXOff + iSizeX / 2 - nGaussMatrixDim / 2;
            nSrcXOff2 = nSrcXOff + nGaussMatrixDim;

            if (nSrcXOff2 > nChunkRightXOff ||
                (dfXRatioDstToSrc > 1 && iDstPixel == nOXSize - 1))
            {
                nSrcXOff2 =
                    std::min(nChunkRightXOff, nSrcXOff + nGaussMatrixDim);
            }

            int nXShiftGaussMatrix = 0;
            if (nSrcXOff < nChunkXOff)
            {
                nXShiftGaussMatrix = -(nSrcXOff - nChunkXOff);
                nSrcXOff = nChunkXOff;
            }

            if (poColorTable == nullptr)
            {
                double dfTotal = 0.0;
                GInt64 nCount = 0;
                const int *panLineWeight =
                    panGaussMatrix + nYShiftGaussMatrix * nGaussMatrixDim +
                    nXShiftGaussMatrix;

                for (int j = 0, iY = nSrcYOff; iY < nSrcYOff2;
                     ++iY, ++j, panLineWeight += nGaussMatrixDim)
                {
                    for (int i = 0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i)
                    {
                        const double val =
                            padfSrcScanline[iX - nChunkXOff +
                                            static_cast<GPtrDiff_t>(iY -
                                                                    nSrcYOff) *
                                                nChunkXSize];
                        if (pabySrcScanlineNodataMask == nullptr ||
                            pabySrcScanlineNodataMask[iX - nChunkXOff +
                                                      static_cast<GPtrDiff_t>(
                                                          iY - nSrcYOff) *
                                                          nChunkXSize])
                        {
                            const int nWeight = panLineWeight[i];
                            dfTotal += val * nWeight;
                            nCount += nWeight;
                        }
                    }
                }

                if (nCount == 0)
                {
                    padfDstScanline[iDstPixel - nDstXOff] = dfNoDataValue;
                }
                else
                {
                    padfDstScanline[iDstPixel - nDstXOff] = dfTotal / nCount;
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

                for (int j = 0, iY = nSrcYOff; iY < nSrcYOff2;
                     ++iY, ++j, panLineWeight += nGaussMatrixDim)
                {
                    for (int i = 0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i)
                    {
                        const double val =
                            padfSrcScanline[iX - nChunkXOff +
                                            static_cast<GPtrDiff_t>(iY -
                                                                    nSrcYOff) *
                                                nChunkXSize];
                        if (val < 0 || val >= colorEntries.size())
                            continue;

                        size_t idx = static_cast<size_t>(val);
                        if (colorEntries[idx].c4)
                        {
                            const int nWeight = panLineWeight[i];
                            nTotalR +=
                                static_cast<GInt64>(colorEntries[idx].c1) *
                                nWeight;
                            nTotalG +=
                                static_cast<GInt64>(colorEntries[idx].c2) *
                                nWeight;
                            nTotalB +=
                                static_cast<GInt64>(colorEntries[idx].c3) *
                                nWeight;
                            nTotalWeight += nWeight;
                        }
                    }
                }

                if (nTotalWeight == 0)
                {
                    padfDstScanline[iDstPixel - nDstXOff] = dfNoDataValue;
                }
                else
                {
                    GDALColorEntry color;

                    color.c1 = static_cast<short>((nTotalR + nTotalWeight / 2) /
                                                  nTotalWeight);
                    color.c2 = static_cast<short>((nTotalG + nTotalWeight / 2) /
                                                  nTotalWeight);
                    color.c3 = static_cast<short>((nTotalB + nTotalWeight / 2) /
                                                  nTotalWeight);
                    padfDstScanline[iDstPixel - nDstXOff] =
                        BestColorEntry(colorEntries, color);
                }
            }
        }
    }

#ifdef DEBUG_OUT_OF_BOUND_ACCESS
    CPLFree(panGaussMatrixDup);
#endif

    return CE_None;
}

/************************************************************************/
/*                       GDALResampleChunk_Mode()                       */
/************************************************************************/

template <class T> static inline bool IsSame(T a, T b)
{
    return a == b;
}

template <> bool IsSame<GFloat16>(GFloat16 a, GFloat16 b)
{
    return a == b || (CPLIsNan(a) && CPLIsNan(b));
}

template <> bool IsSame<float>(float a, float b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

template <> bool IsSame<double>(double a, double b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

namespace
{
struct ComplexFloat16
{
    GFloat16 r;
    GFloat16 i;
};
}  // namespace

template <> bool IsSame<ComplexFloat16>(ComplexFloat16 a, ComplexFloat16 b)
{
    return (a.r == b.r && a.i == b.i) ||
           (CPLIsNan(a.r) && CPLIsNan(a.i) && CPLIsNan(b.r) && CPLIsNan(b.i));
}

template <>
bool IsSame<std::complex<float>>(std::complex<float> a, std::complex<float> b)
{
    return a == b || (std::isnan(a.real()) && std::isnan(a.imag()) &&
                      std::isnan(b.real()) && std::isnan(b.imag()));
}

template <>
bool IsSame<std::complex<double>>(std::complex<double> a,
                                  std::complex<double> b)
{
    return a == b || (std::isnan(a.real()) && std::isnan(a.imag()) &&
                      std::isnan(b.real()) && std::isnan(b.imag()));
}

template <class T>
static CPLErr GDALResampleChunk_ModeT(const GDALOverviewResampleArgs &args,
                                      const T *pChunk, T *const pDstBuffer)

{
    const double dfXRatioDstToSrc = args.dfXRatioDstToSrc;
    const double dfYRatioDstToSrc = args.dfYRatioDstToSrc;
    const double dfSrcXDelta = args.dfSrcXDelta;
    const double dfSrcYDelta = args.dfSrcYDelta;
    const GByte *pabyChunkNodataMask = args.pabyChunkNodataMask;
    const int nChunkXOff = args.nChunkXOff;
    const int nChunkXSize = args.nChunkXSize;
    const int nChunkYOff = args.nChunkYOff;
    const int nChunkYSize = args.nChunkYSize;
    const int nDstXOff = args.nDstXOff;
    const int nDstXOff2 = args.nDstXOff2;
    const int nDstYOff = args.nDstYOff;
    const int nDstYOff2 = args.nDstYOff2;
    const bool bHasNoData = args.bHasNoData;
    const GDALColorTable *poColorTable = args.poColorTable;
    const int nDstXSize = nDstXOff2 - nDstXOff;

    T tNoDataValue;
    if constexpr (std::is_same<T, ComplexFloat16>::value)
    {
        tNoDataValue.r = cpl::NumericLimits<GFloat16>::quiet_NaN();
        tNoDataValue.i = cpl::NumericLimits<GFloat16>::quiet_NaN();
    }
    else if constexpr (std::is_same<T, std::complex<float>>::value ||
                       std::is_same<T, std::complex<double>>::value)
    {
        using BaseT = typename T::value_type;
        tNoDataValue =
            std::complex<BaseT>(std::numeric_limits<BaseT>::quiet_NaN(),
                                std::numeric_limits<BaseT>::quiet_NaN());
    }
    else if (!bHasNoData || !GDALIsValueInRange<T>(args.dfNoDataValue))
        tNoDataValue = 0;
    else
        tNoDataValue = static_cast<T>(args.dfNoDataValue);

    using CountType = uint32_t;
    CountType nMaxNumPx = 0;
    T *paVals = nullptr;
    CountType *panCounts = nullptr;

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    std::vector<int> anVals(256, 0);

    /* ==================================================================== */
    /*      Loop over destination scanlines.                                */
    /* ==================================================================== */
    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        const double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if (dfYRatioDstToSrc > 1 && dfSrcYOff - nSrcYOff > 0.9 &&
            nSrcYOff < nChunkBottomYOff)
            nSrcYOff++;
#endif
        if (nSrcYOff < nChunkYOff)
            nSrcYOff = nChunkYOff;

        const double dfSrcYOff2 =
            dfSrcYDelta + (iDstLine + 1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if (dfYRatioDstToSrc > 1 && nSrcYOff2 - dfSrcYOff2 > 0.9 &&
            nSrcYOff2 > nChunkYOff)
            nSrcYOff2--;
#endif
        if (nSrcYOff2 == nSrcYOff)
            ++nSrcYOff2;
        if (nSrcYOff2 > nChunkBottomYOff)
            nSrcYOff2 = nChunkBottomYOff;

        const T *const paSrcScanline =
            pChunk +
            (static_cast<GPtrDiff_t>(nSrcYOff - nChunkYOff) * nChunkXSize);
        const GByte *pabySrcScanlineNodataMask = nullptr;
        if (pabyChunkNodataMask != nullptr)
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask +
                static_cast<GPtrDiff_t>(nSrcYOff - nChunkYOff) * nChunkXSize;

        T *const paDstScanline = pDstBuffer + (iDstLine - nDstYOff) * nDstXSize;
        /* --------------------------------------------------------------------
         */
        /*      Loop over destination pixels */
        /* --------------------------------------------------------------------
         */
        for (int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel)
        {
            const double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
            // Apply some epsilon to avoid numerical precision issues
            int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a
            // tiny participation in the resulting pixel
            if (dfXRatioDstToSrc > 1 && dfSrcXOff - nSrcXOff > 0.9 &&
                nSrcXOff < nChunkRightXOff)
                nSrcXOff++;
#endif
            if (nSrcXOff < nChunkXOff)
                nSrcXOff = nChunkXOff;

            const double dfSrcXOff2 =
                dfSrcXDelta + (iDstPixel + 1) * dfXRatioDstToSrc;
            int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a
            // tiny participation in the resulting pixel
            if (dfXRatioDstToSrc > 1 && nSrcXOff2 - dfSrcXOff2 > 0.9 &&
                nSrcXOff2 > nChunkXOff)
                nSrcXOff2--;
#endif
            if (nSrcXOff2 == nSrcXOff)
                nSrcXOff2++;
            if (nSrcXOff2 > nChunkRightXOff)
                nSrcXOff2 = nChunkRightXOff;

            bool bRegularProcessing = false;
            if constexpr (!std::is_same<T, GByte>::value)
                bRegularProcessing = true;
            else if (poColorTable && poColorTable->GetColorEntryCount() > 256)
                bRegularProcessing = true;

            if (bRegularProcessing)
            {
                // Sanity check to make sure the allocation of paVals and
                // panCounts don't overflow.
                static_assert(sizeof(CountType) <= sizeof(size_t));
                if (nSrcYOff2 - nSrcYOff <= 0 || nSrcXOff2 - nSrcXOff <= 0 ||
                    static_cast<CountType>(nSrcYOff2 - nSrcYOff) >
                        (std::numeric_limits<CountType>::max() /
                         std::max(sizeof(T), sizeof(CountType))) /
                            static_cast<CountType>(nSrcXOff2 - nSrcXOff))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big downsampling factor");
                    CPLFree(paVals);
                    CPLFree(panCounts);
                    return CE_Failure;
                }
                const CountType nNumPx =
                    static_cast<CountType>(nSrcYOff2 - nSrcYOff) *
                    (nSrcXOff2 - nSrcXOff);
                CountType iMaxInd = 0;
                CountType iMaxVal = 0;

                if (paVals == nullptr || nNumPx > nMaxNumPx)
                {
                    T *paValsNew = static_cast<T *>(
                        VSI_REALLOC_VERBOSE(paVals, nNumPx * sizeof(T)));
                    CountType *panCountsNew =
                        static_cast<CountType *>(VSI_REALLOC_VERBOSE(
                            panCounts, nNumPx * sizeof(CountType)));
                    if (paValsNew != nullptr)
                        paVals = paValsNew;
                    if (panCountsNew != nullptr)
                        panCounts = panCountsNew;
                    if (paValsNew == nullptr || panCountsNew == nullptr)
                    {
                        CPLFree(paVals);
                        CPLFree(panCounts);
                        return CE_Failure;
                    }
                    nMaxNumPx = nNumPx;
                }

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    const GPtrDiff_t iTotYOff =
                        static_cast<GPtrDiff_t>(iY - nSrcYOff) * nChunkXSize -
                        nChunkXOff;
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        if (pabySrcScanlineNodataMask == nullptr ||
                            pabySrcScanlineNodataMask[iX + iTotYOff])
                        {
                            const T val = paSrcScanline[iX + iTotYOff];
                            CountType i = 0;  // Used after for.

                            // Check array for existing entry.
                            for (; i < iMaxInd; ++i)
                            {
                                if (IsSame(paVals[i], val))
                                {
                                    if (++panCounts[i] > panCounts[iMaxVal])
                                    {
                                        iMaxVal = i;
                                    }
                                    break;
                                }
                            }

                            // Add to arr if entry not already there.
                            if (i == iMaxInd)
                            {
                                paVals[iMaxInd] = val;
                                panCounts[iMaxInd] = 1;

                                if (iMaxInd == 0)
                                {
                                    iMaxVal = iMaxInd;
                                }

                                ++iMaxInd;
                            }
                        }
                    }
                }

                if (iMaxInd == 0)
                    paDstScanline[iDstPixel - nDstXOff] = tNoDataValue;
                else
                    paDstScanline[iDstPixel - nDstXOff] = paVals[iMaxVal];
            }
            else if constexpr (std::is_same<T, GByte>::value)
            // ( eSrcDataType == GDT_UInt8 && nEntryCount < 256 )
            {
                // So we go here for a paletted or non-paletted byte band.
                // The input values are then between 0 and 255.
                int nMaxVal = 0;
                int iMaxInd = -1;

                // The cost of this zeroing might be high. Perhaps we should
                // just use the above generic case, and go to this one if the
                // number of source pixels is large enough
                std::fill(anVals.begin(), anVals.end(), 0);

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    const GPtrDiff_t iTotYOff =
                        static_cast<GPtrDiff_t>(iY - nSrcYOff) * nChunkXSize -
                        nChunkXOff;
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        const T val = paSrcScanline[iX + iTotYOff];
                        if (!bHasNoData || val != tNoDataValue)
                        {
                            int nVal = static_cast<int>(val);
                            if (++anVals[nVal] > nMaxVal)
                            {
                                // Sum the density.
                                // Is it the most common value so far?
                                iMaxInd = nVal;
                                nMaxVal = anVals[nVal];
                            }
                        }
                    }
                }

                if (iMaxInd == -1)
                    paDstScanline[iDstPixel - nDstXOff] = tNoDataValue;
                else
                    paDstScanline[iDstPixel - nDstXOff] =
                        static_cast<T>(iMaxInd);
            }
        }
    }

    CPLFree(paVals);
    CPLFree(panCounts);

    return CE_None;
}

static CPLErr GDALResampleChunk_Mode(const GDALOverviewResampleArgs &args,
                                     const void *pChunk, void **ppDstBuffer,
                                     GDALDataType *peDstBufferDataType)
{
    *ppDstBuffer = VSI_MALLOC3_VERBOSE(
        args.nDstXOff2 - args.nDstXOff, args.nDstYOff2 - args.nDstYOff,
        GDALGetDataTypeSizeBytes(args.eWrkDataType));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }

    CPLAssert(args.eSrcDataType == args.eWrkDataType);

    *peDstBufferDataType = args.eWrkDataType;
    switch (args.eWrkDataType)
    {
        // For mode resampling, as no computation is done, only the
        // size of the data type matters... except for Byte where we have
        // special processing. And for floating point values
        case GDT_UInt8:
        {
            return GDALResampleChunk_ModeT(args,
                                           static_cast<const GByte *>(pChunk),
                                           static_cast<GByte *>(*ppDstBuffer));
        }

        case GDT_Int8:
        {
            return GDALResampleChunk_ModeT(args,
                                           static_cast<const int8_t *>(pChunk),
                                           static_cast<int8_t *>(*ppDstBuffer));
        }

        case GDT_Int16:
        case GDT_UInt16:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 2);
            return GDALResampleChunk_ModeT(
                args, static_cast<const uint16_t *>(pChunk),
                static_cast<uint16_t *>(*ppDstBuffer));
        }

        case GDT_CInt16:
        case GDT_Int32:
        case GDT_UInt32:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 4);
            return GDALResampleChunk_ModeT(
                args, static_cast<const uint32_t *>(pChunk),
                static_cast<uint32_t *>(*ppDstBuffer));
        }

        case GDT_CInt32:
        case GDT_Int64:
        case GDT_UInt64:
        {
            CPLAssert(GDALGetDataTypeSizeBytes(args.eWrkDataType) == 8);
            return GDALResampleChunk_ModeT(
                args, static_cast<const uint64_t *>(pChunk),
                static_cast<uint64_t *>(*ppDstBuffer));
        }

        case GDT_Float16:
        {
            return GDALResampleChunk_ModeT(
                args, static_cast<const GFloat16 *>(pChunk),
                static_cast<GFloat16 *>(*ppDstBuffer));
        }

        case GDT_Float32:
        {
            return GDALResampleChunk_ModeT(args,
                                           static_cast<const float *>(pChunk),
                                           static_cast<float *>(*ppDstBuffer));
        }

        case GDT_Float64:
        {
            return GDALResampleChunk_ModeT(args,
                                           static_cast<const double *>(pChunk),
                                           static_cast<double *>(*ppDstBuffer));
        }

        case GDT_CFloat16:
        {
            return GDALResampleChunk_ModeT(
                args, static_cast<const ComplexFloat16 *>(pChunk),
                static_cast<ComplexFloat16 *>(*ppDstBuffer));
        }

        case GDT_CFloat32:
        {
            return GDALResampleChunk_ModeT(
                args, static_cast<const std::complex<float> *>(pChunk),
                static_cast<std::complex<float> *>(*ppDstBuffer));
        }

        case GDT_CFloat64:
        {
            return GDALResampleChunk_ModeT(
                args, static_cast<const std::complex<double> *>(pChunk),
                static_cast<std::complex<double> *>(*ppDstBuffer));
        }

        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                 GDALResampleConvolutionHorizontal()                  */
/************************************************************************/

template <class T>
static inline double
GDALResampleConvolutionHorizontal(const T *pChunk, const double *padfWeights,
                                  int nSrcPixelCount)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;  // Used after for.
    // Intel Compiler 2024.0.2.29 (maybe other versions?) crashes on this
    // manually (untypical) unrolled loop in -O2 and -O3:
    // https://github.com/OSGeo/gdal/issues/9508
#if !defined(__INTEL_CLANG_COMPILER)
    for (; i < nSrcPixelCount - 3; i += 4)
    {
        dfVal1 += double(pChunk[i + 0]) * padfWeights[i];
        dfVal1 += double(pChunk[i + 1]) * padfWeights[i + 1];
        dfVal2 += double(pChunk[i + 2]) * padfWeights[i + 2];
        dfVal2 += double(pChunk[i + 3]) * padfWeights[i + 3];
    }
#endif
    for (; i < nSrcPixelCount; ++i)
    {
        dfVal1 += double(pChunk[i]) * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template <class T>
static inline void GDALResampleConvolutionHorizontalWithMask(
    const T *pChunk, const GByte *pabyMask, const double *padfWeights,
    int nSrcPixelCount, double &dfVal, double &dfWeightSum)
{
    dfVal = 0;
    dfWeightSum = 0;
    int i = 0;
    for (; i < nSrcPixelCount - 3; i += 4)
    {
        const double dfWeight0 = padfWeights[i] * pabyMask[i];
        const double dfWeight1 = padfWeights[i + 1] * pabyMask[i + 1];
        const double dfWeight2 = padfWeights[i + 2] * pabyMask[i + 2];
        const double dfWeight3 = padfWeights[i + 3] * pabyMask[i + 3];
        dfVal += double(pChunk[i + 0]) * dfWeight0;
        dfVal += double(pChunk[i + 1]) * dfWeight1;
        dfVal += double(pChunk[i + 2]) * dfWeight2;
        dfVal += double(pChunk[i + 3]) * dfWeight3;
        dfWeightSum += dfWeight0 + dfWeight1 + dfWeight2 + dfWeight3;
    }
    for (; i < nSrcPixelCount; ++i)
    {
        const double dfWeight = padfWeights[i] * pabyMask[i];
        dfVal += double(pChunk[i]) * dfWeight;
        dfWeightSum += dfWeight;
    }
}

template <class T>
static inline void GDALResampleConvolutionHorizontal_3rows(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeights, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    double dfVal5 = 0.0;
    double dfVal6 = 0.0;
    int i = 0;  // Used after for.
    for (; i < nSrcPixelCount - 3; i += 4)
    {
        dfVal1 += double(pChunkRow1[i + 0]) * padfWeights[i + 0];
        dfVal1 += double(pChunkRow1[i + 1]) * padfWeights[i + 1];
        dfVal2 += double(pChunkRow1[i + 2]) * padfWeights[i + 2];
        dfVal2 += double(pChunkRow1[i + 3]) * padfWeights[i + 3];
        dfVal3 += double(pChunkRow2[i + 0]) * padfWeights[i + 0];
        dfVal3 += double(pChunkRow2[i + 1]) * padfWeights[i + 1];
        dfVal4 += double(pChunkRow2[i + 2]) * padfWeights[i + 2];
        dfVal4 += double(pChunkRow2[i + 3]) * padfWeights[i + 3];
        dfVal5 += double(pChunkRow3[i + 0]) * padfWeights[i + 0];
        dfVal5 += double(pChunkRow3[i + 1]) * padfWeights[i + 1];
        dfVal6 += double(pChunkRow3[i + 2]) * padfWeights[i + 2];
        dfVal6 += double(pChunkRow3[i + 3]) * padfWeights[i + 3];
    }
    for (; i < nSrcPixelCount; ++i)
    {
        dfVal1 += double(pChunkRow1[i]) * padfWeights[i];
        dfVal3 += double(pChunkRow2[i]) * padfWeights[i];
        dfVal5 += double(pChunkRow3[i]) * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
    dfRes3 = dfVal5 + dfVal6;
}

template <class T>
static inline void GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeights, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontal_3rows(pChunkRow1, pChunkRow2, pChunkRow3,
                                            padfWeights, nSrcPixelCount, dfRes1,
                                            dfRes2, dfRes3);
}

template <class T>
static inline void GDALResampleConvolutionHorizontalPixelCount4_3rows(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeights, double &dfRes1, double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontal_3rows(pChunkRow1, pChunkRow2, pChunkRow3,
                                            padfWeights, 4, dfRes1, dfRes2,
                                            dfRes3);
}

/************************************************************************/
/*                  GDALResampleConvolutionVertical()                   */
/************************************************************************/

template <class T>
static inline double
GDALResampleConvolutionVertical(const T *pChunk, size_t nStride,
                                const double *padfWeights, int nSrcLineCount)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;
    size_t j = 0;
    for (; i < nSrcLineCount - 3; i += 4, j += 4 * nStride)
    {
        dfVal1 += pChunk[j + 0 * nStride] * padfWeights[i + 0];
        dfVal1 += pChunk[j + 1 * nStride] * padfWeights[i + 1];
        dfVal2 += pChunk[j + 2 * nStride] * padfWeights[i + 2];
        dfVal2 += pChunk[j + 3 * nStride] * padfWeights[i + 3];
    }
    for (; i < nSrcLineCount; ++i, j += nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template <class T>
static inline void GDALResampleConvolutionVertical_2cols(
    const T *pChunk, size_t nStride, const double *padfWeights,
    int nSrcLineCount, double &dfRes1, double &dfRes2)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    int i = 0;
    size_t j = 0;
    for (; i < nSrcLineCount - 3; i += 4, j += 4 * nStride)
    {
        dfVal1 += pChunk[j + 0 + 0 * nStride] * padfWeights[i + 0];
        dfVal3 += pChunk[j + 1 + 0 * nStride] * padfWeights[i + 0];
        dfVal1 += pChunk[j + 0 + 1 * nStride] * padfWeights[i + 1];
        dfVal3 += pChunk[j + 1 + 1 * nStride] * padfWeights[i + 1];
        dfVal2 += pChunk[j + 0 + 2 * nStride] * padfWeights[i + 2];
        dfVal4 += pChunk[j + 1 + 2 * nStride] * padfWeights[i + 2];
        dfVal2 += pChunk[j + 0 + 3 * nStride] * padfWeights[i + 3];
        dfVal4 += pChunk[j + 1 + 3 * nStride] * padfWeights[i + 3];
    }
    for (; i < nSrcLineCount; ++i, j += nStride)
    {
        dfVal1 += pChunk[j + 0] * padfWeights[i];
        dfVal3 += pChunk[j + 1] * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
}

#ifdef USE_SSE2

#ifdef __AVX__
/************************************************************************/
/*              GDALResampleConvolutionVertical_16cols<T>               */
/************************************************************************/

template <class T>
static inline void
GDALResampleConvolutionVertical_16cols(const T *pChunk, size_t nStride,
                                       const double *padfWeights,
                                       int nSrcLineCount, float *afDest)
{
    int i = 0;
    size_t j = 0;
    XMMReg4Double v_acc0 = XMMReg4Double::Zero();
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    XMMReg4Double v_acc3 = XMMReg4Double::Zero();
    for (; i < nSrcLineCount - 3; i += 4, j += 4 * nStride)
    {
        XMMReg4Double w0 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 0);
        XMMReg4Double w1 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 1);
        XMMReg4Double w2 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 2);
        XMMReg4Double w3 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 3);
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 0 * nStride) * w0;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 0 * nStride) * w0;
        v_acc2 += XMMReg4Double::Load4Val(pChunk + j + 8 + 0 * nStride) * w0;
        v_acc3 += XMMReg4Double::Load4Val(pChunk + j + 12 + 0 * nStride) * w0;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 1 * nStride) * w1;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 1 * nStride) * w1;
        v_acc2 += XMMReg4Double::Load4Val(pChunk + j + 8 + 1 * nStride) * w1;
        v_acc3 += XMMReg4Double::Load4Val(pChunk + j + 12 + 1 * nStride) * w1;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 2 * nStride) * w2;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 2 * nStride) * w2;
        v_acc2 += XMMReg4Double::Load4Val(pChunk + j + 8 + 2 * nStride) * w2;
        v_acc3 += XMMReg4Double::Load4Val(pChunk + j + 12 + 2 * nStride) * w2;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 3 * nStride) * w3;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 3 * nStride) * w3;
        v_acc2 += XMMReg4Double::Load4Val(pChunk + j + 8 + 3 * nStride) * w3;
        v_acc3 += XMMReg4Double::Load4Val(pChunk + j + 12 + 3 * nStride) * w3;
    }
    for (; i < nSrcLineCount; ++i, j += nStride)
    {
        XMMReg4Double w = XMMReg4Double::Load1ValHighAndLow(padfWeights + i);
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0) * w;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4) * w;
        v_acc2 += XMMReg4Double::Load4Val(pChunk + j + 8) * w;
        v_acc3 += XMMReg4Double::Load4Val(pChunk + j + 12) * w;
    }
    v_acc0.Store4Val(afDest);
    v_acc1.Store4Val(afDest + 4);
    v_acc2.Store4Val(afDest + 8);
    v_acc3.Store4Val(afDest + 12);
}

template <class T>
static inline void GDALResampleConvolutionVertical_16cols(const T *, int,
                                                          const double *, int,
                                                          double *)
{
    // Cannot be reached
    CPLAssert(false);
}

#else

/************************************************************************/
/*               GDALResampleConvolutionVertical_8cols<T>               */
/************************************************************************/

template <class T>
static inline void
GDALResampleConvolutionVertical_8cols(const T *pChunk, size_t nStride,
                                      const double *padfWeights,
                                      int nSrcLineCount, float *afDest)
{
    int i = 0;
    size_t j = 0;
    XMMReg4Double v_acc0 = XMMReg4Double::Zero();
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    for (; i < nSrcLineCount - 3; i += 4, j += 4 * nStride)
    {
        XMMReg4Double w0 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 0);
        XMMReg4Double w1 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 1);
        XMMReg4Double w2 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 2);
        XMMReg4Double w3 =
            XMMReg4Double::Load1ValHighAndLow(padfWeights + i + 3);
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 0 * nStride) * w0;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 0 * nStride) * w0;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 1 * nStride) * w1;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 1 * nStride) * w1;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 2 * nStride) * w2;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 2 * nStride) * w2;
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0 + 3 * nStride) * w3;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4 + 3 * nStride) * w3;
    }
    for (; i < nSrcLineCount; ++i, j += nStride)
    {
        XMMReg4Double w = XMMReg4Double::Load1ValHighAndLow(padfWeights + i);
        v_acc0 += XMMReg4Double::Load4Val(pChunk + j + 0) * w;
        v_acc1 += XMMReg4Double::Load4Val(pChunk + j + 4) * w;
    }
    v_acc0.Store4Val(afDest);
    v_acc1.Store4Val(afDest + 4);
}

template <class T>
static inline void GDALResampleConvolutionVertical_8cols(const T *, int,
                                                         const double *, int,
                                                         double *)
{
    // Cannot be reached
    CPLAssert(false);
}

#endif  // __AVX__

/************************************************************************/
/*               GDALResampleConvolutionHorizontalSSE2<T>               */
/************************************************************************/

template <class T>
static inline double GDALResampleConvolutionHorizontalSSE2(
    const T *pChunk, const double *padfWeightsAligned, int nSrcPixelCount)
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    int i = 0;  // Used after for.
    for (; i < nSrcPixelCount - 7; i += 8)
    {
        // Retrieve the pixel & accumulate
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunk + i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunk + i + 4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i + 4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;
    }

    v_acc1 += v_acc2;

    double dfVal = v_acc1.GetHorizSum();
    for (; i < nSrcPixelCount; ++i)
    {
        dfVal += pChunk[i] * padfWeightsAligned[i];
    }
    return dfVal;
}

/************************************************************************/
/*               GDALResampleConvolutionHorizontal<GByte>               */
/************************************************************************/

template <>
inline double GDALResampleConvolutionHorizontal<GByte>(
    const GByte *pChunk, const double *padfWeightsAligned, int nSrcPixelCount)
{
    return GDALResampleConvolutionHorizontalSSE2(pChunk, padfWeightsAligned,
                                                 nSrcPixelCount);
}

template <>
inline double GDALResampleConvolutionHorizontal<GUInt16>(
    const GUInt16 *pChunk, const double *padfWeightsAligned, int nSrcPixelCount)
{
    return GDALResampleConvolutionHorizontalSSE2(pChunk, padfWeightsAligned,
                                                 nSrcPixelCount);
}

/************************************************************************/
/*           GDALResampleConvolutionHorizontalWithMaskSSE2<T>           */
/************************************************************************/

template <class T>
static inline void GDALResampleConvolutionHorizontalWithMaskSSE2(
    const T *pChunk, const GByte *pabyMask, const double *padfWeightsAligned,
    int nSrcPixelCount, double &dfVal, double &dfWeightSum)
{
    int i = 0;  // Used after for.
    XMMReg4Double v_acc = XMMReg4Double::Zero();
    XMMReg4Double v_acc_weight = XMMReg4Double::Zero();
    for (; i < nSrcPixelCount - 3; i += 4)
    {
        const XMMReg4Double v_pixels = XMMReg4Double::Load4Val(pChunk + i);
        const XMMReg4Double v_mask = XMMReg4Double::Load4Val(pabyMask + i);
        XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);
        v_weight *= v_mask;
        v_acc += v_pixels * v_weight;
        v_acc_weight += v_weight;
    }

    dfVal = v_acc.GetHorizSum();
    dfWeightSum = v_acc_weight.GetHorizSum();
    for (; i < nSrcPixelCount; ++i)
    {
        const double dfWeight = padfWeightsAligned[i] * pabyMask[i];
        dfVal += pChunk[i] * dfWeight;
        dfWeightSum += dfWeight;
    }
}

/************************************************************************/
/*           GDALResampleConvolutionHorizontalWithMask<GByte>           */
/************************************************************************/

template <>
inline void GDALResampleConvolutionHorizontalWithMask<GByte>(
    const GByte *pChunk, const GByte *pabyMask,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfVal,
    double &dfWeightSum)
{
    GDALResampleConvolutionHorizontalWithMaskSSE2(
        pChunk, pabyMask, padfWeightsAligned, nSrcPixelCount, dfVal,
        dfWeightSum);
}

template <>
inline void GDALResampleConvolutionHorizontalWithMask<GUInt16>(
    const GUInt16 *pChunk, const GByte *pabyMask,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfVal,
    double &dfWeightSum)
{
    GDALResampleConvolutionHorizontalWithMaskSSE2(
        pChunk, pabyMask, padfWeightsAligned, nSrcPixelCount, dfVal,
        dfWeightSum);
}

/************************************************************************/
/*           GDALResampleConvolutionHorizontal_3rows_SSE2<T>            */
/************************************************************************/

template <class T>
static inline void GDALResampleConvolutionHorizontal_3rows_SSE2(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero(),
                  v_acc2 = XMMReg4Double::Zero(),
                  v_acc3 = XMMReg4Double::Zero();
    int i = 0;
    for (; i < nSrcPixelCount - 7; i += 8)
    {
        // Retrieve the pixel & accumulate.
        XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1 + i);
        XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow1 + i + 4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i + 4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc1 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow2 + i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2 + i + 4);
        v_acc2 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow3 + i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow3 + i + 4);
        v_acc3 += v_pixels1 * v_weight1;
        v_acc3 += v_pixels2 * v_weight2;
    }

    dfRes1 = v_acc1.GetHorizSum();
    dfRes2 = v_acc2.GetHorizSum();
    dfRes3 = v_acc3.GetHorizSum();
    for (; i < nSrcPixelCount; ++i)
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*            GDALResampleConvolutionHorizontal_3rows<GByte>            */
/************************************************************************/

template <>
inline void GDALResampleConvolutionHorizontal_3rows<GByte>(
    const GByte *pChunkRow1, const GByte *pChunkRow2, const GByte *pChunkRow3,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

template <>
inline void GDALResampleConvolutionHorizontal_3rows<GUInt16>(
    const GUInt16 *pChunkRow1, const GUInt16 *pChunkRow2,
    const GUInt16 *pChunkRow3, const double *padfWeightsAligned,
    int nSrcPixelCount, double &dfRes1, double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

/************************************************************************/
/*    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2<T>    */
/************************************************************************/

template <class T>
static inline void GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    XMMReg4Double v_acc3 = XMMReg4Double::Zero();
    int i = 0;  // Use after for.
    for (; i < nSrcPixelCount - 3; i += 4)
    {
        // Retrieve the pixel & accumulate.
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1 + i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2 + i);
        const XMMReg4Double v_pixels3 = XMMReg4Double::Load4Val(pChunkRow3 + i);
        const XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);

        v_acc1 += v_pixels1 * v_weight;
        v_acc2 += v_pixels2 * v_weight;
        v_acc3 += v_pixels3 * v_weight;
    }

    dfRes1 = v_acc1.GetHorizSum();
    dfRes2 = v_acc2.GetHorizSum();
    dfRes3 = v_acc3.GetHorizSum();

    for (; i < nSrcPixelCount; ++i)
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*    GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>     */
/************************************************************************/

template <>
inline void GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>(
    const GByte *pChunkRow1, const GByte *pChunkRow2, const GByte *pChunkRow3,
    const double *padfWeightsAligned, int nSrcPixelCount, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

template <>
inline void GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GUInt16>(
    const GUInt16 *pChunkRow1, const GUInt16 *pChunkRow2,
    const GUInt16 *pChunkRow3, const double *padfWeightsAligned,
    int nSrcPixelCount, double &dfRes1, double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

/************************************************************************/
/*      GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2<T>      */
/************************************************************************/

template <class T>
static inline void GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
    const T *pChunkRow1, const T *pChunkRow2, const T *pChunkRow3,
    const double *padfWeightsAligned, double &dfRes1, double &dfRes2,
    double &dfRes3)
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
/*      GDALResampleConvolutionHorizontalPixelCount4_3rows<GByte>       */
/************************************************************************/

template <>
inline void GDALResampleConvolutionHorizontalPixelCount4_3rows<GByte>(
    const GByte *pChunkRow1, const GByte *pChunkRow2, const GByte *pChunkRow3,
    const double *padfWeightsAligned, double &dfRes1, double &dfRes2,
    double &dfRes3)
{
    GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, dfRes1, dfRes2,
        dfRes3);
}

template <>
inline void GDALResampleConvolutionHorizontalPixelCount4_3rows<GUInt16>(
    const GUInt16 *pChunkRow1, const GUInt16 *pChunkRow2,
    const GUInt16 *pChunkRow3, const double *padfWeightsAligned, double &dfRes1,
    double &dfRes2, double &dfRes3)
{
    GDALResampleConvolutionHorizontalPixelCount4_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3, padfWeightsAligned, dfRes1, dfRes2,
        dfRes3);
}

#endif  // USE_SSE2

/************************************************************************/
/*                   GDALResampleChunk_Convolution()                    */
/************************************************************************/

template <class T, class Twork, GDALDataType eWrkDataType,
          bool bKernelWithNegativeWeights, bool bNeedRescale>
static CPLErr GDALResampleChunk_ConvolutionT(
    const GDALOverviewResampleArgs &args, const T *pChunk, void *pDstBuffer,
    FilterFuncType pfnFilterFunc, FilterFunc4ValuesType pfnFilterFunc4Values,
    int nKernelRadius, float fMaxVal)

{
    const double dfXRatioDstToSrc = args.dfXRatioDstToSrc;
    const double dfYRatioDstToSrc = args.dfYRatioDstToSrc;
    const double dfSrcXDelta = args.dfSrcXDelta;
    const double dfSrcYDelta = args.dfSrcYDelta;
    constexpr int nBands = 1;
    const GByte *pabyChunkNodataMask = args.pabyChunkNodataMask;
    const int nChunkXOff = args.nChunkXOff;
    const int nChunkXSize = args.nChunkXSize;
    const int nChunkYOff = args.nChunkYOff;
    const int nChunkYSize = args.nChunkYSize;
    const int nDstXOff = args.nDstXOff;
    const int nDstXOff2 = args.nDstXOff2;
    const int nDstYOff = args.nDstYOff;
    const int nDstYOff2 = args.nDstYOff2;
    const bool bHasNoData = args.bHasNoData;
    double dfNoDataValue = args.dfNoDataValue;

    if (!bHasNoData)
        dfNoDataValue = 0.0;
    const auto dstDataType = args.eOvrDataType;
    const int nDstDataTypeSize = GDALGetDataTypeSizeBytes(dstDataType);
    const double dfReplacementVal =
        bHasNoData ? GDALGetNoDataReplacementValue(dstDataType, dfNoDataValue)
                   : dfNoDataValue;
    // cppcheck-suppress unreadVariable
    const int isIntegerDT = GDALDataTypeIsInteger(dstDataType);
    const bool bNoDataValueInt64Valid =
        isIntegerDT && GDALIsValueExactAs<GInt64>(dfNoDataValue);
    const auto nNodataValueInt64 =
        bNoDataValueInt64Valid ? static_cast<GInt64>(dfNoDataValue) : 0;
    constexpr int nWrkDataTypeSize = static_cast<int>(sizeof(Twork));

    // TODO: we should have some generic function to do this.
    Twork fDstMin = cpl::NumericLimits<Twork>::lowest();
    Twork fDstMax = cpl::NumericLimits<Twork>::max();
    if (dstDataType == GDT_UInt8)
    {
        fDstMin = std::numeric_limits<GByte>::min();
        fDstMax = std::numeric_limits<GByte>::max();
    }
    else if (dstDataType == GDT_Int8)
    {
        fDstMin = std::numeric_limits<GInt8>::min();
        fDstMax = std::numeric_limits<GInt8>::max();
    }
    else if (dstDataType == GDT_UInt16)
    {
        fDstMin = std::numeric_limits<GUInt16>::min();
        fDstMax = std::numeric_limits<GUInt16>::max();
    }
    else if (dstDataType == GDT_Int16)
    {
        fDstMin = std::numeric_limits<GInt16>::min();
        fDstMax = std::numeric_limits<GInt16>::max();
    }
    else if (dstDataType == GDT_UInt32)
    {
        fDstMin = static_cast<Twork>(std::numeric_limits<GUInt32>::min());
        fDstMax = static_cast<Twork>(std::numeric_limits<GUInt32>::max());
    }
    else if (dstDataType == GDT_Int32)
    {
        // cppcheck-suppress unreadVariable
        fDstMin = static_cast<Twork>(std::numeric_limits<GInt32>::min());
        // cppcheck-suppress unreadVariable
        fDstMax = static_cast<Twork>(std::numeric_limits<GInt32>::max());
    }
    else if (dstDataType == GDT_UInt64)
    {
        // cppcheck-suppress unreadVariable
        fDstMin = static_cast<Twork>(std::numeric_limits<uint64_t>::min());
        // cppcheck-suppress unreadVariable
        // (1 << 64) - 2048: largest uint64 value a double can hold
        fDstMax = static_cast<Twork>(18446744073709549568ULL);
    }
    else if (dstDataType == GDT_Int64)
    {
        // cppcheck-suppress unreadVariable
        fDstMin = static_cast<Twork>(std::numeric_limits<int64_t>::min());
        // cppcheck-suppress unreadVariable
        // (1 << 63) - 1024: largest int64 that a double can hold
        fDstMax = static_cast<Twork>(9223372036854774784LL);
    }

    auto replaceValIfNodata = [bHasNoData, isIntegerDT, fDstMin, fDstMax,
                               bNoDataValueInt64Valid, nNodataValueInt64,
                               dfNoDataValue, dfReplacementVal](Twork fVal)
    {
        if (!bHasNoData)
            return fVal;

        // Clamp value before comparing to nodata: this is only needed for
        // kernels with negative weights (Lanczos)
        Twork fClamped = fVal;
        if (fClamped < fDstMin)
            fClamped = fDstMin;
        else if (fClamped > fDstMax)
            fClamped = fDstMax;
        if (isIntegerDT)
        {
            if (bNoDataValueInt64Valid)
            {
                const double fClampedRounded = double(std::round(fClamped));
                if (fClampedRounded >=
                        static_cast<double>(static_cast<Twork>(
                            std::numeric_limits<int64_t>::min())) &&
                    fClampedRounded <= static_cast<double>(static_cast<Twork>(
                                           9223372036854774784LL)) &&
                    nNodataValueInt64 ==
                        static_cast<GInt64>(std::round(fClamped)))
                {
                    // Do not use the nodata value
                    return static_cast<Twork>(dfReplacementVal);
                }
            }
        }
        else if (dfNoDataValue == static_cast<double>(fClamped))
        {
            // Do not use the nodata value
            return static_cast<Twork>(dfReplacementVal);
        }
        return fClamped;
    };

    /* -------------------------------------------------------------------- */
    /*      Allocate work buffers.                                          */
    /* -------------------------------------------------------------------- */
    const int nDstXSize = nDstXOff2 - nDstXOff;
    Twork *pafWrkScanline = nullptr;
    if (dstDataType != eWrkDataType)
    {
        pafWrkScanline =
            static_cast<Twork *>(VSI_MALLOC2_VERBOSE(nDstXSize, sizeof(Twork)));
        if (pafWrkScanline == nullptr)
            return CE_Failure;
    }

    const double dfXScale = 1.0 / dfXRatioDstToSrc;
    const double dfXScaleWeight = (dfXScale >= 1.0) ? 1.0 : dfXScale;
    const double dfXScaledRadius = nKernelRadius / dfXScaleWeight;
    const double dfYScale = 1.0 / dfYRatioDstToSrc;
    const double dfYScaleWeight = (dfYScale >= 1.0) ? 1.0 : dfYScale;
    const double dfYScaledRadius = nKernelRadius / dfYScaleWeight;

    // Temporary array to store result of horizontal filter.
    double *const padfHorizontalFiltered = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(nChunkYSize, nDstXSize, sizeof(double) * nBands));

    // To store convolution coefficients.
    double *const padfWeights =
        static_cast<double *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            static_cast<int>(
                2 + 2 * std::max(dfXScaledRadius, dfYScaledRadius) + 0.5) *
            sizeof(double)));

    GByte *pabyChunkNodataMaskHorizontalFiltered = nullptr;
    if (pabyChunkNodataMask)
        pabyChunkNodataMaskHorizontalFiltered =
            static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nChunkYSize, nDstXSize));
    if (padfHorizontalFiltered == nullptr || padfWeights == nullptr ||
        (pabyChunkNodataMask != nullptr &&
         pabyChunkNodataMaskHorizontalFiltered == nullptr))
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
    const bool bSrcPixelCountLess8 = dfXScaledRadius < 4;
#endif
    for (int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel)
    {
        const double dfSrcPixel =
            (iDstPixel + 0.5) * dfXRatioDstToSrc + dfSrcXDelta;
        int nSrcPixelStart =
            static_cast<int>(floor(dfSrcPixel - dfXScaledRadius + 0.5));
        if (nSrcPixelStart < nChunkXOff)
            nSrcPixelStart = nChunkXOff;
        int nSrcPixelStop =
            static_cast<int>(dfSrcPixel + dfXScaledRadius + 0.5);
        if (nSrcPixelStop > nChunkRightXOff)
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
        for (; nSrcPixel < nSrcPixelStop - 3; nSrcPixel += 4)
        {
            padfWeights[nSrcPixel - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel + 1 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel + 2 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel + 3 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcPixel - nSrcPixelStart);
        }
        for (; nSrcPixel < nSrcPixelStop; ++nSrcPixel, dfX += dfXScaleWeight)
        {
            const double dfWeight = pfnFilterFunc(dfX);
            padfWeights[nSrcPixel - nSrcPixelStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        const int nHeight = nChunkYSize * nBands;
        if (pabyChunkNodataMask == nullptr)
        {
            // For floating-point data types, we must scale down a bit values
            // if input values are close to +/- std::numeric_limits<T>::max()
#ifdef OLD_CPPCHECK
            constexpr double mulFactor = 1;
#else
            constexpr double mulFactor =
                (bNeedRescale &&
                 (std::is_same_v<T, float> || std::is_same_v<T, double>))
                    ? 2
                    : 1;
#endif

            if (dfWeightSum != 0)
            {
                const double dfInvWeightSum = 1.0 / (mulFactor * dfWeightSum);
                for (int i = 0; i < nSrcPixelCount; ++i)
                {
                    padfWeights[i] *= dfInvWeightSum;
                }
            }

            const auto ScaleValue = [
#ifdef _MSC_VER
                                        mulFactor
#endif
            ](double dfVal, [[maybe_unused]] const T *inputValues,
                                    [[maybe_unused]] int nInputValues)
            {
                constexpr bool isFloat =
                    std::is_same_v<T, float> || std::is_same_v<T, double>;
                if constexpr (isFloat)
                {
                    if (std::isfinite(dfVal))
                    {
                        return std::clamp(dfVal,
                                          -std::numeric_limits<double>::max() /
                                              mulFactor,
                                          std::numeric_limits<double>::max() /
                                              mulFactor) *
                               mulFactor;
                    }
                    else if constexpr (bKernelWithNegativeWeights)
                    {
                        if (std::isnan(dfVal))
                        {
                            // Either one of the input value is NaN or they are +/-Inf
                            const bool isPositive = inputValues[0] >= 0;
                            for (int i = 0; i < nInputValues; ++i)
                            {
                                if (std::isnan(inputValues[i]))
                                    return dfVal;
                                // cppcheck-suppress knownConditionTrueFalse
                                if ((inputValues[i] >= 0) != isPositive)
                                    return dfVal;
                            }
                            // All values are positive or negative infinity
                            return static_cast<double>(inputValues[0]);
                        }
                    }
                }
                return dfVal;
            };

            int iSrcLineOff = 0;
#ifdef USE_SSE2
            if (nSrcPixelCount == 4)
            {
                for (; iSrcLineOff < nHeight - 2; iSrcLineOff += 3)
                {
                    const size_t j =
                        static_cast<size_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontalPixelCount4_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize, padfWeights, dfVal1,
                        dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal1, pChunk + j, 4);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            1) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal2, pChunk + j + nChunkXSize, 4);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            2) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal3, pChunk + j + 2 * nChunkXSize, 4);
                }
            }
            else if (bSrcPixelCountLess8)
            {
                for (; iSrcLineOff < nHeight - 2; iSrcLineOff += 3)
                {
                    const size_t j =
                        static_cast<size_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize, padfWeights,
                        nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal1, pChunk + j, nSrcPixelCount);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            1) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal2, pChunk + j + nChunkXSize,
                                   nSrcPixelCount);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            2) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal3, pChunk + j + 2 * nChunkXSize,
                                   nSrcPixelCount);
                }
            }
            else
#endif
            {
                for (; iSrcLineOff < nHeight - 2; iSrcLineOff += 3)
                {
                    const size_t j =
                        static_cast<size_t>(iSrcLineOff) * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontal_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize, padfWeights,
                        nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal1, pChunk + j, nSrcPixelCount);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            1) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal2, pChunk + j + nChunkXSize,
                                   nSrcPixelCount);
                    padfHorizontalFiltered[(static_cast<size_t>(iSrcLineOff) +
                                            2) *
                                               nDstXSize +
                                           iDstPixel - nDstXOff] =
                        ScaleValue(dfVal3, pChunk + j + 2 * nChunkXSize,
                                   nSrcPixelCount);
                }
            }
            for (; iSrcLineOff < nHeight; ++iSrcLineOff)
            {
                const size_t j =
                    static_cast<size_t>(iSrcLineOff) * nChunkXSize +
                    (nSrcPixelStart - nChunkXOff);
                const double dfVal = GDALResampleConvolutionHorizontal(
                    pChunk + j, padfWeights, nSrcPixelCount);
                padfHorizontalFiltered[static_cast<size_t>(iSrcLineOff) *
                                           nDstXSize +
                                       iDstPixel - nDstXOff] =
                    ScaleValue(dfVal, pChunk + j, nSrcPixelCount);
            }
        }
        else
        {
            for (int iSrcLineOff = 0; iSrcLineOff < nHeight; ++iSrcLineOff)
            {
                const size_t j =
                    static_cast<size_t>(iSrcLineOff) * nChunkXSize +
                    (nSrcPixelStart - nChunkXOff);

                if (bKernelWithNegativeWeights)
                {
                    int nConsecutiveValid = 0;
                    int nMaxConsecutiveValid = 0;
                    for (int k = 0; k < nSrcPixelCount; k++)
                    {
                        if (pabyChunkNodataMask[j + k])
                            nConsecutiveValid++;
                        else if (nConsecutiveValid)
                        {
                            nMaxConsecutiveValid = std::max(
                                nMaxConsecutiveValid, nConsecutiveValid);
                            nConsecutiveValid = 0;
                        }
                    }
                    nMaxConsecutiveValid =
                        std::max(nMaxConsecutiveValid, nConsecutiveValid);
                    if (nMaxConsecutiveValid < nSrcPixelCount / 2)
                    {
                        const size_t nTempOffset =
                            static_cast<size_t>(iSrcLineOff) * nDstXSize +
                            iDstPixel - nDstXOff;
                        padfHorizontalFiltered[nTempOffset] = 0.0;
                        pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 0;
                        continue;
                    }
                }

                double dfVal = 0.0;
                GDALResampleConvolutionHorizontalWithMask(
                    pChunk + j, pabyChunkNodataMask + j, padfWeights,
                    nSrcPixelCount, dfVal, dfWeightSum);
                const size_t nTempOffset =
                    static_cast<size_t>(iSrcLineOff) * nDstXSize + iDstPixel -
                    nDstXOff;
                if (dfWeightSum > 0.0)
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

    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        Twork *const pafDstScanline =
            pafWrkScanline
                ? pafWrkScanline
                : static_cast<Twork *>(pDstBuffer) +
                      static_cast<size_t>(iDstLine - nDstYOff) * nDstXSize;

        const double dfSrcLine =
            (iDstLine + 0.5) * dfYRatioDstToSrc + dfSrcYDelta;
        int nSrcLineStart =
            static_cast<int>(floor(dfSrcLine - dfYScaledRadius + 0.5));
        int nSrcLineStop = static_cast<int>(dfSrcLine + dfYScaledRadius + 0.5);
        if (nSrcLineStart < nChunkYOff)
            nSrcLineStart = nChunkYOff;
        if (nSrcLineStop > nChunkBottomYOff)
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
        for (; nSrcLine < nSrcLineStop - 3;
             nSrcLine += 4, dfY += 4 * dfYScaleWeight)
        {
            padfWeights[nSrcLine - nSrcLineStart] = dfY;
            padfWeights[nSrcLine + 1 - nSrcLineStart] = dfY + dfYScaleWeight;
            padfWeights[nSrcLine + 2 - nSrcLineStart] =
                dfY + 2 * dfYScaleWeight;
            padfWeights[nSrcLine + 3 - nSrcLineStart] =
                dfY + 3 * dfYScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcLine - nSrcLineStart);
        }
        for (; nSrcLine < nSrcLineStop; ++nSrcLine, dfY += dfYScaleWeight)
        {
            const double dfWeight = pfnFilterFunc(dfY);
            padfWeights[nSrcLine - nSrcLineStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        if (pabyChunkNodataMask == nullptr)
        {
            // For floating-point data types, we must scale down a bit values
            // if input values are close to +/- std::numeric_limits<T>::max()
#ifdef OLD_CPPCHECK
            constexpr double mulFactor = 1;
#else
            constexpr double mulFactor =
                (bNeedRescale &&
                 (std::is_same_v<T, float> || std::is_same_v<T, double>))
                    ? 2
                    : 1;
#endif

            if (dfWeightSum != 0)
            {
                const double dfInvWeightSum = 1.0 / (mulFactor * dfWeightSum);
                for (int i = 0; i < nSrcLineCount; ++i)
                    padfWeights[i] *= dfInvWeightSum;
            }

            int iFilteredPixelOff = 0;  // Used after for.
            // j used after for.
            size_t j =
                (nSrcLineStart - nChunkYOff) * static_cast<size_t>(nDstXSize);
#ifdef USE_SSE2
            if constexpr ((!bNeedRescale || !std::is_same_v<T, float>) &&
                          eWrkDataType == GDT_Float32)
            {
#ifdef __AVX__
                for (; iFilteredPixelOff < nDstXSize - 15;
                     iFilteredPixelOff += 16, j += 16)
                {
                    GDALResampleConvolutionVertical_16cols(
                        padfHorizontalFiltered + j, nDstXSize, padfWeights,
                        nSrcLineCount, pafDstScanline + iFilteredPixelOff);
                    if (bHasNoData)
                    {
                        for (int k = 0; k < 16; k++)
                        {
                            pafDstScanline[iFilteredPixelOff + k] =
                                replaceValIfNodata(
                                    pafDstScanline[iFilteredPixelOff + k]);
                        }
                    }
                }
#else
                for (; iFilteredPixelOff < nDstXSize - 7;
                     iFilteredPixelOff += 8, j += 8)
                {
                    GDALResampleConvolutionVertical_8cols(
                        padfHorizontalFiltered + j, nDstXSize, padfWeights,
                        nSrcLineCount, pafDstScanline + iFilteredPixelOff);
                    if (bHasNoData)
                    {
                        for (int k = 0; k < 8; k++)
                        {
                            pafDstScanline[iFilteredPixelOff + k] =
                                replaceValIfNodata(
                                    pafDstScanline[iFilteredPixelOff + k]);
                        }
                    }
                }
#endif

                for (; iFilteredPixelOff < nDstXSize; iFilteredPixelOff++, j++)
                {
                    const Twork fVal =
                        static_cast<Twork>(GDALResampleConvolutionVertical(
                            padfHorizontalFiltered + j, nDstXSize, padfWeights,
                            nSrcLineCount));
                    pafDstScanline[iFilteredPixelOff] =
                        replaceValIfNodata(fVal);
                }
            }
            else
#endif
            {
                const auto ScaleValue = [
#ifdef _MSC_VER
                                            mulFactor
#endif
                ](double dfVal, [[maybe_unused]] const double *inputValues,
                                        [[maybe_unused]] int nStride,
                                        [[maybe_unused]] int nInputValues)
                {
                    constexpr bool isFloat =
                        std::is_same_v<T, float> || std::is_same_v<T, double>;
                    if constexpr (isFloat)
                    {
                        if (std::isfinite(dfVal))
                        {
                            return std::clamp(
                                       dfVal,
                                       static_cast<double>(
                                           -std::numeric_limits<Twork>::max()) /
                                           mulFactor,
                                       static_cast<double>(
                                           std::numeric_limits<Twork>::max()) /
                                           mulFactor) *
                                   mulFactor;
                        }
                        else if constexpr (bKernelWithNegativeWeights)
                        {
                            if (std::isnan(dfVal))
                            {
                                // Either one of the input value is NaN or they are +/-Inf
                                const bool isPositive = inputValues[0] >= 0;
                                for (int i = 0; i < nInputValues; ++i)
                                {
                                    if (std::isnan(inputValues[i * nStride]))
                                        return dfVal;
                                    // cppcheck-suppress knownConditionTrueFalse
                                    if ((inputValues[i] >= 0) != isPositive)
                                        return dfVal;
                                }
                                // All values are positive or negative infinity
                                return inputValues[0];
                            }
                        }
                    }

                    return dfVal;
                };

                for (; iFilteredPixelOff < nDstXSize - 1;
                     iFilteredPixelOff += 2, j += 2)
                {
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    GDALResampleConvolutionVertical_2cols(
                        padfHorizontalFiltered + j, nDstXSize, padfWeights,
                        nSrcLineCount, dfVal1, dfVal2);
                    pafDstScanline[iFilteredPixelOff] =
                        replaceValIfNodata(static_cast<Twork>(
                            ScaleValue(dfVal1, padfHorizontalFiltered + j,
                                       nDstXSize, nSrcLineCount)));
                    pafDstScanline[iFilteredPixelOff + 1] =
                        replaceValIfNodata(static_cast<Twork>(
                            ScaleValue(dfVal2, padfHorizontalFiltered + j + 1,
                                       nDstXSize, nSrcLineCount)));
                }
                if (iFilteredPixelOff < nDstXSize)
                {
                    const double dfVal = GDALResampleConvolutionVertical(
                        padfHorizontalFiltered + j, nDstXSize, padfWeights,
                        nSrcLineCount);
                    pafDstScanline[iFilteredPixelOff] =
                        replaceValIfNodata(static_cast<Twork>(
                            ScaleValue(dfVal, padfHorizontalFiltered + j,
                                       nDstXSize, nSrcLineCount)));
                }
            }
        }
        else
        {
            for (int iFilteredPixelOff = 0; iFilteredPixelOff < nDstXSize;
                 ++iFilteredPixelOff)
            {
                double dfVal = 0.0;
                dfWeightSum = 0.0;
                size_t j = (nSrcLineStart - nChunkYOff) *
                               static_cast<size_t>(nDstXSize) +
                           iFilteredPixelOff;
                if (bKernelWithNegativeWeights)
                {
                    int nConsecutiveValid = 0;
                    int nMaxConsecutiveValid = 0;
                    for (int i = 0; i < nSrcLineCount; ++i, j += nDstXSize)
                    {
                        const double dfWeight =
                            padfWeights[i] *
                            pabyChunkNodataMaskHorizontalFiltered[j];
                        if (pabyChunkNodataMaskHorizontalFiltered[j])
                        {
                            nConsecutiveValid++;
                        }
                        else if (nConsecutiveValid)
                        {
                            nMaxConsecutiveValid = std::max(
                                nMaxConsecutiveValid, nConsecutiveValid);
                            nConsecutiveValid = 0;
                        }
                        dfVal += padfHorizontalFiltered[j] * dfWeight;
                        dfWeightSum += dfWeight;
                    }
                    nMaxConsecutiveValid =
                        std::max(nMaxConsecutiveValid, nConsecutiveValid);
                    if (nMaxConsecutiveValid < nSrcLineCount / 2)
                    {
                        pafDstScanline[iFilteredPixelOff] =
                            static_cast<Twork>(dfNoDataValue);
                        continue;
                    }
                }
                else
                {
                    for (int i = 0; i < nSrcLineCount; ++i, j += nDstXSize)
                    {
                        const double dfWeight =
                            padfWeights[i] *
                            pabyChunkNodataMaskHorizontalFiltered[j];
                        dfVal += padfHorizontalFiltered[j] * dfWeight;
                        dfWeightSum += dfWeight;
                    }
                }
                if (dfWeightSum > 0.0)
                {
                    pafDstScanline[iFilteredPixelOff] = replaceValIfNodata(
                        static_cast<Twork>(dfVal / dfWeightSum));
                }
                else
                {
                    pafDstScanline[iFilteredPixelOff] =
                        static_cast<Twork>(dfNoDataValue);
                }
            }
        }

        if (fMaxVal != 0.0f)
        {
            if constexpr (std::is_same_v<T, double>)
            {
                for (int i = 0; i < nDstXSize; ++i)
                {
                    if (pafDstScanline[i] > static_cast<double>(fMaxVal))
                        pafDstScanline[i] = static_cast<double>(fMaxVal);
                }
            }
            else
            {
                for (int i = 0; i < nDstXSize; ++i)
                {
                    if (pafDstScanline[i] > fMaxVal)
                        pafDstScanline[i] = fMaxVal;
                }
            }
        }

        if (pafWrkScanline)
        {
            GDALCopyWords64(pafWrkScanline, eWrkDataType, nWrkDataTypeSize,
                            static_cast<GByte *>(pDstBuffer) +
                                static_cast<size_t>(iDstLine - nDstYOff) *
                                    nDstXSize * nDstDataTypeSize,
                            dstDataType, nDstDataTypeSize, nDstXSize);
        }
    }

    VSIFree(pafWrkScanline);
    VSIFreeAligned(padfWeights);
    VSIFree(padfHorizontalFiltered);
    VSIFree(pabyChunkNodataMaskHorizontalFiltered);

    return CE_None;
}

template <bool bKernelWithNegativeWeights, bool bNeedRescale>
static CPLErr
GDALResampleChunk_ConvolutionInternal(const GDALOverviewResampleArgs &args,
                                      const void *pChunk, void **ppDstBuffer,
                                      GDALDataType *peDstBufferDataType)
{
    GDALResampleAlg eResample;
    if (EQUAL(args.pszResampling, "BILINEAR"))
        eResample = GRA_Bilinear;
    else if (EQUAL(args.pszResampling, "CUBIC"))
        eResample = GRA_Cubic;
    else if (EQUAL(args.pszResampling, "CUBICSPLINE"))
        eResample = GRA_CubicSpline;
    else if (EQUAL(args.pszResampling, "LANCZOS"))
        eResample = GRA_Lanczos;
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
    if (eResample != GRA_Bilinear && args.nOvrNBITS > 0 &&
        (args.eOvrDataType == GDT_UInt8 || args.eOvrDataType == GDT_UInt16 ||
         args.eOvrDataType == GDT_UInt32))
    {
        int nBits = args.nOvrNBITS;
        if (nBits == GDALGetDataTypeSizeBits(args.eOvrDataType))
            nBits = 0;
        if (nBits > 0 && nBits < 32)
            fMaxVal = static_cast<float>((1U << nBits) - 1);
    }

    *ppDstBuffer = VSI_MALLOC3_VERBOSE(
        args.nDstXOff2 - args.nDstXOff, args.nDstYOff2 - args.nDstYOff,
        GDALGetDataTypeSizeBytes(args.eOvrDataType));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }
    *peDstBufferDataType = args.eOvrDataType;

    switch (args.eWrkDataType)
    {
        case GDT_UInt8:
        {
            return GDALResampleChunk_ConvolutionT<GByte, float, GDT_Float32,
                                                  bKernelWithNegativeWeights,
                                                  bNeedRescale>(
                args, static_cast<const GByte *>(pChunk), *ppDstBuffer,
                pfnFilterFunc, pfnFilterFunc4Values, nKernelRadius, fMaxVal);
        }

        case GDT_UInt16:
        {
            return GDALResampleChunk_ConvolutionT<GUInt16, float, GDT_Float32,
                                                  bKernelWithNegativeWeights,
                                                  bNeedRescale>(
                args, static_cast<const GUInt16 *>(pChunk), *ppDstBuffer,
                pfnFilterFunc, pfnFilterFunc4Values, nKernelRadius, fMaxVal);
        }

        case GDT_Float32:
        {
            return GDALResampleChunk_ConvolutionT<float, float, GDT_Float32,
                                                  bKernelWithNegativeWeights,
                                                  bNeedRescale>(
                args, static_cast<const float *>(pChunk), *ppDstBuffer,
                pfnFilterFunc, pfnFilterFunc4Values, nKernelRadius, fMaxVal);
        }

        case GDT_Float64:
        {
            return GDALResampleChunk_ConvolutionT<double, double, GDT_Float64,
                                                  bKernelWithNegativeWeights,
                                                  bNeedRescale>(
                args, static_cast<const double *>(pChunk), *ppDstBuffer,
                pfnFilterFunc, pfnFilterFunc4Values, nKernelRadius, fMaxVal);
        }

        default:
            break;
    }

    CPLAssert(false);
    return CE_Failure;
}

static CPLErr
GDALResampleChunk_Convolution(const GDALOverviewResampleArgs &args,
                              const void *pChunk, void **ppDstBuffer,
                              GDALDataType *peDstBufferDataType)
{
    if (EQUAL(args.pszResampling, "CUBIC") ||
        EQUAL(args.pszResampling, "LANCZOS"))
        return GDALResampleChunk_ConvolutionInternal<
            /* bKernelWithNegativeWeights=*/true, /* bNeedRescale = */ true>(
            args, pChunk, ppDstBuffer, peDstBufferDataType);
    else if (EQUAL(args.pszResampling, "CUBICSPLINE"))
        return GDALResampleChunk_ConvolutionInternal<false, true>(
            args, pChunk, ppDstBuffer, peDstBufferDataType);
    else
        return GDALResampleChunk_ConvolutionInternal<false, false>(
            args, pChunk, ppDstBuffer, peDstBufferDataType);
}

/************************************************************************/
/*                       GDALResampleChunkC32R()                        */
/************************************************************************/

static CPLErr GDALResampleChunkC32R(const int nSrcWidth, const int nSrcHeight,
                                    const float *pafChunk, const int nChunkYOff,
                                    const int nChunkYSize, const int nDstYOff,
                                    const int nDstYOff2, const int nOvrXSize,
                                    const int nOvrYSize, void **ppDstBuffer,
                                    GDALDataType *peDstBufferDataType,
                                    const char *pszResampling)

{
    enum Method
    {
        NEAR,
        AVERAGE,
        AVERAGE_MAGPHASE,
        RMS,
    };

    Method eMethod = NEAR;
    if (STARTS_WITH_CI(pszResampling, "NEAR"))
    {
        eMethod = NEAR;
    }
    else if (EQUAL(pszResampling, "AVERAGE_MAGPHASE"))
    {
        eMethod = AVERAGE_MAGPHASE;
    }
    else if (EQUAL(pszResampling, "RMS"))
    {
        eMethod = RMS;
    }
    else if (STARTS_WITH_CI(pszResampling, "AVER"))
    {
        eMethod = AVERAGE;
    }
    else
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Resampling method %s is not supported for complex data types. "
            "Only NEAREST, AVERAGE, AVERAGE_MAGPHASE and RMS are supported",
            pszResampling);
        return CE_Failure;
    }

    const int nOXSize = nOvrXSize;
    *ppDstBuffer = VSI_MALLOC3_VERBOSE(nOXSize, nDstYOff2 - nDstYOff,
                                       GDALGetDataTypeSizeBytes(GDT_CFloat32));
    if (*ppDstBuffer == nullptr)
    {
        return CE_Failure;
    }
    float *const pafDstBuffer = static_cast<float *>(*ppDstBuffer);
    *peDstBufferDataType = GDT_CFloat32;

    const int nOYSize = nOvrYSize;
    const double dfXRatioDstToSrc = static_cast<double>(nSrcWidth) / nOXSize;
    const double dfYRatioDstToSrc = static_cast<double>(nSrcHeight) / nOYSize;

    /* ==================================================================== */
    /*      Loop over destination scanlines.                                */
    /* ==================================================================== */
    for (int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine)
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if (nSrcYOff < nChunkYOff)
            nSrcYOff = nChunkYOff;

        int nSrcYOff2 =
            static_cast<int>(0.5 + (iDstLine + 1) * dfYRatioDstToSrc);
        if (nSrcYOff2 == nSrcYOff)
            nSrcYOff2++;

        if (nSrcYOff2 > nSrcHeight || iDstLine == nOYSize - 1)
        {
            if (nSrcYOff == nSrcHeight && nSrcHeight - 1 >= nChunkYOff)
                nSrcYOff = nSrcHeight - 1;
            nSrcYOff2 = nSrcHeight;
        }
        if (nSrcYOff2 > nChunkYOff + nChunkYSize)
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        const float *const pafSrcScanline =
            pafChunk +
            (static_cast<size_t>(nSrcYOff - nChunkYOff) * nSrcWidth) * 2;
        float *const pafDstScanline =
            pafDstBuffer +
            static_cast<size_t>(iDstLine - nDstYOff) * 2 * nOXSize;

        /* --------------------------------------------------------------------
         */
        /*      Loop over destination pixels */
        /* --------------------------------------------------------------------
         */
        for (int iDstPixel = 0; iDstPixel < nOXSize; ++iDstPixel)
        {
            const size_t iDstPixelSZ = static_cast<size_t>(iDstPixel);
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 =
                static_cast<int>(0.5 + (iDstPixel + 1) * dfXRatioDstToSrc);
            if (nSrcXOff2 == nSrcXOff)
                nSrcXOff2++;
            if (nSrcXOff2 > nSrcWidth || iDstPixel == nOXSize - 1)
            {
                if (nSrcXOff == nSrcWidth && nSrcWidth - 1 >= 0)
                    nSrcXOff = nSrcWidth - 1;
                nSrcXOff2 = nSrcWidth;
            }
            const size_t nSrcXOffSZ = static_cast<size_t>(nSrcXOff);

            if (eMethod == NEAR)
            {
                pafDstScanline[iDstPixelSZ * 2] =
                    pafSrcScanline[nSrcXOffSZ * 2];
                pafDstScanline[iDstPixelSZ * 2 + 1] =
                    pafSrcScanline[nSrcXOffSZ * 2 + 1];
            }
            else if (eMethod == AVERAGE_MAGPHASE)
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                double dfTotalM = 0.0;
                size_t nCount = 0;

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        const double dfR = double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2]);
                        const double dfI = double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2 +
                                           1]);
                        dfTotalR += dfR;
                        dfTotalI += dfI;
                        dfTotalM += std::hypot(dfR, dfI);
                        ++nCount;
                    }
                }

                CPLAssert(nCount > 0);
                if (nCount == 0)
                {
                    pafDstScanline[iDstPixelSZ * 2] = 0.0;
                    pafDstScanline[iDstPixelSZ * 2 + 1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixelSZ * 2] = static_cast<float>(
                        dfTotalR / static_cast<double>(nCount));
                    pafDstScanline[iDstPixelSZ * 2 + 1] = static_cast<float>(
                        dfTotalI / static_cast<double>(nCount));
                    const double dfM =
                        double(std::hypot(pafDstScanline[iDstPixelSZ * 2],
                                          pafDstScanline[iDstPixelSZ * 2 + 1]));
                    const double dfDesiredM =
                        dfTotalM / static_cast<double>(nCount);
                    double dfRatio = 1.0;
                    if (dfM != 0.0)
                        dfRatio = dfDesiredM / dfM;

                    pafDstScanline[iDstPixelSZ * 2] *=
                        static_cast<float>(dfRatio);
                    pafDstScanline[iDstPixelSZ * 2 + 1] *=
                        static_cast<float>(dfRatio);
                }
            }
            else if (eMethod == RMS)
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                size_t nCount = 0;

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        const double dfR = double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2]);
                        const double dfI = double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2 +
                                           1]);

                        dfTotalR += SQUARE(dfR);
                        dfTotalI += SQUARE(dfI);

                        ++nCount;
                    }
                }

                CPLAssert(nCount > 0);
                if (nCount == 0)
                {
                    pafDstScanline[iDstPixelSZ * 2] = 0.0;
                    pafDstScanline[iDstPixelSZ * 2 + 1] = 0.0;
                }
                else
                {
                    /* compute RMS */
                    pafDstScanline[iDstPixelSZ * 2] = static_cast<float>(
                        sqrt(dfTotalR / static_cast<double>(nCount)));
                    pafDstScanline[iDstPixelSZ * 2 + 1] = static_cast<float>(
                        sqrt(dfTotalI / static_cast<double>(nCount)));
                }
            }
            else if (eMethod == AVERAGE)
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                size_t nCount = 0;

                for (int iY = nSrcYOff; iY < nSrcYOff2; ++iY)
                {
                    for (int iX = nSrcXOff; iX < nSrcXOff2; ++iX)
                    {
                        // TODO(schwehr): Maybe use std::complex?
                        dfTotalR += double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2]);
                        dfTotalI += double(
                            pafSrcScanline[static_cast<size_t>(iX) * 2 +
                                           static_cast<size_t>(iY - nSrcYOff) *
                                               nSrcWidth * 2 +
                                           1]);
                        ++nCount;
                    }
                }

                CPLAssert(nCount > 0);
                if (nCount == 0)
                {
                    pafDstScanline[iDstPixelSZ * 2] = 0.0;
                    pafDstScanline[iDstPixelSZ * 2 + 1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixelSZ * 2] = static_cast<float>(
                        dfTotalR / static_cast<double>(nCount));
                    pafDstScanline[iDstPixelSZ * 2 + 1] = static_cast<float>(
                        dfTotalI / static_cast<double>(nCount));
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

static CPLErr GDALRegenerateCascadingOverviews(
    GDALRasterBand *poSrcBand, int nOverviews, GDALRasterBand **papoOvrBands,
    const char *pszResampling, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)

{
    /* -------------------------------------------------------------------- */
    /*      First, we must put the overviews in order from largest to       */
    /*      smallest.                                                       */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < nOverviews - 1; ++i)
    {
        for (int j = 0; j < nOverviews - i - 1; ++j)
        {
            if (papoOvrBands[j]->GetXSize() *
                    static_cast<float>(papoOvrBands[j]->GetYSize()) <
                papoOvrBands[j + 1]->GetXSize() *
                    static_cast<float>(papoOvrBands[j + 1]->GetYSize()))
            {
                GDALRasterBand *poTempBand = papoOvrBands[j];
                papoOvrBands[j] = papoOvrBands[j + 1];
                papoOvrBands[j + 1] = poTempBand;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Count total pixels so we can prepare appropriate scaled         */
    /*      progress functions.                                             */
    /* -------------------------------------------------------------------- */
    double dfTotalPixels = 0.0;

    for (int i = 0; i < nOverviews; ++i)
    {
        dfTotalPixels += papoOvrBands[i]->GetXSize() *
                         static_cast<double>(papoOvrBands[i]->GetYSize());
    }

    /* -------------------------------------------------------------------- */
    /*      Generate all the bands.                                         */
    /* -------------------------------------------------------------------- */
    double dfPixelsProcessed = 0.0;

    CPLStringList aosOptions(papszOptions);
    aosOptions.SetNameValue("CASCADING", "YES");
    for (int i = 0; i < nOverviews; ++i)
    {
        GDALRasterBand *poBaseBand = poSrcBand;
        if (i != 0)
            poBaseBand = papoOvrBands[i - 1];

        double dfPixels = papoOvrBands[i]->GetXSize() *
                          static_cast<double>(papoOvrBands[i]->GetYSize());

        void *pScaledProgressData = GDALCreateScaledProgress(
            dfPixelsProcessed / dfTotalPixels,
            (dfPixelsProcessed + dfPixels) / dfTotalPixels, pfnProgress,
            pProgressData);

        const CPLErr eErr = GDALRegenerateOverviewsEx(
            poBaseBand, 1,
            reinterpret_cast<GDALRasterBandH *>(papoOvrBands) + i,
            pszResampling, GDALScaledProgress, pScaledProgressData,
            aosOptions.List());
        GDALDestroyScaledProgress(pScaledProgressData);

        if (eErr != CE_None)
            return eErr;

        dfPixelsProcessed += dfPixels;

        // Only do the bit2grayscale promotion on the base band.
        if (STARTS_WITH_CI(pszResampling,
                           "AVERAGE_BIT2G" /* AVERAGE_BIT2GRAYSCALE */))
            pszResampling = "AVERAGE";
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALGetResampleFunction()                       */
/************************************************************************/

GDALResampleFunction GDALGetResampleFunction(const char *pszResampling,
                                             int *pnRadius)
{
    if (pnRadius)
        *pnRadius = 0;
    if (STARTS_WITH_CI(pszResampling, "NEAR"))
        return GDALResampleChunk_Near;
    else if (STARTS_WITH_CI(pszResampling, "AVER") ||
             EQUAL(pszResampling, "RMS"))
        return GDALResampleChunk_AverageOrRMS;
    else if (EQUAL(pszResampling, "GAUSS"))
    {
        if (pnRadius)
            *pnRadius = 1;
        return GDALResampleChunk_Gauss;
    }
    else if (EQUAL(pszResampling, "MODE"))
        return GDALResampleChunk_Mode;
    else if (EQUAL(pszResampling, "CUBIC"))
    {
        if (pnRadius)
            *pnRadius = GWKGetFilterRadius(GRA_Cubic);
        return GDALResampleChunk_Convolution;
    }
    else if (EQUAL(pszResampling, "CUBICSPLINE"))
    {
        if (pnRadius)
            *pnRadius = GWKGetFilterRadius(GRA_CubicSpline);
        return GDALResampleChunk_Convolution;
    }
    else if (EQUAL(pszResampling, "LANCZOS"))
    {
        if (pnRadius)
            *pnRadius = GWKGetFilterRadius(GRA_Lanczos);
        return GDALResampleChunk_Convolution;
    }
    else if (EQUAL(pszResampling, "BILINEAR"))
    {
        if (pnRadius)
            *pnRadius = GWKGetFilterRadius(GRA_Bilinear);
        return GDALResampleChunk_Convolution;
    }
    else
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "GDALGetResampleFunction: Unsupported resampling method \"%s\".",
            pszResampling);
        return nullptr;
    }
}

/************************************************************************/
/*                       GDALGetOvrWorkDataType()                       */
/************************************************************************/

GDALDataType GDALGetOvrWorkDataType(const char *pszResampling,
                                    GDALDataType eSrcDataType)
{
    if (STARTS_WITH_CI(pszResampling, "NEAR") || EQUAL(pszResampling, "MODE"))
    {
        return eSrcDataType;
    }
    else if (eSrcDataType == GDT_UInt8 &&
             (STARTS_WITH_CI(pszResampling, "AVER") ||
              EQUAL(pszResampling, "RMS") || EQUAL(pszResampling, "CUBIC") ||
              EQUAL(pszResampling, "CUBICSPLINE") ||
              EQUAL(pszResampling, "LANCZOS") ||
              EQUAL(pszResampling, "BILINEAR") || EQUAL(pszResampling, "MODE")))
    {
        return GDT_UInt8;
    }
    else if (eSrcDataType == GDT_UInt16 &&
             (STARTS_WITH_CI(pszResampling, "AVER") ||
              EQUAL(pszResampling, "RMS") || EQUAL(pszResampling, "CUBIC") ||
              EQUAL(pszResampling, "CUBICSPLINE") ||
              EQUAL(pszResampling, "LANCZOS") ||
              EQUAL(pszResampling, "BILINEAR") || EQUAL(pszResampling, "MODE")))
    {
        return GDT_UInt16;
    }
    else if (EQUAL(pszResampling, "GAUSS"))
        return GDT_Float64;

    if (eSrcDataType == GDT_UInt8 || eSrcDataType == GDT_Int8 ||
        eSrcDataType == GDT_UInt16 || eSrcDataType == GDT_Int16 ||
        eSrcDataType == GDT_Float32)
    {
        return GDT_Float32;
    }
    return GDT_Float64;
}

namespace
{
// Structure to hold a pointer to free with CPLFree()
struct PointerHolder
{
    void *ptr = nullptr;

    explicit PointerHolder(void *ptrIn) : ptr(ptrIn)
    {
    }

    ~PointerHolder()
    {
        CPLFree(ptr);
    }

    PointerHolder(const PointerHolder &) = delete;
    PointerHolder &operator=(const PointerHolder &) = delete;
};
}  // namespace

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
CPLErr GDALRegenerateOverviews(GDALRasterBandH hSrcBand, int nOverviewCount,
                               GDALRasterBandH *pahOvrBands,
                               const char *pszResampling,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData)

{
    return GDALRegenerateOverviewsEx(hSrcBand, nOverviewCount, pahOvrBands,
                                     pszResampling, pfnProgress, pProgressData,
                                     nullptr);
}

/************************************************************************/
/*                     GDALRegenerateOverviewsEx()                      */
/************************************************************************/

constexpr int RADIUS_TO_DIAMETER = 2;

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
 * @param papszOptions NULL terminated list of options as key=value pairs, or
 * NULL
 * @return CE_None on success or CE_Failure on failure.
 * @since GDAL 3.6
 */
CPLErr GDALRegenerateOverviewsEx(GDALRasterBandH hSrcBand, int nOverviewCount,
                                 GDALRasterBandH *pahOvrBands,
                                 const char *pszResampling,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData, CSLConstList papszOptions)

{
    GDALRasterBand *poSrcBand = GDALRasterBand::FromHandle(hSrcBand);
    GDALRasterBand **papoOvrBands =
        reinterpret_cast<GDALRasterBand **>(pahOvrBands);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    if (EQUAL(pszResampling, "NONE"))
        return CE_None;

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn =
        GDALGetResampleFunction(pszResampling, &nKernelRadius);

    if (pfnResampleFn == nullptr)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Check color tables...                                           */
    /* -------------------------------------------------------------------- */
    GDALColorTable *poColorTable = nullptr;

    if ((STARTS_WITH_CI(pszResampling, "AVER") || EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "MODE") || EQUAL(pszResampling, "GAUSS")) &&
        poSrcBand->GetColorInterpretation() == GCI_PaletteIndex)
    {
        poColorTable = poSrcBand->GetColorTable();
        if (poColorTable != nullptr)
        {
            if (poColorTable->GetPaletteInterpretation() != GPI_RGB)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Computing overviews on palette index raster bands "
                         "with a palette whose color interpretation is not RGB "
                         "will probably lead to unexpected results.");
                poColorTable = nullptr;
            }
            else if (poColorTable->IsIdentity())
            {
                poColorTable = nullptr;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Computing overviews on palette index raster bands "
                     "without a palette will probably lead to unexpected "
                     "results.");
        }
    }
    // Not ready yet
    else if ((EQUAL(pszResampling, "CUBIC") ||
              EQUAL(pszResampling, "CUBICSPLINE") ||
              EQUAL(pszResampling, "LANCZOS") ||
              EQUAL(pszResampling, "BILINEAR")) &&
             poSrcBand->GetColorInterpretation() == GCI_PaletteIndex)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Computing %s overviews on palette index raster bands "
                 "will probably lead to unexpected results.",
                 pszResampling);
    }

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.

    GDALRasterBand *poMaskBand = nullptr;
    bool bUseNoDataMask = false;
    bool bCanUseCascaded = true;

    if (!STARTS_WITH_CI(pszResampling, "NEAR"))
    {
        // Special case if we are an alpha/mask band. We want it to be
        // considered as the mask band to avoid alpha=0 to be taken into account
        // in average computation.
        if (poSrcBand->IsMaskBand())
        {
            poMaskBand = poSrcBand;
            bUseNoDataMask = true;
        }
        else
        {
            poMaskBand = poSrcBand->GetMaskBand();
            const int nMaskFlags = poSrcBand->GetMaskFlags();
            bCanUseCascaded =
                (nMaskFlags == GMF_NODATA || nMaskFlags == GMF_ALL_VALID);
            bUseNoDataMask = (nMaskFlags & GMF_ALL_VALID) == 0;
        }
    }

    int nHasNoData = 0;
    const double dfNoDataValue = poSrcBand->GetNoDataValue(&nHasNoData);
    const bool bHasNoData = CPL_TO_BOOL(nHasNoData);
    const bool bPropagateNoData =
        CPLTestBool(CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO"));

    if (poSrcBand->GetBand() == 1 && bUseNoDataMask &&
        CSLFetchNameValue(papszOptions, "CASCADING") == nullptr)
    {
        std::string osDetailMessage;
        if (poSrcBand->HasConflictingMaskSources(&osDetailMessage, false))
        {
            CPLError(
                CE_Warning, CPLE_AppDefined, "%s%s", osDetailMessage.c_str(),
                bHasNoData
                    ? "Only the nodata value will be taken into account."
                    : "Only the first listed one will be taken into account.");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If we are operating on multiple overviews, and using            */
    /*      averaging, lets do them in cascading order to reduce the        */
    /*      amount of computation.                                          */
    /* -------------------------------------------------------------------- */

    // In case the mask made be computed from another band of the dataset,
    // we can't use cascaded generation, as the computation of the overviews
    // of the band used for the mask band may not have yet occurred (#3033).
    if ((STARTS_WITH_CI(pszResampling, "AVER") ||
         EQUAL(pszResampling, "GAUSS") || EQUAL(pszResampling, "RMS") ||
         EQUAL(pszResampling, "CUBIC") || EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") || EQUAL(pszResampling, "BILINEAR") ||
         EQUAL(pszResampling, "MODE")) &&
        nOverviewCount > 1 && bCanUseCascaded)
        return GDALRegenerateCascadingOverviews(
            poSrcBand, nOverviewCount, papoOvrBands, pszResampling, pfnProgress,
            pProgressData, papszOptions);

    /* -------------------------------------------------------------------- */
    /*      Setup one horizontal swath to read from the raw buffer.         */
    /* -------------------------------------------------------------------- */
    int nFRXBlockSize = 0;
    int nFRYBlockSize = 0;
    poSrcBand->GetBlockSize(&nFRXBlockSize, &nFRYBlockSize);

    const GDALDataType eSrcDataType = poSrcBand->GetRasterDataType();
    const bool bUseGenericResampleFn = STARTS_WITH_CI(pszResampling, "NEAR") ||
                                       EQUAL(pszResampling, "MODE") ||
                                       !GDALDataTypeIsComplex(eSrcDataType);
    const GDALDataType eWrkDataType =
        bUseGenericResampleFn
            ? GDALGetOvrWorkDataType(pszResampling, eSrcDataType)
            : GDT_CFloat32;

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    int nMaxOvrFactor = 1;
    for (int iOverview = 0; iOverview < nOverviewCount; ++iOverview)
    {
        const int nDstWidth = papoOvrBands[iOverview]->GetXSize();
        const int nDstHeight = papoOvrBands[iOverview]->GetYSize();
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nWidth) / nDstWidth + 0.5));
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nHeight) / nDstHeight + 0.5));
    }

    int nFullResYChunk = nFRYBlockSize;
    int nMaxChunkYSizeQueried = 0;

    const auto UpdateChunkHeightAndGetChunkSize =
        [&nFullResYChunk, &nMaxChunkYSizeQueried, nKernelRadius, nMaxOvrFactor,
         eWrkDataType, nWidth]()
    {
        // Make sure that round(nChunkYOff / nMaxOvrFactor) < round((nChunkYOff
        // + nFullResYChunk) / nMaxOvrFactor)
        if (nMaxOvrFactor > INT_MAX / RADIUS_TO_DIAMETER)
        {
            return GINTBIG_MAX;
        }
        nFullResYChunk =
            std::max(nFullResYChunk, RADIUS_TO_DIAMETER * nMaxOvrFactor);
        if ((nKernelRadius > 0 &&
             nMaxOvrFactor > INT_MAX / (RADIUS_TO_DIAMETER * nKernelRadius)) ||
            nFullResYChunk >
                INT_MAX - RADIUS_TO_DIAMETER * nKernelRadius * nMaxOvrFactor)
        {
            return GINTBIG_MAX;
        }
        nMaxChunkYSizeQueried =
            nFullResYChunk + RADIUS_TO_DIAMETER * nKernelRadius * nMaxOvrFactor;
        if (GDALGetDataTypeSizeBytes(eWrkDataType) >
            std::numeric_limits<int64_t>::max() /
                (static_cast<int64_t>(nMaxChunkYSizeQueried) * nWidth))
        {
            return GINTBIG_MAX;
        }
        return static_cast<GIntBig>(GDALGetDataTypeSizeBytes(eWrkDataType)) *
               nMaxChunkYSizeQueried * nWidth;
    };

    const char *pszChunkYSize =
        CPLGetConfigOption("GDAL_OVR_CHUNKYSIZE", nullptr);
#ifndef __COVERITY__
    // Only configurable for debug / testing
    if (pszChunkYSize)
    {
        nFullResYChunk = atoi(pszChunkYSize);
    }
#endif

    // Only configurable for debug / testing
    const int nChunkMaxSize =
        atoi(CPLGetConfigOption("GDAL_OVR_CHUNK_MAX_SIZE", "10485760"));

    auto nChunkSize = UpdateChunkHeightAndGetChunkSize();
    if (nChunkSize > nChunkMaxSize)
    {
        if (poColorTable == nullptr && nFRXBlockSize < nWidth &&
            !GDALDataTypeIsComplex(eSrcDataType) &&
            (!STARTS_WITH_CI(pszResampling, "AVER") ||
             EQUAL(pszResampling, "AVERAGE")))
        {
            // If this is tiled, then use GDALRegenerateOverviewsMultiBand()
            // which use a block based strategy, which is much less memory
            // hungry.
            return GDALRegenerateOverviewsMultiBand(
                1, &poSrcBand, nOverviewCount, &papoOvrBands, pszResampling,
                pfnProgress, pProgressData, papszOptions);
        }
        else if (nOverviewCount > 1 && STARTS_WITH_CI(pszResampling, "NEAR"))
        {
            return GDALRegenerateCascadingOverviews(
                poSrcBand, nOverviewCount, papoOvrBands, pszResampling,
                pfnProgress, pProgressData, papszOptions);
        }
    }
    else if (pszChunkYSize == nullptr)
    {
        // Try to get as close as possible to nChunkMaxSize
        while (nChunkSize < nChunkMaxSize / 2)
        {
            nFullResYChunk *= 2;
            nChunkSize = UpdateChunkHeightAndGetChunkSize();
        }
    }

    // Structure describing a resampling job
    struct OvrJob
    {
        // Buffers to free when job is finished
        std::shared_ptr<PointerHolder> oSrcMaskBufferHolder{};
        std::shared_ptr<PointerHolder> oSrcBufferHolder{};
        std::unique_ptr<PointerHolder> oDstBufferHolder{};

        GDALRasterBand *poDstBand = nullptr;

        // Input parameters of pfnResampleFn
        GDALResampleFunction pfnResampleFn = nullptr;
        int nSrcWidth = 0;
        int nSrcHeight = 0;
        int nDstWidth = 0;
        GDALOverviewResampleArgs args{};
        const void *pChunk = nullptr;
        bool bUseGenericResampleFn = false;

        // Output values of resampling function
        CPLErr eErr = CE_Failure;
        void *pDstBuffer = nullptr;
        GDALDataType eDstBufferDataType = GDT_Unknown;

        void SetSrcMaskBufferHolder(
            const std::shared_ptr<PointerHolder> &oSrcMaskBufferHolderIn)
        {
            oSrcMaskBufferHolder = oSrcMaskBufferHolderIn;
        }

        void SetSrcBufferHolder(
            const std::shared_ptr<PointerHolder> &oSrcBufferHolderIn)
        {
            oSrcBufferHolder = oSrcBufferHolderIn;
        }

        void NotifyFinished()
        {
            std::lock_guard guard(mutex);
            bFinished = true;
            cv.notify_one();
        }

        bool IsFinished()
        {
            std::lock_guard guard(mutex);
            return bFinished;
        }

        void WaitFinished()
        {
            std::unique_lock oGuard(mutex);
            while (!bFinished)
            {
                cv.wait(oGuard);
            }
        }

      private:
        // Synchronization
        bool bFinished = false;
        std::mutex mutex{};
        std::condition_variable cv{};
    };

    // Thread function to resample
    const auto JobResampleFunc = [](void *pData)
    {
        OvrJob *poJob = static_cast<OvrJob *>(pData);

        if (poJob->bUseGenericResampleFn)
        {
            poJob->eErr = poJob->pfnResampleFn(poJob->args, poJob->pChunk,
                                               &(poJob->pDstBuffer),
                                               &(poJob->eDstBufferDataType));
        }
        else
        {
            poJob->eErr = GDALResampleChunkC32R(
                poJob->nSrcWidth, poJob->nSrcHeight,
                static_cast<const float *>(poJob->pChunk),
                poJob->args.nChunkYOff, poJob->args.nChunkYSize,
                poJob->args.nDstYOff, poJob->args.nDstYOff2,
                poJob->args.nOvrXSize, poJob->args.nOvrYSize,
                &(poJob->pDstBuffer), &(poJob->eDstBufferDataType),
                poJob->args.pszResampling);
        }

        poJob->oDstBufferHolder =
            std::make_unique<PointerHolder>(poJob->pDstBuffer);

        poJob->NotifyFinished();
    };

    // Function to write resample data to target band
    const auto WriteJobData = [](const OvrJob *poJob)
    {
        return poJob->poDstBand->RasterIO(
            GF_Write, 0, poJob->args.nDstYOff, poJob->nDstWidth,
            poJob->args.nDstYOff2 - poJob->args.nDstYOff, poJob->pDstBuffer,
            poJob->nDstWidth, poJob->args.nDstYOff2 - poJob->args.nDstYOff,
            poJob->eDstBufferDataType, 0, 0, nullptr);
    };

    // Wait for completion of oldest job and serialize it
    const auto WaitAndFinalizeOldestJob =
        [WriteJobData](std::list<std::unique_ptr<OvrJob>> &jobList)
    {
        auto poOldestJob = jobList.front().get();
        poOldestJob->WaitFinished();
        CPLErr l_eErr = poOldestJob->eErr;
        if (l_eErr == CE_None)
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

    const int nThreads = GDALGetNumThreads(GDAL_DEFAULT_MAX_THREAD_COUNT,
                                           /* bDefaultToAllCPUs=*/false);
    auto poThreadPool =
        nThreads > 1 ? GDALGetGlobalThreadPool(nThreads) : nullptr;
    auto poJobQueue = poThreadPool ? poThreadPool->CreateJobQueue()
                                   : std::unique_ptr<CPLJobQueue>(nullptr);

    /* -------------------------------------------------------------------- */
    /*      Loop over image operating on chunks.                            */
    /* -------------------------------------------------------------------- */
    int nChunkYOff = 0;
    CPLErr eErr = CE_None;

    for (nChunkYOff = 0; nChunkYOff < nHeight && eErr == CE_None;
         nChunkYOff += nFullResYChunk)
    {
        if (!pfnProgress(nChunkYOff / static_cast<double>(nHeight), nullptr,
                         pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            eErr = CE_Failure;
        }

        if (nFullResYChunk + nChunkYOff > nHeight)
            nFullResYChunk = nHeight - nChunkYOff;

        int nChunkYOffQueried = nChunkYOff - nKernelRadius * nMaxOvrFactor;
        int nChunkYSizeQueried =
            nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;
        if (nChunkYOffQueried < 0)
        {
            nChunkYSizeQueried += nChunkYOffQueried;
            nChunkYOffQueried = 0;
        }
        if (nChunkYOffQueried + nChunkYSizeQueried > nHeight)
            nChunkYSizeQueried = nHeight - nChunkYOffQueried;

        // Avoid accumulating too many tasks and exhaust RAM
        // Try to complete already finished jobs
        while (eErr == CE_None && !jobList.empty())
        {
            auto poOldestJob = jobList.front().get();
            if (!poOldestJob->IsFinished())
                break;
            eErr = poOldestJob->eErr;
            if (eErr == CE_None)
            {
                eErr = WriteJobData(poOldestJob);
            }

            jobList.pop_front();
        }

        // And in case we have saturated the number of threads,
        // wait for completion of tasks to go below the threshold.
        while (eErr == CE_None &&
               jobList.size() >= static_cast<size_t>(nThreads))
        {
            eErr = WaitAndFinalizeOldestJob(jobList);
        }

        // (Re)allocate buffers if needed
        if (pChunk == nullptr)
        {
            pChunk = VSI_MALLOC3_VERBOSE(GDALGetDataTypeSizeBytes(eWrkDataType),
                                         nMaxChunkYSizeQueried, nWidth);
        }
        if (bUseNoDataMask && pabyChunkNodataMask == nullptr)
        {
            pabyChunkNodataMask = static_cast<GByte *>(
                VSI_MALLOC2_VERBOSE(nMaxChunkYSizeQueried, nWidth));
        }

        if (pChunk == nullptr ||
            (bUseNoDataMask && pabyChunkNodataMask == nullptr))
        {
            CPLFree(pChunk);
            CPLFree(pabyChunkNodataMask);
            return CE_Failure;
        }

        // Read chunk.
        if (eErr == CE_None)
            eErr = poSrcBand->RasterIO(GF_Read, 0, nChunkYOffQueried, nWidth,
                                       nChunkYSizeQueried, pChunk, nWidth,
                                       nChunkYSizeQueried, eWrkDataType, 0, 0,
                                       nullptr);
        if (eErr == CE_None && bUseNoDataMask)
            eErr = poMaskBand->RasterIO(GF_Read, 0, nChunkYOffQueried, nWidth,
                                        nChunkYSizeQueried, pabyChunkNodataMask,
                                        nWidth, nChunkYSizeQueried, GDT_UInt8,
                                        0, 0, nullptr);

        // Special case to promote 1bit data to 8bit 0/255 values.
        if (EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE"))
        {
            if (eWrkDataType == GDT_Float32)
            {
                float *pafChunk = static_cast<float *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pafChunk[i] == 1.0f)
                        pafChunk[i] = 255.0f;
                }
            }
            else if (eWrkDataType == GDT_UInt8)
            {
                GByte *pabyChunk = static_cast<GByte *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pabyChunk[i] == 1)
                        pabyChunk[i] = 255;
                }
            }
            else if (eWrkDataType == GDT_UInt16)
            {
                GUInt16 *pasChunk = static_cast<GUInt16 *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pasChunk[i] == 1)
                        pasChunk[i] = 255;
                }
            }
            else if (eWrkDataType == GDT_Float64)
            {
                double *padfChunk = static_cast<double *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (padfChunk[i] == 1.0)
                        padfChunk[i] = 255.0;
                }
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE_MINISWHITE"))
        {
            if (eWrkDataType == GDT_Float32)
            {
                float *pafChunk = static_cast<float *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pafChunk[i] == 1.0f)
                        pafChunk[i] = 0.0f;
                    else if (pafChunk[i] == 0.0f)
                        pafChunk[i] = 255.0f;
                }
            }
            else if (eWrkDataType == GDT_UInt8)
            {
                GByte *pabyChunk = static_cast<GByte *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pabyChunk[i] == 1)
                        pabyChunk[i] = 0;
                    else if (pabyChunk[i] == 0)
                        pabyChunk[i] = 255;
                }
            }
            else if (eWrkDataType == GDT_UInt16)
            {
                GUInt16 *pasChunk = static_cast<GUInt16 *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (pasChunk[i] == 1)
                        pasChunk[i] = 0;
                    else if (pasChunk[i] == 0)
                        pasChunk[i] = 255;
                }
            }
            else if (eWrkDataType == GDT_Float64)
            {
                double *padfChunk = static_cast<double *>(pChunk);
                for (size_t i = 0;
                     i < static_cast<size_t>(nChunkYSizeQueried) * nWidth; i++)
                {
                    if (padfChunk[i] == 1.0)
                        padfChunk[i] = 0.0;
                    else if (padfChunk[i] == 0.0)
                        padfChunk[i] = 255.0;
                }
            }
            else
            {
                CPLAssert(false);
            }
        }

        auto oSrcBufferHolder =
            std::make_shared<PointerHolder>(poJobQueue ? pChunk : nullptr);
        auto oSrcMaskBufferHolder = std::make_shared<PointerHolder>(
            poJobQueue ? pabyChunkNodataMask : nullptr);

        for (int iOverview = 0; iOverview < nOverviewCount && eErr == CE_None;
             ++iOverview)
        {
            GDALRasterBand *poDstBand = papoOvrBands[iOverview];
            const int nDstWidth = poDstBand->GetXSize();
            const int nDstHeight = poDstBand->GetYSize();

            const double dfXRatioDstToSrc =
                static_cast<double>(nWidth) / nDstWidth;
            const double dfYRatioDstToSrc =
                static_cast<double>(nHeight) / nDstHeight;

            /* --------------------------------------------------------------------
             */
            /*      Figure out the line to start writing to, and the first line
             */
            /*      to not write to.  In theory this approach should ensure that
             */
            /*      every output line will be written if all input chunks are */
            /*      processed. */
            /* --------------------------------------------------------------------
             */
            int nDstYOff =
                static_cast<int>(0.5 + nChunkYOff / dfYRatioDstToSrc);
            if (nDstYOff == nDstHeight)
                continue;
            int nDstYOff2 = static_cast<int>(
                0.5 + (nChunkYOff + nFullResYChunk) / dfYRatioDstToSrc);

            if (nChunkYOff + nFullResYChunk == nHeight)
                nDstYOff2 = nDstHeight;
#if DEBUG_VERBOSE
            CPLDebug("GDAL",
                     "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)", 0,
                     nChunkYOffQueried, nWidth, nChunkYSizeQueried, 0, nDstYOff,
                     nDstWidth, nDstYOff2 - nDstYOff);
#endif

            auto poJob = std::make_unique<OvrJob>();
            poJob->pfnResampleFn = pfnResampleFn;
            poJob->bUseGenericResampleFn = bUseGenericResampleFn;
            poJob->args.eOvrDataType = poDstBand->GetRasterDataType();
            poJob->args.nOvrXSize = poDstBand->GetXSize();
            poJob->args.nOvrYSize = poDstBand->GetYSize();
            const char *pszNBITS =
                poDstBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
            poJob->args.nOvrNBITS = pszNBITS ? atoi(pszNBITS) : 0;
            poJob->args.dfXRatioDstToSrc = dfXRatioDstToSrc;
            poJob->args.dfYRatioDstToSrc = dfYRatioDstToSrc;
            poJob->args.eWrkDataType = eWrkDataType;
            poJob->pChunk = pChunk;
            poJob->args.pabyChunkNodataMask = pabyChunkNodataMask;
            poJob->nSrcWidth = nWidth;
            poJob->nSrcHeight = nHeight;
            poJob->args.nChunkXOff = 0;
            poJob->args.nChunkXSize = nWidth;
            poJob->args.nChunkYOff = nChunkYOffQueried;
            poJob->args.nChunkYSize = nChunkYSizeQueried;
            poJob->nDstWidth = nDstWidth;
            poJob->args.nDstXOff = 0;
            poJob->args.nDstXOff2 = nDstWidth;
            poJob->args.nDstYOff = nDstYOff;
            poJob->args.nDstYOff2 = nDstYOff2;
            poJob->poDstBand = poDstBand;
            poJob->args.pszResampling = pszResampling;
            poJob->args.bHasNoData = bHasNoData;
            poJob->args.dfNoDataValue = dfNoDataValue;
            poJob->args.poColorTable = poColorTable;
            poJob->args.eSrcDataType = eSrcDataType;
            poJob->args.bPropagateNoData = bPropagateNoData;

            if (poJobQueue)
            {
                poJob->SetSrcMaskBufferHolder(oSrcMaskBufferHolder);
                poJob->SetSrcBufferHolder(oSrcBufferHolder);
                poJobQueue->SubmitJob(JobResampleFunc, poJob.get());
                jobList.emplace_back(std::move(poJob));
            }
            else
            {
                JobResampleFunc(poJob.get());
                eErr = poJob->eErr;
                if (eErr == CE_None)
                {
                    eErr = WriteJobData(poJob.get());
                }
            }
        }

        if (poJobQueue)
        {
            pChunk = nullptr;
            pabyChunkNodataMask = nullptr;
        }
    }

    VSIFree(pChunk);
    VSIFree(pabyChunkNodataMask);

    // Wait for all pending jobs to complete
    while (!jobList.empty())
    {
        const auto l_eErr = WaitAndFinalizeOldestJob(jobList);
        if (l_eErr != CE_None && eErr == CE_None)
            eErr = l_eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Renormalized overview mean / stddev if needed.                  */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None && EQUAL(pszResampling, "AVERAGE_MP"))
    {
        GDALOverviewMagnitudeCorrection(
            poSrcBand, nOverviewCount,
            reinterpret_cast<GDALRasterBandH *>(papoOvrBands),
            GDALDummyProgress, nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      It can be important to flush out data to overviews.             */
    /* -------------------------------------------------------------------- */
    for (int iOverview = 0; eErr == CE_None && iOverview < nOverviewCount;
         ++iOverview)
    {
        eErr = papoOvrBands[iOverview]->FlushCache(false);
    }

    if (eErr == CE_None)
        pfnProgress(1.0, nullptr, pProgressData);

    return eErr;
}

/************************************************************************/
/*                  GDALRegenerateOverviewsMultiBand()                  */
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
 * @param papszOptions (GDAL >= 3.6) NULL terminated list of options as
 *                     key=value pairs, or NULL
 *                     Starting with GDAL 3.8, the XOFF, YOFF, XSIZE and YSIZE
 *                     options can be specified to express that overviews should
 *                     be regenerated only in the specified subset of the source
 *                     dataset.
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALRegenerateOverviewsMultiBand(
    int nBands, GDALRasterBand *const *papoSrcBands, int nOverviews,
    GDALRasterBand *const *const *papapoOverviewBands,
    const char *pszResampling, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)
{
    CPL_IGNORE_RET_VAL(papszOptions);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    if (EQUAL(pszResampling, "NONE") || nBands == 0 || nOverviews == 0)
        return CE_None;

    // Sanity checks.
    if (!STARTS_WITH_CI(pszResampling, "NEAR") &&
        !EQUAL(pszResampling, "RMS") && !EQUAL(pszResampling, "AVERAGE") &&
        !EQUAL(pszResampling, "GAUSS") && !EQUAL(pszResampling, "CUBIC") &&
        !EQUAL(pszResampling, "CUBICSPLINE") &&
        !EQUAL(pszResampling, "LANCZOS") && !EQUAL(pszResampling, "BILINEAR") &&
        !EQUAL(pszResampling, "MODE"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRegenerateOverviewsMultiBand: pszResampling='%s' "
                 "not supported",
                 pszResampling);
        return CE_Failure;
    }

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn =
        GDALGetResampleFunction(pszResampling, &nKernelRadius);
    if (pfnResampleFn == nullptr)
        return CE_Failure;

    const int nToplevelSrcWidth = papoSrcBands[0]->GetXSize();
    const int nToplevelSrcHeight = papoSrcBands[0]->GetYSize();
    if (nToplevelSrcWidth <= 0 || nToplevelSrcHeight <= 0)
        return CE_None;
    GDALDataType eDataType = papoSrcBands[0]->GetRasterDataType();
    for (int iBand = 1; iBand < nBands; ++iBand)
    {
        if (papoSrcBands[iBand]->GetXSize() != nToplevelSrcWidth ||
            papoSrcBands[iBand]->GetYSize() != nToplevelSrcHeight)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same dimensions");
            return CE_Failure;
        }
        if (papoSrcBands[iBand]->GetRasterDataType() != eDataType)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same data type");
            return CE_Failure;
        }
    }

    for (int iOverview = 0; iOverview < nOverviews; ++iOverview)
    {
        const auto poOvrFirstBand = papapoOverviewBands[0][iOverview];
        const int nDstWidth = poOvrFirstBand->GetXSize();
        const int nDstHeight = poOvrFirstBand->GetYSize();
        for (int iBand = 1; iBand < nBands; ++iBand)
        {
            const auto poOvrBand = papapoOverviewBands[iBand][iOverview];
            if (poOvrBand->GetXSize() != nDstWidth ||
                poOvrBand->GetYSize() != nDstHeight)
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "of the same level must have the same dimensions");
                return CE_Failure;
            }
            if (poOvrBand->GetRasterDataType() != eDataType)
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "must have the same data type as the source bands");
                return CE_Failure;
            }
        }
    }

    // First pass to compute the total number of pixels to write.
    double dfTotalPixelCount = 0;
    const int nSrcXOff = atoi(CSLFetchNameValueDef(papszOptions, "XOFF", "0"));
    const int nSrcYOff = atoi(CSLFetchNameValueDef(papszOptions, "YOFF", "0"));
    const int nSrcXSize = atoi(CSLFetchNameValueDef(
        papszOptions, "XSIZE", CPLSPrintf("%d", nToplevelSrcWidth)));
    const int nSrcYSize = atoi(CSLFetchNameValueDef(
        papszOptions, "YSIZE", CPLSPrintf("%d", nToplevelSrcHeight)));
    for (int iOverview = 0; iOverview < nOverviews; ++iOverview)
    {
        dfTotalPixelCount +=
            static_cast<double>(nSrcXSize) / nToplevelSrcWidth *
            papapoOverviewBands[0][iOverview]->GetXSize() *
            static_cast<double>(nSrcYSize) / nToplevelSrcHeight *
            papapoOverviewBands[0][iOverview]->GetYSize();
    }

    const GDALDataType eWrkDataType =
        GDALGetOvrWorkDataType(pszResampling, eDataType);
    const int nWrkDataTypeSize =
        std::max(1, GDALGetDataTypeSizeBytes(eWrkDataType));

    const bool bIsMask = papoSrcBands[0]->IsMaskBand();

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.
    const bool bUseNoDataMask =
        !STARTS_WITH_CI(pszResampling, "NEAR") &&
        (bIsMask || (papoSrcBands[0]->GetMaskFlags() & GMF_ALL_VALID) == 0);

    std::vector<bool> abHasNoData(nBands);
    std::vector<double> adfNoDataValue(nBands);

    for (int iBand = 0; iBand < nBands; ++iBand)
    {
        int nHasNoData = 0;
        adfNoDataValue[iBand] =
            papoSrcBands[iBand]->GetNoDataValue(&nHasNoData);
        abHasNoData[iBand] = CPL_TO_BOOL(nHasNoData);
    }

    std::string osDetailMessage;
    if (bUseNoDataMask &&
        papoSrcBands[0]->HasConflictingMaskSources(&osDetailMessage, false))
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s%s", osDetailMessage.c_str(),
                 abHasNoData[0]
                     ? "Only the nodata value will be taken into account."
                     : "Only the first listed one will be taken into account.");
    }

    const bool bPropagateNoData =
        CPLTestBool(CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO"));

    const int nThreads = GDALGetNumThreads(GDAL_DEFAULT_MAX_THREAD_COUNT,
                                           /* bDefaultToAllCPUs=*/false);
    auto poThreadPool =
        nThreads > 1 ? GDALGetGlobalThreadPool(nThreads) : nullptr;
    auto poJobQueue = poThreadPool ? poThreadPool->CreateJobQueue()
                                   : std::unique_ptr<CPLJobQueue>(nullptr);

    // Only configurable for debug / testing
    const GIntBig nChunkMaxSize = []() -> GIntBig
    {
        const char *pszVal =
            CPLGetConfigOption("GDAL_OVR_CHUNK_MAX_SIZE", nullptr);
        if (pszVal)
        {
            GIntBig nRet = 0;
            CPLParseMemorySize(pszVal, &nRet, nullptr);
            return std::max<GIntBig>(100, nRet);
        }
        return 10 * 1024 * 1024;
    }();

    // Only configurable for debug / testing
    const GIntBig nChunkMaxSizeForTempFile = []() -> GIntBig
    {
        const char *pszVal = CPLGetConfigOption(
            "GDAL_OVR_CHUNK_MAX_SIZE_FOR_TEMP_FILE", nullptr);
        if (pszVal)
        {
            GIntBig nRet = 0;
            CPLParseMemorySize(pszVal, &nRet, nullptr);
            return std::max<GIntBig>(100, nRet);
        }
        const auto nUsableRAM = CPLGetUsablePhysicalRAM();
        if (nUsableRAM > 0)
            return nUsableRAM / 10;
        // Select a value to be able to at least downsample by 2 for a RGB
        // 1024x1024 tiled output: (2 * 1024 + 2) * (2 * 1024 + 2) * 3 = 12 MB
        return 100 * 1024 * 1024;
    }();

    // Second pass to do the real job.
    double dfCurPixelCount = 0;
    CPLErr eErr = CE_None;
    for (int iOverview = 0; iOverview < nOverviews && eErr == CE_None;
         ++iOverview)
    {
        int iSrcOverview = -1;  // -1 means the source bands.

        const int nDstTotalWidth =
            papapoOverviewBands[0][iOverview]->GetXSize();
        const int nDstTotalHeight =
            papapoOverviewBands[0][iOverview]->GetYSize();

        // Compute the coordinates of the target region to refresh
        constexpr double EPS = 1e-8;
        const int nDstXOffStart = static_cast<int>(
            static_cast<double>(nSrcXOff) / nToplevelSrcWidth * nDstTotalWidth +
            EPS);
        const int nDstXOffEnd =
            std::min(static_cast<int>(
                         std::ceil(static_cast<double>(nSrcXOff + nSrcXSize) /
                                       nToplevelSrcWidth * nDstTotalWidth -
                                   EPS)),
                     nDstTotalWidth);
        const int nDstWidth = nDstXOffEnd - nDstXOffStart;
        const int nDstYOffStart =
            static_cast<int>(static_cast<double>(nSrcYOff) /
                                 nToplevelSrcHeight * nDstTotalHeight +
                             EPS);
        const int nDstYOffEnd =
            std::min(static_cast<int>(
                         std::ceil(static_cast<double>(nSrcYOff + nSrcYSize) /
                                       nToplevelSrcHeight * nDstTotalHeight -
                                   EPS)),
                     nDstTotalHeight);
        const int nDstHeight = nDstYOffEnd - nDstYOffStart;

        // Try to use previous level of overview as the source to compute
        // the next level.
        int nSrcWidth = nToplevelSrcWidth;
        int nSrcHeight = nToplevelSrcHeight;
        if (iOverview > 0 &&
            papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstTotalWidth)
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
            iSrcOverview = iOverview - 1;
        }

        const double dfXRatioDstToSrc =
            static_cast<double>(nSrcWidth) / nDstTotalWidth;
        const double dfYRatioDstToSrc =
            static_cast<double>(nSrcHeight) / nDstTotalHeight;

        const int nOvrFactor =
            std::max(1, std::max(static_cast<int>(0.5 + dfXRatioDstToSrc),
                                 static_cast<int>(0.5 + dfYRatioDstToSrc)));

        int nDstChunkXSize = 0;
        int nDstChunkYSize = 0;
        papapoOverviewBands[0][iOverview]->GetBlockSize(&nDstChunkXSize,
                                                        &nDstChunkYSize);

        constexpr int PIXEL_MARGIN = 2;
        // Try to extend the chunk size so that the memory needed to acquire
        // source pixels goes up to 10 MB.
        // This can help for drivers that support multi-threaded reading
        const int nFullResYChunk = static_cast<int>(std::min<double>(
            nSrcHeight, PIXEL_MARGIN + nDstChunkYSize * dfYRatioDstToSrc));
        const int nFullResYChunkQueried = static_cast<int>(std::min<int64_t>(
            nSrcHeight,
            nFullResYChunk + static_cast<int64_t>(RADIUS_TO_DIAMETER) *
                                 nKernelRadius * nOvrFactor));
        while (nDstChunkXSize < nDstWidth)
        {
            constexpr int INCREASE_FACTOR = 2;

            const int nFullResXChunk = static_cast<int>(std::min<double>(
                nSrcWidth, PIXEL_MARGIN + INCREASE_FACTOR * nDstChunkXSize *
                                              dfXRatioDstToSrc));

            const int nFullResXChunkQueried =
                static_cast<int>(std::min<int64_t>(
                    nSrcWidth,
                    nFullResXChunk + static_cast<int64_t>(RADIUS_TO_DIAMETER) *
                                         nKernelRadius * nOvrFactor));

            if (nBands > nChunkMaxSize / nFullResXChunkQueried /
                             nFullResYChunkQueried / nWrkDataTypeSize)
            {
                break;
            }

            nDstChunkXSize *= INCREASE_FACTOR;
        }
        nDstChunkXSize = std::min(nDstChunkXSize, nDstWidth);

        const int nFullResXChunk = static_cast<int>(std::min<double>(
            nSrcWidth, PIXEL_MARGIN + nDstChunkXSize * dfXRatioDstToSrc));
        const int nFullResXChunkQueried = static_cast<int>(std::min<int64_t>(
            nSrcWidth,
            nFullResXChunk + static_cast<int64_t>(RADIUS_TO_DIAMETER) *
                                 nKernelRadius * nOvrFactor));

        // Make sure that the RAM requirements to acquire the source data does
        // not exceed nChunkMaxSizeForTempFile
        // If so, reduce the destination chunk size, generate overviews in a
        // temporary dataset, and copy that temporary dataset over the target
        // overview bands (to avoid issues with lossy compression)
        const bool bOverflowFullResXChunkYChunkQueried =
            nBands > std::numeric_limits<int64_t>::max() /
                         nFullResXChunkQueried / nFullResYChunkQueried /
                         nWrkDataTypeSize;

        const auto nMemRequirement =
            bOverflowFullResXChunkYChunkQueried
                ? 0
                : static_cast<GIntBig>(nFullResXChunkQueried) *
                      nFullResYChunkQueried * nBands * nWrkDataTypeSize;
        // Use a temporary dataset with a smaller destination chunk size
        const auto nOverShootFactor =
            nMemRequirement / nChunkMaxSizeForTempFile;

        constexpr int MIN_OVERSHOOT_FACTOR = 4;
        const auto nSqrtOverShootFactor = std::max<GIntBig>(
            MIN_OVERSHOOT_FACTOR, static_cast<GIntBig>(std::ceil(std::sqrt(
                                      static_cast<double>(nOverShootFactor)))));
        constexpr int DEFAULT_CHUNK_SIZE = 256;
        constexpr int GTIFF_BLOCK_SIZE_MULTIPLE = 16;
        const int nReducedDstChunkXSize =
            bOverflowFullResXChunkYChunkQueried
                ? DEFAULT_CHUNK_SIZE
                : std::max(1, static_cast<int>(nDstChunkXSize /
                                               nSqrtOverShootFactor) &
                                  ~(GTIFF_BLOCK_SIZE_MULTIPLE - 1));
        const int nReducedDstChunkYSize =
            bOverflowFullResXChunkYChunkQueried
                ? DEFAULT_CHUNK_SIZE
                : std::max(1, static_cast<int>(nDstChunkYSize /
                                               nSqrtOverShootFactor) &
                                  ~(GTIFF_BLOCK_SIZE_MULTIPLE - 1));

        if (bOverflowFullResXChunkYChunkQueried ||
            nMemRequirement > nChunkMaxSizeForTempFile)
        {
            const auto nDTSize =
                std::max(1, GDALGetDataTypeSizeBytes(eDataType));
            const bool bTmpDSMemRequirementOverflow =
                nBands > std::numeric_limits<int64_t>::max() / nDstWidth /
                             nDstHeight / nDTSize;
            const auto nTmpDSMemRequirement =
                bTmpDSMemRequirementOverflow
                    ? 0
                    : static_cast<GIntBig>(nDstWidth) * nDstHeight * nBands *
                          nDTSize;

            // make sure that one band buffer doesn't overflow size_t
            const bool bChunkSizeOverflow =
                static_cast<size_t>(nDTSize) >
                std::numeric_limits<size_t>::max() / nDstWidth / nDstHeight;
            const size_t nChunkSize =
                bChunkSizeOverflow
                    ? 0
                    : static_cast<size_t>(nDstWidth) * nDstHeight * nDTSize;

            const auto CreateVRT =
                [nBands, nSrcWidth, nSrcHeight, nDstTotalWidth, nDstTotalHeight,
                 pszResampling, eWrkDataType, papoSrcBands, papapoOverviewBands,
                 iSrcOverview, &abHasNoData,
                 &adfNoDataValue](int nVRTBlockXSize, int nVRTBlockYSize)
            {
                auto poVRTDS = std::make_unique<VRTDataset>(
                    nDstTotalWidth, nDstTotalHeight, nVRTBlockXSize,
                    nVRTBlockYSize);

                for (int iBand = 0; iBand < nBands; ++iBand)
                {
                    auto poVRTSrc = std::make_unique<VRTSimpleSource>();
                    poVRTSrc->SetResampling(pszResampling);
                    poVRTDS->AddBand(eWrkDataType);
                    auto poVRTBand = static_cast<VRTSourcedRasterBand *>(
                        poVRTDS->GetRasterBand(iBand + 1));

                    auto poSrcBand = papoSrcBands[iBand];
                    if (iSrcOverview != -1)
                        poSrcBand = papapoOverviewBands[iBand][iSrcOverview];
                    poVRTBand->ConfigureSource(
                        poVRTSrc.get(), poSrcBand, false, 0, 0, nSrcWidth,
                        nSrcHeight, 0, 0, nDstTotalWidth, nDstTotalHeight);
                    // Add the source to the band
                    poVRTBand->AddSource(poVRTSrc.release());
                    if (abHasNoData[iBand])
                        poVRTBand->SetNoDataValue(adfNoDataValue[iBand]);
                }

                if (papoSrcBands[0]->GetMaskFlags() == GMF_PER_DATASET &&
                    poVRTDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
                {
                    VRTSourcedRasterBand *poMaskVRTBand =
                        cpl::down_cast<VRTSourcedRasterBand *>(
                            poVRTDS->GetRasterBand(1)->GetMaskBand());
                    auto poSrcBand = papoSrcBands[0];
                    if (iSrcOverview != -1)
                        poSrcBand = papapoOverviewBands[0][iSrcOverview];
                    poMaskVRTBand->AddMaskBandSource(
                        poSrcBand->GetMaskBand(), 0, 0, nSrcWidth, nSrcHeight,
                        0, 0, nDstTotalWidth, nDstTotalHeight);
                }

                return poVRTDS;
            };

            // If the overview accommodates chunking, do so and recurse
            // to avoid generating full size temporary files
            if (!bOverflowFullResXChunkYChunkQueried &&
                !bTmpDSMemRequirementOverflow && !bChunkSizeOverflow &&
                (nDstChunkXSize < nDstWidth || nDstChunkYSize < nDstHeight))
            {
                // Create a VRT with the smaller chunk to do the scaling
                auto poVRTDS =
                    CreateVRT(nReducedDstChunkXSize, nReducedDstChunkYSize);

                std::vector<GDALRasterBand *> apoVRTBand(nBands);
                std::vector<GDALRasterBand *> apoDstBand(nBands);
                for (int iBand = 0; iBand < nBands; ++iBand)
                {
                    apoDstBand[iBand] = papapoOverviewBands[iBand][iOverview];
                    apoVRTBand[iBand] = poVRTDS->GetRasterBand(iBand + 1);
                }

                // Use a flag to avoid reading from the overview being built
                GDALRasterIOExtraArg sExtraArg;
                INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                if (iSrcOverview == -1)
                    sExtraArg.bUseOnlyThisScale = true;

                // A single band buffer for data transfer to the overview
                std::vector<GByte> abyChunk;
                try
                {
                    abyChunk.resize(nChunkSize);
                }
                catch (const std::exception &)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Out of memory allocating temporary buffer");
                    return CE_Failure;
                }

                // Loop over output height, in chunks
                for (int nDstYOff = nDstYOffStart;
                     nDstYOff < nDstYOffEnd && eErr == CE_None;
                     /* */)
                {
                    const int nDstYCount =
                        std::min(nDstChunkYSize, nDstYOffEnd - nDstYOff);
                    // Loop over output width, in output chunks
                    for (int nDstXOff = nDstXOffStart;
                         nDstXOff < nDstXOffEnd && eErr == CE_None;
                         /* */)
                    {
                        const int nDstXCount =
                            std::min(nDstChunkXSize, nDstXOffEnd - nDstXOff);
                        // Read and transfer the chunk to the overview
                        for (int iBand = 0; iBand < nBands && eErr == CE_None;
                             ++iBand)
                        {
                            eErr = apoVRTBand[iBand]->RasterIO(
                                GF_Read, nDstXOff, nDstYOff, nDstXCount,
                                nDstYCount, abyChunk.data(), nDstXCount,
                                nDstYCount, eDataType, 0, 0, &sExtraArg);
                            if (eErr == CE_None)
                            {
                                eErr = apoDstBand[iBand]->RasterIO(
                                    GF_Write, nDstXOff, nDstYOff, nDstXCount,
                                    nDstYCount, abyChunk.data(), nDstXCount,
                                    nDstYCount, eDataType, 0, 0, nullptr);
                            }
                        }

                        dfCurPixelCount +=
                            static_cast<double>(nDstXCount) * nDstYCount;

                        nDstXOff += nDstXCount;
                    }  // width

                    if (!pfnProgress(dfCurPixelCount / dfTotalPixelCount,
                                     nullptr, pProgressData))
                    {
                        CPLError(CE_Failure, CPLE_UserInterrupt,
                                 "User terminated");
                        eErr = CE_Failure;
                    }

                    nDstYOff += nDstYCount;
                }  // height

                if (CE_None != eErr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error while writing overview");
                    return CE_Failure;
                }

                pfnProgress(1.0, nullptr, pProgressData);
                // Flush the overviews we just generated
                for (int iBand = 0; iBand < nBands; ++iBand)
                    apoDstBand[iBand]->FlushCache(false);

                continue;  // Next overview
            }  // chunking via temporary dataset

            std::unique_ptr<GDALDataset> poTmpDS;
            // Config option mostly/only for autotest purposes
            const char *pszGDAL_OVR_TEMP_DRIVER =
                CPLGetConfigOption("GDAL_OVR_TEMP_DRIVER", "");
            if ((!bTmpDSMemRequirementOverflow &&
                 nTmpDSMemRequirement <= nChunkMaxSizeForTempFile &&
                 !EQUAL(pszGDAL_OVR_TEMP_DRIVER, "GTIFF")) ||
                EQUAL(pszGDAL_OVR_TEMP_DRIVER, "MEM"))
            {
                auto poTmpDrv = GetGDALDriverManager()->GetDriverByName("MEM");
                if (!poTmpDrv)
                {
                    eErr = CE_Failure;
                    break;
                }
                poTmpDS.reset(poTmpDrv->Create("", nDstTotalWidth,
                                               nDstTotalHeight, nBands,
                                               eDataType, nullptr));
            }
            else
            {
                // Create a temporary file for the overview
                auto poTmpDrv =
                    GetGDALDriverManager()->GetDriverByName("GTiff");
                if (!poTmpDrv)
                {
                    eErr = CE_Failure;
                    break;
                }
                std::string osTmpFilename;
                auto poDstDS = papapoOverviewBands[0][0]->GetDataset();
                if (poDstDS)
                {
                    osTmpFilename = poDstDS->GetDescription();
                    VSIStatBufL sStatBuf;
                    if (!osTmpFilename.empty() &&
                        VSIStatL(osTmpFilename.c_str(), &sStatBuf) == 0)
                        osTmpFilename += "_tmp_ovr.tif";
                }
                if (osTmpFilename.empty())
                {
                    osTmpFilename = CPLGenerateTempFilenameSafe(nullptr);
                    osTmpFilename += ".tif";
                }
                CPLDebug("GDAL", "Creating temporary file %s of %d x %d x %d",
                         osTmpFilename.c_str(), nDstWidth, nDstHeight, nBands);
                CPLStringList aosCO;
                if (0 == ((nReducedDstChunkXSize % GTIFF_BLOCK_SIZE_MULTIPLE) |
                          (nReducedDstChunkYSize % GTIFF_BLOCK_SIZE_MULTIPLE)))
                {
                    aosCO.SetNameValue("TILED", "YES");
                    aosCO.SetNameValue("BLOCKXSIZE",
                                       CPLSPrintf("%d", nReducedDstChunkXSize));
                    aosCO.SetNameValue("BLOCKYSIZE",
                                       CPLSPrintf("%d", nReducedDstChunkYSize));
                }
                if (const char *pszCOList =
                        poTmpDrv->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST))
                {
                    aosCO.SetNameValue(
                        "COMPRESS", strstr(pszCOList, "ZSTD") ? "ZSTD" : "LZW");
                }
                poTmpDS.reset(poTmpDrv->Create(osTmpFilename.c_str(), nDstWidth,
                                               nDstHeight, nBands, eDataType,
                                               aosCO.List()));
                if (poTmpDS)
                {
                    poTmpDS->MarkSuppressOnClose();
                    VSIUnlink(osTmpFilename.c_str());
                }
            }
            if (!poTmpDS)
            {
                eErr = CE_Failure;
                break;
            }

            // Create a full size VRT to do the resampling without edge effects
            auto poVRTDS =
                CreateVRT(nReducedDstChunkXSize, nReducedDstChunkYSize);

            // Allocate a band buffer with the overview chunk size
            std::unique_ptr<void, VSIFreeReleaser> pDstBuffer(
                VSI_MALLOC3_VERBOSE(size_t(nWrkDataTypeSize), nDstChunkXSize,
                                    nDstChunkYSize));
            if (pDstBuffer == nullptr)
            {
                eErr = CE_Failure;
                break;
            }

            // Use a flag to avoid reading the overview being built
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            if (iSrcOverview == -1)
                sExtraArg.bUseOnlyThisScale = true;

            // Scale and copy data from the VRT to the temp file
            for (int nDstYOff = nDstYOffStart;
                 nDstYOff < nDstYOffEnd && eErr == CE_None;
                 /* */)
            {
                const int nDstYCount =
                    std::min(nReducedDstChunkYSize, nDstYOffEnd - nDstYOff);
                for (int nDstXOff = nDstXOffStart;
                     nDstXOff < nDstXOffEnd && eErr == CE_None;
                     /* */)
                {
                    const int nDstXCount =
                        std::min(nReducedDstChunkXSize, nDstXOffEnd - nDstXOff);
                    for (int iBand = 0; iBand < nBands && eErr == CE_None;
                         ++iBand)
                    {
                        auto poSrcBand = poVRTDS->GetRasterBand(iBand + 1);
                        eErr = poSrcBand->RasterIO(
                            GF_Read, nDstXOff, nDstYOff, nDstXCount, nDstYCount,
                            pDstBuffer.get(), nDstXCount, nDstYCount,
                            eWrkDataType, 0, 0, &sExtraArg);
                        if (eErr == CE_None)
                        {
                            // Write to the temporary dataset, shifted
                            auto poOvrBand = poTmpDS->GetRasterBand(iBand + 1);
                            eErr = poOvrBand->RasterIO(
                                GF_Write, nDstXOff - nDstXOffStart,
                                nDstYOff - nDstYOffStart, nDstXCount,
                                nDstYCount, pDstBuffer.get(), nDstXCount,
                                nDstYCount, eWrkDataType, 0, 0, nullptr);
                        }
                    }
                    nDstXOff += nDstXCount;
                }
                nDstYOff += nDstYCount;
            }

            // Copy from the temporary to the overview
            for (int nDstYOff = nDstYOffStart;
                 nDstYOff < nDstYOffEnd && eErr == CE_None;
                 /* */)
            {
                const int nDstYCount =
                    std::min(nDstChunkYSize, nDstYOffEnd - nDstYOff);
                for (int nDstXOff = nDstXOffStart;
                     nDstXOff < nDstXOffEnd && eErr == CE_None;
                     /* */)
                {
                    const int nDstXCount =
                        std::min(nDstChunkXSize, nDstXOffEnd - nDstXOff);
                    for (int iBand = 0; iBand < nBands && eErr == CE_None;
                         ++iBand)
                    {
                        auto poSrcBand = poTmpDS->GetRasterBand(iBand + 1);
                        eErr = poSrcBand->RasterIO(
                            GF_Read, nDstXOff - nDstXOffStart,
                            nDstYOff - nDstYOffStart, nDstXCount, nDstYCount,
                            pDstBuffer.get(), nDstXCount, nDstYCount,
                            eWrkDataType, 0, 0, nullptr);
                        if (eErr == CE_None)
                        {
                            // Write to the destination overview bands
                            auto poOvrBand =
                                papapoOverviewBands[iBand][iOverview];
                            eErr = poOvrBand->RasterIO(
                                GF_Write, nDstXOff, nDstYOff, nDstXCount,
                                nDstYCount, pDstBuffer.get(), nDstXCount,
                                nDstYCount, eWrkDataType, 0, 0, nullptr);
                        }
                    }
                    nDstXOff += nDstXCount;
                }
                nDstYOff += nDstYCount;
            }

            if (eErr != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to write overview %d", iOverview);
                return eErr;
            }

            // Flush the data to overviews.
            for (int iBand = 0; iBand < nBands; ++iBand)
                papapoOverviewBands[iBand][iOverview]->FlushCache(false);

            continue;
        }

        // Structure describing a resampling job
        struct OvrJob
        {
            // Buffers to free when job is finished
            std::unique_ptr<PointerHolder> oSrcMaskBufferHolder{};
            std::unique_ptr<PointerHolder> oSrcBufferHolder{};
            std::unique_ptr<PointerHolder> oDstBufferHolder{};

            GDALRasterBand *poDstBand = nullptr;

            // Input parameters of pfnResampleFn
            GDALResampleFunction pfnResampleFn = nullptr;
            GDALOverviewResampleArgs args{};
            const void *pChunk = nullptr;

            // Output values of resampling function
            CPLErr eErr = CE_Failure;
            void *pDstBuffer = nullptr;
            GDALDataType eDstBufferDataType = GDT_Unknown;

            void NotifyFinished()
            {
                std::lock_guard guard(mutex);
                bFinished = true;
                cv.notify_one();
            }

            bool IsFinished()
            {
                std::lock_guard guard(mutex);
                return bFinished;
            }

            void WaitFinished()
            {
                std::unique_lock oGuard(mutex);
                while (!bFinished)
                {
                    cv.wait(oGuard);
                }
            }

          private:
            // Synchronization
            bool bFinished = false;
            std::mutex mutex{};
            std::condition_variable cv{};
        };

        // Thread function to resample
        const auto JobResampleFunc = [](void *pData)
        {
            OvrJob *poJob = static_cast<OvrJob *>(pData);

            poJob->eErr = poJob->pfnResampleFn(poJob->args, poJob->pChunk,
                                               &(poJob->pDstBuffer),
                                               &(poJob->eDstBufferDataType));

            poJob->oDstBufferHolder.reset(new PointerHolder(poJob->pDstBuffer));

            poJob->NotifyFinished();
        };

        // Function to write resample data to target band
        const auto WriteJobData = [](const OvrJob *poJob)
        {
            return poJob->poDstBand->RasterIO(
                GF_Write, poJob->args.nDstXOff, poJob->args.nDstYOff,
                poJob->args.nDstXOff2 - poJob->args.nDstXOff,
                poJob->args.nDstYOff2 - poJob->args.nDstYOff, poJob->pDstBuffer,
                poJob->args.nDstXOff2 - poJob->args.nDstXOff,
                poJob->args.nDstYOff2 - poJob->args.nDstYOff,
                poJob->eDstBufferDataType, 0, 0, nullptr);
        };

        // Wait for completion of oldest job and serialize it
        const auto WaitAndFinalizeOldestJob =
            [WriteJobData](std::list<std::unique_ptr<OvrJob>> &jobList)
        {
            auto poOldestJob = jobList.front().get();
            poOldestJob->WaitFinished();
            CPLErr l_eErr = poOldestJob->eErr;
            if (l_eErr == CE_None)
            {
                l_eErr = WriteJobData(poOldestJob);
            }

            jobList.pop_front();
            return l_eErr;
        };

        // Queue of jobs
        std::list<std::unique_ptr<OvrJob>> jobList;

        std::vector<std::unique_ptr<void, VSIFreeReleaser>> apaChunk(nBands);
        std::vector<std::unique_ptr<GByte, VSIFreeReleaser>>
            apabyChunkNoDataMask(nBands);

        // Iterate on destination overview, block by block.
        for (int nDstYOff = nDstYOffStart;
             nDstYOff < nDstYOffEnd && eErr == CE_None;
             nDstYOff += nDstChunkYSize)
        {
            int nDstYCount;
            if (nDstYOff + nDstChunkYSize <= nDstYOffEnd)
                nDstYCount = nDstChunkYSize;
            else
                nDstYCount = nDstYOffEnd - nDstYOff;

            int nChunkYOff = static_cast<int>(nDstYOff * dfYRatioDstToSrc);
            int nChunkYOff2 = static_cast<int>(
                ceil((nDstYOff + nDstYCount) * dfYRatioDstToSrc));
            if (nChunkYOff2 > nSrcHeight ||
                nDstYOff + nDstYCount == nDstTotalHeight)
                nChunkYOff2 = nSrcHeight;
            int nYCount = nChunkYOff2 - nChunkYOff;
            CPLAssert(nYCount <= nFullResYChunk);

            int nChunkYOffQueried = nChunkYOff - nKernelRadius * nOvrFactor;
            int nChunkYSizeQueried =
                nYCount + RADIUS_TO_DIAMETER * nKernelRadius * nOvrFactor;
            if (nChunkYOffQueried < 0)
            {
                nChunkYSizeQueried += nChunkYOffQueried;
                nChunkYOffQueried = 0;
            }
            if (nChunkYSizeQueried + nChunkYOffQueried > nSrcHeight)
                nChunkYSizeQueried = nSrcHeight - nChunkYOffQueried;
            CPLAssert(nChunkYSizeQueried <= nFullResYChunkQueried);

            if (!pfnProgress(std::min(1.0, dfCurPixelCount / dfTotalPixelCount),
                             nullptr, pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                eErr = CE_Failure;
            }

            // Iterate on destination overview, block by block.
            for (int nDstXOff = nDstXOffStart;
                 nDstXOff < nDstXOffEnd && eErr == CE_None;
                 nDstXOff += nDstChunkXSize)
            {
                int nDstXCount = 0;
                if (nDstXOff + nDstChunkXSize <= nDstXOffEnd)
                    nDstXCount = nDstChunkXSize;
                else
                    nDstXCount = nDstXOffEnd - nDstXOff;

                dfCurPixelCount += static_cast<double>(nDstXCount) * nDstYCount;

                int nChunkXOff = static_cast<int>(nDstXOff * dfXRatioDstToSrc);
                int nChunkXOff2 = static_cast<int>(
                    ceil((nDstXOff + nDstXCount) * dfXRatioDstToSrc));
                if (nChunkXOff2 > nSrcWidth ||
                    nDstXOff + nDstXCount == nDstTotalWidth)
                    nChunkXOff2 = nSrcWidth;
                const int nXCount = nChunkXOff2 - nChunkXOff;
                CPLAssert(nXCount <= nFullResXChunk);

                int nChunkXOffQueried = nChunkXOff - nKernelRadius * nOvrFactor;
                int nChunkXSizeQueried =
                    nXCount + RADIUS_TO_DIAMETER * nKernelRadius * nOvrFactor;
                if (nChunkXOffQueried < 0)
                {
                    nChunkXSizeQueried += nChunkXOffQueried;
                    nChunkXOffQueried = 0;
                }
                if (nChunkXSizeQueried + nChunkXOffQueried > nSrcWidth)
                    nChunkXSizeQueried = nSrcWidth - nChunkXOffQueried;
                CPLAssert(nChunkXSizeQueried <= nFullResXChunkQueried);
#if DEBUG_VERBOSE
                CPLDebug("GDAL",
                         "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)",
                         nChunkXOffQueried, nChunkYOffQueried,
                         nChunkXSizeQueried, nChunkYSizeQueried, nDstXOff,
                         nDstYOff, nDstXCount, nDstYCount);
#endif

                // Avoid accumulating too many tasks and exhaust RAM

                // Try to complete already finished jobs
                while (eErr == CE_None && !jobList.empty())
                {
                    auto poOldestJob = jobList.front().get();
                    if (!poOldestJob->IsFinished())
                        break;
                    eErr = poOldestJob->eErr;
                    if (eErr == CE_None)
                    {
                        eErr = WriteJobData(poOldestJob);
                    }

                    jobList.pop_front();
                }

                // And in case we have saturated the number of threads,
                // wait for completion of tasks to go below the threshold.
                while (eErr == CE_None &&
                       jobList.size() >= static_cast<size_t>(nThreads))
                {
                    eErr = WaitAndFinalizeOldestJob(jobList);
                }

                // Read the source buffers for all the bands.
                for (int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand)
                {
                    // (Re)allocate buffers if needed
                    if (apaChunk[iBand] == nullptr)
                    {
                        apaChunk[iBand].reset(VSI_MALLOC3_VERBOSE(
                            nFullResXChunkQueried, nFullResYChunkQueried,
                            nWrkDataTypeSize));
                        if (apaChunk[iBand] == nullptr)
                        {
                            eErr = CE_Failure;
                        }
                    }
                    if (bUseNoDataMask &&
                        apabyChunkNoDataMask[iBand] == nullptr)
                    {
                        apabyChunkNoDataMask[iBand].reset(
                            static_cast<GByte *>(VSI_MALLOC2_VERBOSE(
                                nFullResXChunkQueried, nFullResYChunkQueried)));
                        if (apabyChunkNoDataMask[iBand] == nullptr)
                        {
                            eErr = CE_Failure;
                        }
                    }

                    if (eErr == CE_None)
                    {
                        GDALRasterBand *poSrcBand = nullptr;
                        if (iSrcOverview == -1)
                            poSrcBand = papoSrcBands[iBand];
                        else
                            poSrcBand =
                                papapoOverviewBands[iBand][iSrcOverview];
                        eErr = poSrcBand->RasterIO(
                            GF_Read, nChunkXOffQueried, nChunkYOffQueried,
                            nChunkXSizeQueried, nChunkYSizeQueried,
                            apaChunk[iBand].get(), nChunkXSizeQueried,
                            nChunkYSizeQueried, eWrkDataType, 0, 0, nullptr);

                        if (bUseNoDataMask && eErr == CE_None)
                        {
                            auto poMaskBand = poSrcBand->IsMaskBand()
                                                  ? poSrcBand
                                                  : poSrcBand->GetMaskBand();
                            eErr = poMaskBand->RasterIO(
                                GF_Read, nChunkXOffQueried, nChunkYOffQueried,
                                nChunkXSizeQueried, nChunkYSizeQueried,
                                apabyChunkNoDataMask[iBand].get(),
                                nChunkXSizeQueried, nChunkYSizeQueried,
                                GDT_UInt8, 0, 0, nullptr);
                        }
                    }
                }

                // Compute the resulting overview block.
                for (int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand)
                {
                    auto poJob = std::make_unique<OvrJob>();
                    poJob->pfnResampleFn = pfnResampleFn;
                    poJob->poDstBand = papapoOverviewBands[iBand][iOverview];
                    poJob->args.eOvrDataType =
                        poJob->poDstBand->GetRasterDataType();
                    poJob->args.nOvrXSize = poJob->poDstBand->GetXSize();
                    poJob->args.nOvrYSize = poJob->poDstBand->GetYSize();
                    const char *pszNBITS = poJob->poDstBand->GetMetadataItem(
                        "NBITS", "IMAGE_STRUCTURE");
                    poJob->args.nOvrNBITS = pszNBITS ? atoi(pszNBITS) : 0;
                    poJob->args.dfXRatioDstToSrc = dfXRatioDstToSrc;
                    poJob->args.dfYRatioDstToSrc = dfYRatioDstToSrc;
                    poJob->args.eWrkDataType = eWrkDataType;
                    poJob->pChunk = apaChunk[iBand].get();
                    poJob->args.pabyChunkNodataMask =
                        apabyChunkNoDataMask[iBand].get();
                    poJob->args.nChunkXOff = nChunkXOffQueried;
                    poJob->args.nChunkXSize = nChunkXSizeQueried;
                    poJob->args.nChunkYOff = nChunkYOffQueried;
                    poJob->args.nChunkYSize = nChunkYSizeQueried;
                    poJob->args.nDstXOff = nDstXOff;
                    poJob->args.nDstXOff2 = nDstXOff + nDstXCount;
                    poJob->args.nDstYOff = nDstYOff;
                    poJob->args.nDstYOff2 = nDstYOff + nDstYCount;
                    poJob->args.pszResampling = pszResampling;
                    poJob->args.bHasNoData = abHasNoData[iBand];
                    poJob->args.dfNoDataValue = adfNoDataValue[iBand];
                    poJob->args.eSrcDataType = eDataType;
                    poJob->args.bPropagateNoData = bPropagateNoData;

                    if (poJobQueue)
                    {
                        poJob->oSrcMaskBufferHolder.reset(new PointerHolder(
                            apabyChunkNoDataMask[iBand].release()));

                        poJob->oSrcBufferHolder.reset(
                            new PointerHolder(apaChunk[iBand].release()));

                        poJobQueue->SubmitJob(JobResampleFunc, poJob.get());
                        jobList.emplace_back(std::move(poJob));
                    }
                    else
                    {
                        JobResampleFunc(poJob.get());
                        eErr = poJob->eErr;
                        if (eErr == CE_None)
                        {
                            eErr = WriteJobData(poJob.get());
                        }
                    }
                }
            }
        }

        // Wait for all pending jobs to complete
        while (!jobList.empty())
        {
            const auto l_eErr = WaitAndFinalizeOldestJob(jobList);
            if (l_eErr != CE_None && eErr == CE_None)
                eErr = l_eErr;
        }

        // Flush the data to overviews.
        for (int iBand = 0; iBand < nBands; ++iBand)
        {
            if (papapoOverviewBands[iBand][iOverview]->FlushCache(false) !=
                CE_None)
                eErr = CE_Failure;
        }
    }

    if (eErr == CE_None)
        pfnProgress(1.0, nullptr, pProgressData);

    return eErr;
}

/************************************************************************/
/*                  GDALRegenerateOverviewsMultiBand()                  */
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
 * The GDAL_NUM_THREADS configuration option can be set
 * to "ALL_CPUS" or a integer value to specify the number of threads to use for
 * overview computation.
 *
 * @param apoSrcBands the list of source bands to downsample
 * @param aapoOverviewBands bidimension array of bands. First dimension is
 *                          indexed by bands. Second dimension is indexed by
 *                          overview levels. All aapoOverviewBands[i] arrays
 *                          must have the same size (i.e. same number of
 *                          overviews)
 * @param pszResampling Resampling algorithm ("NEAREST", "AVERAGE", "RMS",
 * "GAUSS", "CUBIC", "CUBICSPLINE", "LANCZOS" or "BILINEAR").
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @param papszOptions NULL terminated list of options as
 *                     key=value pairs, or NULL
 *                     The XOFF, YOFF, XSIZE and YSIZE
 *                     options can be specified to express that overviews should
 *                     be regenerated only in the specified subset of the source
 *                     dataset.
 * @return CE_None on success or CE_Failure on failure.
 * @since 3.10
 */

CPLErr GDALRegenerateOverviewsMultiBand(
    const std::vector<GDALRasterBand *> &apoSrcBands,
    const std::vector<std::vector<GDALRasterBand *>> &aapoOverviewBands,
    const char *pszResampling, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)
{
    CPLAssert(apoSrcBands.size() == aapoOverviewBands.size());
    for (size_t i = 1; i < aapoOverviewBands.size(); ++i)
    {
        CPLAssert(aapoOverviewBands[i].size() == aapoOverviewBands[0].size());
    }

    if (aapoOverviewBands.empty())
        return CE_None;

    std::vector<GDALRasterBand **> apapoOverviewBands;
    for (auto &apoOverviewBands : aapoOverviewBands)
    {
        auto papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLMalloc(apoOverviewBands.size() * sizeof(GDALRasterBand *)));
        for (size_t i = 0; i < apoOverviewBands.size(); ++i)
        {
            papoOverviewBands[i] = apoOverviewBands[i];
        }
        apapoOverviewBands.push_back(papoOverviewBands);
    }
    const CPLErr eErr = GDALRegenerateOverviewsMultiBand(
        static_cast<int>(apoSrcBands.size()), apoSrcBands.data(),
        static_cast<int>(aapoOverviewBands[0].size()),
        apapoOverviewBands.data(), pszResampling, pfnProgress, pProgressData,
        papszOptions);
    for (GDALRasterBand **papoOverviewBands : apapoOverviewBands)
        CPLFree(papoOverviewBands);
    return eErr;
}

/************************************************************************/
/*                        GDALComputeBandStats()                        */
/************************************************************************/

/** Undocumented
 * @param hSrcBand undocumented.
 * @param nSampleStep Step between scanlines used to compute statistics.
 *                    When nSampleStep is equal to 1, all scanlines will
 *                    be processed.
 * @param pdfMean undocumented.
 * @param pdfStdDev undocumented.
 * @param pfnProgress undocumented.
 * @param pProgressData undocumented.
 * @return undocumented
 */
CPLErr CPL_STDCALL GDALComputeBandStats(GDALRasterBandH hSrcBand,
                                        int nSampleStep, double *pdfMean,
                                        double *pdfStdDev,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData)

{
    VALIDATE_POINTER1(hSrcBand, "GDALComputeBandStats", CE_Failure);

    GDALRasterBand *poSrcBand = GDALRasterBand::FromHandle(hSrcBand);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    if (nSampleStep >= nHeight || nSampleStep < 1)
        nSampleStep = 1;

    GDALDataType eWrkType = GDT_Unknown;
    float *pafData = nullptr;
    GDALDataType eType = poSrcBand->GetRasterDataType();
    const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
    if (bComplex)
    {
        pafData = static_cast<float *>(
            VSI_MALLOC2_VERBOSE(nWidth, 2 * sizeof(float)));
        eWrkType = GDT_CFloat32;
    }
    else
    {
        pafData =
            static_cast<float *>(VSI_MALLOC2_VERBOSE(nWidth, sizeof(float)));
        eWrkType = GDT_Float32;
    }

    if (nWidth == 0 || pafData == nullptr)
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
        if (!pfnProgress(iLine / static_cast<double>(nHeight), nullptr,
                         pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            CPLFree(pafData);
            return CE_Failure;
        }

        const CPLErr eErr =
            poSrcBand->RasterIO(GF_Read, 0, iLine, nWidth, 1, pafData, nWidth,
                                1, eWrkType, 0, 0, nullptr);
        if (eErr != CE_None)
        {
            CPLFree(pafData);
            return eErr;
        }

        for (int iPixel = 0; iPixel < nWidth; ++iPixel)
        {
            float fValue = 0.0f;

            if (bComplex)
            {
                // Compute the magnitude of the complex value.
                fValue =
                    std::hypot(pafData[static_cast<size_t>(iPixel) * 2],
                               pafData[static_cast<size_t>(iPixel) * 2 + 1]);
            }
            else
            {
                fValue = pafData[iPixel];
            }

            dfSum += static_cast<double>(fValue);
            dfSum2 += static_cast<double>(fValue) * static_cast<double>(fValue);
        }

        nSamples += nWidth;
        iLine += nSampleStep;
    } while (iLine < nHeight);

    if (!pfnProgress(1.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        CPLFree(pafData);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Produce the result values.                                      */
    /* -------------------------------------------------------------------- */
    if (pdfMean != nullptr)
        *pdfMean = dfSum / nSamples;

    if (pdfStdDev != nullptr)
    {
        const double dfMean = dfSum / nSamples;

        *pdfStdDev = sqrt((dfSum2 / nSamples) - (dfMean * dfMean));
    }

    CPLFree(pafData);

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
CPLErr GDALOverviewMagnitudeCorrection(GDALRasterBandH hBaseBand,
                                       int nOverviewCount,
                                       GDALRasterBandH *pahOverviews,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData)

{
    VALIDATE_POINTER1(hBaseBand, "GDALOverviewMagnitudeCorrection", CE_Failure);

    /* -------------------------------------------------------------------- */
    /*      Compute mean/stddev for source raster.                          */
    /* -------------------------------------------------------------------- */
    double dfOrigMean = 0.0;
    double dfOrigStdDev = 0.0;
    {
        const CPLErr eErr =
            GDALComputeBandStats(hBaseBand, 2, &dfOrigMean, &dfOrigStdDev,
                                 pfnProgress, pProgressData);

        if (eErr != CE_None)
            return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Loop on overview bands.                                         */
    /* -------------------------------------------------------------------- */
    for (int iOverview = 0; iOverview < nOverviewCount; ++iOverview)
    {
        GDALRasterBand *poOverview =
            GDALRasterBand::FromHandle(pahOverviews[iOverview]);
        double dfOverviewMean, dfOverviewStdDev;

        const CPLErr eErr =
            GDALComputeBandStats(pahOverviews[iOverview], 1, &dfOverviewMean,
                                 &dfOverviewStdDev, pfnProgress, pProgressData);

        if (eErr != CE_None)
            return eErr;

        double dfGain = 1.0;
        if (dfOrigStdDev >= 0.0001)
            dfGain = dfOrigStdDev / dfOverviewStdDev;

        /* --------------------------------------------------------------------
         */
        /*      Apply gain and offset. */
        /* --------------------------------------------------------------------
         */
        const int nWidth = poOverview->GetXSize();
        const int nHeight = poOverview->GetYSize();

        GDALDataType eWrkType = GDT_Unknown;
        float *pafData = nullptr;
        const GDALDataType eType = poOverview->GetRasterDataType();
        const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
        if (bComplex)
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, 2 * sizeof(float)));
            eWrkType = GDT_CFloat32;
        }
        else
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, sizeof(float)));
            eWrkType = GDT_Float32;
        }

        if (pafData == nullptr)
        {
            return CE_Failure;
        }

        for (int iLine = 0; iLine < nHeight; ++iLine)
        {
            if (!pfnProgress(iLine / static_cast<double>(nHeight), nullptr,
                             pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                CPLFree(pafData);
                return CE_Failure;
            }

            if (poOverview->RasterIO(GF_Read, 0, iLine, nWidth, 1, pafData,
                                     nWidth, 1, eWrkType, 0, 0,
                                     nullptr) != CE_None)
            {
                CPLFree(pafData);
                return CE_Failure;
            }

            for (int iPixel = 0; iPixel < nWidth; ++iPixel)
            {
                if (bComplex)
                {
                    pafData[static_cast<size_t>(iPixel) * 2] *=
                        static_cast<float>(dfGain);
                    pafData[static_cast<size_t>(iPixel) * 2 + 1] *=
                        static_cast<float>(dfGain);
                }
                else
                {
                    pafData[iPixel] = static_cast<float>(
                        (double(pafData[iPixel]) - dfOverviewMean) * dfGain +
                        dfOrigMean);
                }
            }

            if (poOverview->RasterIO(GF_Write, 0, iLine, nWidth, 1, pafData,
                                     nWidth, 1, eWrkType, 0, 0,
                                     nullptr) != CE_None)
            {
                CPLFree(pafData);
                return CE_Failure;
            }
        }

        if (!pfnProgress(1.0, nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            CPLFree(pafData);
            return CE_Failure;
        }

        CPLFree(pafData);
    }

    return CE_None;
}
