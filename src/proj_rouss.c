/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003, 2006   Gerald I. Evenden
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
#include <projects.h>

struct pj_opaque {
    double s0;
    double A1, A2, A3, A4, A5, A6;
    double B1, B2, B3, B4, B5, B6, B7, B8;
    double C1, C2, C3, C4, C5, C6, C7, C8;
    double D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11;
    void *en;
};
PROJ_HEAD(rouss, "Roussilhe Stereographic") "\n\tAzi., Ellps.";


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double s, al, cp, sp, al2, s2;

    cp = cos(lp.phi);
    sp = sin(lp.phi);
    s = proj_mdist(lp.phi, sp, cp,  Q->en) - Q->s0;
    s2 = s * s;
    al = lp.lam * cp / sqrt(1. - P->es * sp * sp);
    al2 = al * al;
    xy.x = P->k0 * al*(1.+s2*(Q->A1+s2*Q->A4)-al2*(Q->A2+s*Q->A3+s2*Q->A5
                +al2*Q->A6));
    xy.y = P->k0 * (al2*(Q->B1+al2*Q->B4)+
        s*(1.+al2*(Q->B3-al2*Q->B6)+s2*(Q->B2+s2*Q->B8)+
        s*al2*(Q->B5+s*Q->B7)));

    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double s, al, x = xy.x / P->k0, y = xy.y / P->k0, x2, y2;;

    x2 = x * x;
    y2 = y * y;
    al = x*(1.-Q->C1*y2+x2*(Q->C2+Q->C3*y-Q->C4*x2+Q->C5*y2-Q->C7*x2*y)
        +y2*(Q->C6*y2-Q->C8*x2*y));
    s = Q->s0 + y*(1.+y2*(-Q->D2+Q->D8*y2))+
        x2*(-Q->D1+y*(-Q->D3+y*(-Q->D5+y*(-Q->D7+y*Q->D11)))+
        x2*(Q->D4+y*(Q->D6+y*Q->D10)-x2*Q->D9));
    lp.phi=proj_inv_mdist(P->ctx, s, Q->en);
    s = sin(lp.phi);
    lp.lam=al * sqrt(1. - P->es * s * s)/cos(lp.phi);

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    if (P->opaque->en)
        pj_dealloc (P->opaque->en);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(rouss) {
    double N0, es2, t, t2, R_R0_2, R_R0_4;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (!((Q->en = proj_mdist_ini(P->es))))
        E_ERROR_0;
    es2 = sin(P->phi0);
    Q->s0 = proj_mdist(P->phi0, es2, cos(P->phi0), Q->en);
    t = 1. - (es2 = P->es * es2 * es2);
    N0 = 1./sqrt(t);
    R_R0_2 = t * t / P->one_es;
    R_R0_4 = R_R0_2 * R_R0_2;
    t = tan(P->phi0);
    t2 = t * t;
    Q->C1 = Q->A1 = R_R0_2 / 4.;
    Q->C2 = Q->A2 = R_R0_2 * (2 * t2 - 1. - 2. * es2) / 12.;
    Q->A3 = R_R0_2 * t * (1. + 4. * t2)/ ( 12. * N0);
    Q->A4 = R_R0_4 / 24.;
    Q->A5 = R_R0_4 * ( -1. + t2 * (11. + 12. * t2))/24.;
    Q->A6 = R_R0_4 * ( -2. + t2 * (11. - 2. * t2))/240.;
    Q->B1 = t / (2. * N0);
    Q->B2 = R_R0_2 / 12.;
    Q->B3 = R_R0_2 * (1. + 2. * t2 - 2. * es2)/4.;
    Q->B4 = R_R0_2 * t * (2. - t2)/(24. * N0);
    Q->B5 = R_R0_2 * t * (5. + 4.* t2)/(8. * N0);
    Q->B6 = R_R0_4 * (-2. + t2 * (-5. + 6. * t2))/48.;
    Q->B7 = R_R0_4 * (5. + t2 * (19. + 12. * t2))/24.;
    Q->B8 = R_R0_4 / 120.;
    Q->C3 = R_R0_2 * t * (1. + t2)/(3. * N0);
    Q->C4 = R_R0_4 * (-3. + t2 * (34. + 22. * t2))/240.;
    Q->C5 = R_R0_4 * (4. + t2 * (13. + 12. * t2))/24.;
    Q->C6 = R_R0_4 / 16.;
    Q->C7 = R_R0_4 * t * (11. + t2 * (33. + t2 * 16.))/(48. * N0);
    Q->C8 = R_R0_4 * t * (1. + t2 * 4.)/(36. * N0);
    Q->D1 = t / (2. * N0);
    Q->D2 = R_R0_2 / 12.;
    Q->D3 = R_R0_2 * (2 * t2 + 1. - 2. * es2) / 4.;
    Q->D4 = R_R0_2 * t * (1. + t2)/(8. * N0);
    Q->D5 = R_R0_2 * t * (1. + t2 * 2.)/(4. * N0);
    Q->D6 = R_R0_4 * (1. + t2 * (6. + t2 * 6.))/16.;
    Q->D7 = R_R0_4 * t2 * (3. + t2 * 4.)/8.;
    Q->D8 = R_R0_4 / 80.;
    Q->D9 = R_R0_4 * t * (-21. + t2 * (178. - t2 * 26.))/720.;
    Q->D10 = R_R0_4 * t * (29. + t2 * (86. + t2 * 48.))/(96. * N0);
    Q->D11 = R_R0_4 * t * (37. + t2 * 44.)/(96. * N0);

    P->fwd = e_forward;
    P->inv = e_inverse;

    return P;
}


#ifndef PJ_SELFTEST
int pj_rouss_selftest (void) {return 0;}
#else

int pj_rouss_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=rouss   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222644.89413161727,  110611.09186837047},
        { 222644.89413161727, -110611.09186837047},
        {-222644.89413161727,  110611.09186837047},
        {-222644.89413161727, -110611.09186837047},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305682019911,  0.00090436947683699559},
        { 0.0017966305682019911, -0.00090436947683699559},
        {-0.0017966305682019911,  0.00090436947683699559},
        {-0.0017966305682019911, -0.00090436947683699559},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
