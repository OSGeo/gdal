/*
The Natural Earth II projection was designed by Tom Patterson, US National
Park Service, in 2012, using Flex Projector. The polynomial equation was
developed by Bojan Savric and Bernhard Jenny, College of Earth, Ocean,
and Atmospheric Sciences, Oregon State University.
Port to PROJ.4 by Bojan Savric, 4 April 2016
*/
#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(natearth2, "Natural Earth 2") "\n\tPCyl., Sph.";

#define A0 0.84719
#define A1 -0.13063
#define A2 -0.04515
#define A3 0.05494
#define A4 -0.02326
#define A5 0.00331
#define B0 1.01183
#define B1 -0.02625
#define B2 0.01926
#define B3 -0.00396
#define C0 B0
#define C1 (9 * B1)
#define C2 (11 * B2)
#define C3 (13 * B3)
#define EPS 1e-11
#define MAX_Y (0.84719 * 0.535117535153096 * M_PI)


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double phi2, phi4, phi6;
    (void) P;

    phi2 = lp.phi * lp.phi;
    phi4 = phi2 * phi2;
    phi6 = phi2 * phi4;

    xy.x = lp.lam * (A0 + A1 * phi2 + phi6 * phi6 * (A2 + A3 * phi2 + A4 * phi4 + A5 * phi6));
    xy.y = lp.phi * (B0 + phi4 * phi4 * (B1 + B2 * phi2 + B3 * phi4));
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double yc, tol, y2, y4, y6, f, fder;
    (void) P;

    /* make sure y is inside valid range */
    if (xy.y > MAX_Y) {
        xy.y = MAX_Y;
    } else if (xy.y < -MAX_Y) {
        xy.y = -MAX_Y;
    }

    /* latitude */
    yc = xy.y;
    for (;;) { /* Newton-Raphson */
        y2 = yc * yc;
        y4 = y2 * y2;
        f = (yc * (B0 + y4 * y4 * (B1 + B2 * y2 + B3 * y4))) - xy.y;
        fder = C0 + y4 * y4 * (C1 + C2 * y2 + C3 * y4);
        yc -= tol = f / fder;
        if (fabs(tol) < EPS) {
            break;
        }
    }
    lp.phi = yc;

    /* longitude */
    y2 = yc * yc;
    y4 = y2 * y2;
    y6 = y2 * y4;

    lp.lam = xy.x / (A0 + A1 * y2 + y6 * y6 * (A2 + A3 * y2 + A4 * y4 + A5 * y6));

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;

    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(natearth2) {
    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_natearth2_selftest (void) {return 0;}
#else

int pj_natearth2_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=natearth2   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 189255.172934730799,  113022.495810907014},
        { 189255.172934730799, -113022.495810907014},
        {-189255.172934730799,  113022.495810907014},
        {-189255.172934730799, -113022.495810907014},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00211344929691056112,  0.000884779612080993237},
        { 0.00211344929691056112, -0.000884779612080993237},
        {-0.00211344929691056112,  0.000884779612080993237},
        {-0.00211344929691056112, -0.000884779612080993237},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
