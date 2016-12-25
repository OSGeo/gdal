#define PJ_LIB__
#include   <projects.h>

struct pj_opaque {
    double qp;
    double *apa;
};

PROJ_HEAD(cea, "Equal Area Cylindrical") "\n\tCyl, Sph&Ell\n\tlat_ts=";
# define EPS    1e-10


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    xy.x = P->k0 * lp.lam;
    xy.y = 0.5 * pj_qsfn (sin (lp.phi), P->e, P->one_es) / P->k0;
    return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    xy.x = P->k0 * lp.lam;
    xy.y = sin(lp.phi) / P->k0;
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    lp.phi = pj_authlat(asin( 2. * xy.y * P->k0 / P->opaque->qp), P->opaque->apa);
    lp.lam = xy.x / P->k0;
    return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double t;

    if ((t = fabs(xy.y *= P->k0)) - EPS <= 1.) {
        if (t >= 1.)
            lp.phi = xy.y < 0. ? -M_HALFPI : M_HALFPI;
        else
            lp.phi = asin(xy.y);
        lp.lam = xy.x / P->k0;
    } else I_ERROR;
    return (lp);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque->apa);
    pj_dealloc (P->opaque);
    return pj_dealloc (P);
}

static void freeup (PJ *P) {
   freeup_new (P);
    return;
}

PJ *PROJECTION(cea) {
    double t = 0.0;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;


    if (pj_param(P->ctx, P->params, "tlat_ts").i) {
        P->k0 = cos(t = pj_param(P->ctx, P->params, "rlat_ts").f);
        if (P->k0 < 0.) {
            E_ERROR(-24);
        }
    }
    if (P->es) {
        t = sin(t);
        P->k0 /= sqrt(1. - P->es * t * t);
        P->e = sqrt(P->es);
        if (!(Q->apa = pj_authset(P->es))) E_ERROR_0;
        Q->qp = pj_qsfn(1., P->e, P->one_es);
        P->inv = e_inverse;
        P->fwd = e_forward;
    } else {
        P->inv = s_inverse;
        P->fwd = s_forward;
    }

    return P;
}


#ifndef PJ_SELFTEST
int pj_cea_selftest (void) {return 0;}
#else

int pj_cea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=cea   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=cea   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222638.981586547132,  110568.812396267356},
        { 222638.981586547132, -110568.812396265886},
        {-222638.981586547132,  110568.812396267356},
        {-222638.981586547132, -110568.812396265886},
    };

    XY s_fwd_expect[] = {
        { 223402.144255274179,  111695.401198614476},
        { 223402.144255274179, -111695.401198614476},
        {-223402.144255274179,  111695.401198614476},
        {-223402.144255274179, -111695.401198614476},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179663056823904264,  0.000904369476105564289},
        { 0.00179663056823904264, -0.000904369476105564289},
        {-0.00179663056823904264,  0.000904369476105564289},
        {-0.00179663056823904264, -0.000904369476105564289},
    };

    LP s_inv_expect[] = {
        { 0.00179049310978382265,  0.000895246554928338998},
        { 0.00179049310978382265, -0.000895246554928338998},
        {-0.00179049310978382265,  0.000895246554928338998},
        {-0.00179049310978382265, -0.000895246554928338998},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}

#endif
