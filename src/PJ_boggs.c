#define PJ_LIB__
# include	<projects.h>
PROJ_HEAD(boggs, "Boggs Eumorphic") "\n\tPCyl., no inv., Sph.";
# define NITER	20
# define EPS	1e-7
# define ONETOL 1.000001
# define FXC	2.00276
# define FXC2	1.11072
# define FYC	0.49931


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	double theta, th1, c;
	int i;
	(void) P;

	theta = lp.phi;
	if (fabs(fabs(lp.phi) - M_HALFPI) < EPS)
		xy.x = 0.;
	else {
		c = sin(theta) * M_PI;
		for (i = NITER; i; --i) {
			theta -= th1 = (theta + sin(theta) - c) /
				(1. + cos(theta));
			if (fabs(th1) < EPS) break;
		}
		theta *= 0.5;
		xy.x = FXC * lp.lam / (1. / cos(lp.phi) + FXC2 / cos(theta));
	}
	xy.y = FYC * (lp.phi + M_SQRT2 * sin(theta));
	return (xy);
}


static void *freeup_new (PJ *P) {                        /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(boggs) {
    P->es = 0.;
    P->fwd = s_forward;
    return P;
}

#ifndef PJ_SELFTEST
int pj_boggs_selftest (void) {return 0;}
#else
int pj_boggs_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=boggs   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 211949.70080818201,   117720.99830541089},
        { 211949.70080818201,  -117720.99830541089},
        {-211949.70080818201,   117720.99830541089},
        {-211949.70080818201,  -117720.99830541089},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 0, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif
