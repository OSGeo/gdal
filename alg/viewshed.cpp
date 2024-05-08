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

void Viewshed::setVisibility(int iPixel, double dfZ, std::vector<double>& vHeight)
{
    if (vHeight[iPixel] + oOpts.targetHeight < dfZ)
        vResult[iPixel] = oOpts.invisibleVal;
    else
        vResult[iPixel] = oOpts.visibleVal;

    if (vHeight[iPixel] < dfZ)
        vHeight[iPixel] = dfZ;
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

bool Viewshed::createOutputDataset(int nXSize, int nYSize)
{
    GDALDriverManager *hMgr = GetGDALDriverManager();
    GDALDriver *hDriver = hMgr->GetDriverByName(oOpts.outputFormat.c_str());
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return false;
    }

    /* create output raster */
    poDstDS.reset(hDriver->Create(oOpts.outputFilename.c_str(), nXSize, nYSize, 1,
                oOpts.outputMode == OutputMode::Normal ? GDT_Byte : GDT_Float64,
                const_cast<char **>(oOpts.creationOpts.List())));
    if (!poDstDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                oOpts.outputFilename.c_str());
        return false;
    }
    return true;
}

bool Viewshed::allocate(int nXSize)
{
    try
    {
        vFirstLineVal.resize(nXSize);
        vLastLineVal.resize(nXSize);
        vThisLineVal.resize(nXSize);
        vResult.resize(nXSize);

        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult.resize(nXSize);
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate vectors for viewshed");
        return false;
    }
    return true;
}

