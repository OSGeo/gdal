/* set ellipsoid parameters a and es */
#include <projects.h>
#include <string.h>
#define SIXTH .1666666666666666667 /* 1/6 */
#define RA4 .04722222222222222222 /* 17/360 */
#define RA6 .02215608465608465608 /* 67/3024 */
#define RV4 .06944444444444444444 /* 5/72 */
#define RV6 .04243827160493827160 /* 55/1296 */
	int /* initialize geographic shape parameters */
pj_ell_set(projCtx ctx, paralist *pl, double *a, double *es) {
	int i;
	double b=0.0, e;
	char *name;
	paralist *start = 0;

        /* clear any previous error */
        pj_ctx_set_errno(ctx,0);

        /* check for varying forms of ellipsoid input */
	*a = *es = 0.;
	/* R takes precedence */
	if (pj_param(ctx, pl, "tR").i)
		*a = pj_param(ctx,pl, "dR").f;
	else { /* probable elliptical figure */

		/* check if ellps present and temporarily append its values to pl */
        if ((name = pj_param(ctx,pl, "sellps").s) != NULL) {
			char *s;

			for (start = pl; start && start->next ; start = start->next) ;
			for (i = 0; (s = pj_ellps[i].id) && strcmp(name, s) ; ++i) ;
			if (!s) { pj_ctx_set_errno( ctx, -9); return 1; }
			start->next = pj_mkparam(pj_ellps[i].major);
			start->next->next = pj_mkparam(pj_ellps[i].ell);
		}
		*a = pj_param(ctx,pl, "da").f;
		if (pj_param(ctx,pl, "tes").i) /* eccentricity squared */
			*es = pj_param(ctx,pl, "des").f;
		else if (pj_param(ctx,pl, "te").i) { /* eccentricity */
			e = pj_param(ctx,pl, "de").f;
			*es = e * e;
		} else if (pj_param(ctx,pl, "trf").i) { /* recip flattening */
			*es = pj_param(ctx,pl, "drf").f;
			if (!*es) {
				pj_ctx_set_errno( ctx, -10);
				goto bomb;
			}
			*es = 1./ *es;
			*es = *es * (2. - *es);
		} else if (pj_param(ctx,pl, "tf").i) { /* flattening */
			*es = pj_param(ctx,pl, "df").f;
			*es = *es * (2. - *es);
		} else if (pj_param(ctx,pl, "tb").i) { /* minor axis */
			b = pj_param(ctx,pl, "db").f;
			*es = 1. - (b * b) / (*a * *a);
		}     /* else *es == 0. and sphere of radius *a */
		if (!b)
			b = *a * sqrt(1. - *es);
		/* following options turn ellipsoid into equivalent sphere */
		if (pj_param(ctx,pl, "bR_A").i) { /* sphere--area of ellipsoid */
			*a *= 1. - *es * (SIXTH + *es * (RA4 + *es * RA6));
			*es = 0.;
		} else if (pj_param(ctx,pl, "bR_V").i) { /* sphere--vol. of ellipsoid */
			*a *= 1. - *es * (SIXTH + *es * (RV4 + *es * RV6));
			*es = 0.;
		} else if (pj_param(ctx,pl, "bR_a").i) { /* sphere--arithmetic mean */
			*a = .5 * (*a + b);
			*es = 0.;
		} else if (pj_param(ctx,pl, "bR_g").i) { /* sphere--geometric mean */
			*a = sqrt(*a * b);
			*es = 0.;
		} else if (pj_param(ctx,pl, "bR_h").i) { /* sphere--harmonic mean */
			*a = 2. * *a * b / (*a + b);
			*es = 0.;
		} else if ((i = pj_param(ctx,pl, "tR_lat_a").i) || /* sphere--arith. */
			pj_param(ctx,pl, "tR_lat_g").i) { /* or geom. mean at latitude */
			double tmp;

			tmp = sin(pj_param(ctx,pl, i ? "rR_lat_a" : "rR_lat_g").f);
			if (fabs(tmp) > M_HALFPI) {
                                pj_ctx_set_errno(ctx,-11);
				goto bomb;
			}
			tmp = 1. - *es * tmp * tmp;
			*a *= i ? .5 * (1. - *es + tmp) / ( tmp * sqrt(tmp)) :
				sqrt(1. - *es) / tmp;
			*es = 0.;
		}
bomb:
		if (start) { /* clean up temporary extension of list */
			pj_dalloc(start->next->next);
			pj_dalloc(start->next);
			start->next = 0;
		}
		if (ctx->last_errno)
			return 1;
	}
	/* some remaining checks */
	if (*es < 0.)
        	{ pj_ctx_set_errno( ctx, -12); return 1; }
	if (*a <= 0.)
        	{ pj_ctx_set_errno( ctx, -13); return 1; }
	return 0;
}
