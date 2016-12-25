/* dervative of (*P->fwd) projection */
#define PJ_LIB__
#include "projects.h"
	int
pj_deriv(LP lp, double h, PJ *P, struct DERIVS *der) {
	XY t;

	lp.lam += h;
	lp.phi += h;
	if (fabs(lp.phi) > M_HALFPI) return 1;
	h += h;
	t = (*P->fwd)(lp, P);
	if (t.x == HUGE_VAL) return 1;
	der->x_l = t.x; der->y_p = t.y; der->x_p = -t.x; der->y_l = -t.y;
	lp.phi -= h;
	if (fabs(lp.phi) > M_HALFPI) return 1;
	t = (*P->fwd)(lp, P);
	if (t.x == HUGE_VAL) return 1;
	der->x_l += t.x; der->y_p -= t.y; der->x_p += t.x; der->y_l -= t.y;
	lp.lam -= h;
	t = (*P->fwd)(lp, P);
	if (t.x == HUGE_VAL) return 1;
	der->x_l -= t.x; der->y_p -= t.y; der->x_p += t.x; der->y_l += t.y;
	lp.phi += h;
	t = (*P->fwd)(lp, P);
	if (t.x == HUGE_VAL) return 1;
	der->x_l -= t.x; der->y_p += t.y; der->x_p -= t.x; der->y_l += t.y;
	der->x_l /= (h += h);
	der->y_p /= h;
	der->x_p /= h;
	der->y_l /= h;
	return 0;
}
