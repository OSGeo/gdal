#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double C_x, C_y, A, B, D;
};

PROJ_HEAD(putp6, "Putnins P6") "\n\tPCyl., Sph.";
PROJ_HEAD(putp6p, "Putnins P6'") "\n\tPCyl., Sph.";

#define EPS      1e-10
#define NITER    10
#define CON_POLE 1.732050807568877


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double p, r, V;
    int i;

    p = Q->B * sin(lp.phi);
    lp.phi *=  1.10265779;
    for (i = NITER; i ; --i) {
        r = sqrt(1. + lp.phi * lp.phi);
        lp.phi -= V = ( (Q->A - r) * lp.phi - log(lp.phi + r) - p ) /
            (Q->A - 2. * r);
        if (fabs(V) < EPS)
            break;
    }
    if (!i)
        lp.phi = p < 0. ? -CON_POLE : CON_POLE;
    xy.x = Q->C_x * lp.lam * (Q->D - sqrt(1. + lp.phi * lp.phi));
    xy.y = Q->C_y * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double r;

    lp.phi = xy.y / Q->C_y;
    r = sqrt(1. + lp.phi * lp.phi);
    lp.lam = xy.x / (Q->C_x * (Q->D - r));
    lp.phi = aasin( P->ctx, ( (Q->A - r) * lp.phi - log(lp.phi + r) ) / Q->B);

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


PJ *PROJECTION(putp6) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 1.01346;
    Q->C_y = 0.91910;
    Q->A   = 4.;
    Q->B   = 2.1471437182129378784;
    Q->D   = 2.;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


PJ *PROJECTION(putp6p) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 0.44329;
    Q->C_y = 0.80404;
    Q->A   = 6.;
    Q->B   = 5.61125;
    Q->D   = 3.;

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_putp6_selftest (void) {return 0;}
#else

int pj_putp6_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp6   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 226369.395133402577,  110218.523796520662},
        { 226369.395133402577, -110218.523796520749},
        {-226369.395133402577,  110218.523796520662},
        {-226369.395133402577, -110218.523796520749},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00176671315102969921,  0.000907295534210503544},
        { 0.00176671315102969921, -0.000907295534205924308},
        {-0.00176671315102969921,  0.000907295534210503544},
        {-0.00176671315102969921, -0.000907295534205924308},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif


#ifndef PJ_SELFTEST
int pj_putp6p_selftest (void) {return 0;}
#else

int pj_putp6p_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp6p   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 198034.195132195076,  125989.475461323193},
        { 198034.195132195076, -125989.475461323193},
        {-198034.195132195076,  125989.475461323193},
        {-198034.195132195076, -125989.475461323193},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00201955053120177067,  0.000793716441164738612},
        { 0.00201955053120177067, -0.000793716441164738612},
        {-0.00201955053120177067,  0.000793716441164738612},
        {-0.00201955053120177067, -0.000793716441164738612},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif
