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
#include <cassert>

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
        
        mutable std::list<const Ring*> interiorRings;

        const Ring* closestExterior = nullptr;

        bool isIn( const Ring& other ) const
        {
            // FIXME
            // This could probably be optimized by avoiding copies
            // (and a dependency to OGR/GEOS)

            assert( other.points.size() >= 4 );
            Point p = points.front();

            OGRLinearRing r;
            for ( const auto& pt : other.points ) {
                r.addPoint( pt.x, pt.y );
            }
            OGRPolygon poly;
            poly.addRing( &r );

            OGRPoint toTest( p.x, p.y );

            return toTest.Within( &poly ) != 0;
        }

        void checkInclusionWith( const Ring& other )
        {
            if ( isIn( other ) ) {
                if ( closestExterior ) {
                    if ( other.isIn( *closestExterior ) ) {
                        closestExterior = &other;
                    }
                }
                else {
                    // no closest parent yet
                    closestExterior = &other;
                }
            }
        }

        // recursive function to test if a ring is an inner ring
        bool isInnerRing() const
        {
            return (closestExterior != nullptr) && ( !closestExterior->isInnerRing() );
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

    // level -> rings
    std::map<double, std::list<Ring>> rings_;

    PolygonWriter& writer_;

public:
    const bool polygonize = true;

    PolygonRingAppender( PolygonWriter& writer )
        : rings_()
        , writer_( writer )
    {}

    void addLine( double level, LineString& ls, bool )
    {
        Ring r;
        r.points.swap( ls );
        rings_[level].push_back( std::move(r) );
    }

    ~PolygonRingAppender()
    {
        // FIXME
        // This "ring sorting" algorithm could be optimized. There is no need to
        // wait for the completion of contour ring computation to sort them out.
        // For each new ring closed, we can determine which other rings are inside
        // it and do not consider them anymore.

        if ( rings_.size() == 0 )
            return;

        // compute inner rings
        for ( auto& itLevel: rings_ ) {
            for ( auto& currentRing: itLevel.second ) {
                for ( const auto& otherRing: itLevel.second ) {
                    currentRing.checkInclusionWith( otherRing );
                }
            }

            // sort inner / outer rings
            for ( auto& currentRing: itLevel.second ) {
                if ( currentRing.isInnerRing() ) {
                    currentRing.closestExterior->interiorRings.push_back( &currentRing );
                }
            }
        }

        // emit each polygon with its parts and interior rings
        for ( const auto& r: rings_ ) {
            writer_.startPolygon( r.first );
            for ( const auto& part : r.second ) {
                if ( ! part.isInnerRing() ) {
                    writer_.addPart( part.points );
                    for ( const Ring* interiorRing : part.interiorRings ) {
                        writer_.addInteriorRing( interiorRing->points );
                    }
                }
            }
            writer_.endPolygon();
        }
    }
};

}

#endif
