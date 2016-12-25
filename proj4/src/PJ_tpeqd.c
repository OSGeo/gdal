#define PJ_LIB__
#include	<projects.h>


PROJ_HEAD(tpeqd, "Two Point Equidistant")
	"\n\tMisc Sph\n\tlat_1= lon_1= lat_2= lon_2=";

struct pj_opaque {
	double cp1, sp1, cp2, sp2, ccs, cs, sc, r2z0, z02, dlam2; \
	double hz0, thz0, rhshz0, ca, sa, lp, lamc;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
    struct pj_opaque *Q = P->opaque;
	double t, z1, z2, dl1, dl2, sp, cp;

	sp = sin(lp.phi);
	cp = cos(lp.phi);
	z1 = aacos(P->ctx, Q->sp1 * sp + Q->cp1 * cp * cos (dl1 = lp.lam + Q->dlam2));
	z2 = aacos(P->ctx, Q->sp2 * sp + Q->cp2 * cp * cos (dl2 = lp.lam - Q->dlam2));
	z1 *= z1;
	z2 *= z2;

	xy.x = Q->r2z0 * (t = z1 - z2);
	t = Q->z02 - t;
	xy.y = Q->r2z0 * asqrt (4. * Q->z02 * z2 - t * t);
	if ((Q->ccs * sp - cp * (Q->cs * sin(dl1) - Q->sc * sin(dl2))) < 0.)
		xy.y = -xy.y;
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double cz1, cz2, s, d, cp, sp;

	cz1 = cos (hypot(xy.y, xy.x + Q->hz0));
	cz2 = cos (hypot(xy.y, xy.x - Q->hz0));
	s = cz1 + cz2;
	d = cz1 - cz2;
	lp.lam = - atan2(d, (s * Q->thz0));
	lp.phi = aacos(P->ctx, hypot (Q->thz0 * s, d) * Q->rhshz0);
	if ( xy.y < 0. )
		lp.phi = - lp.phi;
	/* lam--phi now in system relative to P1--P2 base equator */
	sp = sin (lp.phi);
	cp = cos (lp.phi);
	lp.phi = aasin (P->ctx, Q->sa * sp + Q->ca * cp * (s = cos(lp.lam -= Q->lp)));
	lp.lam = atan2 (cp * sin(lp.lam), Q->sa * cp * s - Q->ca * sp) + Q->lamc;
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


PJ *PROJECTION(tpeqd) {
	double lam_1, lam_2, phi_1, phi_2, A12, pp;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;


	/* get control point locations */
	phi_1 = pj_param(P->ctx, P->params, "rlat_1").f;
	lam_1 = pj_param(P->ctx, P->params, "rlon_1").f;
	phi_2 = pj_param(P->ctx, P->params, "rlat_2").f;
	lam_2 = pj_param(P->ctx, P->params, "rlon_2").f;

	if (phi_1 == phi_2 && lam_1 == lam_2)
        E_ERROR(-25);
	P->lam0  = adjlon (0.5 * (lam_1 + lam_2));
	Q->dlam2 = adjlon (lam_2 - lam_1);

	Q->cp1 = cos (phi_1);
	Q->cp2 = cos (phi_2);
	Q->sp1 = sin (phi_1);
	Q->sp2 = sin (phi_2);
	Q->cs = Q->cp1 * Q->sp2;
	Q->sc = Q->sp1 * Q->cp2;
	Q->ccs = Q->cp1 * Q->cp2 * sin(Q->dlam2);
	Q->z02 = aacos(P->ctx, Q->sp1 * Q->sp2 + Q->cp1 * Q->cp2 * cos (Q->dlam2));
	Q->hz0 = .5 * Q->z02;
	A12 = atan2(Q->cp2 * sin (Q->dlam2),
		Q->cp1 * Q->sp2 - Q->sp1 * Q->cp2 * cos (Q->dlam2));
	Q->ca = cos(pp = aasin(P->ctx, Q->cp1 * sin(A12)));
	Q->sa = sin(pp);
	Q->lp = adjlon ( atan2 (Q->cp1 * cos(A12), Q->sp1) - Q->hz0);
	Q->dlam2 *= .5;
	Q->lamc = M_HALFPI - atan2(sin(A12) * Q->sp1, cos(A12)) - Q->dlam2;
	Q->thz0 = tan (Q->hz0);
	Q->rhshz0 = .5 / sin (Q->hz0);
	Q->r2z0 = 0.5 / Q->z02;
	Q->z02 *= Q->z02;

    P->inv = s_inverse;
    P->fwd = s_forward;
	P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_tpeqd_selftest (void) {return 0;}
#else

int pj_tpeqd_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=tpeqd   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=tpeqd   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {-27750.758831679042,  -222599.40369177726},
        {-250434.93702403645,  -222655.93819326628},
        {-27750.758831679042,  222599.40369177726},
        {-250434.93702403645,  222655.93819326628},
    };

    XY s_fwd_expect[] = {
        {-27845.882978485075,  -223362.43069526015},
        {-251293.37876465076,  -223419.15898590829},
        {-27845.882978485075,  223362.43069526015},
        {-251293.37876465076,  223419.15898590829},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {-0.00089855554821257374,  1.2517966304145272},
        {0.0008985555481998515,  1.2517966304145272},
        {-0.00089855431859741167,  1.2482033692781642},
        {0.00089855431859741167,  1.2482033692781642},
    };

    LP s_inv_expect[] = {
        {-0.00089548606640108474,  1.2517904929571837},
        {0.0008954860663883625,  1.2517904929571837},
        {-0.000895484845182587,  1.248209506737604},
        {0.00089548484516986475,  1.248209506737604},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
