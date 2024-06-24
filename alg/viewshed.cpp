/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
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

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <future>

#include "gdal_alg.h"
#include "gdal_priv_templates.hpp"

#include "viewshed.h"

/************************************************************************/
/*                        GDALViewshedGenerate()                        */
/************************************************************************/

/**
 * Create viewshed from raster DEM.
 *
 * This algorithm will generate a viewshed raster from an input DEM raster
 * by using a modified algorithm of "Generating Viewsheds without Using
 * Sightlines" published at
 * https://www.asprs.org/wp-content/uploads/pers/2000journal/january/2000_jan_87-90.pdf
 * This appoach provides a relatively fast calculation, since the output raster
 * is generated in a single scan. The gdal/apps/gdal_viewshed.cpp mainline can
 * be used as an example of how to use this function. The output raster will be
 * of type Byte or Float64.
 *
 * \note The algorithm as implemented currently will only output meaningful
 * results if the georeferencing is in a projected coordinate reference system.
 *
 * @param hBand The band to read the DEM data from. Only the part of the raster
 * within the specified maxdistance around the observer point is processed.
 *
 * @param pszDriverName Driver name (GTiff if set to NULL)
 *
 * @param pszTargetRasterName The name of the target raster to be generated.
 * Must not be NULL
 *
 * @param papszCreationOptions creation options.
 *
 * @param dfObserverX observer X value (in SRS units)
 *
 * @param dfObserverY observer Y value (in SRS units)
 *
 * @param dfObserverHeight The height of the observer above the DEM surface.
 *
 * @param dfTargetHeight The height of the target above the DEM surface.
 * (default 0)
 *
 * @param dfVisibleVal pixel value for visibility (default 255)
 *
 * @param dfInvisibleVal pixel value for invisibility (default 0)
 *
 * @param dfOutOfRangeVal The value to be set for the cells that fall outside of
 * the range specified by dfMaxDistance.
 *
 * @param dfNoDataVal The value to be set for the cells that have no data.
 *                    If set to a negative value, nodata is not set.
 *                    Note: currently, no special processing of input cells at a
 * nodata value is done (which may result in erroneous results).
 *
 * @param dfCurvCoeff Coefficient to consider the effect of the curvature and
 * refraction. The height of the DEM is corrected according to the following
 * formula: [Height] -= dfCurvCoeff * [Target Distance]^2 / [Earth Diameter] For
 * the effect of the atmospheric refraction we can use 0.85714.
 *
 * @param eMode The mode of the viewshed calculation.
 * Possible values GVM_Diagonal = 1, GVM_Edge = 2 (default), GVM_Max = 3,
 * GVM_Min = 4.
 *
 * @param dfMaxDistance maximum distance range to compute viewshed.
 *                      It is also used to clamp the extent of the output
 * raster. If set to 0, then unlimited range is assumed, that is to say the
 *                      computation is performed on the extent of the whole
 * raster.
 *
 * @param pfnProgress A GDALProgressFunc that may be used to report progress
 * to the user, or to interrupt the algorithm.  May be NULL if not required.
 *
 * @param pProgressArg The callback data for the pfnProgress function.
 *
 * @param heightMode Type of information contained in output raster. Possible
 * values GVOT_NORMAL = 1 (default), GVOT_MIN_TARGET_HEIGHT_FROM_DEM = 2,
 *                   GVOT_MIN_TARGET_HEIGHT_FROM_GROUND = 3
 *
 *                   GVOT_NORMAL returns a raster of type Byte containing
 * visible locations.
 *
 *                   GVOT_MIN_TARGET_HEIGHT_FROM_DEM and
 * GVOT_MIN_TARGET_HEIGHT_FROM_GROUND will return a raster of type Float64
 * containing the minimum target height for target to be visible from the DEM
 * surface or ground level respectively. Parameters dfTargetHeight, dfVisibleVal
 * and dfInvisibleVal will be ignored.
 *
 *
 * @param papszExtraOptions Future extra options. Must be set to NULL currently.
 *
 * @return not NULL output dataset on success (to be closed with GDALClose()) or
 * NULL if an error occurs.
 *
 * @since GDAL 3.1
 */
GDALDatasetH GDALViewshedGenerate(
    GDALRasterBandH hBand, const char *pszDriverName,
    const char *pszTargetRasterName, CSLConstList papszCreationOptions,
    double dfObserverX, double dfObserverY, double dfObserverHeight,
    double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
    double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
    GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
    void *pProgressArg, GDALViewshedOutputType heightMode,
    [[maybe_unused]] CSLConstList papszExtraOptions)
{
    using namespace gdal;

    Viewshed::Options oOpts;
    oOpts.outputFormat = pszDriverName;
    oOpts.outputFilename = pszTargetRasterName;
    oOpts.creationOpts = papszCreationOptions;
    oOpts.observer.x = dfObserverX;
    oOpts.observer.y = dfObserverY;
    oOpts.observer.z = dfObserverHeight;
    oOpts.targetHeight = dfTargetHeight;
    oOpts.curveCoeff = dfCurvCoeff;
    oOpts.maxDistance = dfMaxDistance;
    oOpts.nodataVal = dfNoDataVal;

    switch (eMode)
    {
        case GVM_Edge:
            oOpts.cellMode = Viewshed::CellMode::Edge;
            break;
        case GVM_Diagonal:
            oOpts.cellMode = Viewshed::CellMode::Diagonal;
            break;
        case GVM_Min:
            oOpts.cellMode = Viewshed::CellMode::Min;
            break;
        case GVM_Max:
            oOpts.cellMode = Viewshed::CellMode::Max;
            break;
    }

    switch (heightMode)
    {
        case GVOT_MIN_TARGET_HEIGHT_FROM_DEM:
            oOpts.outputMode = Viewshed::OutputMode::DEM;
            break;
        case GVOT_MIN_TARGET_HEIGHT_FROM_GROUND:
            oOpts.outputMode = Viewshed::OutputMode::Ground;
            break;
        case GVOT_NORMAL:
            oOpts.outputMode = Viewshed::OutputMode::Normal;
            break;
    }

    if (!GDALIsValueInRange<uint8_t>(dfVisibleVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfVisibleVal out of range. Must be [0, 255].");
        return nullptr;
    }
    if (!GDALIsValueInRange<uint8_t>(dfInvisibleVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfInvisibleVal out of range. Must be [0, 255].");
        return nullptr;
    }
    if (!GDALIsValueInRange<uint8_t>(dfOutOfRangeVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfOutOfRangeVal out of range. Must be [0, 255].");
        return nullptr;
    }
    oOpts.visibleVal = dfVisibleVal;
    oOpts.invisibleVal = dfInvisibleVal;
    oOpts.outOfRangeVal = dfOutOfRangeVal;

    gdal::Viewshed v(oOpts);

    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;
    v.run(hBand, pfnProgress, pProgressArg);

    return GDALDataset::FromHandle(v.output().release());
}

namespace gdal
{

namespace
{

// Calculate the height adjustment factor.
double CalcHeightAdjFactor(const GDALDataset *poDataset, double dfCurveCoeff)
{
    const OGRSpatialReference *poDstSRS = poDataset->GetSpatialRef();

    if (poDstSRS)
    {
        OGRErr eSRSerr;

        // If we can't get a SemiMajor axis from the SRS, it will be SRS_WGS84_SEMIMAJOR
        double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

        /* If we fetched the axis from the SRS, use it */
        if (eSRSerr != OGRERR_FAILURE)
            return dfCurveCoeff / (dfSemiMajor * 2.0);

        CPLDebug("GDALViewshedGenerate",
                 "Unable to fetch SemiMajor axis from spatial reference");
    }
    return 0;
}

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

}  // unnamed namespace

/// Calculate the extent of the output raster in terms of the input raster.
///
/// @param nX  observer X position in the input raster
/// @param nY  observer Y position in the input raster
/// @return  false on error, true otherwise
bool Viewshed::calcOutputExtent(int nX, int nY)
{
    // We start with the assumption that the output size matches the input.
    oOutExtent.xStop = GDALGetRasterBandXSize(pSrcBand);
    oOutExtent.yStop = GDALGetRasterBandYSize(pSrcBand);

    if (!oOutExtent.containsY(nY))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Observer position above or below the raster "
                 "not currently supported");
        return false;
    }
    if (!oOutExtent.contains(nX, nY))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "NOTE: The observer location falls outside of the DEM area");
        //ABELL - Make sure observer Z is specified.
    }

    constexpr double EPSILON = 1e-8;
    if (oOpts.maxDistance > 0)
    {
        //ABELL - This assumes that the transformation is only a scaling. Should be fixed.
        //  Find the distance in the direction of the transformed unit vector in the X and Y
        //  directions and use those factors to determine the limiting values in the raster space.
        int nXStart = static_cast<int>(
            std::floor(nX - adfInvTransform[1] * oOpts.maxDistance + EPSILON));
        int nXStop = static_cast<int>(
            std::ceil(nX + adfInvTransform[1] * oOpts.maxDistance - EPSILON) +
            1);
        int nYStart =
            static_cast<int>(std::floor(
                nY - std::fabs(adfInvTransform[5]) * oOpts.maxDistance +
                EPSILON)) -
            (adfInvTransform[5] > 0 ? 1 : 0);
        int nYStop = static_cast<int>(
            std::ceil(nY + std::fabs(adfInvTransform[5]) * oOpts.maxDistance -
                      EPSILON) +
            (adfInvTransform[5] < 0 ? 1 : 0));

        // If the limits are invalid, set the window size to zero to trigger the error below.
        if (nXStart >= oOutExtent.xStop || nXStop < 0 ||
            nYStart >= oOutExtent.yStop || nYStop < 0)
        {
            oOutExtent = Window();
        }
        else
        {
            oOutExtent.xStart = std::max(nXStart, 0);
            oOutExtent.xStop = std::min(nXStop, oOutExtent.xStop);

            oOutExtent.yStart = std::max(nYStart, 0);
            oOutExtent.yStop = std::min(nYStop, oOutExtent.yStop);
        }
    }

    if (oOutExtent.xSize() == 0 || oOutExtent.ySize() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid target raster size due to transform "
                 "and/or distance limitation.");
        return false;
    }
    return true;
}

