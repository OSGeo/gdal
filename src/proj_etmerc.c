/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2008   Gerald I. Evenden
*/

/*
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* The code in this file is largly based upon procedures:
 *
 * Written by: Knud Poder and Karsten Engsager
 *
 * Based on math from: R.Koenig and K.H. Weise, "Mathematische
 * Grundlagen der hoeheren Geodaesie und Kartographie,
 * Springer-Verlag, Berlin/Goettingen" Heidelberg, 1951.
 *
 * Modified and used here by permission of Reference Networks
 * Division, Kort og Matrikelstyrelsen (KMS), Copenhagen, Denmark
 *
*/


#define PROJ_LIB__
#define PJ_LIB__

#include <projects.h>


struct pj_opaque {
    double    Qn;    /* Merid. quad., scaled to the projection */ \
    double    Zb;    /* Radius vector in polar coord. systems  */ \
    double    cgb[6]; /* Constants for Gauss -> Geo lat */ \
    double    cbg[6]; /* Constants for Geo lat -> Gauss */ \
    double    utg[6]; /* Constants for transv. merc. -> geo */ \
    double    gtu[6]; /* Constants for geo -> transv. merc. */
};

PROJ_HEAD(etmerc, "Extended Transverse Mercator")
    "\n\tCyl, Sph\n\tlat_ts=(0)\nlat_0=(0)";
PROJ_HEAD(utm, "Universal Transverse Mercator (UTM)")
	"\n\tCyl, Sph\n\tzone= south";

#define PROJ_ETMERC_ORDER 6


#ifdef _GNU_SOURCE
    inline
#endif
static double log1py(double x) {              /* Compute log(1+x) accurately */
    volatile double
      y = 1 + x,
      z = y - 1;
    /* Here's the explanation for this magic: y = 1 + z, exactly, and z
     * approx x, thus log(y)/z (which is nearly constant near z = 0) returns
     * a good approximation to the true log(1 + x)/x.  The multiplication x *
     * (log(y)/z) introduces little additional error. */
    return z == 0 ? x : x * log(y) / z;
}


#ifdef _GNU_SOURCE
    inline
#endif
static double asinhy(double x) {              /* Compute asinh(x) accurately */
    double y = fabs(x);         /* Enforce odd parity */
    y = log1py(y * (1 + y/(hypot(1.0, y) + 1)));
    return x < 0 ? -y : y;
}


#ifdef _GNU_SOURCE
    inline
#endif
static double gatg(double *p1, int len_p1, double B) {
    double *p;
    double h = 0, h1, h2 = 0, cos_2B;

    cos_2B = 2*cos(2*B);
    for (p = p1 + len_p1, h1 = *--p; p - p1; h2 = h1, h1 = h)
        h = -h2 + cos_2B*h1 + *--p;
    return (B + h*sin(2*B));
}

/* Complex Clenshaw summation */
#ifdef _GNU_SOURCE
    inline
#endif
static double clenS(double *a, int size, double arg_r, double arg_i, double *R, double *I) {
    double      *p, r, i, hr, hr1, hr2, hi, hi1, hi2;
    double      sin_arg_r, cos_arg_r, sinh_arg_i, cosh_arg_i;

    /* arguments */
    p = a + size;
#ifdef _GNU_SOURCE
    sincos(arg_r, &sin_arg_r, &cos_arg_r);
#else
    sin_arg_r  = sin(arg_r);
    cos_arg_r  = cos(arg_r);
#endif
    sinh_arg_i = sinh(arg_i);
    cosh_arg_i = cosh(arg_i);
    r          =  2*cos_arg_r*cosh_arg_i;
    i          = -2*sin_arg_r*sinh_arg_i;

    /* summation loop */
    for (hi1 = hr1 = hi = 0, hr = *--p; a - p;) {
        hr2 = hr1;
        hi2 = hi1;
        hr1 = hr;
        hi1 = hi;
        hr  = -hr2 + r*hr1 - i*hi1 + *--p;
        hi  = -hi2 + i*hr1 + r*hi1;
    }

    r   = sin_arg_r*cosh_arg_i;
    i   = cos_arg_r*sinh_arg_i;
    *R  = r*hr - i*hi;
    *I  = r*hi + i*hr;
    return *R;
}


/* Real Clenshaw summation */
static double clens(double *a, int size, double arg_r) {
    double      *p, r, hr, hr1, hr2, cos_arg_r;

    p = a + size;
    cos_arg_r  = cos(arg_r);
    r          =  2*cos_arg_r;

    /* summation loop */
    for (hr1 = 0, hr = *--p; a - p;) {
        hr2 = hr1;
        hr1 = hr;
        hr  = -hr2 + r*hr1 + *--p;
    }
    return sin (arg_r)*hr;
}



static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double sin_Cn, cos_Cn, cos_Ce, sin_Ce, dCn, dCe;
    double Cn = lp.phi, Ce = lp.lam;

    /* ell. LAT, LNG -> Gaussian LAT, LNG */
    Cn  = gatg (Q->cbg, PROJ_ETMERC_ORDER, Cn);
    /* Gaussian LAT, LNG -> compl. sph. LAT */
#ifdef _GNU_SOURCE
    sincos (Cn, &sin_Cn, &cos_Cn);
    sincos (Ce, &sin_Ce, &cos_Ce);
#else
    sin_Cn = sin (Cn);
    cos_Cn = cos (Cn);
    sin_Ce = sin (Ce);
    cos_Ce = cos (Ce);
#endif

    Cn     = atan2 (sin_Cn, cos_Ce*cos_Cn);
    Ce     = atan2 (sin_Ce*cos_Cn,  hypot (sin_Cn, cos_Cn*cos_Ce));

    /* compl. sph. N, E -> ell. norm. N, E */
    Ce  = asinhy ( tan (Ce) );     /* Replaces: Ce  = log(tan(FORTPI + Ce*0.5)); */
    Cn += clenS (Q->gtu, PROJ_ETMERC_ORDER, 2*Cn, 2*Ce, &dCn, &dCe);
    Ce += dCe;
    if (fabs (Ce) <= 2.623395162778) {
        xy.y  = Q->Qn * Cn + Q->Zb;  /* Northing */
        xy.x  = Q->Qn * Ce;          /* Easting  */
    } else
        xy.x = xy.y = HUGE_VAL;
    return xy;
}



static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double sin_Cn, cos_Cn, cos_Ce, sin_Ce, dCn, dCe;
    double Cn = xy.y, Ce = xy.x;

    /* normalize N, E */
    Cn = (Cn - Q->Zb)/Q->Qn;
    Ce = Ce/Q->Qn;

    if (fabs(Ce) <= 2.623395162778) { /* 150 degrees */
        /* norm. N, E -> compl. sph. LAT, LNG */
        Cn += clenS(Q->utg, PROJ_ETMERC_ORDER, 2*Cn, 2*Ce, &dCn, &dCe);
        Ce += dCe;
        Ce = atan (sinh (Ce)); /* Replaces: Ce = 2*(atan(exp(Ce)) - FORTPI); */
        /* compl. sph. LAT -> Gaussian LAT, LNG */
#ifdef _GNU_SOURCE
        sincos (Cn, &sin_Cn, &cos_Cn);
        sincos (Ce, &sin_Ce, &cos_Ce);
#else
        sin_Cn = sin (Cn);
        cos_Cn = cos (Cn);
        sin_Ce = sin (Ce);
        cos_Ce = cos (Ce);
#endif
        Ce     = atan2 (sin_Ce, cos_Ce*cos_Cn);
        Cn     = atan2 (sin_Cn*cos_Ce,  hypot (sin_Ce, cos_Ce*cos_Cn));
        /* Gaussian LAT, LNG -> ell. LAT, LNG */
        lp.phi = gatg (Q->cgb,  PROJ_ETMERC_ORDER, Cn);
        lp.lam = Ce;
    }
    else
        lp.phi = lp.lam = HUGE_VAL;
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

