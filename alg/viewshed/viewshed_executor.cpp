/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 * (c) 2024 info@hobu.co
 *
 ******************************************************************************
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <limits>

#include "viewshed_executor.h"
#include "progress.h"
#include "util.h"

// cppcheck-suppress-begin knownConditionTrueFalse
namespace gdal
{
namespace viewshed
{

//! @cond Doxygen_Suppress
CPLErr DummyBand::IReadBlock(int, int, void *)
{
    return CE_Failure;
}

//! @endcond

namespace
{

/// Determines whether a value is a valid intersection coordinate.
/// @param  i  Value to test.
/// @return  True if the value doesn't represent an invalid intersection.
bool valid(int i)
{
    return i != INVALID_ISECT;
}

/// Determines whether a value is an invalid intersection coordinate.
/// @param  i  Value to test.
/// @return  True if the value represents an invalid intersection.
bool invalid(int i)
{
    return !valid(i);
}

/// Calculate the height at nDistance units along a line through the origin given the height
/// at nDistance - 1 units along the line.
/// \param nDistance  Distance along the line for the target point.
/// \param Za  Height at the line one unit previous to the target point.
double CalcHeightLine(int nDistance, double Za)
{
    assert(nDistance > 1);
    return Za * nDistance / (nDistance - 1);
}

/// Calculate the height at nDistance units along a line through the origin given the height
/// at nDistance - 1 units along the line.
/// \param nDistance  Distance along the line for the target point.
/// \param Zcur  Height at the line at the target point.
/// \param Za    Height at the line one unit previous to the target point.
double CalcHeightLine(int nDistance, double Zcur, double Za)
{
    nDistance = std::abs(nDistance);
    assert(nDistance > 0);
    if (nDistance == 1)
        return Zcur;
    return CalcHeightLine(nDistance, Za);
}

// Calculate the height Zc of a point (i, j, Zc) given a line through the origin (0, 0, 0)
// and passing through the line connecting (i - 1, j, Za) and (i, j - 1, Zb).
// In other words, the origin and the two points form a plane and we're calculating Zc
// of the point (i, j, Zc), also on the plane.
double CalcHeightDiagonal(int i, int j, double Za, double Zb)
{
    return (Za * i + Zb * j) / (i + j - 1);
}

// Calculate the height Zc of a point (i, j, Zc) given a line through the origin (0, 0, 0)
// and through the line connecting (i -1, j - 1, Za) and (i - 1, j, Zb). In other words,
// the origin and the other two points form a plane and we're calculating Zc of the
// point (i, j, Zc), also on the plane.
double CalcHeightEdge(int i, int j, double Za, double Zb)
{
    assert(i != j);
    return (Za * i + Zb * (j - i)) / (j - 1);
}

double doDiagonal(int nXOffset, [[maybe_unused]] int nYOffset,
                  double dfThisPrev, double dfLast,
                  [[maybe_unused]] double dfLastPrev)
{
    return CalcHeightDiagonal(nXOffset, nYOffset, dfThisPrev, dfLast);
}

double doEdge(int nXOffset, int nYOffset, double dfThisPrev, double dfLast,
              double dfLastPrev)
{
    if (nXOffset >= nYOffset)
        return CalcHeightEdge(nYOffset, nXOffset, dfLastPrev, dfThisPrev);
    else
        return CalcHeightEdge(nXOffset, nYOffset, dfLastPrev, dfLast);
}

double doMin(int nXOffset, int nYOffset, double dfThisPrev, double dfLast,
             double dfLastPrev)
{
    double dfEdge = doEdge(nXOffset, nYOffset, dfThisPrev, dfLast, dfLastPrev);
    double dfDiagonal =
        doDiagonal(nXOffset, nYOffset, dfThisPrev, dfLast, dfLastPrev);
    return std::min(dfEdge, dfDiagonal);
}

double doMax(int nXOffset, int nYOffset, double dfThisPrev, double dfLast,
             double dfLastPrev)
{
    double dfEdge = doEdge(nXOffset, nYOffset, dfThisPrev, dfLast, dfLastPrev);
    double dfDiagonal =
        doDiagonal(nXOffset, nYOffset, dfThisPrev, dfLast, dfLastPrev);
    return std::max(dfEdge, dfDiagonal);
}

}  // unnamed namespace

/// Constructor - the viewshed algorithm executor
/// @param srcBand  Source raster band
/// @param sdBand  Standard-deviation raster band
/// @param dstBand  Destination raster band
/// @param nX  X position of observer
/// @param nY  Y position of observer
/// @param outExtent  Extent of output raster (relative to input)
/// @param curExtent  Extent of active raster.
/// @param opts  Configuration options.
/// @param progress  Reference to the progress tracker.
/// @param emitWarningIfNoData  Whether a warning must be emitted if an input
///                             pixel is at the nodata value.
ViewshedExecutor::ViewshedExecutor(GDALRasterBand &srcBand,
                                   GDALRasterBand &sdBand,
                                   GDALRasterBand &dstBand, int nX, int nY,
                                   const Window &outExtent,
                                   const Window &curExtent, const Options &opts,
                                   Progress &progress, bool emitWarningIfNoData)
    : m_pool(4), m_dummyBand(), m_srcBand(srcBand), m_sdBand(sdBand),
      m_dstBand(dstBand),
      // If the standard deviation band isn't a dummy band, we're in SD mode.
      m_hasSdBand(dynamic_cast<DummyBand *>(&m_sdBand) == nullptr),
      m_emitWarningIfNoData(emitWarningIfNoData), oOutExtent(outExtent),
      oCurExtent(curExtent), m_nX(nX - oOutExtent.xStart), m_nY(nY),
      oOpts(opts), oProgress(progress),
      m_dfMinDistance2(opts.minDistance * opts.minDistance),
      m_dfMaxDistance2(opts.maxDistance * opts.maxDistance)
{
    if (m_dfMaxDistance2 == 0)
        m_dfMaxDistance2 = std::numeric_limits<double>::max();
    if (opts.lowPitch != -90.0)
        m_lowTanPitch = std::tan(oOpts.lowPitch * (2 * M_PI / 360.0));
    if (opts.highPitch != 90.0)
        m_highTanPitch = std::tan(oOpts.highPitch * (2 * M_PI / 360.0));
    m_srcBand.GetDataset()->GetGeoTransform(m_gt);
    int hasNoData = false;
    m_noDataValue = m_srcBand.GetNoDataValue(&hasNoData);
    m_hasNoData = hasNoData;
}

/// Constructor - the viewshed algorithm executor
/// @param srcBand  Source raster band
/// @param dstBand  Destination raster band
/// @param nX  X position of observer
/// @param nY  Y position of observer
/// @param outExtent  Extent of output raster (relative to input)
/// @param curExtent  Extent of active raster.
/// @param opts  Configuration options.
/// @param progress  Reference to the progress tracker.
/// @param emitWarningIfNoData  Whether a warning must be emitted if an input
///                             pixel is at the nodata value.
ViewshedExecutor::ViewshedExecutor(GDALRasterBand &srcBand,
                                   GDALRasterBand &dstBand, int nX, int nY,
                                   const Window &outExtent,
                                   const Window &curExtent, const Options &opts,
                                   Progress &progress, bool emitWarningIfNoData)
    : ViewshedExecutor(srcBand, m_dummyBand, dstBand, nX, nY, outExtent,
                       curExtent, opts, progress, emitWarningIfNoData)
{
}

// calculate the height adjustment factor.
double ViewshedExecutor::calcHeightAdjFactor()
{
    std::lock_guard g(oMutex);

    const OGRSpatialReference *poDstSRS =
        m_dstBand.GetDataset()->GetSpatialRef();

    if (poDstSRS)
    {
        OGRErr eSRSerr;

        // If we can't get a SemiMajor axis from the SRS, it will be SRS_WGS84_SEMIMAJOR
        double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

        /* If we fetched the axis from the SRS, use it */
        if (eSRSerr != OGRERR_FAILURE)
            return oOpts.curveCoeff / (dfSemiMajor * 2.0);

        CPLDebug("GDALViewshedGenerate",
                 "Unable to fetch SemiMajor axis from spatial reference");
    }
    return 0;
}

/// Set the output Z value depending on the observable height and computation mode
/// in normal mode.
///
/// dfResult  Reference to the result cell
/// dfCellVal  Reference to the current cell height. Replace with observable height.
/// dfZ  Minimum observable height at cell.
void ViewshedExecutor::setOutputNormal(Lines &lines, int pos, double dfZ)
{
    double &cur = lines.cur[pos];
    double &result = lines.result[pos];

    if (oOpts.outputMode != OutputMode::Normal)
    {
        double adjustment = dfZ - cur;
        if (adjustment > 0)
            result += adjustment;
    }
    else
    {
        double cellHeight = cur + oOpts.targetHeight;
        result = (cellHeight < dfZ) ? oOpts.invisibleVal : oOpts.visibleVal;
    }
    cur = std::max(cur, dfZ);
}

/// Set the output Z value depending on the observable height and computation when
/// making an standard deviation pass.
///
/// dfResult  Reference to the result cell
/// dfCellVal  Reference to the current cell height. Replace with observable height.
/// dfZ  Minimum observable height at cell.
void ViewshedExecutor::setOutputSd(Lines &lines, int pos, double dfZ)
{
    double &cur = lines.cur[pos];
    double &result = lines.result[pos];
    double &sd = lines.sd[pos];

    assert(oOpts.outputMode == OutputMode::Normal);
    if (result == oOpts.invisibleVal)
    {
        double cellHeight = cur + oOpts.targetHeight;
        if (cellHeight > dfZ)
            result = oOpts.maybeVisibleVal;
    }

    if (sd <= 1)
        cur = std::max(dfZ, cur);
    else
        cur = dfZ;
}

/// Read a line of raster data.
///
/// @param  nLine  Line number to read.
/// @param  lines  Raster line to fill.
/// @return  Success or failure.
bool ViewshedExecutor::readLine(int nLine, Lines &lines)
{
    std::lock_guard g(iMutex);

    if (GDALRasterIO(&m_srcBand, GF_Read, oOutExtent.xStart, nLine,
                     oOutExtent.xSize(), 1, lines.cur.data(),
                     oOutExtent.xSize(), 1, GDT_Float64, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RasterIO error when reading DEM at position (%d,%d), "
                 "size (%d,%d)",
                 oOutExtent.xStart, nLine, oOutExtent.xSize(), 1);
        return false;
    }

    if (sdMode())
    {
        double nodata = m_sdBand.GetNoDataValue();
        CPLErr sdStatus = m_sdBand.RasterIO(
            GF_Read, oOutExtent.xStart, nLine, oOutExtent.xSize(), 1,
            lines.sd.data(), oOutExtent.xSize(), 1, GDT_Float64, 0, 0, nullptr);
        if (sdStatus != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when reading standard deviation band at "
                     "position (%d,%d), "
                     "size (%d,%d)",
                     oOutExtent.xStart, nLine, oOutExtent.xSize(), 1);
            return false;
        }
        // Set the standard deviation to 1000 if nodata is found.
        for (size_t i = 0; i < lines.sd.size(); ++i)
            if (lines.sd[i] == nodata)
                lines.sd[i] = 1000.0;
    }

