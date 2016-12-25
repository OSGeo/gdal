/* PROJ.4 Cartographic Projection System
*/
#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(lcca, "Lambert Conformal Conic Alternative")
    "\n\tConic, Sph&Ell\n\tlat_0=";

#define MAX_ITER 10
#define DEL_TOL 1e-12

struct pj_opaque {
    double *en;
    double r0, l, M0;
    double C;
};


static double fS(double S, double C) {        /* func to compute dr */

    return S * ( 1. + S * S * C);
}


static double fSp(double S, double C) {       /* deriv of fs */

    return 1. + 3.* S * S * C;
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double S, r, dr;

    S = pj_mlfn(lp.phi, sin(lp.phi), cos(lp.phi), Q->en) - Q->M0;
    dr = fS(S, Q->C);
    r = Q->r0 - dr;
    xy.x = P->k0 * (r * sin( lp.lam *= Q->l ) );
    xy.y = P->k0 * (Q->r0 - r * cos(lp.lam) );
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double theta, dr, S, dif;
    int i;

    xy.x /= P->k0;
    xy.y /= P->k0;
    theta = atan2(xy.x , Q->r0 - xy.y);
    dr = xy.y - xy.x * tan(0.5 * theta);
    lp.lam = theta / Q->l;
    S = dr;
    for (i = MAX_ITER; i ; --i) {
        S -= (dif = (fS(S, Q->C) - dr) / fSp(S, Q->C));
        if (fabs(dif) < DEL_TOL) break;
    }
    if (!i) I_ERROR
    lp.phi = pj_inv_mlfn(P->ctx, S + Q->M0, P->es, Q->en);

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque->en);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(lcca) {
    double s2p0, N0, R0, tan0;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    (Q->en = pj_enfn(P->es));
    if (!Q->en) E_ERROR_0;
    if (!pj_param(P->ctx, P->params, "tlat_0").i) E_ERROR(50);
    if (P->phi0 == 0.) E_ERROR(51);
    Q->l = sin(P->phi0);
    Q->M0 = pj_mlfn(P->phi0, Q->l, cos(P->phi0), Q->en);
    s2p0 = Q->l * Q->l;
    R0 = 1. / (1. - P->es * s2p0);
    N0 = sqrt(R0);
    R0 *= P->one_es * N0;
    tan0 = tan(P->phi0);
    Q->r0 = N0 / tan0;
    Q->C = 1. / (6. * R0 * N0);

    P->inv = e_inverse;
    P->fwd = e_forward;

    return P;
}



#ifndef PJ_SELFTEST
int pj_lcca_selftest (void) {return 0;}
#else

int pj_lcca_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=lcca   +ellps=GRS80  +lat_0=1 +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222605.285770237417,  67.8060072715846616},
        { 222740.037637936533, -221125.539829601563},
        {-222605.285770237417,  67.8060072715846616},
        {-222740.037637936533, -221125.539829601563},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179690290525662526, 1.00090436621350798},
        { 0.00179690192174008037, 0.999095632791497268},
        {-0.00179690290525662526, 1.00090436621350798},
        {-0.00179690192174008037, 0.999095632791497268},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
