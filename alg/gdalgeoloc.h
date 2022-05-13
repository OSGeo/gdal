/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer.
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

#ifndef GDALGEOLOC_H
#define GDALGEOLOC_H

#include "gdal_alg_priv.h"

/************************************************************************/
/*                           GDALGeoLoc                                 */
/************************************************************************/

/*! @cond Doxygen_Suppress */

template<class Accessors> struct GDALGeoLoc
{
    static bool LoadGeolocFinish( GDALGeoLocTransformInfo *psTransform );

    static bool GenerateBackMap( GDALGeoLocTransformInfo *psTransform );

    static bool PixelLineToXY(const GDALGeoLocTransformInfo *psTransform,
                                const int nGeoLocPixel,
                                const int nGeoLocLine,
                                double& dfX,
                                double& dfY);

    static bool PixelLineToXY(const GDALGeoLocTransformInfo *psTransform,
                                const double dfGeoLocPixel,
                                const double dfGeoLocLine,
                                double& dfX,
                                double& dfY);

    static bool ExtractSquare(const GDALGeoLocTransformInfo *psTransform,
                              int nX, int nY,
                              double& dfX_0_0, double& dfY_0_0,
                              double& dfX_1_0, double& dfY_1_0,
                              double& dfX_0_1, double& dfY_0_1,
                              double& dfX_1_1, double& dfY_1_1);

    static int Transform( void *pTransformArg,
                         int bDstToSrc,
                         int nPointCount,
                         double *padfX, double *padfY,
                         double * /* padfZ */,
                         int *panSuccess );
};
/*! @endcond */


bool GDALGeoLocExtractSquare(const GDALGeoLocTransformInfo *psTransform,
                             int nX, int nY,
                             double& dfX_0_0, double& dfY_0_0,
                             double& dfX_1_0, double& dfY_1_0,
                             double& dfX_0_1, double& dfY_0_1,
                             double& dfX_1_1, double& dfY_1_1);

void GDALInverseBilinearInterpolation(const double x,
                                      const double y,
                                      const double x0,
                                      const double y0,
                                      const double x1,
                                      const double y1,
                                      const double x2,
                                      const double y2,
                                      const double x3,
                                      const double y3,
                                      double& i,
                                      double& j);

/************************************************************************/
/*                           ShiftGeoX()                                */
/************************************************************************/

// Avoid discontinuity at anti-meridian when interpolating longitude
// dfXRef is a "reference" longitude, typically the one of 4 points to
// interpolate), towards which we apply a potential +/- 360 deg shift.
// This may result in a value slightly outside [-180,180]
static double ShiftGeoX(const GDALGeoLocTransformInfo *psTransform,
                        double dfXRef,
                        double dfX)
{
    if( !psTransform->bGeographicSRSWithMinus180Plus180LongRange )
        return dfX;
    // The threshold at 170 deg is a bit arbitrary. A smarter approach
    // would try to guess the "average" longitude step between 2 grid values
    // and use 180 - average_step * some_factor as the threshold.
    if( dfXRef < -170 && dfX > 170 )
        return dfX - 360;
    if( dfXRef > 170 && dfX < -170 )
        return dfX + 360;
    return dfX;
}

#endif
