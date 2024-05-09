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

double CalcHeightLine(int i, double Za, double Zo)
{
    if (i == 1)
        return Za;
    else
        return (Za - Zo) / (i - 1) + Za;
}

double CalcHeightDiagonal(int i, int j, double Za, double Zb, double Zo)
{
    return ((Za - Zo) * i + (Zb - Zo) * j) / (i + j - 1) + Zo;
}

double CalcHeightEdge(int i, int j, double Za, double Zb, double Zo)
{
    if (i == j)
        return CalcHeightLine(i, Za, Zo);
    else
        return ((Za - Zo) * i + (Zb - Zo) * (j - i)) / (j - 1) + Zo;
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

bool Viewshed::adjustHeightInRange(int iPixel, int iLine, double &dfHeight)
{
    if (dfMaxDistance2 == 0 && dfHeightAdjFactor == 0)
        return true;

    double dfX = adfTransform[1] * iPixel + adfTransform[2] * iLine;
    double dfY = adfTransform[4] * iPixel + adfTransform[5] * iLine;
    double dfR2 = dfX * dfX + dfY * dfY;

    dfHeight -= dfHeightAdjFactor * dfR2;

    return (dfMaxDistance2 == 0 || dfR2 <= dfMaxDistance2);
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

    /* process first line */
    if (!readLine(nY, vThisLineVal.data()))
        return false;

    const double dfZObserver = oOpts.observer.z + vThisLineVal[nX];

    dfHeightAdjFactor = CalcHeightAdjFactor(poDstDS.get(), oOpts.curveCoeff);

    /* mark the observer point as visible */
    double dfGroundLevel = 0;
    if (oOpts.outputMode == OutputMode::DEM)
        dfGroundLevel = vThisLineVal[nX];

    vResult[nX] = oOpts.visibleVal;

    //ABELL - Do we care about this conditional?
    if (oOpts.outputMode != OutputMode::Normal)
        vHeightResult[nX] = dfGroundLevel;

    dfGroundLevel = 0;
    //ABELL - I think nX is guaranteed to be in the range checked below.
    if (nX > 0)
    {
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX - 1];
        CPL_IGNORE_RET_VAL(adjustHeightInRange(1, 0, vThisLineVal[nX - 1]));
        vResult[nX - 1] = oOpts.visibleVal;
        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult[nX - 1] = dfGroundLevel;
    }
    if (nX < oOutExtent.xSize() - 1)
    {
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX + 1];
        CPL_IGNORE_RET_VAL(adjustHeightInRange(1, 0, vThisLineVal[nX + 1]));
        vResult[nX + 1] = oOpts.visibleVal;
        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult[nX + 1] = dfGroundLevel;
    }

    /* process left direction */
    for (int iPixel = nX - 2; iPixel >= 0; iPixel--)
    {
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[iPixel];

        if (adjustHeightInRange(nX - iPixel, 0, vThisLineVal[iPixel]))
        {
            double dfZ = CalcHeightLine(nX - iPixel, vThisLineVal[iPixel + 1],
                                        dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] =
                    std::max(0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

            setVisibility(iPixel, dfZ);
        }
        else
        {
            for (; iPixel >= 0; iPixel--)
            {
                vResult[iPixel] = oOpts.outOfRangeVal;
                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = oOpts.outOfRangeVal;
            }
        }
    }
    /* process right direction */
    for (int iPixel = nX + 2; iPixel < oOutExtent.xSize(); iPixel++)
    {
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[iPixel];
        if (adjustHeightInRange(iPixel - nX, 0, vThisLineVal[iPixel]))
        {
            double dfZ = CalcHeightLine(iPixel - nX, vThisLineVal[iPixel - 1],
                                        dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] =
                    std::max(0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

            setVisibility(iPixel, dfZ);
        }
        else
        {
            for (; iPixel < oOutExtent.xSize(); iPixel++)
            {
                vResult[iPixel] = oOpts.outOfRangeVal;
                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = oOpts.outOfRangeVal;
            }
        }
    }
    /* write result line */

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

    // Save the first line for use later.
    vFirstLineVal = vThisLineVal;
    /* scan upwards */

    vLastLineVal = vThisLineVal;
    for (int iLine = nY - 1; iLine >= oOutExtent.yStart; iLine--)
    {
        if (!readLine(iLine, vThisLineVal.data()))
            return false;

        /* set up initial point on the scanline */
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX];
        if (adjustHeightInRange(0, nY - iLine, vThisLineVal[nX]))
        {
            double dfZ =
                CalcHeightLine(nY - iLine, vLastLineVal[nX], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] =
                    std::max(0.0, (dfZ - vThisLineVal[nX] + dfGroundLevel));

            setVisibility(nX, dfZ);
        }
        else
        {
            vResult[nX] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = oOpts.outOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];
            if (adjustHeightInRange(nX - iPixel, nY - iLine,
                                    vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        nX - iPixel, nY - iLine, vThisLineVal[iPixel + 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ =
                        nX - iPixel >= nY - iLine
                            ? CalcHeightEdge(nY - iLine, nX - iPixel,
                                             vLastLineVal[iPixel + 1],
                                             vThisLineVal[iPixel + 1],
                                             dfZObserver)
                            : CalcHeightEdge(nX - iPixel, nY - iLine,
                                             vLastLineVal[iPixel + 1],
                                             vLastLineVal[iPixel], dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < oOutExtent.xSize(); iPixel++)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];

            if (adjustHeightInRange(iPixel - nX, nY - iLine,
                                    vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        iPixel - nX, nY - iLine, vThisLineVal[iPixel - 1],
                        vLastLineVal[iPixel], dfZObserver);
                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ =
                        iPixel - nX >= nY - iLine
                            ? CalcHeightEdge(nY - iLine, iPixel - nX,
                                             vLastLineVal[iPixel - 1],
                                             vThisLineVal[iPixel - 1],
                                             dfZObserver)
                            : CalcHeightEdge(iPixel - nX, nY - iLine,
                                             vLastLineVal[iPixel - 1],
                                             vLastLineVal[iPixel], dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ);
            }
            else
            {
                for (; iPixel < oOutExtent.xSize(); iPixel++)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }

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

        /* set up initial point on the scanline */
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX];

        if (adjustHeightInRange(0, iLine - nY, vThisLineVal[nX]))
        {
            double dfZ =
                CalcHeightLine(iLine - nY, vLastLineVal[nX], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] =
                    std::max(0.0, (dfZ - vThisLineVal[nX] + dfGroundLevel));

            setVisibility(nX, dfZ);
        }
        else
        {
            vResult[nX] = oOpts.outOfRangeVal;
            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = oOpts.outOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];

            if (adjustHeightInRange(nX - iPixel, iLine - nY,
                                    vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        nX - iPixel, iLine - nY, vThisLineVal[iPixel + 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ =
                        nX - iPixel >= iLine - nY
                            ? CalcHeightEdge(iLine - nY, nX - iPixel,
                                             vLastLineVal[iPixel + 1],
                                             vThisLineVal[iPixel + 1],
                                             dfZObserver)
                            : CalcHeightEdge(nX - iPixel, iLine - nY,
                                             vLastLineVal[iPixel + 1],
                                             vLastLineVal[iPixel], dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < oOutExtent.xSize(); iPixel++)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];

            if (adjustHeightInRange(iPixel - nX, iLine - nY,
                                    vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        iPixel - nX, iLine - nY, vThisLineVal[iPixel - 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ =
                        iPixel - nX >= iLine - nY
                            ? CalcHeightEdge(iLine - nY, iPixel - nX,
                                             vLastLineVal[iPixel - 1],
                                             vThisLineVal[iPixel - 1],
                                             dfZObserver)
                            : CalcHeightEdge(iPixel - nX, iLine - nY,
                                             vLastLineVal[iPixel - 1],
                                             vLastLineVal[iPixel], dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ);
            }
            else
            {
                for (; iPixel < oOutExtent.xSize(); iPixel++)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }

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
