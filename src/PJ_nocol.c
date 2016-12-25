#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(nicol, "Nicolosi Globular") "\n\tMisc Sph, no inv.";

#define EPS 1e-10


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    if (fabs(lp.lam) < EPS) {
        xy.x = 0;
        xy.y = lp.phi;
    } else if (fabs(lp.phi) < EPS) {
        xy.x = lp.lam;
        xy.y = 0.;
    } else if (fabs(fabs(lp.lam) - M_HALFPI) < EPS) {
        xy.x = lp.lam * cos(lp.phi);
        xy.y = M_HALFPI * sin(lp.phi);
    } else if (fabs(fabs(lp.phi) - M_HALFPI) < EPS) {
        xy.x = 0;
        xy.y = lp.phi;
    } else {
        double tb, c, d, m, n, r2, sp;

        tb = M_HALFPI / lp.lam - lp.lam / M_HALFPI;
        c = lp.phi / M_HALFPI;
        d = (1 - c * c)/((sp = sin(lp.phi)) - c);
        r2 = tb / d;
        r2 *= r2;
        m = (tb * sp / d - 0.5 * tb)/(1. + r2);
        n = (sp / r2 + 0.5 * d)/(1. + 1./r2);
        xy.x = cos(lp.phi);
        xy.x = sqrt(m * m + xy.x * xy.x / (1. + r2));
        xy.x = M_HALFPI * ( m + (lp.lam < 0. ? -xy.x : xy.x));
        xy.y = sqrt(n * n - (sp * sp / r2 + d * sp - 1.) /
            (1. + 1./r2));
        xy.y = M_HALFPI * ( n + (lp.phi < 0. ? xy.y : -xy.y ));
    }
    return (xy);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;

    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(nicol) {
    P->es = 0.;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_nicol_selftest (void) {return 0;}
#else

int pj_nicol_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=nicol   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223374.561814139714,  111732.553988545071},
        { 223374.561814139714, -111732.553988545071},
        {-223374.561814139714,  111732.553988545071},
        {-223374.561814139714, -111732.553988545071},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
