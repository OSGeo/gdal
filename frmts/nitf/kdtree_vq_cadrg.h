/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Specialization of KDTree for CADRG VQ compression
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef KDTREE_VQ_CADRG_INCLUDED
#define KDTREE_VQ_CADRG_INCLUDED

#include "kdtree.h"

#include <array>
#include <limits>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Weffc++"
#endif
#include "../../third_party/libdivide/libdivide.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef KDTREE_USE_SSE2

#include <emmintrin.h>
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace
{
inline __m128i blendv_epi8(__m128i a, __m128i b, __m128i mask)
{
#ifdef __SSE4_1__
    return _mm_blendv_epi8(a, b, mask);
#else
    return _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, b));
#endif
}
}  // namespace
#endif

namespace
{
template <class T> T square(T x)
{
    return x * x;
}

/************************************************************************/
/*                            filled_array()                            */
/************************************************************************/

template <typename T, std::size_t N>
constexpr std::array<T, N> filled_array(const T &value)
{
#ifdef __COVERITY__
    std::array<T, N> a{};
#else
    std::array<T, N> a;
#endif
    for (auto &x : a)
        x = value;
    return a;
}
}  // namespace

/************************************************************************/
/*                       ColorTableBased4x4Pixels                       */
/************************************************************************/

struct ColorTableBased4x4Pixels
{
    static constexpr int COMP_COUNT = 3;
    explicit ColorTableBased4x4Pixels(const std::vector<GByte> &R,
                                      const std::vector<GByte> &G,
                                      const std::vector<GByte> &B)
        : m_R(R), m_G(G), m_B(B), m_RGB({&m_R, &m_G, &m_B})
#if defined(KDTREE_USE_SSE2) && defined(__AVX2__)
          ,
          m_RGB32({&m_R32, &m_G32, &m_B32})
#endif
    {
#if defined(KDTREE_USE_SSE2) && defined(__AVX2__)
        for (size_t i = 0; i < R.size(); ++i)
        {
            m_R32.push_back(R[i]);
            m_G32.push_back(G[i]);
            m_B32.push_back(B[i]);
        }
#endif
    }

    const std::vector<GByte> &m_R;
    const std::vector<GByte> &m_G;
    const std::vector<GByte> &m_B;
    std::array<const std::vector<GByte> *const, COMP_COUNT> m_RGB;
#if defined(KDTREE_USE_SSE2) && defined(__AVX2__)
    std::vector<int32_t> m_R32{}, m_G32{}, m_B32{};
    std::array<const std::vector<int32_t> *const, COMP_COUNT> m_RGB32;
#endif
};

/************************************************************************/
/*                   Vector<ColorTableBased4x4Pixels>                   */
/************************************************************************/

template <> class Vector<ColorTableBased4x4Pixels>
{
  public:
    static constexpr int PIX_COUNT = 4 * 4;

  private:
    static constexpr int COMP_COUNT = ColorTableBased4x4Pixels::COMP_COUNT;

    std::array<GByte, PIX_COUNT> m_vals;

    // cppcheck-suppress uninitMemberVarPrivate
    Vector() = default;

  public:
    explicit Vector(const std::array<GByte, PIX_COUNT> &vals) : m_vals(vals)
    {
    }

    static constexpr int DIM_COUNT /* specialize */ = COMP_COUNT * PIX_COUNT;

    inline GByte val(int i) const
    {
        return m_vals[i];
    }

    inline GByte *vals()
    {
        return m_vals.data();
    }

    inline const std::array<GByte, PIX_COUNT> &vals() const
    {
        return m_vals;
    }

    static constexpr bool getReturnUInt8 /* specialize */ = true;

    inline int get(int i,
                   const ColorTableBased4x4Pixels &ctxt) const /* specialize */
    {
        return (*ctxt.m_RGB[i / PIX_COUNT])[m_vals[i % PIX_COUNT]];
    }

#if defined(KDTREE_USE_SSE2)
    static constexpr bool hasComputeFourSquaredDistances /* specialize */ =
        true;

#if defined(__SSE4_1__) && defined(__GNUC__)
    static constexpr bool hasComputeHeightSumAndSumSquareSSE2 /* specialize */ =
        true;

    /************************************************************************/
    /*                    computeHeightSumAndSumSquareSSE2()                  */
    /************************************************************************/

    inline void computeHeightSumAndSumSquareSSE2(
        int k, const ColorTableBased4x4Pixels &ctxt, int count, __m128i &sum0,
        __m128i &sumSquare0_lo, __m128i &sumSquare0_hi, __m128i &sum1,
        __m128i &sumSquare1_lo, __m128i &sumSquare1_hi) const
    {
#if defined(__AVX2__)
        const int32_t *comp_data = ctxt.m_RGB32[k / PIX_COUNT]->data();
        const GByte *pindices = m_vals.data() + (k % PIX_COUNT);
        const auto idx = _mm256_cvtepu8_epi32(
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(pindices)));
        constexpr int SCALE = static_cast<int>(sizeof(*comp_data));
        const auto vals = _mm256_i32gather_epi32(comp_data, idx, SCALE);
        const auto vcount = _mm256_set1_epi32(count);
        const auto vals_mul_count = _mm256_mullo_epi32(vals, vcount);
        sum0 = _mm_add_epi32(sum0, _mm256_castsi256_si128(vals_mul_count));
        sum1 = _mm_add_epi32(sum1, _mm256_extracti128_si256(vals_mul_count, 1));
        const auto vals_sq_mul_count = _mm256_mullo_epi32(vals, vals_mul_count);
        const auto vals0_sq_mul_count =
            _mm256_castsi256_si128(vals_sq_mul_count);
        const auto vals1_sq_mul_count =
            _mm256_extracti128_si256(vals_sq_mul_count, 1);
#else
        const GByte *comp_data = ctxt.m_RGB[k / PIX_COUNT]->data();
        const GByte *pindices = m_vals.data() + (k % PIX_COUNT);
        const auto i32_from_epu8_gather_epu8 =
            [](const GByte *base_addr, const GByte *pindices)
        {
            return _mm_setr_epi32(
                base_addr[pindices[0]], base_addr[pindices[1]],
                base_addr[pindices[2]], base_addr[pindices[3]]);
        };
        const auto vals0 = i32_from_epu8_gather_epu8(comp_data, pindices + 0);
        const auto vals1 = i32_from_epu8_gather_epu8(comp_data, pindices + 4);
        const auto vcount = _mm_set1_epi32(count);
        const auto vals0_mul_count = _mm_mullo_epi32(vals0, vcount);
        const auto vals1_mul_count = _mm_mullo_epi32(vals1, vcount);
        sum0 = _mm_add_epi32(sum0, vals0_mul_count);
        sum1 = _mm_add_epi32(sum1, vals1_mul_count);
        const auto vals0_sq_mul_count = _mm_mullo_epi32(vals0, vals0_mul_count);
        const auto vals1_sq_mul_count = _mm_mullo_epi32(vals1, vals1_mul_count);
#endif
        sumSquare0_lo = _mm_add_epi64(
            sumSquare0_lo,
            _mm_unpacklo_epi32(vals0_sq_mul_count, _mm_setzero_si128()));
        sumSquare0_hi = _mm_add_epi64(
            sumSquare0_hi,
            _mm_unpackhi_epi32(vals0_sq_mul_count, _mm_setzero_si128()));
        sumSquare1_lo = _mm_add_epi64(
            sumSquare1_lo,
            _mm_unpacklo_epi32(vals1_sq_mul_count, _mm_setzero_si128()));
        sumSquare1_hi = _mm_add_epi64(
            sumSquare1_hi,
            _mm_unpackhi_epi32(vals1_sq_mul_count, _mm_setzero_si128()));
    }

#else
    static constexpr bool hasComputeHeightSumAndSumSquareSSE2 /* specialize */ =
        false;
