/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Pairwise Nearest Neighbor (PNN) clustering for Vector
 *           Quantization (VQ) using a KDTree.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef KDTREE_INCLUDED
#define KDTREE_INCLUDED

/**
 * This file implements Pairwise Nearest Neighbor (PNN) clustering for Vector
 * Quantization (VQ) using a KDTree.
 *
 * It implements paper "A New Vector Quantization Clustering Algorithm", by
 * William H. Equitz, from IEEE Transactions on Acoustics, Speech, and Signal
 * Processing, Vol. 37, Issue 10, October 1989. DOI: 10.1109/29.35395
 * https://ieeexplore.ieee.org/document/35395 (behind paywall)
 *
 * A higher level (freely accessible) and more generic paper on PNN clustering
 * is also available at
 * https://www.researchgate.net/publication/27661047_Pairwise_Nearest_Neighbor_Method_Revisited
 *
 * Papers "Analysis of Compression Techniques for Common Mapping Stdandard (CMS)
 * Raster Data" by N.J. Markuson, July 1994 (https://apps.dtic.mil/sti/tr/pdf/ADA283396.pdf)
 * and "Compression of Digitized Map Image" by D.A. Southard, March 1992
 * (https://apps.dtic.mil/sti/tr/pdf/ADA250707.pdf) analyses VQ compression and
 * contain a high-level description of the Equitz paper.
 */

#include <cassert>
#include <cstdio>

#include <algorithm>
#include <array>
#include <deque>
#include <iterator>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "cpl_error.h"

// #define DEBUG_INVARIANTS

#ifdef KDTREE_DEBUG_TIMING
#include <sys/time.h>

static double totalTimeRebalancing = 0;
static double totalTimeCentroid = 0;
static double totalTimeStats = 0;

#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("unroll-loops")
#endif

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(KDTREE_DISABLE_SIMD)
#define KDTREE_USE_SSE2
#endif

#ifdef KDTREE_USE_SSE2
#include <emmintrin.h>
#endif

/************************************************************************/
/*                              Vector<T>                               */
/************************************************************************/

/** "Interface" of a "vector" of dimension DIM_COUNT to insert and cluster in
 * a PNNKDTree.
 *
 * Below functions must be implemented in classes that specialize Vector: there
 * is no default generic implementation.
 *
 * There are no constraints on the T type.
 */
template <class T> class Vector
{
  public:
    /** Returns the dimension of the vector. */
    static constexpr int DIM_COUNT = -1;

    /** Whether the get() method returns uint8_t values.
     * Used for speed optimizations.
     */
    static constexpr bool getReturnUInt8 = false;

    /** Returns the k(th) value of the vector, with k in [0, DIM_COUNT-1] range.
     *
     * The actual returned type might not be double, but must be convertible to
     * double.
     */
    double get(int k, const T &ctxt) const /* = 0 */;

#ifdef KDTREE_USE_SSE2
    /** Whether the computeHeightSumAndSumSquareSSE2() method is implemented.
     * Used for speed optimizations.
     */
    static constexpr bool hasComputeHeightSumAndSumSquareSSE2 = false;

    /** The function must do the equivalent of:
     *
     *   for (int i = 0; i < 8; ++i )
     *   {
     *      int val = item.m_vec.get(k + i, ctxt);
     *      int valMulCount = val * item.m_count;
     *      {sum0, sum1}[i] += valMulCount;
     *      {sumSquare0_lo, sumSquare0_hi,sumSquare1_lo, sumSquare1_hi}[i] += val * valMulCount;
     *   }
     *
     * k is in the [0, DIM_COUNT-8-1] range (and generally a multiple of 8).
     */
    void computeHeightSumAndSumSquareSSE2(int k, const T &ctxt, int count,
                                          __m128i &sum0, __m128i &sumSquare0_lo,
                                          __m128i &sumSquare0_hi, __m128i &sum1,
                                          __m128i &sumSquare1_lo,
                                          __m128i &sumSquare1_hi) const
        /* = 0 */;
#endif

    /** Returns the squared distance between this vector and other.
     * It must be symmetric, that is this->squared_distance(other, ctx) must
     * be equal to other.squared_distance(*this, ctx).
     */
    double squared_distance(const Vector &other, const T &ctxt) const /* = 0 */;

    /** Whether the compute_four_squared_distances() method is implemented
     * Used for speed optimizations.
     */
    static constexpr bool hasComputeFourSquaredDistances = false;

    /** Equivalent of
     *
     * for(int i = 0; i < 4; ++i)
     * {
     *      tabSquaredDist[i] = squared_distance(*(other[i]), ctxt);
     * }
     */
    void compute_four_squared_distances(
        const std::array<const Vector *const, 4> &others,
        std::array<int, 4> & /* out */ tabSquaredDist, const T &ctxt) const
        /* = 0 */;

    /** Computes a new vector that is the centroid of vector a of weight nA,
     * and vector b of weight nB.
     */
    static Vector centroid(const Vector &a, int nA, const Vector &b, int nB,
                           const T &ctxt) /* = 0 */;
};

/************************************************************************/
/*                            BucketItem<T>                             */
/************************************************************************/

/** Definition of an item placed in a bucket of a PNNKDTree.
 *
 * This class does not need to be specialized.
 */
template <class T> struct BucketItem
{
  public:
    /** Value vector */
    Vector<T> m_vec;

    /** Type of elements in m_origVectorIndices */
    using IdxType = int;

    /** Vector that points to indices in the original value space that evaluate
     * to m_vec.
     * Typically m_origVectorIndices.size() == m_count, but
     * the clustering algorithm will not enforce it. It will just concatenate
     * m_origVectorIndices from different BucketItem when merging them.
     */
    std::vector<IdxType> m_origVectorIndices;

    /** Number of samples that have the value of m_vec */
    int m_count;

    /** Constructor */
    BucketItem(const Vector<T> &vec, int count,
               std::vector<IdxType> &&origVectorIndices)
        : m_vec(vec), m_origVectorIndices(std::move(origVectorIndices)),
          m_count(count)
    {
    }

    BucketItem(BucketItem &&) = default;
    BucketItem &operator=(BucketItem &&) = default;

  private:
    BucketItem(const BucketItem &) = delete;
    BucketItem &operator=(const BucketItem &) = delete;
};

/************************************************************************/
/*                             PNNKDTree<T>                             */
/************************************************************************/

/**
 * KDTree designed for Pairwise Nearest Neighbor (PNN) clustering for Vector
 * Quantization (VQ).
 *
 * This class does not need to be specialized.
 */
template <class T> class PNNKDTree
{
  public:
    PNNKDTree() = default;

    /* Inserts value vectors with their cardinality in the KD-Tree.
     *
     * This method must be called only once.
     *
     * Returns the initial count of buckets, that must be passed as an input
     * to cluster().
     */
    int insert(std::vector<BucketItem<T>> &&vectors, const T &ctxt);

    /** Iterate over leaf nodes (that contain buckets) */
    void iterateOverLeaves(const std::function<void(PNNKDTree &)> &f);

    /** Perform clustering to reduce the number of buckets from initialBucketCount
     * to targetCount.
     *
     * It modifies the tree structure, and returns the achieved number of
     * buckets (<= targetCount).
     */
    int cluster(int initialBucketCount, int targetCount, const T &ctxt);

    /** Returns the bucket items for this node. */
    inline const std::vector<BucketItem<T>> &bucketItems() const
    {
        return m_bucketItems;
    }

    /** Returns the bucket items for this node. */
    inline std::vector<BucketItem<T>> &bucketItems()
    {
        return m_bucketItems;
    }

  private:
    static constexpr int BUCKET_MAX_SIZE = 8;

    /** Left node. When non null, m_right is also non null, and m_bucketItems is empty. */
    std::unique_ptr<PNNKDTree> m_left{};

    /** Right node. When non null, m_left is also non null, and m_bucketItems is empty. */
    std::unique_ptr<PNNKDTree> m_right{};

    /** Contains items that form a bucket. The bucket is nominally at most BUCKET_MAX_SIZE
     * large (maybe transiently slightly larger during clustering operations).
     *
     * m_bucketItems is non empty only on leaf nodes.
     */
    std::vector<BucketItem<T>> m_bucketItems{};

    /** Data type returned by Vector<T>::get() */
    using ValType = decltype(std::declval<Vector<T>>().get(
        0, *static_cast<const T *>(nullptr)));

    /** Clean the current node and move it to queueNodes.
     *
     * This saves dynamic allocation and de-allocation of nodes when rebalancing.
     */
    void freeAndMoveToQueue(std::deque<std::unique_ptr<PNNKDTree>> &queueNodes);

    int insert(std::vector<BucketItem<T>> &&vectors, int totalCount,
               std::vector<std::pair<ValType, int>> &weightedVals,
               std::deque<std::unique_ptr<PNNKDTree>> &queueNodes,
               std::vector<BucketItem<T>> &vectLeft,
               std::vector<BucketItem<T>> &vectRight, const T &ctxt);

    /** Rebalance the KD-Tree. Current implementation fully rebuilds a new
     * KD-Tree using the insert() algorithm
     */
    int rebalance(const T &ctxt, std::vector<BucketItem<T>> &newLeaves,
                  std::deque<std::unique_ptr<PNNKDTree>> &queueNodes);
};

/************************************************************************/
/*                        PNNKDTree<T>::insert()                        */
/************************************************************************/

template <class T>
int PNNKDTree<T>::insert(std::vector<BucketItem<T>> &&vectors, const T &ctxt)
{
    assert(m_left == nullptr);
    assert(m_right == nullptr);
    assert(m_bucketItems.empty());

    int totalCount = 0;
    for (const auto &it : vectors)
    {
        totalCount += it.m_count;
    }
    std::vector<std::pair<ValType, int>> weightedVals;
    std::deque<std::unique_ptr<PNNKDTree>> queueNodes;
    std::vector<BucketItem<T>> vectLeft;
    std::vector<BucketItem<T>> vectRight;
    if (totalCount == 0)
        return 0;
    return insert(std::move(vectors), totalCount, weightedVals, queueNodes,
                  vectLeft, vectRight, ctxt);
}

/************************************************************************/
/*                        PNNKDTree<T>::insert()                        */
/************************************************************************/

template <class T>
int PNNKDTree<T>::insert(std::vector<BucketItem<T>> &&vectors, int totalCount,
                         std::vector<std::pair<ValType, int>> &weightedVals,
                         std::deque<std::unique_ptr<PNNKDTree>> &queueNodes,
                         std::vector<BucketItem<T>> &vectLeft,
                         std::vector<BucketItem<T>> &vectRight, const T &ctxt)
{
#ifdef DEBUG_INVARIANTS
    std::map<Vector<T>, int> mapValuesToBucketIdx;
    for (int i = 0; i < static_cast<int>(vectors.size()); ++i)
    {
        CPLAssert(mapValuesToBucketIdx.find(vectors[i].m_vec) ==
                  mapValuesToBucketIdx.end());
        mapValuesToBucketIdx[vectors[i].m_vec] = i;
    }
#endif

    if (vectors.size() <= BUCKET_MAX_SIZE)
    {
        m_bucketItems = std::move(vectors);
        return static_cast<int>(m_bucketItems.size());
    }

#ifdef KDTREE_DEBUG_TIMING
    struct timeval tv1, tv2;
    gettimeofday(&tv1, nullptr);
#endif

    // Find dimension with maximum variance
    double maxM2 = 0;
    int maxM2_k = 0;

    for (int k = 0; k < Vector<T>::DIM_COUNT; ++k)
    {
        if constexpr (Vector<T>::getReturnUInt8)
        {
            constexpr int MAX_BYTE_VALUE = std::numeric_limits<uint8_t>::max();
            bool canUseOptimization =
                (totalCount <= std::numeric_limits<int64_t>::max() /
                                   (MAX_BYTE_VALUE * MAX_BYTE_VALUE));
            if (canUseOptimization)
            {
                int maxCountPerVector = 0;
                for (const auto &item : vectors)
                {
                    maxCountPerVector =
                        std::max(maxCountPerVector, item.m_count);
                }
                canUseOptimization = (maxCountPerVector <=
                                      std::numeric_limits<int32_t>::max() /
                                          (MAX_BYTE_VALUE * MAX_BYTE_VALUE));
            }
            if (canUseOptimization)
            {
                // Do statistics computation in integer domain if possible.

#if !(defined(__i386__) || defined(_M_IX86))
                // Below code requires more than 8 general purpose registers,
                // so exclude i386.

                constexpr int VALS_AT_ONCE = 4;
                if constexpr ((Vector<T>::DIM_COUNT % VALS_AT_ONCE) == 0)
                {
#ifdef KDTREE_USE_SSE2
                    constexpr int TWICE_VALS_AT_ONCE = 2 * VALS_AT_ONCE;
                    if constexpr ((Vector<T>::DIM_COUNT % TWICE_VALS_AT_ONCE) ==
                                      0 &&
                                  Vector<
                                      T>::hasComputeHeightSumAndSumSquareSSE2)
                    {
                        __m128i sum0 = _mm_setzero_si128();
                        __m128i sumSquare0_lo = _mm_setzero_si128();
                        __m128i sumSquare0_hi = _mm_setzero_si128();
                        __m128i sum1 = _mm_setzero_si128();
                        __m128i sumSquare1_lo = _mm_setzero_si128();
                        __m128i sumSquare1_hi = _mm_setzero_si128();

                        for (const auto &item : vectors)
                        {
                            item.m_vec.computeHeightSumAndSumSquareSSE2(
                                k, ctxt, item.m_count, sum0, sumSquare0_lo,
                                sumSquare0_hi, sum1, sumSquare1_lo,
                                sumSquare1_hi);
                        }
                        int64_t sumSquares[TWICE_VALS_AT_ONCE];
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(sumSquares + 0),
                            sumSquare0_lo);
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(sumSquares + 2),
                            sumSquare0_hi);
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(sumSquares + 4),
                            sumSquare1_lo);
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(sumSquares + 6),
                            sumSquare1_hi);
                        int sums[TWICE_VALS_AT_ONCE];
                        _mm_storeu_si128(reinterpret_cast<__m128i *>(sums),
                                         sum0);
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(sums + VALS_AT_ONCE),
                            sum1);
                        for (int i = 0; i < TWICE_VALS_AT_ONCE; ++i)
                        {
                            const double M2 = static_cast<double>(
                                sumSquares[i] * totalCount -
                                static_cast<int64_t>(sums[i]) * sums[i]);
                            if (M2 > maxM2)
                            {
                                maxM2 = M2;
                                maxM2_k = k + i;
                            }
                        }
                        k += TWICE_VALS_AT_ONCE - 1;
                    }
                    else
#endif
                    {
                        int sum0 = 0;
                        int sum1 = 0;
                        int sum2 = 0;
                        int sum3 = 0;
                        int64_t sumSquare0 = 0;
                        int64_t sumSquare1 = 0;
                        int64_t sumSquare2 = 0;
                        int64_t sumSquare3 = 0;
                        for (const auto &item : vectors)
                        {
                            const int val0 = item.m_vec.get(k + 0, ctxt);
                            const int val1 = item.m_vec.get(k + 1, ctxt);
                            const int val2 = item.m_vec.get(k + 2, ctxt);
                            const int val3 = item.m_vec.get(k + 3, ctxt);
                            const int val0MulCount = val0 * item.m_count;
                            const int val1MulCount = val1 * item.m_count;
                            const int val2MulCount = val2 * item.m_count;
                            const int val3MulCount = val3 * item.m_count;
                            sum0 += val0MulCount;
                            sum1 += val1MulCount;
                            sum2 += val2MulCount;
                            sum3 += val3MulCount;
                            // It's fine to cast to int64 after multiplication
                            sumSquare0 +=
                                cpl::fits_on<int64_t>(val0 * val0MulCount);
                            sumSquare1 +=
                                cpl::fits_on<int64_t>(val1 * val1MulCount);
                            sumSquare2 +=
                                cpl::fits_on<int64_t>(val2 * val2MulCount);
                            sumSquare3 +=
                                cpl::fits_on<int64_t>(val3 * val3MulCount);
                        }

                        const double M2[] = {
                            static_cast<double>(sumSquare0 * totalCount -
                                                static_cast<int64_t>(sum0) *
                                                    sum0),
                            static_cast<double>(sumSquare1 * totalCount -
                                                static_cast<int64_t>(sum1) *
                                                    sum1),
                            static_cast<double>(sumSquare2 * totalCount -
                                                static_cast<int64_t>(sum2) *
                                                    sum2),
                            static_cast<double>(sumSquare3 * totalCount -
                                                static_cast<int64_t>(sum3) *
                                                    sum3)};
                        for (int i = 0; i < VALS_AT_ONCE; ++i)
                        {
                            if (M2[i] > maxM2)
                            {
                                maxM2 = M2[i];
                                maxM2_k = k + i;
                            }
                        }
                        k += VALS_AT_ONCE - 1;
                    }
                }
                else
#endif
                {
                    int sum = 0;
                    int64_t sumSquare = 0;
                    for (const auto &item : vectors)
                    {
                        const int val = item.m_vec.get(k, ctxt);
                        const int valMulCount = val * item.m_count;
                        sum += valMulCount;
                        // It's fine to cast to int64 after multiplication
                        sumSquare += cpl::fits_on<int64_t>(val * valMulCount);
                    }
                    const double M2 =
                        static_cast<double>(sumSquare * totalCount -
                                            static_cast<int64_t>(sum) * sum);
                    if (M2 > maxM2)
                    {
                        maxM2 = M2;
                        maxM2_k = k;
                    }
                }
                continue;
            }
        }

        // Generic code path:

        // First pass to compute mean value along k(th) dimension
        double sum = 0;
        for (const auto &item : vectors)
        {
            sum += static_cast<double>(item.m_vec.get(k, ctxt)) * item.m_count;
        }
        const double mean = sum / totalCount;
        // Second pass to compute M2 value (n * variance) along k(th) dimension
        double M2 = 0;
        for (const auto &item : vectors)
        {
            const double delta =
                static_cast<double>(item.m_vec.get(k, ctxt)) - mean;
            M2 += delta * delta * item.m_count;
        }
        if (M2 > maxM2)
        {
            maxM2 = M2;
            maxM2_k = k;
        }
    }

#ifdef KDTREE_DEBUG_TIMING
    gettimeofday(&tv2, nullptr);
    totalTimeStats +=
        (tv2.tv_sec + tv2.tv_usec * 1e-6) - (tv1.tv_sec + tv1.tv_usec * 1e-6);
#endif

    // Find median value along that dimension
    weightedVals.reserve(vectors.size());
    weightedVals.clear();
    for (const auto &item : vectors)
    {
        const auto d = item.m_vec.get(maxM2_k, ctxt);
        weightedVals.emplace_back(d, item.m_count);
    }

    std::sort(weightedVals.begin(), weightedVals.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    auto median = weightedVals[0].first;
    int cumulativeCount = 0;
    const int targetCount = totalCount / 2;
    for (const auto &[value, count] : weightedVals)
    {
        cumulativeCount += count;
        if (cumulativeCount > targetCount)
        {
            median = value;
            break;
        }
    }

    // Split the original vectors in a "left" half that is below or equal to
    // the median and a "right" half that is above.
    vectLeft.clear();
    vectLeft.reserve(weightedVals.size() / 2);
    vectRight.clear();
    vectRight.reserve(weightedVals.size() / 2);
    int countLeft = 0;
    int countRight = 0;
    for (auto &item : vectors)
    {
        if (item.m_vec.get(maxM2_k, ctxt) <= median)
        {
            countLeft += item.m_count;
            vectLeft.push_back(std::move(item));
        }
        else
        {
            countRight += item.m_count;
            vectRight.push_back(std::move(item));
        }
    }

    // In some cases, the median can actually be the maximum value
    // Then, retry but excluding the median itself.
    if (vectLeft.empty() || vectRight.empty())
    {
        if (!vectLeft.empty())
            vectors = std::move(vectLeft);
        else
            vectors = std::move(vectRight);
        vectLeft.clear();
        vectRight.clear();
        countLeft = 0;
        countRight = 0;
        for (auto &item : vectors)
        {
            if (item.m_vec.get(maxM2_k, ctxt) < median)
            {
                countLeft += item.m_count;
                vectLeft.push_back(std::move(item));
            }
            else
            {
                countRight += item.m_count;
                vectRight.push_back(std::move(item));
            }
        }

        // Normally we shouldn't reach that point, unless the initial samples
        // where all identical, and thus clustering wasn't needed.
        if (vectLeft.empty() || vectRight.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected situation in %s:%d\n", __FILE__, __LINE__);
            return 0;
        }
    }
    vectors.clear();

    // Allocate (or recycle) left and right nodes
    if (!queueNodes.empty())
    {
        m_left = std::move(queueNodes.back());
        queueNodes.pop_back();
    }
    else
        m_left = std::make_unique<PNNKDTree<T>>();

    if (!queueNodes.empty())
    {
        m_right = std::move(queueNodes.back());
        queueNodes.pop_back();
    }
    else
        m_right = std::make_unique<PNNKDTree<T>>();

    // Recursively insert vectLeft in m_left and vectRight in m_right
    std::vector<BucketItem<T>> vectTmp;
    // Sort for replicability of results across platforms
    const auto sortFunc = [](const BucketItem<T> &a, const BucketItem<T> &b)
    { return a.m_vec < b.m_vec; };
    std::sort(vectLeft.begin(), vectLeft.end(), sortFunc);
    std::sort(vectRight.begin(), vectRight.end(), sortFunc);
    int retLeft = m_left->insert(std::move(vectLeft), countLeft, weightedVals,
                                 queueNodes, vectors, vectTmp, ctxt);
    int retRight =
        m_right->insert(std::move(vectRight), countRight, weightedVals,
                        queueNodes, vectors, vectTmp, ctxt);
    vectLeft = std::vector<BucketItem<T>>();
    vectRight = std::vector<BucketItem<T>>();
    return (retLeft == 0 || retRight == 0) ? 0 : retLeft + retRight;
}

