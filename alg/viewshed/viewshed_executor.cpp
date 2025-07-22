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

namespace gdal
{
namespace viewshed
{

namespace
{

/// Calculate the height at nDistance units along a line through the origin given the height
/// at nDistance - 1 units along the line.
/// \param nDistance  Distance along the line for the target point.
/// \param Za  Height at the line one unit previous to the target point.
double CalcHeightLine(int nDistance, double Za)
{
    nDistance = std::abs(nDistance);
    assert(nDistance != 1);
    return Za * nDistance / (nDistance - 1);
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
    : m_pool(4), m_srcBand(srcBand), m_dstBand(dstBand),
      m_emitWarningIfNoData(emitWarningIfNoData), oOutExtent(outExtent),
      oCurExtent(curExtent), m_nX(nX - oOutExtent.xStart), m_nY(nY),
      oOpts(opts), oProgress(progress),
      m_dfMaxDistance2(opts.maxDistance * opts.maxDistance)
{
    if (m_dfMaxDistance2 == 0)
        m_dfMaxDistance2 = std::numeric_limits<double>::max();
    m_srcBand.GetDataset()->GetGeoTransform(m_adfTransform.data());
    int hasNoData = false;
    m_noDataValue = m_srcBand.GetNoDataValue(&hasNoData);
    m_hasNoData = hasNoData;
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

/// Set the output Z value depending on the observable height and computation mode.
///
/// dfResult  Reference to the result cell
/// dfCellVal  Reference to the current cell height. Replace with observable height.
/// dfZ  Minimum observable height at cell.
void ViewshedExecutor::setOutput(double &dfResult, double &dfCellVal,
                                 double dfZ)
{
    if (oOpts.outputMode != OutputMode::Normal)
    {
        double adjustment = dfZ - dfCellVal;
        if (adjustment > 0)
            dfResult += adjustment;
    }
    else
        dfResult = (dfCellVal + oOpts.targetHeight < dfZ) ? oOpts.invisibleVal
                                                          : oOpts.visibleVal;
    dfCellVal = std::max(dfCellVal, dfZ);
}

/// Read a line of raster data.
///
/// @param  nLine  Line number to read.
/// @param  data  Pointer to location in which to store data.
/// @return  Success or failure.
bool ViewshedExecutor::readLine(int nLine, double *data)
{
    std::lock_guard g(iMutex);

    if (GDALRasterIO(&m_srcBand, GF_Read, oOutExtent.xStart, nLine,
                     oOutExtent.xSize(), 1, data, oOutExtent.xSize(), 1,
                     GDT_Float64, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RasterIO error when reading DEM at position (%d,%d), "
                 "size (%d,%d)",
                 oOutExtent.xStart, nLine, oOutExtent.xSize(), 1);
        return false;
    }
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
/// @param  vThisLineVal  Line height data.
/// @return [left, right)  Leftmost and one past the rightmost cell in the line within
///    the max distance. Indices are limited to the raster extent (right may be just
///    outside the raster).
std::pair<int, int>
ViewshedExecutor::adjustHeight(int nYOffset, std::vector<double> &vThisLineVal)
{
    int nLeft = 0;
    int nRight = oCurExtent.xSize();

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
    if (static_cast<bool>(m_dfHeightAdjFactor) || m_dfMaxDistance2 > 0)
    {
        // Hoist invariants from the loops.
        const double dfLineX = m_adfTransform[2] * nYOffset;
        const double dfLineY = m_adfTransform[5] * nYOffset;

        // Go left
        double *pdfHeight = vThisLineVal.data() + nXStart;
        for (int nXOffset = nXStart - m_nX; nXOffset >= -m_nX;
             nXOffset--, pdfHeight--)
        {
            double dfX = m_adfTransform[1] * nXOffset + dfLineX;
            double dfY = m_adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > m_dfMaxDistance2)
            {
                nLeft = nXOffset + m_nX + 1;
                break;
            }
            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfHeightAdjFactor * dfR2 + m_dfZObserver;
        }

        // Go right.
        pdfHeight = vThisLineVal.data() + nXStart + 1;
        for (int nXOffset = nXStart - m_nX + 1;
             nXOffset < oCurExtent.xSize() - m_nX; nXOffset++, pdfHeight++)
        {
            double dfX = m_adfTransform[1] * nXOffset + dfLineX;
            double dfY = m_adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > m_dfMaxDistance2)
            {
                nRight = nXOffset + m_nX;
                break;
            }
            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfHeightAdjFactor * dfR2 + m_dfZObserver;
        }
    }
    else
    {
        // No curvature adjustment. Just normalize for the observer height.
        double *pdfHeight = vThisLineVal.data();
        for (int i = 0; i < oCurExtent.xSize(); ++i)
        {
            CheckNoData(*pdfHeight);
            *pdfHeight -= m_dfZObserver;
            pdfHeight++;
        }
    }
    return {nLeft, nRight};
}

/// Process the first line (the one with the Y coordinate the same as the observer).
///
/// @param vLastLineVal  Vector in which to store the read line. Becomes the last line
///    in further processing.
/// @return True on success, false otherwise.
bool ViewshedExecutor::processFirstLine(std::vector<double> &vLastLineVal)
{
    int nLine = oOutExtent.clampY(m_nY);
    int nYOffset = nLine - m_nY;

    std::vector<double> vResult(oOutExtent.xSize());
    std::vector<double> vThisLineVal(oOutExtent.xSize());

    if (!readLine(nLine, vThisLineVal.data()))
        return false;

    // If the observer is outside of the raster, take the specified value as the Z height,
    // otherwise, take it as an offset from the raster height at that location.
    m_dfZObserver = oOpts.observer.z;
    if (oCurExtent.containsX(m_nX))
    {
        m_dfZObserver += vThisLineVal[m_nX];
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[m_nX] = oOpts.visibleVal;
    }
    m_dfHeightAdjFactor = calcHeightAdjFactor();

    // In DEM mode the base is the pre-adjustment value.  In ground mode the base is zero.
    if (oOpts.outputMode == OutputMode::DEM)
        vResult = vThisLineVal;

    // iLeft and iRight are the processing limits for the line.
    const auto [iLeft, iRight] = adjustHeight(nYOffset, vThisLineVal);

    if (!oCurExtent.containsY(m_nY))
        processFirstLineTopOrBottom(iLeft, iRight, vResult, vThisLineVal);
    else
    {
        CPLJobQueuePtr pQueue = m_pool.CreateJobQueue();
        pQueue->SubmitJob(
            [&, left = iLeft]() {
                processFirstLineLeft(m_nX - 1, left - 1, vResult, vThisLineVal);
            });

        pQueue->SubmitJob(
            [&, right = iRight]()
            { processFirstLineRight(m_nX + 1, right, vResult, vThisLineVal); });
        pQueue->WaitCompletion();
    }

    // Make the current line the last line.
    vLastLineVal = std::move(vThisLineVal);

    // Create the output writer.
    if (!writeLine(nLine, vResult))
        return false;

    return oProgress.lineComplete();
}

// If the observer is above or below the raster, set all cells in the first line near the
// observer as observable provided they're in range. Mark cells out of range as such.
/// @param  iLeft  Leftmost observable raster position in range of the target line.
/// @param  iRight One past the rightmost observable raster position of the target line.
/// @param  vResult  Result line.
/// @param  vThisLineVal  Heights of the cells in the target line
void ViewshedExecutor::processFirstLineTopOrBottom(
    int iLeft, int iRight, std::vector<double> &vResult,
    std::vector<double> &vThisLineVal)
{
    double *pResult = vResult.data() + iLeft;
    double *pThis = vThisLineVal.data() + iLeft;
    for (int iPixel = iLeft; iPixel < iRight; ++iPixel, ++pResult, pThis++)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            *pResult = oOpts.visibleVal;
        else
            setOutput(*pResult, *pThis, *pThis);
    }
    std::fill(vResult.begin(), vResult.begin() + iLeft, oOpts.outOfRangeVal);
    std::fill(vResult.begin() + iRight, vResult.begin() + oCurExtent.xStop,
              oOpts.outOfRangeVal);
}

/// Process the part of the first line to the left of the observer.
///
/// @param iStart  X coordinate of the first cell to the left of the observer to be procssed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
void ViewshedExecutor::processFirstLineLeft(int iStart, int iEnd,
                                            std::vector<double> &vResult,
                                            std::vector<double> &vThisLineVal)
{
    // If end is to the right of start, everything is taken care of by right processing.
    if (iEnd >= iStart)
        return;

    iStart = oCurExtent.clampX(iStart);

    double *pThis = vThisLineVal.data() + iStart;

    // If the start cell is next to the observer, just mark it visible.
    if (iStart + 1 == m_nX || iStart + 1 == oCurExtent.xStop)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[iStart] = oOpts.visibleVal;
        else
            setOutput(vResult[iStart], *pThis, *pThis);
        iStart--;
        pThis--;
    }

