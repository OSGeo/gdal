#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(nell_h, "Nell-Hammer") "\n\tPCyl., Sph.";

#define NITER 9
#define EPS 1e-7


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = 0.5 * lp.lam * (1. + cos(lp.phi));
    xy.y = 2.0 * (lp.phi - tan(0.5 *lp.phi));

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double V, c, p;
    int i;
    (void) P;

    p = 0.5 * xy.y;
    for (i = NITER; i ; --i) {
        c = cos(0.5 * lp.phi);
        lp.phi -= V = (lp.phi - tan(lp.phi/2) - p)/(1. - 0.5/(c*c));
        if (fabs(V) < EPS)
            break;
    }
    if (!i) {
        lp.phi = p < 0. ? -M_HALFPI : M_HALFPI;
        lp.lam = 2. * xy.x;
    } else
        lp.lam = 2. * xy.x / (1. + cos(lp.phi));

    return lp;
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


PJ *PROJECTION(nell_h) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_nell_h_selftest (void) {return 0;}
#else

int pj_nell_h_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=nell_h   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223385.131640952837,  111698.236533561678},
        { 223385.131640952837, -111698.236533561678},
        {-223385.131640952837,  111698.236533561678},
        {-223385.131640952837, -111698.236533561678},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049310989310567,  0.000895246554910125378},
        { 0.00179049310989310567, -0.000895246554910125378},
        {-0.00179049310989310567,  0.000895246554910125378},
        {-0.00179049310989310567, -0.000895246554910125378},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
