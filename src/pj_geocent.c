/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Stub projection for geocentric.  The transformation isn't
 *           really done here since this code is 2D.  The real transformation
 *           is handled by pj_transform.c.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 *****************************************************************************/

#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(geocent, "Geocentric")  "\n\t";

static XY forward(LP lp, PJ *P) {
    XY xy = {0.0,0.0};
    (void) P;
    xy.x = lp.lam;
    xy.y = lp.phi;
    return xy;
}

static LP inverse(XY xy, PJ *P) {
    LP lp = {0.0,0.0};
    (void) P;
    lp.phi = xy.y;
    lp.lam = xy.x;
    return lp;
}


static void *freeup_new (PJ *P) {
    if (0==P)
        return 0;

    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}

PJ *PROJECTION(geocent) {
    P->is_geocent = 1;
    P->x0 = 0.0;
    P->y0 = 0.0;
    P->inv = inverse;
    P->fwd = forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_geocent_selftest (void) {return 0;}
#else

int pj_geocent_selftest (void) {

    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=geocent   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=geocent   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222638.98158654713,  111319.49079327357},
        { 222638.98158654713, -111319.49079327357},
        {-222638.98158654713,  111319.49079327357},
        {-222638.98158654713, -111319.49079327357},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305682390426,  0.00089831528411952132},
        { 0.0017966305682390426, -0.00089831528411952132},
        {-0.0017966305682390426,  0.00089831528411952132},
        {-0.0017966305682390426, -0.00089831528411952132},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}

#endif