    // Go from the observer to the left, calculating Z as we go.
    for (int iPixel = iStart; iPixel > iEnd; iPixel--, pThis--)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ = CalcHeightLine(nXOffset, *(pThis + 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }
    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin(), vResult.begin() + iEnd + 1, oOpts.outOfRangeVal);
}

/// Process the part of the first line to the right of the observer.
///
/// @param iStart  X coordinate of the first cell to the right of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
void ViewshedExecutor::processFirstLineRight(int iStart, int iEnd,
                                             std::vector<double> &vResult,
                                             std::vector<double> &vThisLineVal)
{
    // If start is to the right of end, everything is taken care of by left processing.
    if (iStart >= iEnd)
        return;

    iStart = oCurExtent.clampX(iStart);

    double *pThis = vThisLineVal.data() + iStart;

    // If the start cell is next to the observer, just mark it visible.
    if (iStart - 1 == m_nX || iStart == oCurExtent.xStart)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[iStart] = oOpts.visibleVal;
        else
            setOutput(vResult[iStart], *pThis, *pThis);
        iStart++;
        pThis++;
    }

    // Go from the observer to the right, calculating Z as we go.
    for (int iPixel = iStart; iPixel < iEnd; iPixel++, pThis++)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ = CalcHeightLine(nXOffset, *(pThis - 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }
    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin() + iEnd, vResult.end(), oOpts.outOfRangeVal);
}

/// Process a line to the left of the observer.
///
/// @param nYOffset  Offset of the line being processed from the observer
/// @param iStart  X coordinate of the first cell to the left of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
/// @param vLastLineVal  Observable height of each cell in the previous line processed.
void ViewshedExecutor::processLineLeft(int nYOffset, int iStart, int iEnd,
                                       std::vector<double> &vResult,
                                       std::vector<double> &vThisLineVal,
                                       std::vector<double> &vLastLineVal)
{
    // If start to the left of end, everything is taken care of by processing right.
    if (iStart <= iEnd)
        return;
    iStart = oCurExtent.clampX(iStart);

    nYOffset = std::abs(nYOffset);
    double *pThis = vThisLineVal.data() + iStart;
    double *pLast = vLastLineVal.data() + iStart;

    // If the observer is to the right of the raster, mark the first cell to the left as
    // visible. This may mark an out-of-range cell with a value, but this will be fixed
    // with the out of range assignment at the end.
    if (iStart == oCurExtent.xStop - 1)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[iStart] = oOpts.visibleVal;
        else
            setOutput(vResult[iStart], *pThis, *pThis);
        iStart--;
        pThis--;
        pLast--;
    }

    // Go from the observer to the left, calculating Z as we go.
    for (int iPixel = iStart; iPixel > iEnd; iPixel--, pThis--, pLast--)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ;
        if (nXOffset == nYOffset)
        {
            if (nXOffset == 1)
                dfZ = *pThis;
            else
                dfZ = CalcHeightLine(nXOffset, *(pLast + 1));
        }
        else
            dfZ =
                oZcalc(nXOffset, nYOffset, *(pThis + 1), *pLast, *(pLast + 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }

    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin(), vResult.begin() + iEnd + 1, oOpts.outOfRangeVal);
}

/// Process a line to the right of the observer.
///
/// @param nYOffset  Offset of the line being processed from the observer
/// @param iStart  X coordinate of the first cell to the right of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
/// @param vLastLineVal  Observable height of each cell in the previous line processed.
void ViewshedExecutor::processLineRight(int nYOffset, int iStart, int iEnd,
                                        std::vector<double> &vResult,
                                        std::vector<double> &vThisLineVal,
                                        std::vector<double> &vLastLineVal)
{
    // If start is to the right of end, everything is taken care of by processing left.
    if (iStart >= iEnd)
        return;
    iStart = oCurExtent.clampX(iStart);

    nYOffset = std::abs(nYOffset);
    double *pThis = vThisLineVal.data() + iStart;
    double *pLast = vLastLineVal.data() + iStart;

    // If the observer is to the left of the raster, mark the first cell to the right as
    // visible. This may mark an out-of-range cell with a value, but this will be fixed
    // with the out of range assignment at the end.
    if (iStart == 0)
    {
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[iStart] = oOpts.visibleVal;
        else
            setOutput(vResult[0], *pThis, *pThis);
        iStart++;
        pThis++;
        pLast++;
    }

    // Go from the observer to the right, calculating Z as we go.
    for (int iPixel = iStart; iPixel < iEnd; iPixel++, pThis++, pLast++)
    {
        int nXOffset = std::abs(iPixel - m_nX);
        double dfZ;
        if (nXOffset == nYOffset)
        {
            if (nXOffset == 1)
                dfZ = *pThis;
            else
                dfZ = CalcHeightLine(nXOffset, *(pLast - 1));
        }
        else
            dfZ =
                oZcalc(nXOffset, nYOffset, *(pThis - 1), *pLast, *(pLast - 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }

    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin() + iEnd, vResult.end(), oOpts.outOfRangeVal);
}

/// Process a line above or below the observer.
///
/// @param nLine  Line number being processed.
/// @param vLastLineVal  Vector in which to store the read line. Becomes the last line
///    in further processing.
/// @return True on success, false otherwise.
bool ViewshedExecutor::processLine(int nLine, std::vector<double> &vLastLineVal)
{
    int nYOffset = nLine - m_nY;
    std::vector<double> vResult(oOutExtent.xSize());
    std::vector<double> vThisLineVal(oOutExtent.xSize());

    if (!readLine(nLine, vThisLineVal.data()))
        return false;

    // In DEM mode the base is the input DEM value.
    // In ground mode the base is zero.
    if (oOpts.outputMode == OutputMode::DEM)
        vResult = vThisLineVal;

    // Adjust height of the read line.
    const auto [iLeft, iRight] = adjustHeight(nYOffset, vThisLineVal);

    // Handle the initial position on the line.
    if (oCurExtent.containsX(m_nX))
    {
        if (iLeft < iRight)
        {
            double dfZ;
            if (std::abs(nYOffset) == 1)
                dfZ = vThisLineVal[m_nX];
            else
                dfZ = CalcHeightLine(nYOffset, vLastLineVal[m_nX]);
            setOutput(vResult[m_nX], vThisLineVal[m_nX], dfZ);
        }
        else
            vResult[m_nX] = oOpts.outOfRangeVal;
    }

    // process left half then right half of line
    CPLJobQueuePtr pQueue = m_pool.CreateJobQueue();
    pQueue->SubmitJob(
        [&, left = iLeft]()
        {
            processLineLeft(nYOffset, m_nX - 1, left - 1, vResult, vThisLineVal,
                            vLastLineVal);
        });
    pQueue->SubmitJob(
        [&, right = iRight]()
        {
            processLineRight(nYOffset, m_nX + 1, right, vResult, vThisLineVal,
                             vLastLineVal);
        });
    pQueue->WaitCompletion();

    // Make the current line the last line.
    vLastLineVal = std::move(vThisLineVal);

    if (!writeLine(nLine, vResult))
        return false;

    return oProgress.lineComplete();
}

/// Run the viewshed computation
/// @return  Success as true or false.
bool ViewshedExecutor::run()
{
    std::vector<double> vFirstLineVal(oCurExtent.xSize());
    if (!processFirstLine(vFirstLineVal))
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
            std::vector<double> vLastLineVal = vFirstLineVal;

            for (int nLine = yStart - 1; nLine >= oCurExtent.yStart && !err;
                 nLine--)
                if (!processLine(nLine, vLastLineVal))
                    err = true;
        });

    // scan downwards
    pQueue->SubmitJob(
        [&]()
        {
            std::vector<double> vLastLineVal = vFirstLineVal;

            for (int nLine = yStart + 1; nLine < oCurExtent.yStop && !err;
                 nLine++)
                if (!processLine(nLine, vLastLineVal))
                    err = true;
        });
    return true;
}

}  // namespace viewshed
}  // namespace gdal
