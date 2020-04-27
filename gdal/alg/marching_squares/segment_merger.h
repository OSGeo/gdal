/******************************************************************************
 *
 * Project:  Marching square algorithm
 * Purpose:  Core algorithm implementation for contour line generation.
 * Author:   Oslandia <infos at oslandia dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
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
#ifndef MARCHING_SQUARES_SEGMENT_MERGER_H
#define MARCHING_SQUARES_SEGMENT_MERGER_H

#include "point.h"

#include <list>
#include <map>

#include <iostream>

namespace marching_squares {

// SegmentMerger: join segments into linestrings and possibly into rings of polygons
template <typename LineWriter, typename LevelGenerator>
struct SegmentMerger
{
    struct LineStringEx
    {
        LineString ls = LineString();
        bool isMerged = false;
    };
    // a collection of unmerged linestrings
    typedef std::list<LineStringEx> Lines;
    
    SegmentMerger( LineWriter& lineWriter, const LevelGenerator& levelGenerator, bool polygonize_ )
        : polygonize( polygonize_ )
        , lineWriter_( lineWriter )
        , lines_()
        , levelGenerator_(levelGenerator)
    {}

    ~SegmentMerger()
    {
        if ( polygonize )
        {
            for (auto it =lines_.begin(); it!=lines_.end(); ++it) {
                if ( ! it->second.empty() )
                    debug("remaining unclosed contour");
            }
        }
        // write all remaining (non-closed) lines
        for (auto it = lines_.begin(); it!=lines_.end(); ++it)
        {
            const int levelIdx = it->first;
            while (it->second.begin() != it->second.end())
            {
                lineWriter_.addLine( levelGenerator_.level( levelIdx ), it->second.begin()->ls, /* closed */ false );
                it->second.pop_front();
            }
        }
    }

    void addBorderSegment(int levelIdx, const Point &start, const Point &end)
    {
        addSegment_(levelIdx, start, end);
    }

    void addSegment(int levelIdx, const Point &start, const Point &end)
    {
        addSegment_(levelIdx, start, end);
    }

    void beginningOfLine()
    {
        if ( polygonize ) return;

        // mark lines as non merged
        for ( auto& l : lines_ ) {
            for ( auto& ls : l.second ) {
                ls.isMerged = false;
            }
        }
    }

    void endOfLine()
    {
        if ( polygonize ) return;

        // At the end of the line, we know that if no segment has been merged to an
        // existing line, it means there won't be anything more in the future,
        // we can then emit the line (this both speeds up and saves memory)

        for ( auto& l : lines_ ) {
            const int levelIdx = l.first;
            auto it = l.second.begin();
            while ( it != l.second.end() ) {
                if ( ! it->isMerged ) {
                    // Note that emitLine_ erases `it` and returns an iterator advanced
                    // to the next element.
                    it = emitLine_( levelIdx, it, /* closed */ false );
                }
                else {
                    ++it;
                }
            }
        }
    }

    // non copyable
    SegmentMerger( const SegmentMerger<LineWriter, LevelGenerator>& ) = delete;
    SegmentMerger<LineWriter, LevelGenerator>& operator=( const SegmentMerger<LineWriter, LevelGenerator>& ) = delete;

    const bool polygonize;
private:
    LineWriter &lineWriter_;
    // lines of each level
    std::map< int, Lines > lines_;
    const LevelGenerator &levelGenerator_;

    void addSegment_(int levelIdx, const Point &start, const Point &end)
    {
        Lines& lines = lines_[levelIdx];

        if (start == end)
        {
            debug("degenerate segment (%f %f)", start.x, start.y);
            return;
        }
        // attempt to merge segment with existing line
        auto it = lines.begin();
        for(; it != lines.end(); ++it)
        {
            if ( it->ls.back() == end ) {
                it->ls.push_back( start );
                it->isMerged = true;
                break;
            }
            if ( it->ls.front() == end ) {
                it->ls.push_front( start );
                it->isMerged = true;
                break;
            }
            if ( it->ls.back() == start ) {
                it->ls.push_back( end );
                it->isMerged = true;
                break;
            }
            if ( it->ls.front() == start ) {
                it->ls.push_front( end );
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
        else if ( polygonize && (it->ls.front() == it->ls.back()) ) {
            // ring closed
            emitLine_( levelIdx, it, /* closed */ true );
            return;
        }
        else
        {
            // try to perform linemerge with another line
            // since we got out of the previous loop on the first match
            // there is no need to test previous elements
            // also: a segment merges at most two lines, no need to stall here ;)
            auto other = it;
            ++other;
            for(; other != lines.end(); ++other)
            {
                if (it->ls.back() == other->ls.front())
                {
                    it->ls.pop_back();
                    it->ls.splice(it->ls.end(), other->ls);
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if ( it->ls.front() == it->ls.back() )
                        emitLine_( levelIdx, it, /* closed */ true );
                    break;
                }
                else if (other->ls.back() == it->ls.front())
                {
                    it->ls.pop_front();
                    other->ls.splice(other->ls.end(), it->ls);
                    other->isMerged = true;
                    lines.erase(it);
                    // if that makes a closed ring, returns it
                    if ( other->ls.front() == other->ls.back() )
                        emitLine_( levelIdx, other, /* closed */ true );
                    break;
                }
                // two lists must be merged but one is in the opposite direction
                else if (it->ls.back() == other->ls.back())
                {
                    it->ls.pop_back();
                    for ( auto rit = other->ls.rbegin(); rit != other->ls.rend(); ++rit ) {
                        it->ls.push_back( *rit );
                    }
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if ( it->ls.front() == it->ls.back() )
                        emitLine_( levelIdx, it, /* closed */ true );
                    break;
                }
                else if (it->ls.front() == other->ls.front())
                {
                    it->ls.pop_front();
                    for ( auto rit = other->ls.begin(); rit != other->ls.end(); ++rit ) {
                        it->ls.push_front( *rit );
                    }
                    it->isMerged = true;
                    lines.erase(other);
                    // if that makes a closed ring, returns it
                    if ( it->ls.front() == it->ls.back() )
                        emitLine_( levelIdx, it, /* closed */ true );
                    break;
                }
            }
        }
    }

    typename Lines::iterator emitLine_( int levelIdx, typename Lines::iterator it, bool closed )
    {
        Lines& lines = lines_[levelIdx];
        if ( lines.empty() )
            lines_.erase( levelIdx );

        // consume "it" and remove it from the list
        lineWriter_.addLine( levelGenerator_.level( levelIdx ), it->ls, closed );
        return lines.erase( it );
    }
};

}
#endif
