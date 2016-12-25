#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(vandg4, "van der Grinten IV") "\n\tMisc Sph, no inv.";

#define TOL 1e-10


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double x1, t, bt, ct, ft, bt2, ct2, dt, dt2;
    (void) P;

    if (fabs(lp.phi) < TOL) {
        xy.x = lp.lam;
        xy.y = 0.;
    } else if (fabs(lp.lam) < TOL || fabs(fabs(lp.phi) - M_HALFPI) < TOL) {
        xy.x = 0.;
        xy.y = lp.phi;
    } else {
        bt = fabs(M_TWO_D_PI * lp.phi);
        bt2 = bt * bt;
        ct = 0.5 * (bt * (8. - bt * (2. + bt2)) - 5.)
            / (bt2 * (bt - 1.));
        ct2 = ct * ct;
        dt = M_TWO_D_PI * lp.lam;
        dt = dt + 1. / dt;
        dt = sqrt(dt * dt - 4.);
        if ((fabs(lp.lam) - M_HALFPI) < 0.) dt = -dt;
        dt2 = dt * dt;
        x1 = bt + ct; x1 *= x1;
        t = bt + 3.*ct;
        ft = x1 * (bt2 + ct2 * dt2 - 1.) + (1.-bt2) * (
            bt2 * (t * t + 4. * ct2) +
            ct2 * (12. * bt * ct + 4. * ct2) );
        x1 = (dt*(x1 + ct2 - 1.) + 2.*sqrt(ft)) /
            (4.* x1 + dt2);
        xy.x = M_HALFPI * x1;
        xy.y = M_HALFPI * sqrt(1. + dt * fabs(x1) - x1 * x1);
        if (lp.lam < 0.) xy.x = -xy.x;
        if (lp.phi < 0.) xy.y = -xy.y;
    }
    return xy;
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


PJ *PROJECTION(vandg4) {
    P->es = 0.;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_vandg4_selftest (void) {return 0;}
#else

int pj_vandg4_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=vandg4   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223374.57729435508,  111701.19548415358},
        { 223374.57729435508, -111701.19548415358},
        {-223374.57729435508,  111701.19548415358},
        {-223374.57729435508, -111701.19548415358},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
