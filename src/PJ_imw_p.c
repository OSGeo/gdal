#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(imw_p, "International Map of the World Polyconic")
    "\n\tMod. Polyconic, Ell\n\tlat_1= and lat_2= [lon_1=]";

#define TOL 1e-10
#define EPS 1e-10

struct pj_opaque {
    double  P, Pp, Q, Qp, R_1, R_2, sphi_1, sphi_2, C2; \
    double  phi_1, phi_2, lam_1; \
    double  *en; \
    int mode; /* = 0, phi_1 and phi_2 != 0, = 1, phi_1 = 0, = -1 phi_2 = 0 */
};


static int phi12(PJ *P, double *del, double *sig) {
    struct pj_opaque *Q = P->opaque;
    int err = 0;

    if (!pj_param(P->ctx, P->params, "tlat_1").i ||
        !pj_param(P->ctx, P->params, "tlat_2").i) {
        err = -41;
    } else {
        Q->phi_1 = pj_param(P->ctx, P->params, "rlat_1").f;
        Q->phi_2 = pj_param(P->ctx, P->params, "rlat_2").f;
        *del = 0.5 * (Q->phi_2 - Q->phi_1);
        *sig = 0.5 * (Q->phi_2 + Q->phi_1);
        err = (fabs(*del) < EPS || fabs(*sig) < EPS) ? -42 : 0;
    }
    return err;
}


static XY loc_for(LP lp, PJ *P, double *yc) {
    struct pj_opaque *Q = P->opaque;
    XY xy;

    if (! lp.phi) {
        xy.x = lp.lam;
        xy.y = 0.;
    } else {
        double xa, ya, xb, yb, xc, D, B, m, sp, t, R, C;

        sp = sin(lp.phi);
        m = pj_mlfn(lp.phi, sp, cos(lp.phi), Q->en);
        xa = Q->Pp + Q->Qp * m;
        ya = Q->P + Q->Q * m;
        R = 1. / (tan(lp.phi) * sqrt(1. - P->es * sp * sp));
        C = sqrt(R * R - xa * xa);
        if (lp.phi < 0.) C = - C;
        C += ya - R;
        if (Q->mode < 0) {
            xb = lp.lam;
            yb = Q->C2;
        } else {
            t = lp.lam * Q->sphi_2;
            xb = Q->R_2 * sin(t);
            yb = Q->C2 + Q->R_2 * (1. - cos(t));
        }
        if (Q->mode > 0) {
            xc = lp.lam;
            *yc = 0.;
        } else {
            t = lp.lam * Q->sphi_1;
            xc = Q->R_1 * sin(t);
            *yc = Q->R_1 * (1. - cos(t));
        }
        D = (xb - xc)/(yb - *yc);
        B = xc + D * (C + R - *yc);
        xy.x = D * sqrt(R * R * (1 + D * D) - B * B);
        if (lp.phi > 0)
            xy.x = - xy.x;
        xy.x = (B + xy.x) / (1. + D * D);
        xy.y = sqrt(R * R - xy.x * xy.x);
        if (lp.phi > 0)
            xy.y = - xy.y;
        xy.y += C + R;
    }
    return xy;
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    double yc;

    xy = loc_for(lp, P, &yc);
    return (xy);
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    XY t;
    double yc;

    lp.phi = Q->phi_2;
    lp.lam = xy.x / cos(lp.phi);
    do {
        t = loc_for(lp, P, &yc);
        lp.phi = ((lp.phi - Q->phi_1) * (xy.y - yc) / (t.y - yc)) + Q->phi_1;
        lp.lam = lp.lam * xy.x / t.x;
    } while (fabs(t.x - xy.x) > TOL || fabs(t.y - xy.y) > TOL);

    return lp;
}


static void xy(PJ *P, double phi, double *x, double *y, double *sp, double *R) {
    double F;

    *sp = sin(phi);
    *R = 1./(tan(phi) * sqrt(1. - P->es * *sp * *sp ));
    F = P->opaque->lam_1 * *sp;
    *y = *R * (1 - cos(F));
    *x = *R * sin(F);
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


PJ *PROJECTION(imw_p) {
    double del, sig, s, t, x1, x2, T2, y1, m1, m2, y2;
    int i;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (!(Q->en = pj_enfn(P->es))) E_ERROR_0;
    if( (i = phi12(P, &del, &sig)) != 0)
        E_ERROR(i);
    if (Q->phi_2 < Q->phi_1) { /* make sure P->phi_1 most southerly */
        del = Q->phi_1;
        Q->phi_1 = Q->phi_2;
        Q->phi_2 = del;
    }
    if (pj_param(P->ctx, P->params, "tlon_1").i)
        Q->lam_1 = pj_param(P->ctx, P->params, "rlon_1").f;
    else { /* use predefined based upon latitude */
        sig = fabs(sig * RAD_TO_DEG);
        if (sig <= 60)      sig = 2.;
        else if (sig <= 76) sig = 4.;
        else                sig = 8.;
        Q->lam_1 = sig * DEG_TO_RAD;
    }
    Q->mode = 0;
    if (Q->phi_1) xy(P, Q->phi_1, &x1, &y1, &Q->sphi_1, &Q->R_1);
    else {
        Q->mode = 1;
        y1 = 0.;
        x1 = Q->lam_1;
    }
    if (Q->phi_2) xy(P, Q->phi_2, &x2, &T2, &Q->sphi_2, &Q->R_2);
    else {
        Q->mode = -1;
        T2 = 0.;
        x2 = Q->lam_1;
    }
    m1 = pj_mlfn(Q->phi_1, Q->sphi_1, cos(Q->phi_1), Q->en);
    m2 = pj_mlfn(Q->phi_2, Q->sphi_2, cos(Q->phi_2), Q->en);
    t = m2 - m1;
    s = x2 - x1;
    y2 = sqrt(t * t - s * s) + y1;
    Q->C2 = y2 - T2;
    t = 1. / t;
    Q->P = (m2 * y1 - m1 * y2) * t;
    Q->Q = (y2 - y1) * t;
    Q->Pp = (m2 * x1 - m1 * x2) * t;
    Q->Qp = (x2 - x1) * t;

    P->fwd = e_forward;
    P->inv = e_inverse;

    return P;
}


#ifndef PJ_SELFTEST
int pj_imw_p_selftest (void) {return 0;}
#else

int pj_imw_p_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=imw_p   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222588.4411393762,   55321.128653809537},
        { 222756.90637768712, -165827.58428832365},
        {-222588.4411393762,   55321.128653809537},
        {-222756.90637768712, -165827.58428832365},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966991379592214, 0.50090492361427374},
        { 0.0017966979081574697, 0.49909507588689922},
        {-0.0017966991379592214, 0.50090492361427374},
        {-0.0017966979081574697, 0.49909507588689922},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
