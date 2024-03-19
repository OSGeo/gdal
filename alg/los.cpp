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
static
bool Bresenham2D(int x1, int y1, int x2, int y2, std::function< auto(int, int) -> bool> OnBresenhamPoint) {
    bool isAboveTerrain = true;
	int	dx, dy;
	int	incx, incy;

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
    int	balance;

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

		while (y != y2  && isAboveTerrain)
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
bool GetElevation(const GDALRasterBandH hBand, const int x, const int y, double& val)
{
    CPLAssert(x >= 0);
    CPLAssert(x <= GDALGetRasterBandXSize(hBand));
    CPLAssert(y >= 0);
    CPLAssert(y <= GDALGetRasterBandYSize(hBand));

    return GDALRasterIO(hBand, GF_Read, x, y, 1, 1, &val, 1, 1, GDT_Float64, 0, 0) == CE_None;
}

// Check a single location is above terrain.
bool IsAboveTerrain(const GDALRasterBandH hBand, const int x, const int y, const double z)
{
    double terrainHeight;
    if(GetElevation(hBand, x, y, terrainHeight)) {
        return z > terrainHeight;
    } else {
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
 * This algorithm will check line of sight using a 3D bresenham algorithm.
 * https://www.researchgate.net/publication/2411280_Efficient_Line-of-Sight_Algorithms_for_Real_Terrain_Data
 *
 * @param hBand The band to read the DEM data from.
 * 
 * @param xA The X location to check on the raster to read the DEM data from.
 *
 * @return True if the two points are within Line of Sight.
 *
 * @since GDAL 3.X
 */

bool GDALIsLineOfSightVisible(
    const GDALRasterBandH hBand, const int xA, const int yA, const double zA, const int xB, const int yB, const double zB, const char** papszOptions)
{
    if(hBand == nullptr) {
        return false;
    }

    // Perform a preliminary check of the start and end points.
    if (!IsAboveTerrain(hBand, xA, yA, zA)) {
        return false;
    
    }
    if (!IsAboveTerrain(hBand, xB, yB, zB)) {
        return false;
    }

    // If both X and Y are the same, no further checks are needed.
    if(xA == xB && yA == yB) {
        return true;
    }

    // TODO if both X's or Y's are the same, it could be optimized for vertical/horizontal lines.


    // Use an interpolated Z height with 2D bresenham for the remaining cases.

    // Lambda for Linear interpolate like C++20 std::lerp.
    auto lerp = [](const int a, const int b, const float t) {
        return a + t * (b - a);
    };

    // Lambda for getting Z test height given x input along the bresenham line.
    auto GetZValue = [&](const int x) -> double {
        const auto xDiff = xB - xA;
        const auto xPercent = x / xDiff;
        return lerp(zA, zB, xPercent);
    };

    // Lambda to get elevation at a bresenham-computed location.
    auto OnBresenhamPoint = [&](const int x, const int y) -> bool {
        const auto z = GetZValue(x);
        return IsAboveTerrain(hBand, x, y, z);
    };

    return Bresenham2D(xA, yA, xB, yB, OnBresenhamPoint);
}


