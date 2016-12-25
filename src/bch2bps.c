/* convert bivariate w Chebyshev series to w Power series */
#include <projects.h>
/* basic support procedures */
	static void /* clear vector to zero */
clear(projUV *p, int n) { static const projUV c = {0., 0.}; while (n--) *p++ = c; }
	static void /* clear matrix rows to zero */
bclear(projUV **p, int n, int m) { while (n--) clear(*p++, m); }
	static void /* move vector */
bmove(projUV *a, projUV *b, int n) { while (n--) *a++ = *b++; }
	static void /* a <- m * b - c */
submop(projUV *a, double m, projUV *b, projUV *c, int n) {
	while (n--) {
		a->u = m * b->u - c->u;
		a++->v = m * b++->v - c++->v;
	}
}
	static void /* a <- b - c */
subop(projUV *a, projUV *b, projUV *c, int n) {
	while (n--) {
		a->u = b->u - c->u;
		a++->v = b++->v - c++->v;
	}
}
	static void /* multiply vector a by scalar m */
dmult(projUV *a, double m, int n) { while(n--) { a->u *= m; a->v *= m; ++a; } }
	static void /* row adjust a[] <- a[] - m * b[] */
dadd(projUV *a, projUV *b, double m, int n) {
	while(n--) {
		a->u -= m * b->u;
		a++->v -= m * b++->v;
	}
}
	static void /* convert row to pover series */
rows(projUV *c, projUV *d, int n) {
	projUV sv, *dd;
	int j, k;

	dd = (projUV *)vector1(n-1, sizeof(projUV));
	sv.u = sv.v = 0.;
	for (j = 0; j < n; ++j) d[j] = dd[j] = sv;
	d[0] = c[n-1];
	for (j = n-2; j >= 1; --j) {
		for (k = n-j; k >= 1; --k) {
			sv = d[k];
			d[k].u = 2. * d[k-1].u - dd[k].u;
			d[k].v = 2. * d[k-1].v - dd[k].v;
			dd[k] = sv;
		}
		sv = d[0];
		d[0].u = -dd[0].u + c[j].u;
		d[0].v = -dd[0].v + c[j].v;
		dd[0] = sv;
	}
	for (j = n-1; j >= 1; --j) {
		d[j].u = d[j-1].u - dd[j].u;
		d[j].v = d[j-1].v - dd[j].v;
	}
	d[0].u = -dd[0].u + .5 * c[0].u;
	d[0].v = -dd[0].v + .5 * c[0].v;
	pj_dalloc(dd);
}
	static void /* convert columns to power series */
cols(projUV **c, projUV **d, int nu, int nv) {
	projUV *sv, **dd;
	int j, k;

	dd = (projUV **)vector2(nu, nv, sizeof(projUV));
	sv = (projUV *)vector1(nv, sizeof(projUV));
	bclear(d, nu, nv);
	bclear(dd, nu, nv);
	bmove(d[0], c[nu-1], nv);
	for (j = nu-2; j >= 1; --j) {
		for (k = nu-j; k >= 1; --k) {
			bmove(sv, d[k], nv);
			submop(d[k], 2., d[k-1], dd[k], nv);
			bmove(dd[k], sv, nv);
		}
		bmove(sv, d[0], nv);
		subop(d[0], c[j], dd[0], nv);
		bmove(dd[0], sv, nv);
	}
	for (j = nu-1; j >= 1; --j)
		subop(d[j], d[j-1], dd[j], nv);
	submop(d[0], .5, c[0], dd[0], nv);
	freev2((void **) dd, nu);
	pj_dalloc(sv);
}
	static void /* row adjust for range -1 to 1 to a to b */
rowshft(double a, double b, projUV *d, int n) {
	int k, j;
	double fac, cnst;

	cnst = 2. / (b - a);
	fac = cnst;
	for (j = 1; j < n; ++j) {
		d[j].u *= fac;
		d[j].v *= fac;
		fac *= cnst;
	}
	cnst = .5 * (a + b);
	for (j = 0; j <= n-2; ++j)
		for (k = n - 2; k >= j; --k) {
			d[k].u -= cnst * d[k+1].u;
			d[k].v -= cnst * d[k+1].v;
		}
}
	static void /* column adjust for range -1 to 1 to a to b */
colshft(double a, double b, projUV **d, int n, int m) {
	int k, j;
	double fac, cnst;

	cnst = 2. / (b - a);
	fac = cnst;
	for (j = 1; j < n; ++j) {
		dmult(d[j], fac, m);
		fac *= cnst;
	}
	cnst = .5 * (a + b);
	for (j = 0; j <= n-2; ++j)
		for (k = n - 2; k >= j; --k)
			dadd(d[k], d[k+1], cnst, m);
}
	int /* entry point */
bch2bps(projUV a, projUV b, projUV **c, int nu, int nv) {
	projUV **d;
	int i;

	if (nu < 1 || nv < 1 || !(d = (projUV **)vector2(nu, nv, sizeof(projUV))))
		return 0;
	/* do rows to power series */
	for (i = 0; i < nu; ++i) {
		rows(c[i], d[i], nv);
		rowshft(a.v, b.v, d[i], nv);
	}
	/* do columns to power series */
	cols(d, c, nu, nv);
	colshft(a.u, b.u, c, nu, nv);
	freev2((void **) d, nu);
	return 1;
}