    // Initialize the result line.
    // In DEM mode the base is the pre-adjustment value.  In ground mode the base is zero.
    if (oOpts.outputMode == OutputMode::DEM)
        lines.result = lines.cur;
    else if (oOpts.outputMode == OutputMode::Ground)
        std::fill(lines.result.begin(), lines.result.end(), 0);

    return true;
}

/// Write an output line of either visibility or height data.
///
/// @param  nLine  Line number being written.
/// @param vResult  Result line to write.
/// @return  True on success, false otherwise.
bool ViewshedExecutor::writeLine(int nLine, std::vector<double> &vResult)
{
    // GDALRasterIO isn't thread-safe.
    std::lock_guard g(oMutex);

    if (GDALRasterIO(&m_dstBand, GF_Write, 0, nLine - oOutExtent.yStart,
                     oOutExtent.xSize(), 1, vResult.data(), oOutExtent.xSize(),
                     1, GDT_Float64, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RasterIO error when writing target raster at position "
                 "(%d,%d), size (%d,%d)",
                 0, nLine - oOutExtent.yStart, oOutExtent.xSize(), 1);
        return false;
    }
    return true;
}

/// Adjust the height of the line of data by the observer height and the curvature of the
/// earth.
///
/// @param  nYOffset  Y offset of the line being adjusted.
/// @param  lines  Raster lines to adjust.
/// @return  Processing limits of the line based on min/max distance.
LineLimits ViewshedExecutor::adjustHeight(int nYOffset, Lines &lines)
{
    LineLimits ll(0, m_nX + 1, m_nX + 1, oCurExtent.xSize());

    // Find the starting point in the raster (m_nX may be outside)
    int nXStart = oCurExtent.clampX(m_nX);

    const auto CheckNoData = [this](double val)
    {
        if (!m_hasFoundNoData &&
            ((m_hasNoData && val == m_noDataValue) || std::isnan(val)))
        {
            m_hasFoundNoData = true;
            if (m_emitWarningIfNoData)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Nodata value found in input DEM. Output will be "
                         "likely incorrect");
            }
        }
    };

    // If there is a height adjustment factor other than zero or a max distance,
    // calculate the adjusted height of the cell, stopping if we've exceeded the max
    // distance.
    if (static_cast<bool>(m_dfHeightAdjFactor) || oOpts.pitchMasking() ||
        m_dfMaxDistance2 > 0 || m_dfMinDistance2 > 0)
    {
        // Hoist invariants from the loops.
        const double dfLineX = m_gt[2] * nYOffset;
        const double dfLineY = m_gt[5] * nYOffset;

        // Go left
        double *pdfHeight = lines.cur.data() + nXStart;
        for (int nXOffset = nXStart - m_nX; nXOffset >= -m_nX;
             nXOffset--, pdfHeight--)
        {
            double dfX = m_gt[1] * nXOffset + dfLineX;
            double dfY = m_gt[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;

            if (dfR2 < m_dfMinDistance2)
                ll.leftMin--;
            else if (dfR2 > m_dfMaxDistance2)
            {
                ll.left = nXOffset + m_nX + 1;
                break;
            }

            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfHeightAdjFactor * dfR2 + m_dfZObserver;
            if (oOpts.pitchMasking())
                calcPitchMask(*pdfHeight, std::sqrt(dfR2),
                              lines.result[m_nX + nXOffset],
                              lines.pitchMask[m_nX + nXOffset]);
        }

        // Go right.
        pdfHeight = lines.cur.data() + nXStart + 1;
        for (int nXOffset = nXStart - m_nX + 1;
             nXOffset < oCurExtent.xSize() - m_nX; nXOffset++, pdfHeight++)
        {
            double dfX = m_gt[1] * nXOffset + dfLineX;
            double dfY = m_gt[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;

            if (dfR2 < m_dfMinDistance2)
                ll.rightMin++;
            else if (dfR2 > m_dfMaxDistance2)
            {
                ll.right = nXOffset + m_nX;
                break;
            }

            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfHeightAdjFactor * dfR2 + m_dfZObserver;
            if (oOpts.pitchMasking())
                calcPitchMask(*pdfHeight, std::sqrt(dfR2),
                              lines.result[m_nX + nXOffset],
                              lines.pitchMask[m_nX + nXOffset]);
        }
    }
    else
    {
        // No curvature adjustment. Just normalize for the observer height.
        double *pdfHeight = lines.cur.data();
        for (int i = 0; i < oCurExtent.xSize(); ++i)
        {
            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfZObserver;
            pdfHeight++;
        }
    }
    return ll;
}

/// Calculate the pitch masking value to apply after running the viewshed algorithm.
///
/// @param  dfZ  Adjusted input height.
/// @param  dfDist  Distance from observer to cell.
/// @param  dfResult  Result value to which adjustment may be added.
/// @param  maskVal  Output mask value.
void ViewshedExecutor::calcPitchMask(double dfZ, double dfDist, double dfResult,
                                     double &maskVal)
{
    if (oOpts.lowPitchMasking())
    {
        double dfZMask = dfDist * m_lowTanPitch;
        double adjustment = dfZMask - dfZ;
        if (adjustment > 0)
        {
            maskVal = (oOpts.outputMode == OutputMode::Normal
                           ? std::numeric_limits<double>::infinity()
                           : adjustment + dfResult);
            return;
        }
    }
    if (oOpts.highPitchMasking())
    {
        double dfZMask = dfDist * m_highTanPitch;
        if (dfZ > dfZMask)
            maskVal = std::numeric_limits<double>::infinity();
    }
}

/// Process the first line (the one with the Y coordinate the same as the observer).
///
/// @param lines  Raster lines to process.
/// @return True on success, false otherwise.
bool ViewshedExecutor::processFirstLine(Lines &lines)
{
    int nLine = oOutExtent.clampY(m_nY);
    int nYOffset = nLine - m_nY;

    if (!readLine(nLine, lines))
        return false;

    // If the observer is outside of the raster, take the specified value as the Z height,
    // otherwise, take it as an offset from the raster height at that location.
    m_dfZObserver = oOpts.observer.z;
    if (oCurExtent.containsX(m_nX))
        m_dfZObserver += lines.cur[m_nX];

    LineLimits ll = adjustHeight(nYOffset, lines);

    std::vector<double> savedInput;
    if (sdMode())
        savedInput = lines.cur;

    if (oCurExtent.containsX(m_nX))
    {
        if (ll.leftMin != ll.rightMin)
            lines.result[m_nX] = oOpts.outOfRangeVal;
        else if (oOpts.outputMode == OutputMode::Normal)
            lines.result[m_nX] = oOpts.visibleVal;
    }

    auto process = [this, &ll, &lines](bool sdCalc)
    {
        if (!oCurExtent.containsY(m_nY))
            processFirstLineTopOrBottom(ll, lines);
        else
        {
            CPLJobQueuePtr pQueue = m_pool.CreateJobQueue();
            pQueue->SubmitJob([&]()
                              { processFirstLineLeft(ll, lines, sdCalc); });
            pQueue->SubmitJob([&]()
                              { processFirstLineRight(ll, lines, sdCalc); });
            pQueue->WaitCompletion();
        }
    };

    process(false);
    lines.prev = lines.cur;
    if (sdMode())
    {
        lines.cur = std::move(savedInput);
        process(true);
        lines.prevTmp = lines.cur;
    }

    if (oOpts.pitchMasking())
        applyPitchMask(lines.result, lines.pitchMask);
    if (!writeLine(nLine, lines.result))
        return false;

    return oProgress.lineComplete();
}

/// Set the pitch masked value into the result vector when applicable.
///
/// @param  vResult  Result vector.
/// @param  vPitchMaskVal  Pitch mask values (nan is no masking, inf is out-of-range, else
///                        actual value).
void ViewshedExecutor::applyPitchMask(std::vector<double> &vResult,
                                      const std::vector<double> &vPitchMaskVal)
{
    for (size_t i = 0; i < vResult.size(); ++i)
    {
        if (std::isnan(vPitchMaskVal[i]))
            continue;
        if (std::isinf(vPitchMaskVal[i]))
            vResult[i] = oOpts.outOfRangeVal;
        else
            vResult[i] = vPitchMaskVal[i];
    }
}

/// If the observer is above or below the raster, set all cells in the first line near the
/// observer as observable provided they're in range. Mark cells out of range as such.
/// @param  ll  Line limits for processing.
/// @param  lines  Raster lines to process.
void ViewshedExecutor::processFirstLineTopOrBottom(const LineLimits &ll,
                                                   Lines &lines)
{
    for (int iPixel = ll.left; iPixel < ll.right; ++iPixel)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            lines.result[iPixel] = oOpts.visibleVal;
        else
            setOutputNormal(lines, iPixel, lines.cur[iPixel]);
    }

    std::fill(lines.result.begin(), lines.result.begin() + ll.left,
              oOpts.outOfRangeVal);
    std::fill(lines.result.begin() + ll.right,
              lines.result.begin() + oCurExtent.xStop, oOpts.outOfRangeVal);
}

