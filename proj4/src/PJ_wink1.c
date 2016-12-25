#define PJ_LIB__
#include <projects.h>
PROJ_HEAD(wink1, "Winkel I") "\n\tPCyl., Sph.\n\tlat_ts=";


struct pj_opaque {
	double	cosphi1;
};



static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	xy.x = .5 * lp.lam * (P->opaque->cosphi1 + cos(lp.phi));
	xy.y = lp.phi;
	return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	lp.phi = xy.y;
	lp.lam = 2. * xy.x / (P->opaque->cosphi1 + cos(lp.phi));
	return (lp);
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


PJ *PROJECTION(wink1) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	P->opaque->cosphi1 = cos (pj_param(P->ctx, P->params, "rlat_ts").f);
	P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_wink1_selftest (void) {return 0;}
#else

int pj_wink1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wink1   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223385.13164095284,  111701.07212763709},
        { 223385.13164095284,  -111701.07212763709},
        {-223385.13164095284,  111701.07212763709},
        {-223385.13164095284,  -111701.07212763709},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931098931057,  0.00089524655489191132},
        { 0.0017904931098931057, -0.00089524655489191132},
        {-0.0017904931098931057,  0.00089524655489191132},
        {-0.0017904931098931057, -0.00089524655489191132},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
