#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(ocea, "Oblique Cylindrical Equal Area") "\n\tCyl, Sph"
    "lonc= alpha= or\n\tlat_1= lat_2= lon_1= lon_2=";

struct pj_opaque {
    double  rok;
    double  rtk;
    double  sinphi;
    double  cosphi;
    double  singam;
    double  cosgam;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double t;

    xy.y = sin(lp.lam);
    t = cos(lp.lam);
    xy.x = atan((tan(lp.phi) * Q->cosphi + Q->sinphi * xy.y) / t);
    if (t < 0.)
        xy.x += M_PI;
    xy.x *= Q->rtk;
    xy.y = Q->rok * (Q->sinphi * sin(lp.phi) - Q->cosphi * cos(lp.phi) * xy.y);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double t, s;

    xy.y /= Q->rok;
    xy.x /= Q->rtk;
    t = sqrt(1. - xy.y * xy.y);
    lp.phi = asin(xy.y * Q->sinphi + t * Q->cosphi * (s = sin(xy.x)));
    lp.lam = atan2(t * Q->sinphi * s - xy.y * Q->cosphi,
        t * cos(xy.x));
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


PJ *PROJECTION(ocea) {
    double phi_0=0.0, phi_1, phi_2, lam_1, lam_2, lonz, alpha;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->rok = P->a / P->k0;
    Q->rtk = P->a * P->k0;
    /*If the keyword "alpha" is found in the sentence then use 1point+1azimuth*/
    if ( pj_param(P->ctx, P->params, "talpha").i) {
        /*Define Pole of oblique transformation from 1 point & 1 azimuth*/
        alpha   = pj_param(P->ctx, P->params, "ralpha").f;
        lonz = pj_param(P->ctx, P->params, "rlonc").f;
        /*Equation 9-8 page 80 (http://pubs.usgs.gov/pp/1395/report.pdf)*/
        Q->singam = atan(-cos(alpha)/(-sin(phi_0) * sin(alpha))) + lonz;
        /*Equation 9-7 page 80 (http://pubs.usgs.gov/pp/1395/report.pdf)*/
        Q->sinphi = asin(cos(phi_0) * sin(alpha));
    /*If the keyword "alpha" is NOT found in the sentence then use 2points*/
    } else {
        /*Define Pole of oblique transformation from 2 points*/
        phi_1 = pj_param(P->ctx, P->params, "rlat_1").f;
        phi_2 = pj_param(P->ctx, P->params, "rlat_2").f;
        lam_1 = pj_param(P->ctx, P->params, "rlon_1").f;
        lam_2 = pj_param(P->ctx, P->params, "rlon_2").f;
        /*Equation 9-1 page 80 (http://pubs.usgs.gov/pp/1395/report.pdf)*/
        Q->singam = atan2(cos(phi_1) * sin(phi_2) * cos(lam_1) -
            sin(phi_1) * cos(phi_2) * cos(lam_2),
            sin(phi_1) * cos(phi_2) * sin(lam_2) -
            cos(phi_1) * sin(phi_2) * sin(lam_1) );
        /*Equation 9-2 page 80 (http://pubs.usgs.gov/pp/1395/report.pdf)*/
        Q->sinphi = atan(-cos(Q->singam - lam_1) / tan(phi_1));
    }
    P->lam0 = Q->singam + M_HALFPI;
    Q->cosphi = cos(Q->sinphi);
    Q->sinphi = sin(Q->sinphi);
    Q->cosgam = cos(Q->singam);
    Q->singam = sin(Q->singam);
    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_ocea_selftest (void) {return 0;}
#else

int pj_ocea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=ocea   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {127964312562778.156,  1429265667691.05786},
        {129394957619297.641,  1429265667691.06812},
        {127964312562778.188, -1429265667691.0498},
        {129394957619297.688, -1429265667691.03955},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 179.999999999860108,  2.79764548403721305e-10},
        {-179.999999999860108,  2.7976454840372327e-10},
        { 179.999999999860108, -2.7976454840372327e-10},
        {-179.999999999860108, -2.79764548403721305e-10},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
