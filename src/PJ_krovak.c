 /*
 * Project:  PROJ.4
 * Purpose:  Implementation of the krovak (Krovak) projection.
 *           Definition: http://www.ihsenergy.com/epsg/guid7.html#1.4.3
 * Author:   Thomas Flemming, tf@ttqv.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Thomas Flemming, tf@ttqv.com
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************
 * A description of the (forward) projection is found in:
 *
 *      Bohuslav Veverka,
 *
 *      KROVAK’S PROJECTION AND ITS USE FOR THE
 *      CZECH REPUBLIC AND THE SLOVAK REPUBLIC,
 *
 *      50 years of the Research Institute of
 *      and the Slovak Republic Geodesy, Topography and Cartography
 *
 * which can be found via the Wayback Machine:
 *
 *      https://web.archive.org/web/20150216143806/https://www.vugtk.cz/odis/sborniky/sb2005/Sbornik_50_let_VUGTK/Part_1-Scientific_Contribution/16-Veverka.pdf
 *
 * Further info, including the inverse projection, is given by EPSG:
 *
 *      Guidance Note 7 part 2
 *      Coordinate Conversions and Transformations including Formulas
 *
 *      http://www.iogp.org/pubs/373-07-2.pdf
 *
 * Variable names in this file mostly follows what is used in the
 * paper by Veverka.
 *
 * According to EPSG the full Krovak projection method should have
 * the following parameters.  Within PROJ.4 the azimuth, and pseudo
 * standard parallel are hardcoded in the algorithm and can't be
 * altered from outside. The others all have defaults to match the
 * common usage with Krovak projection.
 *
 *      lat_0 = latitude of centre of the projection
 *
 *      lon_0 = longitude of centre of the projection
 *
 *      ** = azimuth (true) of the centre line passing through the
 *           centre of the projection
 *
 *      ** = latitude of pseudo standard parallel
 *
 *      k  = scale factor on the pseudo standard parallel
 *
 *      x_0 = False Easting of the centre of the projection at the
 *            apex of the cone
 *
 *      y_0 = False Northing of the centre of the projection at
 *            the apex of the cone
 *
 *****************************************************************************/


#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(krovak, "Krovak") "\n\tPCyl., Ellps.";

#define EPS 1e-15
#define S45 0.785398163397448  /* 45 deg */
#define S90 1.570796326794896  /* 90 deg */
#define UQ  1.04216856380474   /* DU(2, 59, 42, 42.69689) */
#define S0  1.37008346281555   /* Latitude of pseudo standard parallel 78deg 30'00" N */

struct pj_opaque {
    double alpha;
    double k;
    double n;
    double rho0;
    double ad;
    int czech;
};


static XY e_forward (LP lp, PJ *P) {                /* Ellipsoidal, forward */
    struct pj_opaque *Q = P->opaque;
    XY xy = {0.0,0.0};

    double gfi, u, deltav, s, d, eps, rho;

    gfi = pow ( (1. + P->e * sin(lp.phi)) / (1. - P->e * sin(lp.phi)), Q->alpha * P->e / 2.);

    u = 2. * (atan(Q->k * pow( tan(lp.phi / 2. + S45), Q->alpha) / gfi)-S45);
    deltav = -lp.lam * Q->alpha;

    s = asin(cos(Q->ad) * sin(u) + sin(Q->ad) * cos(u) * cos(deltav));
    d = asin(cos(u) * sin(deltav) / cos(s));

    eps = Q->n * d;
    rho = Q->rho0 * pow(tan(S0 / 2. + S45) , Q->n) / pow(tan(s / 2. + S45) , Q->n);

    xy.y = rho * cos(eps);
    xy.x = rho * sin(eps);

    xy.y *= Q->czech;
    xy.x *= Q->czech;

    return xy;
}


