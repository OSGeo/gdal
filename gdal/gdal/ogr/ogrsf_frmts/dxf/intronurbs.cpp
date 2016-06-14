/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Low level spline interpolation.
 * Author:   David F. Rogers
 *
 ******************************************************************************

This code is derived from the code associated with the book "An Introduction
to NURBS" by David F. Rogers.  More information on the book and the code is
available at:

  http://www.nar-associates.com/nurbs/


Copyright (c) 2009, David F. Rogers
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of David F. Rogers nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <vector>
#include "cpl_port.h" // in case of -DDEBUG_BOOL

/* used by ogrdxflayer.cpp */
void rbspline2(int npts,int k,int p1,double b[],double h[],
        bool xflag, double x[], double p[]);

/************************************************************************/
/*                                knot()                                */
/************************************************************************/

/*
    Subroutine to generate a B-spline open knot vector with multiplicity
    equal to the order at the ends.

    c            = order of the basis function
    n            = the number of defining polygon vertices
    nplus2       = index of x() for the first occurrence of the maximum knot vector value
    nplusc       = maximum value of the knot vector -- $n + c$
    x()          = array containing the knot vector
*/

static void knot(int n,int c,double x[])

{
    int nplusc,nplus2,i;

    nplusc = n + c;
    nplus2 = n + 2;

    x[1] = 0.0;
    for (i = 2; i <= nplusc; i++){
        if ( (i > c) && (i < nplus2) )
            x[i] = x[i-1] + 1.0;
        else
            x[i] = x[i-1];
    }
}

/************************************************************************/
/*                               rbasis()                               */
/************************************************************************/

/*  Subroutine to generate rational B-spline basis functions--open knot vector

	C code for An Introduction to NURBS
	by David F. Rogers. Copyright (C) 2000 David F. Rogers,
	All rights reserved.

	Name: rbais
	Language: C
	Subroutines called: none
	Book reference: Chapter 4, Sec. 4. , p 296

	c        = order of the B-spline basis function
    d        = first term of the basis function recursion relation
    e        = second term of the basis function recursion relation
	h[]	     = array containing the homogeneous weights
    npts     = number of defining polygon vertices
    nplusc   = constant -- npts + c -- maximum number of knot values
    r[]      = array containing the rationalbasis functions
               r[1] contains the basis function associated with B1 etc.
    t        = parameter value
    temp[]   = temporary array
    x[]      = knot vector
*/

static void rbasis(int c,double t,int npts, double x[], double h[], double r[])

{
    int nplusc;
    int i,k;
    double d,e;
    double sum;
    std::vector<double> temp;

    nplusc = npts + c;

    temp.resize( nplusc+1 );

/* calculate the first order nonrational basis functions n[i]	*/

    for (i = 1; i<= nplusc-1; i++){
    	if (( t >= x[i]) && (t < x[i+1]))
            temp[i] = 1.0;
        else
            temp[i] = 0.0;
    }

/* calculate the higher order nonrational basis functions */

    for (k = 2; k <= c; k++){
    	for (i = 1; i <= nplusc-k; i++){
            if (temp[i] != 0)    /* if the lower order basis function is zero skip the calculation */
                d = ((t-x[i])*temp[i])/(x[i+k-1]-x[i]);
            else
                d = 0;

    	    if (temp[i+1] != 0)     /* if the lower order basis function is zero skip the calculation */
                e = ((x[i+k]-t)*temp[i+1])/(x[i+k]-x[i+1]);
            else
                e = 0;

    	    temp[i] = d + e;
        }
    }

    if (t == (double)x[nplusc]){		/*    pick up last point	*/
        temp[npts] = 1;
    }

/* calculate sum for denominator of rational basis functions */

    sum = 0.;
    for (i = 1; i <= npts; i++){
        sum = sum + temp[i]*h[i];
    }

/* form rational basis functions and put in r vector */

    for (i = 1; i <= npts; i++){
    	if (sum != 0){
            r[i] = (temp[i]*h[i])/(sum);}
        else
            r[i] = 0;
    }
}

/************************************************************************/
/*                              rbspline()                              */
/************************************************************************/

/*  Subroutine to generate a rational B-spline curve using an uniform open knot vector

	C code for An Introduction to NURBS
	by David F. Rogers. Copyright (C) 2000 David F. Rogers,
	All rights reserved.

	Name: rbspline.c
	Language: C
	Subroutines called: knot.c, rbasis.c, fmtmul.c
	Book reference: Chapter 4, Alg. p. 297

    b[]         = array containing the defining polygon vertices
                  b[1] contains the x-component of the vertex
                  b[2] contains the y-component of the vertex
                  b[3] contains the z-component of the vertex
	h[]			= array containing the homogeneous weighting factors
    k           = order of the B-spline basis function
    nbasis      = array containing the basis functions for a single value of t
    nplusc      = number of knot values
    npts        = number of defining polygon vertices
    p[,]        = array containing the curve points
                  p[1] contains the x-component of the point
                  p[2] contains the y-component of the point
                  p[3] contains the z-component of the point
    p1          = number of points to be calculated on the curve
    t           = parameter value 0 <= t <= npts - k + 1
    x[]         = array containing the knot vector
*/

void rbspline2(int npts,int k,int p1,double b[],double h[],
        bool xflag, double x[], double p[])

{
    int i,j,icount,jcount;
    int i1;
    int nplusc;

    double step;
    double t;
    double temp;
    std::vector<double> nbasis;

    nplusc = npts + k;

    nbasis.resize( npts+1 );

/* generate the uniform open knot vector */

    if( xflag == true )
        knot(npts,k,x);

    icount = 0;

/*    calculate the points on the rational B-spline curve */

    t = 0;
    step = ((double)x[nplusc])/((double)(p1-1));

    for (i1 = 1; i1<= p1; i1++){

        if ((double)x[nplusc] - t < 5e-6){
            t = (double)x[nplusc];
        }

        /* generate the basis function for this value of t */
        rbasis(k,t,npts,x,h,&(nbasis[0]));
        for (j = 1; j <= 3; j++){      /* generate a point on the curve */
            jcount = j;
            p[icount+j] = 0.;

            for (i = 1; i <= npts; i++){ /* Do local matrix multiplication */
                temp = nbasis[i]*b[jcount];
                p[icount + j] = p[icount + j] + temp;
                jcount = jcount + 3;
            }
        }
    	icount = icount + 3;
        t = t + step;
    }
}