#endif

  private:
    /************************************************************************/
    /*                           gatherRGB_epi16()                          */
    /************************************************************************/

    static inline void gatherRGB_epi16(const GByte *indices,
                                       const ColorTableBased4x4Pixels &ctxt,
                                       __m128i &r, __m128i &g, __m128i &b)
    {
        const uint8_t i0 = indices[0];
        const uint8_t i1 = indices[1];
        const uint8_t i2 = indices[2];
        const uint8_t i3 = indices[3];
        const uint8_t i4 = indices[4];
        const uint8_t i5 = indices[5];
        const uint8_t i6 = indices[6];
        const uint8_t i7 = indices[7];

        r = _mm_setr_epi16(ctxt.m_R[i0], ctxt.m_R[i1], ctxt.m_R[i2],
                           ctxt.m_R[i3], ctxt.m_R[i4], ctxt.m_R[i5],
                           ctxt.m_R[i6], ctxt.m_R[i7]);

        g = _mm_setr_epi16(ctxt.m_G[i0], ctxt.m_G[i1], ctxt.m_G[i2],
                           ctxt.m_G[i3], ctxt.m_G[i4], ctxt.m_G[i5],
                           ctxt.m_G[i6], ctxt.m_G[i7]);

        b = _mm_setr_epi16(ctxt.m_B[i0], ctxt.m_B[i1], ctxt.m_B[i2],
                           ctxt.m_B[i3], ctxt.m_B[i4], ctxt.m_B[i5],
                           ctxt.m_B[i6], ctxt.m_B[i7]);
    }

    /************************************************************************/
    /*                            updateSums()                              */
    /************************************************************************/

    static inline void updateSums(const Vector *other, int i,
                                  const ColorTableBased4x4Pixels &ctxt,
                                  __m128i rA, __m128i gA, __m128i bA,
                                  __m128i &acc)
    {
        __m128i rB, gB, bB;
        gatherRGB_epi16(other->m_vals.data() + i, ctxt, rB, gB, bB);

        // Compute signed differences
        const auto diffR = _mm_sub_epi16(rA, rB);
        const auto diffG = _mm_sub_epi16(gA, gB);
        const auto diffB = _mm_sub_epi16(bA, bB);

        // Square differences
        const auto sqR = _mm_mullo_epi16(diffR, diffR);
        const auto sqG = _mm_mullo_epi16(diffG, diffG);
        const auto sqB = _mm_mullo_epi16(diffB, diffB);

        // Extend to 32 bit before summing R,G,B
        const auto sqR_lo = _mm_unpacklo_epi16(sqR, _mm_setzero_si128());
        const auto sqR_hi = _mm_unpackhi_epi16(sqR, _mm_setzero_si128());
        const auto sqG_lo = _mm_unpacklo_epi16(sqG, _mm_setzero_si128());
        const auto sqG_hi = _mm_unpackhi_epi16(sqG, _mm_setzero_si128());
        const auto sqB_lo = _mm_unpacklo_epi16(sqB, _mm_setzero_si128());
        const auto sqB_hi = _mm_unpackhi_epi16(sqB, _mm_setzero_si128());

        // Sum RGB
        acc = _mm_add_epi32(acc, sqR_lo);
        acc = _mm_add_epi32(acc, sqR_hi);
        acc = _mm_add_epi32(acc, sqG_lo);
        acc = _mm_add_epi32(acc, sqG_hi);
        acc = _mm_add_epi32(acc, sqB_lo);
        acc = _mm_add_epi32(acc, sqB_hi);
    }

  public:
    /************************************************************************/
    /*                   compute_four_squared_distances()                   */
    /************************************************************************/

    void compute_four_squared_distances(
        const std::array<const Vector *const, 4> &others,
        std::array<int, 4> & /* out */ tabSquaredDist,
        const ColorTableBased4x4Pixels &ctxt) const
    {
        __m128i acc_0 = _mm_setzero_si128();
        __m128i acc_1 = _mm_setzero_si128();
        __m128i acc_2 = _mm_setzero_si128();
        __m128i acc_3 = _mm_setzero_si128();

        for (int i = 0; i < 16; i += 8)
        {
            __m128i rA, gA, bA;
            gatherRGB_epi16(m_vals.data() + i, ctxt, rA, gA, bA);

            updateSums(others[0], i, ctxt, rA, gA, bA, acc_0);
            updateSums(others[1], i, ctxt, rA, gA, bA, acc_1);
            updateSums(others[2], i, ctxt, rA, gA, bA, acc_2);
            updateSums(others[3], i, ctxt, rA, gA, bA, acc_3);
        }

        const auto horizontalSum = [](__m128i acc)
        {
            // Horizontal reduction 4 => 1
            auto tmp = _mm_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2));
            acc = _mm_add_epi32(acc, tmp);
            tmp = _mm_shuffle_epi32(acc, _MM_SHUFFLE(2, 3, 0, 1));
            acc = _mm_add_epi32(acc, tmp);
            return _mm_cvtsi128_si32(acc);
        };

        tabSquaredDist[0] = horizontalSum(acc_0);
        tabSquaredDist[1] = horizontalSum(acc_1);
        tabSquaredDist[2] = horizontalSum(acc_2);
        tabSquaredDist[3] = horizontalSum(acc_3);
    }

#else
    static constexpr bool hasComputeFourSquaredDistances /* specialize */ =
        false;
#endif

    /************************************************************************/
    /*                          squared_distance()                          */
    /************************************************************************/

    int squared_distance(
        const Vector &other,
        const ColorTableBased4x4Pixels &ctxt) const /* specialize */
    {
#if defined(KDTREE_USE_SSE2) && !defined(__AVX2__)
        __m128i acc0 = _mm_setzero_si128();
        __m128i acc1 = _mm_setzero_si128();

        for (int i = 0; i < 2; ++i)
        {
            __m128i rA, gA, bA;
            gatherRGB_epi16(m_vals.data() + i * 8, ctxt, rA, gA, bA);

            __m128i rB, gB, bB;
            gatherRGB_epi16(other.m_vals.data() + i * 8, ctxt, rB, gB, bB);

            // Compute signed differences
            const auto diffR = _mm_sub_epi16(rA, rB);
            const auto diffG = _mm_sub_epi16(gA, gB);
            const auto diffB = _mm_sub_epi16(bA, bB);

            // Square differences
            const auto sqR = _mm_mullo_epi16(diffR, diffR);
            const auto sqG = _mm_mullo_epi16(diffG, diffG);
            const auto sqB = _mm_mullo_epi16(diffB, diffB);

            // Extend to 32 bit before summing R,G,B
            const auto sqR_lo = _mm_unpacklo_epi16(sqR, _mm_setzero_si128());
            const auto sqR_hi = _mm_unpackhi_epi16(sqR, _mm_setzero_si128());
            const auto sqG_lo = _mm_unpacklo_epi16(sqG, _mm_setzero_si128());
            const auto sqG_hi = _mm_unpackhi_epi16(sqG, _mm_setzero_si128());
            const auto sqB_lo = _mm_unpacklo_epi16(sqB, _mm_setzero_si128());
            const auto sqB_hi = _mm_unpackhi_epi16(sqB, _mm_setzero_si128());

            // Sum RGB
            acc0 = _mm_add_epi32(acc0, sqR_lo);
            acc1 = _mm_add_epi32(acc1, sqR_hi);
            acc0 = _mm_add_epi32(acc0, sqG_lo);
            acc1 = _mm_add_epi32(acc1, sqG_hi);
            acc0 = _mm_add_epi32(acc0, sqB_lo);
            acc1 = _mm_add_epi32(acc1, sqB_hi);
        }

        // Horizontal reduction 4 => 1
        auto acc = _mm_add_epi32(acc0, acc1);
        auto tmp = _mm_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2));
        acc = _mm_add_epi32(acc, tmp);
        tmp = _mm_shuffle_epi32(acc, _MM_SHUFFLE(2, 3, 0, 1));
        acc = _mm_add_epi32(acc, tmp);
        return _mm_cvtsi128_si32(acc);

#elif defined(KDTREE_USE_SSE2) && defined(__AVX2__)
        const auto idxA =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(m_vals.data()));
        const auto idxB = _mm_loadu_si128(
            reinterpret_cast<const __m128i *>(other.m_vals.data()));

        // Convert from 16 uint8_t values into to 2 vectors of 8 int32_t
        const auto idxA_lo = _mm256_cvtepu8_epi32(idxA);
        const auto idxB_lo = _mm256_cvtepu8_epi32(idxB);
        const auto idxA_hi = _mm256_cvtepu8_epi32(_mm_srli_si128(idxA, 8));
        const auto idxB_hi = _mm256_cvtepu8_epi32(_mm_srli_si128(idxB, 8));

        // Gather R, G, B for A and B (8 at a time)
        const auto gather_epi32 = [](const std::vector<int> &v, __m256i idx)
        {
            constexpr int SCALE = static_cast<int>(sizeof(int32_t));
            return _mm256_i32gather_epi32(v.data(), idx, SCALE);
        };
        const auto rA_lo = gather_epi32(ctxt.m_R32, idxA_lo);
        const auto rB_lo = gather_epi32(ctxt.m_R32, idxB_lo);
        const auto gA_lo = gather_epi32(ctxt.m_G32, idxA_lo);
        const auto gB_lo = gather_epi32(ctxt.m_G32, idxB_lo);
        const auto bA_lo = gather_epi32(ctxt.m_B32, idxA_lo);
        const auto bB_lo = gather_epi32(ctxt.m_B32, idxB_lo);

        const auto rA_hi = gather_epi32(ctxt.m_R32, idxA_hi);
        const auto rB_hi = gather_epi32(ctxt.m_R32, idxB_hi);
        const auto gA_hi = gather_epi32(ctxt.m_G32, idxA_hi);
        const auto gB_hi = gather_epi32(ctxt.m_G32, idxB_hi);
        const auto bA_hi = gather_epi32(ctxt.m_B32, idxA_hi);
        const auto bB_hi = gather_epi32(ctxt.m_B32, idxB_hi);

        // Compute square of differences
        const auto square_epi32 = [](__m256i x)
        { return _mm256_mullo_epi32(x, x); };

        const auto dr_lo = square_epi32(_mm256_sub_epi32(rA_lo, rB_lo));
        const auto dg_lo = square_epi32(_mm256_sub_epi32(gA_lo, gB_lo));
        const auto db_lo = square_epi32(_mm256_sub_epi32(bA_lo, bB_lo));

        const auto dr_hi = square_epi32(_mm256_sub_epi32(rA_hi, rB_hi));
        const auto dg_hi = square_epi32(_mm256_sub_epi32(gA_hi, gB_hi));
        const auto db_hi = square_epi32(_mm256_sub_epi32(bA_hi, bB_hi));

        // Sum RGB
        const auto sum_lo =
            _mm256_add_epi32(_mm256_add_epi32(dr_lo, dg_lo), db_lo);
        const auto sum_hi =
            _mm256_add_epi32(_mm256_add_epi32(dr_hi, dg_hi), db_hi);

        // Horizontal reduction 16 => 8
        const auto sum8 = _mm256_add_epi32(sum_lo, sum_hi);

        // Horizontal reduction 8 => 4
        const auto sum8_lo = _mm256_castsi256_si128(sum8);
        const auto sum8_hi = _mm256_extracti128_si256(sum8, 1);
        auto sum = _mm_add_epi32(sum8_lo, sum8_hi);

        // Horizontal reduction 4 => 1
        auto tmp = _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum = _mm_add_epi32(sum, tmp);
        tmp = _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1));
        sum = _mm_add_epi32(sum, tmp);

        return _mm_cvtsi128_si32(sum);

#else
        int nSqDist1 = 0;
        int nSqDist2 = 0;
        int nSqDist3 = 0;
        for (int i = 0; i < PIX_COUNT; ++i)
        {
            const int aEntry = m_vals[i];
            const int bEntry = other.m_vals[i];
            nSqDist1 += square(ctxt.m_R[aEntry] - ctxt.m_R[bEntry]);
            nSqDist2 += square(ctxt.m_G[aEntry] - ctxt.m_G[bEntry]);
            nSqDist3 += square(ctxt.m_B[aEntry] - ctxt.m_B[bEntry]);
        }
        return nSqDist1 + nSqDist2 + nSqDist3;
#endif
    }

    /************************************************************************/
    /*                              centroid()                              */
    /************************************************************************/

    static Vector
    centroid(const Vector &a, int nA, const Vector &b, int nB,
             const ColorTableBased4x4Pixels &ctxt) /* specialize */
    {
        auto minSqDist = filled_array<int, PIX_COUNT>(256 * 256 * COMP_COUNT);
        Vector res;
        libdivide::divider<uint32_t> divisor(static_cast<uint32_t>(nA + nB));
        for (int k = 0; k < PIX_COUNT; ++k)
        {
            const int aEntry = a.m_vals[k];
            const int bEntry = b.m_vals[k];
            const int meanR =
                static_cast<uint32_t>(ctxt.m_R[aEntry] * nA +
                                      ctxt.m_R[bEntry] * nB + (nA + nB) / 2) /
                divisor;
            const int meanG =
                static_cast<uint32_t>(ctxt.m_G[aEntry] * nA +
                                      ctxt.m_G[bEntry] * nB + (nA + nB) / 2) /
                divisor;
            const int meanB =
                static_cast<uint32_t>(ctxt.m_B[aEntry] * nA +
                                      ctxt.m_B[bEntry] * nB + (nA + nB) / 2) /
                divisor;

            assert(meanR <= 255);
            assert(meanG <= 255);
            assert(meanB <= 255);

#ifdef PRECISE_DISTANCE_COMPUTATION
            constexpr int BIT_SHIFT = 0;
#else
            // Minimum value to avoid int16 overflow when adding 3 squares of
            // uint8, because 3 * ((255 * 255) >> 3) = 24384 < INT16_MAX
            constexpr int BIT_SHIFT = 3;
#endif

            int i = 0;
#if defined(KDTREE_USE_SSE2)
            const auto targetR = _mm_set1_epi16(static_cast<short>(meanR));
            const auto targetG = _mm_set1_epi16(static_cast<short>(meanG));
            const auto targetB = _mm_set1_epi16(static_cast<short>(meanB));

            // Initialize min distance vector with max int32 values
#ifdef PRECISE_DISTANCE_COMPUTATION
            auto minDistVec0 = _mm_set1_epi32(std::numeric_limits<int>::max());
            auto minDistVec1 = _mm_set1_epi32(std::numeric_limits<int>::max());
            auto minDistVec2 = _mm_set1_epi32(std::numeric_limits<int>::max());
            auto minDistVec3 = _mm_set1_epi32(std::numeric_limits<int>::max());
#else
            auto minDistVec0 =
                _mm_set1_epi16(std::numeric_limits<short>::max());
            auto minDistVec1 =
                _mm_set1_epi16(std::numeric_limits<short>::max());
#endif

            // Initialize index vectors for tracking best index per lane
            auto idx = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                     13, 14, 15);
            const auto zero = _mm_setzero_si128();
            auto idxMin = zero;

            const auto square_lo_epi16 = [](__m128i x)
            { return _mm_mullo_epi16(x, x); };

            constexpr int VALS_AT_ONCE = 16;
            for (; i + VALS_AT_ONCE <= static_cast<int>(ctxt.m_R.size());
                 i += VALS_AT_ONCE)
            {
                // Load 16 color components
                const auto valsR = _mm_loadu_si128(
                    reinterpret_cast<const __m128i *>(ctxt.m_R.data() + i));
                const auto valsG = _mm_loadu_si128(
                    reinterpret_cast<const __m128i *>(ctxt.m_G.data() + i));
                const auto valsB = _mm_loadu_si128(
                    reinterpret_cast<const __m128i *>(ctxt.m_B.data() + i));

                const auto valsR_lo = _mm_unpacklo_epi8(valsR, zero);
                const auto valsR_hi = _mm_unpackhi_epi8(valsR, zero);
                const auto valsG_lo = _mm_unpacklo_epi8(valsG, zero);
                const auto valsG_hi = _mm_unpackhi_epi8(valsG, zero);
                const auto valsB_lo = _mm_unpacklo_epi8(valsB, zero);
                const auto valsB_hi = _mm_unpackhi_epi8(valsB, zero);

                // Compute signed differences
                const auto diffR_lo = _mm_sub_epi16(valsR_lo, targetR);
                const auto diffR_hi = _mm_sub_epi16(valsR_hi, targetR);
                const auto diffG_lo = _mm_sub_epi16(valsG_lo, targetG);
                const auto diffG_hi = _mm_sub_epi16(valsG_hi, targetG);
                const auto diffB_lo = _mm_sub_epi16(valsB_lo, targetB);
                const auto diffB_hi = _mm_sub_epi16(valsB_hi, targetB);

                // Square differences
                const auto sqR_lo = square_lo_epi16(diffR_lo);
                const auto sqR_hi = square_lo_epi16(diffR_hi);
                const auto sqG_lo = square_lo_epi16(diffG_lo);
                const auto sqG_hi = square_lo_epi16(diffG_hi);
                const auto sqB_lo = square_lo_epi16(diffB_lo);
                const auto sqB_hi = square_lo_epi16(diffB_hi);

#ifdef PRECISE_DISTANCE_COMPUTATION
                // Convert squares from 16-bit to 32-bit integers
                const auto sqR0 = _mm_unpacklo_epi16(sqR_lo, zero);
                const auto sqR1 = _mm_unpackhi_epi16(sqR_lo, zero);
                const auto sqR2 = _mm_unpacklo_epi16(sqR_hi, zero);
                const auto sqR3 = _mm_unpackhi_epi16(sqR_hi, zero);

                const auto sqG0 = _mm_unpacklo_epi16(sqG_lo, zero);
                const auto sqG1 = _mm_unpackhi_epi16(sqG_lo, zero);
                const auto sqG2 = _mm_unpacklo_epi16(sqG_hi, zero);
                const auto sqG3 = _mm_unpackhi_epi16(sqG_hi, zero);

                const auto sqB0 = _mm_unpacklo_epi16(sqB_lo, zero);
                const auto sqB1 = _mm_unpackhi_epi16(sqB_lo, zero);
                const auto sqB2 = _mm_unpacklo_epi16(sqB_hi, zero);
                const auto sqB3 = _mm_unpackhi_epi16(sqB_hi, zero);

                // Sum squared differences for each 32-bit lane: (R + G + B)
                const auto dist0 =
                    _mm_add_epi32(_mm_add_epi32(sqR0, sqG0), sqB0);
                const auto dist1 =
                    _mm_add_epi32(_mm_add_epi32(sqR1, sqG1), sqB1);
                const auto dist2 =
                    _mm_add_epi32(_mm_add_epi32(sqR2, sqG2), sqB2);
                const auto dist3 =
                    _mm_add_epi32(_mm_add_epi32(sqR3, sqG3), sqB3);

                // Compare with current minimum distances
                auto mask0 = _mm_cmplt_epi32(dist0, minDistVec0);
                auto mask1 = _mm_cmplt_epi32(dist1, minDistVec1);
                auto mask2 = _mm_cmplt_epi32(dist2, minDistVec2);
                auto mask3 = _mm_cmplt_epi32(dist3, minDistVec3);

                // Update minimum distances
                minDistVec0 = blendv_epi8(minDistVec0, dist0, mask0);
                minDistVec1 = blendv_epi8(minDistVec1, dist1, mask1);
                minDistVec2 = blendv_epi8(minDistVec2, dist2, mask2);
                minDistVec3 = blendv_epi8(minDistVec3, dist3, mask3);

                // Merge the 4 masks of 4 x uint32_t into
                // a single mask 16 x 1 uint8_t mask
                mask0 = _mm_srli_epi32(mask0, 24);
                mask1 = _mm_srli_epi32(mask1, 24);
                mask2 = _mm_srli_epi32(mask2, 24);
                mask3 = _mm_srli_epi32(mask3, 24);
                const auto mask_merged =
                    _mm_packus_epi16(_mm_packs_epi32(mask0, mask1),
                                     _mm_packs_epi32(mask2, mask3));

                // Update indices
                idxMin = blendv_epi8(idxMin, idx, mask_merged);

#else
                // Sum squared differences, by removing a few LSB bits to avoid
                // overflows.
                const auto dist0 = _mm_add_epi16(
                    _mm_add_epi16(_mm_srli_epi16(sqR_lo, BIT_SHIFT),
                                  _mm_srli_epi16(sqG_lo, BIT_SHIFT)),
                    _mm_srli_epi16(sqB_lo, BIT_SHIFT));
                const auto dist1 = _mm_add_epi16(
                    _mm_add_epi16(_mm_srli_epi16(sqR_hi, BIT_SHIFT),
                                  _mm_srli_epi16(sqG_hi, BIT_SHIFT)),
                    _mm_srli_epi16(sqB_hi, BIT_SHIFT));

                // Compare with current minimum distances
                auto mask0 = _mm_cmplt_epi16(dist0, minDistVec0);
                auto mask1 = _mm_cmplt_epi16(dist1, minDistVec1);

                // Update minimum distances
                minDistVec0 = blendv_epi8(minDistVec0, dist0, mask0);
                minDistVec1 = blendv_epi8(minDistVec1, dist1, mask1);

                // Merge the 2 masks of 8 x uint16_t into
                // a single mask 16 x 1 uint8_t mask
                mask0 = _mm_srli_epi16(mask0, 8);
                mask1 = _mm_srli_epi16(mask1, 8);
                const auto mask_merged = _mm_packus_epi16(mask0, mask1);

                // Update indices
                idxMin = blendv_epi8(idxMin, idx, mask_merged);
#endif

                idx = _mm_add_epi8(idx, _mm_set1_epi8(VALS_AT_ONCE));
            }

            // Horizontal update
#ifdef PRECISE_DISTANCE_COMPUTATION
            int minDistVals[VALS_AT_ONCE];
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 0),
                             minDistVec0);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 4),
                             minDistVec1);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 8),
                             minDistVec2);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 12),
                             minDistVec3);
#else
            short minDistVals[VALS_AT_ONCE];
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 0),
                             minDistVec0);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minDistVals + 8),
                             minDistVec1);
#endif

            GByte minIdxVals[VALS_AT_ONCE];
            _mm_storeu_si128(reinterpret_cast<__m128i *>(minIdxVals), idxMin);

            for (int j = 0; j < VALS_AT_ONCE; ++j)
            {
                if (minDistVals[j] < minSqDist[k] ||
                    (minDistVals[j] == minSqDist[k] &&
                     minIdxVals[j] < res.m_vals[k]))
                {
                    minSqDist[k] = minDistVals[j];
                    res.m_vals[k] = minIdxVals[j];
                }
            }
#endif

            // Generic/scalar code
            for (; i < static_cast<int>(ctxt.m_R.size()); ++i)
            {
                const int sqDist = (square(meanR - ctxt.m_R[i]) >> BIT_SHIFT) +
                                   (square(meanG - ctxt.m_G[i]) >> BIT_SHIFT) +
                                   (square(meanB - ctxt.m_B[i]) >> BIT_SHIFT);
                if (sqDist < minSqDist[k])
                {
                    minSqDist[k] = sqDist;
                    res.m_vals[k] = static_cast<GByte>(i);
                }
            }
        }
        return res;
    }

    /************************************************************************/
    /*                           operator == ()                             */
    /************************************************************************/

    inline bool operator==(const Vector &other) const
    {
        return m_vals == other.m_vals;
    }

    /************************************************************************/
    /*                           operator < ()                              */
    /************************************************************************/

    // Purely arbitrary for the purpose of distinguishing a vector from
    // another one
    inline bool operator<(const Vector &other) const
    {
        return m_vals < other.m_vals;
    }
};

#endif  // KDTREE_VQ_CADRG_INCLUDED
