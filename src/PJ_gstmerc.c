#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(gstmerc, "Gauss-Schreiber Transverse Mercator (aka Gauss-Laborde Reunion)")
    "\n\tCyl, Sph&Ell\n\tlat_0= lon_0= k_0=";

struct pj_opaque {
    double lamc;
    double phic;
    double c;
    double n1;
    double n2;
    double XS;
    double YS;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double L, Ls, sinLs1, Ls1;

    L = Q->n1*lp.lam;
    Ls = Q->c + Q->n1 * log(pj_tsfn(-1.0 * lp.phi, -1.0 * sin(lp.phi), P->e));
    sinLs1 = sin(L) / cosh(Ls);
    Ls1 = log(pj_tsfn(-1.0 * asin(sinLs1), 0.0, 0.0));
    xy.x = (Q->XS + Q->n2*Ls1) * P->ra;
    xy.y = (Q->YS + Q->n2*atan(sinh(Ls) / cos(L))) * P->ra;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double L, LC, sinC;

    L = atan(sinh((xy.x * P->a - Q->XS) / Q->n2) / cos((xy.y * P->a - Q->YS) / Q->n2));
    sinC = sin((xy.y * P->a - Q->YS) / Q->n2) / cosh((xy.x * P->a - Q->XS) / Q->n2);
    LC = log(pj_tsfn(-1.0 * asin(sinC), 0.0, 0.0));
    lp.lam = L / Q->n1;
    lp.phi = -1.0 * pj_phi2(P->ctx, exp((LC - Q->c) / Q->n1), P->e);

    return lp;
}


static void *freeup_new (PJ *P) {              /* Destructor */
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


PJ *PROJECTION(gstmerc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->lamc = P->lam0;
    Q->n1 = sqrt(1.0 + P->es * pow(cos(P->phi0), 4.0) / (1.0 - P->es));
    Q->phic = asin(sin(P->phi0) / Q->n1);
    Q->c = log(pj_tsfn(-1.0 * Q->phic, 0.0, 0.0))
         - Q->n1 * log(pj_tsfn(-1.0 * P->phi0, -1.0 * sin(P->phi0), P->e));
    Q->n2 = P->k0 * P->a * sqrt(1.0 - P->es) / (1.0 - P->es * sin(P->phi0) * sin(P->phi0));
    Q->XS = 0;
    Q->YS = -1.0 * Q->n2 * Q->phic;

    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_gstmerc_selftest (void) {return 0;}
#else

int pj_gstmerc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=gstmerc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };


    XY s_fwd_expect[] = {
        { 223413.46640632182,  111769.14504058557},
        { 223413.46640632182, -111769.14504058668},
        {-223413.46640632302,  111769.14504058557},
        {-223413.46640632302, -111769.14504058668},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931097109673,  0.0008952465544509083},
        { 0.0017904931097109673, -0.0008952465544509083},
        {-0.0017904931097109673,  0.0008952465544509083},
        {-0.0017904931097109673, -0.0008952465544509083},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
