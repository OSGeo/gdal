/******************************************************************************
 * $Id$
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

#ifdef HAVE_ARMADILLO
/* Include before #define A(r,c) because armadillo uses A in its include files */
#include "armadillo"
#endif

#include "thinplatespline.h"

#ifdef HAVE_FLOAT_H
#  include <float.h>
#elif defined(HAVE_VALUES_H)
#  include <values.h>
#endif

#ifndef FLT_MAX
#  define FLT_MAX 1e+37
#  define FLT_MIN 1e-37
#endif

VizGeorefSpline2D* viz_xy2llz;
VizGeorefSpline2D* viz_llz2xy;

/////////////////////////////////////////////////////////////////////////////////////
//// vizGeorefSpline2D
/////////////////////////////////////////////////////////////////////////////////////

#define A(r,c) _AA[ _nof_eqs * (r) + (c) ]
#define Ainv(r,c) _Ainv[ _nof_eqs * (r) + (c) ]


#define VIZ_GEOREF_SPLINE_DEBUG 0

static int matrixInvert( int N, double input[], double output[] );

void VizGeorefSpline2D::grow_points()

{
    int new_max = _max_nof_points*2 + 2 + 3;
    int i;
    
    if( _max_nof_points == 0 )
    {
        x = (double *) VSIMalloc( sizeof(double) * new_max );
        y = (double *) VSIMalloc( sizeof(double) * new_max );
        u = (double *) VSIMalloc( sizeof(double) * new_max );
        unused = (int *) VSIMalloc( sizeof(int) * new_max );
        index = (int *) VSIMalloc( sizeof(int) * new_max );
        for( i = 0; i < VIZGEOREF_MAX_VARS; i++ )
        {
            rhs[i] = (double *) VSICalloc( sizeof(double), new_max );
            coef[i] = (double *) VSICalloc( sizeof(double), new_max );
        }
    }
    else
    {
        x = (double *) VSIRealloc( x, sizeof(double) * new_max );
        y = (double *) VSIRealloc( y, sizeof(double) * new_max );
        u = (double *) VSIRealloc( u, sizeof(double) * new_max );
        unused = (int *) VSIRealloc( unused, sizeof(int) * new_max );
        index = (int *) VSIRealloc( index, sizeof(int) * new_max );
        for( i = 0; i < VIZGEOREF_MAX_VARS; i++ )
        {
            rhs[i] = (double *) 
                VSIRealloc( rhs[i], sizeof(double) * new_max );
            coef[i] = (double *) 
                VSIRealloc( coef[i], sizeof(double) * new_max );
        }
    }

    _max_nof_points = new_max - 3;
}

int VizGeorefSpline2D::add_point( const double Px, const double Py, const double *Pvars )
{
    type = VIZ_GEOREF_SPLINE_POINT_WAS_ADDED;
    int i;

    if( _nof_points == _max_nof_points )
        grow_points();

    i = _nof_points;
    //A new point is added
    x[i] = Px;
    y[i] = Py;
    for ( int j = 0; j < _nof_vars; j++ )
        rhs[j][i+3] = Pvars[j];
    _nof_points++;
    return 1;
}

bool VizGeorefSpline2D::change_point(int index, double Px, double Py, double* Pvars)
{
    if ( index < _nof_points )
    {
        int i = index;
        x[i] = Px;
        y[i] = Py;
        for ( int j = 0; j < _nof_vars; j++ )
            rhs[j][i+3] = Pvars[j];
    }

    return( true );
}

bool VizGeorefSpline2D::get_xy(int index, double& outX, double& outY)
{
    bool ok;

    if ( index < _nof_points )
    {
        ok = true;
        outX = x[index];
        outY = y[index];
    }
    else
    {
        ok = false;
        outX = outY = 0.0f;
    }

    return(ok);
}

int VizGeorefSpline2D::delete_point(const double Px, const double Py )
{
    for ( int i = 0; i < _nof_points; i++ )
    {
        if ( ( fabs(Px - x[i]) <= _tx ) && ( fabs(Py - y[i]) <= _ty ) )
        {
            for ( int j = i; j < _nof_points - 1; j++ )
            {
                x[j] = x[j+1];
                y[j] = y[j+1];
                for ( int k = 0; k < _nof_vars; k++ )
                    rhs[k][j+3] = rhs[k][j+3+1];
            }
            _nof_points--;
            type = VIZ_GEOREF_SPLINE_POINT_WAS_DELETED;
            return(1);
        }
    }
    return(0);
}

#define SQ(x) ((x)*(x))

static CPL_INLINE double VizGeorefSpline2DBase_func( const double x1, const double y1,
                          const double x2, const double y2 )
{
    double dist  = SQ( x2 - x1 )  + SQ( y2 - y1 );
    return dist ? dist * log( dist ) : 0.0;
}

#if defined(__GNUC__) && defined(__x86_64__)

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
 * Return the logrithm of x
 *
 * Method :                  
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

static const V2DF
v2_ln2_div_2pow20 = {6.93147180559945286e-01 / 1048576, 6.93147180559945286e-01 / 1048576},
v2_Lg1 = {6.666666666666735130e-01, 6.666666666666735130e-01},
v2_Lg2 = {3.999999999940941908e-01, 3.999999999940941908e-01}, 
v2_Lg3 = {2.857142874366239149e-01, 2.857142874366239149e-01},
v2_Lg4 = {2.222219843214978396e-01, 2.222219843214978396e-01},
v2_Lg5 = {1.818357216161805012e-01, 1.818357216161805012e-01},
v2_Lg6 = {1.531383769920937332e-01, 1.531383769920937332e-01},
v2_Lg7 = {1.479819860511658591e-01, 1.479819860511658591e-01},
v2_one = { 1.0, 1.0 },
v2_const1023_mul_2pow20 = { 1023.0 * 1048576, 1023.0 * 1048576};

#define GET_HIGH_WORD(hx,x) memcpy(&hx,((char*)&x+4),4)
#define SET_HIGH_WORD(x,hx) memcpy(((char*)&x+4),&hx,4)

#define MAKE_WIDE_CST(x) ((((long long)(x)) << 32) | (x))
static const long long cst_expmask = MAKE_WIDE_CST(0xfff00000);
static const long long cst_0x95f64 = MAKE_WIDE_CST(0x00095f64);
static const long long cst_0x100000 = MAKE_WIDE_CST(0x00100000);
static const long long cst_0x3ff00000 = MAKE_WIDE_CST(0x3ff00000);

/* Modified version of __ieee754_log(), less precise than log() but a bit */
/* faste, and computing 4 log() at a time. Assumes that the values are > 0 */
static void FastApproxLog4Val(v2dfunion* x)
{
    V2DF f[2],s[2],z[2],R[2],w[2],t1[2],t2[2];
    v2dfunion dk[2];
    i64union k[2], hx[2], i[2];

    GET_HIGH_WORD(hx[0].i[0],x[0].d[0]);
    GET_HIGH_WORD(hx[0].i[1],x[0].d[1]);
    k[0].li = hx[0].li & cst_expmask;
    hx[0].li &= ~cst_expmask;
    i[0].li = (hx[0].li + cst_0x95f64) & cst_0x100000;
    hx[0].li |= i[0].li ^ cst_0x3ff00000;
    SET_HIGH_WORD(x[0].d[0],hx[0].i[0]);     /* normalize x or x/2 */
    SET_HIGH_WORD(x[0].d[1],hx[0].i[1]);     /* normalize x or x/2 */
    k[0].li += i[0].li;
    dk[0].d[0] = (double)k[0].i[0];
    dk[0].d[1] = (double)k[0].i[1];

    GET_HIGH_WORD(hx[1].i[0],x[1].d[0]);
    GET_HIGH_WORD(hx[1].i[1],x[1].d[1]);
    k[1].li = hx[1].li & cst_expmask;
    hx[1].li &= ~cst_expmask;
    i[1].li = (hx[1].li + cst_0x95f64) & cst_0x100000;
    hx[1].li |= i[1].li ^ cst_0x3ff00000;
    SET_HIGH_WORD(x[1].d[0],hx[1].i[0]);     /* normalize x or x/2 */
    SET_HIGH_WORD(x[1].d[1],hx[1].i[1]);     /* normalize x or x/2 */
    k[1].li += i[1].li;
    dk[1].d[0] = (double)k[1].i[0];
    dk[1].d[1] = (double)k[1].i[1];

    f[0] = x[0].v2-v2_one;
    s[0] = f[0]/(x[0].v2+v2_one);
    z[0] = s[0]*s[0];
    w[0] = z[0]*z[0];
    t1[0]= w[0]*(v2_Lg2+w[0]*(v2_Lg4+w[0]*v2_Lg6));
    t2[0]= z[0]*(v2_Lg1+w[0]*(v2_Lg3+w[0]*(v2_Lg5/*+w[0]*v2_Lg7*/)));
    R[0] = t2[0]+t1[0];
    x[0].v2 = ((dk[0].v2 - v2_const1023_mul_2pow20)*v2_ln2_div_2pow20-(s[0]*(f[0]-R[0])-f[0]));

    f[1] = x[1].v2-v2_one;
    s[1] = f[1]/(x[1].v2+v2_one);
    z[1] = s[1]*s[1];
    w[1] = z[1]*z[1];
    t1[1]= w[1]*(v2_Lg2+w[1]*(v2_Lg4+w[1]*v2_Lg6));
    t2[1]= z[1]*(v2_Lg1+w[1]*(v2_Lg3+w[1]*(v2_Lg5/*+w[1]*v2_Lg7*/)));
    R[1] = t2[1]+t1[1];
    x[1].v2 = ((dk[1].v2- v2_const1023_mul_2pow20)*v2_ln2_div_2pow20-(s[1]*(f[1]-R[1])-f[1]));
}

static CPL_INLINE void VizGeorefSpline2DBase_func4( double* res,
                                         const double* pxy,
                                         const double* xr, const double* yr )
{
    v2dfunion x1v, y1v, xv[2], yv[2], dist[2], resv[2];
    xv[0].d[0] = xr[0];
    xv[0].d[1] = xr[1];
    xv[1].d[0] = xr[2];
    xv[1].d[1] = xr[3];
    yv[0].d[0] = yr[0];
    yv[0].d[1] = yr[1];
    yv[1].d[0] = yr[2];
    yv[1].d[1] = yr[3];
    x1v.d[0] = pxy[0];
    x1v.d[1] = pxy[0];
    y1v.d[0] = pxy[1];
    y1v.d[1] = pxy[1];
    dist[0].v2 = SQ( xv[0].v2 - x1v.v2 ) + SQ( yv[0].v2 - y1v.v2 );
    dist[1].v2 = SQ( xv[1].v2 - x1v.v2 ) + SQ( yv[1].v2 - y1v.v2 );
    resv[0] = dist[0];
    resv[1] = dist[1];
    FastApproxLog4Val(dist);
    resv[0].v2 *= dist[0].v2;
    resv[1].v2 *= dist[1].v2;
    res[0] = resv[0].d[0];
    res[1] = resv[0].d[1];
    res[2] = resv[1].d[0];
    res[3] = resv[1].d[1];
}
#else
static void VizGeorefSpline2DBase_func4( double* res,
                                         const double* pxy,
                                         const double* xr, const double* yr )
{
    double dist0  = SQ( xr[0] - pxy[0] ) + SQ( yr[0] - pxy[1] );
    res[0] = dist0 ? dist0 * log(dist0) : 0.0;
    double dist1  = SQ( xr[1] - pxy[0] ) + SQ( yr[1] - pxy[1] );
    res[1] = dist1 ? dist1 * log(dist1) : 0.0;
    double dist2  = SQ( xr[2] - pxy[0] ) + SQ( yr[2] - pxy[1] );
    res[2] = dist2 ? dist2 * log(dist2) : 0.0;
    double dist3  = SQ( xr[3] - pxy[0] ) + SQ( yr[3] - pxy[1] );
    res[3] = dist3 ? dist3 * log(dist3) : 0.0;
}
#endif

int VizGeorefSpline2D::solve(void)
{
    int r, c, v;
    int p;
	
    //	No points at all
    if ( _nof_points < 1 )
    {
        type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
        return(0);
    }
	
    // Only one point
    if ( _nof_points == 1 )
    {
        type = VIZ_GEOREF_SPLINE_ONE_POINT;
        return(1);
    }
    // Just 2 points - it is necessarily 1D case
    if ( _nof_points == 2 )
    {
        _dx = x[1] - x[0];
        _dy = y[1] - y[0];	 
        double fact = 1.0 / ( _dx * _dx + _dy * _dy );
        _dx *= fact;
        _dy *= fact;
		
        type = VIZ_GEOREF_SPLINE_TWO_POINTS;
        return(2);
    }
	
    // More than 2 points - first we have to check if it is 1D or 2D case
		
    double xmax = x[0], xmin = x[0], ymax = y[0], ymin = y[0];
    double delx, dely;
    double xx, yy;
    double sumx = 0.0f, sumy= 0.0f, sumx2 = 0.0f, sumy2 = 0.0f, sumxy = 0.0f;
    double SSxx, SSyy, SSxy;
	
    for ( p = 0; p < _nof_points; p++ )
    {
        xx = x[p];
        yy = y[p];
		
        xmax = MAX( xmax, xx );
        xmin = MIN( xmin, xx );
        ymax = MAX( ymax, yy );
        ymin = MIN( ymin, yy );
		
        sumx  += xx;
        sumx2 += xx * xx;
        sumy  += yy;
        sumy2 += yy * yy;
        sumxy += xx * yy;
    }
    delx = xmax - xmin;
    dely = ymax - ymin;
	
    SSxx = sumx2 - sumx * sumx / _nof_points;
    SSyy = sumy2 - sumy * sumy / _nof_points;
    SSxy = sumxy - sumx * sumy / _nof_points;
	
    if ( delx < 0.001 * dely || dely < 0.001 * delx || 
         fabs ( SSxy * SSxy / ( SSxx * SSyy ) ) > 0.99 )
    {
        int p1;
		
        type = VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL;
		
        _dx = _nof_points * sumx2 - sumx * sumx;
        _dy = _nof_points * sumy2 - sumy * sumy;
        double fact = 1.0 / sqrt( _dx * _dx + _dy * _dy );
        _dx *= fact;
        _dy *= fact;
		
        for ( p = 0; p < _nof_points; p++ )
        {
            double dxp = x[p] - x[0];
            double dyp = y[p] - y[0];
            u[p] = _dx * dxp + _dy * dyp;
            unused[p] = 1;
        }
		
        for ( p = 0; p < _nof_points; p++ )
        {
            int min_index = -1;
            double min_u = 0;
            for ( p1 = 0; p1 < _nof_points; p1++ )
            {
                if ( unused[p1] )
                {
                    if ( min_index < 0 || u[p1] < min_u )
                    {
                        min_index = p1;
                        min_u = u[p1];
                    }
                }
            }
            index[p] = min_index;
            unused[min_index] = 0;
        }
		
        return(3);
    }
	
    type = VIZ_GEOREF_SPLINE_FULL;
    // Make the necessary memory allocations
    if ( _AA )
        CPLFree(_AA);
    if ( _Ainv )
        CPLFree(_Ainv);
	
    _nof_eqs = _nof_points + 3;
    
    if( _nof_eqs > INT_MAX / _nof_eqs )
    {
        fprintf(stderr, "Too many coefficients. Computation aborted.\n");
        return 0;
    }
	
    _AA = ( double * )VSICalloc( _nof_eqs * _nof_eqs, sizeof( double ) );
    _Ainv = ( double * )VSICalloc( _nof_eqs * _nof_eqs, sizeof( double ) );
    
    if( _AA == NULL || _Ainv == NULL )
    {
        fprintf(stderr, "Out-of-memory while allocating temporary arrays. Computation aborted.\n");
        return 0;
    }
	
    // Calc the values of the matrix A
    for ( r = 0; r < 3; r++ )
        for ( c = 0; c < 3; c++ )
            A(r,c) = 0.0;
		
    for ( c = 0; c < _nof_points; c++ )
    {
        A(0,c+3) = 1.0;
        A(1,c+3) = x[c];
        A(2,c+3) = y[c];
			
        A(c+3,0) = 1.0;
        A(c+3,1) = x[c];
        A(c+3,2) = y[c];
    }
		
    for ( r = 0; r < _nof_points; r++ )
        for ( c = r; c < _nof_points; c++ )
        {
            A(r+3,c+3) = VizGeorefSpline2DBase_func( x[r], y[r], x[c], y[c] );
            if ( r != c )
                A(c+3,r+3 ) = A(r+3,c+3);
        }
			
#if VIZ_GEOREF_SPLINE_DEBUG
			
    for ( r = 0; r < _nof_eqs; r++ )
    {
        for ( c = 0; c < _nof_eqs; c++ )
            fprintf(stderr, "%f", A(r,c));
        fprintf(stderr, "\n");
    }
			
#endif
			
    // Invert the matrix
    int status = matrixInvert( _nof_eqs, _AA, _Ainv );
			
    if ( !status )
    {
        fprintf(stderr, " There is a problem to invert the interpolation matrix\n");
        return 0;
    }
			
    // calc the coefs
    for ( v = 0; v < _nof_vars; v++ )
        for ( r = 0; r < _nof_eqs; r++ )
        {
            coef[v][r] = 0.0;
            for ( c = 0; c < _nof_eqs; c++ )
                coef[v][r] += Ainv(r,c) * rhs[v][c];
        }
				
    return(4);
}

