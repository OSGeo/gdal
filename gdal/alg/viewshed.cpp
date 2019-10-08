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

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cmath>
#include <cstring>
#include <array>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "ogr_api.h"
#include "ogr_core.h"
#include "commonutils.h"

CPL_CVSID("$Id$")



#define EARTH_DIAMETER 12741994.0

inline static void SetVisibility(int iPixel, double dfZ, double dfZTarget, double* padfZVal,
    std::vector<GByte>& vResult, GByte byVisibleVal, GByte byInvisibleVal)
{
    if (padfZVal[iPixel] + dfZTarget < dfZ)
    {
        padfZVal[iPixel] = dfZ;
        vResult[iPixel] = byInvisibleVal;
    }
    else
        vResult[iPixel] = byVisibleVal;
}

inline static bool AdjustHeightInRange(const double* adfGeoTransform, int iPixel, int iLine, double& dfHeight, double dfDistance2, double dfCurvCoeff)
{
    if (dfDistance2 <= 0 && dfCurvCoeff == 0)
        return true;

    double dfX = adfGeoTransform[1] * iPixel + adfGeoTransform[2] * iLine;
    double dfY = adfGeoTransform[4] * iPixel + adfGeoTransform[5] * iLine;
    double dfR2 = dfX * dfX + dfY * dfY;

    /* calc adjustment */
    if (dfCurvCoeff != 0)
        dfHeight -= dfCurvCoeff * dfR2 / EARTH_DIAMETER;

    if (dfDistance2 > 0 && dfR2 > dfDistance2)
        return false;

    return true;
}

inline static double CalcHeightLine(int i, double Za, double Zo)
{
    if (i == 1)
        return Za;
    else
        return (Za - Zo) / (i - 1) + Za;
}

inline static double CalcHeightDiagonal(int i, int j, double Za, double Zb, double Zo)
{
    return ((Za - Zo) * i + (Zb - Zo) * j) / (i + j - 1) + Zo;
}

inline static double CalcHeightEdge(int i, int j, double Za, double Zb, double Zo)
{
    if (i == j)
        return CalcHeightLine(i, Za, Zo);
    else
        return ((Za - Zo) * i + (Zb - Zo) * (j - i)) / (j - 1) + Zo;
}

inline static double CalcHeight(double dfZ, double dfZ2, GDALViewshedMode eMode)
{
    if (eMode == GVM_Edge)
        return dfZ2;
    else if (eMode == GVM_Max)
        return (std::max)(dfZ, dfZ2);
    else if (eMode == GVM_Min)
        return (std::min)(dfZ, dfZ2);
    else
        return dfZ;
}


/************************************************************************/
/*                        GDALViewshedGenerate()                         */
/************************************************************************/

/**
 * Create viewshed from raster DEM.
 *
 * This algorithm will generate contour vectors for the input raster band
 * on the requested set of contour levels.  The vector contours are written
 * to the passed in OGR vector layer.  Also, a NODATA value may be specified
 * to identify pixels that should not be considered in contour line generation.
 *
 * The gdal/apps/gdal_viewshed.cpp mainline can be used as an example of
 * how to use this function.
 *
 * ALGORITHM RULES
 *
 * @param hBand The band to read raster data from.  The whole band will be
 * processed.
 *
 * @param pszTargetRasterName raster datasource name (created if given)
 *
 * @param dfObserverX observer X value (in SRS units)
 *
 * @param dfObserverY observer Y value (in SRS units)
 *
 * @param dfObserverHeight observer height
 *
 * @param dfTargetHeight target height. (default 0)
 *
 * @param  dfVisibleVal pixel value for visibility (default 0)
 *
 * @param dfInvisibleVal pixel value for invisibility (default 255)
 *
 * @param dfOutOfRangeVal Out of range value
 *
 * @param dfNoDataVal NODATA value to use
 *
 * @param dfCurvCoeff curvature coefficient
 *
 * @param eMode which GDALViewshedMode to use
 *
 * @param dfMaxDistance maximum distance range to compute viewshed
 *
 * @param pfnProgress A GDALProgressFunc that may be used to report progress
 * to the user, or to interrupt the algorithm.  May be NULL if not required.
 *
 * @param pProgressArg The callback data for the pfnProgress function.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALViewshedGenerate(GDALRasterBandH hBand, const char* pszTargetRasterName,
                    double dfObserverX, double dfObserverY, double dfObserverHeight,
                    double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
                    double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
                    GDALViewshedMode eMode, double dfMaxDistance,
                    GDALProgressFunc pfnProgress, void *pProgressArg, CSLConstList /*papszExtraOptions*/)

{
    VALIDATE_POINTER1( hBand, "GDALViewshedGenerate", CE_Failure );

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    if( !pfnProgress( 0.0, "", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    GByte byNoDataVal = dfNoDataVal >= 0 ? static_cast<GByte>(dfNoDataVal) : 0;
    GByte byVisibleVal = dfVisibleVal >= 0 ? static_cast<GByte>(dfVisibleVal) : 255;
    GByte byInvisibleVal = dfInvisibleVal >= 0 ? static_cast<GByte>(dfInvisibleVal) : 0;
    GByte byOutOfRangeVal = dfOutOfRangeVal >= 0 ? static_cast<GByte>(dfOutOfRangeVal) : 0;

    /* set up geotransformation */
    double adfGeoTransform[6];
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    GDALDatasetH hSrcDS = GDALGetBandDataset( hBand );
    if( hSrcDS != nullptr )
        GDALGetGeoTransform( hSrcDS, adfGeoTransform );

    double adfInvGeoTransform[6];
    if (!GDALInvGeoTransform(adfGeoTransform, adfInvGeoTransform))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return CE_Failure;
    }

    /* calculate observer position */
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvGeoTransform, dfObserverX, dfObserverY, &dfX, &dfY);
    int nX = static_cast<int>(std::round(dfX));
    int nY = static_cast<int>(std::round(dfY));

    int nXSize = GDALGetRasterBandXSize( hBand );
    int nYSize = GDALGetRasterBandYSize( hBand );

    if (nX < 0 || nX > nXSize || nY < 0 || nY > nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "The observer location falls outside of the DEM area");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    /* calculate the area of interest */
    int nXStart = dfMaxDistance > 0? (std::max)(0, static_cast<int>(floor(nX - adfInvGeoTransform[1] * dfMaxDistance))) : 0;
    int nXStop = dfMaxDistance > 0? (std::min)(nXSize, static_cast<int>(ceil(nX + adfInvGeoTransform[1] * dfMaxDistance) + 1)) : nXSize;
    int nYStart = dfMaxDistance > 0? (std::max)(0, static_cast<int>(floor(nY + adfInvGeoTransform[5] * dfMaxDistance))) : 0;
    int nYStop = dfMaxDistance > 0 ? (std::min)(nYSize, static_cast<int>(ceil(nY - adfInvGeoTransform[5] * dfMaxDistance) + 1)) : nYSize;

    /* normalize horizontal index (0 - nXSize) */
    nXSize = nXStop - nXStart;
    nX -= nXStart;

    std::vector<double> vFirstLineVal;
    std::vector<double> vLastLineVal;
    std::vector<double> vThisLineVal;
    std::vector<GByte> vResult;

    try
    {
        vFirstLineVal.resize(nXSize);
        vLastLineVal.resize(nXSize);
        vThisLineVal.resize(nXSize);
        vResult.resize(nXSize);
    } catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate vectors for viewshed");
        return CE_Failure;
    }

    double *padfFirstLineVal = static_cast<double *>(
        vFirstLineVal.data());
    double *padfLastLineVal = static_cast<double *>(
        vLastLineVal.data());
    double *padfThisLineVal = static_cast<double *>(
        vThisLineVal.data());
    GByte *pabyResult = static_cast<GByte *>(
        vResult.data());

    GDALRasterBandH hTargetBand = nullptr;
    GDALDatasetH hDstDS = nullptr;
    if (pszTargetRasterName != nullptr)
    {
        std::vector<CPLString> aoDrivers =
            GetOutputDriversFor(pszTargetRasterName, GDAL_OF_RASTER);
        if (aoDrivers.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot guess driver for %s", pszTargetRasterName);
            return CE_Failure;
        }
        else
        {
            if (aoDrivers.size() > 1)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Several drivers matching %s extension. Using %s",
                    CPLGetExtension(pszTargetRasterName), aoDrivers[0].c_str());
            }
            GDALDriverH hDriver = GDALGetDriverByName(aoDrivers[0].c_str());
            if (hDriver == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot get driver for %s", aoDrivers[0].c_str());
                return CE_Failure;
            }
            /* create output raster */
            hDstDS = GDALCreate(hDriver, pszTargetRasterName, nXSize, nYStop - nYStart, 1,
                    GDT_Byte, nullptr);
            if (hDstDS == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot create dataset for %s", pszTargetRasterName);
                return CE_Failure;
            }

            /* copy srs */
            GDALSetSpatialRef(hDstDS, GDALGetSpatialRef(hSrcDS));

            std::array<double, 6> adfDstGeoTransform;
            adfDstGeoTransform[0] = adfGeoTransform[0] + adfGeoTransform[1] * nXStart;
            adfDstGeoTransform[1] = adfGeoTransform[1];
            adfDstGeoTransform[2] = adfGeoTransform[2];
            adfDstGeoTransform[3] = adfGeoTransform[3] + adfGeoTransform[5] * nYStart;
            adfDstGeoTransform[4] = adfGeoTransform[4];
            adfDstGeoTransform[5] = adfGeoTransform[5];
            GDALSetGeoTransform(hDstDS, adfDstGeoTransform.data());

            hTargetBand = GDALGetRasterBand(hDstDS, 1);
            if (hTargetBand == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot get band for %s", pszTargetRasterName);
                return CE_Failure;
            }

            if (dfNoDataVal >= 0)
                GDALSetRasterNoDataValue(hTargetBand, byNoDataVal);
        }
    }

    /* process first line */
    if (GDALRasterIO(hBand, GF_Read, nXStart, nY, nXSize, 1,
        padfFirstLineVal, nXSize, 1, GDT_Float64, 0, 0) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "RasterIO error when reading DEM");
        return CE_Failure;
    }

    double dfZObserver = dfObserverHeight + padfFirstLineVal[nX];
    double dfZ = 0.0;
    int iPixel, iLine;
    double dfDistance2 = dfMaxDistance * dfMaxDistance;

    /* mark the observer point as visible */
    pabyResult[nX] = byVisibleVal;
    if (nX > 0)
    {
        AdjustHeightInRange(adfGeoTransform,
                            1,
                            0,
                            padfFirstLineVal[nX - 1],
                            dfDistance2,
                            dfCurvCoeff);
        pabyResult[nX - 1] = byVisibleVal;
    }
    if (nX < nXSize - 1)
    {
        AdjustHeightInRange(adfGeoTransform,
                            1,
                            0,
                            padfFirstLineVal[nX + 1],
                            dfDistance2,
                            dfCurvCoeff);
        pabyResult[nX + 1] = byVisibleVal;
    }

    /* process left direction */
    for (iPixel = nX - 2; iPixel >= 0; iPixel--)
    {
        bool adjusted = AdjustHeightInRange(adfGeoTransform,
                                            nX - iPixel,
                                            0,
                                            padfFirstLineVal[iPixel],
                                            dfDistance2,
                                            dfCurvCoeff);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nX - iPixel,
                                 padfFirstLineVal[iPixel + 1],
                                 dfZObserver);
            SetVisibility(  iPixel,
                            dfZ,
                            dfTargetHeight,
                            padfFirstLineVal,
                            vResult,
                            byVisibleVal,
                            byInvisibleVal);
        }
        else
        {
            for (; iPixel >= 0; iPixel--)
                pabyResult[iPixel] = byOutOfRangeVal;
        }
    }
    /* process right direction */
    for (iPixel = nX + 2; iPixel < nXSize; iPixel++)
    {
        bool adjusted = AdjustHeightInRange(adfGeoTransform,
                                            iPixel - nX,
                                            0,
                                            padfFirstLineVal[iPixel],
                                            dfDistance2,
                                            dfCurvCoeff);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iPixel - nX,
                                 padfFirstLineVal[iPixel - 1],
                                 dfZObserver);
            SetVisibility(iPixel,
                          dfZ,
                          dfTargetHeight,
                          padfFirstLineVal,
                          vResult,
                          byVisibleVal,
                          byInvisibleVal);
        }
        else
        {
            for (; iPixel < nXSize; iPixel++)
                pabyResult[iPixel] = byOutOfRangeVal;
        }
    }
    /* write result line */
    if (hTargetBand != nullptr)
    {
        if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - nYStart, nXSize, 1,
            pabyResult, nXSize, 1, GDT_Byte, 0, 0) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when writing target raster");
            return CE_Failure;
        }
    }

    /* scan upwards */
    memcpy(padfLastLineVal, padfFirstLineVal, nXSize * sizeof(double));
    for (iLine = nY - 1; iLine >= nYStart && eErr == CE_None; iLine--)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
            padfThisLineVal, nXSize, 1, GDT_Float64, 0, 0) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM");
            return CE_Failure;
        }

        /* set up initial point on the scanline */
        bool adjusted = AdjustHeightInRange(adfGeoTransform,
                                            0,
                                            nY - iLine,
                                            padfThisLineVal[nX],
                                            dfDistance2,
                                            dfCurvCoeff);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nY - iLine,
                                 padfLastLineVal[nX],
                                 dfZObserver);
            SetVisibility(nX,
                          dfZ,
                          dfTargetHeight,
                          padfThisLineVal,
                          vResult,
                          byVisibleVal,
                          byInvisibleVal);
        }
        else
        {
            pabyResult[nX] = byOutOfRangeVal;
            --iLine;
            for (; iLine >= nYStart && eErr == CE_None; iLine--)
                pabyResult[nX] = byOutOfRangeVal;
        }

        /* process left direction */
        for (iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            bool left_adjusted = AdjustHeightInRange(adfGeoTransform,
                                                     nX - iPixel,
                                                     nY - iLine,
                                                     padfThisLineVal[iPixel],
                                                     dfDistance2,
                                                     dfCurvCoeff);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(nX - iPixel,
                                             nY - iLine,
                                             padfThisLineVal[iPixel + 1],
                                             padfLastLineVal[iPixel],
                                             dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 = nX - iPixel >= nY - iLine ?
                        CalcHeightEdge(nY - iLine,
                                       nX - iPixel,
                                       padfLastLineVal[iPixel + 1],
                                       padfThisLineVal[iPixel + 1],
                                       dfZObserver) :
                        CalcHeightEdge(nX - iPixel,
                                       nY - iLine,
                                       padfLastLineVal[iPixel + 1],
                                       padfLastLineVal[iPixel],
                                       dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                SetVisibility(iPixel,
                              dfZ,
                              dfTargetHeight,
                              padfThisLineVal,
                              vResult,
                              byVisibleVal,
                              byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                    pabyResult[iPixel] = byOutOfRangeVal;
            }
        }
        /* process right direction */
        for (iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            bool right_adjusted = AdjustHeightInRange(adfGeoTransform,
                                                      iPixel - nX,
                                                      nY - iLine,
                                                      padfThisLineVal[iPixel],
                                                      dfDistance2,
                                                      dfCurvCoeff);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(iPixel - nX,
                                             nY - iLine,
                                             padfThisLineVal[iPixel - 1],
                                             padfLastLineVal[iPixel],
                                             dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 = iPixel - nX >= nY - iLine ?
                        CalcHeightEdge(nY - iLine,
                                       iPixel - nX,
                                       padfLastLineVal[iPixel - 1],
                                       padfThisLineVal[iPixel - 1],
                                       dfZObserver) :
                        CalcHeightEdge(iPixel - nX,
                                       nY - iLine,
                                       padfLastLineVal[iPixel - 1],
                                       padfLastLineVal[iPixel],
                                       dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                SetVisibility(iPixel,
                              dfZ,
                              dfTargetHeight,
                              padfThisLineVal,
                              vResult,
                              byVisibleVal,
                              byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                    pabyResult[iPixel] = byOutOfRangeVal;
            }
        }

        /* write result line */
        if (hTargetBand != nullptr)
        {
            if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                pabyResult, nXSize, 1, GDT_Byte, 0, 0) != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "RasterIO error when writing target raster");
                return CE_Failure;
            }
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if (!pfnProgress((nYStart - iLine + 1) / static_cast<double>(nYStop),
                "", pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return CE_Failure;
        }
    }
    /* scan downwards */
    memcpy(padfLastLineVal, padfFirstLineVal, nXSize * sizeof(double));
    for(iLine = nY + 1; iLine < nYStop && eErr == CE_None; iLine++ )
    {
        if (GDALRasterIO( hBand, GF_Read, nXStart, iLine, nXStop - nXStart, 1,
            padfThisLineVal, nXStop - nXStart, 1, GDT_Float64, 0, 0 ) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM");
            return CE_Failure;
        }

        /* set up initial point on the scanline */
        bool adjusted = AdjustHeightInRange(adfGeoTransform,
                                            0,
                                            iLine - nY,
                                            padfThisLineVal[nX],
                                            dfDistance2,
                                            dfCurvCoeff);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iLine - nY,
                                 padfLastLineVal[nX],
                                 dfZObserver);
            SetVisibility(nX,
                          dfZ,
                          dfTargetHeight,
                          padfThisLineVal,
                          vResult,
                          byVisibleVal,
                          byInvisibleVal);
        }
        else
        {
            for (; iLine < nYStop; iLine++)
                pabyResult[iPixel] = byOutOfRangeVal;
        }

        /* process left direction */
        for (iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            bool left_adjusted = AdjustHeightInRange(adfGeoTransform,
                                                     nX - iPixel,
                                                     iLine - nY,
                                                     padfThisLineVal[iPixel],
                                                     dfDistance2,
                                                     dfCurvCoeff);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(nX - iPixel, iLine - nY,
                        padfThisLineVal[iPixel + 1], padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 = nX - iPixel >= iLine - nY ?
                        CalcHeightEdge(iLine - nY,
                                       nX - iPixel,
                                       padfLastLineVal[iPixel + 1],
                                       padfThisLineVal[iPixel + 1],
                                       dfZObserver) :
                        CalcHeightEdge(nX - iPixel,
                                       iLine - nY,
                                       padfLastLineVal[iPixel + 1],
                                       padfLastLineVal[iPixel],
                                       dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                SetVisibility(iPixel,
                              dfZ,
                              dfTargetHeight,
                              padfThisLineVal,
                              vResult,
                              byVisibleVal,
                              byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                    pabyResult[iPixel] = byOutOfRangeVal;
            }
        }
        /* process right direction */
        for (iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            bool right_adjusted = AdjustHeightInRange(adfGeoTransform,
                                                      iPixel - nX,
                                                      iLine - nY,
                                                      padfThisLineVal[iPixel],
                                                      dfDistance2,
                                                      dfCurvCoeff);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(iPixel - nX,
                                             iLine - nY,
                                             padfThisLineVal[iPixel - 1],
                                             padfLastLineVal[iPixel],
                                             dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 = iPixel - nX >= iLine - nY ?
                        CalcHeightEdge(iLine - nY,
                                       iPixel - nX,
                                       padfLastLineVal[iPixel - 1],
                                       padfThisLineVal[iPixel - 1],
                                       dfZObserver) :
                        CalcHeightEdge(iPixel - nX,
                                       iLine - nY,
                                       padfLastLineVal[iPixel - 1],
                                       padfLastLineVal[iPixel],
                                       dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                SetVisibility(iPixel,
                              dfZ,
                              dfTargetHeight,
                              padfThisLineVal,
                              vResult,
                              byVisibleVal,
                              byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                    pabyResult[iPixel] = byOutOfRangeVal;
            }
        }

        /* write result line */
        if (hTargetBand != nullptr)
        {
            if (GDALRasterIO(hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
                pabyResult, nXSize, 1, GDT_Byte, 0, 0) != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "RasterIO error when writing target raster");
                return CE_Failure;
            }
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if(!pfnProgress((iLine + 1) / static_cast<double>(nYStop),
                         "", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return CE_Failure;
        }
    }

    if (hDstDS != nullptr)
        GDALClose(hDstDS);


    return CE_None;
}
