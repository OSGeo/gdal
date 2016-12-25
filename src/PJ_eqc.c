#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double rc;
};

PROJ_HEAD(eqc, "Equidistant Cylindrical (Plate Caree)")
    "\n\tCyl, Sph\n\tlat_ts=[, lat_0=0]";


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    xy.x = Q->rc * lp.lam;
    xy.y = lp.phi - P->phi0;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.lam = xy.x / Q->rc;
    lp.phi = xy.y + P->phi0;

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


PJ *PROJECTION(eqc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if ((Q->rc = cos(pj_param(P->ctx, P->params, "rlat_ts").f)) <= 0.) E_ERROR(-24);
    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_eqc_selftest (void) {return 0;}
#else

int pj_eqc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eqc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223402.144255274179,  111701.07212763709},
        { 223402.144255274179, -111701.07212763709},
        {-223402.144255274179,  111701.07212763709},
        {-223402.144255274179, -111701.07212763709},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049310978382265,  0.000895246554891911323},
        { 0.00179049310978382265, -0.000895246554891911323},
        {-0.00179049310978382265,  0.000895246554891911323},
        {-0.00179049310978382265, -0.000895246554891911323},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
