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
#include <limits>
//ABELL
#include <iostream>
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include "gdal_alg.h"
#include "gdal_priv.h"
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
    oOpts.visibleVal = static_cast<uint8_t>(dfVisibleVal);
    oOpts.invisibleVal = static_cast<uint8_t>(dfInvisibleVal);
    oOpts.outOfRangeVal = static_cast<uint8_t>(dfOutOfRangeVal);

    gdal::Viewshed v(oOpts);

    //ABELL - Make a function for progress that captures the progress argument.
    v.run(hBand, pfnProgress, pProgressArg);

    return GDALDataset::FromHandle(v.output().release());
}

namespace gdal
{

namespace
{

const int Right = 0x01;
const int Left = 0x02;
const int Up = 0x04;
const int Down = 0X08;

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

// Calculate the height at i units along a line through the origin given the height
// at i - 1 units along the line.
double CalcHeightLine(int i, double Za)
{
    if (i == 1)
        return Za;
    else
        return Za * i / (i - 1);
}

// Calulate the height Zc of a point (i, j, Zc) given a line through the origin (0, 0, 0)
// and passing through the line connecting (i - 1, j, Za) and (i, j - 1, Zb).
// In other words, the origin and the two points form a plane and we're calculating Zc
// of the point (i, j Zc), also on the plane.
double CalcHeightDiagonal(int i, int j, double Za, double Zb)
{
    return (Za * i + Zb * j) / (i + j - 1);
}

double CalcHeightEdge(int i, int j, double Za, double Zb)
{
    if (i == j)
        return CalcHeightLine(i, Za);
    else
        return (Za * i + Zb * (j - i)) / (j - 1);
}

}  // unnamed namespace

/// \brief  Calculate the output extent of the output raster in terms of the input raster.
/// \return  false on error, true otherwise
/// \param nX  observer X position in the input raster
/// \param nY  observer Y position in the input raster
bool Viewshed::calcOutputExtent(int nX, int nY)
{
    // We start with the assumption that the output size matches the input.
    oOutExtent.xStop = GDALGetRasterBandXSize(hBand);
    oOutExtent.yStop = GDALGetRasterBandYSize(hBand);

    if (nX < 0 || nX >= oOutExtent.xStop || nY < 0 || nY >= oOutExtent.yStop)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The observer location falls outside of the DEM area");
        return false;
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

        oOutExtent.xStart = std::max(nXStart, 0);
        oOutExtent.yStart = std::max(nYStart, 0);
        oOutExtent.xStop = std::min(nXStop, oOutExtent.xStop);
        oOutExtent.yStop = std::min(nYStop, oOutExtent.yStop);
    }

    if (oOutExtent.xSize() == 0 || oOutExtent.ySize() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid target raster size");
        return false;
    }
    return true;
}

