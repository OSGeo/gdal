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
#ifndef MARCHING_SQUARE_UTILITY_H
#define MARCHING_SQUARE_UTILITY_H

#include <limits>
#include <cmath>

namespace marching_squares {

// This is used to determine the maximum level value for polygons,
// the one that spans all the remaining plane
const double Inf = std::numeric_limits<double>::max();

const double NaN = std::numeric_limits<double>::quiet_NaN();

#define debug(format, ...) CPLDebug("MarchingSquare", format, ##__VA_ARGS__ )

// Perturb a value if it is too close to a level value
inline
double fudge(double level, double value)
{
    // FIXME
    // This is too "hard coded". The perturbation to apply really depend on values
    // between which we have to interpolate, so that the result of interpolation
    // should give coordinates that are "numerically" stable for classical algorithms
    // to work (on polygons for instance).
    //
    // Ideally we should probably use snap rounding to ensure no contour lines are
    // within a user-provided minimum distance.

    const double absTol = 1e-6;
    return std::abs(level - value) < absTol ? value + absTol : value;
}

}
#endif
