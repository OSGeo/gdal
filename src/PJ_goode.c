#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(goode, "Goode Homolosine") "\n\tPCyl, Sph.";

#define Y_COR   0.05280
#define PHI_LIM 0.71093078197902358062

C_NAMESPACE PJ *pj_sinu(PJ *), *pj_moll(PJ *);

struct pj_opaque {
    PJ *sinu;
    PJ *moll;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    if (fabs(lp.phi) <= PHI_LIM)
        xy = Q->sinu->fwd(lp, Q->sinu);
    else {
        xy = Q->moll->fwd(lp, Q->moll);
        xy.y -= lp.phi >= 0.0 ? Y_COR : -Y_COR;
    }
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    if (fabs(xy.y) <= PHI_LIM)
        lp = Q->sinu->inv(xy, Q->sinu);
    else {
        xy.y += xy.y >= 0.0 ? Y_COR : -Y_COR;
        lp = Q->moll->inv(xy, Q->moll);
    }
    return lp;
}


static void *freeup_new (PJ *P) {              /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc(P);
    if (P->opaque->sinu)
        pj_dealloc(P->opaque->sinu);
    if (P->opaque->moll)
        pj_dealloc(P->opaque->moll);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);

}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(goode) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    P->es = 0.;
    if (!(Q->sinu = pj_sinu(0)) || !(Q->moll = pj_moll(0)))
        E_ERROR_0;
    Q->sinu->es = 0.;
        Q->sinu->ctx = P->ctx;
        Q->moll->ctx = P->ctx;
    if (!(Q->sinu = pj_sinu(Q->sinu)) || !(Q->moll = pj_moll(Q->moll)))
        E_ERROR_0;

    P->fwd = s_forward;
    P->inv = s_inverse;

    return P;
}


#ifndef PJ_SELFTEST
int pj_goode_selftest (void) {return 0;}
#else

int pj_goode_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=goode   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223368.11902663155,  111701.07212763709},        { 223368.11902663155, -111701.07212763709},        {-223368.11902663155,  111701.07212763709},        {-223368.11902663155, -111701.07212763709},    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931100023887,  0.00089524655489191132},        { 0.0017904931100023887, -0.00089524655489191132},        {-0.0017904931100023887,  0.00089524655489191132},        {-0.0017904931100023887, -0.00089524655489191132},    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
