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
#ifndef MARCHING_SQUARE_UTILITY_H
#define MARCHING_SQUARE_UTILITY_H

#include <limits>
#include <cmath>

namespace marching_squares
{

// This is used to determine the maximum level value for polygons,
// the one that spans all the remaining plane
constexpr double Inf = std::numeric_limits<double>::infinity();

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

#define debug(format, ...) CPLDebug("MarchingSquare", format, ##__VA_ARGS__)

// Perturb a value if it is too close to a level value
inline double fudge(double value, double minLevel, double level)
{
    // FIXME
    // This is too "hard coded". The perturbation to apply really depend on
    // values between which we have to interpolate, so that the result of
    // interpolation should give coordinates that are "numerically" stable for
    // classical algorithms to work (on polygons for instance).
    //
    // Ideally we should probably use snap rounding to ensure no contour lines
    // are within a user-provided minimum distance.

    const double absTol = 1e-6;
    // Do not fudge the level that would correspond to the absolute minimum
    // level of the raster, so it gets included.
    // Cf scenario of https://github.com/OSGeo/gdal/issues/10167
    if (level == minLevel)
        return value;
    return std::abs(level - value) < absTol ? value + absTol : value;
}

}  // namespace marching_squares
#endif