/// Process the part of the first line to the left of the observer.
///
/// @param ll      Line limits for masking.
/// @param sdCalc  True when doing standard deviation calculation.
/// @param lines   Raster lines to process.
void ViewshedExecutor::processFirstLineLeft(const LineLimits &ll, Lines &lines,
                                            bool sdCalc)
{
    int iEnd = ll.left - 1;
    int iStart = m_nX - 1;  // One left of the observer.

    // If end is to the right of start, everything is taken care of by right processing.
    if (iEnd >= iStart)
    {
        maskLineLeft(lines.result, ll, m_nY);
        return;
    }

    iStart = oCurExtent.clampX(iStart);

    // If the start cell is next to the observer, just mark it visible.
    if (iStart + 1 == m_nX || iStart + 1 == oCurExtent.xStop)
    {
        double dfZ = lines.cur[iStart];
        if (oOpts.outputMode == OutputMode::Normal)
        {
            lines.result[iStart] = oOpts.visibleVal;
            if (sdCalc)
                if (lines.sd[iStart] > 1)
                    lines.cur[iStart] =
                        m_dfZObserver;  // Should this be a minimum value?
        }
        else
            setOutputNormal(lines, iStart, dfZ);
        iStart--;
    }

    // Go from the observer to the left, calculating Z as we go.
    for (int iPixel = iStart; iPixel > iEnd; iPixel--)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ = CalcHeightLine(nXOffset, lines.cur[iPixel + 1]);
        if (!sdCalc)
            setOutputNormal(lines, iPixel, dfZ);
        else
            setOutputSd(lines, iPixel, dfZ);
    }

    maskLineLeft(lines.result, ll, m_nY);
}

