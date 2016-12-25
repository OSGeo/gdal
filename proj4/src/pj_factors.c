/* projection scale factors */
#define PJ_LIB__
#include <projects.h>
#include <errno.h>
#ifndef DEFAULT_H
#define DEFAULT_H   1e-5    /* radian default for numeric h */
#endif
#define EPS 1.0e-12
	int
pj_factors(LP lp, PJ *P, double h, struct FACTORS *fac) {
	struct DERIVS der;
	double cosphi, t, n, r;

	/* check for forward and latitude or longitude overange */
	if ((t = fabs(lp.phi)-M_HALFPI) > EPS || fabs(lp.lam) > 10.) {
                pj_ctx_set_errno( P->ctx, -14);
		return 1;
	} else { /* proceed */
		errno = pj_errno = 0;
                P->ctx->last_errno = 0;

		if (h < EPS)
			h = DEFAULT_H;
		if (fabs(lp.phi) > (M_HALFPI - h))
                /* adjust to value around pi/2 where derived still exists*/
		        lp.phi = lp.phi < 0. ? (-M_HALFPI+h) : (M_HALFPI-h);
		else if (P->geoc)
			lp.phi = atan(P->rone_es * tan(lp.phi));
		lp.lam -= P->lam0;	/* compute del lp.lam */
		if (!P->over)
			lp.lam = adjlon(lp.lam); /* adjust del longitude */
		if (P->spc)	/* get what projection analytic values */
			P->spc(lp, P, fac);
		if (((fac->code & (IS_ANAL_XL_YL+IS_ANAL_XP_YP)) !=
			  (IS_ANAL_XL_YL+IS_ANAL_XP_YP)) &&
			  pj_deriv(lp, h, P, &der))
			return 1;
		if (!(fac->code & IS_ANAL_XL_YL)) {
			fac->der.x_l = der.x_l;
			fac->der.y_l = der.y_l;
		}
		if (!(fac->code & IS_ANAL_XP_YP)) {
			fac->der.x_p = der.x_p;
			fac->der.y_p = der.y_p;
		}
		cosphi = cos(lp.phi);
		if (!(fac->code & IS_ANAL_HK)) {
			fac->h = hypot(fac->der.x_p, fac->der.y_p);
			fac->k = hypot(fac->der.x_l, fac->der.y_l) / cosphi;
			if (P->es) {
				t = sin(lp.phi);
				t = 1. - P->es * t * t;
				n = sqrt(t);
				fac->h *= t * n / P->one_es;
				fac->k *= n;
				r = t * t / P->one_es;
			} else
				r = 1.;
		} else if (P->es) {
			r = sin(lp.phi);
			r = 1. - P->es * r * r;
			r = r * r / P->one_es;
		} else
			r = 1.;
		/* convergence */
		if (!(fac->code & IS_ANAL_CONV)) {
			fac->conv = - atan2(fac->der.y_l, fac->der.x_l);
			if (fac->code & IS_ANAL_XL_YL)
				fac->code |= IS_ANAL_CONV;
		}
		/* areal scale factor */
		fac->s = (fac->der.y_p * fac->der.x_l - fac->der.x_p * fac->der.y_l) *
			r / cosphi;
		/* meridian-parallel angle theta prime */
		fac->thetap = aasin(P->ctx,fac->s / (fac->h * fac->k));
		/* Tissot ellips axis */
		t = fac->k * fac->k + fac->h * fac->h;
		fac->a = sqrt(t + 2. * fac->s);
		t = (t = t - 2. * fac->s) <= 0. ? 0. : sqrt(t);
		fac->b = 0.5 * (fac->a - t);
		fac->a = 0.5 * (fac->a + t);
		/* omega */
		fac->omega = 2. * aasin(P->ctx,(fac->a - fac->b)/(fac->a + fac->b));
	}
	return 0;
}
