#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(lask, "Laskowski") "\n\tMisc Sph, no inv.";

#define a10  0.975534
#define a12 -0.119161
#define a32 -0.0143059
#define a14 -0.0547009
#define b01  1.00384
#define b21  0.0802894
#define b03  0.0998909
#define b41  0.000199025
#define b23 -0.0285500
#define b05 -0.0491032


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double l2, p2;
    (void) P;

    l2 = lp.lam * lp.lam;
    p2 = lp.phi * lp.phi;
    xy.x = lp.lam * (a10 + p2 * (a12 + l2 * a32 + p2 * a14));
    xy.y = lp.phi * (b01 + l2 * (b21 + p2 * b23 + l2 * b41) +
               p2 * (b03 + p2 * b05));
    return xy;
}

static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(lask) {

    P->fwd = s_forward;
    P->es = 0.;

    return P;
}

#ifndef PJ_SELFTEST
int pj_lask_selftest (void) {return 0;}
#else

int pj_lask_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=lask   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 217928.275907355128,  112144.32922014239},
        { 217928.275907355128, -112144.32922014239},
        {-217928.275907355128,  112144.32922014239},
        {-217928.275907355128, -112144.32922014239},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
