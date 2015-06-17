/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Implementation of GDAL scattered data gridder.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_vsi.h"
#include "cpl_string.h"
#include "gdalgrid.h"
#include <float.h>
#include <limits.h>
#include "cpl_multiproc.h"
#include "gdalgrid_priv.h"

CPL_CVSID("$Id$");

#define TO_RADIANS (3.14159265358979323846 / 180.0)

#ifndef DBL_MAX
# ifdef __DBL_MAX__
#  define DBL_MAX __DBL_MAX__
# else
#  define DBL_MAX 1.7976931348623157E+308
# endif /* __DBL_MAX__ */
#endif /* DBL_MAX */

/************************************************************************/
/*                        GDALGridGetPointBounds()                      */
/************************************************************************/

static void GDALGridGetPointBounds(const void* hFeature, CPLRectObj* pBounds)
{
    GDALGridPoint* psPoint = (GDALGridPoint*) hFeature;
    GDALGridXYArrays* psXYArrays = psPoint->psXYArrays;
    int i = psPoint->i;
    double dfX = psXYArrays->padfX[i];
    double dfY = psXYArrays->padfY[i];
    pBounds->minx = dfX;
    pBounds->miny = dfY;
    pBounds->maxx = dfX;
    pBounds->maxy = dfY;
};

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridInverseDistanceToAPowerOptions object. 
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
GDALGridInverseDistanceToAPower( const void *poOptions, GUInt32 nPoints,
                                 const double *padfX, const double *padfY,
                                 const double *padfZ,
                                 double dfXPoint, double dfYPoint,
                                 double *pdfValue,
                                 CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double      dfRadius1 =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfRadius1;
    double      dfRadius2 =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double dfAngle = TO_RADIANS
        * ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    const double    dfPowerDiv2 =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfPower / 2;
    const double    dfSmoothing =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfSmoothing;
    const GUInt32   nMaxPoints = 
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->nMaxPoints;
    double  dfNominator = 0.0, dfDenominator = 0.0;
    GUInt32 i, n = 0;

    for ( i = 0; i < nPoints; i++ )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;
        const double dfR2 =
            dfRX * dfRX + dfRY * dfRY + dfSmoothing * dfSmoothing;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            // If the test point is close to the grid node, use the point
            // value directly as a node value to avoid singularity.
            if ( dfR2 < 0.0000000000001 )
            {
                (*pdfValue) = padfZ[i];
                return CE_None;
            }
            else
            {
                const double dfW = pow( dfR2, dfPowerDiv2 );
                double dfInvW = 1.0 / dfW;
                dfNominator += dfInvW * padfZ[i];
                dfDenominator += dfInvW;
                n++;
                if ( nMaxPoints > 0 && n > nMaxPoints )
                    break;
            }
        }
    }

    if ( n < ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->nMinPoints
         || dfDenominator == 0.0 )
    {
        (*pdfValue) =
            ((GDALGridInverseDistanceToAPowerOptions*)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfNominator / dfDenominator;

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
GDALGridInverseDistanceToAPowerNoSearch( const void *poOptions, GUInt32 nPoints,
                                         const double *padfX, const double *padfY,
                                         const double *padfZ,
                                         double dfXPoint, double dfYPoint,
                                         double *pdfValue,
                                         CPL_UNUSED void* hExtraParamsIn )
{
    const double    dfPowerDiv2 =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfPower / 2;
    const double    dfSmoothing =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfSmoothing;
    const double    dfSmoothing2 = dfSmoothing * dfSmoothing;
    double  dfNominator = 0.0, dfDenominator = 0.0;
    GUInt32 i = 0;
    int bPower2 = (dfPowerDiv2 == 1.0);

    if( bPower2 )
    {
        if( dfSmoothing2 > 0 )
        {
            for ( i = 0; i < nPoints; i++ )
            {
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;
                const double dfR2 =
                    dfRX * dfRX + dfRY * dfRY + dfSmoothing2;

                double dfInvR2 = 1.0 / dfR2;
                dfNominator += dfInvR2 * padfZ[i];
                dfDenominator += dfInvR2;
            }
        }
        else
        {
            for ( i = 0; i < nPoints; i++ )
            {
                const double dfRX = padfX[i] - dfXPoint;
                const double dfRY = padfY[i] - dfYPoint;
                const double dfR2 =
                    dfRX * dfRX + dfRY * dfRY;

                // If the test point is close to the grid node, use the point
                // value directly as a node value to avoid singularity.
                if ( dfR2 < 0.0000000000001 )
                {
                    break;
                }
                else
                {
                    double dfInvR2 = 1.0 / dfR2;
                    dfNominator += dfInvR2 * padfZ[i];
                    dfDenominator += dfInvR2;
                }
            }
        }
    }
    else
    {
        for ( i = 0; i < nPoints; i++ )
        {
            const double dfRX = padfX[i] - dfXPoint;
            const double dfRY = padfY[i] - dfYPoint;
            const double dfR2 =
                dfRX * dfRX + dfRY * dfRY + dfSmoothing2;

            // If the test point is close to the grid node, use the point
            // value directly as a node value to avoid singularity.
            if ( dfR2 < 0.0000000000001 )
            {
                break;
            }
            else
            {
                const double dfW = pow( dfR2, dfPowerDiv2 );
                double dfInvW = 1.0 / dfW;
                dfNominator += dfInvW * padfZ[i];
                dfDenominator += dfInvW;
            }
        }
    }

    if( i != nPoints )
    {
        (*pdfValue) = padfZ[i];
    }
    else
    if ( dfDenominator == 0.0 )
    {
        (*pdfValue) =
            ((GDALGridInverseDistanceToAPowerOptions*)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfNominator / dfDenominator;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridMovingAverageOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridMovingAverage( const void *poOptions, GUInt32 nPoints,
                       const double *padfX, const double *padfY,
                       const double *padfZ,
                       double dfXPoint, double dfYPoint, double *pdfValue,
                       CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 = ((GDALGridMovingAverageOptions *)poOptions)->dfRadius1;
    double  dfRadius2 = ((GDALGridMovingAverageOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridMovingAverageOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double  dfAccumulator = 0.0;
    GUInt32 i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            dfAccumulator += padfZ[i];
            n++;
        }

        i++;
    }

    if ( n < ((GDALGridMovingAverageOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridMovingAverageOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfAccumulator / n;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridNearestNeighborOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridNearestNeighbor( const void *poOptions, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         const double *padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         void* hExtraParamsIn)
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridNearestNeighborOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridNearestNeighborOptions *)poOptions)->dfRadius2;
    double  dfR12;
    GDALGridExtraParameters* psExtraParams = (GDALGridExtraParameters*) hExtraParamsIn;
    CPLQuadTree* hQuadTree = psExtraParams->hQuadTree;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridNearestNeighborOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    // If the nearest point will not be found, its value remains as NODATA.
    double      dfNearestValue =
        ((GDALGridNearestNeighborOptions *)poOptions)->dfNoDataValue;
    // Nearest distance will be initialized with the distance to the first
    // point in array.
    double      dfNearestR = DBL_MAX;
    GUInt32 i = 0;

    double dfSearchRadius = psExtraParams->dfInitialSearchRadius;
    if( hQuadTree != NULL && dfRadius1 == dfRadius2 && dfSearchRadius > 0 )
    {
        CPLRectObj sAoi;
        if( dfRadius1 > 0 )
            dfSearchRadius = ((GDALGridNearestNeighborOptions *)poOptions)->dfRadius1;
        while(dfSearchRadius > 0)
        {
            sAoi.minx = dfXPoint - dfSearchRadius;
            sAoi.miny = dfYPoint - dfSearchRadius;
            sAoi.maxx = dfXPoint + dfSearchRadius;
            sAoi.maxy = dfYPoint + dfSearchRadius;
            int nFeatureCount = 0;
            GDALGridPoint** papsPoints = (GDALGridPoint**)
                    CPLQuadTreeSearch(hQuadTree, &sAoi, &nFeatureCount);
            if( nFeatureCount != 0 )
            {
                if( dfRadius1 > 0 )
                    dfNearestR = dfRadius1;
                for(int k=0; k<nFeatureCount; k++)
                {
                    int i = papsPoints[k]->i;
                    double  dfRX = padfX[i] - dfXPoint;
                    double  dfRY = padfY[i] - dfYPoint;

                    const double    dfR2 = dfRX * dfRX + dfRY * dfRY;
                    if( dfR2 <= dfNearestR )
                    {
                        dfNearestR = dfR2;
                        dfNearestValue = padfZ[i];
                    }
                }

                CPLFree(papsPoints);
                break;
            }
            else
            {
                CPLFree(papsPoints);
                if( dfRadius1 > 0 )
                    break;
                dfSearchRadius *= 2;
                //CPLDebug("GDAL_GRID", "Increasing search radius to %.16g", dfSearchRadius);
            }
        }
    }
    else
    {
        while ( i < nPoints )
        {
            double  dfRX = padfX[i] - dfXPoint;
            double  dfRY = padfY[i] - dfYPoint;

            if ( bRotated )
            {
                double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
                double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

                dfRX = dfRXRotated;
                dfRY = dfRYRotated;
            }

            // Is this point located inside the search ellipse?
            if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            {
                const double    dfR2 = dfRX * dfRX + dfRY * dfRY;
                if ( dfR2 <= dfNearestR )
                {
                    dfNearestR = dfR2;
                    dfNearestValue = padfZ[i];
                }
            }

            i++;
        }
    }

    (*pdfValue) = dfNearestValue;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricMinimum( const void *poOptions, GUInt32 nPoints,
                           const double *padfX, const double *padfY,
                           const double *padfZ,
                           double dfXPoint, double dfYPoint, double *pdfValue,
                           CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double      dfMinimumValue=0.0;
    GUInt32     i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            if ( n )
            {
                if ( dfMinimumValue > padfZ[i] )
                    dfMinimumValue = padfZ[i];
            }
            else
                dfMinimumValue = padfZ[i];
            n++;
        }

        i++;
    }

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfMinimumValue;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricMaximum( const void *poOptions, GUInt32 nPoints,
                           const double *padfX, const double *padfY,
                           const double *padfZ,
                           double dfXPoint, double dfYPoint, double *pdfValue,
                           CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double      dfMaximumValue=0.0;
    GUInt32     i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            if ( n )
            {
                if ( dfMaximumValue < padfZ[i] )
                    dfMaximumValue = padfZ[i];
            }
            else
                dfMaximumValue = padfZ[i];
            n++;
        }

        i++;
    }

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfMaximumValue;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricRange( const void *poOptions, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         const double *padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double      dfMaximumValue=0.0, dfMinimumValue=0.0;
    GUInt32     i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            if ( n )
            {
                if ( dfMinimumValue > padfZ[i] )
                    dfMinimumValue = padfZ[i];
                if ( dfMaximumValue < padfZ[i] )
                    dfMaximumValue = padfZ[i];
            }
            else
                dfMinimumValue = dfMaximumValue = padfZ[i];
            n++;
        }

        i++;
    }

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfMaximumValue - dfMinimumValue;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricCount( const void *poOptions, GUInt32 nPoints,
                         const double *padfX, const double *padfY,
                         CPL_UNUSED const double *padfZ,
                         double dfXPoint, double dfYPoint, double *pdfValue,
                         CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    GUInt32     i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
            n++;

        i++;
    }

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = (double)n;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricAverageDistance( const void *poOptions, GUInt32 nPoints,
                                   const double *padfX, const double *padfY,
                                   CPL_UNUSED const double *padfZ,
                                   double dfXPoint, double dfYPoint,
                                   double *pdfValue,
                                   CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double      dfAccumulator = 0.0;
    GUInt32     i = 0, n = 0;

    while ( i < nPoints )
    {
        double  dfRX = padfX[i] - dfXPoint;
        double  dfRY = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX * dfCoeff1 + dfRY * dfCoeff2;
            double dfRYRotated = dfRY * dfCoeff1 - dfRX * dfCoeff2;

            dfRX = dfRXRotated;
            dfRY = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX * dfRX + dfRadius1 * dfRY * dfRY <= dfR12 )
        {
            dfAccumulator += sqrt( dfRX * dfRX + dfRY * dfRY );
            n++;
        }

        i++;
    }

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfAccumulator / n;

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
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridDataMetricsOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridDataMetricAverageDistancePts( const void *poOptions, GUInt32 nPoints,
                                      const double *padfX, const double *padfY,
                                      CPL_UNUSED const double *padfZ,
                                      double dfXPoint, double dfYPoint,
                                      double *pdfValue,
                                      CPL_UNUSED void* hExtraParamsIn )
{
    // TODO: For optimization purposes pre-computed parameters should be moved
    // out of this routine to the calling function.

    // Pre-compute search ellipse parameters
    double  dfRadius1 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius1;
    double  dfRadius2 =
        ((GDALGridDataMetricsOptions *)poOptions)->dfRadius2;
    double  dfR12;

    dfRadius1 *= dfRadius1;
    dfRadius2 *= dfRadius2;
    dfR12 = dfRadius1 * dfRadius2;

    // Compute coefficients for coordinate system rotation.
    double      dfCoeff1 = 0.0, dfCoeff2 = 0.0;
    const double    dfAngle =
        TO_RADIANS * ((GDALGridDataMetricsOptions *)poOptions)->dfAngle;
    const bool  bRotated = ( dfAngle == 0.0 ) ? false : true;
    if ( bRotated )
    {
        dfCoeff1 = cos(dfAngle);
        dfCoeff2 = sin(dfAngle);
    }

    double      dfAccumulator = 0.0;
    GUInt32     i = 0, n = 0;

    // Search for the first point within the search ellipse
    while ( i < nPoints - 1 )
    {
        double  dfRX1 = padfX[i] - dfXPoint;
        double  dfRY1 = padfY[i] - dfYPoint;

        if ( bRotated )
        {
            double dfRXRotated = dfRX1 * dfCoeff1 + dfRY1 * dfCoeff2;
            double dfRYRotated = dfRY1 * dfCoeff1 - dfRX1 * dfCoeff2;

            dfRX1 = dfRXRotated;
            dfRY1 = dfRYRotated;
        }

        // Is this point located inside the search ellipse?
        if ( dfRadius2 * dfRX1 * dfRX1 + dfRadius1 * dfRY1 * dfRY1 <= dfR12 )
        {
            GUInt32 j;
            
            // Search all the remaining points within the ellipse and compute
            // distances between them and the first point
            for ( j = i + 1; j < nPoints; j++ )
            {
                double  dfRX2 = padfX[j] - dfXPoint;
                double  dfRY2 = padfY[j] - dfYPoint;
                
                if ( bRotated )
                {
                    double dfRXRotated = dfRX2 * dfCoeff1 + dfRY2 * dfCoeff2;
                    double dfRYRotated = dfRY2 * dfCoeff1 - dfRX2 * dfCoeff2;

                    dfRX2 = dfRXRotated;
                    dfRY2 = dfRYRotated;
                }

                if ( dfRadius2 * dfRX2 * dfRX2 + dfRadius1 * dfRY2 * dfRY2 <= dfR12 )
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

    if ( n < ((GDALGridDataMetricsOptions *)poOptions)->nMinPoints
         || n == 0 )
    {
        (*pdfValue) =
            ((GDALGridDataMetricsOptions *)poOptions)->dfNoDataValue;
    }
    else
        (*pdfValue) = dfAccumulator / n;

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
 * algorithm will use the value of the nearest point or the nodata value.
 *
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridLinearOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param dfXPoint X coordinate of the point to compute.
 * @param dfYPoint Y coordinate of the point to compute.
 * @param pdfValue Pointer to variable where the computed grid node value
 * will be returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 *
 * @since GDAL 2.1
 */

CPLErr
GDALGridLinear( const void *poOptions, CPL_UNUSED GUInt32 nPoints,
                const double *padfX, const double *padfY,
                const double *padfZ,
                double dfXPoint, double dfYPoint, double *pdfValue,
                void* hExtraParams )
{
    GDALGridExtraParameters* psExtraParams = (GDALGridExtraParameters*) hExtraParams;
    GDALTriangulation* psTriangulation = psExtraParams->psTriangulation;

    int nOutputFacetIdx = -1;
    int bRet = GDALTriangulationFindFacetDirected( psTriangulation,
                                                   psExtraParams->nInitialFacetIdx,
                                                   dfXPoint, dfYPoint,
                                                   &nOutputFacetIdx );
    CPLAssert(nOutputFacetIdx >= 0);
    // Reuse output facet idx as next initial index since we proceed line by line
    psExtraParams->nInitialFacetIdx = nOutputFacetIdx;
    
    if( bRet )
    {
        double lambda1, lambda2, lambda3;
        GDALTriangulationComputeBarycentricCoordinates(
                                                psTriangulation,
                                                nOutputFacetIdx,
                                                dfXPoint, dfYPoint,
                                                &lambda1, &lambda2, &lambda3);
        int i1 = psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[0];
        int i2 = psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[1];
        int i3 = psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[2];
        *pdfValue = lambda1 * padfZ[i1] + lambda2 * padfZ[i2] + lambda3 * padfZ[i3];
    }
    else 
    {
        double dfRadius = ((GDALGridLinearOptions*)poOptions)->dfRadius;
        if( dfRadius == 0 )
            *pdfValue = ((GDALGridLinearOptions*)poOptions)->dfNoDataValue;
        else
        {
            GDALGridNearestNeighborOptions sNeighbourOptions;
            sNeighbourOptions.dfRadius1 = dfRadius < 0 ? 0 : dfRadius;
            sNeighbourOptions.dfRadius2 = dfRadius < 0 ? 0 : dfRadius;
            sNeighbourOptions.dfAngle = 0;
            sNeighbourOptions.dfNoDataValue = ((GDALGridLinearOptions*)poOptions)->dfNoDataValue;
            return GDALGridNearestNeighbor(&sNeighbourOptions, nPoints,
                                        padfX, padfY, padfZ,
                                        dfXPoint, dfYPoint, pdfValue,
                                        hExtraParams);
#if 0
            // Disabled since the nearest point is not necessarily the nearest
            // vertex of the "nearest" triangle
            int iBestIdx;
            double dfBestDist;
            for(int i=0;i<3;i++)
            {
                int idx = psTriangulation->pasFacets[nOutputFacetIdx].anVertexIdx[i];
                double dfDist = (padfX[idx] - dfXPoint) * (padfX[idx] - dfXPoint) +
                                (padfY[idx] - dfYPoint) * (padfY[idx] - dfYPoint);
                if( i == 0 || dfDist < dfBestDist )
                {
                    iBestIdx = idx;
                    dfBestDist = dfDist;
                }
            }
            if( dfRadius < 0 || dfBestDist <= dfRadius * dfRadius )
                *pdfValue = padfZ[iBestIdx];
            else
                *pdfValue = ((GDALGridLinearOptions*)poOptions)->dfNoDataValue;
#endif
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

    CPLJoinableThread  *hThread;
    volatile int   *pnCounter;
    volatile int   *pbStop;
    CPLCond        *hCond;
    CPLMutex       *hCondMutex;

    GDALProgressFunc pfnRealProgress;
    void *pRealProgressArg;
};

/************************************************************************/
/*                   GDALGridProgressMultiThread()                      */
/************************************************************************/

/* Return TRUE if the computation must be interrupted */
static int GDALGridProgressMultiThread(GDALGridJob* psJob)
{
    CPLAcquireMutex(psJob->hCondMutex, 1.0);
    (*(psJob->pnCounter)) ++;
    CPLCondSignal(psJob->hCond);
    int bStop = *(psJob->pbStop);
    CPLReleaseMutex(psJob->hCondMutex);

    return bStop;
}

/************************************************************************/
/*                      GDALGridProgressMonoThread()                    */
/************************************************************************/

/* Return TRUE if the computation must be interrupted */
static int GDALGridProgressMonoThread(GDALGridJob* psJob)
{
    int nCounter = ++(*(psJob->pnCounter));
    if( !psJob->pfnRealProgress( (nCounter / (double) psJob->nYSize),
                                 "", psJob->pRealProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        *(psJob->pbStop) = TRUE;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALGridJobProcess()                         */
/************************************************************************/

static void GDALGridJobProcess(void* user_data)
{
    GDALGridJob* psJob = (GDALGridJob*)user_data;
    GUInt32 nXPoint, nYPoint;

    const GUInt32 nYStart = psJob->nYStart;
    const GUInt32 nYStep = psJob->nYStep;
    GByte *pabyData = psJob->pabyData;

    const GUInt32 nXSize = psJob->nXSize;
    const GUInt32 nYSize = psJob->nYSize;
    const double dfXMin = psJob->dfXMin;
    const double dfYMin = psJob->dfYMin;
    const double dfDeltaX = psJob->dfDeltaX;
    const double dfDeltaY = psJob->dfDeltaY;
    GUInt32 nPoints = psJob->nPoints;
    const double* padfX = psJob->padfX;
    const double* padfY = psJob->padfY;
    const double* padfZ = psJob->padfZ;
    const void *poOptions = psJob->poOptions;
    GDALGridFunction  pfnGDALGridMethod = psJob->pfnGDALGridMethod;
    // Have a local copy of sExtraParameters since we want to modify
    // nInitialFacetIdx
    GDALGridExtraParameters sExtraParameters = *(psJob->psExtraParameters);
    GDALDataType eType = psJob->eType;
    int (*pfnProgress)(GDALGridJob* psJob) = psJob->pfnProgress;

    int         nDataTypeSize = GDALGetDataTypeSize(eType) / 8;
    int         nLineSpace = nXSize * nDataTypeSize;

    /* -------------------------------------------------------------------- */
    /*  Allocate a buffer of scanline size, fill it with gridded values     */
    /*  and use GDALCopyWords() to copy values into output data array with  */
    /*  appropriate data type conversion.                                   */
    /* -------------------------------------------------------------------- */
    double      *padfValues = (double *)VSIMalloc2( sizeof(double), nXSize );
    if( padfValues == NULL )
    {
        *(psJob->pbStop) = TRUE;
        pfnProgress(psJob); /* to notify the main thread */
        return;
    }

    for ( nYPoint = nYStart; nYPoint < nYSize; nYPoint += nYStep )
    {
        const double    dfYPoint = dfYMin + ( nYPoint + 0.5 ) * dfDeltaY;

        for ( nXPoint = 0; nXPoint < nXSize; nXPoint++ )
        {
            const double    dfXPoint = dfXMin + ( nXPoint + 0.5 ) * dfDeltaX;

            if ( (*pfnGDALGridMethod)( poOptions, nPoints, padfX, padfY, padfZ,
                                       dfXPoint, dfYPoint,
                                       padfValues + nXPoint, &sExtraParameters ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Gridding failed at X position %lu, Y position %lu",
                          (long unsigned int)nXPoint,
                          (long unsigned int)nYPoint );
                *(psJob->pbStop) = TRUE;
                pfnProgress(psJob); /* to notify the main thread */
                break;
            }
        }

        GDALCopyWords( padfValues, GDT_Float64, sizeof(double),
                       pabyData + nYPoint * nLineSpace, eType, nDataTypeSize,
                       nXSize );

        if( *(psJob->pbStop) || pfnProgress(psJob) )
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
    int                 bFreePadfXYZArrays;

    void               *pabyX;
    void               *pabyY;
    void               *pabyZ;
};

static void GDALGridContextCreateQuadTree(GDALGridContext* psContext);

/**
 * Creates a context to do regular gridding from the scattered data.
 *
 * This function takes the arrays of X and Y coordinates and corresponding Z
 * values as input to preprare computation of regular grid (or call it a raster) from these
 * scattered data.
 *
 * On Intel/AMD i386/x86_64 architectures, some
 * gridding methods will be optimized with SSE instructions (provided GDAL
 * has been compiled with such support, and it is availabable at runtime).
 * Currently, only 'invdist' algorithm with default parameters has an optimized
 * implementation.
 * This can provide substantial speed-up, but sometimes at the expense of
 * reduced floating point precision. This can be disabled by setting the
 * GDAL_USE_SSE configuration option to NO.
 * A further optimized version can use the AVX
 * instruction set. This can be disabled by setting the GDAL_USE_AVX
 * configuration option to NO.
 * 
 *
 * @param eAlgorithm Gridding method. 
 * @param poOptions Options to control choosen gridding method.
 * @param nPoints Number of elements in input arrays.
 * @param padfX Input array of X coordinates. 
 * @param padfY Input array of Y coordinates. 
 * @param padfZ Input array of Z values. 
 * @param bCallerWillKeepPointArraysAlive Whether the provided padfX, padfY, padfZ
 *        arrays will still be "alive" during the calls to GDALGridContextProcess().
 *        Setting to TRUE prevent them from being duplicated in the context.
 *        If unsure, set to FALSE.
 *
 * @return the context (to be freed with GDALGridContextFree()) or NULL in case or error
 *
 * @since GDAL 2.1
 */

GDALGridContext*
GDALGridContextCreate( GDALGridAlgorithm eAlgorithm, const void *poOptions,
                       GUInt32 nPoints,
                       const double *padfX, const double *padfY, const double *padfZ,
                       int bCallerWillKeepPointArraysAlive )
{
    GDALGridFunction    pfnGDALGridMethod = NULL;
    CPLAssert( poOptions );
    CPLAssert( padfX );
    CPLAssert( padfY );
    CPLAssert( padfZ );
    int bCreateQuadTree = FALSE;

    /* Potentially unaligned pointers */
    void* pabyX = NULL;
    void* pabyY = NULL;
    void* pabyZ = NULL;

    /* Starting address aligned on 16-byte boundary */
    float* pafXAligned = NULL;
    float* pafYAligned = NULL;
    float* pafZAligned = NULL;

    void *poOptionsNew = NULL;

    switch ( eAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
            poOptionsNew = CPLMalloc(sizeof(GDALGridInverseDistanceToAPowerOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridInverseDistanceToAPowerOptions));

            if ( ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->
                 dfRadius1 == 0.0 &&
                 ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->
                 dfRadius2 == 0.0 )
            {
                const double    dfPower =
                    ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfPower;
                const double    dfSmoothing =
                    ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfSmoothing;

                pfnGDALGridMethod = GDALGridInverseDistanceToAPowerNoSearch;
                if( dfPower == 2.0 && dfSmoothing == 0.0 )
                {
#ifdef HAVE_AVX_AT_COMPILE_TIME

#define ALIGN32(x)  (((char*)(x)) + ((32 - (((size_t)(x)) % 32)) % 32))

                    if( CSLTestBoolean(CPLGetConfigOption("GDAL_USE_AVX", "YES")) &&
                        CPLHaveRuntimeAVX() )
                    {
                        pabyX = (float*)VSIMalloc(sizeof(float) * nPoints + 31);
                        pabyY = (float*)VSIMalloc(sizeof(float) * nPoints + 31);
                        pabyZ = (float*)VSIMalloc(sizeof(float) * nPoints + 31);
                        if( pabyX != NULL && pabyY != NULL && pabyZ != NULL)
                        {
                            CPLDebug("GDAL_GRID", "Using AVX optimized version");
                            pafXAligned = (float*) ALIGN32(pabyX);
                            pafYAligned = (float*) ALIGN32(pabyY);
                            pafZAligned = (float*) ALIGN32(pabyZ);
                            pfnGDALGridMethod = GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX;
                            GUInt32 i;
                            for(i=0;i<nPoints;i++)
                            {
                                pafXAligned[i] = (float) padfX[i];
                                pafYAligned[i] = (float) padfY[i];
                                pafZAligned[i] = (float) padfZ[i];
                            }
                        }
                        else
                        {
                            VSIFree(pabyX);
                            VSIFree(pabyY);
                            VSIFree(pabyZ);
                            pabyX = pabyY = pabyZ = NULL;
                        }
                    }
#endif

#ifdef HAVE_SSE_AT_COMPILE_TIME

#define ALIGN16(x)  (((char*)(x)) + ((16 - (((size_t)(x)) % 16)) % 16))

                    if( pafXAligned == NULL &&
                        CSLTestBoolean(CPLGetConfigOption("GDAL_USE_SSE", "YES")) &&
                        CPLHaveRuntimeSSE() )
                    {
                        pabyX = (float*)VSIMalloc(sizeof(float) * nPoints + 15);
                        pabyY = (float*)VSIMalloc(sizeof(float) * nPoints + 15);
                        pabyZ = (float*)VSIMalloc(sizeof(float) * nPoints + 15);
                        if( pabyX != NULL && pabyY != NULL && pabyZ != NULL)
                        {
                            CPLDebug("GDAL_GRID", "Using SSE optimized version");
                            pafXAligned = (float*) ALIGN16(pabyX);
                            pafYAligned = (float*) ALIGN16(pabyY);
                            pafZAligned = (float*) ALIGN16(pabyZ);
                            pfnGDALGridMethod = GDALGridInverseDistanceToAPower2NoSmoothingNoSearchSSE;
                            GUInt32 i;
                            for(i=0;i<nPoints;i++)
                            {
                                pafXAligned[i] = (float) padfX[i];
                                pafYAligned[i] = (float) padfY[i];
                                pafZAligned[i] = (float) padfZ[i];
                            }
                        }
                        else
                        {
                            VSIFree(pabyX);
                            VSIFree(pabyY);
                            VSIFree(pabyZ);
                            pabyX = pabyY = pabyZ = NULL;
                        }
                    }
#endif // HAVE_SSE_AT_COMPILE_TIME
                }
            }
            else
                pfnGDALGridMethod = GDALGridInverseDistanceToAPower;
            break;

        case GGA_MovingAverage:
            poOptionsNew = CPLMalloc(sizeof(GDALGridMovingAverageOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridMovingAverageOptions));
            
            pfnGDALGridMethod = GDALGridMovingAverage;
            break;

        case GGA_NearestNeighbor:
            poOptionsNew = CPLMalloc(sizeof(GDALGridNearestNeighborOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridNearestNeighborOptions));

            pfnGDALGridMethod = GDALGridNearestNeighbor;
            bCreateQuadTree = (nPoints > 100 &&
                (((GDALGridNearestNeighborOptions *)poOptions)->dfRadius1 ==
                ((GDALGridNearestNeighborOptions *)poOptions)->dfRadius2));
            break;

        case GGA_MetricMinimum:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricMinimum;
            break;

        case GGA_MetricMaximum:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricMaximum;
            break;

        case GGA_MetricRange:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricRange;
            break;

        case GGA_MetricCount:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricCount;
            break;

        case GGA_MetricAverageDistance:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricAverageDistance;
            break;

        case GGA_MetricAverageDistancePts:
            poOptionsNew = CPLMalloc(sizeof(GDALGridDataMetricsOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridDataMetricsOptions));

            pfnGDALGridMethod = GDALGridDataMetricAverageDistancePts;
            break;

        case GGA_Linear:
            poOptionsNew = CPLMalloc(sizeof(GDALGridLinearOptions));
            memcpy(poOptionsNew, poOptions, sizeof(GDALGridLinearOptions));

            pfnGDALGridMethod = GDALGridLinear;
            break;

        default:
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "GDAL does not support gridding method %d", eAlgorithm );
            return NULL;
    }

    if( pafXAligned != NULL )
        bCallerWillKeepPointArraysAlive = TRUE;
    if( !bCallerWillKeepPointArraysAlive )
    {
        double* padfXNew = (double*)VSIMalloc2(nPoints, sizeof(double));
        double* padfYNew = (double*)VSIMalloc2(nPoints, sizeof(double));
        double* padfZNew = (double*)VSIMalloc2(nPoints, sizeof(double));
        if( padfXNew == NULL || padfYNew == NULL || padfZNew == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Not enough memory to duplicate X,Y,Z arrays");
            VSIFree(padfXNew);
            VSIFree(padfYNew);
            VSIFree(padfZNew);
            CPLFree(poOptionsNew);
            return NULL;
        }
        memcpy(padfXNew, padfX, nPoints * sizeof(double));
        memcpy(padfYNew, padfY, nPoints * sizeof(double));
        memcpy(padfZNew, padfZ, nPoints * sizeof(double));
        padfX = padfXNew;
        padfY = padfYNew;
        padfZ = padfZNew;
    }

    GDALGridContext* psContext = (GDALGridContext*)CPLMalloc(sizeof(GDALGridContext));
    psContext->eAlgorithm = eAlgorithm;
    psContext->poOptions = poOptionsNew;
    psContext->pfnGDALGridMethod = pfnGDALGridMethod;
    psContext->nPoints = nPoints;
    psContext->pasGridPoints = NULL;
    psContext->sXYArrays.padfX = padfX;
    psContext->sXYArrays.padfY = padfY;
    psContext->sExtraParameters.hQuadTree = NULL;
    psContext->sExtraParameters.dfInitialSearchRadius = 0;
    psContext->sExtraParameters.pafX = pafXAligned;
    psContext->sExtraParameters.pafY = pafYAligned;
    psContext->sExtraParameters.pafZ = pafZAligned;
    psContext->sExtraParameters.psTriangulation = NULL;
    psContext->sExtraParameters.nInitialFacetIdx = 0;
    psContext->padfX = pafXAligned ? NULL : (double*)padfX;
    psContext->padfY = pafXAligned ? NULL : (double*)padfY;
    psContext->padfZ = pafXAligned ? NULL : (double*)padfZ;
    psContext->bFreePadfXYZArrays = !bCallerWillKeepPointArraysAlive;
    psContext->pabyX = pabyX;
    psContext->pabyY = pabyY;
    psContext->pabyZ = pabyZ;
    
/* -------------------------------------------------------------------- */
/*  Create quadtree if requested and possible.                          */
/* -------------------------------------------------------------------- */
    if( bCreateQuadTree )
    {
        GDALGridContextCreateQuadTree(psContext);
    }
    
    if( eAlgorithm == GGA_Linear )
    {
        psContext->sExtraParameters.psTriangulation =
                GDALTriangulationCreateDelaunay(nPoints, padfX, padfY);
        if( psContext->sExtraParameters.psTriangulation == NULL )
        {
            GDALGridContextFree(psContext);
            return NULL;
        }
        GDALTriangulationComputeBarycentricCoefficients(
            psContext->sExtraParameters.psTriangulation, padfX, padfY );
    }

    return psContext;
}

/************************************************************************/
/*                      GDALGridContextCreateQuadTree()                 */
/************************************************************************/

void GDALGridContextCreateQuadTree(GDALGridContext* psContext)
{
    GUInt32 nPoints = psContext->nPoints;
    psContext->pasGridPoints = (GDALGridPoint*)
            VSIMalloc2(nPoints, sizeof(GDALGridPoint));
    if( psContext->pasGridPoints != NULL )
    {
        const double* padfX = psContext->padfX;
        const double* padfY = psContext->padfY;

        CPLRectObj sRect;
        GUInt32 i;

        /* Determine point extents */
        sRect.minx = padfX[0];
        sRect.miny = padfY[0];
        sRect.maxx = padfX[0];
        sRect.maxy = padfY[0];
        for(i = 1; i < nPoints; i++)
        {
            if( padfX[i] < sRect.minx ) sRect.minx = padfX[i];
            if( padfY[i] < sRect.miny ) sRect.miny = padfY[i];
            if( padfX[i] > sRect.maxx ) sRect.maxx = padfX[i];
            if( padfY[i] > sRect.maxy ) sRect.maxy = padfY[i];
        }

        /* Initial value for search radius is the typical dimension of a */
        /* "pixel" of the point array (assuming rather uniform distribution) */
        psContext->sExtraParameters.dfInitialSearchRadius =
                                sqrt((sRect.maxx - sRect.minx) *
                                        (sRect.maxy - sRect.miny) / nPoints);

        psContext->sExtraParameters.hQuadTree =
                    CPLQuadTreeCreate(&sRect, GDALGridGetPointBounds );

        for(i = 0; i < nPoints; i++)
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
void GDALGridContextFree(GDALGridContext* psContext)
{
    if( psContext )
    {
        CPLFree( psContext->poOptions );
        CPLFree( psContext->pasGridPoints );
        if( psContext->sExtraParameters.hQuadTree != NULL )
            CPLQuadTreeDestroy( psContext->sExtraParameters.hQuadTree );
        if( psContext->bFreePadfXYZArrays )
        {
            CPLFree(psContext->padfX);
            CPLFree(psContext->padfY);
            CPLFree(psContext->padfZ);
        }
        CPLFree(psContext->pabyX);
        CPLFree(psContext->pabyY);
        CPLFree(psContext->pabyZ);
        if( psContext->sExtraParameters.psTriangulation )
            GDALTriangulationFree( psContext->sExtraParameters.psTriangulation );
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
 * It is possible to set the GDAL_NUM_THREADS
 * configuration option to parallelize the processing. The value to set is
 * the number of worker threads, or ALL_CPUS to use all the cores/CPUs of the
 * computer (default value).
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

CPLErr GDALGridContextProcess(GDALGridContext* psContext,
                              double dfXMin, double dfXMax, double dfYMin, double dfYMax,
                              GUInt32 nXSize, GUInt32 nYSize, GDALDataType eType, void *pData,
                              GDALProgressFunc pfnProgress, void *pProgressArg )
{
    CPLAssert( psContext );
    CPLAssert( pData );

    if ( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if ( nXSize == 0 || nYSize == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Output raster dimesions should have non-zero size." );
        return CE_Failure;
    }

    const double    dfDeltaX = ( dfXMax - dfXMin ) / nXSize;
    const double    dfDeltaY = ( dfYMax - dfYMin ) / nYSize;
    
    // For linear, check if we will need to fallback to nearest neighbour
    // by sampling along the edges (if all points on edges are within triangles,
    // then interior points will also be!)
    if( psContext->eAlgorithm == GGA_Linear &&
        psContext->sExtraParameters.hQuadTree == NULL )
    {
        int bNeedNearest = FALSE;
        int nStartLeft = 0, nStartRight = 0;
        const double    dfXPointMin = dfXMin + ( 0 + 0.5 ) * dfDeltaX;
        const double    dfXPointMax = dfXMin + ( nXSize - 1 + 0.5 ) * dfDeltaX;
        for ( GUInt32 nYPoint = 0; !bNeedNearest && nYPoint < nYSize; nYPoint ++ )
        {
            const double    dfYPoint = dfYMin + ( nYPoint + 0.5 ) * dfDeltaY;

            if( !GDALTriangulationFindFacetDirected( psContext->sExtraParameters.psTriangulation,
                                                     nStartLeft,
                                                     dfXPointMin, dfYPoint,
                                                     &nStartLeft) )
            {
                bNeedNearest = TRUE;
            }
            if( !GDALTriangulationFindFacetDirected( psContext->sExtraParameters.psTriangulation,
                                                     nStartRight,
                                                     dfXPointMax, dfYPoint,
                                                     &nStartRight) )
            {
                bNeedNearest = TRUE;
            }
        }
        int nStartTop = 0, nStartBottom = 0;
        const double    dfYPointMin = dfYMin + ( 0 + 0.5 ) * dfDeltaY;
        const double    dfYPointMax = dfYMin + ( nYSize - 1 + 0.5 ) * dfDeltaY;
        for ( GUInt32 nXPoint = 1; !bNeedNearest && nXPoint + 1 < nXSize; nXPoint ++ )
        {
            const double    dfXPoint = dfXMin + ( nXPoint + 0.5 ) * dfDeltaX;

            if( !GDALTriangulationFindFacetDirected( psContext->sExtraParameters.psTriangulation,
                                                     nStartTop,
                                                     dfXPoint, dfYPointMin,
                                                     &nStartTop) )
            {
                bNeedNearest = TRUE;
            }
            if( !GDALTriangulationFindFacetDirected( psContext->sExtraParameters.psTriangulation,
                                                     nStartBottom,
                                                     dfXPoint, dfYPointMax,
                                                     &nStartBottom) )
            {
                bNeedNearest = TRUE;
            }
        }
        if( bNeedNearest )
        {
            CPLDebug("GDAL_GRID", "Will need nearest neighbour");
            GDALGridContextCreateQuadTree(psContext);
        }
    }

    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    int nThreads;
    if (EQUAL(pszThreads, "ALL_CPUS"))
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszThreads);
    if (nThreads > 128)
        nThreads = 128;
    if (nThreads >= (int)nYSize / 2)
        nThreads = (int)nYSize / 2;

    volatile int nCounter = 0;
    volatile int bStop = FALSE;

    GDALGridJob sJob;
    sJob.nYStart = 0;
    sJob.pabyData = (GByte*) pData;
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
    sJob.pfnProgress = NULL;
    sJob.eType = eType;
    sJob.pfnRealProgress = pfnProgress;
    sJob.pRealProgressArg = pProgressArg;
    sJob.pnCounter = &nCounter;
    sJob.pbStop = &bStop;
    sJob.hCond = NULL;
    sJob.hCondMutex = NULL;
    sJob.hThread = NULL;

    if( nThreads > 1 )
    {
        sJob.hCond = CPLCreateCond();
        if( sJob.hCond == NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot create condition. Reverting to monothread processing");
            nThreads = 1;
        }
    }

    if( nThreads <= 1 )
    {
        sJob.pfnProgress = GDALGridProgressMonoThread;

        GDALGridJobProcess(&sJob);
    }
    else
    {
        GDALGridJob* pasJobs = (GDALGridJob*) CPLMalloc(sizeof(GDALGridJob) * nThreads);
        int i;

        CPLDebug("GDAL_GRID", "Using %d threads", nThreads);

        sJob.nYStep = nThreads;
        sJob.hCondMutex = CPLCreateMutex(); /* and take implicitely the mutex */
        sJob.pfnProgress = GDALGridProgressMultiThread;

/* -------------------------------------------------------------------- */
/*      Start threads.                                                  */
/* -------------------------------------------------------------------- */
        for(i = 0; i < nThreads && !bStop; i++)
        {
            memcpy(&pasJobs[i], &sJob, sizeof(GDALGridJob));
            pasJobs[i].nYStart = i;
            pasJobs[i].hThread = CPLCreateJoinableThread( GDALGridJobProcess,
                                                          (void*) &pasJobs[i] );
        }

/* -------------------------------------------------------------------- */
/*      Report progress.                                                */
/* -------------------------------------------------------------------- */
        while(nCounter < (int)nYSize && !bStop)
        {
            CPLCondWait(sJob.hCond, sJob.hCondMutex);

            int nLocalCounter = nCounter;
            CPLReleaseMutex(sJob.hCondMutex);

            if( !pfnProgress( nLocalCounter / (double) nYSize, "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                bStop = TRUE;
            }

            CPLAcquireMutex(sJob.hCondMutex, 1.0);
        }

        /* Release mutex before joining threads, otherwise they will dead-lock */
        /* forever in GDALGridProgressMultiThread() */
        CPLReleaseMutex(sJob.hCondMutex);

/* -------------------------------------------------------------------- */
/*      Wait for all threads to complete and finish.                    */
/* -------------------------------------------------------------------- */
        for(i=0;i<nThreads;i++)
        {
            if( pasJobs[i].hThread )
                CPLJoinThread(pasJobs[i].hThread);
        }

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
 * has been compiled with such support, and it is availabable at runtime).
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
 * @param poOptions Options to control choosen gridding method.
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
    CPLErr eErr = CE_Failure;
    GDALGridContext* psContext = GDALGridContextCreate( eAlgorithm,poOptions,
                                                        nPoints,
                                                        padfX, padfY, padfZ,
                                                        TRUE );
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
/*                                                                      */
/*      Translates mnemonic gridding algorithm names into               */
/*      GDALGridAlgorithm code, parse control parameters and assign     */
/*      defaults.                                                       */
/************************************************************************/

CPLErr ParseAlgorithmAndOptions( const char *pszAlgorithm,
                                 GDALGridAlgorithm *peAlgorithm,
                                 void **ppOptions )
{
    CPLAssert( pszAlgorithm );
    CPLAssert( peAlgorithm );
    CPLAssert( ppOptions );

    *ppOptions = NULL;

    char **papszParms = CSLTokenizeString2( pszAlgorithm, ":", FALSE );

    if ( CSLCount(papszParms) < 1 )
        return CE_Failure;

    if ( EQUAL(papszParms[0], szAlgNameInvDist) )
        *peAlgorithm = GGA_InverseDistanceToAPower;
    else if ( EQUAL(papszParms[0], szAlgNameAverage) )
        *peAlgorithm = GGA_MovingAverage;
    else if ( EQUAL(papszParms[0], szAlgNameNearest) )
        *peAlgorithm = GGA_NearestNeighbor;
    else if ( EQUAL(papszParms[0], szAlgNameMinimum) )
        *peAlgorithm = GGA_MetricMinimum;
    else if ( EQUAL(papszParms[0], szAlgNameMaximum) )
        *peAlgorithm = GGA_MetricMaximum;
    else if ( EQUAL(papszParms[0], szAlgNameRange) )
        *peAlgorithm = GGA_MetricRange;
    else if ( EQUAL(papszParms[0], szAlgNameCount) )
        *peAlgorithm = GGA_MetricCount;
    else if ( EQUAL(papszParms[0], szAlgNameAverageDistance) )
        *peAlgorithm = GGA_MetricAverageDistance;
    else if ( EQUAL(papszParms[0], szAlgNameAverageDistancePts) )
        *peAlgorithm = GGA_MetricAverageDistancePts;
    else if ( EQUAL(papszParms[0], szAlgNameLinear) )
        *peAlgorithm = GGA_Linear;
    else
    {
        fprintf( stderr, "Unsupported gridding method \"%s\".\n",
                 papszParms[0] );
        CSLDestroy( papszParms );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Parse algorithm parameters and assign defaults.                 */
/* -------------------------------------------------------------------- */
    const char  *pszValue;

    switch ( *peAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
        default:
            *ppOptions =
                CPLMalloc( sizeof(GDALGridInverseDistanceToAPowerOptions) );

            pszValue = CSLFetchNameValue( papszParms, "power" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfPower = (pszValue) ? CPLAtofM(pszValue) : 2.0;

            pszValue = CSLFetchNameValue( papszParms, "smoothing" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfSmoothing = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "radius1" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfRadius1 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "radius2" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfRadius2 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "angle" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfAngle = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "max_points" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                nMaxPoints = (GUInt32) ((pszValue) ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParms, "min_points" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                nMinPoints = (GUInt32) ((pszValue) ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParms, "nodata" );
            ((GDALGridInverseDistanceToAPowerOptions *)*ppOptions)->
                dfNoDataValue = (pszValue) ? CPLAtofM(pszValue) : 0.0;
            break;

        case GGA_MovingAverage:
            *ppOptions =
                CPLMalloc( sizeof(GDALGridMovingAverageOptions) );

            pszValue = CSLFetchNameValue( papszParms, "radius1" );
            ((GDALGridMovingAverageOptions *)*ppOptions)->
                dfRadius1 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "radius2" );
            ((GDALGridMovingAverageOptions *)*ppOptions)->
                dfRadius2 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "angle" );
            ((GDALGridMovingAverageOptions *)*ppOptions)->
                dfAngle = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "min_points" );
            ((GDALGridMovingAverageOptions *)*ppOptions)->
                nMinPoints = (GUInt32) ((pszValue) ? CPLAtofM(pszValue) : 0);

            pszValue = CSLFetchNameValue( papszParms, "nodata" );
            ((GDALGridMovingAverageOptions *)*ppOptions)->
                dfNoDataValue = (pszValue) ? CPLAtofM(pszValue) : 0.0;
            break;

        case GGA_NearestNeighbor:
            *ppOptions =
                CPLMalloc( sizeof(GDALGridNearestNeighborOptions) );

            pszValue = CSLFetchNameValue( papszParms, "radius1" );
            ((GDALGridNearestNeighborOptions *)*ppOptions)->
                dfRadius1 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "radius2" );
            ((GDALGridNearestNeighborOptions *)*ppOptions)->
                dfRadius2 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "angle" );
            ((GDALGridNearestNeighborOptions *)*ppOptions)->
                dfAngle = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "nodata" );
            ((GDALGridNearestNeighborOptions *)*ppOptions)->
                dfNoDataValue = (pszValue) ? CPLAtofM(pszValue) : 0.0;
            break;

        case GGA_MetricMinimum:
        case GGA_MetricMaximum:
        case GGA_MetricRange:
        case GGA_MetricCount:
        case GGA_MetricAverageDistance:
        case GGA_MetricAverageDistancePts:
            *ppOptions =
                CPLMalloc( sizeof(GDALGridDataMetricsOptions) );

            pszValue = CSLFetchNameValue( papszParms, "radius1" );
            ((GDALGridDataMetricsOptions *)*ppOptions)->
                dfRadius1 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "radius2" );
            ((GDALGridDataMetricsOptions *)*ppOptions)->
                dfRadius2 = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "angle" );
            ((GDALGridDataMetricsOptions *)*ppOptions)->
                dfAngle = (pszValue) ? CPLAtofM(pszValue) : 0.0;

            pszValue = CSLFetchNameValue( papszParms, "min_points" );
            ((GDALGridDataMetricsOptions *)*ppOptions)->
                nMinPoints = (pszValue) ? atol(pszValue) : 0;

            pszValue = CSLFetchNameValue( papszParms, "nodata" );
            ((GDALGridDataMetricsOptions *)*ppOptions)->
                dfNoDataValue = (pszValue) ? CPLAtofM(pszValue) : 0.0;
            break;

        case GGA_Linear:
            *ppOptions =
                CPLMalloc( sizeof(GDALGridLinearOptions) );

            pszValue = CSLFetchNameValue( papszParms, "radius" );
            ((GDALGridLinearOptions *)*ppOptions)->
                dfRadius = (pszValue) ? CPLAtofM(pszValue) : -1.0;

            pszValue = CSLFetchNameValue( papszParms, "nodata" );
            ((GDALGridLinearOptions *)*ppOptions)->
                dfNoDataValue = (pszValue) ? CPLAtofM(pszValue) : 0.0;
            break;
   }

    CSLDestroy( papszParms );
    return CE_None;
}