/// Mask cells based on angle intersection to the left of the observer.
///
/// @param vResult  Result raaster line.
/// @param nLine  Line number.
/// @return  True when all cells have been masked.
bool ViewshedExecutor::maskAngleLeft(std::vector<double> &vResult, int nLine)
{
    auto clamp = [this](int x)
    { return (x < 0 || x >= m_nX) ? INVALID_ISECT : x; };

    if (!oOpts.angleMasking())
        return false;

    if (nLine != m_nY)
    {
        int startAngleX =
            clamp(hIntersect(oOpts.startAngle, m_nX, m_nY, nLine));
        int endAngleX = clamp(hIntersect(oOpts.endAngle, m_nX, m_nY, nLine));
        // If neither X intersect is in the quadrant and a ray in the quadrant isn't
        // between start and stop, fill it all and return true.  If it is in between
        // start and stop, we're done.
        if (invalid(startAngleX) && invalid(endAngleX))
        {
            // Choose a test angle in quadrant II or III depending on the line.
            double testAngle = nLine < m_nY ? m_testAngle[2] : m_testAngle[3];
            if (!rayBetween(oOpts.startAngle, oOpts.endAngle, testAngle))
            {
                std::fill(vResult.begin(), vResult.begin() + m_nX,
                          oOpts.outOfRangeVal);
                return true;
            }
            return false;
        }
        if (nLine > m_nY)
            std::swap(startAngleX, endAngleX);
        if (invalid(startAngleX))
            startAngleX = 0;
        if (invalid(endAngleX))
            endAngleX = m_nX - 1;
        if (startAngleX <= endAngleX)
        {
            std::fill(vResult.begin(), vResult.begin() + startAngleX,
                      oOpts.outOfRangeVal);
            std::fill(vResult.begin() + endAngleX + 1, vResult.begin() + m_nX,
                      oOpts.outOfRangeVal);
        }
        else
        {
            std::fill(vResult.begin() + endAngleX + 1,
                      vResult.begin() + startAngleX, oOpts.outOfRangeVal);
        }
    }
    // nLine == m_nY
    else if (!rayBetween(oOpts.startAngle, oOpts.endAngle, M_PI))
    {
        std::fill(vResult.begin(), vResult.begin() + m_nX, oOpts.outOfRangeVal);
        return true;
    }
    return false;
}

