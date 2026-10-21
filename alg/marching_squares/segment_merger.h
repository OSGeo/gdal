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
#ifndef MARCHING_SQUARES_SEGMENT_MERGER_H
#define MARCHING_SQUARES_SEGMENT_MERGER_H

#include "cpl_error.h"
#include "point.h"

#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <vector>

#include <iostream>

namespace marching_squares
{

// SegmentMerger: join segments into linestrings and possibly into rings of
// polygons
template <typename LineWriter, typename LevelGenerator> struct SegmentMerger
{
    struct LineStringEx
    {
        LineString ls = LineString();
        bool isMerged = false;
    };

    // a collection of unmerged linestrings
    typedef std::list<LineStringEx> Lines;

    SegmentMerger(LineWriter &lineWriter, const LevelGenerator &levelGenerator,
                  bool polygonize_)
        : polygonize(polygonize_), lineWriter_(lineWriter), lines_(),
          endpointIndex_(), levelGenerator_(levelGenerator), m_anSkipLevels()
    {
    }

    ~SegmentMerger()
    {
        if (polygonize)
        {
            for (auto it = lines_.begin(); it != lines_.end(); ++it)
            {
                if (!it->second.empty())
                    debug("remaining unclosed contour");
            }
        }
        // write all remaining (non-closed) lines
        for (auto it = lines_.begin(); it != lines_.end(); ++it)
        {
            const int levelIdx = it->first;

            // Skip levels that should be skipped
            if (std::find(m_anSkipLevels.begin(), m_anSkipLevels.end(),
                          levelIdx) != m_anSkipLevels.end())
            {
                continue;
            }
            while (it->second.begin() != it->second.end())
            {
                lineWriter_.addLine(levelGenerator_.level(levelIdx),
                                    it->second.begin()->ls, /* closed */ false);
                it->second.pop_front();
            }
        }
    }

    void addSegment(int levelIdx, const Point &start, const Point &end)
    {
        addSegment_(levelIdx, start, end);
    }

    void addBorderSegment(int levelIdx, const Point &start, const Point &end)
    {
        addSegment_(levelIdx, start, end);
    }

    void beginningOfLine()
    {
        if (polygonize)
            return;

        // mark lines as non merged
        for (auto &l : lines_)
        {
            for (auto &ls : l.second)
            {
                ls.isMerged = false;
            }
        }
    }

    void endOfLine()
    {
        if (polygonize)
            return;

        // At the end of the line, we know that if no segment has been merged to
        // an existing line, it means there won't be anything more in the
        // future, we can then emit the line (this both speeds up and saves
        // memory)

        for (auto &l : lines_)
        {
            const int levelIdx = l.first;
            auto it = l.second.begin();
            while (it != l.second.end())
            {
                if (!it->isMerged)
                {
                    // Note that emitLine_ erases `it` and returns an iterator
                    // advanced to the next element.
                    it = emitLine_(levelIdx, it, /* closed */ false);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // non copyable
    SegmentMerger(const SegmentMerger<LineWriter, LevelGenerator> &) = delete;
    SegmentMerger<LineWriter, LevelGenerator> &
    operator=(const SegmentMerger<LineWriter, LevelGenerator> &) = delete;

    /**
     * @brief setSkipLevels sets the levels that should be skipped
     *        when polygonize option is set.
     * @param anSkipLevels integer 0-based levels to skip.
     */
    void setSkipLevels(const std::vector<int> &anSkipLevels)
    {
        // Warn if polygonize is not set
        if (!polygonize)
        {
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "setSkipLevels is ignored when polygonize option is not set");
        }
        m_anSkipLevels = anSkipLevels;
    }

    const bool polygonize;

  private:
    LineWriter &lineWriter_;
    // lines of each level
    std::map<int, Lines> lines_;

    // Index of the open lines of each level, keyed by their two endpoints.
    // Marching squares emits segments whose endpoints have degree two, so a
    // point can only ever be shared by two segment ends; a lookup here
    // replaces a linear scan over all open lines, which is quadratic on
    // rasters that keep many lines open at once (e.g. many long parallel
    // contours crossing each scanline).
    struct PointCompare
    {
        bool operator()(const Point &a, const Point &b) const
        {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        }
    };

    // A marching squares point has degree two, so at most two line ends can
    // sit on it (the two ends of the same line, when it has closed): a fixed
    // two-slot entry avoids a heap allocation per endpoint.
    struct EndpointEntry
    {
        typename Lines::iterator its[2];
        int n = 0;
    };

    typedef std::map<Point, EndpointEntry, PointCompare> EndpointIndex;
    std::map<int, EndpointIndex> endpointIndex_;

    const LevelGenerator &levelGenerator_;

    // Store 0-indexed levels to skip when polygonize option is set
    std::vector<int> m_anSkipLevels;

    void registerEndpoint_(EndpointIndex &idx, typename Lines::iterator it,
                           const Point &p)
    {
        EndpointEntry &e = idx[p];
        assert(e.n < 2);
        e.its[e.n++] = it;
    }

    void unregisterEndpoint_(EndpointIndex &idx, typename Lines::iterator it,
                             const Point &p)
    {
        auto f = idx.find(p);
        assert(f != idx.end());
        EndpointEntry &e = f->second;
        if (e.its[0] == it)
            e.its[0] = e.its[1];
        else
            assert(e.n == 2 && e.its[1] == it);
        e.n--;
        if (e.n == 0)
            idx.erase(f);
    }

    void addSegment_(int levelIdx, const Point &start, const Point &end)
    {

        Lines &lines = lines_[levelIdx];

        if (start == end)
        {
            debug("degenerate segment (%f %f)", start.x, start.y);
            return;
        }

        auto idxIt = endpointIndex_.find(levelIdx);
        if (idxIt == endpointIndex_.end())
        {
            addSegmentLinear_(levelIdx, lines, start, end);
            // The linear scans above are quadratic when many lines stay open
            // at once (e.g. long parallel contours crossing each scanline):
            // past this size, switch the level to the endpoint index. Below
            // it, the scans are cheaper than maintaining the index.
            constexpr std::size_t INDEX_THRESHOLD = 100;
            if (lines.size() > INDEX_THRESHOLD)
            {
                EndpointIndex &idx = endpointIndex_[levelIdx];
                for (auto it = lines.begin(); it != lines.end(); ++it)
                {
                    registerEndpoint_(idx, it, it->ls.front());
                    registerEndpoint_(idx, it, it->ls.back());
                }
            }
        }
        else
        {
            addSegmentIndexed_(levelIdx, lines, idxIt->second, start, end);
        }
    }

    // Merge the segment by scanning the open lines: cheap while there are
    // few of them, quadratic when there are many.
    void addSegmentLinear_(int levelIdx, Lines &lines, const Point &start,
                           const Point &end)
    {
        // attempt to merge segment with existing line
        auto it = lines.begin();
        for (; it != lines.end(); ++it)
        {
            if (it->ls.back() == end)
            {
                it->ls.push_back(start);
                it->isMerged = true;
                break;
            }
            if (it->ls.front() == end)
            {
                it->ls.push_front(start);
                it->isMerged = true;
                break;
            }
            if (it->ls.back() == start)
            {
                it->ls.push_back(end);
                it->isMerged = true;
                break;
            }
            if (it->ls.front() == start)
            {
                it->ls.push_front(end);
                it->isMerged = true;
                break;
            }
        }

        if (it == lines.end())
        {
            // new line
            lines.push_back(LineStringEx());
            lines.back().ls.push_back(start);
            lines.back().ls.push_back(end);
            lines.back().isMerged = true;
        }
        else if (polygonize && (it->ls.front() == it->ls.back()))
        {
            // ring closed
            emitLine_(levelIdx, it, /* closed */ true);
            return;
        }
        else
        {
            // try to perform linemerge with another line
            // since we got out of the previous loop on the first match
            // there is no need to test previous elements
            // also: a segment merges at most two lines, no need to stall here
            // ;)
            auto other = it;
            ++other;
            for (; other != lines.end(); ++other)
            {
                if (it->ls.back() == other->ls.front())
                {
                    it->ls.pop_back();
                    it->ls.splice(it->ls.end(), other->ls);
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if (it->ls.front() == it->ls.back())
                        emitLine_(levelIdx, it, /* closed */ true);
                    break;
                }
                else if (other->ls.back() == it->ls.front())
                {
                    it->ls.pop_front();
                    other->ls.splice(other->ls.end(), it->ls);
                    other->isMerged = true;
                    lines.erase(it);
                    // if that makes a closed ring, returns it
                    if (other->ls.front() == other->ls.back())
                        emitLine_(levelIdx, other, /* closed */ true);
                    break;
                }
                // two lists must be merged but one is in the opposite direction
                else if (it->ls.back() == other->ls.back())
                {
                    it->ls.pop_back();
                    for (auto rit = other->ls.rbegin(); rit != other->ls.rend();
                         ++rit)
                    {
                        it->ls.push_back(*rit);
                    }
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if (it->ls.front() == it->ls.back())
                        emitLine_(levelIdx, it, /* closed */ true);
                    break;
                }
                else if (it->ls.front() == other->ls.front())
                {
                    it->ls.pop_front();
                    for (auto rit = other->ls.begin(); rit != other->ls.end();
                         ++rit)
                    {
                        it->ls.push_front(*rit);
                    }
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if (it->ls.front() == it->ls.back())
                        emitLine_(levelIdx, it, /* closed */ true);
                    break;
                }
            }
        }
    }

    // Merge the segment through the endpoint index. Same merging rules as
    // the linear scan (a marching squares point has degree two, so at most
    // one line can match each of the segment's endpoints), but O(log n)
    // lookups instead of O(n) scans.
    void addSegmentIndexed_(int levelIdx, Lines &lines, EndpointIndex &idx,
                            const Point &start, const Point &end)
    {
        // attempt to merge the segment with an existing line whose endpoint
        // matches one of the segment's endpoints
        auto findLine = [&](const Point &p)
        {
            auto f = idx.find(p);
            return f == idx.end() ? lines.end() : f->second.its[0];
        };

        Point matched = end;
        Point added = start;
        typename Lines::iterator it = findLine(end);
        if (it == lines.end())
        {
            matched = start;
            added = end;
            it = findLine(start);
        }

        if (it == lines.end())
        {
            // new line
            lines.push_back(LineStringEx());
            it = std::prev(lines.end());
            it->ls.push_back(start);
            it->ls.push_back(end);
            it->isMerged = true;
            registerEndpoint_(idx, it, start);
            registerEndpoint_(idx, it, end);
            return;
        }

        // extend the matched line with the segment's other endpoint
        unregisterEndpoint_(idx, it, matched);
        if (it->ls.back() == matched)
            it->ls.push_back(added);
        else
            it->ls.push_front(added);
        it->isMerged = true;
        registerEndpoint_(idx, it, added);

        // The extension may close a ring, or bring the line's new endpoint
        // onto another line's endpoint; merging two lines moves the free
        // endpoint again, so iterate until neither applies.
        for (;;)
        {
            if (polygonize && it->ls.front() == it->ls.back())
            {
                // ring closed
                emitLine_(levelIdx, it, /* closed */ true);
                return;
            }
            // is there another line ending at `added`?
            auto f = idx.find(added);
            assert(f != idx.end());
            typename Lines::iterator other = lines.end();
            for (int i = 0; i < f->second.n; i++)
            {
                if (f->second.its[i] != it)
                {
                    other = f->second.its[i];
                    break;
                }
            }
            if (other == lines.end())
                return;

            // merge `other` into `it` at the shared point `added`
            unregisterEndpoint_(idx, it, added);
            unregisterEndpoint_(idx, other, added);
            const Point otherEnd = other->ls.front() == added
                                       ? other->ls.back()
                                       : other->ls.front();
            unregisterEndpoint_(idx, other, otherEnd);
            if (it->ls.back() == added)
            {
                it->ls.pop_back();
                if (other->ls.front() == added)
                {
                    it->ls.splice(it->ls.end(), other->ls);
                }
                else
                {
                    // opposite direction: append reversed
                    for (auto rit = other->ls.rbegin(); rit != other->ls.rend();
                         ++rit)
                    {
                        it->ls.push_back(*rit);
                    }
                }
            }
            else
            {
                it->ls.pop_front();
                if (other->ls.back() == added)
                {
                    it->ls.splice(it->ls.begin(), other->ls);
                }
                else
                {
                    // opposite direction: prepend reversed
                    for (auto rit = other->ls.begin(); rit != other->ls.end();
                         ++rit)
                    {
                        it->ls.push_front(*rit);
                    }
                }
            }
            lines.erase(other);
            registerEndpoint_(idx, it, otherEnd);
            added = otherEnd;
        }
    }

    typename Lines::iterator emitLine_(int levelIdx,
                                       typename Lines::iterator it, bool closed)
    {

        Lines &lines = lines_[levelIdx];
        if (lines.empty())
            lines_.erase(levelIdx);

        auto idxIt = endpointIndex_.find(levelIdx);
        if (idxIt != endpointIndex_.end())
        {
            unregisterEndpoint_(idxIt->second, it, it->ls.front());
            unregisterEndpoint_(idxIt->second, it, it->ls.back());
            // An emptied index means no line is open at this level anymore:
            // drop it so the level returns to the linear path until it grows
            // past the threshold again.
            if (idxIt->second.empty())
                endpointIndex_.erase(idxIt);
        }

        // consume "it" and remove it from the list
        // but clear the line if the level should be skipped
        if (std::find(m_anSkipLevels.begin(), m_anSkipLevels.end(), levelIdx) !=
            m_anSkipLevels.end())
        {
            it->ls.clear();
        }
        lineWriter_.addLine(levelGenerator_.level(levelIdx), it->ls, closed);
        return lines.erase(it);
    }
};

}  // namespace marching_squares
#endif
