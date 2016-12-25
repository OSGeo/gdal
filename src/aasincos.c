/* arc sin, cosine, tan2 and sqrt that will NOT fail */
#include <projects.h>
#define ONE_TOL	 1.00000000000001
#define TOL	0.000000001
#define ATOL 1e-50

	double
aasin(projCtx ctx,double v) {
	double av;
        
	if ((av = fabs(v)) >= 1.) {
		if (av > ONE_TOL)
                        pj_ctx_set_errno( ctx, -19 );
		return (v < 0. ? -M_HALFPI : M_HALFPI);
	}
	return asin(v);
}

	double
aacos(projCtx ctx, double v) {
	double av;

	if ((av = fabs(v)) >= 1.) {
		if (av > ONE_TOL)
                        pj_ctx_set_errno( ctx, -19 );
		return (v < 0. ? M_PI : 0.);
	}
	return acos(v);
}
	double
asqrt(double v) { return ((v <= 0) ? 0. : sqrt(v)); }
	double
aatan2(double n, double d) {
	return ((fabs(n) < ATOL && fabs(d) < ATOL) ? 0. : atan2(n,d));
}