/// Mask cells based on angle intersection to the right of the observer.
///
/// @param vResult  Result raaster line.
/// @param nLine  Line number.
/// @return  True when all cells have been masked.
bool ViewshedExecutor::maskAngleRight(std::vector<double> &vResult, int nLine)
{
    int lineLength = static_cast<int>(vResult.size());

    auto clamp = [this, lineLength](int x)
    { return (x <= m_nX || x >= lineLength) ? INVALID_ISECT : x; };

    if (oOpts.startAngle == oOpts.endAngle)
        return false;

    if (nLine != m_nY)
    {
        int startAngleX =
            clamp(hIntersect(oOpts.startAngle, m_nX, m_nY, nLine));
        int endAngleX = clamp(hIntersect(oOpts.endAngle, m_nX, m_nY, nLine));

        // If neither X intersect is in the quadrant and a ray in the quadrant isn't
        // between start and stop, fill it all and return true.  If it is in between
        // start and stop, we're done.
        if (invalid(startAngleX) && invalid(endAngleX))
        {
            // Choose a test angle in quadrant I or IV depending on the line.
            double testAngle = nLine < m_nY ? m_testAngle[1] : m_testAngle[4];
            if (!rayBetween(oOpts.startAngle, oOpts.endAngle, testAngle))
            {
                std::fill(vResult.begin() + m_nX + 1, vResult.end(),
                          oOpts.outOfRangeVal);
                return true;
            }
            return false;
        }

        if (nLine > m_nY)
            std::swap(startAngleX, endAngleX);
        if (invalid(endAngleX))
            endAngleX = lineLength - 1;
        if (invalid(startAngleX))
            startAngleX = m_nX + 1;
        if (startAngleX <= endAngleX)
        {
            std::fill(vResult.begin() + m_nX + 1, vResult.begin() + startAngleX,
                      oOpts.outOfRangeVal);
            std::fill(vResult.begin() + endAngleX + 1, vResult.end(),
                      oOpts.outOfRangeVal);
        }
        else
        {
            std::fill(vResult.begin() + endAngleX + 1,
                      vResult.begin() + startAngleX, oOpts.outOfRangeVal);
        }
    }
    // nLine == m_nY
    else if (!rayBetween(oOpts.startAngle, oOpts.endAngle, 0))
    {
        std::fill(vResult.begin() + m_nX + 1, vResult.end(),
                  oOpts.outOfRangeVal);
        return true;
    }
    return false;
}

