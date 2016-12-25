#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(fouc_s, "Foucaut Sinusoidal") "\n\tPCyl., Sph.";

#define MAX_ITER    10
#define LOOP_TOL    1e-7

struct pj_opaque {
    double n, n1;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double t;

    t = cos(lp.phi);
    xy.x = lp.lam * t / (Q->n + Q->n1 * t);
    xy.y = Q->n * lp.phi + Q->n1 * sin(lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double V;
    int i;

    if (Q->n) {
        lp.phi = xy.y;
        for (i = MAX_ITER; i ; --i) {
            lp.phi -= V = (Q->n * lp.phi + Q->n1 * sin(lp.phi) - xy.y ) /
                (Q->n + Q->n1 * cos(lp.phi));
            if (fabs(V) < LOOP_TOL)
                break;
        }
        if (!i)
            lp.phi = xy.y < 0. ? -M_HALFPI : M_HALFPI;
    } else
        lp.phi = aasin(P->ctx,xy.y);
    V = cos(lp.phi);
    lp.lam = xy.x * (Q->n + Q->n1 * V) / V;
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


PJ *PROJECTION(fouc_s) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->n = pj_param(P->ctx, P->params, "dn").f;
    if (Q->n < 0. || Q->n > 1.)
        E_ERROR(-99)
    Q->n1 = 1. - Q->n;
    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;
    return P;
}


#ifndef PJ_SELFTEST
int pj_fouc_s_selftest (void) {return 0;}
#else

int pj_fouc_s_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=fouc_s   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223402.14425527424,  111695.40119861449},
        { 223402.14425527424, -111695.40119861449},
        {-223402.14425527424,  111695.40119861449},
        {-223402.14425527424, -111695.40119861449},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931097838226,  0.000895246554928339},
        { 0.0017904931097838226, -0.000895246554928339},
        {-0.0017904931097838226,  0.000895246554928339},
        {-0.0017904931097838226, -0.000895246554928339},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