/// Read a line of raster data.
///
/// @param  nLine  Line number to read.
/// @param  data  Pointer to location in which to store data.
/// @return  Success or failure.
bool Viewshed::readLine(int nLine, double *data)
{
    std::lock_guard g(iMutex);

    if (GDALRasterIO(pSrcBand, GF_Read, oOutExtent.xStart, nLine,
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
bool Viewshed::writeLine(int nLine, std::vector<double> &vResult)
{
    // GDALRasterIO isn't thread-safe.
    std::lock_guard g(oMutex);

    if (GDALRasterIO(pDstBand, GF_Write, 0, nLine - oOutExtent.yStart,
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

/// Emit progress information saying that a line has been written to output.
///
/// @return  True on success, false otherwise.
bool Viewshed::lineProgress()
{
    if (nLineCount < oCurExtent.ySize())
        nLineCount++;
    return emitProgress(nLineCount / static_cast<double>(oCurExtent.ySize()));
}

/// Emit progress information saying that a fraction of work has been completed.
///
/// @return  True on success, false otherwise.
bool Viewshed::emitProgress(double fraction)
{
    // Call the progress function.
    if (!oProgress(fraction, ""))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return false;
    }
    return true;
}

/// Adjust the height of the line of data by the observer height and the curvature of the
/// earth.
///
/// @param  nYOffset  Y offset of the line being adjusted.
/// @param  nX  X location of the observer.
/// @param  vThisLineVal  Line height data.
/// @return [left, right)  Leftmost and one past the rightmost cell in the line within
///    the max distance
std::pair<int, int> Viewshed::adjustHeight(int nYOffset, int nX,
                                           std::vector<double> &vThisLineVal)
{
    int nLeft = 0;
    int nRight = oCurExtent.xSize();

    // Find the starting point in the raster (nX may be outside)
    int nXStart = oCurExtent.clampX(nX);

    // If there is a height adjustment factor other than zero or a max distance,
    // calculate the adjusted height of the cell, stopping if we've exceeded the max
    // distance.
    if (static_cast<bool>(dfHeightAdjFactor) || dfMaxDistance2 > 0)
    {
        // Hoist invariants from the loops.
        const double dfLineX = adfTransform[2] * nYOffset;
        const double dfLineY = adfTransform[5] * nYOffset;

        // Go left
        double *pdfHeight = vThisLineVal.data() + nXStart;
        for (int nXOffset = nXStart - nX; nXOffset >= -nX;
             nXOffset--, pdfHeight--)
        {
            double dfX = adfTransform[1] * nXOffset + dfLineX;
            double dfY = adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > dfMaxDistance2)
            {
                nLeft = nXOffset + nX + 1;
                break;
            }
            *pdfHeight -= dfHeightAdjFactor * dfR2 + dfZObserver;
        }

        // Go right.
        pdfHeight = vThisLineVal.data() + nXStart + 1;
        for (int nXOffset = nXStart - nX + 1;
             nXOffset < oCurExtent.xSize() - nX; nXOffset++, pdfHeight++)
        {
            double dfX = adfTransform[1] * nXOffset + dfLineX;
            double dfY = adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > dfMaxDistance2)
            {
                nRight = nXOffset + nX;
                break;
            }
            *pdfHeight -= dfHeightAdjFactor * dfR2 + dfZObserver;
        }
    }
    else
    {
        // No curvature adjustment. Just normalize for the observer height.
        double *pdfHeight = vThisLineVal.data();
        for (int i = 0; i < oCurExtent.xSize(); ++i)
        {
            *pdfHeight -= dfZObserver;
            pdfHeight++;
        }
    }
    return {nLeft, nRight};
}

/// Create the output dataset.
///
/// @return  True on success, false otherwise.
bool Viewshed::createOutputDataset()
{
    GDALDriverManager *hMgr = GetGDALDriverManager();
    GDALDriver *hDriver = hMgr->GetDriverByName(oOpts.outputFormat.c_str());
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return false;
    }

    /* create output raster */
    poDstDS.reset(hDriver->Create(
        oOpts.outputFilename.c_str(), oOutExtent.xSize(), oOutExtent.ySize(), 1,
        oOpts.outputMode == OutputMode::Normal ? GDT_Byte : GDT_Float64,
        const_cast<char **>(oOpts.creationOpts.List())));
    if (!poDstDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                 oOpts.outputFilename.c_str());
        return false;
    }

    /* copy srs */
    GDALDatasetH hSrcDS = GDALGetBandDataset(pSrcBand);
    if (hSrcDS)
        poDstDS->SetSpatialRef(
            GDALDataset::FromHandle(hSrcDS)->GetSpatialRef());

    std::array<double, 6> adfDstTransform;
    adfDstTransform[0] = adfTransform[0] + adfTransform[1] * oOutExtent.xStart +
                         adfTransform[2] * oOutExtent.yStart;
    adfDstTransform[1] = adfTransform[1];
    adfDstTransform[2] = adfTransform[2];
    adfDstTransform[3] = adfTransform[3] + adfTransform[4] * oOutExtent.xStart +
                         adfTransform[5] * oOutExtent.yStart;
    adfDstTransform[4] = adfTransform[4];
    adfDstTransform[5] = adfTransform[5];
    poDstDS->SetGeoTransform(adfDstTransform.data());

    pDstBand = poDstDS->GetRasterBand(1);
    if (!pDstBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 oOpts.outputFilename.c_str());
        return false;
    }

    if (oOpts.nodataVal >= 0)
        GDALSetRasterNoDataValue(pDstBand, oOpts.nodataVal);
    return true;
}

