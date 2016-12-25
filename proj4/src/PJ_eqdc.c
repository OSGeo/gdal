#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double phi1;
    double phi2;
    double n;
    double rho;
    double rho0;
    double c;
    double *en;
    int     ellips;
};

PROJ_HEAD(eqdc, "Equidistant Conic")
    "\n\tConic, Sph&Ell\n\tlat_1= lat_2=";
# define EPS10  1.e-10


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    Q->rho = Q->c - (Q->ellips ? pj_mlfn(lp.phi, sin(lp.phi),
        cos(lp.phi), Q->en) : lp.phi);
    xy.x = Q->rho * sin( lp.lam *= Q->n );
    xy.y = Q->rho0 - Q->rho * cos(lp.lam);

    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    if ((Q->rho = hypot(xy.x, xy.y = Q->rho0 - xy.y)) != 0.0 ) {
        if (Q->n < 0.) {
            Q->rho = -Q->rho;
            xy.x = -xy.x;
            xy.y = -xy.y;
        }
        lp.phi = Q->c - Q->rho;
        if (Q->ellips)
            lp.phi = pj_inv_mlfn(P->ctx, lp.phi, P->es, Q->en);
        lp.lam = atan2(xy.x, xy.y) / Q->n;
    } else {
        lp.lam = 0.;
        lp.phi = Q->n > 0. ? M_HALFPI : -M_HALFPI;
    }
    return lp;
}


static void special(LP lp, PJ *P, struct FACTORS *fac) {
    struct pj_opaque *Q = P->opaque;
    double sinphi, cosphi;

    sinphi = sin(lp.phi);
    cosphi = cos(lp.phi);
    fac->code |= IS_ANAL_HK;
    fac->h = 1.;
    fac->k = Q->n * (Q->c - (Q->ellips ? pj_mlfn(lp.phi, sinphi,
        cosphi, Q->en) : lp.phi)) / pj_msfn(sinphi, cosphi, P->es);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    if (P->opaque->en)
        pj_dealloc (P->opaque->en);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(eqdc) {
    double cosphi, sinphi;
    int secant;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
    Q->phi2 = pj_param(P->ctx, P->params, "rlat_2").f;
    if (fabs(Q->phi1 + Q->phi2) < EPS10) E_ERROR(-21);
    if (!(Q->en = pj_enfn(P->es)))
        E_ERROR_0;
    Q->n = sinphi = sin(Q->phi1);
    cosphi = cos(Q->phi1);
    secant = fabs(Q->phi1 - Q->phi2) >= EPS10;
    if( (Q->ellips = (P->es > 0.)) ) {
        double ml1, m1;

        m1 = pj_msfn(sinphi, cosphi, P->es);
        ml1 = pj_mlfn(Q->phi1, sinphi, cosphi, Q->en);
        if (secant) { /* secant cone */
            sinphi = sin(Q->phi2);
            cosphi = cos(Q->phi2);
            Q->n = (m1 - pj_msfn(sinphi, cosphi, P->es)) /
                (pj_mlfn(Q->phi2, sinphi, cosphi, Q->en) - ml1);
        }
        Q->c = ml1 + m1 / Q->n;
        Q->rho0 = Q->c - pj_mlfn(P->phi0, sin(P->phi0),
            cos(P->phi0), Q->en);
    } else {
        if (secant)
            Q->n = (cosphi - cos(Q->phi2)) / (Q->phi2 - Q->phi1);
        Q->c = Q->phi1 + cos(Q->phi1) / Q->n;
        Q->rho0 = Q->c - P->phi0;
    }
    P->inv = e_inverse;
    P->fwd = e_forward;
    P->spc = special;

    return P;
}


#ifndef PJ_SELFTEST
int pj_eqdc_selftest (void) {return 0;}
#else

int pj_eqdc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=eqdc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=eqdc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222588.440269285755,  110659.134907347048},
        { 222756.836702042434, -110489.578087220681},
        {-222588.440269285755,  110659.134907347048},
        {-222756.836702042434, -110489.578087220681},
    };

    XY s_fwd_expect[] = {
        { 223351.088175113517,  111786.108747173785},
        { 223521.200266735133, -111615.970741240744},
        {-223351.088175113517,  111786.108747173785},
        {-223521.200266735133, -111615.970741240744},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179635944879094839,  0.000904368858588402644},
        { 0.00179635822020772734, -0.000904370095529954975},
        {-0.00179635944879094839,  0.000904368858588402644},
        {-0.00179635822020772734, -0.000904370095529954975},
    };

    LP s_inv_expect[] = {
        { 0.0017902210900486641,   0.000895245944814909169},
        { 0.00179021986984890255, -0.000895247165333684842},
        {-0.0017902210900486641,   0.000895245944814909169},
        {-0.00179021986984890255, -0.000895247165333684842},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
