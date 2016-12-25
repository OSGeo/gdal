#define PJ_LIB__
#include	<projects.h>

PROJ_HEAD(urmfps, "Urmaev Flat-Polar Sinusoidal") "\n\tPCyl, Sph.\n\tn=";
PROJ_HEAD(wag1, "Wagner I (Kavraisky VI)") "\n\tPCyl, Sph.";

struct pj_opaque {
	double	n, C_y;
};

#define C_x 0.8773826753
#define Cy 1.139753528477


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
	lp.phi = aasin (P->ctx,P->opaque->n * sin (lp.phi));
	xy.x = C_x * lp.lam * cos (lp.phi);
	xy.y = P->opaque->C_y * lp.phi;
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0, 0.0};
	xy.y /= P->opaque->C_y;
	lp.phi = aasin(P->ctx, sin (xy.y) / P->opaque->n);
	lp.lam = xy.x / (C_x * cos (xy.y));
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

static PJ *setup(PJ *P) {
	P->opaque->C_y = Cy / P->opaque->n;
	P->es = 0.;
	P->inv = s_inverse;
	P->fwd = s_forward;
	return P;
}


PJ *PROJECTION(urmfps) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	if (pj_param(P->ctx, P->params, "tn").i) {
		P->opaque->n = pj_param(P->ctx, P->params, "dn").f;
		if (P->opaque->n <= 0. || P->opaque->n > 1.)
			E_ERROR(-40)
	} else
		E_ERROR(-40)

    return setup(P);
}


PJ *PROJECTION(wag1) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	P->opaque->n = 0.8660254037844386467637231707;
    return setup(P);
}


#ifndef PJ_SELFTEST
int pj_urmfps_selftest (void) {return 0;}
#else
int pj_urmfps_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=urmfps   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 196001.70813419219,  127306.84332999329},
        { 196001.70813419219, -127306.84332999329},
        {-196001.70813419219,  127306.84332999329},
        {-196001.70813419219, -127306.84332999329},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.002040720839642371,  0.00078547381740438178},
        { 0.002040720839642371, -0.00078547381740438178},
        {-0.002040720839642371,  0.00078547381740438178},
        {-0.002040720839642371, -0.00078547381740438178},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}
#endif


#ifndef PJ_SELFTEST
int pj_wag1_selftest (void) {return 0;}
#else
int pj_wag1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag1   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 195986.78156115755,  127310.07506065986},
        { 195986.78156115755, -127310.07506065986},
        {-195986.78156115755,  127310.07506065986},
        {-195986.78156115755, -127310.07506065986},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.002040720839738254,  0.00078547381739207999},
        { 0.002040720839738254, -0.00078547381739207999},
        {-0.002040720839738254,  0.00078547381739207999},
        {-0.002040720839738254, -0.00078547381739207999},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}
#endif