namespace
{

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

/// Process the part of the first line to the left of the observer.
///
/// @param nX  X coordinate of the observer.
/// @param iStart  X coordinate of the first cell to the left of the observer to be procssed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
void Viewshed::processFirstLineLeft(int nX, int iStart, int iEnd,
                                    std::vector<double> &vResult,
                                    std::vector<double> &vThisLineVal)
{
    // If end is to the right of start, everything is taken care of by right processing.
    if (iEnd >= iStart)
        return;

    iStart = oCurExtent.clampX(iStart);

    double *pThis = vThisLineVal.data() + iStart;

    // If the start cell is next to the observer, just mark it visible.
    if (iStart + 1 == nX || iStart + 1 == oCurExtent.xStop)
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
        int nXOffset = std::abs(iPixel - nX);
        double dfZ = CalcHeightLine(nXOffset, *(pThis + 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }
    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin(), vResult.begin() + iEnd + 1, oOpts.outOfRangeVal);
}

/// Process the part of the first line to the right of the observer.
///
/// @param nX  X coordinate of the observer.
/// @param iStart  X coordinate of the first cell to the right of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
void Viewshed::processFirstLineRight(int nX, int iStart, int iEnd,
                                     std::vector<double> &vResult,
                                     std::vector<double> &vThisLineVal)
{
    // If start is to the right of end, everything is taken care of by left processing.
    if (iStart >= iEnd)
        return;

    iStart = oCurExtent.clampX(iStart);

    double *pThis = vThisLineVal.data() + iStart;

    // If the start cell is next to the observer, just mark it visible.
    if (iStart - 1 == nX || iStart == oCurExtent.xStart)
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
        int nXOffset = std::abs(iPixel - nX);
        double dfZ = CalcHeightLine(nXOffset, *(pThis - 1));
        setOutput(vResult[iPixel], *pThis, dfZ);
    }
    // For cells outside of the [start, end) range, set the outOfRange value.
    std::fill(vResult.begin() + iEnd, vResult.end(), oOpts.outOfRangeVal);
}

