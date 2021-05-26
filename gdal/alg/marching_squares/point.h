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

#ifndef MARCHING_SQUARE_POINT_H
#define MARCHING_SQUARE_POINT_H

#include "utility.h"
#include <ostream>
#include <algorithm>
#include <cmath>
#include <list>

namespace marching_squares {

// regular point
struct Point
{
    Point(): x(NaN), y(NaN) {} // just to be able to make an uninitialized list
    Point(double x_, double y_): x(x_), y(y_) {}
    double x;
    double y;
};

inline
bool operator==(const Point& lhs, const Point& rhs)
{
    return (lhs.x == rhs.x) && (lhs.y == rhs.y);
}

inline
std::ostream & operator<<(std::ostream & o, const Point& p)
{
    o << p.x << " " << p.y;
    return o;
}

// Test if a point is to the left or right of an infinite line.
// Returns true if it is to the left and right otherwise
// 0 if p2 is on the line and less than if p2 is to the right of the line
inline bool
isLeft(const Point& p0, const Point& p1, const Point& p2 )
{
    return ( (p1.x - p0.x) * (p2.y - p0.y)
            - (p2.x -  p0.x) * (p1.y - p0.y) ) > 0;
}

// LineString type
typedef std::list<Point> LineString;

inline
std::ostream & operator<<(std::ostream & o, const LineString& ls)
{
    o << "{";
    for ( const auto& p: ls )
    {
        o << p << ", ";
    }
    o << "}";
    return o;
}

// Point with a value
struct ValuedPoint
{
    ValuedPoint(double x_, double y_, double value_): x(x_), y(y_), value(value_) {}
    const double x;
    const double y;
    const double value;
};

}

#endif
