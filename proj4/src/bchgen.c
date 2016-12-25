/* generate double bivariate Chebychev polynomial */
#include <projects.h>
	int
bchgen(projUV a, projUV b, int nu, int nv, projUV **f, projUV(*func)(projUV)) {
	int i, j, k;
	projUV arg, *t, bma, bpa, *c;
	double d, fac;

	bma.u = 0.5 * (b.u - a.u); bma.v = 0.5 * (b.v - a.v);
	bpa.u = 0.5 * (b.u + a.u); bpa.v = 0.5 * (b.v + a.v);
	for ( i = 0; i < nu; ++i) {
		arg.u = cos(M_PI * (i + 0.5) / nu) * bma.u + bpa.u;
		for ( j = 0; j < nv; ++j) {
			arg.v = cos(M_PI * (j + 0.5) / nv) * bma.v + bpa.v;
			f[i][j] = (*func)(arg);
			if ((f[i][j]).u == HUGE_VAL)
				return(1);
		}
	}
	if (!(c = (projUV *) vector1(nu, sizeof(projUV)))) return 1;
	fac = 2. / nu;
	for ( j = 0; j < nv ; ++j) {
		for ( i = 0; i < nu; ++i) {
			arg.u = arg.v = 0.;
			for (k = 0; k < nu; ++k) {
				d = cos(M_PI * i * (k + .5) / nu);
				arg.u += f[k][j].u * d;
				arg.v += f[k][j].v * d;
			}
			arg.u *= fac;
			arg.v *= fac;
			c[i] = arg;
		}
		for (i = 0; i < nu; ++i)
			f[i][j] = c[i];
	}
	pj_dalloc(c);
	if (!(c = (projUV*) vector1(nv, sizeof(projUV)))) return 1;
	fac = 2. / nv;
	for ( i = 0; i < nu; ++i) {
		t = f[i];
		for (j = 0; j < nv; ++j) {
			arg.u = arg.v = 0.;
			for (k = 0; k < nv; ++k) {
				d = cos(M_PI * j * (k + .5) / nv);
				arg.u += t[k].u * d;
				arg.v += t[k].v * d;
			}
			arg.u *= fac;
			arg.v *= fac;
			c[j] = arg;
		}
		f[i] = c;
		c = t;
	}
	pj_dalloc(c);
	return(0);
}
