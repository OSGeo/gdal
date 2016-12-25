/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003   Gerald I. Evenden
*/
/*
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
#include    <projects.h>


struct pj_opaque {
    double phic0;
    double cosc0, sinc0;
    double R2;
    void *en;
};


PROJ_HEAD(sterea, "Oblique Stereographic Alternative") "\n\tAzimuthal, Sph&Ell";
# define DEL_TOL    1.e-14
# define MAX_ITER   10



static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double cosc, sinc, cosl, k;

    lp = pj_gauss(P->ctx, lp, Q->en);
    sinc = sin(lp.phi);
    cosc = cos(lp.phi);
    cosl = cos(lp.lam);
    k = P->k0 * Q->R2 / (1. + Q->sinc0 * sinc + Q->cosc0 * cosc * cosl);
    xy.x = k * cosc * sin(lp.lam);
    xy.y = k * (Q->cosc0 * sinc - Q->sinc0 * cosc * cosl);
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double rho, c, sinc, cosc;

    xy.x /= P->k0;
    xy.y /= P->k0;
    if ( (rho = hypot (xy.x, xy.y)) ) {
        c = 2. * atan2 (rho, Q->R2);
        sinc = sin (c);
        cosc = cos (c);
        lp.phi = asin (cosc * Q->sinc0 + xy.y * sinc * Q->cosc0 / rho);
        lp.lam = atan2 (xy.x * sinc, rho * Q->cosc0 * cosc - xy.y * Q->sinc0 * sinc);
    } else {
        lp.phi = Q->phic0;
        lp.lam = 0.;
    }
    return pj_inv_gauss(P->ctx, lp, Q->en);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque->en);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(sterea) {
    double R;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));

    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->en = pj_gauss_ini(P->e, P->phi0, &(Q->phic0), &R);
    if (0==Q->en)
        return freeup_new (P);

    Q->sinc0 = sin (Q->phic0);
    Q->cosc0 = cos (Q->phic0);
    Q->R2 = 2. * R;

    P->inv = e_inverse;
    P->fwd = e_forward;
    return P;
}


#ifndef PJ_SELFTEST
int pj_sterea_selftest (void) {return 0;}
#else

int pj_sterea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=sterea   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=sterea   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222644.89410919772,  110611.09187173686},
        { 222644.89410919772, -110611.09187173827},
        {-222644.89410919772,  110611.09187173686},
        {-222644.89410919772, -110611.09187173827},
    };

    XY s_fwd_expect[] = {
        { 223407.81025950745,  111737.93899644315},
        { 223407.81025950745, -111737.93899644315},
        {-223407.81025950745,  111737.93899644315},
        {-223407.81025950745, -111737.93899644315},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305682019911,  0.00090436947683099009},
        { 0.0017966305682019911, -0.00090436947684371233},
        {-0.0017966305682019911,  0.00090436947683099009},
        {-0.0017966305682019911, -0.00090436947684371233},
    };

    LP s_inv_expect[] = {
        { 0.001790493109747395,  0.00089524655465446378},
        { 0.001790493109747395, -0.00089524655465446378},
        {-0.001790493109747395,  0.00089524655465446378},
        {-0.001790493109747395, -0.00089524655465446378},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
