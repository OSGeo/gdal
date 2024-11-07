/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Raster Interpolation
 * Purpose:  Interpolation algorithms with cache
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Javier Jimenez Shaw
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRESAMPLINGKERNELS_H_INCLUDED
#define GDALRESAMPLINGKERNELS_H_INCLUDED

#include <cmath>

/*! @cond Doxygen_Suppress */

static inline double CubicKernel(double dfX)
{
    // http://en.wikipedia.org/wiki/Bicubic_interpolation#Bicubic_convolution_algorithm
    // W(x) formula with a = -0.5 (cubic hermite spline )
    // or
    // https://www.cs.utexas.edu/~fussell/courses/cs384g-fall2013/lectures/mitchell/Mitchell.pdf
    // k(x) (formula 8) with (B,C)=(0,0.5) the Catmull-Rom spline
    double dfAbsX = fabs(dfX);
    if (dfAbsX <= 1.0)
    {
        double dfX2 = dfX * dfX;
        return dfX2 * (1.5 * dfAbsX - 2.5) + 1;
    }
    else if (dfAbsX <= 2.0)
    {
        double dfX2 = dfX * dfX;
        return dfX2 * (-0.5 * dfAbsX + 2.5) - 4 * dfAbsX + 2;
    }
    else
        return 0.0;
}

static inline double CubicSplineKernel(double dfVal)
{
    if (dfVal > 2.0)
        return 0.0;

    const double xm1 = dfVal - 1.0;
    const double xp1 = dfVal + 1.0;
    const double xp2 = dfVal + 2.0;

    const double a = xp2 <= 0.0 ? 0.0 : xp2 * xp2 * xp2;
    const double b = xp1 <= 0.0 ? 0.0 : xp1 * xp1 * xp1;
    const double c = dfVal <= 0.0 ? 0.0 : dfVal * dfVal * dfVal;
    const double d = xm1 <= 0.0 ? 0.0 : xm1 * xm1 * xm1;

    return 0.16666666666666666667 * (a - (4.0 * b) + (6.0 * c) - (4.0 * d));
}

/*! @endcond */

#endif /* ndef GDALRESAMPLINGKERNELS_H_INCLUDED */
