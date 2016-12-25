#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double  A, B;
};

PROJ_HEAD(putp5, "Putnins P5") "\n\tPCyl., Sph.";
PROJ_HEAD(putp5p, "Putnins P5'") "\n\tPCyl., Sph.";

#define C 1.01346
#define D 1.2158542


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    xy.x = C * lp.lam * (Q->A - Q->B * sqrt(1. + D * lp.phi * lp.phi));
    xy.y = C * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.phi = xy.y / C;
    lp.lam = xy.x / (C * (Q->A - Q->B * sqrt(1. + D * lp.phi * lp.phi)));

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(putp5) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->A = 2.;
    Q->B = 1.;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


PJ *PROJECTION(putp5p) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->A = 1.5;
    Q->B = 0.5;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_putp5_selftest (void) {return 0;}
#else

int pj_putp5_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp5   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 226367.21338056153,  113204.56855847509},
        { 226367.21338056153, -113204.56855847509},
        {-226367.21338056153,  113204.56855847509},
        {-226367.21338056153, -113204.56855847509},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00176671315102969553,  0.000883356575387199546},
        { 0.00176671315102969553, -0.000883356575387199546},
        {-0.00176671315102969553,  0.000883356575387199546},
        {-0.00176671315102969553, -0.000883356575387199546},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif


#ifndef PJ_SELFTEST
int pj_putp5p_selftest (void) {return 0;}
#else

int pj_putp5p_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp5p   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 226388.175248755841,  113204.56855847509},
        { 226388.175248755841, -113204.56855847509},
        {-226388.175248755841,  113204.56855847509},
        {-226388.175248755841, -113204.56855847509},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00176671315090204742,  0.000883356575387199546},
        { 0.00176671315090204742, -0.000883356575387199546},
        {-0.00176671315090204742,  0.000883356575387199546},
        {-0.00176671315090204742, -0.000883356575387199546},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif
