#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(larr, "Larrivee") "\n\tMisc Sph, no inv.";

#define SIXTH .16666666666666666


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = 0.5 * lp.lam * (1. + sqrt(cos(lp.phi)));
    xy.y = lp.phi / (cos(0.5 * lp.phi) * cos(SIXTH * lp.lam));
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


PJ *PROJECTION(larr) {

    P->es = 0;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_larr_selftest (void) {return 0;}
#else

int pj_larr_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=larr   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223393.637624200899,  111707.215961255497},
        {223393.637624200899,  -111707.215961255497},
        {-223393.637624200899,  111707.215961255497},
        {-223393.637624200899,  -111707.215961255497},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
