/* general inverse projection */
#define PJ_LIB__
#include <projects.h>
#include <errno.h>
# define EPS 1.0e-12
	LP /* inverse projection entry */
pj_inv(XY xy, PJ *P) {
	LP lp;

	/* can't do as much preliminary checking as with forward */
	if (xy.x == HUGE_VAL || xy.y == HUGE_VAL) {
		lp.lam = lp.phi = HUGE_VAL;
		pj_ctx_set_errno( P->ctx, -15);
                return lp;
	}

	errno = pj_errno = 0;
        P->ctx->last_errno = 0;

	xy.x = (xy.x * P->to_meter - P->x0) * P->ra; /* descale and de-offset */
	xy.y = (xy.y * P->to_meter - P->y0) * P->ra;

        /* Check for NULL pointer */
        if (P->inv != NULL)
        {
	    lp = (*P->inv)(xy, P); /* inverse project */
	    if (P->ctx->last_errno )
		lp.lam = lp.phi = HUGE_VAL;
	    else {
		lp.lam += P->lam0; /* reduce from del lp.lam */
		if (!P->over)
			lp.lam = adjlon(lp.lam); /* adjust longitude to CM */
		if (P->geoc && fabs(fabs(lp.phi)-M_HALFPI) > EPS)
			lp.phi = atan(P->one_es * tan(lp.phi));
	    }
        }
        else
        {
           lp.lam = lp.phi = HUGE_VAL;
        }
	return lp;
}
