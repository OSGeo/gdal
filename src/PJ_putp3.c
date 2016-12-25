#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double  A;
};

PROJ_HEAD(putp3, "Putnins P3") "\n\tPCyl., Sph.";
PROJ_HEAD(putp3p, "Putnins P3'") "\n\tPCyl., Sph.";

#define C       0.79788456
#define RPISQ   0.1013211836


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};

    xy.x = C * lp.lam * (1. - P->opaque->A * lp.phi * lp.phi);
    xy.y = C * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};

    lp.phi = xy.y / C;
    lp.lam = xy.x / (C * (1. - P->opaque->A * lp.phi * lp.phi));

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


PJ *PROJECTION(putp3) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->A = 4. * RPISQ;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

PJ *PROJECTION(putp3p) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->A = 2. * RPISQ;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_putp3_selftest (void) {return 0;}
#else

int pj_putp3_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp3   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 178227.115507793525,  89124.5607860879827},
        { 178227.115507793525, -89124.5607860879827},
        {-178227.115507793525,  89124.5607860879827},
        {-178227.115507793525, -89124.5607860879827},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00224405032986489889,  0.00112202516475805899},
        { 0.00224405032986489889, -0.00112202516475805899},
        {-0.00224405032986489889,  0.00112202516475805899},
        {-0.00224405032986489889, -0.00112202516475805899},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif


#ifndef PJ_SELFTEST
int pj_putp3p_selftest (void) {return 0;}
#else

int pj_putp3p_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp3p   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 178238.118539984745,  89124.5607860879827},
        { 178238.118539984745, -89124.5607860879827},
        {-178238.118539984745,  89124.5607860879827},
        {-178238.118539984745, -89124.5607860879827},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00224405032969050844,  0.00112202516475805899},
        { 0.00224405032969050844, -0.00112202516475805899},
        {-0.00224405032969050844,  0.00112202516475805899},
        {-0.00224405032969050844, -0.00112202516475805899},

    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
