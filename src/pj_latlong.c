/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Stub projection implementation for lat/long coordinates. We 
 *           don't actually change the coordinates, but we want proj=latlong
 *           to act sort of like a projection.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

/* very loosely based upon DMA code by Bradford W. Drew */
#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(lonlat, "Lat/long (Geodetic)")  "\n\t";
PROJ_HEAD(latlon, "Lat/long (Geodetic alias)")  "\n\t";
PROJ_HEAD(latlong, "Lat/long (Geodetic alias)")  "\n\t";
PROJ_HEAD(longlat, "Lat/long (Geodetic alias)")  "\n\t";


 static XY forward(LP lp, PJ *P) {
    XY xy = {0.0,0.0};
    xy.x = lp.lam / P->a;
    xy.y = lp.phi / P->a;
    return xy;
}


static LP inverse(XY xy, PJ *P) {
    LP lp = {0.0,0.0};
    lp.phi = xy.y * P->a;
    lp.lam = xy.x * P->a;
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


PJ *PROJECTION(latlong) {
    P->is_latlong = 1;
    P->x0 = 0.0;
    P->y0 = 0.0;
    P->inv = inverse;
    P->fwd = forward;

    return P;
}


PJ *PROJECTION(longlat) {
    P->is_latlong = 1;
    P->x0 = 0.0;
    P->y0 = 0.0;
    P->inv = inverse;
    P->fwd = forward;

    return P;
}


PJ *PROJECTION(latlon) {
    P->is_latlong = 1;
    P->x0 = 0.0;
    P->y0 = 0.0;
    P->inv = inverse;
    P->fwd = forward;

    return P;
}


PJ *PROJECTION(lonlat) {
    P->is_latlong = 1;
    P->x0 = 0.0;
    P->y0 = 0.0;
    P->inv = inverse; P->fwd = forward;

    return P;
}


/* Bogus self-test functions. Self-tests can't be implemented the usual way for
 * these "projections" since they can't be used directly from proj.
 * We still need them though, as all projections are automatically added to
 * the list of self-test functions.
 *
 * The code should be covered by the tests in nad/.
 * */
int pj_latlong_selftest (void) {return 0;}
int pj_longlat_selftest (void) {return 0;}
int pj_latlon_selftest (void) {return 0;}
int pj_lonlat_selftest (void) {return 0;}