/************************************************************************/
/*                  PNNKDTree<T>::iterateOverLeaves()                   */
/************************************************************************/

template <class T>
void PNNKDTree<T>::iterateOverLeaves(const std::function<void(PNNKDTree &)> &f)
{
    if (m_left && m_right)
    {
        m_left->iterateOverLeaves(f);
        m_right->iterateOverLeaves(f);
    }
    else
    {
        f(*this);
    }
}

/************************************************************************/
/*                       PNNKDTree<T>::cluster()                        */
/************************************************************************/

template <class T>
int PNNKDTree<T>::cluster(int initialBucketCount, int targetCount,
                          const T &ctxt)
{
    int curBucketCount = initialBucketCount;

    std::vector<BucketItem<T>> newLeaves;
    newLeaves.reserve(initialBucketCount);
    std::deque<std::unique_ptr<PNNKDTree>> queueNodes;

    struct TupleInfo
    {
        PNNKDTree *bucket;
        int i;
        int j;
        double increasedDistortion;
    };

    std::vector<TupleInfo> distCollector;
    distCollector.reserve(curBucketCount);

    int iter = 0;
#ifdef DEBUG_INVARIANTS
    std::map<Vector<T>, int> mapValuesToBucketIdx;
#endif
    while (curBucketCount > targetCount)
    {
        /* For each bucket (leaf node), compute the increase in distortion
         * that would result in merging each (i,j) vector it contains.
         */
        distCollector.clear();
        iterateOverLeaves(
            [&distCollector, &ctxt](PNNKDTree &bucket)
            {
                const int itemsCount =
                    static_cast<int>(bucket.m_bucketItems.size());
                for (int i = 0; i < itemsCount - 1; ++i)
                {
                    const auto &itemI = bucket.m_bucketItems[i];
                    int j = i + 1;
                    if constexpr (Vector<T>::hasComputeFourSquaredDistances)
                    {
                        constexpr int CHUNK_SIZE = 4;
                        if (j + CHUNK_SIZE <= itemsCount)
                        {
                            std::array<const Vector<T> *const, CHUNK_SIZE>
                                otherVectors = {
                                    &(bucket.m_bucketItems[j + 0].m_vec),
                                    &(bucket.m_bucketItems[j + 1].m_vec),
                                    &(bucket.m_bucketItems[j + 2].m_vec),
                                    &(bucket.m_bucketItems[j + 3].m_vec)};
                            std::array<int, CHUNK_SIZE> tabSquaredDist;
                            itemI.m_vec.compute_four_squared_distances(
                                otherVectors, tabSquaredDist, ctxt);
                            for (int subj = 0; subj < CHUNK_SIZE; ++subj)
                            {
                                const auto &itemJ =
                                    bucket.m_bucketItems[j + subj];
                                const double increasedDistortion =
                                    static_cast<double>(itemI.m_count) *
                                    itemJ.m_count * tabSquaredDist[subj] /
                                    (itemI.m_count + itemJ.m_count);
                                TupleInfo ti;
                                ti.bucket = &bucket;
                                ti.i = i;
                                ti.j = j + subj;
                                ti.increasedDistortion = increasedDistortion;
                                distCollector.push_back(std::move(ti));
                            }

                            j += CHUNK_SIZE;
                        }
                    }
                    for (; j < itemsCount; ++j)
                    {
                        const auto &itemJ = bucket.m_bucketItems[j];
                        const double increasedDistortion =
                            static_cast<double>(itemI.m_count) * itemJ.m_count *
                            itemI.m_vec.squared_distance(itemJ.m_vec, ctxt) /
                            (itemI.m_count + itemJ.m_count);
                        TupleInfo ti;
                        ti.bucket = &bucket;
                        ti.i = i;
                        ti.j = j;
                        ti.increasedDistortion = increasedDistortion;
                        distCollector.push_back(std::move(ti));
                    }
                }
            });

        /** Identify the median of the increased distortion */
        const int bucketCountToMerge =
            std::min(static_cast<int>(distCollector.size() / 2),
                     curBucketCount - targetCount);
        const auto sortFunc = [](const TupleInfo &a, const TupleInfo &b)
        {
            return a.increasedDistortion < b.increasedDistortion ||
                   (a.increasedDistortion == b.increasedDistortion &&
                    (a.bucket->m_bucketItems[0].m_vec <
                         b.bucket->m_bucketItems[0].m_vec ||
                     (a.bucket->m_bucketItems[0].m_vec ==
                          b.bucket->m_bucketItems[0].m_vec &&
                      (a.i < b.i || (a.i == b.i && a.j < b.j)))));
        };
        const auto median_iter = distCollector.begin() + bucketCountToMerge;
        std::nth_element(distCollector.begin(), median_iter,
                         distCollector.end(), sortFunc);
        /** Sort elements by increasing increasedDistortion, but only for
         * the first half of the array
         */
        std::sort(distCollector.begin(), median_iter, sortFunc);

        static_assert(BUCKET_MAX_SIZE <= sizeof(uint32_t) * 8);
        std::map<PNNKDTree *, uint32_t> invalidatedClusters;

        int expectedBucketCount = curBucketCount;

        // Merge all the tuple of vectors whose increased distortion is lower
        // than the median.
        for (auto oIterCollector = distCollector.begin();
             oIterCollector != median_iter; ++oIterCollector)
        {
            const auto &tupleInfo = *oIterCollector;
            // assert( tupleInfo.increasedDistortion <= median_iter->increasedDistortion );
            auto oIter = invalidatedClusters.find(tupleInfo.bucket);
            if (oIter != invalidatedClusters.end())
            {
                // Be careful not to merge a (i,j) tuple whose at least one of
                // the element has already been merged.
                // (this aspect is not covered by the Equitz's paper)
                if ((oIter->second &
                     ((1U << tupleInfo.i) | (1U << tupleInfo.j))) != 0)
                {
                    continue;
                }
            }
            else
            {
                oIter =
                    invalidatedClusters.insert(std::pair(tupleInfo.bucket, 0))
                        .first;
            }

            auto &bucketItemI = tupleInfo.bucket->m_bucketItems[tupleInfo.i];
            const auto &bucketItemJ =
                tupleInfo.bucket->m_bucketItems[tupleInfo.j];
            auto origVectorIndices = std::move(bucketItemI.m_origVectorIndices);
            origVectorIndices.insert(origVectorIndices.end(),
                                     bucketItemJ.m_origVectorIndices.begin(),
                                     bucketItemJ.m_origVectorIndices.end());

#ifdef KDTREE_DEBUG_TIMING
            struct timeval tv1, tv2;
            gettimeofday(&tv1, nullptr);
#endif
            auto newVal = Vector<T>::centroid(
                bucketItemI.m_vec, bucketItemI.m_count, bucketItemJ.m_vec,
                bucketItemJ.m_count, ctxt);

#ifdef KDTREE_DEBUG_TIMING
            gettimeofday(&tv2, nullptr);
            totalTimeCentroid += (tv2.tv_sec + tv2.tv_usec * 1e-6) -
                                 (tv1.tv_sec + tv1.tv_usec * 1e-6);
#endif

            // Look if there is an existing item in the bucket with the new
            // vector value
            int bucketItemIdx = -1;
            for (int i = 0;
                 i < static_cast<int>(tupleInfo.bucket->m_bucketItems.size());
                 ++i)
            {
                if ((oIter->second & (1U << i)) == 0 &&
                    tupleInfo.bucket->m_bucketItems[i].m_vec == newVal)
                {
                    bucketItemIdx = i;
                    break;
                }
            }
            oIter->second |= ((1U << tupleInfo.i) | (1U << tupleInfo.j));
            int newCount = bucketItemI.m_count + bucketItemJ.m_count;
            if (bucketItemIdx >= 0 && bucketItemIdx != tupleInfo.i &&
                bucketItemIdx != tupleInfo.j)
            {
                oIter->second |= ((1U << bucketItemIdx));
                auto &existingItem =
                    tupleInfo.bucket->m_bucketItems[bucketItemIdx];
                newCount += existingItem.m_count;
                origVectorIndices.insert(
                    origVectorIndices.end(),
                    std::make_move_iterator(
                        existingItem.m_origVectorIndices.begin()),
                    std::make_move_iterator(
                        existingItem.m_origVectorIndices.end()));
            }
            // Insert the new bucket item
            tupleInfo.bucket->m_bucketItems.emplace_back(
                std::move(newVal), newCount, std::move(origVectorIndices));

            --expectedBucketCount;
        }

        // Remove items that have been merged
        for (auto [node, indices] : invalidatedClusters)
        {
            // Inside a same bucket, be careful to remove from the end so that
            // noted indices are still valid...
            for (int i = static_cast<int>(node->m_bucketItems.size()) - 1;
                 i >= 0; --i)
            {
                if ((indices & (1U << i)) != 0)
                {
                    node->m_bucketItems.erase(node->m_bucketItems.begin() + i);
                }
            }

#ifdef DEBUG_INVARIANTS
            mapValuesToBucketIdx.clear();
            for (int i = 0; i < static_cast<int>(node->m_bucketItems.size());
                 ++i)
            {
                CPLAssert(
                    mapValuesToBucketIdx.find(node->m_bucketItems[i].m_vec) ==
                    mapValuesToBucketIdx.end());
                mapValuesToBucketIdx[node->m_bucketItems[i].m_vec] = i;
            }
#endif
        }

        // Rebalance the tree only half of the time, to speed up things a bit
        // This is quite arbitrary. Systematic rebalancing could result in
        // slightly better results.
        if ((iter % 2) == 0)
            curBucketCount = expectedBucketCount;
        else
            curBucketCount = rebalance(ctxt, newLeaves, queueNodes);
        ++iter;
    }

    return curBucketCount;
}

