/* general inverse projection */
#define PJ_LIB__
#include <projects.h>
#include <errno.h>
# define EPS 1.0e-12
	LPZ /* inverse projection entry */
pj_inv3d(XYZ xyz, PJ *P) {
	LPZ lpz;

	/* can't do as much preliminary checking as with forward */
	if (xyz.x == HUGE_VAL || xyz.y == HUGE_VAL || xyz.z == HUGE_VAL ) {
		lpz.lam = lpz.phi = lpz.z = HUGE_VAL;
		pj_ctx_set_errno( P->ctx, -15);
                return lpz;
	}

	errno = pj_errno = 0;
        P->ctx->last_errno = 0;

	xyz.x = (xyz.x * P->to_meter - P->x0) * P->ra; /* descale and de-offset */
	xyz.y = (xyz.y * P->to_meter - P->y0) * P->ra;
        /* z is not scaled since that is handled by vto_meter before we get here */

        /* Check for NULL pointer */
        if (P->inv3d != NULL)
        {
	    lpz = (*P->inv3d)(xyz, P); /* inverse project */
	    if (P->ctx->last_errno )
		lpz.lam = lpz.phi = lpz.z = HUGE_VAL;
	    else {
		lpz.lam += P->lam0; /* reduce from del lp.lam */
		if (!P->over)
			lpz.lam = adjlon(lpz.lam); /* adjust longitude to CM */

                /* This may be redundant and never used */
		if (P->geoc && fabs(fabs(lpz.phi)-M_HALFPI) > EPS)
			lpz.phi = atan(P->one_es * tan(lpz.phi));
	    }
        }
        else
        {
            lpz.lam = lpz.phi = lpz.z = HUGE_VAL;
        }
	return lpz;
}
