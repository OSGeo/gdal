#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(lcc, "Lambert Conformal Conic")
    "\n\tConic, Sph&Ell\n\tlat_1= and lat_2= or lat_0";

# define EPS10  1.e-10

struct pj_opaque {
    double phi1;
    double phi2;
    double n;
    double rho0;
    double c;
    int    ellips;
};


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double rho;

    if (fabs(fabs(lp.phi) - M_HALFPI) < EPS10) {
        if ((lp.phi * Q->n) <= 0.) F_ERROR;
        rho = 0.;
    } else {
        rho = Q->c * (Q->ellips ? pow(pj_tsfn(lp.phi, sin(lp.phi),
            P->e), Q->n) : pow(tan(M_FORTPI + .5 * lp.phi), -Q->n));
    }
    lp.lam *= Q->n;
    xy.x = P->k0 * (rho * sin( lp.lam) );
    xy.y = P->k0 * (Q->rho0 - rho * cos(lp.lam) );
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double rho;

    xy.x /= P->k0;
    xy.y /= P->k0;

    xy.y = Q->rho0 - xy.y;
    rho = hypot(xy.x, xy.y);
    if (rho != 0.0) {
        if (Q->n < 0.) {
            rho = -rho;
            xy.x = -xy.x;
            xy.y = -xy.y;
        }
        if (Q->ellips) {
            lp.phi = pj_phi2(P->ctx, pow(rho / Q->c, 1./Q->n), P->e);
            if (lp.phi == HUGE_VAL)
                I_ERROR;
        } else
            lp.phi = 2. * atan(pow(Q->c / rho, 1./Q->n)) - M_HALFPI;
        lp.lam = atan2(xy.x, xy.y) / Q->n;
    } else {
        lp.lam = 0.;
        lp.phi = Q->n > 0. ? M_HALFPI : -M_HALFPI;
    }
    return lp;
}

static void special(LP lp, PJ *P, struct FACTORS *fac) {
    struct pj_opaque *Q = P->opaque;
    double rho;
    if (fabs(fabs(lp.phi) - M_HALFPI) < EPS10) {
        if ((lp.phi * Q->n) <= 0.) return;
        rho = 0.;
    } else
        rho = Q->c * (Q->ellips ? pow(pj_tsfn(lp.phi, sin(lp.phi),
            P->e), Q->n) : pow(tan(M_FORTPI + .5 * lp.phi), -Q->n));
    fac->code |= IS_ANAL_HK + IS_ANAL_CONV;
    fac->k = fac->h = P->k0 * Q->n * rho /
        pj_msfn(sin(lp.phi), cos(lp.phi), P->es);
    fac->conv = - Q->n * lp.lam;
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


PJ *PROJECTION(lcc) {
    double cosphi, sinphi;
    int secant;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));

    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;


    Q->phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
    if (pj_param(P->ctx, P->params, "tlat_2").i)
        Q->phi2 = pj_param(P->ctx, P->params, "rlat_2").f;
    else {
        Q->phi2 = Q->phi1;
        if (!pj_param(P->ctx, P->params, "tlat_0").i)
            P->phi0 = Q->phi1;
    }
    if (fabs(Q->phi1 + Q->phi2) < EPS10) E_ERROR(-21);
    Q->n = sinphi = sin(Q->phi1);
    cosphi = cos(Q->phi1);
    secant = fabs(Q->phi1 - Q->phi2) >= EPS10;
    if( (Q->ellips = (P->es != 0.)) ) {
        double ml1, m1;

        P->e = sqrt(P->es);
        m1 = pj_msfn(sinphi, cosphi, P->es);
        ml1 = pj_tsfn(Q->phi1, sinphi, P->e);
        if (secant) { /* secant cone */
            sinphi = sin(Q->phi2);
            Q->n = log(m1 / pj_msfn(sinphi, cos(Q->phi2), P->es));
            Q->n /= log(ml1 / pj_tsfn(Q->phi2, sinphi, P->e));
        }
        Q->c = (Q->rho0 = m1 * pow(ml1, -Q->n) / Q->n);
        Q->rho0 *= (fabs(fabs(P->phi0) - M_HALFPI) < EPS10) ? 0. :
            pow(pj_tsfn(P->phi0, sin(P->phi0), P->e), Q->n);
    } else {
        if (secant)
            Q->n = log(cosphi / cos(Q->phi2)) /
               log(tan(M_FORTPI + .5 * Q->phi2) /
               tan(M_FORTPI + .5 * Q->phi1));
        Q->c = cosphi * pow(tan(M_FORTPI + .5 * Q->phi1), Q->n) / Q->n;
        Q->rho0 = (fabs(fabs(P->phi0) - M_HALFPI) < EPS10) ? 0. :
            Q->c * pow(tan(M_FORTPI + .5 * P->phi0), -Q->n);
    }

    P->inv = e_inverse;
    P->fwd = e_forward;
    P->spc = special;

    return P;
}


#ifndef PJ_SELFTEST
int pj_lcc_selftest (void) {return 0;}
#else

int pj_lcc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=lcc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222588.439735968423,  110660.533870799671},
        { 222756.879700278747, -110532.797660827026},
        {-222588.439735968423,  110660.533870799671},
        {-222756.879700278747, -110532.797660827026},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179635940600536667,  0.000904232207322381741},
        { 0.00179635817735249777, -0.000904233135128348995},
        {-0.00179635940600536667,  0.000904232207322381741},
        {-0.00179635817735249777, -0.000904233135128348995},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