/************************************************************************/
/*                  PNNKDTree<T>::freeAndMoveToQueue()                  */
/************************************************************************/

template <class T>
void PNNKDTree<T>::freeAndMoveToQueue(
    std::deque<std::unique_ptr<PNNKDTree>> &queueNodes)
{
    m_bucketItems.clear();
    if (m_left)
    {
        m_left->freeAndMoveToQueue(queueNodes);
        queueNodes.push_back(std::move(m_left));
    }
    if (m_right)
    {
        m_right->freeAndMoveToQueue(queueNodes);
        queueNodes.push_back(std::move(m_right));
    }
}

/************************************************************************/
/*                      PNNKDTree<T>::rebalance()                       */
/************************************************************************/

template <class T>
int PNNKDTree<T>::rebalance(const T &ctxt,
                            std::vector<BucketItem<T>> &newLeaves,
                            std::deque<std::unique_ptr<PNNKDTree>> &queueNodes)
{
    if (m_left && m_right)
    {
#ifdef KDTREE_DEBUG_TIMING
        struct timeval tv1, tv2;
        gettimeofday(&tv1, nullptr);
#endif
        std::map<Vector<T>,
                 std::pair<int, std::vector<typename BucketItem<T>::IdxType>>>
            mapVectors;
        int totalCount = 0;
        // Rebuild a new map of vector values -> (count, indices)
        // This needs to be a map as we cannot guarantee the uniqueness
        // of vector values after the clustering pass
        iterateOverLeaves(
            [&mapVectors, &totalCount](PNNKDTree &bucket)
            {
                for (auto &item : bucket.m_bucketItems)
                {
                    totalCount += item.m_count;
                    auto oIter = mapVectors.find(item.m_vec);
                    if (oIter == mapVectors.end())
                    {
                        mapVectors[item.m_vec] = std::make_pair(
                            item.m_count, std::move(item.m_origVectorIndices));
                    }
                    else
                    {
                        oIter->second.first += item.m_count;
                        oIter->second.second.insert(
                            oIter->second.second.end(),
                            std::make_move_iterator(
                                item.m_origVectorIndices.begin()),
                            std::make_move_iterator(
                                item.m_origVectorIndices.end()));
                    }
                }
            });

        freeAndMoveToQueue(queueNodes);

        // Convert the map to an array
        newLeaves.clear();
        for (auto &[key, value] : mapVectors)
        {
            newLeaves.emplace_back(std::move(key), value.first,
                                   std::move(value.second));
        }

        std::vector<std::pair<ValType, int>> weightedVals;
        std::vector<BucketItem<T>> vectLeft;
        std::vector<BucketItem<T>> vectRight;
        const int ret = insert(std::move(newLeaves), totalCount, weightedVals,
                               queueNodes, vectLeft, vectRight, ctxt);
#ifdef KDTREE_DEBUG_TIMING
        gettimeofday(&tv2, nullptr);
        totalTimeRebalancing += (tv2.tv_sec + tv2.tv_usec * 1e-6) -
                                (tv1.tv_sec + tv1.tv_usec * 1e-6);
#endif
        newLeaves = std::vector<BucketItem<T>>();
        return ret;
    }
    else
    {
        return static_cast<int>(m_bucketItems.size());
    }
}

#endif  // KDTREE_INCLUDED