/// Perform angle and min/max masking to the left of the observer.
///
/// @param vResult  Raster line to mask.
/// @param ll  Min/max line limits.
/// @param nLine  Line number.
void ViewshedExecutor::maskLineLeft(std::vector<double> &vResult,
                                    const LineLimits &ll, int nLine)
{
    // If we've already masked with angles everything, just return.
    if (maskAngleLeft(vResult, nLine))
        return;

    // Mask cells from the left edge to the left limit.
    std::fill(vResult.begin(), vResult.begin() + ll.left, oOpts.outOfRangeVal);
    // Mask cells from the left min to the observer.
    if (ll.leftMin < m_nX)
        std::fill(vResult.begin() + ll.leftMin, vResult.begin() + m_nX,
                  oOpts.outOfRangeVal);
}

/// Perform angle and min/max masking to the right of the observer.
///
/// @param vResult  Raster line to mask.
/// @param ll  Min/max line limits.
/// @param nLine  Line number.
void ViewshedExecutor::maskLineRight(std::vector<double> &vResult,
                                     const LineLimits &ll, int nLine)
{
    // If we've already masked with angles everything, just return.
    if (maskAngleRight(vResult, nLine))
        return;

    // Mask cells from the observer to right min.
    std::fill(vResult.begin() + m_nX + 1, vResult.begin() + ll.rightMin,
              oOpts.outOfRangeVal);
    // Mask cells from the right limit to the right edge.

    //
    //ABELL - Changed from ll.right + 1
    //
    if (ll.right <= static_cast<int>(vResult.size()))
        std::fill(vResult.begin() + ll.right, vResult.end(),
                  oOpts.outOfRangeVal);
}

/// Process the part of the first line to the right of the observer.
///
/// @param ll  Line limits
/// @param sdCalc  True when doing standard deviation calcuation.
/// @param lines  Raster lines to process.
void ViewshedExecutor::processFirstLineRight(const LineLimits &ll, Lines &lines,
                                             bool sdCalc)
{
    int iStart = m_nX + 1;
    int iEnd = ll.right;

    // If start is to the right of end, everything is taken care of by left processing.
    if (iStart >= iEnd)
    {
        maskLineRight(lines.result, ll, m_nY);
        return;
    }

    iStart = oCurExtent.clampX(iStart);

    // If the start cell is next to the observer, just mark it visible.
    if (iStart - 1 == m_nX || iStart == oCurExtent.xStart)
    {
        double dfZ = lines.cur[iStart];
        if (oOpts.outputMode == OutputMode::Normal)
        {
            lines.result[iStart] = oOpts.visibleVal;
            if (sdCalc)
                if (lines.sd[iStart] > 1)
                    lines.cur[iStart] =
                        m_dfZObserver;  // Use some minimum value instead?
        }
        else
            setOutputNormal(lines, iStart, dfZ);
        iStart++;
    }

    // Go from the observer to the right, calculating Z as we go.
    for (int iPixel = iStart; iPixel < iEnd; iPixel++)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ = CalcHeightLine(nXOffset, lines.cur[iPixel - 1]);
        if (!sdCalc)
            setOutputNormal(lines, iPixel, dfZ);
        else
            setOutputSd(lines, iPixel, dfZ);
    }

    maskLineRight(lines.result, ll, m_nY);
}

/// Process a line to the left of the observer.
///
/// @param nYOffset  Offset of the line being processed from the observer
/// @param ll  Line limits
/// @param lines  Raster lines to process.
/// @param sdCalc  standard deviation calculation indicator.
void ViewshedExecutor::processLineLeft(int nYOffset, LineLimits &ll,
                                       Lines &lines, bool sdCalc)
{
    int iStart = m_nX - 1;
    int iEnd = ll.left - 1;
    int nLine = m_nY + nYOffset;

    // If start to the left of end, everything is taken care of by processing right.
    if (iStart <= iEnd)
    {
        maskLineLeft(lines.result, ll, nLine);
        return;
    }
    iStart = oCurExtent.clampX(iStart);

    // If the observer is to the right of the raster, mark the first cell to the left as
    // visible. This may mark an out-of-range cell with a value, but this will be fixed
    // with the out of range assignment at the end.
    if (iStart == oCurExtent.xStop - 1)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            lines.result[iStart] = oOpts.visibleVal;
        else
            setOutputNormal(lines, iStart, lines.cur[iStart]);
        iStart--;
    }

    // Go from the observer to the left, calculating Z as we go.
    nYOffset = std::abs(nYOffset);
    for (int iPixel = iStart; iPixel > iEnd; iPixel--)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ;
        if (nXOffset == nYOffset)
            dfZ = CalcHeightLine(nYOffset, lines.cur[iPixel],
                                 lines.prev[iPixel + 1]);
        else
            dfZ = oZcalc(nXOffset, nYOffset, lines.cur[iPixel + 1],
                         lines.prev[iPixel], lines.prev[iPixel + 1]);
        if (!sdCalc)
            setOutputNormal(lines, iPixel, dfZ);
        else
            setOutputSd(lines, iPixel, dfZ);
    }

    maskLineLeft(lines.result, ll, nLine);
}