bool Viewshed::run(GDALRasterBandH hBand, GDALProgressFunc pfnProgress,
                   void *pProgressArg)
{
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

    std::array<double, 6> adfInvTransform;
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

    int nXSize = GDALGetRasterBandXSize(hBand);
    int nYSize = GDALGetRasterBandYSize(hBand);

    if (nX < 0 || nX > nXSize || nY < 0 || nY > nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The observer location falls outside of the DEM area");
        return false;
    }

    /* calculate the area of interest */
    constexpr double EPSILON = 1e-8;

    int nXStart = 0;
    int nYStart = 0;
    int nXStop = nXSize;
    int nYStop = nYSize;
    if (oOpts.maxDistance > 0)
    {
        nXStart = static_cast<int>(std::floor(
            nX - adfInvTransform[1] * oOpts.maxDistance + EPSILON));
        nXStop = static_cast<int>(
            std::ceil(nX + adfInvTransform[1] * oOpts.maxDistance -
                      EPSILON) +
            1);
        nYStart =
            static_cast<int>(std::floor(
                nY - std::fabs(adfInvTransform[5]) * oOpts.maxDistance +
                EPSILON)) - (adfInvTransform[5] > 0 ? 1 : 0);
        nYStop = static_cast<int>(
            std::ceil(nY + std::fabs(adfInvTransform[5]) * oOpts.maxDistance - EPSILON) +
            (adfInvTransform[5] < 0 ? 1 : 0));
    }
    nXStart = std::max(nXStart, 0);
    nYStart = std::max(nYStart, 0);
    nXStop = std::min(nXStop, nXSize);
    nYStop = std::min(nYStop, nYSize);

    /* normalize horizontal index (0 - nXSize) */
    nXSize = nXStop - nXStart;
    nX -= nXStart;

    nYSize = nYStop - nYStart;

    if (nXSize == 0 || nYSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid target raster size");
        return false;
    }

    if (!allocate(nXSize))
        return false;


    if (!createOutputDataset(nXSize, nYSize))
        return false;

    /* copy srs */
    if (hSrcDS)
        poDstDS->SetSpatialRef(
            GDALDataset::FromHandle(hSrcDS)->GetSpatialRef());

    std::array<double, 6> adfDstTransform;
    adfDstTransform[0] = adfTransform[0] + adfTransform[1] * nXStart + adfTransform[2] * nYStart;
    adfDstTransform[1] = adfTransform[1];
    adfDstTransform[2] = adfTransform[2];
    adfDstTransform[3] = adfTransform[3] + adfTransform[4] * nXStart + adfTransform[5] * nYStart;
    adfDstTransform[4] = adfTransform[4];
    adfDstTransform[5] = adfTransform[5];
    poDstDS->SetGeoTransform(adfDstTransform.data());

    auto hTargetBand = poDstDS->GetRasterBand(1);
    if (hTargetBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 oOpts.outputFilename.c_str());
        return false;
    }

    if (oOpts.nodataVal >= 0)
        GDALSetRasterNoDataValue(hTargetBand, oOpts.nodataVal);

    /* process first line */
    if (GDALRasterIO(hBand, GF_Read, nXStart, nY, nXSize, 1, vFirstLineVal.data(),
                     nXSize, 1, GDT_Float64, 0, 0))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "RasterIO error when reading DEM at position(%d, %d), size(%d, %d)",
            nXStart, nY, nXSize, 1);
        return false;
    }

    const double dfZObserver = oOpts.observer.z + vFirstLineVal[nX];

    /* If we can't get a SemiMajor axis from the SRS, it will be
     * SRS_WGS84_SEMIMAJOR
     */
    dfHeightAdjFactor = 0;
    const OGRSpatialReference *poDstSRS = poDstDS->GetSpatialRef();
    if (poDstSRS)
    {
        OGRErr eSRSerr;
        double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

        /* If we fetched the axis from the SRS, use it */
        if (eSRSerr != OGRERR_FAILURE)
            dfHeightAdjFactor = oOpts.curveCoeff / (dfSemiMajor * 2.0);
        else
            CPLDebug("GDALViewshedGenerate",
                     "Unable to fetch SemiMajor axis from spatial reference");
    }

    /* mark the observer point as visible */
    double dfGroundLevel = 0;
    if (oOpts.outputMode == OutputMode::DEM)
        dfGroundLevel = vFirstLineVal[nX];

    vResult[nX] = oOpts.visibleVal;

    //ABELL - Do we care about this conditional?
    if (oOpts.outputMode != OutputMode::Normal)
        vHeightResult[nX] = dfGroundLevel;

    dfGroundLevel = 0;
    if (nX > 0)
    {
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vFirstLineVal[nX - 1];
        CPL_IGNORE_RET_VAL(adjustHeightInRange(1, 0, vFirstLineVal[nX - 1]));
        vResult[nX - 1] = oOpts.visibleVal;
        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult[nX - 1] = dfGroundLevel;
    }
    if (nX < nXSize - 1)
    {
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vFirstLineVal[nX + 1];
        CPL_IGNORE_RET_VAL(adjustHeightInRange(1, 0, vFirstLineVal[nX + 1]));
        vResult[nX + 1] = oOpts.visibleVal;
        if (oOpts.outputMode != OutputMode::Normal)
            vHeightResult[nX + 1] = dfGroundLevel;
    }

    /* process left direction */
    for (int iPixel = nX - 2; iPixel >= 0; iPixel--)
    {
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vFirstLineVal[iPixel];

        if (adjustHeightInRange(nX - iPixel, 0, vFirstLineVal[iPixel]))
        {
            double dfZ = CalcHeightLine(
                nX - iPixel, vFirstLineVal[iPixel + 1], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] = std::max(
                    0.0, (dfZ - vFirstLineVal[iPixel] + dfGroundLevel));

            setVisibility(iPixel, dfZ, vFirstLineVal);
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
    for (int iPixel = nX + 2; iPixel < nXSize; iPixel++)
    {
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vFirstLineVal[iPixel];
        if (adjustHeightInRange(iPixel - nX, 0, vFirstLineVal[iPixel]))
        {
            double dfZ = CalcHeightLine(
                iPixel - nX, vFirstLineVal[iPixel - 1], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[iPixel] = std::max(
                    0.0, (dfZ - vFirstLineVal[iPixel] + dfGroundLevel));

            setVisibility(iPixel, dfZ, vFirstLineVal);
        }
        else
        {
            for (; iPixel < nXSize; iPixel++)
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
    if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - nYStart, nXSize, 1, data,
                     nXSize, 1, dataType, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RasterIO error when writing target raster at position "
                 "(%d,%d), size (%d,%d)",
                 0, nY - nYStart, nXSize, 1);
        return false;
    }

    /* scan upwards */
    vLastLineVal = vFirstLineVal;
    for (int iLine = nY - 1; iLine >= nYStart; iLine--)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
                         vThisLineVal.data(), nXSize, 1, GDT_Float64, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when reading DEM at position (%d,%d), "
                     "size (%d,%d)",
                     nXStart, iLine, nXSize, 1);
            return false;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX];
        if (adjustHeightInRange(0, nY - iLine, vThisLineVal[nX]))
        {
            double dfZ =
                CalcHeightLine(nY - iLine, vLastLineVal[nX], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = std::max(0.0, (dfZ - vThisLineVal[nX] + dfGroundLevel));

            setVisibility(nX, dfZ, vThisLineVal);
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
            if (adjustHeightInRange(nX - iPixel, nY - iLine, vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        nX - iPixel, nY - iLine, vThisLineVal[iPixel + 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ = nX - iPixel >= nY - iLine
                                  ? CalcHeightEdge(nY - iLine, nX - iPixel,
                                                   vLastLineVal[iPixel + 1],
                                                   vThisLineVal[iPixel + 1],
                                                   dfZObserver)
                                  : CalcHeightEdge(nX - iPixel, nY - iLine,
                                                   vLastLineVal[iPixel + 1],
                                                   vLastLineVal[iPixel],
                                                   dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ, vThisLineVal);
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
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];

            if (adjustHeightInRange(iPixel - nX, nY - iLine, vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        iPixel - nX, nY - iLine, vThisLineVal[iPixel - 1],
                        vLastLineVal[iPixel], dfZObserver);
                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ = iPixel - nX >= nY - iLine
                                  ? CalcHeightEdge(nY - iLine, iPixel - nX,
                                                   vLastLineVal[iPixel - 1],
                                                   vThisLineVal[iPixel - 1],
                                                   dfZObserver)
                                  : CalcHeightEdge(iPixel - nX, nY - iLine,
                                                   vLastLineVal[iPixel - 1],
                                                   vLastLineVal[iPixel],
                                                   dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ, vThisLineVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                         data, nXSize, 1, dataType, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when writing target raster at position "
                     "(%d,%d), size (%d,%d)",
                     0, iLine - nYStart, nXSize, 1);
            return false;
        }

        std::swap(vLastLineVal, vThisLineVal);

        if (!pfnProgress((nY - iLine) / static_cast<double>(nYSize), "",
                         pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return false;
        }
    }

    /* scan downwards */
    vLastLineVal = vFirstLineVal;
    for (int iLine = nY + 1; iLine < nYStop; iLine++)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
                         vThisLineVal.data(), nXSize, 1, GDT_Float64, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when reading DEM at position (%d,%d), "
                     "size (%d,%d)",
                     nXStart, iLine, nXStop - nXStart, 1);
            return false;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = 0;
        if (oOpts.outputMode == OutputMode::DEM)
            dfGroundLevel = vThisLineVal[nX];

        if (adjustHeightInRange(0, iLine - nY, vThisLineVal[nX]))
        {
            double dfZ =
                CalcHeightLine(iLine - nY, vLastLineVal[nX], dfZObserver);

            if (oOpts.outputMode != OutputMode::Normal)
                vHeightResult[nX] = std::max(0.0, (dfZ - vThisLineVal[nX] + dfGroundLevel));

            setVisibility(nX, dfZ, vThisLineVal);
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

            if (adjustHeightInRange(nX - iPixel, iLine - nY, vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        nX - iPixel, iLine - nY, vThisLineVal[iPixel + 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ = nX - iPixel >= iLine - nY
                                  ? CalcHeightEdge(iLine - nY, nX - iPixel,
                                                   vLastLineVal[iPixel + 1],
                                                   vThisLineVal[iPixel + 1],
                                                   dfZObserver)
                                  : CalcHeightEdge(nX - iPixel, iLine - nY,
                                                   vLastLineVal[iPixel + 1],
                                                   vLastLineVal[iPixel],
                                                   dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ, vThisLineVal);
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
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = 0;
            if (oOpts.outputMode == OutputMode::DEM)
                dfGroundLevel = vThisLineVal[iPixel];

            if (adjustHeightInRange(iPixel - nX, iLine - nY, vThisLineVal[iPixel]))
            {
                double dfDiagZ = 0;
                double dfEdgeZ = 0;
                if (oOpts.cellMode != CellMode::Edge)
                    dfDiagZ = CalcHeightDiagonal(
                        iPixel - nX, iLine - nY, vThisLineVal[iPixel - 1],
                        vLastLineVal[iPixel], dfZObserver);

                if (oOpts.cellMode != CellMode::Diagonal)
                    dfEdgeZ = iPixel - nX >= iLine - nY
                                  ? CalcHeightEdge(iLine - nY, iPixel - nX,
                                                   vLastLineVal[iPixel - 1],
                                                   vThisLineVal[iPixel - 1],
                                                   dfZObserver)
                                  : CalcHeightEdge(iPixel - nX, iLine - nY,
                                                   vLastLineVal[iPixel - 1],
                                                   vLastLineVal[iPixel],
                                                   dfZObserver);

                double dfZ = calcHeight(dfDiagZ, dfEdgeZ);

                if (oOpts.outputMode != OutputMode::Normal)
                    vHeightResult[iPixel] = std::max(
                        0.0, (dfZ - vThisLineVal[iPixel] + dfGroundLevel));

                setVisibility(iPixel, dfZ, vThisLineVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    vResult[iPixel] = oOpts.outOfRangeVal;
                    if (oOpts.outputMode != OutputMode::Normal)
                        vHeightResult[iPixel] = oOpts.outOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                         data, nXSize, 1, dataType, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RasterIO error when writing target raster at position "
                     "(%d,%d), size (%d,%d)",
                     0, iLine - nYStart, nXSize, 1);
            return false;
        }

        std::swap(vLastLineVal, vThisLineVal);

        if (!pfnProgress((iLine - nYStart) / static_cast<double>(nYSize), "",
                         pProgressArg))
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
