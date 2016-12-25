/* general forward projection */
#define PJ_LIB__
#include <projects.h>
#include <errno.h>
# define EPS 1.0e-12
	XY /* forward projection entry */
pj_fwd(LP lp, PJ *P) {
	XY xy;
	double t;

	/* check for forward and latitude or longitude overange */
	if ((t = fabs(lp.phi)-M_HALFPI) > EPS || fabs(lp.lam) > 10.) {
		xy.x = xy.y = HUGE_VAL;
		pj_ctx_set_errno( P->ctx, -14);
	} else { /* proceed with projection */
                P->ctx->last_errno = 0;
                pj_errno = 0;
                errno = 0;

		if (fabs(t) <= EPS)
			lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
		else if (P->geoc)
			lp.phi = atan(P->rone_es * tan(lp.phi));
		lp.lam -= P->lam0;	/* compute del lp.lam */
		if (!P->over)
			lp.lam = adjlon(lp.lam); /* adjust del longitude */

                /* Check for NULL pointer */
                if (P->fwd != NULL)
                {
		    xy = (*P->fwd)(lp, P); /* project */
		    if ( P->ctx->last_errno )
			xy.x = xy.y = HUGE_VAL;
		    /* adjust for major axis and easting/northings */
		    else {
			xy.x = P->fr_meter * (P->a * xy.x + P->x0);
			xy.y = P->fr_meter * (P->a * xy.y + P->y0);
		    }
                }
                else
                {
                    xy.x = xy.y = HUGE_VAL;
                }
	}
	return xy;
}
