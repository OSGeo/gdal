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
#include <memory>
#include <algorithm>

#include "cpl_quad_tree.h"

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
        OGREnvelope bbox;

        void computeBBox()
        {
            for (const auto &pt : points)
                bbox.Merge(pt.x, pt.y);
        }

        mutable std::vector<Ring> interiorRings;

        const Ring *closestExterior = nullptr;

        bool isIn(const Ring &other) const
        {
            // Check if this is inside other using the winding number algorithm
            auto checkPoint = this->points.front();
            // A point outside the candidate ring's bounding box
            // cannot be inside the ring.
            if (checkPoint.x < other.bbox.MinX ||
                checkPoint.x > other.bbox.MaxX ||
                checkPoint.y < other.bbox.MinY ||
                checkPoint.y > other.bbox.MaxY)
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

    // Point-in-polygon accelerator for one target ring. The capture step
    // tests MANY points against the SAME ring (pathological case: a
    // domain-spanning ring with millions of vertices capturing tens of
    // thousands of earlier rings) — bucketing the ring's segments by y makes
    // each test O(segments-in-bucket) instead of O(all vertices). GEOS has
    // the natural structure for this (IndexedPointInAreaLocator), but GEOS
    // is an optional dependency and contouring must work without it; GDAL
    // core has no indexed point-in-polygon type.
    struct RingPIPIndex
    {
        double yMin = 0, yMax = 0, inv = 0;
        std::vector<std::vector<std::pair<Point, Point>>> buckets;

        void build(const Ring &r)
        {
            yMin = r.bbox.MinY;
            yMax = r.bbox.MaxY;
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

    // Per-level spatial index over TOP-LEVEL rings: a CPLQuadTree over ring
    // bounding boxes. Stored features are slot indices into the level's ring
    // vector (encoded in the pointer value, never dereferenced), so vector
    // reallocation is harmless. Rings captured as interior rings of a later
    // ring are removed from the tree and their slot tombstoned (points
    // cleared) rather than erased, keeping the remaining indices stable.
    struct QuadTreeDestroyer
    {
        void operator()(CPLQuadTree *t) const
        {
            CPLQuadTreeDestroy(t);
        }
    };

    std::map<double, std::unique_ptr<CPLQuadTree, QuadTreeDestroyer>> index_;
    CPLRectObj domain_;

    static void *slotFeature(std::size_t idx)
    {
        return reinterpret_cast<void *>(static_cast<std::uintptr_t>(idx + 1));
    }

    static std::size_t featureSlot(void *f)
    {
        return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(f)) -
               1;
    }

    static CPLRectObj ringRect(const Ring &r)
    {
        return CPLRectObj{r.bbox.MinX, r.bbox.MinY, r.bbox.MaxX, r.bbox.MaxY};
    }

    PolygonWriter &writer_;

  public:
    const bool polygonize = true;

    PolygonRingAppender(PolygonWriter &writer, double minX, double minY,
                        double maxX, double maxY)
        : domain_{minX, minY, maxX, maxY}, writer_(writer)
    {
    }

    void addLine(double level, LineString &ls, bool)
    {
        auto &levelRings = rings_[level];
        auto &levelTree = index_[level];
        if (!levelTree)
            levelTree.reset(CPLQuadTreeCreate(&domain_, nullptr));
        if (ls.empty())
        {
            return;
        }
        // Create a new ring from the LineString
        Ring newRing;
        newRing.points.swap(ls);
        newRing.computeBBox();
        // Find the top-level parent (if any) through the index instead of
        // scanning every top-level ring, then descend the (short) nested
        // sibling lists exactly as before.
        Ring *parentRing = nullptr;
        {
            Ring *top = nullptr;
            const auto &fp0 = newRing.points.front();
            CPLRectObj aoi{fp0.x, fp0.y, fp0.x, fp0.y};
            int nHits = 0;
            void **hits = CPLQuadTreeSearch(levelTree.get(), &aoi, &nHits);
            for (int h = 0; h < nHits && top == nullptr; h++)
            {
                Ring &cand = levelRings[featureSlot(hits[h])];
                if (!cand.points.empty() && newRing.isIn(cand))
                    top = &cand;
            }
            CPLFree(hits);
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
            std::vector<std::size_t> captured;
            RingPIPIndex pip;
            bool pipBuilt = false;
            std::size_t nCandidates = 0;
            {
                CPLRectObj aoi = ringRect(newRing);
                int nHits = 0;
                void **hits = CPLQuadTreeSearch(levelTree.get(), &aoi, &nHits);
                for (int h = 0; h < nHits; h++)
                {
                    const std::size_t idx = featureSlot(hits[h]);
                    Ring &cand = levelRings[idx];
                    if (cand.points.empty())
                        continue;
                    const auto &fp = cand.points.front();
                    if (fp.x < newRing.bbox.MinX || fp.x > newRing.bbox.MaxX ||
                        fp.y < newRing.bbox.MinY || fp.y > newRing.bbox.MaxY)
                        continue;
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
                }
                CPLFree(hits);
            }
            std::sort(captured.begin(), captured.end());
            captured.erase(std::unique(captured.begin(), captured.end()),
                           captured.end());
            for (std::size_t idx : captured)
            {
                CPLRectObj rb = ringRect(levelRings[idx]);
                CPLQuadTreeRemove(levelTree.get(), slotFeature(idx), &rb);
                newRing.interiorRings.push_back(std::move(levelRings[idx]));
                levelRings[idx].points.clear();  // tombstone the slot
            }
            levelRings.push_back(std::move(newRing));
            CPLRectObj nb = ringRect(levelRings.back());
            CPLQuadTreeInsertWithBounds(
                levelTree.get(), slotFeature(levelRings.size() - 1), &nb);
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
