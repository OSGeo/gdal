/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003, 2006   Gerald I. Evenden
*/
/*
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/* Computes distance from equator along the meridian to latitude phi
** and inverse on unit ellipsoid.
** Precision commensurate with double precision.
*/
#define PROJ_LIB__
#include <projects.h>
#define MAX_ITER 20
#define TOL 1e-14

struct MDIST {
	int nb;
	double es;
	double E;
	double b[1];
};
#define B ((struct MDIST *)b)
	void *
proj_mdist_ini(double es) {
	double numf, numfi, twon1, denf, denfi, ens, T, twon;
	double den, El, Es;
	double E[MAX_ITER];
	struct MDIST *b;
	int i, j;

/* generate E(e^2) and its terms E[] */
	ens = es;
	numf = twon1 = denfi = 1.;
	denf = 1.;
	twon = 4.;
	Es = El = E[0] = 1.;
	for (i = 1; i < MAX_ITER ; ++i) {
		numf *= (twon1 * twon1);
		den = twon * denf * denf * twon1;
		T = numf/den;
		Es -= (E[i] = T * ens);
		ens *= es;
		twon *= 4.;
		denf *= ++denfi;
		twon1 += 2.;
		if (Es == El) /* jump out if no change */
			break;
		El = Es;
	}
	if ((b = (struct MDIST *)malloc(sizeof(struct MDIST)+
		(i*sizeof(double)))) == NULL)
		return(NULL);
	b->nb = i - 1;
	b->es = es;
	b->E = Es;
	/* generate b_n coefficients--note: collapse with prefix ratios */
	b->b[0] = Es = 1. - Es;
	numf = denf = 1.;
	numfi = 2.;
	denfi = 3.;
	for (j = 1; j < i; ++j) {
		Es -= E[j];
		numf *= numfi;
		denf *= denfi;
		b->b[j] = Es * numf / denf;
		numfi += 2.;
		denfi += 2.;
	}
	return (b);
}
	double
proj_mdist(double phi, double sphi, double cphi, const void *b) {
	double sc, sum, sphi2, D;
	int i;

	sc = sphi * cphi;
	sphi2 = sphi * sphi;
	D = phi * B->E - B->es * sc / sqrt(1. - B->es * sphi2);
	sum = B->b[i = B->nb];
	while (i) sum = B->b[--i] + sphi2 * sum;
	return(D + sc * sum);
}
	double
proj_inv_mdist(projCtx ctx, double dist, const void *b) {
	double s, t, phi, k;
	int i;

	k = 1./(1.- B->es);
	i = MAX_ITER;
	phi = dist;
	while ( i-- ) {
		s = sin(phi);
		t = 1. - B->es * s * s;
		phi -= t = (proj_mdist(phi, s, cos(phi), b) - dist) *
			(t * sqrt(t)) * k;
		if (fabs(t) < TOL) /* that is no change */
			return phi;
	}
		/* convergence failed */
	pj_ctx_set_errno(ctx, -17);
	return phi;
}
