/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2004   Gerald I. Evenden
** Copyright (c) 2012   Martin Raspaud
**
** See also (section 4.4.3.2):
**   http://www.eumetsat.int/en/area4/msg/news/us_doc/cgms_03_26.pdf
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double h;
    double radius_p;
    double radius_p2;
    double radius_p_inv2;
    double radius_g;
    double radius_g_1;
    double C;
    char *sweep_axis;
    int flip_axis;
};

PROJ_HEAD(geos, "Geostationary Satellite View") "\n\tAzi, Sph&Ell\n\th=";


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double Vx, Vy, Vz, tmp;

    /* Calculation of the three components of the vector from satellite to
    ** position on earth surface (lon,lat).*/
    tmp = cos(lp.phi);
    Vx = cos (lp.lam) * tmp;
    Vy = sin (lp.lam) * tmp;
    Vz = sin (lp.phi);

    /* Check visibility*/


    /* Calculation based on view angles from satellite.*/
    tmp = Q->radius_g - Vx;

    if(Q->flip_axis) {
        xy.x = Q->radius_g_1 * atan(Vy / hypot(Vz, tmp));
        xy.y = Q->radius_g_1 * atan(Vz / tmp);
    } else {
        xy.x = Q->radius_g_1 * atan(Vy / tmp);
        xy.y = Q->radius_g_1 * atan(Vz / hypot(Vy, tmp));
    }

    return xy;
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double r, Vx, Vy, Vz, tmp;

    /* Calculation of geocentric latitude. */
    lp.phi = atan (Q->radius_p2 * tan (lp.phi));

    /* Calculation of the three components of the vector from satellite to
    ** position on earth surface (lon,lat).*/
    r = (Q->radius_p) / hypot(Q->radius_p * cos (lp.phi), sin (lp.phi));
    Vx = r * cos (lp.lam) * cos (lp.phi);
    Vy = r * sin (lp.lam) * cos (lp.phi);
    Vz = r * sin (lp.phi);

    /* Check visibility. */
    if (((Q->radius_g - Vx) * Vx - Vy * Vy - Vz * Vz * Q->radius_p_inv2) < 0.)
        F_ERROR;

    /* Calculation based on view angles from satellite. */
    tmp = Q->radius_g - Vx;

    if(Q->flip_axis) {
        xy.x = Q->radius_g_1 * atan (Vy / hypot (Vz, tmp));
        xy.y = Q->radius_g_1 * atan (Vz / tmp);
    } else {
        xy.x = Q->radius_g_1 * atan (Vy / tmp);
        xy.y = Q->radius_g_1 * atan (Vz / hypot (Vy, tmp));
    }

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double Vx, Vy, Vz, a, b, det, k;

    /* Setting three components of vector from satellite to position.*/
    Vx = -1.0;
    if(Q->flip_axis) {
        Vz = tan (xy.y / (Q->radius_g - 1.0));
        Vy = tan (xy.x / (Q->radius_g - 1.0)) * sqrt (1.0 + Vz * Vz);
    } else {
        Vy = tan (xy.x / (Q->radius_g - 1.0));
        Vz = tan (xy.y / (Q->radius_g - 1.0)) * sqrt (1.0 + Vy * Vy);
    }

    /* Calculation of terms in cubic equation and determinant.*/
    a = Vy * Vy + Vz * Vz + Vx * Vx;
    b = 2 * Q->radius_g * Vx;
    if ((det = (b * b) - 4 * a * Q->C) < 0.) I_ERROR;

    /* Calculation of three components of vector from satellite to position.*/
    k  = (-b - sqrt(det)) / (2 * a);
    Vx = Q->radius_g + k * Vx;
    Vy *= k;
    Vz *= k;

    /* Calculation of longitude and latitude.*/
    lp.lam = atan2 (Vy, Vx);
    lp.phi = atan (Vz * cos (lp.lam) / Vx);

    return lp;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double Vx, Vy, Vz, a, b, det, k;

    /* Setting three components of vector from satellite to position.*/
    Vx = -1.0;

    if(Q->flip_axis) {
        Vz = tan (xy.y / Q->radius_g_1);
        Vy = tan (xy.x / Q->radius_g_1) * hypot(1.0, Vz);
    } else {
        Vy = tan (xy.x / Q->radius_g_1);
        Vz = tan (xy.y / Q->radius_g_1) * hypot(1.0, Vy);
    }

    /* Calculation of terms in cubic equation and determinant.*/
    a = Vz / Q->radius_p;
    a   = Vy * Vy + a * a + Vx * Vx;
    b   = 2 * Q->radius_g * Vx;
    if ((det = (b * b) - 4 * a * Q->C) < 0.) I_ERROR;

    /* Calculation of three components of vector from satellite to position.*/
    k  = (-b - sqrt(det)) / (2. * a);
    Vx = Q->radius_g + k * Vx;
    Vy *= k;
    Vz *= k;

    /* Calculation of longitude and latitude.*/
    lp.lam = atan2 (Vy, Vx);
    lp.phi = atan (Vz * cos (lp.lam) / Vx);
    lp.phi = atan (Q->radius_p_inv2 * tan (lp.phi));

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(geos) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if ((Q->h = pj_param(P->ctx, P->params, "dh").f) <= 0.) E_ERROR(-30);

    if (P->phi0) E_ERROR(-46);

    Q->sweep_axis = pj_param(P->ctx, P->params, "ssweep").s;
    if (Q->sweep_axis == NULL)
      Q->flip_axis = 0;
    else {
        if (Q->sweep_axis[1] != '\0' ||
            (Q->sweep_axis[0] != 'x' &&
             Q->sweep_axis[0] != 'y'))
          E_ERROR(-49);
        if (Q->sweep_axis[0] == 'x')
          Q->flip_axis = 1;
        else
          Q->flip_axis = 0;
    }

    Q->radius_g_1 = Q->h / P->a;
    Q->radius_g = 1. + Q->radius_g_1;
    Q->C  = Q->radius_g * Q->radius_g - 1.0;
    if (P->es) {
        Q->radius_p      = sqrt (P->one_es);
        Q->radius_p2     = P->one_es;
        Q->radius_p_inv2 = P->rone_es;
        P->inv = e_inverse;
        P->fwd = e_forward;
    } else {
        Q->radius_p = Q->radius_p2 = Q->radius_p_inv2 = 1.0;
        P->inv = s_inverse;
        P->fwd = s_forward;
    }

    return P;
}


#ifndef PJ_SELFTEST
int pj_geos_selftest (void) {return 0;}
#else

int pj_geos_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=geos   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +h=35785831"};
    char s_args[] = {"+proj=geos   +a=6400000    +lat_1=0.5 +lat_2=2 +h=35785831"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222527.07036580026,  110551.30341332949},
        { 222527.07036580026, -110551.30341332949},
        {-222527.07036580026,  110551.30341332949},
        {-222527.07036580026, -110551.30341332949},
    };

    XY s_fwd_expect[] = {
        { 223289.45763579503,  111677.65745653701},
        { 223289.45763579503,  -111677.65745653701},
        {-223289.45763579503,  111677.65745653701},
        {-223289.45763579503,  -111677.65745653701},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305689715385,  0.00090436947723267452},
        { 0.0017966305689715385, -0.00090436947723267452},
        {-0.0017966305689715385,  0.00090436947723267452},
        {-0.0017966305689715385, -0.00090436947723267452},
    };

    LP s_inv_expect[] = {
        { 0.0017904931105078943,  0.00089524655504237148},
        { 0.0017904931105078943, -0.00089524655504237148},
        {-0.0017904931105078943,  0.00089524655504237148},
        {-0.0017904931105078943, -0.00089524655504237148},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
