/* evaluate complex polynomial */
#include <projects.h>
/* note: coefficients are always from C_1 to C_n
**	i.e. C_0 == (0., 0)
**	n should always be >= 1 though no checks are made
*/
	COMPLEX
pj_zpoly1(COMPLEX z, COMPLEX *C, int n) {
	COMPLEX a;
	double t;

	a = *(C += n);
	while (n-- > 0) {
		a.r = (--C)->r + z.r * (t = a.r) - z.i * a.i;
		a.i = C->i + z.r * a.i + z.i * t;
	}
	a.r = z.r * (t = a.r) - z.i * a.i;
	a.i = z.r * a.i + z.i * t;
	return a;
}
/* evaluate complex polynomial and derivative */
	COMPLEX
pj_zpolyd1(COMPLEX z, COMPLEX *C, int n, COMPLEX *der) {
	COMPLEX a, b;
	double t;
	int first = 1;

	a = *(C += n);
	b = a;
	while (n-- > 0) {
		if (first) {
			first = 0;
		} else {
			b.r = a.r + z.r * (t = b.r) - z.i * b.i;
			b.i = a.i + z.r * b.i + z.i * t;
		}
		a.r = (--C)->r + z.r * (t = a.r) - z.i * a.i;
		a.i = C->i + z.r * a.i + z.i * t;
	}
	b.r = a.r + z.r * (t = b.r) - z.i * b.i;
	b.i = a.i + z.r * b.i + z.i * t;
	a.r = z.r * (t = a.r) - z.i * a.i;
	a.i = z.r * a.i + z.i * t;
	*der = b;
	return a;
}
