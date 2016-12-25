# define HLFPI2	2.46740110027233965467      /* (pi/2)^2 */
# define EPS	1e-10
#define PJ_LIB__
#include	<projects.h>


struct pj_opaque {
	int bacn;
	int ortl;
};

PROJ_HEAD(apian, "Apian Globular I") "\n\tMisc Sph, no inv.";
PROJ_HEAD(ortel, "Ortelius Oval") "\n\tMisc Sph, no inv.";
PROJ_HEAD(bacon, "Bacon Globular") "\n\tMisc Sph, no inv.";


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double ax, f;

	xy.y = Q->bacn ? M_HALFPI * sin(lp.phi) : lp.phi;
	if ((ax = fabs(lp.lam)) >= EPS) {
		if (Q->ortl && ax >= M_HALFPI)
			xy.x = sqrt(HLFPI2 - lp.phi * lp.phi + EPS) + ax - M_HALFPI;
		else {
			f = 0.5 * (HLFPI2 / ax + ax);
			xy.x = ax - f + sqrt(f * f - xy.y * xy.y);
		}
		if (lp.lam < 0.) xy.x = - xy.x;
	} else
		xy.x = 0.;
	return (xy);
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


PJ *PROJECTION(bacon) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->bacn = 1;
	Q->ortl = 0;
	P->es = 0.;
    P->fwd = s_forward;
    return P;
}


PJ *PROJECTION(apian) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->bacn = Q->ortl = 0;
	P->es = 0.;
    P->fwd = s_forward;
    return P;
}


PJ *PROJECTION(ortel) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->bacn = 0;
	Q->ortl = 1;
	P->es = 0.;
    P->fwd = s_forward;
    return P;
}


#ifndef PJ_SELFTEST
int pj_bacon_selftest (void) {return 0;}
#else
int pj_bacon_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=bacon   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {223334.13255596498,  175450.72592266591},
        {223334.13255596498,  -175450.72592266591},
        {-223334.13255596498,  175450.72592266591},
        {-223334.13255596498,  -175450.72592266591},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 0, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif




#ifndef PJ_SELFTEST
int pj_apian_selftest (void) {return 0;}
#else
int pj_apian_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=apian   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223374.57735525275,   111701.07212763709},
        { 223374.57735525275,  -111701.07212763709},
        {-223374.57735525275,   111701.07212763709},
        {-223374.57735525275,  -111701.07212763709},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 0, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif




#ifndef PJ_SELFTEST
int pj_ortel_selftest (void) {return 0;}
#else
int pj_ortel_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=ortel   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223374.57735525275,   111701.07212763709},
        { 223374.57735525275,  -111701.07212763709},
        {-223374.57735525275,   111701.07212763709},
        {-223374.57735525275,  -111701.07212763709},
    };
    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 0, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}
#endif

