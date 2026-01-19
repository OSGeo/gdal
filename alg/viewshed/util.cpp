/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

#include "gdal_priv.h"
#include "util.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/// Normalize a masking angle. Change from clockwise with 0 north (up) to counterclockwise
/// with 0 to the east (right) and change to radians.
///
// @param maskAngle  Masking angle in degrees.
double normalizeAngle(double maskAngle)
{
    maskAngle = 90 - maskAngle;
    if (maskAngle < 0)
        maskAngle += 360;
    return maskAngle * (M_PI / 180);
}

/// Compute the X intersect position on the line Y = y with a ray extending
/// from (nX, nY) along `angle`.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param y  Horizontal line where Y = y.
/// @return  X intersect or NaN
double horizontalIntersect(double angle, int nX, int nY, int y)
{
    double x = std::numeric_limits<double>::quiet_NaN();

    if (nY == y)
        x = nX;
    else if (nY > y)
    {
        if (ARE_REAL_EQUAL(angle, M_PI / 2))
            x = nX;
        else if (angle > 0 && angle < M_PI)
            x = nX + (nY - y) / std::tan(angle);
    }
    else  // nY < y
    {
        if (ARE_REAL_EQUAL(angle, 3 * M_PI / 2))
            x = nX;
        else if (angle > M_PI)
            x = nX - (y - nY) / std::tan(angle);
    }
    return x;
}

/// Compute the X intersect position on the line Y = y with a ray extending
/// from (nX, nY) along `angle`.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param y  Horizontal line where Y = y.
/// @return  Rounded X intersection of the sentinel INVALID_ISECT
int hIntersect(double angle, int nX, int nY, int y)
{
    double x = horizontalIntersect(angle, nX, nY, y);
    if (std::isnan(x))
        return INVALID_ISECT;
    return static_cast<int>(std::round(x));
}

/// Compute the X intersect on one of the horizontal edges of a window
/// with a ray extending from (nX, nY) along `angle`, clamped the the extent of a window.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param win  Window to intersect.
/// @return  X intersect, clamped to the window extent.
int hIntersect(double angle, int nX, int nY, const Window &win)
{
    if (ARE_REAL_EQUAL(angle, M_PI))
        return win.xStart;
    if (ARE_REAL_EQUAL(angle, 0))
        return win.xStop;
    double x = horizontalIntersect(angle, nX, nY, win.yStart);
    if (std::isnan(x))
        x = horizontalIntersect(angle, nX, nY, win.yStop);
    return std::clamp(static_cast<int>(std::round(x)), win.xStart, win.xStop);
}

/// Compute the X intersect position on the line Y = y with a ray extending
/// from (nX, nY) along `angle`.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param x  Vertical  line where X = x.
/// @return  Y intersect or NaN
double verticalIntersect(double angle, int nX, int nY, int x)
{
    double y = std::numeric_limits<double>::quiet_NaN();

    if (nX == x)
        y = nY;
    else if (nX < x)
    {
        if (ARE_REAL_EQUAL(angle, 0))
            y = nY;
        else if (angle < M_PI / 2 || angle > M_PI * 3 / 2)
            y = nY + (nX - x) * std::tan(angle);
    }
    else  // nX > x
    {
        if (ARE_REAL_EQUAL(angle, M_PI))
            y = nY;
        else if (angle > M_PI / 2 && angle < M_PI * 3 / 2)
            y = nY - (x - nX) * std::tan(angle);
    }
    return y;
}

/// Compute the X intersect position on the line Y = y with a ray extending
/// from (nX, nY) along `angle`.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param x  Horizontal line where X = x.
/// @return  Rounded Y intersection of the sentinel INVALID_ISECT
int vIntersect(double angle, int nX, int nY, int x)
{
    double y = verticalIntersect(angle, nX, nY, x);
    if (std::isnan(y))
        return INVALID_ISECT;
    return static_cast<int>(std::round(y));
}

/// Compute the Y intersect on one of the vertical edges of a window
/// with a ray extending from (nX, nY) along `angle`, clamped the the extent
/// of the window.
///
/// @param angle  Angle in radians, standard arrangement.
/// @param nX  X coordinate of ray endpoint.
/// @param nY  Y coordinate of ray endpoint.
/// @param win  Window to intersect.
/// @return  y intersect, clamped to the window extent.
int vIntersect(double angle, int nX, int nY, const Window &win)
{
    if (ARE_REAL_EQUAL(angle, M_PI / 2))
        return win.yStart;
    if (ARE_REAL_EQUAL(angle, 3 * M_PI / 2))
        return win.yStop;
    double y = verticalIntersect(angle, nX, nY, win.xStart);
    if (std::isnan(y))
        y = verticalIntersect(angle, nX, nY, win.xStop);
    return std::clamp(static_cast<int>(std::round(y)), win.yStart, win.yStop);
}

/// Determine if ray is in the slice between two rays starting at `start` and
/// going clockwise to `end` (inclusive). [start, end]
/// @param  start  Start angle.
/// @param  end  End angle.
/// @param  test  Test angle.
/// @return  Whether `test` lies in the slice [start, end]
bool rayBetween(double start, double end, double test)
{
    // Our angles go counterclockwise, so swap start and end
    std::swap(start, end);
    if (start < end)
        return (test >= start && test <= end);
    else if (start > end)
        return (test >= start || test <= end);
    return false;
}

/// Get the band size
///
/// @param  band Raster band
/// @return  The raster band size.
size_t bandSize(GDALRasterBand &band)
{
    return static_cast<size_t>(band.GetXSize()) * band.GetYSize();
}

/// Create the output dataset.
///
/// @param  srcBand  Source raster band.
/// @param  opts  Options.
/// @param  extent  Output dataset extent.
/// @return  The output dataset to be filled with data.
DatasetPtr createOutputDataset(GDALRasterBand &srcBand, const Options &opts,
                               const Window &extent)
{
    GDALDriverManager *hMgr = GetGDALDriverManager();
    GDALDriver *hDriver = hMgr->GetDriverByName(opts.outputFormat.c_str());
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return nullptr;
    }

    /* create output raster */
    DatasetPtr dataset(hDriver->Create(
        opts.outputFilename.c_str(), extent.xSize(), extent.ySize(), 1,
        opts.outputMode == OutputMode::Normal ? GDT_UInt8 : GDT_Float64,
        const_cast<char **>(opts.creationOpts.List())));
    if (!dataset)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                 opts.outputFilename.c_str());
        return nullptr;
    }

    /* copy srs */
    dataset->SetSpatialRef(srcBand.GetDataset()->GetSpatialRef());

    GDALGeoTransform srcGT, dstGT;
    srcBand.GetDataset()->GetGeoTransform(srcGT);
    dstGT[0] = srcGT[0] + srcGT[1] * extent.xStart + srcGT[2] * extent.yStart;
    dstGT[1] = srcGT[1];
    dstGT[2] = srcGT[2];
    dstGT[3] = srcGT[3] + srcGT[4] * extent.xStart + srcGT[5] * extent.yStart;
    dstGT[4] = srcGT[4];
    dstGT[5] = srcGT[5];
    dataset->SetGeoTransform(dstGT);

    GDALRasterBand *pBand = dataset->GetRasterBand(1);
    if (!pBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 opts.outputFilename.c_str());
        return nullptr;
    }

    if (opts.nodataVal >= 0)
        GDALSetRasterNoDataValue(pBand, opts.nodataVal);
    return dataset;
}

}  // namespace viewshed
}  // namespace gdal
