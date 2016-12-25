/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Implementation of the airy (Airy) projection.
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

PROJ_HEAD(airy, "Airy") "\n\tMisc Sph, no inv.\n\tno_cut lat_b=";


struct pj_opaque {
	double	p_halfpi;
	double	sinph0;
	double	cosph0;
	double	Cb;
	int		mode;
	int		no_cut;	/* do not cut at hemisphere limit */
};


# define EPS 1.e-10
# define N_POLE	0
# define S_POLE 1
# define EQUIT	2
# define OBLIQ	3



static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double  sinlam, coslam, cosphi, sinphi, t, s, Krho, cosz;

	sinlam = sin(lp.lam);
	coslam = cos(lp.lam);
	switch (Q->mode) {
	case EQUIT:
	case OBLIQ:
		sinphi = sin(lp.phi);
		cosphi = cos(lp.phi);
		cosz = cosphi * coslam;
		if (Q->mode == OBLIQ)
			cosz = Q->sinph0 * sinphi + Q->cosph0 * cosz;
		if (!Q->no_cut && cosz < -EPS)
			F_ERROR;
		if (fabs(s = 1. - cosz) > EPS) {
			t = 0.5 * (1. + cosz);
			Krho = -log(t)/s - Q->Cb / t;
		} else
			Krho = 0.5 - Q->Cb;
		xy.x = Krho * cosphi * sinlam;
		if (Q->mode == OBLIQ)
			xy.y = Krho * (Q->cosph0 * sinphi -
				Q->sinph0 * cosphi * coslam);
		else
			xy.y = Krho * sinphi;
		break;
	case S_POLE:
	case N_POLE:
		lp.phi = fabs(Q->p_halfpi - lp.phi);
		if (!Q->no_cut && (lp.phi - EPS) > M_HALFPI)
			F_ERROR;
		if ((lp.phi *= 0.5) > EPS) {
			t = tan(lp.phi);
			Krho = -2.*(log(cos(lp.phi)) / t + t * Q->Cb);
			xy.x = Krho * sinlam;
			xy.y = Krho * coslam;
			if (Q->mode == N_POLE)
				xy.y = -xy.y;
		} else
			xy.x = xy.y = 0.;
	}
	return xy;
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


PJ *PROJECTION(airy) {
	double beta;

    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);

    P->opaque = Q;

	Q->no_cut = pj_param(P->ctx, P->params, "bno_cut").i;
	beta = 0.5 * (M_HALFPI - pj_param(P->ctx, P->params, "rlat_b").f);
	if (fabs(beta) < EPS)
		Q->Cb = -0.5;
	else {
		Q->Cb = 1./tan(beta);
		Q->Cb *= Q->Cb * log(cos(beta));
	}

	if (fabs(fabs(P->phi0) - M_HALFPI) < EPS)
		if (P->phi0 < 0.) {
			Q->p_halfpi = -M_HALFPI;
			Q->mode = S_POLE;
		} else {
			Q->p_halfpi =  M_HALFPI;
			Q->mode = N_POLE;
		}
	else {
		if (fabs(P->phi0) < EPS)
			Q->mode = EQUIT;
		else {
			Q->mode = OBLIQ;
			Q->sinph0 = sin(P->phi0);
			Q->cosph0 = cos(P->phi0);
		}
	}
	P->fwd = s_forward;
	P->es = 0.;
    return P;
}


#ifndef PJ_SELFTEST
int pj_airy_selftest (void) {return 0;}
#else

int pj_airy_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=airy   +a=6400000    +lat_1=0 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 189109.88690862127,   94583.752387504152},
        { 189109.88690862127,  -94583.752387504152},
        {-189109.88690862127,   94583.752387504152},
        {-189109.88690862127,  -94583.752387504152},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 0, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}

#endif
