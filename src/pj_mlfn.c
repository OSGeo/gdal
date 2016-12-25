#include <projects.h>
/* meridinal distance for ellipsoid and inverse
**	8th degree - accurate to < 1e-5 meters when used in conjuction
**		with typical major axis values.
**	Inverse determines phi to EPS (1e-11) radians, about 1e-6 seconds.
*/
#define C00 1.
#define C02 .25
#define C04 .046875
#define C06 .01953125
#define C08 .01068115234375
#define C22 .75
#define C44 .46875
#define C46 .01302083333333333333
#define C48 .00712076822916666666
#define C66 .36458333333333333333
#define C68 .00569661458333333333
#define C88 .3076171875
#define EPS 1e-11
#define MAX_ITER 10
#define EN_SIZE 5
	double *
pj_enfn(double es) {
	double t, *en;

	if ((en = (double *)pj_malloc(EN_SIZE * sizeof(double))) != NULL) {
		en[0] = C00 - es * (C02 + es * (C04 + es * (C06 + es * C08)));
		en[1] = es * (C22 - es * (C04 + es * (C06 + es * C08)));
		en[2] = (t = es * es) * (C44 - es * (C46 + es * C48));
		en[3] = (t *= es) * (C66 - es * C68);
		en[4] = t * es * C88;
	} /* else return NULL if unable to allocate memory */
	return en;
}
	double
pj_mlfn(double phi, double sphi, double cphi, double *en) {
	cphi *= sphi;
	sphi *= sphi;
	return(en[0] * phi - cphi * (en[1] + sphi*(en[2]
		+ sphi*(en[3] + sphi*en[4]))));
}
	double
pj_inv_mlfn(projCtx ctx, double arg, double es, double *en) {
	double s, t, phi, k = 1./(1.-es);
	int i;

	phi = arg;
	for (i = MAX_ITER; i ; --i) { /* rarely goes over 2 iterations */
		s = sin(phi);
		t = 1. - es * s * s;
		phi -= t = (pj_mlfn(phi, s, cos(phi), en) - arg) * (t * sqrt(t)) * k;
		if (fabs(t) < EPS)
			return phi;
	}
	pj_ctx_set_errno( ctx, -17 );
	return phi;
}
