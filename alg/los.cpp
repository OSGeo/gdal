/******************************************************************************
 *
 * Project:  Line of Sight
 * Purpose:  Core algorithm implementation for line of sight algorithms.
 * Author:   Ryan Friedman, ryanfriedman5410+gdal@gmail.com
 *
 ******************************************************************************
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

#include <functional>
#include <cmath>

#include "cpl_port.h"
#include "gdal_alg.h"

// There's a plethora of bresenham implementations, all questionable production quality.
// Bresenham optimizes for integer math, which makes sense for raster datasets in 2D.
// For 3D, a 3D bresenham could be used if the altitude is also integer resolution.
// 2D:
// https://codereview.stackexchange.com/questions/77460/bresenhams-line-algorithm-optimization
// https://gist.github.com/ssavi-ict/092501c69e2ffec65e96a8865470ad2f
// https://blog.demofox.org/2015/01/17/bresenhams-drawing-algorithms/
// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
// https://www.cs.helsinki.fi/group/goa/mallinnus/lines/bresenh.html
// https://stackoverflow.com/questions/10060046/drawing-lines-with-bresenhams-line-algorithm
// http://www.edepot.com/linebresenham.html
// 3D:
// https://gist.github.com/yamamushi/5823518

// Run bresenham terrain checking from (x1, y1) to (x2, y2).
// The callback is run at every point along the line,
// which should return True if the point is above terrain.
// Bresenham2D will return true if all points have LOS between the start and end.
static bool
Bresenham2D(const int x1, const int y1, const int x2, const int y2,
            std::function<auto(const int, const int)->bool> OnBresenhamPoint)
{
    bool isAboveTerrain = true;
    int dx, dy;
    int incx, incy;

    if (x2 >= x1)
    {
        dx = x2 - x1;
        incx = 1;
    }
    else
    {
        dx = x1 - x2;
        incx = -1;
    }

    if (y2 >= y1)
    {
        dy = y2 - y1;
        incy = 1;
    }
    else
    {
        dy = y1 - y2;
        incy = -1;
    }

    auto x = x1;
    auto y = y1;
    int balance;

    if (dx >= dy)
    {
        dy <<= 1;
        balance = dy - dx;
        dx *= 2;

        while (x != x2 && isAboveTerrain)
        {
            isAboveTerrain &= OnBresenhamPoint(x, y);
            if (balance >= 0)
            {
                y += incy;
                balance -= dx;
            }
            balance += dy;
            x += incx;
        }
        isAboveTerrain &= OnBresenhamPoint(x, y);
    }
    else
    {
        dx *= 2;
        balance = dx - dy;
        dy *= 2;

        while (y != y2 && isAboveTerrain)
        {
            isAboveTerrain &= OnBresenhamPoint(x, y);
            if (balance >= 0)
            {
                x += incx;
                balance -= dy;
            }
            balance += dx;
            y += incy;
        }
        isAboveTerrain &= OnBresenhamPoint(x, y);
    }
    return isAboveTerrain;
}

// Get the elevation of a single point.
static bool GetElevation(const GDALRasterBandH hBand, const int x, const int y,
                         double &val)
{
    /// @todo GDALCachedPixelAccessor may give increased performance.
    return GDALRasterIO(hBand, GF_Read, x, y, 1, 1, &val, 1, 1, GDT_Float64, 0,
                        0) == CE_None;
}

// Check a single location is above terrain.
static bool IsAboveTerrain(const GDALRasterBandH hBand, const int x,
                           const int y, const double z)
{
    double terrainHeight;
    if (GetElevation(hBand, x, y, terrainHeight))
    {
        return z > terrainHeight;
    }
    else
    {
        return false;
    }
}

/************************************************************************/
/*                        GDALIsLineOfSightVisible()                    */
/************************************************************************/

/**
 * Check Line of Sight between two points.
 * Both input coordinates must be within the raster coordinate bounds.
 *
 * This algorithm will check line of sight using a Bresenham algorithm.
 * https://www.researchgate.net/publication/2411280_Efficient_Line-of-Sight_Algorithms_for_Real_Terrain_Data
 * Line of sight is computed in raster coordinate space, and thus may not be appropriate.
 * For example, datasets referenced against geographic coordinate at high latitudes may have issues.
 *
 * @param hBand The band to read the DEM data from. This must NOT be null.
 *
 * @param xA The X location (raster column) of the first point to check on the raster.
 *
 * @param yA The Y location (raster row) of the first point to check on the raster.
 *
 * @param zA The Z location (height) of the first point to check.
 *
 * @param xB The X location (raster column) of the second point to check on the raster.
 *
 * @param yB The Y location (raster row) of the second point to check on the raster.
 *
 * @param zB The Z location (height) of the second point to check.
 *
 * @param[out] pnxTerrainIntersection The X location where the LOS line
 *             intersects with terrain, or nullptr if it does not intersect
 *             terrain.
 *
 * @param[out] pnyTerrainIntersection The Y location where the LOS line
 *             intersects with terrain, or nullptr if it does not intersect
 *             terrain.
 *
 * @param papszOptions Options for the line of sight algorithm (currently ignored).
 *
 * @return True if the two points are within Line of Sight.
 *
 * @since GDAL 3.9
 */

