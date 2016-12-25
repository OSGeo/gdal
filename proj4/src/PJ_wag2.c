#define PJ_LIB__
# include	<projects.h>
PROJ_HEAD(wag2, "Wagner II") "\n\tPCyl., Sph.";
#define C_x 0.92483
#define C_y 1.38725
#define C_p1 0.88022
#define C_p2 0.88550


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	lp.phi = aasin (P->ctx,C_p1 * sin (C_p2 * lp.phi));
	xy.x = C_x * lp.lam * cos (lp.phi);
	xy.y = C_y * lp.phi;
	return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	lp.phi = xy.y / C_y;
	lp.lam = xy.x / (C_x * cos(lp.phi));
	lp.phi = aasin (P->ctx,sin(lp.phi) / C_p1) / C_p2;
	return (lp);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(wag2) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;
    return P;
}


#ifndef PJ_SELFTEST
int pj_wag2_selftest (void) {return 0;}
#else

int pj_wag2_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag2   +a=6400000  +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 206589.88809996162,   120778.04035754716},
        { 206589.88809996162,  -120778.04035754716},
        {-206589.88809996162,   120778.04035754716},
        {-206589.88809996162,  -120778.04035754716},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0019360240367390709,   0.00082795765763814082},
        { 0.0019360240367390709,  -0.00082795765763814082},
        {-0.0019360240367390709,   0.00082795765763814082},
        {-0.0019360240367390709,  -0.00082795765763814082},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
