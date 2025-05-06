/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <array>
#include <iostream>

#include "gdal_priv.h"
#include "util.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/// Normalize a masking angle. Change from clockwise with 0 north (up) to counterclockwise
/// with 0 to the east (right) and change to radians.
double normalizeAngle(double maskAngle)
{
    maskAngle = 90 - maskAngle;
    if (maskAngle < 0)
        maskAngle += 360;
    return maskAngle * (M_PI / 180);
}

/// Compute the X intersect position on the line Y = y with a ray extending
/// from (nX, nY) along `angle`.
///ABELL doc args
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

int hIntersect(double angle, int nX, int nY, int y)
{
    double x = horizontalIntersect(angle, nX, nY, y);
    if (std::isnan(x))
        return (std::numeric_limits<int>::max)();
    return static_cast<int>(std::round(x));
}

/// Compute the Y intersect position on the line X = x with a ray extending
/// from (nX, nY) along `angle`.
///ABELL doc args
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

int vIntersect(double angle, int nX, int nY, int x)
{
    double y = verticalIntersect(angle, nX, nY, x);
    if (std::isnan(y))
        return (std::numeric_limits<int>::max)();
    return static_cast<int>(std::round(y));
}

// Determine if ray is in the slice between two rays starting at `start` and
// going clockwise to `end`.
bool rayBetween(double start, double end, double test)
{
    // Our angles go counterclockwise, so swap start and end
    std::swap(start, end);
    if (start < end)
        return (test > start && test < end);
    else if (start > end)
        return (test > start || test < end);
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
        opts.outputMode == OutputMode::Normal ? GDT_Byte : GDT_Float64,
        const_cast<char **>(opts.creationOpts.List())));
    if (!dataset)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                 opts.outputFilename.c_str());
        return nullptr;
    }

    /* copy srs */
    dataset->SetSpatialRef(srcBand.GetDataset()->GetSpatialRef());

    std::array<double, 6> adfSrcTransform;
    std::array<double, 6> adfDstTransform;
    srcBand.GetDataset()->GetGeoTransform(adfSrcTransform.data());
    adfDstTransform[0] = adfSrcTransform[0] +
                         adfSrcTransform[1] * extent.xStart +
                         adfSrcTransform[2] * extent.yStart;
    adfDstTransform[1] = adfSrcTransform[1];
    adfDstTransform[2] = adfSrcTransform[2];
    adfDstTransform[3] = adfSrcTransform[3] +
                         adfSrcTransform[4] * extent.xStart +
                         adfSrcTransform[5] * extent.yStart;
    adfDstTransform[4] = adfSrcTransform[4];
    adfDstTransform[5] = adfSrcTransform[5];
    dataset->SetGeoTransform(adfDstTransform.data());

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
