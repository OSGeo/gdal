#define PJ_LIB__
#include	<projects.h>

PROJ_HEAD(cc, "Central Cylindrical") "\n\tCyl, Sph";
#define EPS10 1.e-10


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	if (fabs (fabs(lp.phi) - M_HALFPI) <= EPS10) F_ERROR;
	xy.x = lp.lam;
	xy.y = tan(lp.phi);
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	(void) P;
	lp.phi = atan(xy.y);
	lp.lam = xy.x;
	return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(cc) {
    P->es = 0.;

    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_cc_selftest (void) {return 0;}
#else

int pj_cc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=cc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223402.14425527418,  111712.41554059254},
        {223402.14425527418,  -111712.41554059254},
        {-223402.14425527418,  111712.41554059254},
        {-223402.14425527418,  -111712.41554059254},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.0017904931097838226,  0.00089524655481905597},
        {0.0017904931097838226,  -0.00089524655481905597},
        {-0.0017904931097838226,  0.00089524655481905597},
        {-0.0017904931097838226,  -0.00089524655481905597},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
