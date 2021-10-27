/******************************************************************************
 *
 * Project:  GDAL Warp API
 * Purpose:  Implemenentation of 2D Thin Plate Spline transformer.
 * Author:   VIZRT Development Team.
 *
 * This code was provided by Gilad Ronnen (gro at visrt dot com) with
 * permission to reuse under the following license.
 *
 ******************************************************************************
 * Copyright (c) 2004, VIZRT Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "thinplatespline.h"
#include "gdallinearsystem.h"

#include <climits>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <limits>
#include <utility>

#include "cpl_error.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

//////////////////////////////////////////////////////////////////////////////
//// vizGeorefSpline2D
//////////////////////////////////////////////////////////////////////////////

// #define VIZ_GEOREF_SPLINE_DEBUG 0

bool VizGeorefSpline2D::grow_points()

{
    const int new_max = _max_nof_points * 2 + 2 + 3;

    double *new_x = static_cast<double *>(
        VSI_REALLOC_VERBOSE(x, sizeof(double) * new_max ));
    if( !new_x ) return false;
    x = new_x;
    double *new_y = static_cast<double *>(
        VSI_REALLOC_VERBOSE(y, sizeof(double) * new_max ));
    if( !new_y ) return false;
    y = new_y;
    double *new_u = static_cast<double *>(
        VSI_REALLOC_VERBOSE(u, sizeof(double) * new_max ));
    if( !new_u ) return false;
    u = new_u;
    int *new_unused = static_cast<int *>(
        VSI_REALLOC_VERBOSE(unused, sizeof(int) * new_max ));
    if( !new_unused ) return false;
    unused = new_unused;
    int *new_index = static_cast<int *>(
        VSI_REALLOC_VERBOSE(index, sizeof(int) * new_max));
    if( !new_index ) return false;
    index = new_index;
    for( int i = 0; i < _nof_vars; i++ )
    {
        double* rhs_i_new = static_cast<double *>(
            VSI_REALLOC_VERBOSE(rhs[i], sizeof(double) * new_max));
        if( !rhs_i_new ) return false;
        rhs[i] = rhs_i_new;
        double* coef_i_new = static_cast<double *>(
            VSI_REALLOC_VERBOSE(coef[i], sizeof(double) * new_max));
        if( !coef_i_new ) return false;
        coef[i] = coef_i_new;
        if( _max_nof_points == 0 )
        {
            memset(rhs[i], 0, 3 * sizeof(double));
            memset(coef[i], 0, 3 * sizeof(double));
        }
    }

    _max_nof_points = new_max - 3;
    return true;
}

bool VizGeorefSpline2D::add_point( const double Px, const double Py,
                                   const double *Pvars )
{
    type = VIZ_GEOREF_SPLINE_POINT_WAS_ADDED;
    int i;

    if( _nof_points == _max_nof_points )
    {
        if( !grow_points() )
            return false;
    }

    i = _nof_points;
    // A new point is added.
    x[i] = Px;
    y[i] = Py;
    for( int j = 0; j < _nof_vars; j++ )
        rhs[j][i+3] = Pvars[j];
    _nof_points++;
    return true;
}

#if 0
bool VizGeorefSpline2D::change_point( int index, double Px, double Py,
                                      double* Pvars )
{
    if( index < _nof_points )
    {
        int i = index;
        x[i] = Px;
        y[i] = Py;
        for( int j = 0; j < _nof_vars; j++ )
            rhs[j][i+3] = Pvars[j];
    }

    return true;
}

bool VizGeorefSpline2D::get_xy( int index, double& outX, double& outY )
{
    if( index < _nof_points )
    {
        ok = true;
        outX = x[index];
        outY = y[index];
        return true;
    }

    outX = 0.0;
    outY = 0.0;

    return false;
}

int VizGeorefSpline2D::delete_point( const double Px, const double Py )
{
    for( int i = 0; i < _nof_points; i++ )
    {
        if( ( fabs(Px - x[i]) <= _tx ) && ( fabs(Py - y[i]) <= _ty ) )
        {
            for( int j = i; j < _nof_points - 1; j++ )
            {
                x[j] = x[j+1];
                y[j] = y[j+1];
                for( int k = 0; k < _nof_vars; k++ )
                    rhs[k][j+3] = rhs[k][j+3+1];
            }
            _nof_points--;
            type = VIZ_GEOREF_SPLINE_POINT_WAS_DELETED;
            return 1;
        }
    }
    return 0;
}
#endif

template<typename T> static inline T SQ( const T &x )
{
    return x * x;
}

static inline double
VizGeorefSpline2DBase_func( const double x1, const double y1,
                            const double x2, const double y2 )
{
    const double dist = SQ( x2 - x1 )  + SQ( y2 - y1 );
    return dist != 0.0 ? dist * log( dist ) : 0.0;
}

#if defined(__GNUC__) && defined(__x86_64__)
/* Some versions of ICC fail to compile VizGeorefSpline2DBase_func4 (#6350) */
#if defined(__INTEL_COMPILER)
#if __INTEL_COMPILER >=1500
#define USE_OPTIMIZED_VizGeorefSpline2DBase_func4
#else
#if (__INTEL_COMPILER == 1200) || (__INTEL_COMPILER == 1210)
#define USE_OPTIMIZED_VizGeorefSpline2DBase_func4
#else
#undef USE_OPTIMIZED_VizGeorefSpline2DBase_func4
#endif
#endif
#else // defined(__INTEL_COMPILER)
#define USE_OPTIMIZED_VizGeorefSpline2DBase_func4
#endif // defined(__INTEL_COMPILER)
#endif

