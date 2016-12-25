#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(lagrng, "Lagrange") "\n\tMisc Sph, no inv.\n\tW=";

#define TOL 1e-10

struct pj_opaque {
    double  a1;
    double  hrw;
    double  rw;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double v, c;

    if (fabs(fabs(lp.phi) - M_HALFPI) < TOL) {
        xy.x = 0;
        xy.y = lp.phi < 0 ? -2. : 2.;
    } else {
        lp.phi = sin(lp.phi);
        v = Q->a1 * pow((1. + lp.phi)/(1. - lp.phi), Q->hrw);
        if ((c = 0.5 * (v + 1./v) + cos(lp.lam *= Q->rw)) < TOL)
            F_ERROR;
        xy.x = 2. * sin(lp.lam) / c;
        xy.y = (v - 1./v) / c;
    }
    return xy;
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


PJ *PROJECTION(lagrng) {
    double phi1;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->rw = pj_param(P->ctx, P->params, "dW").f;
    if (Q->rw <= 0) E_ERROR(-27);

    Q->rw = 1. / Q->rw;
    Q->hrw = 0.5 * Q->rw;
    phi1 = sin(pj_param(P->ctx, P->params, "rlat_1").f);
    if (fabs(fabs(phi1) - 1.) < TOL) E_ERROR(-22);

    Q->a1 = pow((1. - phi1)/(1. + phi1), Q->hrw);

    P->es = 0.;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_lagrng_selftest (void) {return 0;}
#else

int pj_lagrng_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=lagrng   +a=6400000 +W=2   +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 111703.37591722561,   27929.8319080333386},
        { 111699.122088816002, -83784.1780133577704},
        {-111703.37591722561,   27929.8319080333386},
        {-111699.122088816002, -83784.1780133577704},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
