#define PJ_LIB__
#include	<projects.h>

PROJ_HEAD(oea, "Oblated Equal Area") "\n\tMisc Sph\n\tn= m= theta=";

struct pj_opaque {
	double	theta;
	double	m, n;
	double	two_r_m, two_r_n, rm, rn, hm, hn;
	double	cp0, sp0;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double Az, M, N, cp, sp, cl, shz;

	cp = cos(lp.phi);
	sp = sin(lp.phi);
	cl = cos(lp.lam);
	Az = aatan2(cp * sin(lp.lam), Q->cp0 * sp - Q->sp0 * cp * cl) + Q->theta;
	shz = sin(0.5 * aacos(P->ctx, Q->sp0 * sp + Q->cp0 * cp * cl));
	M = aasin(P->ctx, shz * sin(Az));
	N = aasin(P->ctx, shz * cos(Az) * cos(M) / cos(M * Q->two_r_m));
	xy.y = Q->n * sin(N * Q->two_r_n);
	xy.x = Q->m * sin(M * Q->two_r_m) * cos(N) / cos(N * Q->two_r_n);

	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double N, M, xp, yp, z, Az, cz, sz, cAz;

	N = Q->hn * aasin(P->ctx,xy.y * Q->rn);
	M = Q->hm * aasin(P->ctx,xy.x * Q->rm * cos(N * Q->two_r_n) / cos(N));
	xp = 2. * sin(M);
	yp = 2. * sin(N) * cos(M * Q->two_r_m) / cos(M);
	cAz = cos(Az = aatan2(xp, yp) - Q->theta);
	z = 2. * aasin(P->ctx, 0.5 * hypot(xp, yp));
	sz = sin(z);
	cz = cos(z);
	lp.phi = aasin(P->ctx, Q->sp0 * cz + Q->cp0 * sz * cAz);
	lp.lam = aatan2(sz * sin(Az),
		Q->cp0 * cz - Q->sp0 * sz * cAz);

	return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(oea) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	if (((Q->n = pj_param(P->ctx, P->params, "dn").f) <= 0.) ||
		((Q->m = pj_param(P->ctx, P->params, "dm").f) <= 0.))
		E_ERROR(-39)
	else {
		Q->theta = pj_param(P->ctx, P->params, "rtheta").f;
		Q->sp0 = sin(P->phi0);
		Q->cp0 = cos(P->phi0);
		Q->rn = 1./ Q->n;
		Q->rm = 1./ Q->m;
		Q->two_r_n = 2. * Q->rn;
		Q->two_r_m = 2. * Q->rm;
		Q->hm = 0.5 * Q->m;
		Q->hn = 0.5 * Q->n;
		P->fwd = s_forward;
		P->inv = s_inverse;
		P->es = 0.;
	}

    return P;
}


#ifndef PJ_SELFTEST
int pj_oea_selftest (void) {return 0;}
#else

int pj_oea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=oea   +a=6400000    +lat_1=0.5 +lat_2=2 +n=1 +m=2 +theta=3"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 228926.872097864107,  99870.4884300760023},
        { 217242.584036940476, -123247.885607474513},
        {-217242.584036940476,  123247.885607474556},
        {-228926.872097864078, -99870.4884300760168},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017411857167771369,   0.000987726819566195693},
        { 0.00183489288577854998, -0.000800312481495174641},
        {-0.00183489288577854954,  0.000800312481495174966},
        {-0.00174118571677713712, -0.000987726819566195043},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
