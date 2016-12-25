#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(tcc, "Transverse Central Cylindrical") "\n\tCyl, Sph, no inv.";

#define EPS10 1.e-10


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
    double b, bt;

    b = cos (lp.phi) * sin (lp.lam);
    if ((bt = 1. - b * b) < EPS10) F_ERROR;
    xy.x = b / sqrt(bt);
    xy.y = atan2 (tan (lp.phi) , cos (lp.lam));
    return xy;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(tcc) {
    P->es  = 0.;
    P->fwd = s_forward;
    P->inv = 0;

    return P;
}


#ifndef PJ_SELFTEST
int pj_tcc_selftest (void) {return 0;}
#else
int pj_tcc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=tcc   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223458.84419245756,  111769.14504058579},
        {223458.84419245756,  -111769.14504058579},
        {-223458.84419245756,  111769.14504058579},
        {-223458.84419245756,  -111769.14504058579},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif
