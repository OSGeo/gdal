#define PJ_LIB__
#include	<projects.h>


PROJ_HEAD(august, "August Epicycloidal") "\n\tMisc Sph, no inv.";
#define M 1.333333333333333


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	double t, c1, c, x1, x12, y1, y12;
	(void) P;

	t = tan(.5 * lp.phi);
	c1 = sqrt(1. - t * t);
	c = 1. + c1 * cos(lp.lam *= .5);
	x1 = sin(lp.lam) *  c1 / c;
	y1 =  t / c;
	xy.x = M * x1 * (3. + (x12 = x1 * x1) - 3. * (y12 = y1 *  y1));
	xy.y = M * y1 * (3. + 3. * x12 - y12);
	return (xy);
}



static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(august) {
    P->inv = 0;
    P->fwd = s_forward;
    P->es = 0.;
    return P;
}

#ifndef PJ_SELFTEST
int pj_august_selftest (void) {return 0;}
#else

int pj_august_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=august   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223404.97818097242,  111722.34028976287},
        {223404.97818097242,  -111722.34028976287},
        {-223404.97818097242,  111722.34028976287},
        {-223404.97818097242,  -111722.34028976287},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
