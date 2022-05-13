/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Vector polygon rasterization code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_alg_priv.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "gdal_alg.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                       dllImageFilledPolygon()                        */
/*                                                                      */
/*      Perform scanline conversion of the passed multi-ring            */
/*      polygon.  Note the polygon does not need to be explicitly       */
/*      closed.  The scanline function will be called with              */
/*      horizontal scanline chunks which may not be entirely            */
/*      contained within the valid raster area (in the X                */
/*      direction).                                                     */
/*                                                                      */
/*      NEW: Nodes' coordinate are kept as double  in order             */
/*      to compute accurately the intersections with the lines          */
/*                                                                      */
/*      A pixel is considered inside a polygon if its center            */
/*      falls inside the polygon. This is robust unless                 */
/*      the nodes are placed in the center of the pixels in which       */
/*      case, due to numerical inaccuracies, it's hard to predict       */
/*      if the pixel will be considered inside or outside the shape.    */
/************************************************************************/

/*
 * NOTE: This code was originally adapted from the gdImageFilledPolygon()
 * function in libgd.
 *
 * http://www.boutell.com/gd/
 *
 * It was later adapted for direct inclusion in GDAL and relicensed under
 * the GDAL MIT license (pulled from the OpenEV distribution).
 */

void GDALdllImageFilledPolygon(int nRasterXSize, int nRasterYSize,
                               int nPartCount, const int *panPartSize,
                               const double *padfX, const double *padfY,
                               const double *dfVariant,
                               llScanlineFunc pfnScanlineFunc, void *pCBData )
{
    if( !nPartCount )
    {
        return;
    }

    int n = 0;
    for( int part = 0; part < nPartCount; part++ )
        n += panPartSize[part];

    std::vector<int> polyInts(n);

    double dminy = padfY[0];
    double dmaxy = padfY[0];
    for( int i = 1; i < n; i++ )
    {
        if( padfY[i] < dminy )
        {
            dminy = padfY[i];
        }
        if( padfY[i] > dmaxy )
        {
            dmaxy = padfY[i];
        }
    }
    int miny = static_cast<int>(dminy);
    int maxy = static_cast<int>(dmaxy);

    if( miny < 0 )
        miny = 0;
    if( maxy >= nRasterYSize )
        maxy = nRasterYSize - 1;

    int minx = 0;
    const int maxx = nRasterXSize - 1;

    // Fix in 1.3: count a vertex only once.
    for( int y = miny; y <= maxy; y++ )
    {
        int partoffset = 0;

        const double dy = y + 0.5;  // Center height of line.

        int part = 0;
        int ints = 0;

        for( int i = 0; i < n; i++ )
        {
            if( i == partoffset + panPartSize[part] )
            {
                partoffset += panPartSize[part];
                part++;
            }

            int ind1 = 0;
            int ind2 = 0;
            if( i == partoffset )
            {
                ind1 = partoffset + panPartSize[part] - 1;
                ind2 = partoffset;
            }
            else
            {
                ind1 = i-1;
                ind2 = i;
            }

            double dy1 = padfY[ind1];
            double dy2 = padfY[ind2];

            if( (dy1 < dy && dy2 < dy) || (dy1 > dy && dy2 > dy) )
                continue;

            double dx1 = 0.0;
            double dx2 = 0.0;
            if( dy1 < dy2 )
            {
                dx1 = padfX[ind1];
                dx2 = padfX[ind2];
            }
            else if( dy1 > dy2 )
            {
                std::swap(dy1, dy2);
                dx2 = padfX[ind1];
                dx1 = padfX[ind2];
            }
            else // if( fabs(dy1-dy2) < 1.e-6 )
            {

                // AE: DO NOT skip bottom horizontal segments
                // -Fill them separately-
                // They are not taken into account twice.
                if( padfX[ind1] > padfX[ind2] )
                {
                    const int horizontal_x1 =
                        static_cast<int>(floor(padfX[ind2] + 0.5));
                    const int horizontal_x2 =
                        static_cast<int>(floor(padfX[ind1] + 0.5));

                    if( (horizontal_x1 >  maxx) ||  (horizontal_x2 <= minx) )
                        continue;

                    // Fill the horizontal segment (separately from the rest).
                    pfnScanlineFunc( pCBData, y, horizontal_x1,
                                     horizontal_x2 - 1,
                                     (dfVariant == nullptr)?0:dfVariant[0] );
                }
                // else: Skip top horizontal segments.
                // They are already filled in the regular loop.
                continue;
            }

            if( dy < dy2 && dy >= dy1 )
            {
                const double intersect = (dy-dy1) * (dx2-dx1) / (dy2-dy1) + dx1;

                polyInts[ints++] = static_cast<int>(floor(intersect + 0.5));
            }
        }

        std::sort (polyInts.begin(), polyInts.begin()+ints);

        for( int i = 0; i + 1 < ints; i += 2 )
        {
            if( polyInts[i] <= maxx && polyInts[i+1] > minx )
            {
                pfnScanlineFunc(pCBData, y, polyInts[i], polyInts[i+1] - 1,
                                dfVariant == nullptr ? 0 : dfVariant[0]);
            }
        }
    }
}

