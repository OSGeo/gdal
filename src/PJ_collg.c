#define PJ_LIB__
# include	<projects.h>

PROJ_HEAD(collg, "Collignon") "\n\tPCyl, Sph.";
#define FXC	1.12837916709551257390
#define FYC	1.77245385090551602729
#define ONEEPS	1.0000001


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
	(void) P;
	if ((xy.y = 1. - sin(lp.phi)) <= 0.)
		xy.y = 0.;
	else
		xy.y = sqrt(xy.y);
	xy.x = FXC * lp.lam * xy.y;
	xy.y = FYC * (1. - xy.y);
	return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
	lp.phi = xy.y / FYC - 1.;
	if (fabs(lp.phi = 1. - lp.phi * lp.phi) < 1.)
		lp.phi = asin(lp.phi);
	else if (fabs(lp.phi) > ONEEPS) I_ERROR
	else	lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
	if ((lp.lam = 1. - sin(lp.phi)) <= 0.)
		lp.lam = 0.;
	else
		lp.lam = xy.x / (FXC * sqrt(lp.lam));
	return (lp);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(collg) {
    P->es = 0.0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_collg_selftest (void) {return 0;}
#else

int pj_collg_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=collg   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {249872.921577929839,  99423.1747884602082},
        {254272.532301245432,  -98559.3077607425657},
        {-249872.921577929839,  99423.1747884602082},
        {-254272.532301245432,  -98559.3077607425657},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.00158679719207879865,  0.00101017310941749921},
        {0.001586769215623956,  -0.00101018201458258111},
        {-0.00158679719207879865,  0.00101017310941749921},
        {-0.001586769215623956,  -0.00101018201458258111},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
