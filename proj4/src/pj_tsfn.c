/* determine small t */
#include <math.h>
#include <projects.h>

	double
pj_tsfn(double phi, double sinphi, double e) {
	sinphi *= e;
	return (tan (.5 * (M_HALFPI - phi)) /
	   pow((1. - sinphi) / (1. + sinphi), .5 * e));
}