/************************************************************************/
/*                         GDALdllImagePoint()                          */
/************************************************************************/

void GDALdllImagePoint( int nRasterXSize, int nRasterYSize,
                        int nPartCount,
                        const int * /*panPartSize*/,
                        const double *padfX, const double *padfY,
                        const double *padfVariant,
                        llPointFunc pfnPointFunc, void *pCBData )
{
    for( int i = 0; i < nPartCount; i++ )
    {
        const int nX = static_cast<int>(floor( padfX[i] ));
        const int nY = static_cast<int>(floor( padfY[i] ));
        double dfVariant = 0.0;
        if( padfVariant != nullptr )
            dfVariant = padfVariant[i];

        if( 0 <= nX && nX < nRasterXSize && 0 <= nY && nY < nRasterYSize )
            pfnPointFunc( pCBData, nY, nX, dfVariant );
    }
}

/************************************************************************/
/*                         GDALdllImageLine()                           */
/************************************************************************/

void GDALdllImageLine( int nRasterXSize, int nRasterYSize,
                       int nPartCount, const int *panPartSize,
                       const double *padfX, const double *padfY,
                       const double *padfVariant,
                       llPointFunc pfnPointFunc, void *pCBData )
{
    if( !nPartCount )
        return;

    for( int i = 0, n = 0; i < nPartCount; n += panPartSize[i++] )
    {
        for( int j = 1; j < panPartSize[i]; j++ )
        {
            int iX = static_cast<int>(floor( padfX[n + j - 1] ));
            int iY = static_cast<int>(floor( padfY[n + j - 1] ));

            const int iX1 = static_cast<int>(floor(padfX[n + j]));
            const int iY1 = static_cast<int>(floor(padfY[n + j]));

            double dfVariant = 0.0;
            double dfVariant1 = 0.0;
            if( padfVariant != nullptr &&
                static_cast<GDALRasterizeInfo *>(pCBData)->eBurnValueSource !=
                    GBV_UserBurnValue )
            {
                dfVariant = padfVariant[n + j - 1];
                dfVariant1 = padfVariant[n + j];
            }

            int nDeltaX = std::abs(iX1 - iX);
            int nDeltaY = std::abs(iY1 - iY);

            // Step direction depends on line direction.
            const int nXStep = ( iX > iX1 ) ? -1 : 1;
            const int nYStep = ( iY > iY1 ) ? -1 : 1;

            // Determine the line slope.
            if( nDeltaX >= nDeltaY )
            {
                const int nXError = nDeltaY << 1;
                const int nYError = nXError - (nDeltaX << 1);
                int nError = nXError - nDeltaX;
                // == 0 makes clang -fcatch-undefined-behavior -ftrapv happy,
                // but if it is == 0, dfDeltaVariant is not really used, so any
                // value is okay.
                const double dfDeltaVariant =
                    nDeltaX == 0
                    ? 0.0
                    : (dfVariant1 - dfVariant) / nDeltaX;

                // Do not burn the end point, unless we are in the last
                // segment. This is to avoid burning twice intermediate points,
                // which causes artifacts in Add mode
                if( j != panPartSize[i] - 1 )
                {
                    nDeltaX --;
                }

                while( nDeltaX-- >= 0 )
                {
                    if( 0 <= iX && iX < nRasterXSize
                        && 0 <= iY && iY < nRasterYSize )
                        pfnPointFunc( pCBData, iY, iX, dfVariant );

                    dfVariant += dfDeltaVariant;
                    iX += nXStep;
                    if( nError > 0 )
                    {
                        iY += nYStep;
                        nError += nYError;
                    }
                    else
                    {
                        nError += nXError;
                    }
                }
            }
            else
            {
                const int nXError = nDeltaX << 1;
                const int nYError = nXError - (nDeltaY << 1);
                int nError = nXError - nDeltaY;
                // == 0 makes clang -fcatch-undefined-behavior -ftrapv happy,
                // but if it is == 0, dfDeltaVariant is not really used, so any
                // value is okay.
                double dfDeltaVariant =
                    nDeltaY == 0
                    ? 0.0
                    : (dfVariant1 - dfVariant) / nDeltaY;

                // Do not burn the end point, unless we are in the last
                // segment. This is to avoid burning twice intermediate points,
                // which causes artifacts in Add mode
                if( j != panPartSize[i] - 1 )
                {
                    nDeltaY --;
                }

                while( nDeltaY-- >= 0 )
                {
                    if( 0 <= iX && iX < nRasterXSize
                        && 0 <= iY && iY < nRasterYSize )
                        pfnPointFunc( pCBData, iY, iX, dfVariant );

                    dfVariant += dfDeltaVariant;
                    iY += nYStep;
                    if( nError > 0 )
                    {
                        iX += nXStep;
                        nError += nYError;
                    }
                    else
                    {
                        nError += nXError;
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                     GDALdllImageLineAllTouched()                     */
/*                                                                      */
/*      This alternate line drawing algorithm attempts to ensure        */
/*      that every pixel touched at all by the line will get set.       */
/*      @param padfVariant should contain the values that are to be     */
/*      added to the burn value.  The values along the line between the */
/*      points will be linearly interpolated. These values are used     */
/*      only if pCBData->eBurnValueSource is set to something other     */
/*      than GBV_UserBurnValue. If NULL is passed, a monotonous line    */
/*      will be drawn with the burn value.                              */
/************************************************************************/

void
GDALdllImageLineAllTouched( int nRasterXSize, int nRasterYSize,
                            int nPartCount, const int *panPartSize,
                            const double *padfX, const double *padfY,
                            const double *padfVariant,
                            llPointFunc pfnPointFunc, void *pCBData,
                            int bAvoidBurningSamePoints )

{
    if( !nPartCount )
        return;

    for( int i = 0, n = 0; i < nPartCount; n += panPartSize[i++] )
    {
        std::set<std::pair<int,int>> lastBurntPoints;
        std::set<std::pair<int,int>> newBurntPoints;

        for( int j = 1; j < panPartSize[i]; j++ )
        {
            lastBurntPoints = std::move(newBurntPoints);
            newBurntPoints.clear();

            double dfX = padfX[n + j - 1];
            double dfY = padfY[n + j - 1];

            double dfXEnd = padfX[n + j];
            double dfYEnd = padfY[n + j];

            double dfVariant = 0.0;
            double dfVariantEnd = 0.0;
            if( padfVariant != nullptr &&
                static_cast<GDALRasterizeInfo *>(pCBData)->eBurnValueSource !=
                    GBV_UserBurnValue )
            {
                dfVariant = padfVariant[n + j - 1];
                dfVariantEnd = padfVariant[n + j];
            }

            // Skip segments that are off the target region.
            if( (dfY < 0.0 && dfYEnd < 0.0)
                || (dfY > nRasterYSize && dfYEnd > nRasterYSize)
                || (dfX < 0.0 && dfXEnd < 0.0)
                || (dfX > nRasterXSize && dfXEnd > nRasterXSize) )
                continue;

            // Swap if needed so we can proceed from left2right (X increasing)
            if( dfX > dfXEnd )
            {
                std::swap(dfX, dfXEnd);
                std::swap(dfY, dfYEnd );
                std::swap(dfVariant, dfVariantEnd);
            }

            // Special case for vertical lines.
            if( floor(dfX) == floor(dfXEnd) || fabs(dfX - dfXEnd) < .01 )
            {
                if( dfYEnd < dfY )
                {
                    std::swap(dfY, dfYEnd );
                    std::swap(dfVariant, dfVariantEnd);
                }

                const int iX = static_cast<int>(floor(dfXEnd));
                int iY = static_cast<int>(floor(dfY));
                int iYEnd = static_cast<int>(floor(dfYEnd));

                if( iX < 0 || iX >= nRasterXSize )
                    continue;

                double dfDeltaVariant = 0.0;
                if( dfYEnd - dfY > 0.0 )
                    dfDeltaVariant =
                        ( dfVariantEnd - dfVariant ) /
                        ( dfYEnd - dfY );  // Per unit change in iY.

                // Clip to the borders of the target region.
                if( iY < 0 )
                    iY = 0;
                if( iYEnd >= nRasterYSize )
                    iYEnd = nRasterYSize - 1;
                dfVariant += dfDeltaVariant * (iY - dfY);

                if( padfVariant == nullptr )
                {
                    for( ; iY <= iYEnd; iY++ )
                    {
                        if( bAvoidBurningSamePoints )
                        {
                            auto yx = std::pair<int,int>(iY,iX);
                            if( lastBurntPoints.find( yx ) != lastBurntPoints.end() )
                            {
                                continue;
                            }
                            newBurntPoints.insert(yx);
                        }
                        pfnPointFunc( pCBData, iY, iX, 0.0 );
                    }
                }
                else
                {
                    for( ; iY <= iYEnd; iY++, dfVariant +=  dfDeltaVariant )
                    {
                        if( bAvoidBurningSamePoints )
                        {
                            auto yx = std::pair<int,int>(iY,iX);
                            if( lastBurntPoints.find( yx ) != lastBurntPoints.end() )
                            {
                                continue;
                            }
                            newBurntPoints.insert(yx);
                        }
                        pfnPointFunc( pCBData, iY, iX, dfVariant );
                    }
                }

                continue;  // Next segment.
            }

            const double dfDeltaVariant =
                ( dfVariantEnd - dfVariant ) /
                ( dfXEnd - dfX );  // Per unit change in iX.

            // Special case for horizontal lines.
            if( floor(dfY) == floor(dfYEnd) || fabs(dfY - dfYEnd) < .01 )
            {
                if( dfXEnd < dfX )
                {
                    std::swap(dfX, dfXEnd);
                    std::swap(dfVariant, dfVariantEnd);
                }

                int iX = static_cast<int>(floor(dfX));
                const int iY = static_cast<int>(floor(dfY));
                int iXEnd = static_cast<int>(floor(dfXEnd));

                if( iY < 0 || iY >= nRasterYSize )
                    continue;

                // Clip to the borders of the target region.
                if( iX < 0 )
                    iX = 0;
                if( iXEnd >= nRasterXSize )
                    iXEnd = nRasterXSize - 1;
                dfVariant += dfDeltaVariant * (iX - dfX);

                if( padfVariant == nullptr )
                {
                    for( ; iX <= iXEnd; iX++ )
                    {
                        if( bAvoidBurningSamePoints )
                        {
                            auto yx = std::pair<int,int>(iY,iX);
                            if( lastBurntPoints.find( yx ) != lastBurntPoints.end() )
                            {
                                continue;
                            }
                            newBurntPoints.insert(yx);
                        }
                        pfnPointFunc( pCBData, iY, iX, 0.0 );
                    }
                }
                else
                {
                    for( ; iX <= iXEnd; iX++, dfVariant +=  dfDeltaVariant )
                    {
                        if( bAvoidBurningSamePoints )
                        {
                            auto yx = std::pair<int,int>(iY,iX);
                            if( lastBurntPoints.find( yx ) != lastBurntPoints.end() )
                            {
                                continue;
                            }
                            newBurntPoints.insert(yx);
                        }
                        pfnPointFunc( pCBData, iY, iX, dfVariant );
                    }
                }

                continue;  // Next segment.
            }

/* -------------------------------------------------------------------- */
/*      General case - left to right sloped.                            */
/* -------------------------------------------------------------------- */
            const double dfSlope = (dfYEnd - dfY) / (dfXEnd - dfX);

            // Clip segment in X.
            if( dfXEnd > nRasterXSize )
            {
                dfYEnd -= (dfXEnd - nRasterXSize) * dfSlope;
                dfXEnd = nRasterXSize;
            }
            if( dfX < 0.0 )
            {
                dfY += (0.0 - dfX) * dfSlope;
                dfVariant += dfDeltaVariant * (0.0 - dfX);
                dfX = 0.0;
            }

            // Clip segment in Y.
            double dfDiffX = 0.0;
            if( dfYEnd > dfY )
            {
                if( dfY < 0.0 )
                {
                    dfDiffX = (0.0 - dfY) / dfSlope;
                    dfX += dfDiffX;
                    dfVariant += dfDeltaVariant * dfDiffX;
                    dfY = 0.0;
                }
                if( dfYEnd >= nRasterYSize )
                {
                    dfXEnd += (dfYEnd - nRasterYSize) / dfSlope;
                    // dfYEnd is no longer used afterwards, but for
                    // consistency it should be:
                    // dfYEnd = nRasterXSize;
                }
            }
            else
            {
                if( dfY >= nRasterYSize )
                {
                  dfDiffX = (nRasterYSize - dfY) / dfSlope;
                    dfX += dfDiffX;
                    dfVariant += dfDeltaVariant * dfDiffX;
                    dfY = nRasterYSize;
                }
                if( dfYEnd < 0.0 )
                {
                    dfXEnd -= ( dfYEnd - 0 ) / dfSlope;
                    // dfYEnd is no longer used afterwards, but for
                    // consistency it should be:
                    // dfYEnd = 0.0;
                }
            }

            // Step from pixel to pixel.
            while( dfX >= 0.0 && dfX < dfXEnd )
            {
                const int iX = static_cast<int>(floor(dfX));
                const int iY = static_cast<int>(floor(dfY));

                // Burn in the current point.
                // We should be able to drop the Y check because we clipped
                // in Y, but there may be some error with all the small steps.
                if( iY >= 0 && iY < nRasterYSize )
                {
                    if( bAvoidBurningSamePoints )
                    {
                        auto yx = std::pair<int,int>(iY,iX);
                        if( lastBurntPoints.find( yx ) == lastBurntPoints.end() &&
                            newBurntPoints.find( yx ) == newBurntPoints.end() )
                        {
                            newBurntPoints.insert(yx);
                            pfnPointFunc( pCBData, iY, iX, dfVariant );
                        }
                    }
                    else
                    {
                        pfnPointFunc( pCBData, iY, iX, dfVariant );
                    }
                }

                double dfStepX = floor(dfX+1.0) - dfX;
                double dfStepY = dfStepX * dfSlope;

                // Step to right pixel without changing scanline?
                if( static_cast<int>(floor(dfY + dfStepY)) == iY )
                {
                    dfX += dfStepX;
                    dfY += dfStepY;
                    dfVariant += dfDeltaVariant * dfStepX;
                }
                else if( dfSlope < 0 )
                {
                    dfStepY = iY - dfY;
                    if( dfStepY > -0.000000001 )
                        dfStepY = -0.000000001;

                    dfStepX = dfStepY / dfSlope;
                    dfX += dfStepX;
                    dfY += dfStepY;
                    dfVariant += dfDeltaVariant * dfStepX;
                }
                else
                {
                    dfStepY = (iY + 1) - dfY;
                    if( dfStepY < 0.000000001 )
                        dfStepY = 0.000000001;

                    dfStepX = dfStepY / dfSlope;
                    dfX += dfStepX;
                    dfY += dfStepY;
                    dfVariant += dfDeltaVariant * dfStepX;
                }
            }  // Next step along segment.
        }  // Next segment.
    }  // Next part.
}
