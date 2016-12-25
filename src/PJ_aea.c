/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Implementation of the aea (Albers Equal Area) projection.
 *           and the leac (Lambert Equal Area Conic) projection
 * Author:   Gerald Evenden (1995)
 *           Thomas Knudsen (2016) - revise/add regression tests
 *
 ******************************************************************************
 * Copyright (c) 1995, Gerald Evenden
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#define PJ_LIB__
#include <projects.h>

# define EPS10	1.e-10
# define TOL7	1.e-7

PROJ_HEAD(aea, "Albers Equal Area") "\n\tConic Sph&Ell\n\tlat_1= lat_2=";
PROJ_HEAD(leac, "Lambert Equal Area Conic") "\n\tConic, Sph&Ell\n\tlat_1= south";


/* determine latitude angle phi-1 */
# define N_ITER 15
# define EPSILON 1.0e-7
# define TOL 1.0e-10
static double phi1_(double qs, double Te, double Tone_es) {
	int i;
	double Phi, sinpi, cospi, con, com, dphi;

	Phi = asin (.5 * qs);
	if (Te < EPSILON)
		return( Phi );
	i = N_ITER;
	do {
		sinpi = sin (Phi);
		cospi = cos (Phi);
		con = Te * sinpi;
		com = 1. - con * con;
		dphi = .5 * com * com / cospi * (qs / Tone_es -
		   sinpi / com + .5 / Te * log ((1. - con) /
		   (1. + con)));
		Phi += dphi;
	} while (fabs(dphi) > TOL && --i);
	return( i ? Phi : HUGE_VAL );
}


struct pj_opaque {
	double	ec;
	double	n;
	double	c;
	double	dd;
	double	n2;
	double	rho0;
	double	rho;
	double	phi1;
	double	phi2;
	double	*en;
	int		ellips;
};



static XY e_forward (LP lp, PJ *P) {   /* Ellipsoid/spheroid, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	if ((Q->rho = Q->c - (Q->ellips ? Q->n * pj_qsfn(sin(lp.phi),
		P->e, P->one_es) : Q->n2 * sin(lp.phi))) < 0.) F_ERROR
	Q->rho = Q->dd * sqrt(Q->rho);
	xy.x = Q->rho * sin( lp.lam *= Q->n );
	xy.y = Q->rho0 - Q->rho * cos(lp.lam);
	return xy;
}


static LP e_inverse (XY xy, PJ *P) {   /* Ellipsoid/spheroid, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	if( (Q->rho = hypot(xy.x, xy.y = Q->rho0 - xy.y)) != 0.0 ) {
		if (Q->n < 0.) {
			Q->rho = -Q->rho;
			xy.x = -xy.x;
			xy.y = -xy.y;
		}
		lp.phi =  Q->rho / Q->dd;
		if (Q->ellips) {
			lp.phi = (Q->c - lp.phi * lp.phi) / Q->n;
			if (fabs(Q->ec - fabs(lp.phi)) > TOL7) {
				if ((lp.phi = phi1_(lp.phi, P->e, P->one_es)) == HUGE_VAL)
					I_ERROR
			} else
				lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
		} else if (fabs(lp.phi = (Q->c - lp.phi * lp.phi) / Q->n2) <= 1.)
			lp.phi = asin(lp.phi);
		else
			lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
		lp.lam = atan2(xy.x, xy.y) / Q->n;
	} else {
		lp.lam = 0.;
		lp.phi = Q->n > 0. ? M_HALFPI : - M_HALFPI;
	}
	return lp;
}


static void *freeup_new (PJ *P) {                        /* Destructor */
    if (0==P)
        return 0;

    if (0==P->opaque)
        return pj_dealloc (P);

    pj_dealloc (P->opaque->en);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


static PJ *setup(PJ *P) {
	double cosphi, sinphi;
	int secant;
    struct pj_opaque *Q = P->opaque;

    P->inv = e_inverse;
    P->fwd = e_forward;

	if (fabs(Q->phi1 + Q->phi2) < EPS10) E_ERROR(-21);
	Q->n = sinphi = sin(Q->phi1);
	cosphi = cos(Q->phi1);
	secant = fabs(Q->phi1 - Q->phi2) >= EPS10;
	if( (Q->ellips = (P->es > 0.))) {
		double ml1, m1;

		if (!(Q->en = pj_enfn(P->es))) E_ERROR_0;
		m1 = pj_msfn(sinphi, cosphi, P->es);
		ml1 = pj_qsfn(sinphi, P->e, P->one_es);
		if (secant) { /* secant cone */
			double ml2, m2;

			sinphi = sin(Q->phi2);
			cosphi = cos(Q->phi2);
			m2 = pj_msfn(sinphi, cosphi, P->es);
			ml2 = pj_qsfn(sinphi, P->e, P->one_es);
			Q->n = (m1 * m1 - m2 * m2) / (ml2 - ml1);
		}
		Q->ec = 1. - .5 * P->one_es * log((1. - P->e) /
			(1. + P->e)) / P->e;
		Q->c = m1 * m1 + Q->n * ml1;
		Q->dd = 1. / Q->n;
		Q->rho0 = Q->dd * sqrt(Q->c - Q->n * pj_qsfn(sin(P->phi0),
			P->e, P->one_es));
	} else {
		if (secant) Q->n = .5 * (Q->n + sin(Q->phi2));
		Q->n2 = Q->n + Q->n;
		Q->c = cosphi * cosphi + Q->n2 * sinphi;
		Q->dd = 1. / Q->n;
		Q->rho0 = Q->dd * sqrt(Q->c - Q->n2 * sin(P->phi0));
	}

	return P;
}


PJ *PROJECTION(aea) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->phi1 = pj_param(P->ctx, P->params, "rlat_1").f;
	Q->phi2 = pj_param(P->ctx, P->params, "rlat_2").f;
    setup(P);
    return P;
}


