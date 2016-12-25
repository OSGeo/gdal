#define PJ_LIB__
#include	<projects.h>

PROJ_HEAD(wag7, "Wagner VII") "\n\tMisc Sph, no inv.";



static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
	double theta, ct, D;

	(void) P; /* Shut up compiler warnnings about unused P */

	theta = asin (xy.y = 0.90630778703664996 * sin(lp.phi));
	xy.x  = 2.66723 * (ct = cos (theta)) * sin (lp.lam /= 3.);
	xy.y *= 1.24104 * (D = 1/(sqrt (0.5 * (1 + ct * cos (lp.lam)))));
	xy.x *= D;
	return (xy);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}



PJ *PROJECTION(wag7) {
    P->fwd = s_forward;
    P->inv = 0;
    P->es = 0.;
    return P;
}


#ifndef PJ_SELFTEST
int pj_wag7_selftest (void) {return 0;}
#else

int pj_wag7_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag7   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 198601.87695731167,  125637.0457141714},
        { 198601.87695731167, -125637.0457141714},
        {-198601.87695731167,  125637.0457141714},
        {-198601.87695731167, -125637.0457141714},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif
