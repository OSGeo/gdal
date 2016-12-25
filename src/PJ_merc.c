#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(merc, "Mercator") "\n\tCyl, Sph&Ell\n\tlat_ts=";

#define EPS10 1.e-10

static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10)
        F_ERROR;
    xy.x = P->k0 * lp.lam;
    xy.y = - P->k0 * log(pj_tsfn(lp.phi, sin(lp.phi), P->e));
    return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10)
        F_ERROR;
    xy.x = P->k0 * lp.lam;
    xy.y = P->k0 * log(tan(M_FORTPI + .5 * lp.phi));
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    if ((lp.phi = pj_phi2(P->ctx, exp(- xy.y / P->k0), P->e)) == HUGE_VAL)
        I_ERROR;
    lp.lam = xy.x / P->k0;
    return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    lp.phi = M_HALFPI - 2. * atan(exp(-xy.y / P->k0));
    lp.lam = xy.x / P->k0;
    return lp;
}


static void freeup(PJ *P) {                             /* Destructor */
    pj_dealloc(P);
}


PJ *PROJECTION(merc) {
    double phits=0.0;
    int is_phits;

    if( (is_phits = pj_param(P->ctx, P->params, "tlat_ts").i) ) {
        phits = fabs(pj_param(P->ctx, P->params, "rlat_ts").f);
        if (phits >= M_HALFPI) E_ERROR(-24);
    }

    if (P->es) { /* ellipsoid */
        if (is_phits)
            P->k0 = pj_msfn(sin(phits), cos(phits), P->es);
        P->inv = e_inverse;
        P->fwd = e_forward;
    }

    else { /* sphere */
        if (is_phits)
            P->k0 = cos(phits);
        P->inv = s_inverse;
        P->fwd = s_forward;
    }

    return P;
}


#ifndef PJ_SELFTEST
int pj_merc_selftest (void) {return 0;}
#else

int pj_merc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=merc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=merc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222638.981586547132,  110579.965218249708},
        { 222638.981586547132, -110579.965218249112},
        {-222638.981586547132,  110579.965218249708},
        {-222638.981586547132, -110579.965218249112},
    };

    XY s_fwd_expect[] = {
        { 223402.144255274179,  111706.743574944077},
        { 223402.144255274179, -111706.743574944485},
        {-223402.144255274179,  111706.743574944077},
        {-223402.144255274179, -111706.743574944485},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179663056823904264,  0.00090436947522799056},
        { 0.00179663056823904264, -0.00090436947522799056},
        {-0.00179663056823904264,  0.00090436947522799056},
        {-0.00179663056823904264,  -0.00090436947522799056},
    };

    LP s_inv_expect[] = {
        { 0.00179049310978382265,  0.000895246554845297135},
        { 0.00179049310978382265, -0.000895246554858019272},
        {-0.00179049310978382265,  0.000895246554845297135},
        {-0.00179049310978382265, -0.000895246554858019272},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
