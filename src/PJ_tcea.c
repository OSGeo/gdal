#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(tcea, "Transverse Cylindrical Equal Area") "\n\tCyl, Sph";


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    xy.x = cos (lp.phi) * sin (lp.lam) / P->k0;
    xy.y = P->k0 * (atan2 (tan (lp.phi), cos (lp.lam)) - P->phi0);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0, 0.0};
    double t;

    xy.y = xy.y / P->k0 + P->phi0;
    xy.x *= P->k0;
    t = sqrt (1. - xy.x * xy.x);
    lp.phi = asin (t * sin (xy.y));
    lp.lam = atan2 (xy.x, t * cos (xy.y));
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(tcea) {
    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;
    return P;
}


#ifndef PJ_SELFTEST
int pj_tcea_selftest (void) {return 0;}
#else
int pj_tcea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=tcea   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223322.76057672748,  111769.14504058579},
        { 223322.76057672748, -111769.14504058579},
        {-223322.76057672748,  111769.14504058579},
        {-223322.76057672748, -111769.14504058579},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931102938101,  0.00089524655445477922},
        { 0.0017904931102938101, -0.00089524655445477922},
        {-0.0017904931102938101,  0.00089524655445477922},
        {-0.0017904931102938101, -0.00089524655445477922},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}
#endif
