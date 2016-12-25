#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(nell, "Nell") "\n\tPCyl., Sph.";

#define MAX_ITER 10
#define LOOP_TOL 1e-7


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double k, V;
    int i;
    (void) P;

    k = 2. * sin(lp.phi);
    V = lp.phi * lp.phi;
    lp.phi *= 1.00371 + V * (-0.0935382 + V * -0.011412);
    for (i = MAX_ITER; i ; --i) {
        lp.phi -= V = (lp.phi + sin(lp.phi) - k) /
            (1. + cos(lp.phi));
        if (fabs(V) < LOOP_TOL)
            break;
    }
    xy.x = 0.5 * lp.lam * (1. + cos(lp.phi));
    xy.y = lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    lp.lam = 2. * xy.x / (1. + cos(xy.y));
    lp.phi = aasin(P->ctx,0.5 * (xy.y + sin(xy.y)));

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


PJ *PROJECTION(nell) {

    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_nell_selftest (void) {return 0;}
#else

int pj_nell_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=nell   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223385.132504695706,  111698.23644718733},
        { 223385.132504695706, -111698.23644718733},
        {-223385.132504695706,  111698.23644718733},
        {-223385.132504695706, -111698.23644718733},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049310989310567,  0.000895246554910125161},
        { 0.00179049310989310567, -0.000895246554910125161},
        {-0.00179049310989310567,  0.000895246554910125161},
        {-0.00179049310989310567, -0.000895246554910125161},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
