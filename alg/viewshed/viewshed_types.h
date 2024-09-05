/****************************************************************************
 * (c) 2024 info@hobu.co
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
#pragma once

#include <algorithm>
#include <string>

#include "gdal_priv.h"

namespace gdal
{
namespace viewshed
{

/// Unique pointer to GDAL dataset.
using DatasetPtr = std::unique_ptr<GDALDataset>;

/**
 * Raster output mode.
 */
enum class OutputMode
{
    Normal,     //!< Normal output mode (visibility only)
    DEM,        //!< Output height from DEM
    Ground,     //!< Output height from ground
    Cumulative  //!< Output observability heat map
};

/**
 * Cell height calculation mode.
 */
enum class CellMode
{
    Diagonal,  //!< Diagonal Mode
    Edge,      //!< Edge Mode
    Max,       //!< Maximum value produced by Diagonal and Edge mode
    Min        //!< Minimum value produced by Diagonal and Edge mode
};

/**
 * A point.
 */
struct Point
{
    double x;  //!< X value
    double y;  //!< Y value
    double z;  //!< Z value
};

/**
 * Options for viewshed generation.
 */
struct Options
{
    Point observer{0, 0, 0};  //!< x, y, and z of the observer
    double visibleVal{255};   //!< raster output value for visible pixels.
    double invisibleVal{0};   //!< raster output value for non-visible pixels.
    double outOfRangeVal{
        0};  //!< raster output value for pixels outside of max distance.
    double nodataVal{-1};      //!< raster output value for pixels with no data
    double targetHeight{0.0};  //!< target height above the DEM surface
    double maxDistance{
        0.0};  //!< maximum distance from observer to compute value
    double curveCoeff{.85714};  //!< coefficient for atmospheric refraction
    OutputMode outputMode{OutputMode::Normal};  //!< Output information.
        //!< Normal, Height from DEM or Height from ground
    std::string outputFormat{};         //!< output raster format
    std::string outputFilename{};       //!< output raster filename
    CPLStringList creationOpts{};       //!< options for output raster creation
    CellMode cellMode{CellMode::Edge};  //!< Mode of cell height calculation.
    int observerSpacing{10};  //!< Observer spacing in cumulative mode.
    uint8_t numJobs{3};       //!< Relative number of jobs in cumulative mode.
};

/**
 * A window in a raster including pixels in [xStart, xStop) and [yStart, yStop).
 */
struct Window
{
    int xStart{};  //!< X start position
    int xStop{};   //!< X end position
    int yStart{};  //!< Y start position
    int yStop{};   //!< Y end position

    /// \brief  Window size in the X direction.
    int xSize() const
    {
        return xStop - xStart;
    }

    /// \brief  Window size in the Y direction.
    int ySize() const
    {
        return yStop - yStart;
    }

    /// \brief  Number of cells.
    size_t size() const
    {
        return static_cast<size_t>(xSize()) * ySize();
    }

    /// \brief  Determine if the X window contains the index.
    /// \param  nX  Index to check
    /// \return  True if the index is contained, false otherwise.
    bool containsX(int nX) const
    {
        return nX >= xStart && nX < xStop;
    }

    /// \brief  Determine if the Y window contains the index.
    /// \param  nY  Index to check
    /// \return  True if the index is contained, false otherwise.
    bool containsY(int nY) const
    {
        return nY >= xStart && nY < yStop;
    }

    /// \brief  Determine if the window contains the index.
    /// \param  nX  X coordinate of the index to check
    /// \param  nY  Y coordinate of the index to check
    /// \return  True if the index is contained, false otherwise.
    bool contains(int nX, int nY) const
    {
        return containsX(nX) && containsY(nY);
    }

    /// \brief  Clamp the argument to be in the window in the X dimension.
    /// \param  nX  Value to clamp.
    /// \return  Clamped value.
    int clampX(int nX) const
    {
        return xSize() ? std::clamp(nX, xStart, xStop - 1) : xStart;
    }

    /// \brief  Clamp the argument to be in the window in the Y dimension.
    /// \param  nY  Value to clamp.
    /// \return  Clamped value.
    int clampY(int nY) const
    {
        return ySize() ? std::clamp(nY, yStart, yStop - 1) : yStart;
    }

    /// \brief  Shift the X dimension by nShift.
    /// \param  nShift  Amount to shift
    void shiftX(int nShift)
    {
        xStart += nShift;
        xStop += nShift;
    }
};

}  // namespace viewshed
}  // namespace gdal