bool Viewshed::readLine(int nLine, double *data)
{
    if (GDALRasterIO(hBand, GF_Read, oOutExtent.xStart, nLine,
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

std::pair<int, int> Viewshed::adjustHeight(int nYOffset, int nX,
                                           double dfObserverHeight,
                                           double *const pdfNx)
{
    int nLeft = 0;
    int nRight = oOutExtent.xSize();
    if (static_cast<bool>(dfHeightAdjFactor) || dfMaxDistance2 > 0)
    {
        // Hoist invariants from the loops.
        const double dfLineX = adfTransform[2] * nYOffset;
        const double dfLineY = adfTransform[5] * nYOffset;

        double *pdfHeight = pdfNx;
        for (int nXOffset = 0; nXOffset >= -nX; nXOffset--, pdfHeight--)
        {
            double dfX = adfTransform[1] * nXOffset + dfLineX;
            double dfY = adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > dfMaxDistance2)
            {
                nLeft = nXOffset + nX + 1;
                break;
            }
            *pdfHeight -= dfHeightAdjFactor * dfR2 + dfObserverHeight;
        }

        pdfHeight = pdfNx + 1;
        for (int nXOffset = 1; nXOffset < oOutExtent.xSize() - nX;
             nXOffset++, pdfHeight++)
        {
            double dfX = adfTransform[1] * nXOffset + dfLineX;
            double dfY = adfTransform[4] * nXOffset + dfLineY;
            double dfR2 = dfX * dfX + dfY * dfY;
            if (dfR2 > dfMaxDistance2)
            {
                nRight = nXOffset + nX;
                break;
            }
            *pdfHeight -= dfHeightAdjFactor * dfR2 + dfObserverHeight;
        }
    }
    else
    {
        double *pdfHeight = pdfNx - nX;
        for (int i = 0; i < oOutExtent.xSize(); ++i)
        {
            *pdfHeight -= dfObserverHeight;
            pdfHeight++;
        }
    }
    return {nLeft, nRight};
}

void Viewshed::setVisibility(int iPixel, double dfZ)
{
    // Shorter alias.
    double &dfCurHeight = vThisLineVal[iPixel];

    if (dfCurHeight + oOpts.targetHeight < dfZ)
        vResult[iPixel] = oOpts.invisibleVal;
    else
        vResult[iPixel] = oOpts.visibleVal;

    dfCurHeight = std::max(dfCurHeight, dfZ);
}

double Viewshed::calcHeight(double dfDiagZ, double dfEdgeZ)
{
    double dfHeight = dfEdgeZ;

    switch (oOpts.cellMode)
    {
        case Viewshed::CellMode::Max:
            dfHeight = std::max(dfDiagZ, dfEdgeZ);
            break;
        case Viewshed::CellMode::Min:
            dfHeight = std::min(dfDiagZ, dfEdgeZ);
            break;
        case Viewshed::CellMode::Diagonal:
            dfHeight = dfDiagZ;
            break;
        default:  // Edge case set in initialization.
            break;
    }
    return dfHeight;
}

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
    GDALDatasetH hSrcDS = GDALGetBandDataset(hBand);
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

    return true;
}

bool Viewshed::allocate()
{
    try
    {
        vFirstLineVal.resize(oOutExtent.xSize());
        vLastLineVal.resize(oOutExtent.xSize());
        vThisLineVal.resize(oOutExtent.xSize());
        vResult.resize(oOutExtent.xSize());

        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult.resize(oOutExtent.xSize());
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate vectors for viewshed");
        return false;
    }
    return true;
}

void Viewshed::processHalfLine(int nX, int nYOffset, int iStart, int iEnd,
                               int iDir)
{
    // We always go from the center out to the left or right.
    double *pThis = vThisLineVal.data() + iStart;
    double *pLast = vLastLineVal.data() + iStart;

    if (iDir & Up)
        nYOffset = -nYOffset;

    // Calculate the visible height at iPixel. We either use edge mode or diagonal mode
    // or we take the min/max of both.
    auto calcZ = [&](int iPixel)
    {
        double dfDiagZ = 0;
        double dfEdgeZ = 0;
        int nXOffset = iPixel - nX;
        int iPrev = 1;
        if (iDir & Left)
        {
            nXOffset = -nXOffset;
            iPrev = -iPrev;
        }

        if (oOpts.cellMode != CellMode::Edge)  // Diagonal, Min, Max
            dfDiagZ = CalcHeightDiagonal(nXOffset, nYOffset, *(pThis + iPrev),
                                         *pLast);

        if (oOpts.cellMode != CellMode::Diagonal)  // Edge, Min, Max
            dfEdgeZ = nXOffset >= nYOffset
                          ? CalcHeightEdge(nYOffset, nXOffset, *(pLast + iPrev),
                                           *(pThis + iPrev))
                          : CalcHeightEdge(nXOffset, nYOffset, *(pLast + iPrev),
                                           *pLast);
        double dfZ = calcHeight(dfDiagZ, dfEdgeZ);
        if (oOpts.outputMode != OutputMode::Normal)
        {
            vHeightResult[iPixel] += (dfZ - *pThis);
            vHeightResult[iPixel] = std::max(0.0, vHeightResult[iPixel]);
        }
        setVisibility(iPixel, dfZ);
    };

    // Go from the center left or right, calculating Z as we go.
    if (iDir & Left)
        for (int iPixel = iStart; iPixel > iEnd; iPixel--, pThis--, pLast--)
            calcZ(iPixel);
    else
        for (int iPixel = iStart; iPixel < iEnd; iPixel++, pThis++, pLast++)
            calcZ(iPixel);

    // For cells outside of the [start, end) range, set the outOfRange value.
    auto setOor = [&](GByte *pResult, double *pHeight)
    {
        *pResult = oOpts.outOfRangeVal;
        if (oOpts.outputMode != OutputMode::Normal)
            *pHeight = oOpts.outOfRangeVal;
    };

    GByte *pResult = vResult.data() + iEnd;
    double *pHeight = vHeightResult.data() + iEnd;
    if (iDir & Left)
        for (int iPixel = iEnd; iPixel >= 0; iPixel--, pResult--, pHeight--)
            setOor(pResult, pHeight);
    else
        for (int iPixel = iEnd; iPixel < oOutExtent.xSize();
             iPixel++, pResult++, pHeight++)
            setOor(pResult, pHeight);
}

bool Viewshed::run(GDALRasterBandH band, GDALProgressFunc pfnProgress,
                   void *pProgressArg)
{
    hBand = band;

    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;

    if (!pfnProgress(0.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return false;
    }

    /* set up geotransformation */
    GDALDatasetH hSrcDS = GDALGetBandDataset(hBand);
    if (hSrcDS != nullptr)
        GDALGetGeoTransform(hSrcDS, adfTransform.data());

    if (!GDALInvGeoTransform(adfTransform.data(), adfInvTransform.data()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return false;
    }

    /* calculate observer position */
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvTransform.data(), oOpts.observer.x,
                          oOpts.observer.y, &dfX, &dfY);
    int nX = static_cast<int>(dfX);
    int nY = static_cast<int>(dfY);

    /* calculate the area of interest */
    if (!calcOutputExtent(nX, nY))
        return false;

    /* normalize horizontal index to [ 0, oOutExtent.xSize() ) */
    nX -= oOutExtent.xStart;

    /* allocate working storage */
    if (!allocate())
        return false;

    /* create the output dataset */
    if (!createOutputDataset())
        return false;

    GDALRasterBand *hTargetBand = poDstDS->GetRasterBand(1);
    if (hTargetBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 oOpts.outputFilename.c_str());
        return false;
    }

    if (oOpts.nodataVal >= 0)
        GDALSetRasterNoDataValue(hTargetBand, oOpts.nodataVal);

    double dfZObserver = 0;

    void *data;
    GDALDataType dataType;
    if (oOpts.outputMode == OutputMode::Normal)
    {
        data = reinterpret_cast<void *>(vResult.data());
        dataType = GDT_Byte;
    }
    else
    {
        data = reinterpret_cast<void *>(vHeightResult.data());
        dataType = GDT_Float64;
    }

    /* process first line */
    {
        if (!readLine(nY, vThisLineVal.data()))
            return false;

        dfZObserver = oOpts.observer.z + vThisLineVal[nX];

        dfHeightAdjFactor =
            CalcHeightAdjFactor(poDstDS.get(), oOpts.curveCoeff);

        // In DEM mode the base is the pre-adjustment value.
        // In ground mode the base is zero.
        if (oOpts.outputMode == OutputMode::DEM)
            vHeightResult = vThisLineVal;
        else if (oOpts.outputMode == OutputMode::Ground)
            std::fill(vHeightResult.begin(), vHeightResult.end(), 0);

        const auto [iLeft, iRight] =
            adjustHeight(0, nX, dfZObserver, vThisLineVal.data() + nX);

        vResult[nX] = oOpts.visibleVal;
        if (nX - 1 >= 0)
            vResult[nX - 1] = oOpts.visibleVal;
        if (nX + 1 < oOutExtent.xSize())
            vResult[nX + 1] = oOpts.visibleVal;

        /* process left direction */
        for (int iPixel = nX - 2; iPixel >= iLeft; iPixel--)
        {
            double dfZ = CalcHeightLine(nX - iPixel, vThisLineVal[iPixel + 1]);

            if (oOpts.outputMode != OutputMode::Normal)
            {
                vHeightResult[iPixel] += (dfZ - vThisLineVal[iPixel]);
                vHeightResult[iPixel] = std::max(vHeightResult[iPixel], 0.0);
            }

            setVisibility(iPixel, dfZ);
        }

        for (int iPixel = 0; iPixel < iLeft; iPixel++)
        {
            vResult[iPixel] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] = oOpts.outOfRangeVal;
        }

        /* process right direction */
        for (int iPixel = nX + 2; iPixel < iRight; iPixel++)
        {
            double dfZ = CalcHeightLine(iPixel - nX, vThisLineVal[iPixel - 1]);

            if (oOpts.outputMode != OutputMode::Normal)
            {
                vHeightResult[iPixel] += (dfZ - vThisLineVal[iPixel]);
                vHeightResult[iPixel] = std::max(0.0, vHeightResult[iPixel]);
            }

            setVisibility(iPixel, dfZ);
        }
        for (int iPixel = iRight; iPixel < oOutExtent.xSize(); iPixel++)
        {
            vResult[iPixel] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] = oOpts.outOfRangeVal;
        }

        /* write result line */
        if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - oOutExtent.yStart,
                         oOutExtent.xSize(), 1, data, oOutExtent.xSize(), 1,
                         dataType, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when writing target raster at position "
                     "(%d,%d), size (%d,%d)",
                     0, nY - oOutExtent.yStart, oOutExtent.xSize(), 1);
            return false;
        }
    }

    // Save the first line for use later.
    vFirstLineVal = vThisLineVal;
    /* scan upwards */

    vLastLineVal = vThisLineVal;
    for (int iLine = nY - 1; iLine >= oOutExtent.yStart; iLine--)
    {
        if (!readLine(iLine, vThisLineVal.data()))
            return false;

        // In DEM mode the base is the pre-adjustment value.
        // In ground mode the base is zero.
        if (oOpts.outputMode == OutputMode::DEM)
            vHeightResult = vThisLineVal;
        else if (oOpts.outputMode == OutputMode::Ground)
            std::fill(vHeightResult.begin(), vHeightResult.end(), 0.0);

        const auto [iLeft, iRight] =
            adjustHeight(iLine - nY, nX, dfZObserver, vThisLineVal.data() + nX);

        /* set up initial point on the scanline */

        // Handle cell at nX if in range.
        if (iLeft < iRight)
        {
            double dfZ = CalcHeightLine(nY - iLine, vLastLineVal[nX]);

            if (oOpts.outputMode != OutputMode::Normal)
            {
                vHeightResult[nX] += (dfZ - vThisLineVal[nX]);
                vHeightResult[nX] = std::max(0.0, vHeightResult[nX]);
            }

            setVisibility(nX, dfZ);
        }
        else
        {
            vResult[nX] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = oOpts.outOfRangeVal;
        }

        /* process left direction */
        processHalfLine(nX, iLine - nY, nX - 1, iLeft - 1, Up | Left);

        /* process right direction */
        processHalfLine(nX, iLine - nY, nX + 1, iRight, Up | Right);

        /* write result line */
        if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - oOutExtent.yStart,
                         oOutExtent.xSize(), 1, data, oOutExtent.xSize(), 1,
                         dataType, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when writing target raster at position "
                     "(%d,%d), size (%d,%d)",
                     0, iLine - oOutExtent.yStart, oOutExtent.xSize(), 1);
            return false;
        }

        // Make this line the last line.
        std::swap(vLastLineVal, vThisLineVal);

        if (!pfnProgress((nY - iLine) / static_cast<double>(oOutExtent.ySize()),
                         "", pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return false;
        }
    }

    // Use the first line as the last. We can move since after this we're done with the
    // first line.
    vLastLineVal = std::move(vFirstLineVal);

    /* scan downwards */
    for (int iLine = nY + 1; iLine < oOutExtent.yStop; iLine++)
    {
        if (!readLine(iLine, vThisLineVal.data()))
            return false;

        // In DEM mode the base is the input DEM value.
        // In ground mode the base is zero.
        if (oOpts.outputMode == OutputMode::DEM)
            vHeightResult = vThisLineVal;
        else if (oOpts.outputMode == OutputMode::Ground)
            std::fill(vHeightResult.begin(), vHeightResult.end(), 0.0);

        const auto [iLeft, iRight] =
            adjustHeight(iLine - nY, nX, dfZObserver, vThisLineVal.data() + nX);

        /* set up initial point on the scanline */
        if (iLeft < iRight)
        {
            double dfZ = CalcHeightLine(iLine - nY, vLastLineVal[nX]);

            if (oOpts.outputMode != OutputMode::Normal)
            {
                vHeightResult[nX] += (dfZ - vThisLineVal[nX]);
                vHeightResult[nX] = std::max(0.0, vHeightResult[nX]);
            }

            setVisibility(nX, dfZ);
        }
        else
        {
            vResult[nX] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = oOpts.outOfRangeVal;
        }

        /* process left direction */
        processHalfLine(nX, iLine - nY, nX - 1, iLeft - 1, Down | Left);

        /* process right direction */
        processHalfLine(nX, iLine - nY, nX + 1, iRight, Down | Right);

        /* write result line */
        if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - oOutExtent.yStart,
                         oOutExtent.xSize(), 1, data, oOutExtent.xSize(), 1,
                         dataType, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when writing target raster at position "
                     "(%d,%d), size (%d,%d)",
                     0, iLine - oOutExtent.yStart, oOutExtent.xSize(), 1);
            return false;
        }

        std::swap(vLastLineVal, vThisLineVal);

        if (!pfnProgress((iLine - oOutExtent.yStart) /
                             static_cast<double>(oOutExtent.ySize()),
                         "", pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return false;
        }
    }

    if (!pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return false;
    }

    return true;
}

}  // namespace gdal
