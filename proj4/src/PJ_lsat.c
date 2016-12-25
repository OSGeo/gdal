/* based upon Snyder and Linck, USGS-NMD */
#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(lsat, "Space oblique for LANDSAT")
    "\n\tCyl, Sph&Ell\n\tlsat= path=";

#define TOL 1e-7

struct pj_opaque {
    double a2, a4, b, c1, c3;
    double q, t, u, w, p22, sa, ca, xj, rlm, rlm2;
};

static void seraz0(double lam, double mult, PJ *P) {
    struct pj_opaque *Q = P->opaque;
    double sdsq, h, s, fc, sd, sq, d__1 = 0;

    lam *= DEG_TO_RAD;
    sd = sin(lam);
    sdsq = sd * sd;
    s = Q->p22 * Q->sa * cos(lam) * sqrt((1. + Q->t * sdsq)
        / ((1. + Q->w * sdsq) * (1. + Q->q * sdsq)));

    h = sqrt((1. + Q->q * sdsq) / (1. + Q->w * sdsq)) * ((1. + Q->w * sdsq)
        / (d__1 * d__1) - Q->p22 * Q->ca);

    sq = sqrt(Q->xj * Q->xj + s * s);
    fc = mult * (h * Q->xj - s * s) / sq;
    Q->b += fc;
    Q->a2 += fc * cos(lam + lam);
    Q->a4 += fc * cos(lam * 4.);
    fc = mult * s * (h + Q->xj) / sq;
    Q->c1 += fc * cos(lam);
    Q->c3 += fc * cos(lam * 3.);
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    int l, nn;
    double lamt, xlam, sdsq, c, d, s, lamdp, phidp, lampp, tanph;
    double lamtp, cl, sd, sp, fac, sav, tanphi;

    if (lp.phi > M_HALFPI)
        lp.phi = M_HALFPI;
    else if (lp.phi < -M_HALFPI)
        lp.phi = -M_HALFPI;

    lampp = lp.phi >= 0. ? M_HALFPI : M_PI_HALFPI;
    tanphi = tan(lp.phi);
    for (nn = 0;;) {
        sav = lampp;
        lamtp = lp.lam + Q->p22 * lampp;
        cl = cos(lamtp);
        if (fabs(cl) < TOL)
            lamtp -= TOL;
        fac = lampp - sin(lampp) * (cl < 0. ? -M_HALFPI : M_HALFPI);
        for (l = 50; l; --l) {
            lamt = lp.lam + Q->p22 * sav;
            c = cos(lamt);
            if (fabs(c) < TOL)
                lamt -= TOL;
            xlam = (P->one_es * tanphi * Q->sa + sin(lamt) * Q->ca) / c;
            lamdp = atan(xlam) + fac;
            if (fabs(fabs(sav) - fabs(lamdp)) < TOL)
                break;
            sav = lamdp;
        }
        if (!l || ++nn >= 3 || (lamdp > Q->rlm && lamdp < Q->rlm2))
            break;
        if (lamdp <= Q->rlm)
            lampp = M_TWOPI_HALFPI;
        else if (lamdp >= Q->rlm2)
            lampp = M_HALFPI;
    }
    if (l) {
        sp = sin(lp.phi);
        phidp = aasin(P->ctx,(P->one_es * Q->ca * sp - Q->sa * cos(lp.phi) *
            sin(lamt)) / sqrt(1. - P->es * sp * sp));
        tanph = log(tan(M_FORTPI + .5 * phidp));
        sd = sin(lamdp);
        sdsq = sd * sd;
        s = Q->p22 * Q->sa * cos(lamdp) * sqrt((1. + Q->t * sdsq)
             / ((1. + Q->w * sdsq) * (1. + Q->q * sdsq)));
        d = sqrt(Q->xj * Q->xj + s * s);
        xy.x = Q->b * lamdp + Q->a2 * sin(2. * lamdp) + Q->a4 *
            sin(lamdp * 4.) - tanph * s / d;
        xy.y = Q->c1 * sd + Q->c3 * sin(lamdp * 3.) + tanph * Q->xj / d;
    } else
        xy.x = xy.y = HUGE_VAL;
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    int nn;
    double lamt, sdsq, s, lamdp, phidp, sppsq, dd, sd, sl, fac, scl, sav, spp;

    lamdp = xy.x / Q->b;
    nn = 50;
    do {
        sav = lamdp;
        sd = sin(lamdp);
        sdsq = sd * sd;
        s = Q->p22 * Q->sa * cos(lamdp) * sqrt((1. + Q->t * sdsq)
             / ((1. + Q->w * sdsq) * (1. + Q->q * sdsq)));
        lamdp = xy.x + xy.y * s / Q->xj - Q->a2 * sin(
            2. * lamdp) - Q->a4 * sin(lamdp * 4.) - s / Q->xj * (
            Q->c1 * sin(lamdp) + Q->c3 * sin(lamdp * 3.));
        lamdp /= Q->b;
    } while (fabs(lamdp - sav) >= TOL && --nn);
    sl = sin(lamdp);
    fac = exp(sqrt(1. + s * s / Q->xj / Q->xj) * (xy.y -
        Q->c1 * sl - Q->c3 * sin(lamdp * 3.)));
    phidp = 2. * (atan(fac) - M_FORTPI);
    dd = sl * sl;
    if (fabs(cos(lamdp)) < TOL)
        lamdp -= TOL;
    spp = sin(phidp);
    sppsq = spp * spp;
    lamt = atan(((1. - sppsq * P->rone_es) * tan(lamdp) *
        Q->ca - spp * Q->sa * sqrt((1. + Q->q * dd) * (
        1. - sppsq) - sppsq * Q->u) / cos(lamdp)) / (1. - sppsq
        * (1. + Q->u)));
    sl = lamt >= 0. ? 1. : -1.;
    scl = cos(lamdp) >= 0. ? 1. : -1;
    lamt -= M_HALFPI * (1. - scl) * sl;
    lp.lam = lamt - Q->p22 * lamdp;
    if (fabs(Q->sa) < TOL)
        lp.phi = aasin(P->ctx,spp / sqrt(P->one_es * P->one_es + P->es * sppsq));
    else
        lp.phi = atan((tan(lamdp) * cos(lamt) - Q->ca * sin(lamt)) /
            (P->one_es * Q->sa));
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


PJ *PROJECTION(lsat) {
    int land, path;
    double lam, alf, esc, ess;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    land = pj_param(P->ctx, P->params, "ilsat").i;
    if (land <= 0 || land > 5) E_ERROR(-28);
    path = pj_param(P->ctx, P->params, "ipath").i;
    if (path <= 0 || path > (land <= 3 ? 251 : 233)) E_ERROR(-29);
    if (land <= 3) {
        P->lam0 = DEG_TO_RAD * 128.87 - M_TWOPI / 251. * path;
        Q->p22 = 103.2669323;
        alf = DEG_TO_RAD * 99.092;
    } else {
        P->lam0 = DEG_TO_RAD * 129.3 - M_TWOPI / 233. * path;
        Q->p22 = 98.8841202;
        alf = DEG_TO_RAD * 98.2;
    }
    Q->p22 /= 1440.;
    Q->sa = sin(alf);
    Q->ca = cos(alf);
    if (fabs(Q->ca) < 1e-9)
        Q->ca = 1e-9;
    esc = P->es * Q->ca * Q->ca;
    ess = P->es * Q->sa * Q->sa;
    Q->w = (1. - esc) * P->rone_es;
    Q->w = Q->w * Q->w - 1.;
    Q->q = ess * P->rone_es;
    Q->t = ess * (2. - P->es) * P->rone_es * P->rone_es;
    Q->u = esc * P->rone_es;
    Q->xj = P->one_es * P->one_es * P->one_es;
    Q->rlm = M_PI * (1. / 248. + .5161290322580645);
    Q->rlm2 = Q->rlm + M_TWOPI;
    Q->a2 = Q->a4 = Q->b = Q->c1 = Q->c3 = 0.;
    seraz0(0., 1., P);
    for (lam = 9.; lam <= 81.0001; lam += 18.)
        seraz0(lam, 4., P);
    for (lam = 18; lam <= 72.0001; lam += 18.)
        seraz0(lam, 2., P);
    seraz0(90., 1., P);
    Q->a2 /= 30.;
    Q->a4 /= 60.;
    Q->b /= 30.;
    Q->c1 /= 15.;
    Q->c3 /= 45.;

    P->inv = e_inverse;
    P->fwd = e_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_lsat_selftest (void) {return 0;}
#else

int pj_lsat_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=lsat +ellps=GRS80 +lat_1=0.5 +lat_2=2 +lsat=1 +path=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {18241950.01455855,    9998256.83982293494},
        {18746856.2533194572, 10215761.669925211},
        {18565503.6836331636,  9085039.14672705345},
        {19019696.9020289108,  9247763.0394328218},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {126.000423834530011,  0.00172378224025701425},
        {126.002213738256714,  0.00188015467480917966},
        {126.000734468914601, -0.00188015467480917966},
        {126.002524372641304, -0.00172378224025701425},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