static PJ *setup(PJ *P) { /* general initialization */
    double f, n, np, Z;
    struct pj_opaque *Q = P->opaque;

    if (P->es <= 0)
        E_ERROR(-34);

    /* flattening */
    f = P->es / (1 + sqrt (1 -  P->es)); /* Replaces: f = 1 - sqrt(1-P->es); */

    /* third flattening */
    np = n = f/(2 - f);

    /* COEF. OF TRIG SERIES GEO <-> GAUSS */
    /* cgb := Gaussian -> Geodetic, KW p190 - 191 (61) - (62) */
    /* cbg := Geodetic -> Gaussian, KW p186 - 187 (51) - (52) */
    /* PROJ_ETMERC_ORDER = 6th degree : Engsager and Poder: ICC2007 */

    Q->cgb[0] = n*( 2 + n*(-2/3.0  + n*(-2      + n*(116/45.0 + n*(26/45.0 +
                n*(-2854/675.0 ))))));
    Q->cbg[0] = n*(-2 + n*( 2/3.0  + n*( 4/3.0  + n*(-82/45.0 + n*(32/45.0 +
                n*( 4642/4725.0))))));
    np     *= n;
    Q->cgb[1] = np*(7/3.0 + n*( -8/5.0  + n*(-227/45.0 + n*(2704/315.0 +
                n*( 2323/945.0)))));
    Q->cbg[1] = np*(5/3.0 + n*(-16/15.0 + n*( -13/9.0  + n*( 904/315.0 +
                n*(-1522/945.0)))));
    np     *= n;
    /* n^5 coeff corrected from 1262/105 -> -1262/105 */
    Q->cgb[2] = np*( 56/15.0  + n*(-136/35.0 + n*(-1262/105.0 +
                n*( 73814/2835.0))));
    Q->cbg[2] = np*(-26/15.0  + n*(  34/21.0 + n*(    8/5.0   +
                n*(-12686/2835.0))));
    np     *= n;
    /* n^5 coeff corrected from 322/35 -> 332/35 */
    Q->cgb[3] = np*(4279/630.0 + n*(-332/35.0 + n*(-399572/14175.0)));
    Q->cbg[3] = np*(1237/630.0 + n*( -12/5.0  + n*( -24832/14175.0)));
    np     *= n;
    Q->cgb[4] = np*(4174/315.0 + n*(-144838/6237.0 ));
    Q->cbg[4] = np*(-734/315.0 + n*( 109598/31185.0));
    np     *= n;
    Q->cgb[5] = np*(601676/22275.0 );
    Q->cbg[5] = np*(444337/155925.0);

    /* Constants of the projections */
    /* Transverse Mercator (UTM, ITM, etc) */
    np = n*n;
    /* Norm. mer. quad, K&W p.50 (96), p.19 (38b), p.5 (2) */
    Q->Qn = P->k0/(1 + n) * (1 + np*(1/4.0 + np*(1/64.0 + np/256.0)));
    /* coef of trig series */
    /* utg := ell. N, E -> sph. N, E,  KW p194 (65) */
    /* gtu := sph. N, E -> ell. N, E,  KW p196 (69) */
    Q->utg[0] = n*(-0.5  + n*( 2/3.0 + n*(-37/96.0 + n*( 1/360.0 +
                n*(  81/512.0 + n*(-96199/604800.0))))));
    Q->gtu[0] = n*( 0.5  + n*(-2/3.0 + n*(  5/16.0 + n*(41/180.0 +
                n*(-127/288.0 + n*(  7891/37800.0 ))))));
    Q->utg[1] = np*(-1/48.0 + n*(-1/15.0 + n*(437/1440.0 + n*(-46/105.0 +
                n*( 1118711/3870720.0)))));
    Q->gtu[1] = np*(13/48.0 + n*(-3/5.0  + n*(557/1440.0 + n*(281/630.0 +
                n*(-1983433/1935360.0)))));
    np      *= n;
    Q->utg[2] = np*(-17/480.0 + n*(  37/840.0 + n*(  209/4480.0  +
                n*( -5569/90720.0 ))));
    Q->gtu[2] = np*( 61/240.0 + n*(-103/140.0 + n*(15061/26880.0 +
                n*(167603/181440.0))));
    np      *= n;
    Q->utg[3] = np*(-4397/161280.0 + n*(  11/504.0 + n*( 830251/7257600.0)));
    Q->gtu[3] = np*(49561/161280.0 + n*(-179/168.0 + n*(6601661/7257600.0)));
    np     *= n;
    Q->utg[4] = np*(-4583/161280.0 + n*(  108847/3991680.0));
    Q->gtu[4] = np*(34729/80640.0  + n*(-3418889/1995840.0));
    np     *= n;
    Q->utg[5] = np*(-20648693/638668800.0);
    Q->gtu[5] = np*(212378941/319334400.0);

    /* Gaussian latitude value of the origin latitude */
    Z = gatg (Q->cbg, PROJ_ETMERC_ORDER, P->phi0);

    /* Origin northing minus true northing at the origin latitude */
    /* i.e. true northing = N - P->Zb                         */
    Q->Zb  = - Q->Qn*(Z + clens(Q->gtu, PROJ_ETMERC_ORDER, 2*Z));
    P->inv = e_inverse;
    P->fwd = e_forward;
	return P;
}



PJ *PROJECTION(etmerc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;
   return setup (P);
}







#ifndef PJ_SELFTEST
int pj_etmerc_selftest (void) {return 0;}
#else

int pj_etmerc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=etmerc   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5 +zone=30"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222650.79679758562,  110642.22941193319},
        {222650.79679758562,  -110642.22941193319},
        {-222650.79679758562,  110642.22941193319},
        {-222650.79679758562,  -110642.22941193319},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966305681649398,  0.00090436947663183873},
        {0.0017966305681649398,  -0.00090436947663183873},
        {-0.0017966305681649398,  0.00090436947663183873},
        {-0.0017966305681649398,  -0.00090436947663183873},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}
#endif












/* utm uses etmerc for the underlying projection */


PJ *PROJECTION(utm) {
	int zone;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	if (!P->es)
        E_ERROR(-34);
	P->y0 = pj_param (P->ctx, P->params, "bsouth").i ? 10000000. : 0.;
	P->x0 = 500000.;
	if (pj_param (P->ctx, P->params, "tzone").i) /* zone input ? */
		if ((zone = pj_param(P->ctx, P->params, "izone").i) > 0 && zone <= 60)
			--zone;
		else
			E_ERROR(-35)
	else /* nearest central meridian input */
		if ((zone = (int)(floor ((adjlon (P->lam0) + M_PI) * 30. / M_PI))) < 0)
			zone = 0;
		else if (zone >= 60)
			zone = 59;
	P->lam0 = (zone + .5) * M_PI / 30. - M_PI;
	P->k0 = 0.9996;
	P->phi0 = 0.;

    return setup (P);
}


#ifndef PJ_SELFTEST
int pj_utm_selftest (void) {return 0;}
#else

int pj_utm_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=utm   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5 +zone=30"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {1057002.4054912981,  110955.14117594929},
        {1057002.4054912981,  -110955.14117594929},
        {611263.81227890507,  110547.10569680421},
        {611263.81227890507,  -110547.10569680421},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {-7.4869520833902357,  0.00090193980983462605},
        {-7.4869520833902357,  -0.00090193980983462605},
        {-7.4905356820622613,  0.00090193535121489081},
        {-7.4905356820622613,  -0.00090193535121489081},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}
#endif



