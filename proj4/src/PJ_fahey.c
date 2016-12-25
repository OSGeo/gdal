#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(fahey, "Fahey") "\n\tPcyl, Sph.";

#define TOL 1e-6


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = tan(0.5 * lp.phi);
    xy.y = 1.819152 * xy.x;
    xy.x = 0.819152 * lp.lam * asqrt(1 - xy.x * xy.x);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;

    xy.y /= 1.819152;
    lp.phi = 2. * atan(xy.y);
    xy.y = 1. - xy.y * xy.y;
    lp.lam = fabs(xy.y) < TOL ? 0. : xy.x / (0.819152 * sqrt(xy.y));
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


PJ *PROJECTION(fahey) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_fahey_selftest (void) {return 0;}
#else

int pj_fahey_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=fahey   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 182993.34464912376,  101603.19356988439},
        { 182993.34464912376, -101603.19356988439},
        {-182993.34464912376,  101603.19356988439},
        {-182993.34464912376, -101603.19356988439},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.0021857886080359551,  0.00098424601668238403},
        {0.0021857886080359551,  -0.00098424601668238403},
        {-0.0021857886080359551,  0.00098424601668238403},
        {-0.0021857886080359551,  -0.00098424601668238403},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
