/******************************************************************************
 *
 * Project:  Marching square algorithm
 * Purpose:  Core algorithm implementation for contour line generation.
 * Author:   Oslandia <infos at oslandia dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef MARCHING_SQUARE_POLYGON_RING_APPENDER_H
#define MARCHING_SQUARE_POLYGON_RING_APPENDER_H

#include <vector>
#include <list>
#include <map>
#include <deque>
#include <cassert>
#include <iterator>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <algorithm>

#include "point.h"
#include "ogr_geometry.h"

namespace marching_squares
{

// Receive rings of different levels and organize them
// into multi-polygons with possible interior rings when requested.
template <typename PolygonWriter> class PolygonRingAppender
{
  private:
    struct Ring
    {
        Ring() : points(), interiorRings()
        {
        }

        Ring(const Ring &other) = default;
        Ring &operator=(const Ring &other) = default;
        // Declaring the copy operations above suppresses the
        // implicit move operations, so vector reshuffles and reallocations
        // deep-copied entire ring subtrees. Restore them.
        Ring(Ring &&other) = default;
        Ring &operator=(Ring &&other) = default;

        LineString points;

        // Bounding box, computed once when the ring is complete;
        // gives isIn() an O(1) reject so parent search stops being
        // O(rings * vertices) per insertion.
        double bbXMin = 0, bbYMin = 0, bbXMax = 0, bbYMax = 0;

        void computeBBox()
        {
            bool first = true;
            for (const auto &pt : points)
            {
                if (first)
                {
                    bbXMin = bbXMax = pt.x;
                    bbYMin = bbYMax = pt.y;
                    first = false;
                    continue;
                }
                if (pt.x < bbXMin)
                    bbXMin = pt.x;
                if (pt.x > bbXMax)
                    bbXMax = pt.x;
                if (pt.y < bbYMin)
                    bbYMin = pt.y;
                if (pt.y > bbYMax)
                    bbYMax = pt.y;
            }
        }

        mutable std::vector<Ring> interiorRings;

        const Ring *closestExterior = nullptr;

        bool isIn(const Ring &other) const
        {
            // Check if this is inside other using the winding number algorithm
            auto checkPoint = this->points.front();
            // A point outside the candidate ring's bounding box
            // cannot be inside the ring.
            if (checkPoint.x < other.bbXMin || checkPoint.x > other.bbXMax ||
                checkPoint.y < other.bbYMin || checkPoint.y > other.bbYMax)
            {
                return false;
            }
            int windingNum = 0;
            auto otherIter = other.points.begin();
            // p1 and p2 define each segment of the ring other that will be
            // tested
            auto p1 = *otherIter;
            while (true)
            {
                otherIter++;
                if (otherIter == other.points.end())
                {
                    break;
                }
                auto p2 = *otherIter;
                if (p1.y <= checkPoint.y)
                {
                    if (p2.y > checkPoint.y)
                    {
                        if (isLeft(p1, p2, checkPoint))
                        {
                            ++windingNum;
                        }
                    }
                }
                else
                {
                    if (p2.y <= checkPoint.y)
                    {
                        if (!isLeft(p1, p2, checkPoint))
                        {
                            --windingNum;
                        }
                    }
                }
                p1 = p2;
            }
            return windingNum != 0;
        }

#ifdef DEBUG
        size_t id() const
        {
            return size_t(static_cast<const void *>(this)) & 0xffff;
        }

        void print(std::ostream &ostr) const
        {
            ostr << id() << ":";
            for (const auto &pt : points)
            {
                ostr << pt.x << "," << pt.y << " ";
            }
        }
#endif
    };

    void processTree(const std::vector<Ring> &tree, int level)
    {
        if (level % 2 == 0)
        {
            for (auto &r : tree)
            {
                writer_.addPart(r.points);
                for (auto &innerRing : r.interiorRings)
                {
                    writer_.addInteriorRing(innerRing.points);
                }
            }
        }
        for (auto &r : tree)
        {
            processTree(r.interiorRings, level + 1);
        }
    }

    // level -> rings
    std::map<double, std::vector<Ring>> rings_;

    // Per-level spatial index over TOP-LEVEL rings. Maps grid
    // cell -> slot indices into the level's ring vector. Slots whose ring has
    // been captured as an interior ring are tombstoned (empty points), never
    // erased, so indices remain stable. Rings overlapping too many cells go
    // to a small linear overflow list.
    // Point-in-polygon accelerator for one target ring. The
    // capture step tests MANY points against the SAME ring (pathological
    // case: a domain-spanning ring with millions of vertices capturing tens
    // of thousands of earlier rings) — bucketing the ring's segments by y
    // makes each test O(segments-in-bucket) instead of O(all vertices).
    struct RingPIPIndex
    {
        double yMin = 0, yMax = 0, inv = 0;
        std::vector<std::vector<std::pair<Point, Point>>> buckets;

        void build(const Ring &r)
        {
            yMin = r.bbYMin;
            yMax = r.bbYMax;
            const size_t nSeg = r.points.size();
            // A segment spanning dy is copied into about
            // dy * nBuckets / (yMax - yMin) + 1 buckets, so the total stored
            // copies is nSeg + sumSpan * nBuckets / (yMax - yMin). Cap
            // nBuckets so that total stays O(nSeg): rings whose segments keep
            // crossing most of the y-range (sawtooth or near-flat rings)
            // would otherwise blow up quadratically.
            double sumSpan = 0;
            {
                auto spanIt = r.points.begin();
                auto prev = *spanIt;
                for (++spanIt; spanIt != r.points.end(); ++spanIt)
                {
                    sumSpan += std::abs(spanIt->y - prev.y);
                    prev = *spanIt;
                }
            }
            size_t nBuckets =
                std::max<size_t>(64, std::min<size_t>(nSeg / 32, 65536));
            if (yMax > yMin && sumSpan > 0)
            {
                const double cap = 4.0 * nSeg * (yMax - yMin) / sumSpan;
                if (cap < static_cast<double>(nBuckets))
                    nBuckets = std::max<size_t>(1, static_cast<size_t>(cap));
            }
            else
            {
                // Degenerate flat ring: every segment would span all buckets.
                nBuckets = 1;
            }
            buckets.assign(nBuckets, {});
            inv = (yMax > yMin) ? nBuckets / (yMax - yMin) : 0;
            auto it = r.points.begin();
            auto p1 = *it;
            for (++it; it != r.points.end(); ++it)
            {
                auto p2 = *it;
                double lo = std::min(p1.y, p2.y), hi = std::max(p1.y, p2.y);
                size_t b0 = bucketOf(lo), b1 = bucketOf(hi);
                for (size_t b = b0; b <= b1; b++)
                    buckets[b].emplace_back(p1, p2);
                p1 = p2;
            }
        }

        size_t bucketOf(double y) const
        {
            if (y <= yMin)
                return 0;
            if (y >= yMax)
                return buckets.size() - 1;
            size_t b = static_cast<size_t>((y - yMin) * inv);
            return b >= buckets.size() ? buckets.size() - 1 : b;
        }

        // identical winding-number semantics to Ring::isIn
        bool contains(const Point &checkPoint) const
        {
            int windingNum = 0;
            for (const auto &seg : buckets[bucketOf(checkPoint.y)])
            {
                const auto &p1 = seg.first;
                const auto &p2 = seg.second;
                if (p1.y <= checkPoint.y)
                {
                    if (p2.y > checkPoint.y)
                    {
                        if (isLeft(p1, p2, checkPoint))
                            ++windingNum;
                    }
                }
                else
                {
                    if (p2.y <= checkPoint.y)
                    {
                        if (!isLeft(p1, p2, checkPoint))
                            --windingNum;
                    }
                }
            }
            return windingNum != 0;
        }
    };

    struct LevelIndex
    {
        // Ring coordinates are raster pixel units here (the georeferencing
        // transform is applied downstream by the writer), so a fixed cell a
        // few tens of pixels wide keeps small rings in O(1) cells while the
        // overflow list bounds domain-spanning rings.
        static constexpr double cell = 32.0;
        static constexpr size_t maxCells = 4096;

        struct CellHash
        {
            size_t operator()(const std::pair<long long, long long> &c) const
            {
                // Unsigned arithmetic: cells can be negative, and signed
                // overflow / shifting negative values is undefined behavior.
                const auto x = static_cast<std::uint64_t>(c.first);
                const auto y = static_cast<std::uint64_t>(c.second);
                return std::hash<std::uint64_t>()(x * 2654435761ULL ^
                                                  (y << 21 | y >> 43));
            }
        };

        std::unordered_map<std::pair<long long, long long>, std::vector<size_t>,
                           CellHash>
            cells;
        std::vector<size_t> large;

        static long long cellOf(double v)
        {
            return static_cast<long long>(std::floor(v / cell));
        }

        void insert(const Ring &r, size_t idx)
        {
            const long long x0 = cellOf(r.bbXMin), x1 = cellOf(r.bbXMax);
            const long long y0 = cellOf(r.bbYMin), y1 = cellOf(r.bbYMax);
            const size_t nCells = static_cast<size_t>(x1 - x0 + 1) *
                                  static_cast<size_t>(y1 - y0 + 1);
            if (nCells > maxCells)
            {
                large.push_back(idx);
                return;
            }
            for (long long cx = x0; cx <= x1; cx++)
                for (long long cy = y0; cy <= y1; cy++)
                    cells[{cx, cy}].push_back(idx);
        }

        // Collect candidate slots whose bbox may contain point (px, py).
        template <typename F> void forPoint(double px, double py, F f) const
        {
            auto it = cells.find({cellOf(px), cellOf(py)});
            if (it != cells.end())
                for (size_t idx : it->second)
                    f(idx);
            for (size_t idx : large)
                f(idx);
        }

        // Collect candidate slots whose bbox may intersect the given bbox.
        template <typename F>
        void forBox(double xmin, double ymin, double xmax, double ymax,
                    F f) const
        {
            const long long x0 = cellOf(xmin), x1 = cellOf(xmax);
            const long long y0 = cellOf(ymin), y1 = cellOf(ymax);
            if (static_cast<size_t>(x1 - x0 + 1) *
                    static_cast<size_t>(y1 - y0 + 1) >
                maxCells)
            {
                // huge query box: fall back to scanning every cell entry
                for (const auto &kv : cells)
                    for (size_t idx : kv.second)
                        f(idx);
            }
            else
            {
                for (long long cx = x0; cx <= x1; cx++)
                    for (long long cy = y0; cy <= y1; cy++)
                    {
                        auto it = cells.find({cx, cy});
                        if (it != cells.end())
                            for (size_t idx : it->second)
                                f(idx);
                    }
            }
            for (size_t idx : large)
                f(idx);
        }
    };

    std::map<double, LevelIndex> index_;

    PolygonWriter &writer_;

  public:
    const bool polygonize = true;

    PolygonRingAppender(PolygonWriter &writer) : rings_(), writer_(writer)
    {
    }

    void addLine(double level, LineString &ls, bool)
    {
        auto &levelRings = rings_[level];
        auto &levelIndex = index_[level];
        if (ls.empty())
        {
            return;
        }
        // Create a new ring from the LineString
        Ring newRing;
        newRing.points.swap(ls);
        newRing.computeBBox();
        // Find the top-level parent (if any) through the grid
        // index instead of scanning every top-level ring, then descend the
        // (short) nested sibling lists exactly as before.
        Ring *parentRing = nullptr;
        {
            Ring *top = nullptr;
            levelIndex.forPoint(
                newRing.points.front().x, newRing.points.front().y,
                [&](size_t idx)
                {
                    Ring &cand = levelRings[idx];
                    if (top == nullptr && !cand.points.empty() &&
                        newRing.isIn(cand))
                        top = &cand;
                });
            if (top != nullptr)
            {
                parentRing = top;
                // descend into nested rings (sibling lists are short)
                std::deque<Ring *> queue;
                std::transform(
                    top->interiorRings.begin(), top->interiorRings.end(),
                    std::back_inserter(queue), [](Ring &r) { return &r; });
                while (!queue.empty())
                {
                    Ring *curRing = queue.front();
                    queue.pop_front();
                    if (newRing.isIn(*curRing))
                    {
                        parentRing = curRing;
                        queue.clear();
                        std::transform(curRing->interiorRings.begin(),
                                       curRing->interiorRings.end(),
                                       std::back_inserter(queue),
                                       [](Ring &r) { return &r; });
                    }
                }
            }
        }
        if (parentRing == nullptr)
        {
            // Top-level insertion: capture existing top-level rings that lie
            // inside the new ring, via the index. Build a per-target PIP
            // index lazily so a huge ring capturing many candidates costs
            // O(V + R * V/B), not O(R * V).
            std::vector<size_t> captured;
            RingPIPIndex pip;
            bool pipBuilt = false;
            size_t nCandidates = 0;
            levelIndex.forBox(
                newRing.bbXMin, newRing.bbYMin, newRing.bbXMax, newRing.bbYMax,
                [&](size_t idx)
                {
                    Ring &cand = levelRings[idx];
                    if (cand.points.empty())
                        return;
                    const auto &fp = cand.points.front();
                    if (fp.x < newRing.bbXMin || fp.x > newRing.bbXMax ||
                        fp.y < newRing.bbYMin || fp.y > newRing.bbYMax)
                        return;
                    if (!pipBuilt && ++nCandidates > 16 &&
                        newRing.points.size() > 512)
                    {
                        pip.build(newRing);
                        pipBuilt = true;
                    }
                    const bool inside =
                        pipBuilt ? pip.contains(fp) : cand.isIn(newRing);
                    if (inside)
                        captured.push_back(idx);
                });
            std::sort(captured.begin(), captured.end());
            captured.erase(std::unique(captured.begin(), captured.end()),
                           captured.end());
            for (size_t idx : captured)
            {
                newRing.interiorRings.push_back(std::move(levelRings[idx]));
                levelRings[idx].points.clear();  // tombstone the slot
            }
            levelRings.push_back(std::move(newRing));
            levelIndex.insert(levelRings.back(), levelRings.size() - 1);
        }
        else
        {
            // Nested insertion: identical to the original algorithm on the
            // parent's (short) child list.
            std::vector<Ring> *parentRingList = &(parentRing->interiorRings);
            auto trueGroupIt = std::partition(
                parentRingList->begin(), parentRingList->end(),
                [&newRing](Ring &pRing) { return !pRing.isIn(newRing); });
            std::move(trueGroupIt, parentRingList->end(),
                      std::back_inserter(newRing.interiorRings));
            parentRingList->erase(trueGroupIt, parentRingList->end());
            parentRingList->push_back(std::move(newRing));
        }
    }

    ~PolygonRingAppender()
    {
        // If there's no rings, nothing to do here
        if (rings_.size() == 0)
            return;

        // Traverse tree of rings
        for (auto &r : rings_)
        {
            // Drop tombstoned slots (rings captured as interior
            // rings of later-arriving parents) before traversal.
            std::vector<Ring> live;
            live.reserve(r.second.size());
            for (auto &ring : r.second)
                if (!ring.points.empty())
                    live.push_back(std::move(ring));
            // For each level, create a multipolygon by traversing the tree of
            // rings and adding a part for every other level
            writer_.startPolygon(r.first);
            processTree(live, 0);
            writer_.endPolygon();
        }
    }
};

}  // namespace marching_squares

#endif