int VizGeorefSpline2D::get_point( const double Px, const double Py, double *vars )
{
	int v, r;
	double tmp, Pu;
	double fact;
	int leftP=0, rightP=0, found = 0;
	
	switch ( type )
	{
	case VIZ_GEOREF_SPLINE_ZERO_POINTS :
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		break;
	case VIZ_GEOREF_SPLINE_ONE_POINT :
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = rhs[v][3];
		break;
	case VIZ_GEOREF_SPLINE_TWO_POINTS :
		fact = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = ( 1 - fact ) * rhs[v][3] + fact * rhs[v][4];
		break;
	case VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL :
		Pu = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
		if ( Pu <= u[index[0]] )
		{
			leftP = index[0];
			rightP = index[1];
		}
		else if ( Pu >= u[index[_nof_points-1]] )
		{
			leftP = index[_nof_points-2];
			rightP = index[_nof_points-1];
		}
		else
		{
			for ( r = 1; !found && r < _nof_points; r++ )
			{
				leftP = index[r-1];
				rightP = index[r];					
				if ( Pu >= u[leftP] && Pu <= u[rightP] )
					found = 1;
			}
		}
		
		fact = ( Pu - u[leftP] ) / ( u[rightP] - u[leftP] );
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = ( 1.0 - fact ) * rhs[v][leftP+3] +
			fact * rhs[v][rightP+3];
		break;
	case VIZ_GEOREF_SPLINE_FULL :
    {
        double Pxy[2] = { Px, Py };
        for ( v = 0; v < _nof_vars; v++ )
            vars[v] = coef[v][0] + coef[v][1] * Px + coef[v][2] * Py;
        
        for ( r = 0; r < (_nof_points & (~3)); r+=4 )
        {
            double tmp[4];
            VizGeorefSpline2DBase_func4( tmp, Pxy, &x[r], &y[r] );
            for ( v= 0; v < _nof_vars; v++ )
                vars[v] += coef[v][r+3] * tmp[0] +
                        coef[v][r+3+1] * tmp[1] +
                        coef[v][r+3+2] * tmp[2] +
                        coef[v][r+3+3] * tmp[3];
        }
        for ( ; r < _nof_points; r++ )
        {
            tmp = VizGeorefSpline2DBase_func( Px, Py, x[r], y[r] );
            for ( v= 0; v < _nof_vars; v++ )
                vars[v] += coef[v][r+3] * tmp;
        }
        break;
    }
	case VIZ_GEOREF_SPLINE_POINT_WAS_ADDED :
		fprintf(stderr, " A point was added after the last solve\n");
		fprintf(stderr, " NO interpolation - return values are zero\n");
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		return(0);
		break;
	case VIZ_GEOREF_SPLINE_POINT_WAS_DELETED :
		fprintf(stderr, " A point was deleted after the last solve\n");
		fprintf(stderr, " NO interpolation - return values are zero\n");
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		return(0);
		break;
	default :
		return(0);
		break;
	}
	return(1);
}

#ifdef HAVE_ARMADILLO

static int matrixInvert( int N, double input[], double output[] )
{
    try
    {
        arma::mat matInput(input,N,N,false);
        const arma::mat& matInv = arma::inv(matInput);
        int row, col;
        for(row = 0; row < N; row++)
            for(col = 0; col < N; col++)
                output[row * N + col] = matInv.at(row, col);
        return true;
        //arma::mat matInv(output,N,N,false);
        //return arma::inv(matInv, matInput);
    }
    catch(...)
    {
        fprintf(stderr, "matrixInvert(): error occured.\n");
        return false;
    }
}

#else

static int matrixInvert( int N, double input[], double output[] )
{
    // Receives an array of dimension NxN as input.  This is passed as a one-
    // dimensional array of N-squared size.  It produces the inverse of the
    // input matrix, returned as output, also of size N-squared.  The Gauss-
    // Jordan Elimination method is used.  (Adapted from a BASIC routine in
    // "Basic Scientific Subroutines Vol. 1", courtesy of Scott Edwards.)
	
    // Array elements 0...N-1 are for the first row, N...2N-1 are for the
    // second row, etc.
	
    // We need to have a temporary array of size N x 2N.  We'll refer to the
    // "left" and "right" halves of this array.
	
    int row, col;
	
#if 0
    fprintf(stderr, "Matrix Inversion input matrix (N=%d)\n", N);
    for ( row=0; row<N; row++ )
    {
        for ( col=0; col<N; col++ )
        {
            fprintf(stderr, "%5.2f ", input[row*N + col ]  );
        }
        fprintf(stderr, "\n");
    }
#endif
	
    int tempSize = 2 * N * N;
    double* temp = (double*) new double[ tempSize ];
    double ftemp;
	
    if (temp == 0) {
		
        fprintf(stderr, "matrixInvert(): ERROR - memory allocation failed.\n");
        return false;
    }
	
    // First create a double-width matrix with the input array on the left
    // and the identity matrix on the right.
	
    for ( row=0; row<N; row++ )
    {
        for ( col=0; col<N; col++ )
        {
            // Our index into the temp array is X2 because it's twice as wide
            // as the input matrix.
			
            temp[ 2*row*N + col ] = input[ row*N+col ];	// left = input matrix
            temp[ 2*row*N + col + N ] = 0.0f;			// right = 0
        }
        temp[ 2*row*N + row + N ] = 1.0f;		// 1 on the diagonal of RHS
    }
	
    // Now perform row-oriented operations to convert the left hand side
    // of temp to the identity matrix.  The inverse of input will then be
    // on the right.
	
    int max;
    int k=0;
    for (k = 0; k < N; k++)
    {
        if (k+1 < N)	// if not on the last row
        {              
            max = k;
            for (row = k+1; row < N; row++) // find the maximum element
            {  
                if (fabs( temp[row*2*N + k] ) > fabs( temp[max*2*N + k] ))
                {
                    max = row;
                }
            }
			
            if (max != k)	// swap all the elements in the two rows
            {        
                for (col=k; col<2*N; col++)
                {
                    ftemp = temp[k*2*N + col];
                    temp[k*2*N + col] = temp[max*2*N + col];
                    temp[max*2*N + col] = ftemp;
                }
            }
        }
		
        ftemp = temp[ k*2*N + k ];
        if ( ftemp == 0.0f ) // matrix cannot be inverted
        {
            delete[] temp;
            return false;
        }
		
        for ( col=k; col<2*N; col++ )
        {
            temp[ k*2*N + col ] /= ftemp;
        }
		
        int i2 = k*2*N ;
        for ( row=0; row<N; row++ )
        {
            if ( row != k )
            {
                int i1 = row*2*N;
                ftemp = temp[ i1 + k ];
                for ( col=k; col<2*N; col++ ) 
                {
                    temp[ i1 + col ] -= ftemp * temp[ i2 + col ];
                }
            }
        }
    }
	
    // Retrieve inverse from the right side of temp
	
    for (row = 0; row < N; row++)
    {
        for (col = 0; col < N; col++)
        {
            output[row*N + col] = temp[row*2*N + col + N ];
        }
    }
	
#if 0
    fprintf(stderr, "Matrix Inversion result matrix:\n");
    for ( row=0; row<N; row++ )
    {
        for ( col=0; col<N; col++ )
        {
            fprintf(stderr, "%5.2f ", output[row*N + col ]  );
        }
        fprintf(stderr, "\n");
    }
#endif
	
    delete [] temp;       // free memory
    return true;
}
#endif
