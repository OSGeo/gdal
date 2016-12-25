#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
    double  height;
    double  sinph0;
    double  cosph0;
    double  p;
    double  rp;
    double  pn1;
    double  pfact;
    double  h;
    double  cg;
    double  sg;
    double  sw;
    double  cw;
    int     mode;
    int     tilt;
};

PROJ_HEAD(nsper, "Near-sided perspective") "\n\tAzi, Sph\n\th=";
PROJ_HEAD(tpers, "Tilted perspective") "\n\tAzi, Sph\n\ttilt= azi= h=";

# define EPS10 1.e-10
# define N_POLE 0
# define S_POLE 1
# define EQUIT  2
# define OBLIQ  3


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  coslam, cosphi, sinphi;

    sinphi = sin(lp.phi);
    cosphi = cos(lp.phi);
    coslam = cos(lp.lam);
    switch (Q->mode) {
    case OBLIQ:
        xy.y = Q->sinph0 * sinphi + Q->cosph0 * cosphi * coslam;
        break;
    case EQUIT:
        xy.y = cosphi * coslam;
        break;
    case S_POLE:
        xy.y = - sinphi;
        break;
    case N_POLE:
        xy.y = sinphi;
        break;
    }
    if (xy.y < Q->rp) F_ERROR;
    xy.y = Q->pn1 / (Q->p - xy.y);
    xy.x = xy.y * cosphi * sin(lp.lam);
    switch (Q->mode) {
    case OBLIQ:
        xy.y *= (Q->cosph0 * sinphi -
           Q->sinph0 * cosphi * coslam);
        break;
    case EQUIT:
        xy.y *= sinphi;
        break;
    case N_POLE:
        coslam = - coslam;
    case S_POLE:
        xy.y *= cosphi * coslam;
        break;
    }
    if (Q->tilt) {
        double yt, ba;

        yt = xy.y * Q->cg + xy.x * Q->sg;
        ba = 1. / (yt * Q->sw * Q->h + Q->cw);
        xy.x = (xy.x * Q->cg - xy.y * Q->sg) * Q->cw * ba;
        xy.y = yt * ba;
    }
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  rh, cosz, sinz;

    if (Q->tilt) {
        double bm, bq, yt;

        yt = 1./(Q->pn1 - xy.y * Q->sw);
        bm = Q->pn1 * xy.x * yt;
        bq = Q->pn1 * xy.y * Q->cw * yt;
        xy.x = bm * Q->cg + bq * Q->sg;
        xy.y = bq * Q->cg - bm * Q->sg;
    }
    rh = hypot(xy.x, xy.y);
    if ((sinz = 1. - rh * rh * Q->pfact) < 0.) I_ERROR;
    sinz = (Q->p - sqrt(sinz)) / (Q->pn1 / rh + rh / Q->pn1);
    cosz = sqrt(1. - sinz * sinz);
    if (fabs(rh) <= EPS10) {
        lp.lam = 0.;
        lp.phi = P->phi0;
    } else {
        switch (Q->mode) {
        case OBLIQ:
            lp.phi = asin(cosz * Q->sinph0 + xy.y * sinz * Q->cosph0 / rh);
            xy.y = (cosz - Q->sinph0 * sin(lp.phi)) * rh;
            xy.x *= sinz * Q->cosph0;
            break;
        case EQUIT:
            lp.phi = asin(xy.y * sinz / rh);
            xy.y = cosz * rh;
            xy.x *= sinz;
            break;
        case N_POLE:
            lp.phi = asin(cosz);
            xy.y = -xy.y;
            break;
        case S_POLE:
            lp.phi = - asin(cosz);
            break;
        }
        lp.lam = atan2(xy.x, xy.y);
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


static PJ *setup(PJ *P) {
    struct pj_opaque *Q = P->opaque;

    if ((Q->height = pj_param(P->ctx, P->params, "dh").f) <= 0.) E_ERROR(-30);
    if (fabs(fabs(P->phi0) - M_HALFPI) < EPS10)
        Q->mode = P->phi0 < 0. ? S_POLE : N_POLE;
    else if (fabs(P->phi0) < EPS10)
        Q->mode = EQUIT;
    else {
        Q->mode = OBLIQ;
        Q->sinph0 = sin(P->phi0);
        Q->cosph0 = cos(P->phi0);
    }
    Q->pn1 = Q->height / P->a; /* normalize by radius */
    Q->p = 1. + Q->pn1;
    Q->rp = 1. / Q->p;
    Q->h = 1. / Q->pn1;
    Q->pfact = (Q->p + 1.) * Q->h;
    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;
    return P;
}


PJ *PROJECTION(nsper) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->tilt = 0;

   return setup(P);
}


PJ *PROJECTION(tpers) {
    double omega, gamma;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    omega = pj_param(P->ctx, P->params, "dtilt").f * DEG_TO_RAD;
    gamma = pj_param(P->ctx, P->params, "dazi").f * DEG_TO_RAD;
    Q->tilt = 1;
    Q->cg = cos(gamma); Q->sg = sin(gamma);
    Q->cw = cos(omega); Q->sw = sin(omega);

    return setup(P);
}


#ifndef PJ_SELFTEST
int pj_nsper_selftest (void) {return 0;}
#else

int pj_nsper_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=nsper   +a=6400000  +h=1000000"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 222239.816114099842,  111153.763991924759},
        { 222239.816114099842, -111153.763991924759},
        {-222239.816114099842,  111153.763991924759},
        {-222239.816114099842, -111153.763991924759},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049311728792437,  0.000895246558425396135},
        { 0.00179049311728792437, -0.000895246558425396135},
        {-0.00179049311728792437,  0.000895246558425396135},
        {-0.00179049311728792437, -0.000895246558425396135},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif


#ifndef PJ_SELFTEST
int pj_tpers_selftest (void) {return 0;}
#else

int pj_tpers_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=tpers   +a=6400000  +h=1000000 +azi=20"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 170820.288955531199,  180460.865555804776},
        { 246853.941538942483, -28439.8780357754222},
        {-246853.941538942483,  28439.8780357754222},
        {-170820.288955531199, -180460.865555804776}
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00198870552603137678,  0.000228871872278689991},
        { 0.00137632081376749859, -0.00145364129728205432},
        {-0.00137632081376749859,  0.00145364129728205432},
        {-0.00198870552603137678, -0.000228871872278689991},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
