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
#ifndef MARCHING_SQUARE_POLYGON_RING_APPENDER_H
#define MARCHING_SQUARE_POLYGON_RING_APPENDER_H

#include <vector>
#include <list>
#include <map>
#include <deque>
#include <cassert>
#include <iterator>

#include "point.h"
#include "ogr_geometry.h"

namespace marching_squares {

// Receive rings of different levels and organize them
// into multi-polygons with possible interior rings when requested.
template <typename PolygonWriter>
class PolygonRingAppender
{
private:
    struct Ring
    {
        Ring() : points(), interiorRings() {}
        Ring( const Ring& other ) = default;
        Ring& operator=( const Ring& other ) = default;

        LineString points;

        mutable std::vector<Ring> interiorRings;

        const Ring* closestExterior = nullptr;

        bool isIn( const Ring& other ) const
        {
            // Check if this is inside other using the winding number algorithm
            auto checkPoint = this->points.front();
            int windingNum = 0;
            auto otherIter = other.points.begin();
            // p1 and p2 define each segment of the ring other that will be tested
            auto p1 = *otherIter;
            while(true) {
                otherIter++;
                if (otherIter == other.points.end()) {
                    break;
                }
                auto p2 = *otherIter;
                if ( p1.y <= checkPoint.y ) {
                    if ( p2.y  > checkPoint.y ) {
                        if ( isLeft(p1, p2, checkPoint) )  {
                             ++windingNum;
                        }
                    }
                } else {
                    if ( p2.y <= checkPoint.y ) {
                        if ( !isLeft( p1, p2, checkPoint)  ) {
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
            return size_t(static_cast<const void*>(this)) & 0xffff;
        }

        void print( std::ostream& ostr ) const
        {
            ostr << id() << ":";
            for ( const auto& pt : points ) {
                ostr << pt.x << "," << pt.y << " ";
            }
        }
#endif
    };

    void processTree(const std::vector<Ring> &tree, int level) {
        if ( level % 2 == 0 ) {
            for( auto &r: tree ) {
                writer_.addPart(r.points);
                for( auto &innerRing: r.interiorRings ) {
                    writer_.addInteriorRing(innerRing.points);
                }
            }
        }
        for( auto &r: tree ) {
            processTree(r.interiorRings, level + 1);
        }
    }

    // level -> rings
    std::map<double, std::vector<Ring>> rings_;

    PolygonWriter& writer_;

public:
    const bool polygonize = true;

    PolygonRingAppender( PolygonWriter& writer )
        : rings_()
        , writer_( writer )
    {}

    void addLine( double level, LineString& ls, bool )
    {
        // Create a new ring from the LineString
        Ring newRing;
        newRing.points.swap( ls );
        auto &levelRings = rings_[level];
        // This queue holds the rings to be checked
        std::deque<Ring*> queue;
        std::transform(levelRings.begin(),
                       levelRings.end(),
                       std::back_inserter(queue),
                       [](Ring &r) {
                           return &r;
                       });
        Ring *parentRing = nullptr;
        while( !queue.empty() ) {
            Ring *curRing = queue.front();
            queue.pop_front();
            if ( newRing.isIn(*curRing) ) {
                // We know that there should only be one ring per level that we should fit in,
                // so we can discard the rest of the queue and try again with the children of this ring
                parentRing = curRing;
                queue.clear();
                std::transform(curRing->interiorRings.begin(),
                            curRing->interiorRings.end(),
                            std::back_inserter(queue),
                            [](Ring &r) {
                                return &r;
                            });
            }
        }
        // Get a pointer to the list we need to check for rings to include in this ring
        std::vector<Ring> *parentRingList;
        if ( parentRing == nullptr ) {
            parentRingList = &levelRings;
        } else {
            parentRingList = &(parentRing->interiorRings);
        }
        // We found a valid parent, so we need to:
        // 1. Find all the inner rings of the parent that are inside the new ring
        auto trueGroupIt = std::partition(
            parentRingList->begin(),
            parentRingList->end(),
            [newRing](Ring &pRing) {
                return !pRing.isIn(newRing);
            }
        );
        // 2. Move those rings out of the parent and into the new ring's interior rings
        std::move(trueGroupIt, parentRingList->end(), std::back_inserter(newRing.interiorRings));
        // 3. Get rid of the moved-from elements in the parent's interior rings
        parentRingList->erase(trueGroupIt, parentRingList->end());
        // 4. Add the new ring to the parent's interior rings
        parentRingList->push_back(newRing);
    }

    ~PolygonRingAppender()
    {
        // If there's no rings, nothing to do here
        if ( rings_.size() == 0 )
            return;

        // Traverse tree of rings
        for ( auto& r: rings_ ) {
            // For each level, create a multipolygon by traversing the tree of
            // rings and adding a part for every other level
            writer_.startPolygon( r.first );
            processTree(r.second, 0);
            writer_.endPolygon();
        }
    }
};

}

#endif