#if defined(USE_OPTIMIZED_VizGeorefSpline2DBase_func4)

/* Derived and adapted from code originating from: */

/* @(#)e_log.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* __ieee754_log(x)
 * Return the logarithm of x
 *
 * Method:
 *   1. Argument Reduction: find k and f such that
 *                      x = 2^k * (1+f),
 *         where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *   2. Approximation of log(1+f).
 *      Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *               = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *               = 2s + s*R
 *      We use a special Reme algorithm on [0,0.1716] to generate
 *      a polynomial of degree 14 to approximate R The maximum error
 *      of this polynomial approximation is bounded by 2**-58.45. In
 *      other words,
 *                      2      4      6      8      10      12      14
 *          R(z) ~ Lg1*s +Lg2*s +Lg3*s +Lg4*s +Lg5*s  +Lg6*s  +Lg7*s
 *      (the values of Lg1 to Lg7 are listed in the program)
 *      and
 *          |      2          14          |     -58.45
 *          | Lg1*s +...+Lg7*s    -  R(z) | <= 2
 *          |                             |
 *      Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 *      In order to guarantee error in log below 1ulp, we compute log
 *      by
 *              log(1+f) = f - s*(f - R)        (if f is not too large)
 *              log(1+f) = f - (hfsq - s*(hfsq+R)).     (better accuracy)
 *
 *      3. Finally,  log(x) = k*ln2 + log(1+f).
 *                          = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
 *         Here ln2 is split into two floating point number:
 *                      ln2_hi + ln2_lo,
 *         where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 *      log(x) is NaN with signal if x < 0 (including -INF) ;
 *      log(+INF) is +INF; log(0) is -INF with signal;
 *      log(NaN) is that NaN with no signal.
 *
 * Accuracy:
 *      according to an error analysis, the error is always less than
 *      1 ulp (unit in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

typedef double V2DF __attribute__ ((__vector_size__ (16)));
typedef union
{
    V2DF v2;
    double d[2];
} v2dfunion;

typedef union
{
    int i[2];
    long long li;
} i64union;

static const V2DF v2_ln2_div_2pow20 =
    { 6.93147180559945286e-01 / 1048576, 6.93147180559945286e-01 / 1048576 };
static const V2DF v2_Lg1 = {6.666666666666735130e-01, 6.666666666666735130e-01};
static const V2DF v2_Lg2 = {3.999999999940941908e-01, 3.999999999940941908e-01};
static const V2DF v2_Lg3 = {2.857142874366239149e-01, 2.857142874366239149e-01};
static const V2DF v2_Lg4 = {2.222219843214978396e-01, 2.222219843214978396e-01};
static const V2DF v2_Lg5 = {1.818357216161805012e-01, 1.818357216161805012e-01};
static const V2DF v2_Lg6 = {1.531383769920937332e-01, 1.531383769920937332e-01};
/*v2_Lg7 = {1.479819860511658591e-01, 1.479819860511658591e-01}, */
static const V2DF v2_one = { 1.0, 1.0 };
static const V2DF v2_const1023_mul_2pow20 =
    { 1023.0 * 1048576, 1023.0 * 1048576 };

