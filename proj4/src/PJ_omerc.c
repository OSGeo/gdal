/*
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

PROJ_HEAD(omerc, "Oblique Mercator")
    "\n\tCyl, Sph&Ell no_rot\n\t"
    "alpha= [gamma=] [no_off] lonc= or\n\t lon_1= lat_1= lon_2= lat_2=";

struct pj_opaque {
    double  A, B, E, AB, ArB, BrA, rB, singam, cosgam, sinrot, cosrot;
    double  v_pole_n, v_pole_s, u_0;
    int no_rot;
};

#define TOL 1.e-7
#define EPS 1.e-10


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double S, T, U, V, W, temp, u, v;

    if (fabs(fabs(lp.phi) - M_HALFPI) > EPS) {
        W = Q->E / pow(pj_tsfn(lp.phi, sin(lp.phi), P->e), Q->B);
        temp = 1. / W;
        S = .5 * (W - temp);
        T = .5 * (W + temp);
        V = sin(Q->B * lp.lam);
        U = (S * Q->singam - V * Q->cosgam) / T;
        if (fabs(fabs(U) - 1.0) < EPS)
            F_ERROR;
        v = 0.5 * Q->ArB * log((1. - U)/(1. + U));
        temp = cos(Q->B * lp.lam);
                if(fabs(temp) < TOL) {
                    u = Q->A * lp.lam;
                } else {
                    u = Q->ArB * atan2((S * Q->cosgam + V * Q->singam), temp);
                }
    } else {
        v = lp.phi > 0 ? Q->v_pole_n : Q->v_pole_s;
        u = Q->ArB * lp.phi;
    }
    if (Q->no_rot) {
        xy.x = u;
        xy.y = v;
    } else {
        u -= Q->u_0;
        xy.x = v * Q->cosrot + u * Q->sinrot;
        xy.y = u * Q->cosrot - v * Q->sinrot;
    }
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  u, v, Qp, Sp, Tp, Vp, Up;

    if (Q->no_rot) {
        v = xy.y;
        u = xy.x;
    } else {
        v = xy.x * Q->cosrot - xy.y * Q->sinrot;
        u = xy.y * Q->cosrot + xy.x * Q->sinrot + Q->u_0;
    }
    Qp = exp(- Q->BrA * v);
    Sp = .5 * (Qp - 1. / Qp);
    Tp = .5 * (Qp + 1. / Qp);
    Vp = sin(Q->BrA * u);
    Up = (Vp * Q->cosgam + Sp * Q->singam) / Tp;
    if (fabs(fabs(Up) - 1.) < EPS) {
        lp.lam = 0.;
        lp.phi = Up < 0. ? -M_HALFPI : M_HALFPI;
    } else {
        lp.phi = Q->E / sqrt((1. + Up) / (1. - Up));
        if ((lp.phi = pj_phi2(P->ctx, pow(lp.phi, 1. / Q->B), P->e)) == HUGE_VAL)
            I_ERROR;
        lp.lam = - Q->rB * atan2((Sp * Q->cosgam -
            Vp * Q->singam), cos(Q->BrA * u));
    }
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


PJ *PROJECTION(omerc) {
    double con, com, cosph0, D, F, H, L, sinph0, p, J, gamma=0,
        gamma0, lamc=0, lam1=0, lam2=0, phi1=0, phi2=0, alpha_c=0;
    int alp, gam, no_off = 0;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->no_rot = pj_param(P->ctx, P->params, "tno_rot").i;
        if ((alp = pj_param(P->ctx, P->params, "talpha").i) != 0)
        alpha_c = pj_param(P->ctx, P->params, "ralpha").f;
        if ((gam = pj_param(P->ctx, P->params, "tgamma").i) != 0)
        gamma = pj_param(P->ctx, P->params, "rgamma").f;
    if (alp || gam) {
        lamc    = pj_param(P->ctx, P->params, "rlonc").f;
        no_off =
                    /* For libproj4 compatability */
                    pj_param(P->ctx, P->params, "tno_off").i
                    /* for backward compatibility */
                    || pj_param(P->ctx, P->params, "tno_uoff").i;
        if( no_off )
        {
            /* Mark the parameter as used, so that the pj_get_def() return them */
            pj_param(P->ctx, P->params, "sno_uoff");
            pj_param(P->ctx, P->params, "sno_off");
        }
    } else {
        lam1 = pj_param(P->ctx, P->params, "rlon_1").f;
        phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
        lam2 = pj_param(P->ctx, P->params, "rlon_2").f;
        phi2 = pj_param(P->ctx, P->params, "rlat_2").f;
        if (fabs(phi1 - phi2) <= TOL ||
            (con = fabs(phi1)) <= TOL ||
            fabs(con - M_HALFPI) <= TOL ||
            fabs(fabs(P->phi0) - M_HALFPI) <= TOL ||
            fabs(fabs(phi2) - M_HALFPI) <= TOL) E_ERROR(-33);
    }
    com = sqrt(P->one_es);
    if (fabs(P->phi0) > EPS) {
        sinph0 = sin(P->phi0);
        cosph0 = cos(P->phi0);
        con = 1. - P->es * sinph0 * sinph0;
        Q->B = cosph0 * cosph0;
        Q->B = sqrt(1. + P->es * Q->B * Q->B / P->one_es);
        Q->A = Q->B * P->k0 * com / con;
        D = Q->B * com / (cosph0 * sqrt(con));
        if ((F = D * D - 1.) <= 0.)
            F = 0.;
        else {
            F = sqrt(F);
            if (P->phi0 < 0.)
                F = -F;
        }
        Q->E = F += D;
        Q->E *= pow(pj_tsfn(P->phi0, sinph0, P->e), Q->B);
    } else {
        Q->B = 1. / com;
        Q->A = P->k0;
        Q->E = D = F = 1.;
    }
    if (alp || gam) {
        if (alp) {
            gamma0 = asin(sin(alpha_c) / D);
            if (!gam)
                gamma = alpha_c;
        } else
            alpha_c = asin(D*sin(gamma0 = gamma));
        if ((con = fabs(alpha_c)) <= TOL ||
            fabs(con - M_PI) <= TOL ||
            fabs(fabs(P->phi0) - M_HALFPI) <= TOL)
            E_ERROR(-32);
        P->lam0 = lamc - asin(.5 * (F - 1. / F) *
           tan(gamma0)) / Q->B;
    } else {
        H = pow(pj_tsfn(phi1, sin(phi1), P->e), Q->B);
        L = pow(pj_tsfn(phi2, sin(phi2), P->e), Q->B);
        F = Q->E / H;
        p = (L - H) / (L + H);
        J = Q->E * Q->E;
        J = (J - L * H) / (J + L * H);
        if ((con = lam1 - lam2) < -M_PI)
            lam2 -= M_TWOPI;
        else if (con > M_PI)
            lam2 += M_TWOPI;
        P->lam0 = adjlon(.5 * (lam1 + lam2) - atan(
           J * tan(.5 * Q->B * (lam1 - lam2)) / p) / Q->B);
        gamma0 = atan(2. * sin(Q->B * adjlon(lam1 - P->lam0)) /
           (F - 1. / F));
        gamma = alpha_c = asin(D * sin(gamma0));
    }
    Q->singam = sin(gamma0);
    Q->cosgam = cos(gamma0);
    Q->sinrot = sin(gamma);
    Q->cosrot = cos(gamma);
    Q->BrA = 1. / (Q->ArB = Q->A * (Q->rB = 1. / Q->B));
    Q->AB = Q->A * Q->B;
    if (no_off)
        Q->u_0 = 0;
    else {
        Q->u_0 = fabs(Q->ArB * atan2(sqrt(D * D - 1.), cos(alpha_c)));
        if (P->phi0 < 0.)
            Q->u_0 = - Q->u_0;
    }
    F = 0.5 * gamma0;
    Q->v_pole_n = Q->ArB * log(tan(M_FORTPI - F));
    Q->v_pole_s = Q->ArB * log(tan(M_FORTPI + F));
    P->inv = e_inverse;
    P->fwd = e_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_omerc_selftest (void) {return 0;}
#else

int pj_omerc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=omerc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222650.796885261341,  110642.229314983808},
        { 222650.796885261341, -110642.229314983808},
        {-222650.796885261545,  110642.229314983808},
        {-222650.796885261545, -110642.229314983808},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179663056816996357,  0.000904369474808157338},
        { 0.00179663056816996357, -0.000904369474820879583},
        {-0.0017966305681604536,   0.000904369474808157338},
        {-0.0017966305681604536,  -0.000904369474820879583},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
