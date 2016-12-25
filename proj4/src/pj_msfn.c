/* determine constant small m */
#include <math.h>
#include <projects.h>
	double
pj_msfn(double sinphi, double cosphi, double es) {
	return (cosphi / sqrt (1. - es * sinphi * sinphi));
}
