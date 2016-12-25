/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Implementation of the Times projection.
 * Author:   Kristian Evers <kristianevers@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Kristian Evers
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
 *****************************************************************************
 * Based on describtion of the Times Projection in
 *
 * Flattening the Earth, Snyder, J.P., 1993, p.213-214.
 *****************************************************************************/

#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(times, "Times") "\n\tCyl, Sph";

static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    double T, S, S2;
    XY xy = {0.0,0.0};
    (void) P;

    T = tan(lp.phi/2.0);
    S = sin(M_FORTPI * T);
    S2 = S*S;

    xy.x = lp.lam * (0.74482 - 0.34588*S2);
    xy.y = 1.70711 *  T;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    double T, S, S2;
    LP lp = {0.0,0.0};
    (void) P;

    T = xy.y / 1.70711;
    S = sin(M_FORTPI * T);
    S2 = S*S;

    lp.lam = xy.x / (0.74482 - 0.34588 * S2);
    lp.phi = 2 * atan(T);

    return lp;
}


static void *freeup_new (PJ *P) {              /* Destructor */
    if (0==P)
        return 0;

    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(times) {
    P->es = 0.0;

    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_times_selftest (void) {return 0;}
#else

int pj_times_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;
    int result;
    int n = 5;

    char s_args[] = {"+proj=times +ellps=sphere"};

    XY *inv_in = malloc(n*sizeof(XY));
    LP *s_inv_expect = malloc(n*sizeof(LP));

    LP fwd_in[] = {
        {  0,   0},
        { 80,  70},
        { 25, -10},
        {-35,  20},
        {-45, -30}
    };

    XY s_fwd_expect[] = {
        { 0.0,  0.0},
        { 5785183.5760670956,  7615452.0661204215},
        { 2065971.5301078814, -951526.0648494592},
        {-2873054.0454850947,  1917730.9530005211},
        {-3651383.2035214868, -2914213.4578159209},
    };

    memcpy(inv_in, &s_fwd_expect, n*sizeof(XY));
    memcpy(s_inv_expect, &fwd_in, n*sizeof(LP));

    result = pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, n, n, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
    free(inv_in);
    free(s_inv_expect);

    return result;
}


#endif
