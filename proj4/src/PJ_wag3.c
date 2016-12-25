#define PJ_LIB__
# include	<projects.h>
PROJ_HEAD(wag3, "Wagner III") "\n\tPCyl., Sph.\n\tlat_ts=";
#define TWOTHIRD 0.6666666666666666666667

struct pj_opaque {
	double	C_x;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	xy.x = P->opaque->C_x * lp.lam * cos(TWOTHIRD * lp.phi);
	xy.y = lp.phi;
	return xy;
}


#if 0
INVERSE(s_inverse); /* spheroid */
#endif


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	lp.phi = xy.y;
	lp.lam = xy.x / (P->opaque->C_x * cos(TWOTHIRD * lp.phi));
	return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(wag3) {
	double ts;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	ts = pj_param (P->ctx, P->params, "rlat_ts").f;
	P->opaque->C_x = cos (ts) / cos (2.*ts/3.);
	P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_wag3_selftest (void) {return 0;}
#else

int pj_wag3_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag3   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223387.02171816575,  111701.07212763709},
        {223387.02171816575,  -111701.07212763709},
        {-223387.02171816575,  111701.07212763709},
        {-223387.02171816575,  -111701.07212763709},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.001790493109880963,  0.00089524655489191132},
        {0.001790493109880963,  -0.00089524655489191132},
        {-0.001790493109880963,  0.00089524655489191132},
        {-0.001790493109880963,  -0.00089524655489191132},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
