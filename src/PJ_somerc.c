#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(somerc, "Swiss. Obl. Mercator") "\n\tCyl, Ell\n\tFor CH1903";

struct pj_opaque {
    double K, c, hlf_e, kR, cosp0, sinp0;
};

#define EPS 1.e-10
#define NITER 6


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0, 0.0};
    double phip, lamp, phipp, lampp, sp, cp;
    struct pj_opaque *Q = P->opaque;

    sp = P->e * sin (lp.phi);
    phip = 2.* atan ( exp ( Q->c * (
        log (tan (M_FORTPI + 0.5 * lp.phi)) - Q->hlf_e * log ((1. + sp)/(1. - sp)))
        + Q->K)) - M_HALFPI;
    lamp = Q->c * lp.lam;
    cp = cos(phip);
    phipp = aasin (P->ctx, Q->cosp0 * sin (phip) - Q->sinp0 * cp * cos (lamp));
    lampp = aasin (P->ctx, cp * sin (lamp) / cos (phipp));
    xy.x = Q->kR * lampp;
    xy.y = Q->kR * log (tan (M_FORTPI + 0.5 * phipp));
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double phip, lamp, phipp, lampp, cp, esp, con, delp;
    int i;

    phipp = 2. * (atan (exp (xy.y / Q->kR)) - M_FORTPI);
    lampp = xy.x / Q->kR;
    cp = cos (phipp);
    phip = aasin (P->ctx, Q->cosp0 * sin (phipp) + Q->sinp0 * cp * cos (lampp));
    lamp = aasin (P->ctx, cp * sin (lampp) / cos (phip));
    con = (Q->K - log (tan (M_FORTPI + 0.5 * phip)))/Q->c;
    for (i = NITER; i ; --i) {
        esp = P->e * sin(phip);
        delp = (con + log(tan(M_FORTPI + 0.5 * phip)) - Q->hlf_e *
            log((1. + esp)/(1. - esp)) ) *
            (1. - esp * esp) * cos(phip) * P->rone_es;
        phip -= delp;
        if (fabs(delp) < EPS)
            break;
    }
    if (i) {
        lp.phi = phip;
        lp.lam = lamp / Q->c;
    } else
        I_ERROR
    return (lp);
}


#if 0
FREEUP; if (P) pj_dalloc(P); }
#endif


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


PJ *PROJECTION(somerc) {
    double cp, phip0, sp;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;


    Q->hlf_e = 0.5 * P->e;
    cp = cos (P->phi0);
    cp *= cp;
    Q->c = sqrt (1 + P->es * cp * cp * P->rone_es);
    sp = sin (P->phi0);
    Q->cosp0 = cos( phip0 = aasin (P->ctx, Q->sinp0 = sp / Q->c) );
    sp *= P->e;
    Q->K = log (tan (M_FORTPI + 0.5 * phip0)) - Q->c * (
        log (tan (M_FORTPI + 0.5 * P->phi0)) - Q->hlf_e *
        log ((1. + sp) / (1. - sp)));
    Q->kR = P->k0 * sqrt(P->one_es) / (1. - sp * sp);
    P->inv = e_inverse;
    P->fwd = e_forward;
    return P;
}


#ifndef PJ_SELFTEST
int pj_somerc_selftest (void) {return 0;}
#else

int pj_somerc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=somerc   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=somerc   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222638.98158654713,  110579.96521824898},
        {222638.98158654713,  -110579.96521825089},
        {-222638.98158654713,  110579.96521824898},
        {-222638.98158654713,  -110579.96521825089},
    };

    XY s_fwd_expect[] = {
        {223402.14425527418,  111706.74357494408},
        {223402.14425527418,  -111706.74357494518},
        {-223402.14425527418,  111706.74357494408},
        {-223402.14425527418,  -111706.74357494518},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966305682390426,  0.00090436947704129484},
        {0.0017966305682390426,  -0.00090436947704377105},
        {-0.0017966305682390426,  0.00090436947704129484},
        {-0.0017966305682390426,  -0.00090436947704377105},
    };

    LP s_inv_expect[] = {
        {0.0017904931097838226,  0.00089524655485801927},
        {0.0017904931097838226,  -0.00089524655484529714},
        {-0.0017904931097838226,  0.00089524655485801927},
        {-0.0017904931097838226,  -0.00089524655484529714},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
