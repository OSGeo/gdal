/* determine latitude angle phi-2 */
#include <projects.h>

#define TOL 1.0e-10
#define N_ITER 15

	double
pj_phi2(projCtx ctx, double ts, double e) {
	double eccnth, Phi, con, dphi;
	int i;

	eccnth = .5 * e;
	Phi = M_HALFPI - 2. * atan (ts);
	i = N_ITER;
	do {
		con = e * sin (Phi);
		dphi = M_HALFPI - 2. * atan (ts * pow((1. - con) /
		   (1. + con), eccnth)) - Phi;
		Phi += dphi;
	} while ( fabs(dphi) > TOL && --i);
	if (i <= 0)
		pj_ctx_set_errno( ctx, -18 );
	return Phi;
}
