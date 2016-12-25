#define PJ_LIB__
#include	<projects.h>

PROJ_HEAD(bonne, "Bonne (Werner lat_1=90)")
	"\n\tConic Sph&Ell\n\tlat_1=";
#define EPS10	1e-10

struct pj_opaque {
	double phi1;
	double cphi1;
	double am1;
	double m1;
	double *en;
};


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double rh, E, c;

	rh = Q->am1 + Q->m1 - pj_mlfn(lp.phi, E = sin(lp.phi), c = cos(lp.phi), Q->en);
	E = c * lp.lam / (rh * sqrt(1. - P->es * E * E));
	xy.x = rh * sin(E);
	xy.y = Q->am1 - rh * cos(E);
	return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double E, rh;

	rh = Q->cphi1 + Q->phi1 - lp.phi;
	if (fabs(rh) > EPS10) {
		xy.x = rh * sin(E = lp.lam * cos(lp.phi) / rh);
		xy.y = Q->cphi1 - rh * cos(E);
	} else
		xy.x = xy.y = 0.;
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double rh;

	rh = hypot(xy.x, xy.y = Q->cphi1 - xy.y);
	lp.phi = Q->cphi1 + Q->phi1 - rh;
	if (fabs(lp.phi) > M_HALFPI) I_ERROR;
	if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10)
		lp.lam = 0.;
	else
		lp.lam = rh * atan2(xy.x, xy.y) / cos(lp.phi);
	return lp;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double s, rh;

	rh = hypot(xy.x, xy.y = Q->am1 - xy.y);
	lp.phi = pj_inv_mlfn(P->ctx, Q->am1 + Q->m1 - rh, P->es, Q->en);
	if ((s = fabs(lp.phi)) < M_HALFPI) {
		s = sin(lp.phi);
		lp.lam = rh * atan2(xy.x, xy.y) *
		   sqrt(1. - P->es * s * s) / cos(lp.phi);
	} else if (fabs(s - M_HALFPI) <= EPS10)
		lp.lam = 0.;
	else I_ERROR;
	return lp;
}


static void *freeup_new (PJ *P) {                        /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque->en);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(bonne) {
	double c;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
	if (fabs(Q->phi1) < EPS10) E_ERROR(-23);
	if (P->es) {
		Q->en = pj_enfn(P->es);
		Q->m1 = pj_mlfn(Q->phi1, Q->am1 = sin(Q->phi1),
			c = cos(Q->phi1), Q->en);
		Q->am1 = c / (sqrt(1. - P->es * Q->am1 * Q->am1) * Q->am1);
		P->inv = e_inverse;
		P->fwd = e_forward;
	} else {
		if (fabs(Q->phi1) + EPS10 >= M_HALFPI)
			Q->cphi1 = 0.;
		else
			Q->cphi1 = 1. / tan(Q->phi1);
		P->inv = s_inverse;
		P->fwd = s_forward;
	}
    return P;
}


#ifndef PJ_SELFTEST
int pj_bonne_selftest (void) {return 0;}
#else
int pj_bonne_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=bonne   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=bonne   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222605.29609715697,   55321.139565494814},
        { 222605.29609923941,  -165827.64779905154},
        {-222605.29609715697,   55321.139565494814},
        {-222605.29609923941,  -165827.64779905154},
    };

    XY s_fwd_expect[] = {
        { 223368.11557252839,   55884.555246393575},
        { 223368.11557463196,  -167517.59936969393},
        {-223368.11557252839,   55884.555246393575},
        {-223368.11557463196,  -167517.59936969393},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966987691132891,  0.50090436853737497},
        { 0.0017966982774478867,  0.4990956309655612},
        {-0.0017966987691132891,  0.50090436853737497},
        {-0.0017966982774478867,  0.4990956309655612},
    };

    LP s_inv_expect[] = {
        { 0.0017905615332457991,  0.50089524631087834},
        { 0.0017905610449335603,  0.49910475320072978},
        {-0.0017905615332457991,  0.50089524631087834},
        {-0.0017905610449335603,  0.49910475320072978},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