/// Process a line to the right of the observer.
///
/// @param nYOffset  Offset of the line being processed from the observer
/// @param ll  Line limits
/// @param lines  Raster lines to process.
/// @param sdCalc  standard deviation calculation indicator.
void ViewshedExecutor::processLineRight(int nYOffset, LineLimits &ll,
                                        Lines &lines, bool sdCalc)
{
    int iStart = m_nX + 1;
    int iEnd = ll.right;
    int nLine = m_nY + nYOffset;

    // If start is to the right of end, everything is taken care of by processing left.
    if (iStart >= iEnd)
    {
        maskLineRight(lines.result, ll, nLine);
        return;
    }
    iStart = oCurExtent.clampX(iStart);

    // If the observer is to the left of the raster, mark the first cell to the right as
    // visible. This may mark an out-of-range cell with a value, but this will be fixed
    // with the out of range assignment at the end.
    if (iStart == 0)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            lines.result[iStart] = oOpts.visibleVal;
        else
            setOutputNormal(lines, 0, lines.cur[0]);
        iStart++;
    }

    // Go from the observer to the right, calculating Z as we go.
    nYOffset = std::abs(nYOffset);
    for (int iPixel = iStart; iPixel < iEnd; iPixel++)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ;
        if (nXOffset == nYOffset)
        {
            if (sdCalc && nXOffset == 1)
            {
                lines.result[iPixel] = oOpts.visibleVal;
                if (lines.sd[iPixel] > 1)
                    lines.cur[iPixel] = m_dfZObserver;
                continue;
            }
            dfZ = CalcHeightLine(nYOffset, lines.cur[iPixel],
                                 lines.prev[iPixel - 1]);
        }
        else
            dfZ = oZcalc(nXOffset, nYOffset, lines.cur[iPixel - 1],
                         lines.prev[iPixel], lines.prev[iPixel - 1]);
        if (!sdCalc)
            setOutputNormal(lines, iPixel, dfZ);
        else
            setOutputSd(lines, iPixel, dfZ);
    }

    maskLineRight(lines.result, ll, nLine);
}

/// Apply angular/distance mask to the initial X position.  Assumes m_nX is in the raster.
/// @param vResult  Raster line on which to apply mask.
/// @param ll  Line limits.
/// @param nLine  Line number.
/// @return True if the initial X position was masked.
bool ViewshedExecutor::maskInitial(std::vector<double> &vResult,
                                   const LineLimits &ll, int nLine)
{
    // Mask min/max.
    if (ll.left >= ll.right || ll.leftMin != ll.rightMin)
    {
        vResult[m_nX] = oOpts.outOfRangeVal;
        return true;
    }

    if (!oOpts.angleMasking())
        return false;

    if (nLine < m_nY)
    {
        if (!rayBetween(oOpts.startAngle, oOpts.endAngle, M_PI / 2))
        {
            vResult[m_nX] = oOpts.outOfRangeVal;
            return true;
        }
    }
    else if (nLine > m_nY)
    {
        if (!rayBetween(oOpts.startAngle, oOpts.endAngle, 3 * M_PI / 2))
        {
            vResult[m_nX] = oOpts.outOfRangeVal;
            return true;
        }
    }
    return false;
}

/// Process a line above or below the observer.
///
/// @param nLine  Line number being processed.
/// @param lines  Raster lines to process.
/// @return True on success, false otherwise.
bool ViewshedExecutor::processLine(int nLine, Lines &lines)
{
    int nYOffset = nLine - m_nY;

    if (!readLine(nLine, lines))
        return false;

    // Adjust height of the read line.
    LineLimits ll = adjustHeight(nYOffset, lines);

    std::vector<double> savedLine;
    if (sdMode())
        savedLine = lines.cur;

    auto process = [this, nYOffset, &ll, &lines](bool sdCalc)
    {
        CPLJobQueuePtr pQueue = m_pool.CreateJobQueue();
        pQueue->SubmitJob([&]()
                          { processLineLeft(nYOffset, ll, lines, sdCalc); });
        pQueue->SubmitJob([&]()
                          { processLineRight(nYOffset, ll, lines, sdCalc); });
        pQueue->WaitCompletion();
    };

    bool masked = false;
    // Handle initial position on the line.
    if (oCurExtent.containsX(m_nX))
    {
        masked = maskInitial(lines.result, ll, nLine);
        if (!masked)
        {
            double dfZ = CalcHeightLine(std::abs(nYOffset), lines.cur[m_nX],
                                        lines.prev[m_nX]);
            setOutputNormal(lines, m_nX, dfZ);
        }
    }

    process(false);

    // Process standard deviation mode
    if (sdMode())
    {
        lines.prev = std::move(lines.prevTmp);
        lines.prevTmp = std::move(lines.cur);
        lines.cur = std::move(savedLine);
        // Handle initial position on the line.
        if (!masked && oCurExtent.containsX(m_nX))
        {
            if (std::abs(nYOffset) == 1)
            {
                lines.result[m_nX] = oOpts.visibleVal;
                if (lines.sd[m_nX] > 1)
                    lines.cur[m_nX] = m_dfZObserver;
            }
            else
            {

                double dfZ = CalcHeightLine(std::abs(nYOffset), lines.cur[m_nX],
                                            lines.prev[m_nX]);
                setOutputSd(lines, m_nX, dfZ);
            }
        }
        process(true);
        lines.prev = std::move(lines.prevTmp);
        lines.prevTmp = lines.cur;
    }
    else
        lines.prev = lines.cur;

    if (oOpts.pitchMasking())
        applyPitchMask(lines.result, lines.pitchMask);
    if (!writeLine(nLine, lines.result))
        return false;

    return oProgress.lineComplete();
}

