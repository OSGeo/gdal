#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(loxim, "Loximuthal") "\n\tPCyl Sph";

#define EPS 1e-8

struct pj_opaque {
    double phi1;
    double cosphi1;
    double tanphi1;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    xy.y = lp.phi - Q->phi1;
    if (fabs(xy.y) < EPS)
        xy.x = lp.lam * Q->cosphi1;
    else {
        xy.x = M_FORTPI + 0.5 * lp.phi;
        if (fabs(xy.x) < EPS || fabs(fabs(xy.x) - M_HALFPI) < EPS)
            xy.x = 0.;
        else
            xy.x = lp.lam * xy.y / log( tan(xy.x) / Q->tanphi1 );
    }
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.phi = xy.y + Q->phi1;
    if (fabs(xy.y) < EPS) {
        lp.lam = xy.x / Q->cosphi1;
    } else {
        lp.lam = M_FORTPI + 0.5 * lp.phi;
        if (fabs(lp.lam) < EPS || fabs(fabs(lp.lam) - M_HALFPI) < EPS)
            lp.lam = 0.;
        else
            lp.lam = xy.x * log( tan(lp.lam) / Q->tanphi1 ) / xy.y ;
    }
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


PJ *PROJECTION(loxim) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
    Q->cosphi1 = cos(Q->phi1);
    if (Q->cosphi1 < EPS)
        E_ERROR(-22);

    Q->tanphi1 = tan(M_FORTPI + 0.5 * Q->phi1);

    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;

   return P;
}


#ifndef PJ_SELFTEST
int pj_loxim_selftest (void) {return 0;}
#else

int pj_loxim_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=loxim   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223382.295791338867,  55850.5360638185448},
        { 223393.637462243292, -167551.608191455656},
        {-223382.295791338867,  55850.5360638185448},
        {-223393.637462243292, -167551.608191455656},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179056141104335601, 0.500895246554891926},
        { 0.00179056116683692576, 0.499104753445108074},
        {-0.00179056141104335601, 0.500895246554891926},
        {-0.00179056116683692576, 0.499104753445108074},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
