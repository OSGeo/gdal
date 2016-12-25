#define PJ_LIB__
#include <projects.h>
#include <string.h>

struct pj_opaque {
    struct PJconsts *link;
    double  lamp;
    double  cphip, sphip;
};

PROJ_HEAD(ob_tran, "General Oblique Transformation") "\n\tMisc Sph"
"\n\to_proj= plus parameters for projection"
"\n\to_lat_p= o_lon_p= (new pole) or"
"\n\to_alpha= o_lon_c= o_lat_c= or"
"\n\to_lon_1= o_lat_1= o_lon_2= o_lat_2=";

#define TOL 1e-10


static XY o_forward(LP lp, PJ *P) {             /* spheroid */
    struct pj_opaque *Q = P->opaque;
    double coslam, sinphi, cosphi;

    coslam = cos(lp.lam);
    sinphi = sin(lp.phi);
    cosphi = cos(lp.phi);
    lp.lam = adjlon(aatan2(cosphi * sin(lp.lam), Q->sphip * cosphi * coslam +
        Q->cphip * sinphi) + Q->lamp);
    lp.phi = aasin(P->ctx,Q->sphip * sinphi - Q->cphip * cosphi * coslam);

    return Q->link->fwd(lp, Q->link);
}


static XY t_forward(LP lp, PJ *P) {             /* spheroid */
    struct pj_opaque *Q = P->opaque;
    double cosphi, coslam;

    cosphi = cos(lp.phi);
    coslam = cos(lp.lam);
    lp.lam = adjlon(aatan2(cosphi * sin(lp.lam), sin(lp.phi)) + Q->lamp);
    lp.phi = aasin(P->ctx, - cosphi * coslam);

    return Q->link->fwd(lp, Q->link);
}


static LP o_inverse(XY xy, PJ *P) {             /* spheroid */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double coslam, sinphi, cosphi;

    lp = Q->link->inv(xy, Q->link);
    if (lp.lam != HUGE_VAL) {
        coslam = cos(lp.lam -= Q->lamp);
        sinphi = sin(lp.phi);
        cosphi = cos(lp.phi);
        lp.phi = aasin(P->ctx,Q->sphip * sinphi + Q->cphip * cosphi * coslam);
        lp.lam = aatan2(cosphi * sin(lp.lam), Q->sphip * cosphi * coslam -
            Q->cphip * sinphi);
    }
    return lp;
}


static LP t_inverse(XY xy, PJ *P) {             /* spheroid */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double cosphi, t;

    lp = Q->link->inv(xy, Q->link);
    if (lp.lam != HUGE_VAL) {
        cosphi = cos(lp.phi);
        t = lp.lam - Q->lamp;
        lp.lam = aatan2(cosphi * sin(t), - sin(lp.phi));
        lp.phi = aasin(P->ctx,cosphi * cos(t));
    }
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    if (P->opaque->link)
        return pj_dealloc (P->opaque->link);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(ob_tran) {
    int i;
    double phip;
    char *name, *s;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    /* get name of projection to be translated */
    if (!(name = pj_param(P->ctx, P->params, "so_proj").s)) E_ERROR(-26);
    for (i = 0; (s = pj_list[i].id) && strcmp(name, s) ; ++i) ;
    if (!s || !(Q->link = (*pj_list[i].proj)(0))) E_ERROR(-37);
    /* copy existing header into new */
    P->es = 0.; /* force to spherical */
    Q->link->params = P->params;
    Q->link->ctx = P->ctx;
    Q->link->over = P->over;
    Q->link->geoc = P->geoc;
    Q->link->a = P->a;
    Q->link->es = P->es;
    Q->link->ra = P->ra;
    Q->link->lam0 = P->lam0;
    Q->link->phi0 = P->phi0;
    Q->link->x0 = P->x0;
    Q->link->y0 = P->y0;
    Q->link->k0 = P->k0;
    /* force spherical earth */
    Q->link->one_es = Q->link->rone_es = 1.;
    Q->link->es = Q->link->e = 0.;
    if (!(Q->link = pj_list[i].proj(Q->link))) {
        return freeup_new(P);
    }
    if (pj_param(P->ctx, P->params, "to_alpha").i) {
        double lamc, phic, alpha;

        lamc    = pj_param(P->ctx, P->params, "ro_lon_c").f;
        phic    = pj_param(P->ctx, P->params, "ro_lat_c").f;
        alpha   = pj_param(P->ctx, P->params, "ro_alpha").f;
/*
        if (fabs(phic) <= TOL ||
            fabs(fabs(phic) - HALFPI) <= TOL ||
            fabs(fabs(alpha) - HALFPI) <= TOL)
*/
        if (fabs(fabs(phic) - M_HALFPI) <= TOL)
            E_ERROR(-32);
        Q->lamp = lamc + aatan2(-cos(alpha), -sin(alpha) * sin(phic));
        phip = aasin(P->ctx,cos(phic) * sin(alpha));
    } else if (pj_param(P->ctx, P->params, "to_lat_p").i) { /* specified new pole */
        Q->lamp = pj_param(P->ctx, P->params, "ro_lon_p").f;
        phip = pj_param(P->ctx, P->params, "ro_lat_p").f;
    } else { /* specified new "equator" points */
        double lam1, lam2, phi1, phi2, con;

        lam1 = pj_param(P->ctx, P->params, "ro_lon_1").f;
        phi1 = pj_param(P->ctx, P->params, "ro_lat_1").f;
        lam2 = pj_param(P->ctx, P->params, "ro_lon_2").f;
        phi2 = pj_param(P->ctx, P->params, "ro_lat_2").f;
        if (fabs(phi1 - phi2) <= TOL ||
            (con = fabs(phi1)) <= TOL ||
            fabs(con - M_HALFPI) <= TOL ||
            fabs(fabs(phi2) - M_HALFPI) <= TOL) E_ERROR(-33);
        Q->lamp = atan2(cos(phi1) * sin(phi2) * cos(lam1) -
            sin(phi1) * cos(phi2) * cos(lam2),
            sin(phi1) * cos(phi2) * sin(lam2) -
            cos(phi1) * sin(phi2) * sin(lam1));
        phip = atan(-cos(Q->lamp - lam1) / tan(phi1));
    }
    if (fabs(phip) > TOL) { /* oblique */
        Q->cphip = cos(phip);
        Q->sphip = sin(phip);
        P->fwd = o_forward;
        P->inv = Q->link->inv ? o_inverse : 0;
    } else { /* transverse */
        P->fwd = t_forward;
        P->inv = Q->link->inv ? t_inverse : 0;
    }

    return P;
}

#ifndef PJ_SELFTEST
int pj_ob_tran_selftest (void) {return 0;}
#else

int pj_ob_tran_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=ob_tran +a=6400000 +o_proj=latlon +o_lon_p=20 +o_lat_p=20 +lon_0=180"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {-2.6856872138416592, 1.2374302350496296},
        {-2.6954069748943286, 1.2026833954513816},
        {-2.8993663925401947, 1.2374302350496296},
        {-2.8896466314875244, 1.2026833954513816},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 121.5518748407577,  -2.5361001573966084},
        { 63.261184340201858,  17.585319578673531},
        {-141.10073322351622,  26.091712304855108},
        {-65.862385598848391,  51.830295078417215},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif
