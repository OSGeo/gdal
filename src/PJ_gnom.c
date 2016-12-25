#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(gnom, "Gnomonic") "\n\tAzi, Sph.";

#define EPS10  1.e-10
#define N_POLE 0
#define S_POLE 1
#define EQUIT  2
#define OBLIQ  3

struct pj_opaque {
    double  sinph0;
    double  cosph0;
    int     mode;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  coslam, cosphi, sinphi;

    sinphi = sin(lp.phi);
    cosphi = cos(lp.phi);
    coslam = cos(lp.lam);

    switch (Q->mode) {
        case EQUIT:
            xy.y = cosphi * coslam;
            break;
        case OBLIQ:
            xy.y = Q->sinph0 * sinphi + Q->cosph0 * cosphi * coslam;
            break;
        case S_POLE:
            xy.y = - sinphi;
            break;
        case N_POLE:
            xy.y = sinphi;
            break;
    }

    if (xy.y <= EPS10) F_ERROR;

    xy.x = (xy.y = 1. / xy.y) * cosphi * sin(lp.lam);
    switch (Q->mode) {
        case EQUIT:
            xy.y *= sinphi;
            break;
        case OBLIQ:
            xy.y *= Q->cosph0 * sinphi - Q->sinph0 * cosphi * coslam;
            break;
        case N_POLE:
            coslam = - coslam;
        case S_POLE:
            xy.y *= cosphi * coslam;
            break;
    }
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  rh, cosz, sinz;

    rh = hypot(xy.x, xy.y);
    sinz = sin(lp.phi = atan(rh));
    cosz = sqrt(1. - sinz * sinz);

    if (fabs(rh) <= EPS10) {
        lp.phi = P->phi0;
        lp.lam = 0.;
    } else {
        switch (Q->mode) {
            case OBLIQ:
                lp.phi = cosz * Q->sinph0 + xy.y * sinz * Q->cosph0 / rh;
                if (fabs(lp.phi) >= 1.)
                    lp.phi = lp.phi > 0. ? M_HALFPI : - M_HALFPI;
                else
                    lp.phi = asin(lp.phi);
                xy.y = (cosz - Q->sinph0 * sin(lp.phi)) * rh;
                xy.x *= sinz * Q->cosph0;
                break;
            case EQUIT:
                lp.phi = xy.y * sinz / rh;
                if (fabs(lp.phi) >= 1.)
                    lp.phi = lp.phi > 0. ? M_HALFPI : - M_HALFPI;
                else
                    lp.phi = asin(lp.phi);
                xy.y = cosz * rh;
                xy.x *= sinz;
                break;
            case S_POLE:
                lp.phi -= M_HALFPI;
                break;
            case N_POLE:
                lp.phi = M_HALFPI - lp.phi;
                xy.y = -xy.y;
                break;
        }
        lp.lam = atan2(xy.x, xy.y);
    }
    return lp;
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


PJ *PROJECTION(gnom) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (fabs(fabs(P->phi0) - M_HALFPI) < EPS10) {
        Q->mode = P->phi0 < 0. ? S_POLE : N_POLE;
    } else if (fabs(P->phi0) < EPS10) {
        Q->mode = EQUIT;
    } else {
        Q->mode = OBLIQ;
        Q->sinph0 = sin(P->phi0);
        Q->cosph0 = cos(P->phi0);
    }

    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_gnom_selftest (void) {return 0;}
#else

int pj_gnom_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=gnom   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223492.92474718543,  111780.50920659291},
        { 223492.92474718543, -111780.50920659291},
        {-223492.92474718543,  111780.50920659291},
        {-223492.92474718543, -111780.50920659291},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931092009798,  0.00089524655438192376},
        { 0.0017904931092009798, -0.00089524655438192376},
        {-0.0017904931092009798,  0.00089524655438192376},
        {-0.0017904931092009798, -0.00089524655438192376},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
