#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(hammer, "Hammer & Eckert-Greifendorff")
    "\n\tMisc Sph, \n\tW= M=";

#define EPS 1.0e-10

struct pj_opaque {
    double w; \
    double m, rm;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double cosphi, d;

    d = sqrt(2./(1. + (cosphi = cos(lp.phi)) * cos(lp.lam *= Q->w)));
    xy.x = Q->m * d * cosphi * sin(lp.lam);
    xy.y = Q->rm * d * sin(lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double z;

    z = sqrt(1. - 0.25*Q->w*Q->w*xy.x*xy.x - 0.25*xy.y*xy.y);
    if (fabs(2.*z*z-1.) < EPS) {
        lp.lam = HUGE_VAL;
        lp.phi = HUGE_VAL;
        pj_errno = -14;
    } else {
        lp.lam = aatan2(Q->w * xy.x * z,2. * z * z - 1)/Q->w;
        lp.phi = aasin(P->ctx,z * xy.y);
    }
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


PJ *PROJECTION(hammer) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (pj_param(P->ctx, P->params, "tW").i) {
        if ((Q->w = fabs(pj_param(P->ctx, P->params, "dW").f)) <= 0.) E_ERROR(-27);
    } else
        Q->w = .5;
    if (pj_param(P->ctx, P->params, "tM").i) {
        if ((Q->m = fabs(pj_param(P->ctx, P->params, "dM").f)) <= 0.) E_ERROR(-27);
    } else
        Q->m = 1.;

    Q->rm = 1. / Q->m;
    Q->m /= Q->w;

    P->es = 0.;
    P->fwd = s_forward;
    P->inv = s_inverse;

    return P;
}


#ifndef PJ_SELFTEST
int pj_hammer_selftest (void) {return 0;}
#else

int pj_hammer_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=hammer   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223373.78870324057,  111703.90739776699},
        { 223373.78870324057, -111703.90739776699},
        {-223373.78870324057,  111703.90739776699},
        {-223373.78870324057, -111703.90739776699},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.001790493109965961,  0.00089524655487369749},
        { 0.001790493109965961, -0.00089524655487369749},
        {-0.001790493109965961,  0.00089524655487369749},
        {-0.001790493109965961, -0.00089524655487369749},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