#define GET_HIGH_WORD(hx,x) memcpy(&hx, reinterpret_cast<char*>(&x)+4,4)
#define SET_HIGH_WORD(x,hx) memcpy(reinterpret_cast<char*>(&x)+4, &hx,4)

#define MAKE_WIDE_CST(x) (((static_cast<long long>(x)) << 32) | (x))
constexpr long long cst_expmask = MAKE_WIDE_CST(0xfff00000);
constexpr long long cst_0x95f64 = MAKE_WIDE_CST(0x00095f64);
constexpr long long cst_0x100000 = MAKE_WIDE_CST(0x00100000);
constexpr long long cst_0x3ff00000 = MAKE_WIDE_CST(0x3ff00000);

// Modified version of __ieee754_log(), less precise than log() but a bit
// faster, and computing 4 log() at a time. Assumes that the values are > 0.
static void FastApproxLog4Val(v2dfunion* x)
{
    i64union hx[2] = {};
    i64union k[2] = {};
    i64union i[2] = {};
    GET_HIGH_WORD(hx[0].i[0], x[0].d[0]);
    GET_HIGH_WORD(hx[0].i[1], x[0].d[1]);

    // coverity[uninit_use]
    k[0].li = hx[0].li & cst_expmask;
    hx[0].li &= ~cst_expmask;
    i[0].li = (hx[0].li + cst_0x95f64) & cst_0x100000;
    hx[0].li |= i[0].li ^ cst_0x3ff00000;
    SET_HIGH_WORD(x[0].d[0], hx[0].i[0]);  // Normalize x or x/2.
    SET_HIGH_WORD(x[0].d[1], hx[0].i[1]);  // Normalize x or x/2.
    k[0].li += i[0].li;

    v2dfunion dk[2] = {};
    dk[0].d[0] = static_cast<double>(k[0].i[0]);
    dk[0].d[1] = static_cast<double>(k[0].i[1]);

    GET_HIGH_WORD(hx[1].i[0], x[1].d[0]);
    GET_HIGH_WORD(hx[1].i[1], x[1].d[1]);
    k[1].li = hx[1].li & cst_expmask;
    hx[1].li &= ~cst_expmask;
    i[1].li = (hx[1].li + cst_0x95f64) & cst_0x100000;
    hx[1].li |= i[1].li ^ cst_0x3ff00000;
    SET_HIGH_WORD(x[1].d[0], hx[1].i[0]);  // Normalize x or x/2.
    SET_HIGH_WORD(x[1].d[1], hx[1].i[1]);  // Normalize x or x/2.
    k[1].li += i[1].li;
    dk[1].d[0] = static_cast<double>(k[1].i[0]);
    dk[1].d[1] = static_cast<double>(k[1].i[1]);

    V2DF f[2] = {};
    f[0] = x[0].v2-v2_one;
    V2DF s[2] = {};
    s[0] = f[0]/(x[0].v2+v2_one);
    V2DF z[2] = {};
    z[0] = s[0]*s[0];
    V2DF w[2] = {};
    w[0] = z[0]*z[0];

    V2DF t1[2] = {};
    // coverity[ptr_arith]
    t1[0]= w[0]*(v2_Lg2+w[0]*(v2_Lg4+w[0]*v2_Lg6));

    V2DF t2[2] = {};
    // coverity[ptr_arith]
    t2[0]= z[0]*(v2_Lg1+w[0]*(v2_Lg3+w[0]*(v2_Lg5/*+w[0]*v2_Lg7*/)));

    V2DF R[2] = {};
    R[0] = t2[0]+t1[0];
    x[0].v2 =
        (dk[0].v2 - v2_const1023_mul_2pow20) * v2_ln2_div_2pow20 -
        (s[0] * (f[0] - R[0]) - f[0]);

    f[1] = x[1].v2-v2_one;
    s[1] = f[1]/(x[1].v2+v2_one);
    z[1] = s[1]*s[1];
    w[1] = z[1]*z[1];
    // coverity[ptr_arith]
    t1[1]= w[1]*(v2_Lg2+w[1]*(v2_Lg4+w[1]*v2_Lg6));
    // coverity[ptr_arith]
    t2[1]= z[1]*(v2_Lg1+w[1]*(v2_Lg3+w[1]*(v2_Lg5/*+w[1]*v2_Lg7*/)));
    R[1] = t2[1]+t1[1];
    x[1].v2 =
        (dk[1].v2 - v2_const1023_mul_2pow20) * v2_ln2_div_2pow20 -
        (s[1] * (f[1] - R[1]) - f[1]);
}

