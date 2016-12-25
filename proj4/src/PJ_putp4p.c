#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double C_x, C_y;
};

PROJ_HEAD(putp4p, "Putnins P4'") "\n\tPCyl., Sph.";
PROJ_HEAD(weren, "Werenskiold I") "\n\tPCyl., Sph.";


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.phi = aasin(P->ctx,0.883883476 * sin(lp.phi));
    xy.x = Q->C_x * lp.lam * cos(lp.phi);
    xy.x /= cos(lp.phi *= 0.333333333333333);
    xy.y = Q->C_y * sin(lp.phi);

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.phi = aasin(P->ctx,xy.y / Q->C_y);
    lp.lam = xy.x * cos(lp.phi) / Q->C_x;
    lp.phi *= 3.;
    lp.lam /= cos(lp.phi);
    lp.phi = aasin(P->ctx,1.13137085 * sin(lp.phi));

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


PJ *PROJECTION(putp4p) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 0.874038744;
    Q->C_y = 3.883251825;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


PJ *PROJECTION(weren) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 1.;
    Q->C_y = 4.442882938;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_putp4p_selftest (void) {return 0;}
#else

int pj_putp4p_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp4p   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 195241.47734938623,  127796.782307926231},
        { 195241.47734938623, -127796.782307926231},
        {-195241.47734938623,  127796.782307926231},
        {-195241.47734938623, -127796.782307926231},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00204852830860296001,  0.000782480174932193733},
        { 0.00204852830860296001, -0.000782480174932193733},
        {-0.00204852830860296001,  0.000782480174932193733},
        {-0.00204852830860296001, -0.000782480174932193733},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif


#ifndef PJ_SELFTEST
int pj_weren_selftest (void) {return 0;}
#else

int pj_weren_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=weren   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223378.515757633519,  146214.093042288267},
        { 223378.515757633519, -146214.093042288267},
        {-223378.515757633519,  146214.093042288267},
        {-223378.515757633519, -146214.093042288267},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049310987240413,  0.000683917989676492265},
        { 0.00179049310987240413, -0.000683917989676492265},
        {-0.00179049310987240413,  0.000683917989676492265},
        {-0.00179049310987240413, -0.000683917989676492265},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