// Calculate the ray angle from the origin to middle of the top or bottom
// of each quadrant.
void ViewshedExecutor::calcTestAngles()
{
    // Quadrant 1.
    {
        int ysize = m_nY + 1;
        int xsize = oCurExtent.xStop - m_nX;
        m_testAngle[1] = atan2(ysize, xsize / 2.0);
    }

    // Quadrant 2.
    {
        int ysize = m_nY + 1;
        int xsize = m_nX + 1;
        m_testAngle[2] = atan2(ysize, -xsize / 2.0);
    }

    // Quadrant 3.
    {
        int ysize = oCurExtent.yStop - m_nY;
        int xsize = m_nX + 1;
        m_testAngle[3] = atan2(-ysize, -xsize / 2.0);
    }

    // Quadrant 4.
    {
        int ysize = oCurExtent.yStop - m_nY;
        int xsize = oCurExtent.xStop - m_nX;
        m_testAngle[4] = atan2(-ysize, xsize / 2.0);
    }

    // Adjust range to [0, 2 * M_PI)
    for (int i = 1; i <= 4; ++i)
        if (m_testAngle[i] < 0)
            m_testAngle[i] += (2 * M_PI);
}

/// Run the viewshed computation
/// @return  Success as true or false.
bool ViewshedExecutor::run()
{
    // If we're doing angular masking, calculate the test angles used later.
    if (oOpts.angleMasking())
        calcTestAngles();

    Lines firstLine(oCurExtent.xSize());
    if (oOpts.pitchMasking())
        firstLine.pitchMask.resize(oOutExtent.xSize(),
                                   std::numeric_limits<double>::quiet_NaN());
    if (sdMode())
        firstLine.sd.resize(oOutExtent.xSize());

    m_dfHeightAdjFactor = calcHeightAdjFactor();

    if (!processFirstLine(firstLine))
        return false;

    if (oOpts.cellMode == CellMode::Edge)
        oZcalc = doEdge;
    else if (oOpts.cellMode == CellMode::Diagonal)
        oZcalc = doDiagonal;
    else if (oOpts.cellMode == CellMode::Min)
        oZcalc = doMin;
    else if (oOpts.cellMode == CellMode::Max)
        oZcalc = doMax;

    // scan upwards
    int yStart = oCurExtent.clampY(m_nY);
    std::atomic<bool> err(false);
    CPLJobQueuePtr pQueue = m_pool.CreateJobQueue();
    pQueue->SubmitJob(
        [&]()
        {
            Lines lines(oCurExtent.xSize());
            lines.prev = firstLine.prev;
            lines.prevTmp = firstLine.prevTmp;
            if (oOpts.pitchMasking())
                lines.pitchMask.resize(
                    oOutExtent.xSize(),
                    std::numeric_limits<double>::quiet_NaN());
            if (sdMode())
                lines.sd.resize(oOutExtent.xSize());

            for (int nLine = yStart - 1; nLine >= oCurExtent.yStart && !err;
                 nLine--)
            {
                if (!processLine(nLine, lines))
                    err = true;
                if (oOpts.pitchMasking())
                    std::fill(lines.pitchMask.begin(), lines.pitchMask.end(),
                              std::numeric_limits<double>::quiet_NaN());
            }
        });

    // scan downwards
    pQueue->SubmitJob(
        [&]()
        {
            Lines lines(oCurExtent.xSize());
            lines.prev = firstLine.prev;
            lines.prevTmp = firstLine.prevTmp;
            if (oOpts.pitchMasking())
                lines.pitchMask.resize(
                    oOutExtent.xSize(),
                    std::numeric_limits<double>::quiet_NaN());
            if (sdMode())
                lines.sd.resize(oOutExtent.xSize());

            for (int nLine = yStart + 1; nLine < oCurExtent.yStop && !err;
                 nLine++)
            {
                if (!processLine(nLine, lines))
                    err = true;
                if (oOpts.pitchMasking())
                    std::fill(lines.pitchMask.begin(), lines.pitchMask.end(),
                              std::numeric_limits<double>::quiet_NaN());
            }
        });
    return true;
}

}  // namespace viewshed
}  // namespace gdal

// cppcheck-suppress-end knownConditionTrueFalse
