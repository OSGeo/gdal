#define PJ_LIB__
# include	<projects.h>

PROJ_HEAD(wink2, "Winkel II") "\n\tPCyl., Sph., no inv.\n\tlat_1=";

struct pj_opaque { double	cosphi1; };

#define MAX_ITER    10
#define LOOP_TOL    1e-7


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
	double k, V;
	int i;

	xy.y = lp.phi * M_TWO_D_PI;
	k = M_PI * sin (lp.phi);
	lp.phi *= 1.8;
	for (i = MAX_ITER; i ; --i) {
		lp.phi -= V = (lp.phi + sin (lp.phi) - k) /
			(1. + cos (lp.phi));
		if (fabs (V) < LOOP_TOL)
			break;
	}
	if (!i)
		lp.phi = (lp.phi < 0.) ? -M_HALFPI : M_HALFPI;
	else
		lp.phi *= 0.5;
	xy.x = 0.5 * lp.lam * (cos (lp.phi) + P->opaque->cosphi1);
	xy.y = M_FORTPI * (sin (lp.phi) + xy.y);
	return xy;
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


PJ *PROJECTION(wink2) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	P->opaque->cosphi1 = cos(pj_param(P->ctx, P->params, "rlat_1").f);
	P->es  = 0.;
    P->inv = 0;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_wink2_selftest (void) {return 0;}
#else

int pj_wink2_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wink2   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223387.39643378611,  124752.03279744535},
        { 223387.39643378611, -124752.03279744535},
        {-223387.39643378611,  124752.03279744535},
        {-223387.39643378611, -124752.03279744535},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif
