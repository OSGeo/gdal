#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(poly, "Polyconic (American)")
	"\n\tConic, Sph&Ell";

struct pj_opaque {
	double ml0; \
	double *en;
};

#define TOL	1e-10
#define CONV	1e-10
#define N_ITER	10
#define I_ITER 20
#define ITOL 1.e-12


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double  ms, sp, cp;

	if (fabs(lp.phi) <= TOL) {
        xy.x = lp.lam;
        xy.y = -Q->ml0;
    } else {
		sp = sin(lp.phi);
		ms = fabs(cp = cos(lp.phi)) > TOL ? pj_msfn(sp, cp, P->es) / sp : 0.;
		xy.x = ms * sin(lp.lam *= sp);
		xy.y = (pj_mlfn(lp.phi, sp, cp, Q->en) - Q->ml0) + ms * (1. - cos(lp.lam));
	}

	return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double  cot, E;

	if (fabs(lp.phi) <= TOL) {
        xy.x = lp.lam;
        xy.y = Q->ml0;
    } else {
		cot = 1. / tan(lp.phi);
		xy.x = sin(E = lp.lam * sin(lp.phi)) * cot;
		xy.y = lp.phi - P->phi0 + cot * (1. - cos(E));
	}

	return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

	xy.y += Q->ml0;
	if (fabs(xy.y) <= TOL) {
        lp.lam = xy.x;
        lp.phi = 0.;
    } else {
		double r, c, sp, cp, s2ph, ml, mlb, mlp, dPhi;
		int i;

		r = xy.y * xy.y + xy.x * xy.x;
		for (lp.phi = xy.y, i = I_ITER; i ; --i) {
			sp = sin(lp.phi);
			s2ph = sp * ( cp = cos(lp.phi));
			if (fabs(cp) < ITOL)
				I_ERROR;
			c = sp * (mlp = sqrt(1. - P->es * sp * sp)) / cp;
			ml = pj_mlfn(lp.phi, sp, cp, Q->en);
			mlb = ml * ml + r;
			mlp = P->one_es / (mlp * mlp * mlp);
			lp.phi += ( dPhi =
				( ml + ml + c * mlb - 2. * xy.y * (c * ml + 1.) ) / (
				P->es * s2ph * (mlb - 2. * xy.y * ml) / c +
				2.* (xy.y - ml) * (c * mlp - 1. / s2ph) - mlp - mlp ));
			if (fabs(dPhi) <= ITOL)
				break;
		}
		if (!i)
			I_ERROR;
		c = sin(lp.phi);
		lp.lam = asin(xy.x * tan(lp.phi) * sqrt(1. - P->es * c * c)) / sin(lp.phi);
	}

	return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	double B, dphi, tp;
	int i;

	if (fabs(xy.y = P->phi0 + xy.y) <= TOL) {
        lp.lam = xy.x;
        lp.phi = 0.;
    } else {
		lp.phi = xy.y;
		B = xy.x * xy.x + xy.y * xy.y;
		i = N_ITER;
		do {
			tp = tan(lp.phi);
			lp.phi -= (dphi = (xy.y * (lp.phi * tp + 1.) - lp.phi -
				.5 * ( lp.phi * lp.phi + B) * tp) /
				((lp.phi - xy.y) / tp - 1.));
		} while (fabs(dphi) > CONV && --i);
		if (! i) I_ERROR;
		lp.lam = asin(xy.x * tan(lp.phi)) / sin(lp.phi);
	}

	return lp;
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


PJ *PROJECTION(poly) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	if (P->es) {
		if (!(Q->en = pj_enfn(P->es))) E_ERROR_0;
		Q->ml0 = pj_mlfn(P->phi0, sin(P->phi0), cos(P->phi0), Q->en);
		P->inv = e_inverse;
		P->fwd = e_forward;
	} else {
		Q->ml0 = -P->phi0;
		P->inv = s_inverse;
		P->fwd = s_forward;
	}

    return P;
}


#ifndef PJ_SELFTEST
int pj_poly_selftest (void) {return 0;}
#else

int pj_poly_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=poly   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=poly   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222605.285770237475,  110642.194561440483},
        { 222605.285770237475, -110642.194561440483},
        {-222605.285770237475,  110642.194561440483},
        {-222605.285770237475, -110642.194561440483},
    };

    XY s_fwd_expect[] = {
        { 223368.105210218986,  111769.110491224754},
        { 223368.105210218986, -111769.110491224754},
        {-223368.105210218986,  111769.110491224754},
        {-223368.105210218986, -111769.110491224754},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00179663056846135222,  0.000904369476631838518},
        { 0.00179663056846135222, -0.000904369476631838518},
        {-0.00179663056846135222,  0.000904369476631838518},
        {-0.00179663056846135222, -0.000904369476631838518},
    };

    LP s_inv_expect[] = {
        { 0.0017904931100023887,  0.000895246554454779222},
        { 0.0017904931100023887, -0.000895246554454779222},
        {-0.0017904931100023887,  0.000895246554454779222},
        {-0.0017904931100023887, -0.000895246554454779222},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