static CPL_INLINE void VizGeorefSpline2DBase_func4(
    double* res,
    const double* pxy,
    const double* xr, const double* yr )
{
    v2dfunion xv[2] = {};
    xv[0].d[0] = xr[0];
    xv[0].d[1] = xr[1];
    xv[1].d[0] = xr[2];
    xv[1].d[1] = xr[3];
    v2dfunion yv[2] = {};
    yv[0].d[0] = yr[0];
    yv[0].d[1] = yr[1];
    yv[1].d[0] = yr[2];
    yv[1].d[1] = yr[3];
    v2dfunion x1v;
    x1v.d[0] = pxy[0];
    x1v.d[1] = pxy[0];
    v2dfunion y1v;
    y1v.d[0] = pxy[1];
    y1v.d[1] = pxy[1];
    v2dfunion dist[2] = {};
    dist[0].v2 = SQ( xv[0].v2 - x1v.v2 ) + SQ( yv[0].v2 - y1v.v2 );
    dist[1].v2 = SQ( xv[1].v2 - x1v.v2 ) + SQ( yv[1].v2 - y1v.v2 );
    v2dfunion resv[2] = { dist[0], dist[1] };
    FastApproxLog4Val(dist);
    resv[0].v2 *= dist[0].v2;
    resv[1].v2 *= dist[1].v2;
    res[0] = resv[0].d[0];
    res[1] = resv[0].d[1];
    res[2] = resv[1].d[0];
    res[3] = resv[1].d[1];
}
#else // defined(USE_OPTIMIZED_VizGeorefSpline2DBase_func4)
static void VizGeorefSpline2DBase_func4( double* res,
                                         const double* pxy,
                                         const double* xr, const double* yr )
{
    double dist0  = SQ( xr[0] - pxy[0] ) + SQ( yr[0] - pxy[1] );
    res[0] = dist0 != 0.0 ? dist0 * log(dist0) : 0.0;
    double dist1  = SQ( xr[1] - pxy[0] ) + SQ( yr[1] - pxy[1] );
    res[1] = dist1 != 0.0 ? dist1 * log(dist1) : 0.0;
    double dist2  = SQ( xr[2] - pxy[0] ) + SQ( yr[2] - pxy[1] );
    res[2] = dist2 != 0.0 ? dist2 * log(dist2) : 0.0;
    double dist3  = SQ( xr[3] - pxy[0] ) + SQ( yr[3] - pxy[1] );
    res[3] = dist3 != 0.0 ? dist3 * log(dist3) : 0.0;
}
#endif // defined(USE_OPTIMIZED_VizGeorefSpline2DBase_func4)

