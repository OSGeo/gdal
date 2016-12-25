#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(ortho, "Orthographic") "\n\tAzi, Sph.";

struct pj_opaque {
	double	sinph0;
	double	cosph0;
	int		mode;
};

#define EPS10 1.e-10
#define N_POLE	0
#define S_POLE 1
#define EQUIT	2
#define OBLIQ	3


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double  coslam, cosphi, sinphi;

	cosphi = cos(lp.phi);
	coslam = cos(lp.lam);
	switch (Q->mode) {
	case EQUIT:
		if (cosphi * coslam < - EPS10) F_ERROR;
		xy.y = sin(lp.phi);
		break;
	case OBLIQ:
		if (Q->sinph0 * (sinphi = sin(lp.phi)) +
		   Q->cosph0 * cosphi * coslam < - EPS10) F_ERROR;
		xy.y = Q->cosph0 * sinphi - Q->sinph0 * cosphi * coslam;
		break;
	case N_POLE:
		coslam = - coslam;
	case S_POLE:
		if (fabs(lp.phi - P->phi0) - EPS10 > M_HALFPI) F_ERROR;
		xy.y = cosphi * coslam;
		break;
	}
	xy.x = cosphi * sin(lp.lam);
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  rh, cosc, sinc;

    if ((sinc = (rh = hypot(xy.x, xy.y))) > 1.) {
        if ((sinc - 1.) > EPS10) I_ERROR;
        sinc = 1.;
    }
    cosc = sqrt(1. - sinc * sinc); /* in this range OK */
    if (fabs(rh) <= EPS10) {
        lp.phi = P->phi0;
        lp.lam = 0.0;
    } else {
        switch (Q->mode) {
        case N_POLE:
            xy.y = -xy.y;
            lp.phi = acos(sinc);
            break;
        case S_POLE:
            lp.phi = - acos(sinc);
            break;
        case EQUIT:
            lp.phi = xy.y * sinc / rh;
            xy.x *= sinc;
            xy.y = cosc * rh;
            goto sinchk;
        case OBLIQ:
            lp.phi = cosc * Q->sinph0 + xy.y * sinc * Q->cosph0 /rh;
            xy.y = (cosc - Q->sinph0 * lp.phi) * rh;
            xy.x *= sinc * Q->cosph0;
        sinchk:
            if (fabs(lp.phi) >= 1.)
                lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
            else
                lp.phi = asin(lp.phi);
            break;
        }
        lp.lam = (xy.y == 0. && (Q->mode == OBLIQ || Q->mode == EQUIT))
             ? (xy.x == 0. ? 0. : xy.x < 0. ? -M_HALFPI : M_HALFPI)
                           : atan2(xy.x, xy.y);
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


PJ *PROJECTION(ortho) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	if (fabs(fabs(P->phi0) - M_HALFPI) <= EPS10)
		Q->mode = P->phi0 < 0. ? S_POLE : N_POLE;
	else if (fabs(P->phi0) > EPS10) {
		Q->mode = OBLIQ;
		Q->sinph0 = sin(P->phi0);
		Q->cosph0 = cos(P->phi0);
	} else
		Q->mode = EQUIT;
	P->inv = s_inverse;
	P->fwd = s_forward;
	P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_ortho_selftest (void) {return 0;}
#else

int pj_ortho_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=ortho   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223322.76057672748,  111695.401198614476},
        { 223322.76057672748, -111695.401198614476},
        {-223322.76057672748,  111695.401198614476},
        {-223322.76057672748, -111695.401198614476},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931102938101,  0.000895246554928338998},
        { 0.0017904931102938101, -0.000895246554928338998},
        {-0.0017904931102938101,  0.000895246554928338998},
        {-0.0017904931102938101, -0.000895246554928338998},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
