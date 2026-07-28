/****************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef VIEWSHED_TYPES_H_INCLUDED
#define VIEWSHED_TYPES_H_INCLUDED

#include <algorithm>
#include <limits>
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
    double maybeVisibleVal{
        2};  //!< raster output for potentially visible pixels.
    double outOfRangeVal{
        0};  //!< raster output value for pixels outside of max distance.
    double nodataVal{-1};      //!< raster output value for pixels with no data
    double targetHeight{0.0};  //!< target height above the DEM surface
    double maxDistance{
        0.0};  //!< maximum distance from observer to compute value
    double minDistance{
        0.0};  //!< minimum distance from observer to compute value.
    double startAngle{0.0};  //!< start angle of observable range
    double endAngle{0.0};    //!< end angle of observable range
    double lowPitch{
        -90.0};  //!< minimum pitch (vertical angle) of observable points
    double highPitch{
        90.0};  //!< maximum pitch (vertical angle) of observable points
    double curveCoeff{.85714};  //!< coefficient for atmospheric refraction
    OutputMode outputMode{OutputMode::Normal};  //!< Output information.
    //!< Normal, Height from DEM or Height from ground
    std::string outputFormat{};         //!< output raster format
    std::string outputFilename{};       //!< output raster filename
    CPLStringList creationOpts{};       //!< options for output raster creation
    CellMode cellMode{CellMode::Edge};  //!< Mode of cell height calculation.
    int observerSpacing{10};  //!< Observer spacing in cumulative mode.
    uint8_t numJobs{3};       //!< Relative number of jobs in cumulative mode.

    /// True if angle masking will occur.
    bool angleMasking() const
    {
        return startAngle != endAngle;
    }

    /// True if low pitch masking will occur.
    bool lowPitchMasking() const
    {
        return lowPitch > -90.0;
    }

    /// True if high pitch masking will occur.
    bool highPitchMasking() const
    {
        return highPitch < 90.0;
    }

    /// True if pitch masking will occur.
    bool pitchMasking() const
    {
        return lowPitchMasking() || highPitchMasking();
    }
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

    /// Returns true when one window is equal to the other.
    bool operator==(const Window &w2) const
    {
        return xStart == w2.xStart && xStop == w2.xStop &&
               yStart == w2.yStart && yStop == w2.yStop;
    }

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

inline std::ostream &operator<<(std::ostream &out, const Window &w)
{
    out << "Xstart/stop Ystart/stop = " << w.xStart << "/" << w.xStop << " "
        << w.yStart << "/" << w.yStop;
    return out;
}

/// Processing limits based on min/max distance restrictions.
/// The left side processing range is [left, leftMin).
/// The right side processing range is [rightMin, right).
struct LineLimits
{
    /// Constructor that takes the members in order.
    LineLimits(int leftArg, int leftMinArg, int rightMinArg, int rightArg)
        : left(leftArg), leftMin(leftMinArg), rightMin(rightMinArg),
          right(rightArg)
    {
    }

    int left;      //!< Starting (leftmost) cell on the left side.
    int leftMin;   //!< One past the rightmost cell on the left side.
    int rightMin;  //!< Starting (leftmost) cell on the right side.
    int right;     //!< One past the rightmost cell on the right side.
};

inline std::ostream &operator<<(std::ostream &out, const LineLimits &ll)
{
    out << "Left/LeftMin RightMin/Right = " << ll.left << "/" << ll.leftMin
        << " " << ll.rightMin << "/" << ll.right;
    return out;
}

constexpr int INVALID_ISECT = std::numeric_limits<int>::max();

}  // namespace viewshed
}  // namespace gdal

#endif