int VizGeorefSpline2D::solve()
{
    // No points at all.
    if( _nof_points < 1 )
    {
        type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
        return 0;
    }

    // Only one point.
    if( _nof_points == 1 )
    {
        type = VIZ_GEOREF_SPLINE_ONE_POINT;
        return 1;
    }
    // Just 2 points - it is necessarily 1D case.
    if( _nof_points == 2 )
    {
        _dx = x[1] - x[0];
        _dy = y[1] - y[0];
        const double denom = _dx * _dx + _dy * _dy;
        if( denom == 0.0 )
            return 0;
        const double fact = 1.0 / denom;
        _dx *= fact;
        _dy *= fact;

        type = VIZ_GEOREF_SPLINE_TWO_POINTS;
        return 2;
    }

    // More than 2 points - first we have to check if it is 1D or 2D case

    double xmax = x[0];
    double xmin = x[0];
    double ymax = y[0];
    double ymin = y[0];
    double sumx = 0.0;
    double sumy = 0.0;
    double sumx2 = 0.0;
    double sumy2 = 0.0;
    double sumxy = 0.0;

    for( int p = 0; p < _nof_points; p++ )
    {
        const double xx = x[p];
        const double yy = y[p];

        xmax = std::max(xmax, xx);
        xmin = std::min(xmin, xx);
        ymax = std::max(ymax, yy);
        ymin = std::min(ymin, yy);

        sumx  += xx;
        sumx2 += xx * xx;
        sumy  += yy;
        sumy2 += yy * yy;
        sumxy += xx * yy;
    }
    const double delx = xmax - xmin;
    const double dely = ymax - ymin;

    const double SSxx = sumx2 - sumx * sumx / _nof_points;
    const double SSyy = sumy2 - sumy * sumy / _nof_points;
    const double SSxy = sumxy - sumx * sumy / _nof_points;

    if( SSxx * SSyy == 0.0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Degenerate system. Computation aborted.");
        return 0;
    }
    if( delx < 0.001 * dely || dely < 0.001 * delx ||
        fabs ( SSxy * SSxy / ( SSxx * SSyy ) ) > 0.99 )
    {
        type = VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL;

        _dx = _nof_points * sumx2 - sumx * sumx;
        _dy = _nof_points * sumy2 - sumy * sumy;
        const double fact = 1.0 / sqrt( _dx * _dx + _dy * _dy );
        _dx *= fact;
        _dy *= fact;

        for( int p = 0; p < _nof_points; p++ )
        {
            const double dxp = x[p] - x[0];
            const double dyp = y[p] - y[0];
            u[p] = _dx * dxp + _dy * dyp;
            unused[p] = 1;
        }

        for( int p = 0; p < _nof_points; p++ )
        {
            int min_index = -1;
            double min_u = 0.0;
            for( int p1 = 0; p1 < _nof_points; p1++ )
            {
                if( unused[p1] )
                {
                    if( min_index < 0 || u[p1] < min_u )
                    {
                        min_index = p1;
                        min_u = u[p1];
                    }
                }
            }
            index[p] = min_index;
            unused[min_index] = 0;
        }

        return 3;
    }

    type = VIZ_GEOREF_SPLINE_FULL;
    // Make the necessary memory allocations.

    _nof_eqs = _nof_points + 3;


    if( _nof_eqs > std::numeric_limits<int>::max() / _nof_eqs )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many coefficients. Computation aborted.");
        return 0;
    }

    GDALMatrix A(_nof_eqs, _nof_eqs);
    x_mean = 0;
    y_mean = 0;
    for( int c = 0; c < _nof_points; c++ )
    {
        x_mean += x[c];
        y_mean += y[c];
    }
    x_mean /= _nof_points;
    y_mean /= _nof_points;

    for( int c = 0; c < _nof_points; c++ )
    {
        x[c] -= x_mean;
        y[c] -= y_mean;
        A(0, c+3) = 1.0;
        A(1, c+3) = x[c];
        A(2, c+3) = y[c];

        A(c+3, 0) = 1.0;
        A(c+3, 1) = x[c];
        A(c+3, 2) = y[c];
    }

    for( int r = 0; r < _nof_points; r++ )
        for( int c = r; c < _nof_points; c++ )
        {
            A(r+3, c+3) = VizGeorefSpline2DBase_func( x[r], y[r], x[c], y[c] );
            if( r != c )
                A(c+3, r+3) = A(r+3, c+3);
        }