static LP e_inverse (XY xy, PJ *P) {                /* Ellipsoidal, inverse */
    struct pj_opaque *Q = P->opaque;
    LP lp = {0.0,0.0};

    double u, deltav, s, d, eps, rho, fi1, xy0;
    int ok;

    xy0 = xy.x;
    xy.x = xy.y;
    xy.y = xy0;

    xy.x *= Q->czech;
    xy.y *= Q->czech;

    rho = sqrt(xy.x * xy.x + xy.y * xy.y);
    eps = atan2(xy.y, xy.x);

    d = eps / sin(S0);
    s = 2. * (atan(  pow(Q->rho0 / rho, 1. / Q->n) * tan(S0 / 2. + S45)) - S45);

    u = asin(cos(Q->ad) * sin(s) - sin(Q->ad) * cos(s) * cos(d));
    deltav = asin(cos(s) * sin(d) / cos(u));

    lp.lam = P->lam0 - deltav / Q->alpha;

    /* ITERATION FOR lp.phi */
    fi1 = u;

    ok = 0;
    do {
        lp.phi = 2. * ( atan( pow( Q->k, -1. / Q->alpha)  *
                              pow( tan(u / 2. + S45) , 1. / Q->alpha)  *
                              pow( (1. + P->e * sin(fi1)) / (1. - P->e * sin(fi1)) , P->e / 2.)
                            )  - S45);

        if (fabs(fi1 - lp.phi) < EPS) ok=1;
        fi1 = lp.phi;
   } while (ok==0);

   lp.lam -= P->lam0;

   return lp;
}


static void *freeup_new (PJ *P) {                   /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc(P);

    pj_dealloc(P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(krovak) {
    double u0, n0, g;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    /* we want Bessel as fixed ellipsoid */
    P->a = 6377397.155;
    P->e = sqrt(P->es = 0.006674372230614);

    /* if latitude of projection center is not set, use 49d30'N */
    if (!pj_param(P->ctx, P->params, "tlat_0").i)
            P->phi0 = 0.863937979737193;

    /* if center long is not set use 42d30'E of Ferro - 17d40' for Ferro */
    /* that will correspond to using longitudes relative to greenwich    */
    /* as input and output, instead of lat/long relative to Ferro */
    if (!pj_param(P->ctx, P->params, "tlon_0").i)
            P->lam0 = 0.7417649320975901 - 0.308341501185665;

    /* if scale not set default to 0.9999 */
    if (!pj_param(P->ctx, P->params, "tk").i)
            P->k0 = 0.9999;

    Q->czech = 1;
    if( !pj_param(P->ctx, P->params, "tczech").i )
        Q->czech = -1;

    /* Set up shared parameters between forward and inverse */
    Q->alpha = sqrt(1. + (P->es * pow(cos(P->phi0), 4)) / (1. - P->es));
    u0 = asin(sin(P->phi0) / Q->alpha);
    g = pow( (1. + P->e * sin(P->phi0)) / (1. - P->e * sin(P->phi0)) , Q->alpha * P->e / 2. );
    Q->k = tan( u0 / 2. + S45) / pow  (tan(P->phi0 / 2. + S45) , Q->alpha) * g;
    n0 = sqrt(1. - P->es) / (1. - P->es * pow(sin(P->phi0), 2));
    Q->n = sin(S0);
    Q->rho0 = P->k0 * n0 / tan(S0);
    Q->ad = S90 - UQ;

    P->inv = e_inverse;
    P->fwd = e_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_krovak_selftest (void) {return 0;}
#else

int pj_krovak_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=krovak +ellps=GRS80"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {-3196535.2325636409,  -6617878.8675514441},
        {-3260035.4405521089,  -6898873.6148780314},
        {-3756305.3288691747,  -6478142.5615715114},
        {-3831703.6585019818,  -6759107.1701553948},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {24.836218918719162,  59.758403933233858},
        {24.836315484509566,  59.756888425730189},
        {24.830447747947495,  59.758403933233858},
        {24.830351182157091,  59.756888425730189},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