/// Process a line to the left of the observer.
///
/// @param nX  X coordinate of the observer.
/// @param nYOffset  Offset of the line being processed from the observer
/// @param iStart  X coordinate of the first cell to the left of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
/// @param vLastLineVal  Observable height of each cell in the previous line processed.
void Viewshed::processLineLeft(int nX, int nYOffset, int iStart, int iEnd,
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
        int nXOffset = std::abs(iPixel - nX);
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
/// @param nX  X coordinate of the observer.
/// @param nYOffset  Offset of the line being processed from the observer
/// @param iStart  X coordinate of the first cell to the right of the observer to be processed.
/// @param iEnd  X coordinate one past the last cell to be processed.
/// @param vResult  Vector in which to store the visibility/height results.
/// @param vThisLineVal  Height of each cell in the line being processed.
/// @param vLastLineVal  Observable height of each cell in the previous line processed.
void Viewshed::processLineRight(int nX, int nYOffset, int iStart, int iEnd,
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
        int nXOffset = std::abs(iPixel - nX);
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

/// Set the output Z value depending on the observable height and computation mode.
///
/// dfResult  Reference to the result cell
/// dfCellVal  Reference to the current cell height. Replace with observable height.
/// dfZ  Minimum observable height at cell.
void Viewshed::setOutput(double &dfResult, double &dfCellVal, double dfZ)
{
    if (oOpts.outputMode != OutputMode::Normal)
    {
        dfResult += (dfZ - dfCellVal);
        dfResult = std::max(0.0, dfResult);
    }
    else
        dfResult = (dfCellVal + oOpts.targetHeight < dfZ) ? oOpts.invisibleVal
                                                          : oOpts.visibleVal;
    dfCellVal = std::max(dfCellVal, dfZ);
}

/// Process the first line (the one with the Y coordinate the same as the observer).
///
/// @param nX  X location of the observer
/// @param nY  Y location of the observer
/// @param nLine  Line number being processed (should always be the same as nY)
/// @param vLastLineVal  Vector in which to store the read line. Becomes the last line
///    in further processing.
/// @return True on success, false otherwise.
bool Viewshed::processFirstLine(int nX, int nY, int nLine,
                                std::vector<double> &vLastLineVal)
{
    int nYOffset = nLine - nY;
    assert(nYOffset == 0);
    std::vector<double> vResult(oOutExtent.xSize());
    std::vector<double> vThisLineVal(oOutExtent.xSize());

    if (!readLine(nLine, vThisLineVal.data()))
        return false;

    // If the observer is outside of the raster, take the specified value as the Z height,
    // otherwise, take it as an offset from the raster height at that location.
    dfZObserver = oOpts.observer.z;
    if (oCurExtent.containsX(nX))
    {
        dfZObserver += vThisLineVal[nX];
        if (oOpts.outputMode == OutputMode::Normal)
            vResult[nX] = oOpts.visibleVal;
    }
    dfHeightAdjFactor = CalcHeightAdjFactor(poDstDS.get(), oOpts.curveCoeff);

    // In DEM mode the base is the pre-adjustment value.  In ground mode the base is zero.
    if (oOpts.outputMode == OutputMode::DEM)
        vResult = vThisLineVal;

    // iLeft and iRight are the processing limits for the line.
    const auto [iLeft, iRight] = adjustHeight(nYOffset, nX, vThisLineVal);

    auto t1 = std::async(
        std::launch::async, [&, left = iLeft]()
        { processFirstLineLeft(nX, nX - 1, left - 1, vResult, vThisLineVal); });

    auto t2 = std::async(
        std::launch::async, [&, right = iRight]()
        { processFirstLineRight(nX, nX + 1, right, vResult, vThisLineVal); });
    t1.wait();
    t2.wait();

    // Make the current line the last line.
    vLastLineVal = std::move(vThisLineVal);

    // Create the output writer.
    if (!writeLine(nY, vResult))
        return false;

    if (!lineProgress())
        return false;
    return true;
}

/// Process a line above or below the observer.
///
/// @param nX  X location of the observer
/// @param nY  Y location of the observer
/// @param nLine  Line number being processed.
/// @param vLastLineVal  Vector in which to store the read line. Becomes the last line
///    in further processing.
/// @return True on success, false otherwise.
bool Viewshed::processLine(int nX, int nY, int nLine,
                           std::vector<double> &vLastLineVal)
{
    int nYOffset = nLine - nY;
    std::vector<double> vResult(oOutExtent.xSize());
    std::vector<double> vThisLineVal(oOutExtent.xSize());

    if (!readLine(nLine, vThisLineVal.data()))
        return false;

    // In DEM mode the base is the input DEM value.
    // In ground mode the base is zero.
    if (oOpts.outputMode == OutputMode::DEM)
        vResult = vThisLineVal;

    // Adjust height of the read line.
    const auto [iLeft, iRight] = adjustHeight(nYOffset, nX, vThisLineVal);

    // Handle the initial position on the line.
    if (oCurExtent.containsX(nX))
    {
        if (iLeft < iRight)
        {
            double dfZ;
            if (std::abs(nYOffset) == 1)
                dfZ = vThisLineVal[nX];
            else
                dfZ = CalcHeightLine(nYOffset, vLastLineVal[nX]);
            setOutput(vResult[nX], vThisLineVal[nX], dfZ);
        }
        else
            vResult[nX] = oOpts.outOfRangeVal;
    }

    // process left half then right half of line
    auto t1 =
        std::async(std::launch::async,
                   [&, left = iLeft]()
                   {
                       processLineLeft(nX, nYOffset, nX - 1, left - 1, vResult,
                                       vThisLineVal, vLastLineVal);
                   });

    auto t2 =
        std::async(std::launch::async,
                   [&, right = iRight]()
                   {
                       processLineRight(nX, nYOffset, nX + 1, right, vResult,
                                        vThisLineVal, vLastLineVal);
                   });
    t1.wait();
    t2.wait();

    // Make the current line the last line.
    vLastLineVal = std::move(vThisLineVal);

    if (!writeLine(nLine, vResult))
        return false;

    if (!lineProgress())
        return false;
    return true;
}

/// Compute the viewshed of a raster band.
///
/// @param band  Pointer to the raster band to be processed.
/// @param pfnProgress  Pointer to the progress function. Can be null.
/// @param pProgressArg  Argument passed to the progress function
/// @return  True on success, false otherwise.
bool Viewshed::run(GDALRasterBandH band, GDALProgressFunc pfnProgress,
                   void *pProgressArg)
{
    using namespace std::placeholders;

    nLineCount = 0;
    pSrcBand = static_cast<GDALRasterBand *>(band);

    oProgress = std::bind(pfnProgress, _1, _2, pProgressArg);

    if (!emitProgress(0))
        return false;

    // set up geotransformation
    GDALDatasetH hSrcDS = GDALGetBandDataset(pSrcBand);
    if (hSrcDS != nullptr)
        GDALGetGeoTransform(hSrcDS, adfTransform.data());

    if (!GDALInvGeoTransform(adfTransform.data(), adfInvTransform.data()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return false;
    }

    // calculate observer position
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvTransform.data(), oOpts.observer.x,
                          oOpts.observer.y, &dfX, &dfY);
    if (!GDALIsValueInRange<int>(dfX))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Observer X value out of range");
        return false;
    }
    if (!GDALIsValueInRange<int>(dfY))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Observer Y value out of range");
        return false;
    }
    int nX = static_cast<int>(dfX);
    int nY = static_cast<int>(dfY);

    // calculate the area of interest
    if (!calcOutputExtent(nX, nY))
        return false;

    // normalize horizontal index to [ 0, oOutExtent.xSize() )
    //ABELL - verify this won't underflow.
    oCurExtent = oOutExtent;
    oCurExtent.shiftX(-oOutExtent.xStart);
    nX -= oOutExtent.xStart;

    // create the output dataset
    if (!createOutputDataset())
        return false;

    std::vector<double> vFirstLineVal(oCurExtent.xSize());

    if (!processFirstLine(nX, nY, nY, vFirstLineVal))
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
    std::atomic<bool> err(false);
    auto tUp = std::async(std::launch::async,
                          [&]()
                          {
                              std::vector<double> vLastLineVal = vFirstLineVal;

                              for (int nLine = nY - 1;
                                   nLine >= oCurExtent.yStart && !err; nLine--)
                                  if (!processLine(nX, nY, nLine, vLastLineVal))
                                      err = true;
                          });

    // scan downwards
    auto tDown = std::async(
        std::launch::async,
        [&]()
        {
            std::vector<double> vLastLineVal = vFirstLineVal;

            for (int nLine = nY + 1; nLine < oCurExtent.yStop && !err; nLine++)
                if (!processLine(nX, nY, nLine, vLastLineVal))
                    err = true;
        });

    tUp.wait();
    tDown.wait();

    if (!emitProgress(1))
        return false;

    return true;
}

}  // namespace gdal