#if VIZ_GEOREF_SPLINE_DEBUG

    for( r = 0; r < _nof_eqs; r++ )
    {
        for( c = 0; c < _nof_eqs; c++ )
            fprintf(stderr, "%f", A(r, c));/*ok*/
        fprintf(stderr, "\n");/*ok*/
    }

#endif

    GDALMatrix RHS(_nof_eqs, _nof_vars);
    for( int iRHS = 0; iRHS < _nof_vars; iRHS++ )
        for( int iRow = 0; iRow < _nof_eqs; iRow++ )
            RHS(iRow, iRHS) = rhs[iRHS][iRow];

    GDALMatrix Coef(_nof_eqs, _nof_vars);

    if( !GDALLinearSystemSolve(A, RHS, Coef ) )
    {
        return 0;
    }

    for( int iRHS = 0; iRHS < _nof_vars; iRHS++ )
        for( int iRow = 0; iRow < _nof_eqs; iRow++ )
            coef[iRHS][iRow] = Coef(iRow, iRHS);

    return 4;
}

int VizGeorefSpline2D::get_point( const double Px, const double Py,
                                  double *vars )
{
    switch( type )
    {
    case VIZ_GEOREF_SPLINE_ZERO_POINTS:
    {
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = 0.0;
        break;
    }
    case VIZ_GEOREF_SPLINE_ONE_POINT:
    {
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = rhs[v][3];
        break;
    }
    case VIZ_GEOREF_SPLINE_TWO_POINTS:
    {
        const double fact = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = ( 1 - fact ) * rhs[v][3] + fact * rhs[v][4];
        break;
    }
    case VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL:
    {
        int leftP = 0;
        int rightP = 0;
        const double Pu = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
        if( Pu <= u[index[0]] )
        {
            leftP = index[0];
            rightP = index[1];
        }
        else if( Pu >= u[index[_nof_points-1]] )
        {
            leftP = index[_nof_points-2];
            rightP = index[_nof_points-1];
        }
        else
        {
            for( int r = 1; r < _nof_points; r++ )
            {
                leftP = index[r-1];
                rightP = index[r];
                if( Pu >= u[leftP] && Pu <= u[rightP] )
                    break;  // Found.
            }
        }

        const double fact = ( Pu - u[leftP] ) / ( u[rightP] - u[leftP] );
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = ( 1.0 - fact ) * rhs[v][leftP+3] +
                fact * rhs[v][rightP+3];
        break;
    }
    case VIZ_GEOREF_SPLINE_FULL:
    {
        const double Pxy[2] = { Px - x_mean, Py -y_mean };
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = coef[v][0] + coef[v][1] * Pxy[0] + coef[v][2] * Pxy[1];

        int r = 0;  // Used after for.
        for( ; r < (_nof_points & (~3)); r+=4 )
        {
            double dfTmp[4] = {};
            VizGeorefSpline2DBase_func4( dfTmp, Pxy, &x[r], &y[r] );
            for( int v = 0; v < _nof_vars; v++ )
                vars[v] += coef[v][r+3] * dfTmp[0] +
                        coef[v][r+3+1] * dfTmp[1] +
                        coef[v][r+3+2] * dfTmp[2] +
                        coef[v][r+3+3] * dfTmp[3];
        }
        for( ; r < _nof_points; r++ )
        {
            const double tmp = VizGeorefSpline2DBase_func( Pxy[0], Pxy[1], x[r], y[r] );
            for( int v= 0; v < _nof_vars; v++ )
                vars[v] += coef[v][r+3] * tmp;
        }
        break;
    }
    case VIZ_GEOREF_SPLINE_POINT_WAS_ADDED:
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A point was added after the last solve."
                 " NO interpolation - return values are zero");
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = 0.0;
        return 0;
    }
    case VIZ_GEOREF_SPLINE_POINT_WAS_DELETED:
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A point was deleted after the last solve."
                 " NO interpolation - return values are zero");
        for( int v = 0; v < _nof_vars; v++ )
            vars[v] = 0.0;
        return 0;
    }
    default:
    {
        return 0;
    }
    }
    return 1;
}

/*! @endcond */
