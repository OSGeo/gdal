/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer, using a quadtree
 *           for inverse
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#include "gdalgeoloc.h"
#include "gdalgeolocquadtree.h"

#include "cpl_quad_tree.h"

#include "ogr_geometry.h"

#include <algorithm>
#include <cstddef>
#include <limits>

/************************************************************************/
/*               GDALGeoLocQuadTreeGetFeatureCorners()                  */
/************************************************************************/

static bool GDALGeoLocQuadTreeGetFeatureCorners(const GDALGeoLocTransformInfo *psTransform,
                                                size_t nIdx,
                                                double& x0,
                                                double& y0,
                                                double& x1,
                                                double& y1,
                                                double& x2,
                                                double& y2,
                                                double& x3,
                                                double& y3)
{
    const size_t nExtendedWidth = psTransform->nGeoLocXSize +
                        ( psTransform->bOriginIsTopLeftCorner ? 0 : 1 );
    int nX = static_cast<int>(nIdx % nExtendedWidth);
    int nY = static_cast<int>(nIdx / nExtendedWidth);

    if( !psTransform->bOriginIsTopLeftCorner )
    {
        nX --;
        nY --;
    }

    return GDALGeoLocExtractSquare(psTransform,
                                   static_cast<int>(nX),
                                   static_cast<int>(nY),
                                   x0, y0,
                                   x1, y1,
                                   x2, y2,
                                   x3, y3);
}

/************************************************************************/
/*               GDALGeoLocQuadTreeGetFeatureBounds()                   */
/************************************************************************/

constexpr size_t BIT_IDX_RANGE_180 = 8 * sizeof(size_t) - 1;
constexpr size_t BIT_IDX_RANGE_180_SET = static_cast<size_t>(1) << BIT_IDX_RANGE_180;

// Callback used by quadtree to retrieve the bounding box, in georeferenced space,
// of a cell of the geolocation array.
static void GDALGeoLocQuadTreeGetFeatureBounds(const void* hFeature, void* pUserData, CPLRectObj* pBounds)
{
    const GDALGeoLocTransformInfo *psTransform =
        static_cast<const GDALGeoLocTransformInfo *>(pUserData);
    size_t nIdx = reinterpret_cast<size_t>(hFeature);
    // Most significant bit set means that geometries crossing the antimeridian
    // should have their longitudes lower or greater than 180 deg.
    const bool bXRefAt180 = (nIdx >> BIT_IDX_RANGE_180) != 0;
    // Clear that bit.
    nIdx &= ~BIT_IDX_RANGE_180_SET;

    double x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    GDALGeoLocQuadTreeGetFeatureCorners(psTransform, nIdx,
                                        x0, y0, x1, y1, x2, y2, x3, y3);

    if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
        std::fabs(x0) > 170 &&
        std::fabs(x1) > 170 &&
        std::fabs(x2) > 170 &&
        std::fabs(x3) > 170 &&
        (std::fabs(x1-x0) > 180 ||
         std::fabs(x2-x0) > 180 ||
         std::fabs(x3-x0) > 180) )
    {
        const double dfXRef = bXRefAt180 ? 180 : -180;
        x0 = ShiftGeoX(psTransform, dfXRef, x0);
        x1 = ShiftGeoX(psTransform, dfXRef, x1);
        x2 = ShiftGeoX(psTransform, dfXRef, x2);
        x3 = ShiftGeoX(psTransform, dfXRef, x3);
    }
    pBounds->minx = std::min(std::min(x0, x1), std::min(x2, x3));
    pBounds->miny = std::min(std::min(y0, y1), std::min(y2, y3));
    pBounds->maxx = std::max(std::max(x0, x1), std::max(x2, x3));
    pBounds->maxy = std::max(std::max(y0, y1), std::max(y2, y3));
}

/************************************************************************/
/*                      GDALGeoLocBuildQuadTree()                       */
/************************************************************************/

bool GDALGeoLocBuildQuadTree( GDALGeoLocTransformInfo *psTransform )
{
    // For the pixel-center convention, insert a "virtual" row and column
    // at top and left of the geoloc array.
    const int nExtraPixel = psTransform->bOriginIsTopLeftCorner ? 0 : 1;

    if( psTransform->nGeoLocXSize > INT_MAX - nExtraPixel ||
        psTransform->nGeoLocYSize > INT_MAX - nExtraPixel ||
        // The >> 1 shift is because we need to reserve the most-significant-bit
        // for the second 'version' of anti-meridian crossing quadrilaterals.
        // See below
        static_cast<size_t>(psTransform->nGeoLocXSize + nExtraPixel) > (std::numeric_limits<size_t>::max() >> 1) /
            static_cast<size_t>(psTransform->nGeoLocYSize + nExtraPixel) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big geolocation array");
        return false;
    }

    const int nExtendedWidth = psTransform->nGeoLocXSize + nExtraPixel;
    const int nExtendedHeight = psTransform->nGeoLocYSize + nExtraPixel;
    const size_t nExtendedXYCount = static_cast<size_t>(nExtendedWidth) * nExtendedHeight;

    CPLDebug("GEOLOC", "Start quadtree construction");

    CPLRectObj globalBounds;
    globalBounds.minx = psTransform->dfMinX;
    globalBounds.miny = psTransform->dfMinY;
    globalBounds.maxx = psTransform->dfMaxX;
    globalBounds.maxy = psTransform->dfMaxY;
    psTransform->hQuadTree = CPLQuadTreeCreateEx(&globalBounds,
                                                 GDALGeoLocQuadTreeGetFeatureBounds,
                                                 psTransform);

    CPLQuadTreeForceUseOfSubNodes(psTransform->hQuadTree);

    for( size_t i = 0; i < nExtendedXYCount; i++ )
    {
        double x0, y0, x1, y1, x2, y2, x3, y3;
        if (!GDALGeoLocQuadTreeGetFeatureCorners(psTransform, i,
                                            x0, y0, x1, y1, x2, y2, x3, y3) )
        {
            continue;
        }

        // Skip too large geometries (typically at very high latitudes)
        // that would fill too many nodes in the quadtree
        if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
            (std::fabs(x0) > 170 ||
             std::fabs(x1) > 170 ||
             std::fabs(x2) > 170 ||
             std::fabs(x3) > 170) &&
            (std::fabs(x1-x0) > 180 ||
             std::fabs(x2-x0) > 180 ||
             std::fabs(x3-x0) > 180) &&
            !(std::fabs(x0) > 170 &&
              std::fabs(x1) > 170 &&
              std::fabs(x2) > 170 &&
              std::fabs(x3) > 170) )
        {
            continue;
        }

        CPLQuadTreeInsert(psTransform->hQuadTree,
                          reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        // For a geometry crossing the antimeridian, we've insert before
        // the "version" around -180 deg. Insert its corresponding version around
        // +180 deg.
        if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
                    std::fabs(x0) > 170 &&
                    std::fabs(x1) > 170 &&
                    std::fabs(x2) > 170 &&
                    std::fabs(x3) > 170 &&
                    (std::fabs(x1-x0) > 180 ||
                     std::fabs(x2-x0) > 180 ||
                     std::fabs(x3-x0) > 180) )
        {
            CPLQuadTreeInsert(psTransform->hQuadTree,
                              reinterpret_cast<void*>(static_cast<uintptr_t>(i | BIT_IDX_RANGE_180_SET)));
        }
    }

    CPLDebug("GEOLOC", "End of quadtree construction");

#ifdef DEBUG_GEOLOC
    int nFeatureCount = 0;
    int nNodeCount = 0;
    int nMaxDepth = 0;
    int nMaxBucketCapacity = 0;
    CPLQuadTreeGetStats(psTransform->hQuadTree,
                        &nFeatureCount, &nNodeCount, &nMaxDepth, &nMaxBucketCapacity);
    CPLDebug("GEOLOC", "Quadtree stats:");
    CPLDebug("GEOLOC", "  nFeatureCount = %d", nFeatureCount);
    CPLDebug("GEOLOC", "  nNodeCount = %d", nNodeCount);
    CPLDebug("GEOLOC", "  nMaxDepth = %d", nMaxDepth);
    CPLDebug("GEOLOC", "  nMaxBucketCapacity = %d", nMaxBucketCapacity);
#endif

    return true;
}

/************************************************************************/
/*                  GDALGeoLocInverseTransformQuadtree()                */
/************************************************************************/

void GDALGeoLocInverseTransformQuadtree(
                    const GDALGeoLocTransformInfo *psTransform,
                    int nPointCount,
                    double *padfX,
                    double *padfY,
                    int *panSuccess )
{
    // Keep those objects in this outer scope, so they are re-used, to
    // save memory allocations.
    OGRPoint oPoint;
    OGRLinearRing oRing;
    oRing.setNumPoints(5);

    const double dfGeorefConventionOffset = psTransform->bOriginIsTopLeftCorner ? 0 : 0.5;

    for( int i = 0; i < nPointCount; i++ )
    {
        if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
        {
            panSuccess[i] = FALSE;
            continue;
        }

        if( psTransform->bSwapXY )
        {
            std::swap(padfX[i], padfY[i]);
        }

        const double dfGeoX = padfX[i];
        const double dfGeoY = padfY[i];

        bool bDone = false;

        CPLRectObj aoi;
        aoi.minx = dfGeoX;
        aoi.maxx = dfGeoX;
        aoi.miny = dfGeoY;
        aoi.maxy = dfGeoY;
        int nFeatureCount = 0;
        void** pahFeatures = CPLQuadTreeSearch(psTransform->hQuadTree,
                                               &aoi,
                                               &nFeatureCount);
        if( nFeatureCount != 0 )
        {
            oPoint.setX(dfGeoX);
            oPoint.setY(dfGeoY);
            for( int iFeat = 0; iFeat < nFeatureCount; iFeat++ )
            {
                size_t nIdx = reinterpret_cast<size_t>(pahFeatures[iFeat]);
                const bool bXRefAt180 = (nIdx >> BIT_IDX_RANGE_180) != 0;
                // Clear that bit.
                nIdx &= ~BIT_IDX_RANGE_180_SET;

                double x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
                GDALGeoLocQuadTreeGetFeatureCorners(psTransform, nIdx,
                                                    x0, y0, x2, y2, x1, y1, x3, y3);

                if( psTransform->bGeographicSRSWithMinus180Plus180LongRange &&
                    std::fabs(x0) > 170 &&
                    std::fabs(x1) > 170 &&
                    std::fabs(x2) > 170 &&
                    std::fabs(x3) > 170 &&
                    (std::fabs(x1-x0) > 180 ||
                     std::fabs(x2-x0) > 180 ||
                     std::fabs(x3-x0) > 180) )
                {
                    const double dfXRef = bXRefAt180 ? 180 : -180;
                    x0 = ShiftGeoX(psTransform, dfXRef, x0);
                    x1 = ShiftGeoX(psTransform, dfXRef, x1);
                    x2 = ShiftGeoX(psTransform, dfXRef, x2);
                    x3 = ShiftGeoX(psTransform, dfXRef, x3);
                }

                oRing.setPoint(0, x0, y0);
                oRing.setPoint(1, x2, y2);
                oRing.setPoint(2, x3, y3);
                oRing.setPoint(3, x1, y1);
                oRing.setPoint(4, x0, y0);

                if( oRing.isPointInRing( &oPoint ) ||
                    oRing.isPointOnRingBoundary( &oPoint ) )
                {
                    const size_t nExtendedWidth = psTransform->nGeoLocXSize +
                                ( psTransform->bOriginIsTopLeftCorner ? 0 : 1 );
                    double dfX = static_cast<double>(nIdx % nExtendedWidth);
                    // store the result as int, and then cast to double, to
                    // avoid Coverity Scan warning about UNINTENDED_INTEGER_DIVISION
                    const size_t nY = nIdx / nExtendedWidth;
                    double dfY = static_cast<double>(nY);
                    if( !psTransform->bOriginIsTopLeftCorner )
                    {
                        dfX -= 1.0;
                        dfY -= 1.0;
                    }
                    GDALInverseBilinearInterpolation(dfGeoX, dfGeoY,
                                                     x0, y0,
                                                     x1, y1,
                                                     x2, y2,
                                                     x3, y3,
                                                     dfX, dfY);

                    dfX = (dfX + dfGeorefConventionOffset) *
                        psTransform->dfPIXEL_STEP + psTransform->dfPIXEL_OFFSET;
                    dfY = (dfY + dfGeorefConventionOffset) *
                        psTransform->dfLINE_STEP + psTransform->dfLINE_OFFSET;

                    bDone = true;
                    panSuccess[i] = TRUE;
                    padfX[i] = dfX;
                    padfY[i] = dfY;
                    break;
                }
            }
        }
        CPLFree(pahFeatures);

        if( !bDone )
        {
            panSuccess[i] = FALSE;
            padfX[i] = HUGE_VAL;
            padfY[i] = HUGE_VAL;
        }
    }
}
