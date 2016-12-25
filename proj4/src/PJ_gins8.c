#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(gins8, "Ginsburg VIII (TsNIIGAiK)") "\n\tPCyl, Sph., no inv.";

#define Cl 0.000952426
#define Cp 0.162388
#define C12 0.08333333333333333


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double t = lp.phi * lp.phi;
    (void) P;

    xy.y = lp.phi * (1. + t * C12);
    xy.x = lp.lam * (1. - Cp * t);
    t = lp.lam * lp.lam;
    xy.x *= (0.87 - Cl * t * t);

    return xy;
}


static void *freeup_new (PJ *P) {              /* Destructor */
    if (0==P)
        return 0;

    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(gins8) {
    P->es = 0.0;
    P->inv = 0;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_gins8_selftest (void) {return 0;}
#else

int pj_gins8_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=gins8   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 194350.25093959007,  111703.90763533533},
        { 194350.25093959007, -111703.90763533533},
        {-194350.25093959007,  111703.90763533533},
        {-194350.25093959007, -111703.90763533533},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
