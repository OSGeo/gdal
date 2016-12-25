#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(eck1, "Eckert I") "\n\tPCyl., Sph.";
#define FC  0.92131773192356127802
#define RP  0.31830988618379067154


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = FC * lp.lam * (1. - RP * fabs(lp.phi));
    xy.y = FC * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;

    lp.phi = xy.y / FC;
    lp.lam = xy.x / (FC * (1. - RP * fabs(lp.phi)));

    return (lp);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(eck1) {
    P->es = 0.0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_eck1_selftest (void) {return 0;}
#else

int pj_eck1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck1   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };
    XY s_fwd_expect[] = {
        { 204680.88820295094,  102912.17842606473},
        { 204680.88820295094, -102912.17842606473},
        {-204680.88820295094,  102912.17842606473},
        {-204680.88820295094, -102912.17842606473},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0019434150820034624,  0.00097170229538813102},
        { 0.0019434150820034624, -0.00097170229538813102},
        {-0.0019434150820034624,  0.00097170229538813102},
        {-0.0019434150820034624, -0.00097170229538813102},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
