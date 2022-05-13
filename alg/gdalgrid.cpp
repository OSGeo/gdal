/******************************************************************************
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Implementation of GDAL scattered data gridder.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdalgrid.h"
#include "gdalgrid_priv.h"

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <limits>
#include <map>
#include <utility>

#include "cpl_conv.h"
#include "cpl_cpu_features.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "cpl_quad_tree.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_worker_thread_pool.h"
#include "gdal.h"

CPL_CVSID("$Id$")

constexpr double TO_RADIANS = M_PI / 180.0;

/************************************************************************/
/*                        GDALGridGetPointBounds()                      */
/************************************************************************/

static void GDALGridGetPointBounds( const void* hFeature, CPLRectObj* pBounds )
{
    const GDALGridPoint* psPoint = static_cast<const GDALGridPoint*>(hFeature);
    GDALGridXYArrays* psXYArrays = psPoint->psXYArrays;
    const int i = psPoint->i;
    const double dfX = psXYArrays->padfX[i];
    const double dfY = psXYArrays->padfY[i];
    pBounds->minx = dfX;
    pBounds->miny = dfY;
    pBounds->maxx = dfX;
    pBounds->maxy = dfY;
}

/************************************************************************/
/*                   GDALGridInverseDistanceToAPower()                  */
/************************************************************************/

/**
 * Inverse distance to a power.
 *
 * The Inverse Distance to a Power gridding method is a weighted average
 * interpolator. You should supply the input arrays with the scattered data
 * values including coordinates of every data point and output grid geometry.
 * The function will compute interpolated value for the given position in
 * output grid.
 *
 * For every grid node the resulting value \f$Z\f$ will be calculated using
 * formula:
 *
 * \f[
 *      Z=\frac{\sum_{i=1}^n{\frac{Z_i}{r_i^p}}}{\sum_{i=1}^n{\frac{1}{r_i^p}}}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$r_i\f$ is an Euclidean distance from the grid node
 *           to point \f$i\f$,
 *      <li> \f$p\f$ is a weighting power,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 *  In this method the weighting factor \f$w\f$ is
 *
 *  \f[
 *      w=\frac{1}{r^p}
 *  \f]
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridInverseDistanceToAPowerOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridInverseDistanceToAPower( const void *poOptionsIn, GUInt32 nPoints,
                                 const double *padfX, const double *padfY,
                                 const double *padfZ,
                                 double dfXPoint, double dfYPoint,
                                 double *pdfValue,
                                 CPL_UNUSED void* hExtraParamsIn)
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridInverseDistanceToAPowerOptions * const poOptions =
        static_cast<const GDALGridInverseDistanceToAPowerOptions *>(
            poOptionsIn);

    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    const double dfPowerDiv2 = poOptions->dfPower / 2;
    const double dfSmoothing = poOptions->dfSmoothing;
    const GUInt32 nMaxPoints = poOptions->nMaxPoints;
    double dfNominator = 0.0;
    double dfDenominator = 0.0;
    GUInt32 n = 0;

    for( GUInt32 i = 0; i < nPoints; i++ )
    {
        double dfRX = padfX[i] - dfXPoint;
        double dfRY = padfY[i] - dfYPoint;
        const double dfR2 =
            dfRX * dfRX + dfRY * dfRY + dfSmoothing * dfSmoothing;

        if( bRotated )
        {
            const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            // If the test point is close to the grid node, use the point
            // value directly as a node value to avoid singularity.
            if( dfR2 < 0.0000000000001 )
            {
                *pdfValue = padfZ[i];
                return CE_None;
            }

            const double dfW = pow( dfR2, dfPowerDiv2 );
            const double dfInvW = 1.0 / dfW;
            dfNominator += dfInvW * padfZ[i];
            dfDenominator += dfInvW;
            n++;
            if( nMaxPoints > 0 && n > nMaxPoints )
                break;
        }
    }

    if( n < poOptions->nMinPoints || dfDenominator == 0.0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfNominator / dfDenominator;
    }

    return CE_None;
}

/************************************************************************/
/*                   GDALGridInverseDistanceToAPowerNearestNeighbor()   */
/************************************************************************/

/**
 * Inverse distance to a power with nearest neighbor search, ideal when
 * max_points used.
 *
 * The Inverse Distance to a Power gridding method is a weighted average
 * interpolator. You should supply the input arrays with the scattered data
 * values including coordinates of every data point and output grid geometry.
 * The function will compute interpolated value for the given position in
 * output grid.
 *
 * For every grid node the resulting value \f$Z\f$ will be calculated using
 * formula for nearest matches:
 *
 * \f[
 *      Z=\frac{\sum_{i=1}^n{\frac{Z_i}{r_i^p}}}{\sum_{i=1}^n{\frac{1}{r_i^p}}}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$r_i\f$ is an Euclidean distance from the grid node
 *           to point \f$i\f$ (with an optional smoothing parameter \f$s\f$),
 *      <li> \f$p\f$ is a weighting power,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 *  In this method the weighting factor \f$w\f$ is
 *
 *  \f[
 *      w=\frac{1}{r^p}
 *  \f]
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridInverseDistanceToAPowerNearestNeighborOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridInverseDistanceToAPowerNearestNeighbor(
    const void *poOptionsIn, GUInt32 nPoints,
    const double *padfX, const double *padfY,
    const double *padfZ,
    double dfXPoint, double dfYPoint,
    double *pdfValue,
    void* hExtraParamsIn )
{
    const
    GDALGridInverseDistanceToAPowerNearestNeighborOptions *const poOptions =
      static_cast<
          const GDALGridInverseDistanceToAPowerNearestNeighborOptions *>(
          poOptionsIn);
    const double dfRadius = poOptions->dfRadius;
    const double dfSmoothing = poOptions->dfSmoothing;
    const double dfSmoothing2 = dfSmoothing * dfSmoothing;

    const GUInt32 nMaxPoints = poOptions->nMaxPoints;

    GDALGridExtraParameters* psExtraParams =
        static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    const double dfRPower2 = psExtraParams->dfRadiusPower2PreComp;
    const double dfRPower4 = psExtraParams->dfRadiusPower4PreComp;

    const double dfPowerDiv2 = psExtraParams->dfPowerDiv2PreComp;

    std::multimap<double, double> oMapDistanceToZValues;
    if( phQuadTree != nullptr )
    {
        const double dfSearchRadius = dfRadius;
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                const double dfR2 = dfRX * dfRX + dfRY * dfRY;
                // real distance + smoothing
                const double dfRsmoothed2 = dfR2 + dfSmoothing2;
                if( dfRsmoothed2 < 0.0000000000001 )
                {
                    *pdfValue = padfZ[i];
                    CPLFree(papsPoints);
                    return CE_None;
                }
                // is point within real distance?
                if( dfR2 <= dfRPower2 )
                {
                    oMapDistanceToZValues.insert(
                        std::make_pair(dfRsmoothed2, padfZ[i]) );
                }
            }
        }
        CPLFree(papsPoints);
    }
    else
    {
        for( GUInt32 i = 0; i < nPoints; i++ )
        {
            const double dfRX = padfX[i] - dfXPoint;
            const double dfRY = padfY[i] - dfYPoint;
            const double dfR2 = dfRX * dfRX + dfRY * dfRY;
            const double dfRsmoothed2 = dfR2 + dfSmoothing2;

            // Is this point located inside the search circle?
            if( dfRPower2 * dfRX * dfRX + dfRPower2 * dfRY * dfRY <= dfRPower4 )
            {
                // If the test point is close to the grid node, use the point
                // value directly as a node value to avoid singularity.
                if( dfRsmoothed2 < 0.0000000000001 )
                {
                    *pdfValue = padfZ[i];
                    return CE_None;
                }

                oMapDistanceToZValues.insert(std::make_pair(dfRsmoothed2, padfZ[i]) );
            }
        }
    }

    double dfNominator = 0.0;
    double dfDenominator = 0.0;
    GUInt32 n = 0;

    // Examine all "neighbors" within the radius (sorted by distance via the
    // multimap), and use the closest n points based on distance until the max
    // is reached.
    for( std::multimap<double, double>::iterator oMapDistanceToZValuesIter =
             oMapDistanceToZValues.begin();
         oMapDistanceToZValuesIter != oMapDistanceToZValues.end();
         ++oMapDistanceToZValuesIter)
    {
        const double dfR2 = oMapDistanceToZValuesIter->first;
        const double dfZ = oMapDistanceToZValuesIter->second;

        const double dfW = pow(dfR2, dfPowerDiv2);
        const double dfInvW = 1.0 / dfW;
        dfNominator += dfInvW * dfZ;
        dfDenominator += dfInvW;
        n++;
        if( nMaxPoints > 0 && n >= nMaxPoints )
        {
            break;
        }
    }

    if( n < poOptions->nMinPoints || dfDenominator == 0.0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfNominator / dfDenominator;
    }

    return CE_None;
}

/************************************************************************/
/*              GDALGridInverseDistanceToAPowerNoSearch()               */
/************************************************************************/

/**
 * Inverse distance to a power for whole data set.
 *
 * This is somewhat optimized version of the Inverse Distance to a Power
 * method. It is used when the search ellips is not set. The algorithm and
 * parameters are the same as in GDALGridInverseDistanceToAPower(), but this
 * implementation works faster, because of no search.
 *
 * @see GDALGridInverseDistanceToAPower()
 */

CPLErr
GDALGridInverseDistanceToAPowerNoSearch(
    const void *poOptionsIn, GUInt32 nPoints,
    const double *padfX, const double *padfY, const double *padfZ,
    double dfXPoint, double dfYPoint,
    double *pdfValue,
    void * /* hExtraParamsIn */)
{
    const GDALGridInverseDistanceToAPowerOptions  * const poOptions =
        static_cast<const GDALGridInverseDistanceToAPowerOptions *>(
            poOptionsIn);
    const double dfPowerDiv2 = poOptions->dfPower / 2.0;
    const double dfSmoothing = poOptions->dfSmoothing;
    const double dfSmoothing2 = dfSmoothing * dfSmoothing;
    double dfNominator = 0.0;
    double dfDenominator = 0.0;
    const bool bPower2 = dfPowerDiv2 == 1.0;

    GUInt32 i = 0;  // Used after if.
    if( bPower2 )
    {
        if( dfSmoothing2 > 0 )
        {
            for( i = 0; i < nPoints; i++ )
            {
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;
                const double dfR2 = dfRX * dfRX + dfRY * dfRY + dfSmoothing2;

                const double dfInvR2 = 1.0 / dfR2;
                dfNominator += dfInvR2 * padfZ[i];
                dfDenominator += dfInvR2;
            }
        }
        else
        {
            for( i = 0; i < nPoints; i++ )
            {
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;
                const double dfR2 = dfRX * dfRX + dfRY * dfRY;

                // If the test point is close to the grid node, use the point
                // value directly as a node value to avoid singularity.
                if( dfR2 < 0.0000000000001 )
                {
                    break;
                }

                const double dfInvR2 = 1.0 / dfR2;
                dfNominator += dfInvR2 * padfZ[i];
                dfDenominator += dfInvR2;
            }
        }
    }
    else
    {
        for( i = 0; i < nPoints; i++ )
        {
            const double dfRX = padfX[i] - dfXPoint;
            const double dfRY = padfY[i] - dfYPoint;
            const double dfR2 = dfRX * dfRX + dfRY * dfRY + dfSmoothing2;

            // If the test point is close to the grid node, use the point
            // value directly as a node value to avoid singularity.
            if( dfR2 < 0.0000000000001 )
            {
                break;
            }

            const double dfW = pow( dfR2, dfPowerDiv2 );
            const double dfInvW = 1.0 / dfW;
            dfNominator += dfInvW * padfZ[i];
            dfDenominator += dfInvW;
        }
    }

    if( i != nPoints )
    {
        *pdfValue = padfZ[i];
    }
    else
    if( dfDenominator == 0.0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfNominator / dfDenominator;
    }

    return CE_None;
}

/************************************************************************/
/*                        GDALGridMovingAverage()                       */
/************************************************************************/

/**
 * Moving average.
 *
 * The Moving Average is a simple data averaging algorithm. It uses a moving
 * window of elliptic form to search values and averages all data points
 * within the window. Search ellipse can be rotated by specified angle, the
 * center of ellipse located at the grid node. Also the minimum number of data
 * points to average can be set, if there are not enough points in window, the
 * grid node considered empty and will be filled with specified NODATA value.
 *
 * Mathematically it can be expressed with the formula:
 *
 * \f[
 *      Z=\frac{\sum_{i=1}^n{Z_i}}{n}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridMovingAverageOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridMovingAverage( const void *poOptionsIn, GUInt32 nPoints,
                       const double *padfX, const double *padfY,
                       const double *padfZ,
                       double dfXPoint, double dfYPoint, double *pdfValue,
                       CPL_UNUSED void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridMovingAverageOptions * const poOptions =
        static_cast<const GDALGridMovingAverageOptions *>(poOptionsIn);
    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;

    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfAccumulator = 0.0;

    GUInt32 n = 0;  // Used after for.
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    dfAccumulator += padfZ[i];
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        for( GUInt32 i = 0; i < nPoints; i++ )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                dfAccumulator += padfZ[i];
                n++;
            }
        }
    }

    

    if( n < poOptions->nMinPoints || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfAccumulator / n;
    }

    return CE_None;
}

/************************************************************************/
/*                        GDALGridNearestNeighbor()                     */
/************************************************************************/

/**
 * Nearest neighbor.
 *
 * The Nearest Neighbor method doesn't perform any interpolation or smoothing,
 * it just takes the value of nearest point found in grid node search ellipse
 * and returns it as a result. If there are no points found, the specified
 * NODATA value will be returned.
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridNearestNeighborOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridNearestNeighbor( const void *poOptionsIn, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         const double *padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         void* hExtraParamsIn)
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridNearestNeighborOptions * const poOptions =
        static_cast<const GDALGridNearestNeighborOptions *>(poOptionsIn);
    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    double dfR12 = dfRadius1 * dfRadius2;
    GDALGridExtraParameters* psExtraParams =
        static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* hQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    // If the nearest point will not be found, its value remains as NODATA.
    double dfNearestValue = poOptions->dfNoDataValue;
    // Nearest distance will be initialized with the distance to the first
    // point in array.
    double dfNearestR = std::numeric_limits<double>::max();
    GUInt32 i = 0;

    double dfSearchRadius = psExtraParams->dfInitialSearchRadius;
    if( hQuadTree != nullptr)
    {
        if( dfRadius1 > 0 )
            dfSearchRadius = poOptions->dfRadius1;
        CPLRectObj sAoi;
        while( dfSearchRadius > 0 )
        {
            sAoi.minx = dfXPoint - dfSearchRadius;
            sAoi.miny = dfYPoint - dfSearchRadius;
            sAoi.maxx = dfXPoint + dfSearchRadius;
            sAoi.maxy = dfYPoint + dfSearchRadius;
            int nFeatureCount = 0;
            GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                    CPLQuadTreeSearch(hQuadTree, &sAoi, &nFeatureCount) );
            if( nFeatureCount != 0 )
            {
                if( dfRadius1 > 0 )
                    dfNearestR = dfRadius1;
                for( int k = 0; k < nFeatureCount; k++)
                {
                    const int idx = papsPoints[k]->i;
                    const double dfRX = padfX[idx] - dfXPoint;
                    const double dfRY = padfY[idx] - dfYPoint;

                    const double dfR2 = dfRX * dfRX + dfRY * dfRY;
                    if( dfR2 <= dfNearestR )
                    {
                        dfNearestR = dfR2;
                        dfNearestValue = padfZ[idx];
                    }
                }

                CPLFree(papsPoints);
                break;
            }

            CPLFree(papsPoints);
            if( dfRadius1 > 0 )
                break;
            dfSearchRadius *= 2;
#if DEBUG_VERBOSE
            CPLDebug(
                "GDAL_GRID", "Increasing search radius to %.16g",
                dfSearchRadius);
#endif
        }
    }
    else
    {
        while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                const double dfR2 = dfRX * dfRX + dfRY * dfRY;
                if( dfR2 <= dfNearestR )
                {
                    dfNearestR = dfR2;
                    dfNearestValue = padfZ[i];
                }
            }

            i++;
        }
    }

    *pdfValue = dfNearestValue;

    return CE_None;
}

/************************************************************************/
/*                      GDALGridDataMetricMinimum()                     */
/************************************************************************/

/**
 * Minimum data value (data metric).
 *
 * Minimum value found in grid node search ellipse. If there are no points
 * found, the specified NODATA value will be returned.
 *
 * \f[
 *      Z=\min{(Z_1,Z_2,\ldots,Z_n)}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn unused.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricMinimum( const void *poOptionsIn, GUInt32 nPoints,
                           const double *padfX, const double *padfY,
                           const double *padfZ,
                           double dfXPoint, double dfYPoint, double *pdfValue,
                           void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);

    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfMinimumValue=0.0;
    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    if(n){
                        if (dfMinimumValue>padfZ[i]){
                            dfMinimumValue=padfZ[i];
                        }
                    }
                    else{
                        dfMinimumValue=padfZ[i];
                    }
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;
        while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                if( n > 0 )
                {
                    if( dfMinimumValue > padfZ[i] )
                        dfMinimumValue = padfZ[i];
                }
                else
                {
                    dfMinimumValue = padfZ[i];
                }
                n++;
            }

            i++;
        }

    }
    

    if( n < poOptions->nMinPoints || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfMinimumValue;
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALGridDataMetricMaximum()                     */
/************************************************************************/

/**
 * Maximum data value (data metric).
 *
 * Maximum value found in grid node search ellipse. If there are no points
 * found, the specified NODATA value will be returned.
 *
 * \f[
 *      Z=\max{(Z_1,Z_2,\ldots,Z_n)}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricMaximum( const void *poOptionsIn, GUInt32 nPoints,
                           const double *padfX, const double *padfY,
                           const double *padfZ,
                           double dfXPoint, double dfYPoint, double *pdfValue,
                           void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);

    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfMaximumValue=0.0;
    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    if(n){
                        if (dfMaximumValue<padfZ[i]){
                            dfMaximumValue=padfZ[i];
                        }
                    }
                    else{
                        dfMaximumValue=padfZ[i];
                    }
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;
         while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                if( n )
                {
                    if( dfMaximumValue < padfZ[i] )
                        dfMaximumValue = padfZ[i];
                }
                else
                {
                    dfMaximumValue = padfZ[i];
                }
                n++;
            }

            i++;
        }

    }

   

    if( n < poOptions->nMinPoints
         || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfMaximumValue;
    }

    return CE_None;
}

/************************************************************************/
/*                       GDALGridDataMetricRange()                      */
/************************************************************************/

/**
 * Data range (data metric).
 *
 * A difference between the minimum and maximum values found in grid node
 * search ellipse. If there are no points found, the specified NODATA
 * value will be returned.
 *
 * \f[
 *      Z=\max{(Z_1,Z_2,\ldots,Z_n)}-\min{(Z_1,Z_2,\ldots,Z_n)}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$Z_i\f$ is a known value at point \f$i\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricRange( const void *poOptionsIn, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         const double *padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);
    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfMaximumValue = 0.0;
    double dfMinimumValue = 0.0;
    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    if( n > 0 )
                    {
                        if( dfMinimumValue > padfZ[i] )
                            dfMinimumValue = padfZ[i];
                        if( dfMaximumValue < padfZ[i] )
                            dfMaximumValue = padfZ[i];
                    }
                    else
                    {
                        dfMinimumValue = padfZ[i];
                        dfMaximumValue = padfZ[i];
                    }
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;
        while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                if( n > 0 )
                {
                    if( dfMinimumValue > padfZ[i] )
                        dfMinimumValue = padfZ[i];
                    if( dfMaximumValue < padfZ[i] )
                        dfMaximumValue = padfZ[i];
                }
                else
                {
                    dfMinimumValue = padfZ[i];
                    dfMaximumValue = padfZ[i];
                }
                n++;
            }

            i++;
        }

    }

    

    if( n < poOptions->nMinPoints || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfMaximumValue - dfMinimumValue;
    }

    return CE_None;
}

/************************************************************************/
/*                       GDALGridDataMetricCount()                      */
/************************************************************************/

/**
 * Number of data points (data metric).
 *
 * A number of data points found in grid node search ellipse.
 *
 * \f[
 *      Z=n
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricCount( const void *poOptionsIn, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         CPL_UNUSED const double * padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);

    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double    dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;
        while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                n++;
            }

            i++;
        }

    }

   

    if( n < poOptions->nMinPoints )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = static_cast<double>(n);
    }

    return CE_None;
}

/************************************************************************/
/*                 GDALGridDataMetricAverageDistance()                  */
/************************************************************************/

/**
 * Average distance (data metric).
 *
 * An average distance between the grid node (center of the search ellipse)
 * and all of the data points found in grid node search ellipse. If there are
 * no points found, the specified NODATA value will be returned.
 *
 * \f[
 *      Z=\frac{\sum_{i = 1}^n r_i}{n}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$r_i\f$ is an Euclidean distance from the grid node
 *           to point \f$i\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values (unused)
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricAverageDistance( const void *poOptionsIn, GUInt32 nPoints,
                                   const double *padfX, const double *padfY,
                                   CPL_UNUSED const double * padfZ,
                                   double dfXPoint, double dfYPoint,
                                   double *pdfValue,
                                   void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);

    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfAccumulator = 0.0;
    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
                {
                    dfAccumulator += sqrt( dfRX * dfRX + dfRY * dfRY );
                    n++;
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;

        while( i < nPoints )
        {
            double dfRX = padfX[i] - dfXPoint;
            double dfRY = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                const double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                dfAccumulator += sqrt( dfRX * dfRX + dfRY * dfRY );
                n++;
            }

            i++;
        }
    }

    

    if( n < poOptions->nMinPoints || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfAccumulator / n;
    }

    return CE_None;
}

/************************************************************************/
/*                 GDALGridDataMetricAverageDistance()                  */
/************************************************************************/

/**
 * Average distance between points (data metric).
 *
 * An average distance between the data points found in grid node search
 * ellipse. The distance between each pair of points within ellipse is
 * calculated and average of all distances is set as a grid node value. If
 * there are no points found, the specified NODATA value will be returned.

 *
 * \f[
 *      Z=\frac{\sum_{i = 1}^{n-1}\sum_{j=i+1}^{n} r_{ij}}{\left(n-1\right)\,n-\frac{n+{\left(n-1\right)}^{2}-1}{2}}
 * \f]
 *
 *  where
 *  <ul>
 *      <li> \f$Z\f$ is a resulting value at the grid node,
 *      <li> \f$r_{ij}\f$ is an Euclidean distance between points
 *           \f$i\f$ and \f$j\f$,
 *      <li> \f$n\f$ is a total number of points in search ellipse.
 *  </ul>
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values (unused)
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParamsIn extra parameters (unused)
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricAverageDistancePts( const void *poOptionsIn, GUInt32 nPoints,
                                      const double *padfX, const double *padfY,
                                      CPL_UNUSED const double * padfZ,
                                      double dfXPoint, double dfYPoint,
                                      double *pdfValue,
                                      void * hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    const GDALGridDataMetricsOptions *const poOptions =
        static_cast<const GDALGridDataMetricsOptions *>(poOptionsIn);
    // Pre-compute search ellipse parameters.
    const double dfRadius1 = poOptions->dfRadius1 * poOptions->dfRadius1;
    double dfSearchRadius = poOptions->dfRadius1;
    const double dfRadius2 = poOptions->dfRadius2 * poOptions->dfRadius2;
    const double dfR12 = dfRadius1 * dfRadius2;

    GDALGridExtraParameters* psExtraParams = static_cast<GDALGridExtraParameters *>(hExtraParamsIn);
    CPLQuadTree* phQuadTree = psExtraParams->hQuadTree;

    // Compute coefficients for coordinate system rotation.
    const double dfAngle = TO_RADIANS * poOptions->dfAngle;
    const bool bRotated = dfAngle != 0.0;
    const double dfCoeff1 = bRotated ? cos(dfAngle) : 0.0;
    const double dfCoeff2 = bRotated ? sin(dfAngle) : 0.0;

    double dfAccumulator = 0.0;
    GUInt32 n = 0;
    if( phQuadTree != nullptr)
    {
        CPLRectObj sAoi;
        sAoi.minx = dfXPoint - dfSearchRadius;
        sAoi.miny = dfYPoint - dfSearchRadius;
        sAoi.maxx = dfXPoint + dfSearchRadius;
        sAoi.maxy = dfYPoint + dfSearchRadius;
        int nFeatureCount = 0;
        GDALGridPoint** papsPoints = reinterpret_cast<GDALGridPoint **>(
                CPLQuadTreeSearch(phQuadTree, &sAoi, &nFeatureCount) );
        if( nFeatureCount != 0 )
        {
            for( int k = 0; k < nFeatureCount-1; k++ )
            {
                const int i = papsPoints[k]->i;
                const double dfRX1 = padfX[i] - dfXPoint;
                const double dfRY1 = padfY[i] - dfYPoint;

                if( dfRadius2 * dfRX1 * dfRX1 + dfRadius1 * dfRY1 * dfRY1 <= dfR12 )
                {
                    for( int j = k; j < nFeatureCount; j++ )
                    // Search all the remaining points within the ellipse and compute
                    // distances between them and the first point.
                    {
                        const int ji = papsPoints[j]->i;
                        double dfRX2 = padfX[ji] - dfXPoint;
                        double dfRY2 = padfY[ji] - dfYPoint;

                        if( dfRadius2 * dfRX2 * dfRX2 + dfRadius1 * dfRY2 * dfRY2 <= dfR12 )
                        {
                            const double dfRX = padfX[ji] - padfX[i];
                            const double dfRY = padfY[ji] - padfY[i];

                            dfAccumulator += sqrt( dfRX * dfRX + dfRY * dfRY );
                            n++;
                        }
                    }
                }
            }
        }
        CPLFree(papsPoints);
    }
    else{
        GUInt32 i = 0;
        while( i < nPoints - 1 )
        {
            double dfRX1 = padfX[i] - dfXPoint;
            double dfRY1 = padfY[i] - dfYPoint;

            if( bRotated )
            {
                const double dfRXRotated = dfRX1 * dfCoeff1 + dfRY1 * dfCoeff2;
                const double dfRYRotated = dfRY1 * dfCoeff1 - dfRX1 * dfCoeff2;

                dfRX1 = dfRXRotated;
                dfRY1 = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if( dfRadius2 * dfRX1 * dfRX1 + dfRadius1 * dfRY1 * dfRY1 <= dfR12 )
            {
                // Search all the remaining points within the ellipse and compute
                // distances between them and the first point.
                for( GUInt32 j = i + 1; j < nPoints; j++ )
                {
                    double dfRX2 = padfX[j] - dfXPoint;
                    double dfRY2 = padfY[j] - dfYPoint;

                    if( bRotated )
                    {
                        const double dfRXRotated =
                            dfRX2 * dfCoeff1 + dfRY2 * dfCoeff2;
                        const double dfRYRotated =
                            dfRY2 * dfCoeff1 - dfRX2 * dfCoeff2;

                        dfRX2 = dfRXRotated;
                        dfRY2 = dfRYRotated;
                    }

                    if( dfRadius2 * dfRX2 * dfRX2 + dfRadius1 * dfRY2 * dfRY2
                        <= dfR12 )
                    {
                        const double dfRX = padfX[j] - padfX[i];
                        const double dfRY = padfY[j] - padfY[i];

                        dfAccumulator += sqrt( dfRX * dfRX + dfRY * dfRY );
                        n++;
                    }
                }
            }

            i++;
        }


    }

    // Search for the first point within the search ellipse.
    
    if( n < poOptions->nMinPoints || n == 0 )
    {
        *pdfValue = poOptions->dfNoDataValue;
    }
    else
    {
        *pdfValue = dfAccumulator / n;
    }

    return CE_None;
}

/************************************************************************/
/*                        GDALGridLinear()                              */
/************************************************************************/

/**
 * Linear interpolation
 *
 * The Linear method performs linear interpolation by finding in which triangle
 * of a Delaunay triangulation the point is, and by doing interpolation from
 * its barycentric coordinates within the triangle.
 * If the point is not in any triangle, depending on the radius, the
 * algorithm will use the value of the nearest point (radius != 0),
 * or the nodata value (radius == 0)
 *
 * @param poOptionsIn Algorithm parameters. This should point to
 * GDALGridLinearOptions object.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 * @param hExtraParams extra parameters
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 *
 * @since GDAL 2.1
 */

CPLErr
GDALGridLinear( const void *poOptionsIn, GUInt32 nPoints,
                const double *padfX, const double *padfY,
                const double *padfZ,
                double dfXPoint, double dfYPoint, double *pdfValue,
                void *hExtraParams )
{
    GDALGridExtraParameters* psExtraParams =
        static_cast<GDALGridExtraParameters *>(hExtraParams);
    GDALTriangulation* psTriangulation = psExtraParams->psTriangulation;

    int nOutputFacetIdx = -1;
    const bool bRet = CPL_TO_BOOL( GDALTriangulationFindFacetDirected(
        psTriangulation, psExtraParams->nInitialFacetIdx,
        dfXPoint, dfYPoint, &nOutputFacetIdx ) );

    if( bRet )
    {
        CPLAssert(nOutputFacetIdx >= 0);
        // Reuse output facet idx as next initial index since we proceed line by
        // line.
        psExtraParams->nInitialFacetIdx = nOutputFacetIdx;

        double lambda1 = 0.0;
        double lambda2 = 0.0;
        double lambda3 = 0.0;
        GDALTriangulationComputeBarycentricCoordinates(
                                                psTriangulation,
                                                nOutputFacetIdx,
                                                dfXPoint, dfYPoint,
                                                &lambda1, &lambda2, &lambda3);
        const int i1 =
            psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[0];
        const int i2 =
            psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[1];
        const int i3 =
            psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[2];
        *pdfValue =
            lambda1 * padfZ[i1] +
            lambda2 * padfZ[i2] +
            lambda3 * padfZ[i3];
    }
    else
    {
        if( nOutputFacetIdx >= 0 )
        {
            // Also reuse this failed output facet, when valid, as seed for
            // next search.
            psExtraParams->nInitialFacetIdx = nOutputFacetIdx;
        }

        const GDALGridLinearOptions *const poOptions =
            static_cast<const GDALGridLinearOptions *>(poOptionsIn);
        const double dfRadius = poOptions->dfRadius;
        if( dfRadius == 0.0 )
        {
            *pdfValue = poOptions->dfNoDataValue;
        }
        else
        {
            GDALGridNearestNeighborOptions sNeighbourOptions;
            sNeighbourOptions.dfRadius1 = dfRadius < 0.0 ? 0.0 : dfRadius;
            sNeighbourOptions.dfRadius2 = dfRadius < 0.0 ? 0.0 : dfRadius;
            sNeighbourOptions.dfAngle = 0.0;
            sNeighbourOptions.dfNoDataValue = poOptions->dfNoDataValue;
            return GDALGridNearestNeighbor( &sNeighbourOptions, nPoints,
                                            padfX, padfY, padfZ,
                                            dfXPoint, dfYPoint, pdfValue,
                                            hExtraParams );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             GDALGridJob                              */
/************************************************************************/

typedef struct _GDALGridJob GDALGridJob;

struct _GDALGridJob
{
    GUInt32             nYStart;

    GByte              *pabyData;
    GUInt32             nYStep;
    GUInt32             nXSize;
    GUInt32             nYSize;
    double              dfXMin;
    double              dfYMin;
    double              dfDeltaX;
    double              dfDeltaY;
    GUInt32             nPoints;
    const double       *padfX;
    const double       *padfY;
    const double       *padfZ;
    const void         *poOptions;
    GDALGridFunction    pfnGDALGridMethod;
    GDALGridExtraParameters* psExtraParameters;
    int               (*pfnProgress)(GDALGridJob* psJob);
    GDALDataType        eType;

    int   *pnCounter;
    volatile int   *pbStop;
    CPLCond        *hCond;
    CPLMutex       *hCondMutex;

    GDALProgressFunc pfnRealProgress;
    void *pRealProgressArg;
};

/************************************************************************/
/*                   GDALGridProgressMultiThread()                      */
/************************************************************************/

// Return TRUE if the computation must be interrupted.
static int GDALGridProgressMultiThread( GDALGridJob* psJob )
{
    CPLAcquireMutex(psJob->hCondMutex, 1.0);
    ++(*psJob->pnCounter);
    CPLCondSignal(psJob->hCond);
    const int bStop = *psJob->pbStop;
    CPLReleaseMutex(psJob->hCondMutex);

    return bStop;
}

/************************************************************************/
/*                      GDALGridProgressMonoThread()                    */
/************************************************************************/

// Return TRUE if the computation must be interrupted.
static int GDALGridProgressMonoThread( GDALGridJob* psJob )
{
    const int nCounter = ++(*psJob->pnCounter);
    if( !psJob->pfnRealProgress( nCounter / static_cast<double>(psJob->nYSize),
                                 "", psJob->pRealProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        *psJob->pbStop = TRUE;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALGridJobProcess()                         */
/************************************************************************/

static void GDALGridJobProcess( void* user_data )
{
    GDALGridJob* const psJob = static_cast<GDALGridJob *>(user_data);
    int (*pfnProgress)(GDALGridJob* psJob) = psJob->pfnProgress;
    const GUInt32 nXSize = psJob->nXSize;

    /* -------------------------------------------------------------------- */
    /*  Allocate a buffer of scanline size, fill it with gridded values     */
    /*  and use GDALCopyWords() to copy values into output data array with  */
    /*  appropriate data type conversion.                                   */
    /* -------------------------------------------------------------------- */
    double *padfValues =
        static_cast<double *>(VSI_MALLOC2_VERBOSE( sizeof(double), nXSize ));
    if( padfValues == nullptr )
    {
        *(psJob->pbStop) = TRUE;
        if( pfnProgress != nullptr )
            pfnProgress(psJob);  // To notify the main thread.
        return;
    }

    const GUInt32 nYStart = psJob->nYStart;
    const GUInt32 nYStep = psJob->nYStep;
    GByte *pabyData = psJob->pabyData;

    const GUInt32 nYSize = psJob->nYSize;
    const double dfXMin = psJob->dfXMin;
    const double dfYMin = psJob->dfYMin;
    const double dfDeltaX = psJob->dfDeltaX;
    const double dfDeltaY = psJob->dfDeltaY;
    const GUInt32 nPoints = psJob->nPoints;
    const double* padfX = psJob->padfX;
    const double* padfY = psJob->padfY;
    const double* padfZ = psJob->padfZ;
    const void *poOptions = psJob->poOptions;
    GDALGridFunction pfnGDALGridMethod = psJob->pfnGDALGridMethod;
    // Have a local copy of sExtraParameters since we want to modify
    // nInitialFacetIdx.
    GDALGridExtraParameters sExtraParameters = *psJob->psExtraParameters;
    const GDALDataType eType = psJob->eType;

    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);
    const int nLineSpace = nXSize * nDataTypeSize;

    for( GUInt32 nYPoint = nYStart; nYPoint < nYSize; nYPoint += nYStep )
    {
        const double dfYPoint = dfYMin + ( nYPoint + 0.5 ) * dfDeltaY;

        for( GUInt32 nXPoint = 0; nXPoint < nXSize; nXPoint++ )
        {
            const double dfXPoint = dfXMin + ( nXPoint + 0.5 ) * dfDeltaX;

            if( (*pfnGDALGridMethod)(poOptions, nPoints, padfX, padfY, padfZ,
                                     dfXPoint, dfYPoint,
                                     padfValues + nXPoint,
                                     &sExtraParameters) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Gridding failed at X position %lu, Y position %lu",
                          static_cast<long unsigned int>(nXPoint),
                          static_cast<long unsigned int>(nYPoint) );
                *psJob->pbStop = TRUE;
                if( pfnProgress != nullptr )
                    pfnProgress(psJob);  // To notify the main thread.
                break;
            }
        }

        GDALCopyWords( padfValues, GDT_Float64, sizeof(double),
                       pabyData + nYPoint * nLineSpace, eType, nDataTypeSize,
                       nXSize );

        if( *psJob->pbStop || (pfnProgress != nullptr && pfnProgress(psJob)) )
            break;
    }

    CPLFree(padfValues);
}

/************************************************************************/
/*                        GDALGridContextCreate()                       */
/************************************************************************/

struct GDALGridContext
{
    GDALGridAlgorithm  eAlgorithm;
    void               *poOptions;
    GDALGridFunction    pfnGDALGridMethod;

    GUInt32             nPoints;
    GDALGridPoint      *pasGridPoints;
    GDALGridXYArrays    sXYArrays;

    GDALGridExtraParameters sExtraParameters;
    double*             padfX;
    double*             padfY;
    double*             padfZ;
    bool                bFreePadfXYZArrays;

    CPLWorkerThreadPool *poWorkerThreadPool;
};

static void GDALGridContextCreateQuadTree( GDALGridContext* psContext );

/**
 * Creates a context to do regular gridding from the scattered data.
 *
 * This function takes the arrays of X and Y coordinates and corresponding Z
 * values as input to prepare computation of regular grid (or call it a raster)
 * from these scattered data.
 *
 * On Intel/AMD i386/x86_64 architectures, some
 * gridding methods will be optimized with SSE instructions (provided GDAL
 * has been compiled with such support, and it is available at runtime).
 * Currently, only 'invdist' algorithm with default parameters has an optimized
 * implementation.
 * This can provide substantial speed-up, but sometimes at the expense of
 * reduced floating point precision. This can be disabled by setting the
 * GDAL_USE_SSE configuration option to NO.
 * A further optimized version can use the AVX
 * instruction set. This can be disabled by setting the GDAL_USE_AVX
 * configuration option to NO.
 *
 * It is possible to set the GDAL_NUM_THREADS
 * configuration option to parallelize the processing. The value to set is
 * the number of worker threads, or ALL_CPUS to use all the cores/CPUs of the
 * computer (default value).
 *
 * @param eAlgorithm Gridding method.
 * @param poOptions Options to control chosen gridding method.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param bCallerWillKeepPointArraysAlive Whether the provided padfX, padfY,
 *        padfZ arrays will still be "alive" during the calls to
 *        GDALGridContextProcess().  Setting to TRUE prevent them from being
 *        duplicated in the context.  If unsure, set to FALSE.
 *
 * @return the context (to be freed with GDALGridContextFree()) or NULL in case
 *         or error.
 *
 * @since GDAL 2.1
 */

GDALGridContext*
GDALGridContextCreate( GDALGridAlgorithm eAlgorithm, const void *poOptions,
                       GUInt32 nPoints,
                       const double *padfX, const double *padfY,
                       const double *padfZ,
                       int bCallerWillKeepPointArraysAlive )
{
    CPLAssert( poOptions );
    CPLAssert( padfX );
    CPLAssert( padfY );
    CPLAssert( padfZ );
    bool bCreateQuadTree = false;

    const unsigned int nPointCountThreshold = atoi(CPLGetConfigOption("GDAL_GRID_POINT_COUNT_THRESHOLD", "100"));
 
    // Starting address aligned on 32-byte boundary for AVX.
    float* pafXAligned = nullptr;
    float* pafYAligned = nullptr;
    float* pafZAligned = nullptr;

    void *poOptionsNew = nullptr;

    GDALGridFunction pfnGDALGridMethod = nullptr;

    switch( eAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
        {
            poOptionsNew =
                CPLMalloc(sizeof(GDALGridInverseDistanceToAPowerOptions));
            memcpy(poOptionsNew, poOptions,
                   sizeof(GDALGridInverseDistanceToAPowerOptions));

            const GDALGridInverseDistanceToAPowerOptions * const poPower =
                static_cast<const GDALGridInverseDistanceToAPowerOptions *>(
                    poOptions);
            if( poPower->dfRadius1 == 0.0 && poPower->dfRadius2 == 0.0 )
            {
                const double dfPower = poPower->dfPower;
                const double dfSmoothing = poPower->dfSmoothing;

                pfnGDALGridMethod = GDALGridInverseDistanceToAPowerNoSearch;
                if( dfPower == 2.0 && dfSmoothing == 0.0 )
                {
#ifdef HAVE_AVX_AT_COMPILE_TIME

                    if( CPLTestBool(
                            CPLGetConfigOption("GDAL_USE_AVX", "YES")) &&
                        CPLHaveRuntimeAVX() )
                    {
                        pafXAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        pafYAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        pafZAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        if( pafXAligned != nullptr && pafYAligned != nullptr &&
                            pafZAligned != nullptr )
                        {
                            CPLDebug("GDAL_GRID",
                                     "Using AVX optimized version");
                            pfnGDALGridMethod =
                                GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX;
                            for( GUInt32 i = 0; i < nPoints; i++ )
                            {
                                pafXAligned[i] = static_cast<float>(padfX[i]);
                                pafYAligned[i] = static_cast<float>(padfY[i]);
                                pafZAligned[i] = static_cast<float>(padfZ[i]);
                            }
                        }
                        else
                        {
                            VSIFree(pafXAligned);
                            VSIFree(pafYAligned);
                            VSIFree(pafZAligned);
                            pafXAligned = nullptr;
                            pafYAligned = nullptr;
                            pafZAligned = nullptr;
                        }
                    }
#endif

#ifdef HAVE_SSE_AT_COMPILE_TIME

                    if( pafXAligned == nullptr &&
                        CPLTestBool(
                            CPLGetConfigOption("GDAL_USE_SSE", "YES")) &&
                        CPLHaveRuntimeSSE() )
                    {
                        pafXAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        pafYAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        pafZAligned = static_cast<float *>(
                            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                                sizeof(float) * nPoints) );
                        if( pafXAligned != nullptr && pafYAligned != nullptr &&
                            pafZAligned != nullptr )
                        {
                            CPLDebug("GDAL_GRID",
                                     "Using SSE optimized version");
                            pfnGDALGridMethod =
                                GDALGridInverseDistanceToAPower2NoSmoothingNoSearchSSE;
                            for( GUInt32 i = 0; i < nPoints; i++ )
                            {
                                pafXAligned[i] = static_cast<float>(padfX[i]);
                                pafYAligned[i] = static_cast<float>(padfY[i]);
                                pafZAligned[i] = static_cast<float>(padfZ[i]);
                            }
                        }
                        else
                        {
                            VSIFree(pafXAligned);
                            VSIFree(pafYAligned);
                            VSIFree(pafZAligned);
                            pafXAligned = nullptr;
                            pafYAligned = nullptr;
                            pafZAligned = nullptr;
                        }
                    }
#endif // HAVE_SSE_AT_COMPILE_TIME
                }
            }
            else
            {
                pfnGDALGridMethod = GDALGridInverseDistanceToAPower;
            }
            break;
        }
        case GGA_InverseDistanceToAPowerNearestNeighbor:
        {
            poOptionsNew = CPLMalloc(
                sizeof(GDALGridInverseDistanceToAPowerNearestNeighborOptions));
            memcpy(poOptionsNew, poOptions,
                   sizeof(
                       GDALGridInverseDistanceToAPowerNearestNeighborOptions));

            pfnGDALGridMethod = GDALGridInverseDistanceToAPowerNearestNeighbor;
            bCreateQuadTree = TRUE;
            break;
        }
        case GGA_MovingAverage:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridMovingAverageOptions));
            memcpy(poOptionsNew,
                   poOptions,
                   sizeof(GDALGridMovingAverageOptions));

            pfnGDALGridMethod = GDALGridMovingAverage;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridMovingAverageOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridMovingAverageOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridMovingAverageOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridMovingAverageOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_NearestNeighbor:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridNearestNeighborOptions));
            memcpy(poOptionsNew, poOptions,
                   sizeof(GDALGridNearestNeighborOptions));

            pfnGDALGridMethod = GDALGridNearestNeighbor;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridNearestNeighborOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridNearestNeighborOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridNearestNeighborOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridNearestNeighborOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricMinimum:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricMinimum;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricMaximum:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricMaximum;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricRange:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricRange;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricCount:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricCount;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricAverageDistance:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricAverageDistance;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_MetricAverageDistancePts:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricAverageDistancePts;
            bCreateQuadTree = (nPoints > nPointCountThreshold &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfAngle == 0.0 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 ==
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius2 &&
                static_cast<const GDALGridDataMetricsOptions *>(poOptions)->dfRadius1 != 0.0);
            break;
        }
        case GGA_Linear:
        {
            poOptionsNew = CPLMalloc(sizeof(GDALGridLinearOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridLinearOptions));

            pfnGDALGridMethod = GDALGridLinear;
            break;
        }
        default:
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "GDAL does not support gridding method %d", eAlgorithm );
            return nullptr;
    }

    if( pafXAligned == nullptr && !bCallerWillKeepPointArraysAlive )
    {
        double* padfXNew =
            static_cast<double *>(VSI_MALLOC2_VERBOSE(nPoints, sizeof(double)));
        double* padfYNew =
            static_cast<double *>(VSI_MALLOC2_VERBOSE(nPoints, sizeof(double)));
        double* padfZNew =
            static_cast<double *>(VSI_MALLOC2_VERBOSE(nPoints, sizeof(double)));
        if( padfXNew == nullptr || padfYNew == nullptr || padfZNew == nullptr )
        {
            VSIFree(padfXNew);
            VSIFree(padfYNew);
            VSIFree(padfZNew);
            CPLFree(poOptionsNew);
            return nullptr;
        }
        memcpy(padfXNew, padfX, nPoints * sizeof(double));
        memcpy(padfYNew, padfY, nPoints * sizeof(double));
        memcpy(padfZNew, padfZ, nPoints * sizeof(double));
        padfX = padfXNew;
        padfY = padfYNew;
        padfZ = padfZNew;
    }
    GDALGridContext* psContext =
        static_cast<GDALGridContext *>(CPLCalloc(1, sizeof(GDALGridContext)));
    psContext->eAlgorithm = eAlgorithm;
    psContext->poOptions = poOptionsNew;
    psContext->pfnGDALGridMethod = pfnGDALGridMethod;
    psContext->nPoints = nPoints;
    psContext->pasGridPoints = nullptr;
    psContext->sXYArrays.padfX = padfX;
    psContext->sXYArrays.padfY = padfY;
    psContext->sExtraParameters.hQuadTree = nullptr;
    psContext->sExtraParameters.dfInitialSearchRadius = 0.0;
    psContext->sExtraParameters.pafX = pafXAligned;
    psContext->sExtraParameters.pafY = pafYAligned;
    psContext->sExtraParameters.pafZ = pafZAligned;
    psContext->sExtraParameters.psTriangulation = nullptr;
    psContext->sExtraParameters.nInitialFacetIdx = 0;
    psContext->padfX = pafXAligned ? nullptr : const_cast<double *>(padfX);
    psContext->padfY = pafXAligned ? nullptr : const_cast<double *>(padfY);
    psContext->padfZ = pafXAligned ? nullptr : const_cast<double *>(padfZ);
    psContext->bFreePadfXYZArrays =
        pafXAligned ? false : !bCallerWillKeepPointArraysAlive;

/* -------------------------------------------------------------------- */
/*  Create quadtree if requested and possible.                          */
/* -------------------------------------------------------------------- */
    if( bCreateQuadTree )
    {
        GDALGridContextCreateQuadTree(psContext);
    }

    /* -------------------------------------------------------------------- */
    /*  Pre-compute extra parameters in GDALGridExtraParameters              */
    /* -------------------------------------------------------------------- */
    if( eAlgorithm == GGA_InverseDistanceToAPowerNearestNeighbor )
    {
        const double dfPower =
            static_cast<const GDALGridInverseDistanceToAPowerNearestNeighborOptions*>(poOptions)->dfPower;
        psContext->sExtraParameters.dfPowerDiv2PreComp = dfPower / 2;

        const double dfRadius =
            static_cast<const GDALGridInverseDistanceToAPowerNearestNeighborOptions*>(poOptions)->dfRadius;
        psContext->sExtraParameters.dfRadiusPower2PreComp = pow ( dfRadius, 2 );
        psContext->sExtraParameters.dfRadiusPower4PreComp = pow ( dfRadius, 4 );
    }

    if( eAlgorithm == GGA_Linear )
    {
        psContext->sExtraParameters.psTriangulation =
                GDALTriangulationCreateDelaunay(nPoints, padfX, padfY);
        if( psContext->sExtraParameters.psTriangulation == nullptr )
        {
            GDALGridContextFree(psContext);
            return nullptr;
        }
        GDALTriangulationComputeBarycentricCoefficients(
            psContext->sExtraParameters.psTriangulation, padfX, padfY );
    }

/* -------------------------------------------------------------------- */
/*  Start thread pool.                                                  */
/* -------------------------------------------------------------------- */
    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    int nThreads = 0;
    if( EQUAL(pszThreads, "ALL_CPUS") )
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszThreads);
    if( nThreads > 128 )
        nThreads = 128;
    if( nThreads > 1 )
    {
        psContext->poWorkerThreadPool = new CPLWorkerThreadPool();
        if( !psContext->poWorkerThreadPool->Setup(nThreads, nullptr, nullptr) )
        {
            delete psContext->poWorkerThreadPool;
            psContext->poWorkerThreadPool = nullptr;
        }
        else
        {
            CPLDebug("GDAL_GRID", "Using %d threads", nThreads);
        }
    }
    else
        psContext->poWorkerThreadPool = nullptr;

    return psContext;
}

/************************************************************************/
/*                      GDALGridContextCreateQuadTree()                 */
/************************************************************************/

void GDALGridContextCreateQuadTree( GDALGridContext* psContext )
{
    const GUInt32 nPoints = psContext->nPoints;
    psContext->pasGridPoints = static_cast<GDALGridPoint *>(
            VSI_MALLOC2_VERBOSE(nPoints, sizeof(GDALGridPoint)) );
    if( psContext->pasGridPoints != nullptr )
    {
        const double * const padfX = psContext->padfX;
        const double * const padfY = psContext->padfY;

        // Determine point extents.
        CPLRectObj sRect;
        sRect.minx = padfX[0];
        sRect.miny = padfY[0];
        sRect.maxx = padfX[0];
        sRect.maxy = padfY[0];
        for( GUInt32 i = 1; i < nPoints; i++ )
        {
            if( padfX[i] < sRect.minx ) sRect.minx = padfX[i];
            if( padfY[i] < sRect.miny ) sRect.miny = padfY[i];
            if( padfX[i] > sRect.maxx ) sRect.maxx = padfX[i];
            if( padfY[i] > sRect.maxy ) sRect.maxy = padfY[i];
        }

        // Initial value for search radius is the typical dimension of a
        // "pixel" of the point array (assuming rather uniform distribution).
        psContext->sExtraParameters.dfInitialSearchRadius =
            sqrt((sRect.maxx - sRect.minx) *
                 (sRect.maxy - sRect.miny) / nPoints);

        psContext->sExtraParameters.hQuadTree =
            CPLQuadTreeCreate(&sRect, GDALGridGetPointBounds );

        for( GUInt32 i = 0; i < nPoints; i++ )
        {
            psContext->pasGridPoints[i].psXYArrays = &(psContext->sXYArrays);
            psContext->pasGridPoints[i].i = i;
            CPLQuadTreeInsert(psContext->sExtraParameters.hQuadTree,
                                psContext->pasGridPoints + i);
        }
    }
}

/************************************************************************/
/*                        GDALGridContextFree()                         */
/************************************************************************/

/**
 * Free a context used created by GDALGridContextCreate()
 *
 * @param psContext the context.
 *
 * @since GDAL 2.1
 */
void GDALGridContextFree( GDALGridContext* psContext )
{
    if( psContext )
    {
        CPLFree( psContext->poOptions );
        CPLFree( psContext->pasGridPoints );
        if( psContext->sExtraParameters.hQuadTree != nullptr )
            CPLQuadTreeDestroy( psContext->sExtraParameters.hQuadTree );
        if( psContext->bFreePadfXYZArrays )
        {
            CPLFree(psContext->padfX);
            CPLFree(psContext->padfY);
            CPLFree(psContext->padfZ);
        }
        VSIFreeAligned(psContext->sExtraParameters.pafX);
        VSIFreeAligned(psContext->sExtraParameters.pafY);
        VSIFreeAligned(psContext->sExtraParameters.pafZ);
        if( psContext->sExtraParameters.psTriangulation )
            GDALTriangulationFree(psContext->sExtraParameters.psTriangulation);
        delete psContext->poWorkerThreadPool;
        CPLFree(psContext);
    }
}

/************************************************************************/
/*                        GDALGridContextProcess()                      */
/************************************************************************/

/**
 * Do the gridding of a window of a raster.
 *
 * This function takes the gridding context as input to preprare computation
 * of regular grid (or call it a raster) from these scattered data.
 * You should supply the extent of the output grid and allocate array
 * sufficient to hold such a grid.
 *
 * @param psContext Gridding context.
 * @param dfXMin Lowest X border of output grid.
 * @param dfXMax Highest X border of output grid.
 * @param dfYMin Lowest Y border of output grid.
 * @param dfYMax Highest Y border of output grid.
 * @param nXSize Number of columns in output grid.
 * @param nYSize Number of rows in output grid.
 * @param eType Data type of output array.
 * @param pData Pointer to array where the computed grid will be stored.
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 * @param pProgressArg argument to be passed to pfnProgress.  May be NULL.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 *
 * @since GDAL 2.1
 */

CPLErr GDALGridContextProcess(
    GDALGridContext* psContext,
    double dfXMin, double dfXMax, double dfYMin, double dfYMax,
    GUInt32 nXSize, GUInt32 nYSize, GDALDataType eType, void *pData,
    GDALProgressFunc pfnProgress, void *pProgressArg )
{
    CPLAssert( psContext );
    CPLAssert( pData );

    if( nXSize == 0 || nYSize == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Output raster dimensions should have non-zero size." );
        return CE_Failure;
    }

    const double dfDeltaX = ( dfXMax - dfXMin ) / nXSize;
    const double dfDeltaY = ( dfYMax - dfYMin ) / nYSize;

    // For linear, check if we will need to fallback to nearest neighbour
    // by sampling along the edges.  If all points on edges are within
    // triangles, then interior points will also be.
    if( psContext->eAlgorithm == GGA_Linear &&
        psContext->sExtraParameters.hQuadTree == nullptr )
    {
        bool bNeedNearest = false;
        int nStartLeft = 0;
        int nStartRight = 0;
        const double dfXPointMin = dfXMin + ( 0 + 0.5 ) * dfDeltaX;
        const double dfXPointMax = dfXMin + ( nXSize - 1 + 0.5 ) * dfDeltaX;
        for( GUInt32 nYPoint = 0; !bNeedNearest && nYPoint < nYSize; nYPoint++ )
        {
            const double dfYPoint = dfYMin + ( nYPoint + 0.5 ) * dfDeltaY;

            if( !GDALTriangulationFindFacetDirected(
                psContext->sExtraParameters.psTriangulation,
                nStartLeft,
                dfXPointMin, dfYPoint,
                &nStartLeft) )
            {
                bNeedNearest = true;
            }
            if( !GDALTriangulationFindFacetDirected(
                psContext->sExtraParameters.psTriangulation,
                nStartRight,
                dfXPointMax, dfYPoint,
                &nStartRight) )
            {
                bNeedNearest = true;
            }
        }
        int nStartTop = 0;
        int nStartBottom = 0;
        const double dfYPointMin = dfYMin + ( 0 + 0.5 ) * dfDeltaY;
        const double dfYPointMax = dfYMin + ( nYSize - 1 + 0.5 ) * dfDeltaY;
        for( GUInt32 nXPoint = 1;
             !bNeedNearest && nXPoint + 1 < nXSize;
             nXPoint++ )
        {
            const double dfXPoint = dfXMin + ( nXPoint + 0.5 ) * dfDeltaX;

            if( !GDALTriangulationFindFacetDirected(
                psContext->sExtraParameters.psTriangulation,
                nStartTop,
                dfXPoint, dfYPointMin,
                &nStartTop) )
            {
                bNeedNearest = true;
            }
            if( !GDALTriangulationFindFacetDirected(
                psContext->sExtraParameters.psTriangulation,
                nStartBottom,
                dfXPoint, dfYPointMax,
                &nStartBottom) )
            {
                bNeedNearest = true;
            }
        }
        if( bNeedNearest )
        {
            CPLDebug("GDAL_GRID", "Will need nearest neighbour");
            GDALGridContextCreateQuadTree(psContext);
        }
    }

    int nCounter = 0;
    volatile int bStop = FALSE;
    GDALGridJob sJob;
    sJob.nYStart = 0;
    sJob.pabyData = static_cast<GByte *>(pData);
    sJob.nYStep = 1;
    sJob.nXSize = nXSize;
    sJob.nYSize = nYSize;
    sJob.dfXMin = dfXMin;
    sJob.dfYMin = dfYMin;
    sJob.dfDeltaX = dfDeltaX;
    sJob.dfDeltaY = dfDeltaY;
    sJob.nPoints = psContext->nPoints;
    sJob.padfX = psContext->padfX;
    sJob.padfY = psContext->padfY;
    sJob.padfZ = psContext->padfZ;
    sJob.poOptions = psContext->poOptions;
    sJob.pfnGDALGridMethod = psContext->pfnGDALGridMethod;
    sJob.psExtraParameters = &psContext->sExtraParameters;
    sJob.pfnProgress = nullptr;
    sJob.eType = eType;
    sJob.pfnRealProgress = pfnProgress;
    sJob.pRealProgressArg = pProgressArg;
    sJob.pnCounter = &nCounter;
    sJob.pbStop = &bStop;
    sJob.hCond = nullptr;
    sJob.hCondMutex = nullptr;

    if( psContext->poWorkerThreadPool == nullptr )
    {
        if( sJob.pfnRealProgress != nullptr &&
            sJob.pfnRealProgress != GDALDummyProgress )
        {
            sJob.pfnProgress = GDALGridProgressMonoThread;
        }

        GDALGridJobProcess(&sJob);
    }
    else
    {
        int nThreads  = psContext->poWorkerThreadPool->GetThreadCount();
        GDALGridJob* pasJobs = static_cast<GDALGridJob *>(
            CPLMalloc(sizeof(GDALGridJob) * nThreads) );

        sJob.nYStep = nThreads;
        sJob.hCondMutex = CPLCreateMutex(); /* and  implicitly take the mutex */
        sJob.hCond = CPLCreateCond();
        sJob.pfnProgress = GDALGridProgressMultiThread;

/* -------------------------------------------------------------------- */
/*      Start threads.                                                  */
/* -------------------------------------------------------------------- */
        for( int i = 0; i < nThreads && !bStop; i++ )
        {
            memcpy(&pasJobs[i], &sJob, sizeof(GDALGridJob));
            pasJobs[i].nYStart = i;
            psContext->poWorkerThreadPool->SubmitJob( GDALGridJobProcess,
                                                      &pasJobs[i] );
        }

/* -------------------------------------------------------------------- */
/*      Report progress.                                                */
/* -------------------------------------------------------------------- */
        while( *(sJob.pnCounter) < static_cast<int>(nYSize) && !bStop )
        {
            CPLCondWait(sJob.hCond, sJob.hCondMutex);

            int nLocalCounter = *(sJob.pnCounter);
            CPLReleaseMutex(sJob.hCondMutex);

            if( pfnProgress != nullptr &&
                !pfnProgress( nLocalCounter / static_cast<double>(nYSize),
                              "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                bStop = TRUE;
            }

            CPLAcquireMutex(sJob.hCondMutex, 1.0);
        }

        // Release mutex before joining threads, otherwise they will dead-lock
        // forever in GDALGridProgressMultiThread().
        CPLReleaseMutex(sJob.hCondMutex);

/* -------------------------------------------------------------------- */
/*      Wait for all threads to complete and finish.                    */
/* -------------------------------------------------------------------- */
        psContext->poWorkerThreadPool->WaitCompletion();

        CPLFree(pasJobs);
        CPLDestroyCond(sJob.hCond);
        CPLDestroyMutex(sJob.hCondMutex);
    }

    return bStop ? CE_Failure : CE_None;
}

/************************************************************************/
/*                            GDALGridCreate()                          */
/************************************************************************/

/**
 * Create regular grid from the scattered data.
 *
 * This function takes the arrays of X and Y coordinates and corresponding Z
 * values as input and computes regular grid (or call it a raster) from these
 * scattered data. You should supply geometry and extent of the output grid
 * and allocate array sufficient to hold such a grid.
 *
 * Starting with GDAL 1.10, it is possible to set the GDAL_NUM_THREADS
 * configuration option to parallelize the processing. The value to set is
 * the number of worker threads, or ALL_CPUS to use all the cores/CPUs of the
 * computer (default value).
 *
 * Starting with GDAL 1.10, on Intel/AMD i386/x86_64 architectures, some
 * gridding methods will be optimized with SSE instructions (provided GDAL
 * has been compiled with such support, and it is available at runtime).
 * Currently, only 'invdist' algorithm with default parameters has an optimized
 * implementation.
 * This can provide substantial speed-up, but sometimes at the expense of
 * reduced floating point precision. This can be disabled by setting the
 * GDAL_USE_SSE configuration option to NO.
 * Starting with GDAL 1.11, a further optimized version can use the AVX
 * instruction set. This can be disabled by setting the GDAL_USE_AVX
 * configuration option to NO.
 *
 * Note: it will be more efficient to use GDALGridContextCreate(),
 * GDALGridContextProcess() and GDALGridContextFree() when doing repeated
 * gridding operations with the same algorithm, parameters and points, and
 * moving the window in the output grid.
 *
 * @param eAlgorithm Gridding method.
 * @param poOptions Options to control chosen gridding method.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates.
 * @param padfY Input array of Y coordinates.
 * @param padfZ Input array of Z values.
 * @param dfXMin Lowest X border of output grid.
 * @param dfXMax Highest X border of output grid.
 * @param dfYMin Lowest Y border of output grid.
 * @param dfYMax Highest Y border of output grid.
 * @param nXSize Number of columns in output grid.
 * @param nYSize Number of rows in output grid.
 * @param eType Data type of output array.
 * @param pData Pointer to array where the computed grid will be stored.
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 * @param pProgressArg argument to be passed to pfnProgress.  May be NULL.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridCreate( GDALGridAlgorithm eAlgorithm, const void *poOptions,
                GUInt32 nPoints,
                const double *padfX, const double *padfY, const double *padfZ,
                double dfXMin, double dfXMax, double dfYMin, double dfYMax,
                GUInt32 nXSize, GUInt32 nYSize, GDALDataType eType, void *pData,
                GDALProgressFunc pfnProgress, void *pProgressArg )
{
    GDALGridContext* psContext = GDALGridContextCreate(eAlgorithm, poOptions,
                                                       nPoints,
                                                       padfX, padfY, padfZ,
                                                       TRUE);
    CPLErr eErr = CE_Failure;
    if( psContext )
    {
        eErr = GDALGridContextProcess( psContext,
                                       dfXMin, dfXMax, dfYMin, dfYMax,
                                       nXSize, nYSize, eType, pData,
                                       pfnProgress, pProgressArg );
    }

    GDALGridContextFree(psContext);
    return eErr;
}

/************************************************************************/
/*                      ParseAlgorithmAndOptions()                      */
/************************************************************************/

/** Translates mnemonic gridding algorithm names into GDALGridAlgorithm code,
 * parse control parameters and assign defaults.
 */
CPLErr ParseAlgorithmAndOptions( const char *pszAlgorithm,
                                 GDALGridAlgorithm *peAlgorithm,
                                 void **ppOptions )
{
    CPLAssert( pszAlgorithm );
    CPLAssert( peAlgorithm );
    CPLAssert( ppOptions );

    *ppOptions = nullptr;

    char **papszParams = CSLTokenizeString2( pszAlgorithm, ":", FALSE );

    if( CSLCount(papszParams) < 1 )
    {
        CSLDestroy( papszParams );
        return CE_Failure;
    }

    if( EQUAL(papszParams[0], szAlgNameInvDist) )
    {
        *peAlgorithm = GGA_InverseDistanceToAPower;
    }
    else if( EQUAL(papszParams[0], szAlgNameInvDistNearestNeighbor) )
    {
        *peAlgorithm = GGA_InverseDistanceToAPowerNearestNeighbor;
    }
    else if( EQUAL(papszParams[0], szAlgNameAverage) )
    {
        *peAlgorithm = GGA_MovingAverage;
    }
    else if( EQUAL(papszParams[0], szAlgNameNearest) )
    {
        *peAlgorithm = GGA_NearestNeighbor;
    }
    else if( EQUAL(papszParams[0], szAlgNameMinimum) )
    {
        *peAlgorithm = GGA_MetricMinimum;
    }
    else if( EQUAL(papszParams[0], szAlgNameMaximum) )
    {
        *peAlgorithm = GGA_MetricMaximum;
    }
    else if( EQUAL(papszParams[0], szAlgNameRange) )
    {
        *peAlgorithm = GGA_MetricRange;
    }
    else if( EQUAL(papszParams[0], szAlgNameCount) )
    {
        *peAlgorithm = GGA_MetricCount;
    }
    else if( EQUAL(papszParams[0], szAlgNameAverageDistance) )
    {
        *peAlgorithm = GGA_MetricAverageDistance;
    }
    else if( EQUAL(papszParams[0], szAlgNameAverageDistancePts) )
    {
        *peAlgorithm = GGA_MetricAverageDistancePts;
    }
    else if( EQUAL(papszParams[0], szAlgNameLinear) )
    {
        *peAlgorithm = GGA_Linear;
    }
    else
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unsupported gridding method \"%s\"",
                  papszParams[0] );
        CSLDestroy( papszParams );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Parse algorithm parameters and assign defaults.                 */
/* -------------------------------------------------------------------- */
    switch( *peAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
        default:
        {
            *ppOptions =
                CPLMalloc( sizeof(GDALGridInverseDistanceToAPowerOptions) );

            GDALGridInverseDistanceToAPowerOptions * const poPowerOpts =
                static_cast<GDALGridInverseDistanceToAPowerOptions *>(
                    *ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "power" );
            poPowerOpts->dfPower = pszValue ? CPLAtofM(pszValue) : 2.0;

            pszValue = CSLFetchNameValue( papszParams, "smoothing" );
            poPowerOpts->dfSmoothing = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius1" );
            poPowerOpts->dfRadius1 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius2" );
            poPowerOpts->dfRadius2 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "angle" );
            poPowerOpts->dfAngle = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "max_points" );
            poPowerOpts->nMaxPoints = static_cast<GUInt32>(
                pszValue ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParams, "min_points" );
            poPowerOpts->nMinPoints = static_cast<GUInt32>(
                pszValue ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poPowerOpts->dfNoDataValue = pszValue ? CPLAtofM(pszValue) : 0.0;

            break;
        }
        case GGA_InverseDistanceToAPowerNearestNeighbor:
        {
            *ppOptions =
                CPLMalloc( sizeof(
                    GDALGridInverseDistanceToAPowerNearestNeighborOptions) );

            GDALGridInverseDistanceToAPowerNearestNeighborOptions * const
                poPowerOpts =
                    static_cast<
                    GDALGridInverseDistanceToAPowerNearestNeighborOptions *>(
                        *ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "power" );
            poPowerOpts->dfPower = pszValue ? CPLAtofM(pszValue) : 2.0;

            pszValue = CSLFetchNameValue( papszParams, "smoothing" );
            poPowerOpts->dfSmoothing = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius" );
            poPowerOpts->dfRadius = pszValue ? CPLAtofM(pszValue) : 1.0;

            pszValue = CSLFetchNameValue( papszParams, "max_points" );
            poPowerOpts->nMaxPoints = static_cast<GUInt32>(
                pszValue ? CPLAtofM(pszValue) : 12);

            pszValue = CSLFetchNameValue( papszParams, "min_points" );
            poPowerOpts->nMinPoints = static_cast<GUInt32>(
                pszValue ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poPowerOpts->dfNoDataValue = pszValue ? CPLAtofM(pszValue) : 0.0;

            break;
        }
        case GGA_MovingAverage:
        {
            *ppOptions =
                CPLMalloc( sizeof(GDALGridMovingAverageOptions) );

            GDALGridMovingAverageOptions * const poAverageOpts =
                static_cast<GDALGridMovingAverageOptions *>(*ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "radius1" );
            poAverageOpts->dfRadius1 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius2" );
            poAverageOpts->dfRadius2 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "angle" );
            poAverageOpts->dfAngle = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "min_points" );
            poAverageOpts->nMinPoints = static_cast<GUInt32>(
                pszValue ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poAverageOpts->dfNoDataValue = pszValue ? CPLAtofM(pszValue) : 0.0;

            break;
        }
        case GGA_NearestNeighbor:
        {
            *ppOptions =
                CPLMalloc( sizeof(GDALGridNearestNeighborOptions) );

            GDALGridNearestNeighborOptions * const poNeighborOpts =
                static_cast<GDALGridNearestNeighborOptions *>(*ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "radius1" );
            poNeighborOpts->dfRadius1 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius2" );
            poNeighborOpts->dfRadius2 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "angle" );
            poNeighborOpts->dfAngle = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poNeighborOpts->dfNoDataValue = pszValue ? CPLAtofM(pszValue) : 0.0;
            break;
        }
        case GGA_MetricMinimum:
        case GGA_MetricMaximum:
        case GGA_MetricRange:
        case GGA_MetricCount:
        case GGA_MetricAverageDistance:
        case GGA_MetricAverageDistancePts:
        {
            *ppOptions =
                CPLMalloc( sizeof(GDALGridDataMetricsOptions) );

            GDALGridDataMetricsOptions * const poMetricsOptions =
                static_cast<GDALGridDataMetricsOptions *>(*ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "radius1" );
            poMetricsOptions->dfRadius1 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "radius2" );
            poMetricsOptions->dfRadius2 = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "angle" );
            poMetricsOptions->dfAngle = pszValue ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParams, "min_points" );
            poMetricsOptions->nMinPoints = pszValue ? atoi(pszValue) : 0;

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poMetricsOptions->dfNoDataValue =
                pszValue ? CPLAtofM(pszValue) : 0.0;
            break;
        }
        case GGA_Linear:
        {
            *ppOptions =
                CPLMalloc( sizeof(GDALGridLinearOptions) );

            GDALGridLinearOptions * const poLinearOpts =
                static_cast<GDALGridLinearOptions *>(*ppOptions);

            const char *pszValue = CSLFetchNameValue( papszParams, "radius" );
            poLinearOpts->dfRadius = pszValue ? CPLAtofM(pszValue) : -1.0;

            pszValue = CSLFetchNameValue( papszParams, "nodata" );
            poLinearOpts->dfNoDataValue = pszValue ? CPLAtofM(pszValue) : 0.0;
            break;
         }
    }

    CSLDestroy( papszParams );
    return CE_None;
}