bool GDALIsLineOfSightVisible(const GDALRasterBandH hBand, const int xA,
                              const int yA, const double zA, const int xB,
                              const int yB, const double zB,
                              int *pnxTerrainIntersection,
                              int *pnyTerrainIntersection,
                              CPL_UNUSED CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hBand, "GDALIsLineOfSightVisible", false);

    // A lambda to set the X-Y intersection if it's not null
    auto SetXYIntersection = [&](const int x, const int y)
    {
        if (pnxTerrainIntersection != nullptr)
        {
            *pnxTerrainIntersection = x;
        }
        if (pnyTerrainIntersection != nullptr)
        {
            *pnyTerrainIntersection = y;
        }
    };

    if (pnxTerrainIntersection)
        *pnxTerrainIntersection = -1;

    if (pnyTerrainIntersection)
        *pnyTerrainIntersection = -1;

    // Perform a preliminary check of the start and end points.
    if (!IsAboveTerrain(hBand, xA, yA, zA))
    {
        SetXYIntersection(xA, yA);
        return false;
    }
    if (!IsAboveTerrain(hBand, xB, yB, zB))
    {
        SetXYIntersection(xB, yB);
        return false;
    }

    // If both X and Y are the same, no further checks are needed.
    if (xA == xB && yA == yB)
    {
        return true;
    }

    // Lambda for Linear interpolate like C++20 std::lerp.
    auto lerp = [](const double a, const double b, const double t)
    { return a + t * (b - a); };

    // Lambda for getting Z test height given y input along the LOS line.
    // Only to be used for vertical line checks.
    auto GetZValueFromY = [&](const int y) -> double
    {
        // A ratio of 0.0 corresponds to being at yA.
        const auto ratio =
            static_cast<double>(y - yA) / static_cast<double>(yB - yA);
        return lerp(zA, zB, ratio);
    };

    // Lambda for getting Z test height given x input along the LOS line.
    // Only to be used for horizontal line checks.
    auto GetZValueFromX = [&](const int x) -> double
    {
        // A ratio of 0.0 corresponds to being at xA.
        const auto ratio =
            static_cast<double>(x - xA) / static_cast<double>(xB - xA);
        return lerp(zA, zB, ratio);
    };

    // Lambda for checking path safety of a vertical line.
    // Returns true if the path has clear LOS.
    auto CheckVerticalLine = [&]() -> bool
    {
        CPLAssert(xA == xB);
        CPLAssert(yA != yB);

        if (yA < yB)
        {
            for (int y = yA; y <= yB; ++y)
            {
                const auto zTest = GetZValueFromY(y);
                if (!IsAboveTerrain(hBand, xA, y, zTest))
                {
                    SetXYIntersection(xA, y);
                    return false;
                }
            }
            return true;
        }
        else
        {
            for (int y = yA; y >= yB; --y)
            {
                const auto zTest = GetZValueFromY(y);
                if (!IsAboveTerrain(hBand, xA, y, zTest))
                {
                    SetXYIntersection(xA, y);
                    return false;
                }
            }
            return true;
        }
    };

    // Lambda for checking path safety of a horizontal line.
    // Returns true if the path has clear LOS.
    auto CheckHorizontalLine = [&]() -> bool
    {
        CPLAssert(yA == yB);
        CPLAssert(xA != xB);

        if (xA < xB)
        {
            for (int x = xA; x <= xB; ++x)
            {
                const auto zTest = GetZValueFromX(x);
                if (!IsAboveTerrain(hBand, x, yA, zTest))
                {
                    SetXYIntersection(x, yA);
                    return false;
                }
            }
            return true;
        }
        else
        {
            for (int x = xA; x >= xB; --x)
            {
                const auto zTest = GetZValueFromX(x);
                if (!IsAboveTerrain(hBand, x, yA, zTest))
                {
                    SetXYIntersection(x, yA);
                    return false;
                }
            }
            return true;
        }
    };

    // Handle special cases if it's a vertical or horizontal line (don't use bresenham).
    if (xA == xB)
    {
        return CheckVerticalLine();
    }
    if (yA == yB)
    {
        return CheckHorizontalLine();
    }

    // Use an interpolated Z height with 2D bresenham for the remaining cases.

    // Lambda for computing the square of a number
    auto SQUARE = [](const double d) -> double { return d * d; };

    // Lambda for getting Z test height given x-y input along the bresenham line.
    auto GetZValueFromXY = [&](const int x, const int y) -> double
    {
        const auto rNum = SQUARE(static_cast<double>(x - xA)) +
                          SQUARE(static_cast<double>(y - yA));
        const auto rDenom = SQUARE(static_cast<double>(xB - xA)) +
                            SQUARE(static_cast<double>(yB - yA));
        /// @todo In order to reduce CPU cost and avoid a sqrt operation, consider
        /// the approach to just the ratio along x or y depending on whether
        /// the line is steep or shallow.
        /// See https://github.com/OSGeo/gdal/pull/9506#discussion_r1532459689.
        const double ratio =
            sqrt(static_cast<double>(rNum) / static_cast<double>(rDenom));
        return lerp(zA, zB, ratio);
    };

    // Lambda to get elevation at a bresenham-computed location.
    auto OnBresenhamPoint = [&](const int x, const int y) -> bool
    {
        const auto z = GetZValueFromXY(x, y);
        const auto isAbove = IsAboveTerrain(hBand, x, y, z);
        if (!isAbove)
        {
            SetXYIntersection(x, y);
        }
        return IsAboveTerrain(hBand, x, y, z);
    };

    return Bresenham2D(xA, yA, xB, yB, OnBresenhamPoint);
}
