/* determine latitude from authalic latitude */
#include <projects.h>
# define P00 .33333333333333333333 /*   1 /     3 */
# define P01 .17222222222222222222 /*  31 /   180 */
# define P02 .10257936507936507937 /* 517 /  5040 */
# define P10 .06388888888888888888 /*  23 /   360 */
# define P11 .06640211640211640212 /* 251 /  3780 */
# define P20 .01677689594356261023 /* 761 / 45360 */
#define APA_SIZE 3
	double *
pj_authset(double es) {
	double t, *APA;

	if ((APA = (double *)pj_malloc(APA_SIZE * sizeof(double))) != NULL) {
		APA[0] = es * P00;
		t = es * es;
		APA[0] += t * P01;
		APA[1] = t * P10;
		t *= es;
		APA[0] += t * P02;
		APA[1] += t * P11;
		APA[2] = t * P20;
	}
	return APA;
}
	double
pj_authlat(double beta, double *APA) {
	double t = beta+beta;
	return(beta + APA[0] * sin(t) + APA[1] * sin(t+t) + APA[2] * sin(t+t+t));
}