PJ *PROJECTION(leac) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	Q->phi2 = pj_param(P->ctx, P->params, "rlat_1").f;
	Q->phi1 = pj_param(P->ctx, P->params, "bsouth").i ? - M_HALFPI: M_HALFPI;
    setup (P);
    return P;
}


#ifndef PJ_SELFTEST
int pj_aea_selftest (void) {return 10000;}
#else

int pj_aea_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=aea   +ellps=GRS80  +lat_1=0 +lat_2=2"};
    char s_args[] = {"+proj=aea   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222571.60875710563,  110653.32674302977},
        {222706.30650839131,  -110484.26714439997},
        {-222571.60875710563,  110653.32674302977},
        {-222706.30650839131,  -110484.26714439997},
    };

    XY s_fwd_expect[] = {
        {223334.08517088494,  111780.43188447191},
        {223470.15499168713,  -111610.33943099028},
        {-223334.08517088494,  111780.43188447191},
        {-223470.15499168713,  -111610.33943099028},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966310597749514,  0.00090436885862202158},
        {0.0017966300767030448,  -0.00090437009538581453},
        {-0.0017966310597749514,  0.00090436885862202158},
        {-0.0017966300767030448,  -0.00090437009538581453},
    };

    LP s_inv_expect[] = {
        {0.0017904935979658752,  0.00089524594491375306},
        {0.0017904926216016812,  -0.00089524716502493225},
        {-0.0017904935979658752,  0.00089524594491375306},
        {-0.0017904926216016812,  -0.00089524716502493225},
    };
    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif



#ifndef PJ_SELFTEST
int pj_leac_selftest (void) {return 10000;}
#else

int pj_leac_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=leac   +ellps=GRS80  +lat_1=0 +lat_2=2"};
    char s_args[] = {"+proj=leac   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {220685.14054297868,  112983.50088939646},
        {224553.31227982609,  -108128.63674487274},
        {-220685.14054297868,  112983.50088939646},
        {-224553.31227982609,  -108128.63674487274},
    };

    XY s_fwd_expect[] = {
        {221432.86859285168,  114119.45452653214},
        {225331.72412711097,  -109245.82943505641},
        {-221432.86859285168,  114119.45452653214},
        {-225331.72412711097,  -109245.82943505641},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966446840328458,  0.00090435171340223211},
        {0.0017966164523713021,  -0.00090438724081843625},
        {-0.0017966446840328458,  0.00090435171340223211},
        {-0.0017966164523713021,  -0.00090438724081843625},
    };

    LP s_inv_expect[] = {
        {0.0017905070979748127,  0.00089522906964877795},
        {0.001790479121519977,  -0.00089526404022281043},
        {-0.0017905070979748127,  0.00089522906964877795},
        {-0.001790479121519977,  -0.00089526404022281043},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}
#endif
