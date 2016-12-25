/*
The Natural Earth projection was designed by Tom Patterson, US National Park
Service, in 2007, using Flex Projector. The shape of the original projection
was defined at every 5 degrees and piece-wise cubic spline interpolation was
used to compute the complete graticule.
The code here uses polynomial functions instead of cubic splines and
is therefore much simpler to program. The polynomial approximation was
developed by Bojan Savric, in collaboration with Tom Patterson and Bernhard
Jenny, Institute of Cartography, ETH Zurich. It slightly deviates from
Patterson's original projection by adding additional curvature to meridians
where they meet the horizontal pole line. This improvement is by intention
and designed in collaboration with Tom Patterson.
Port to PROJ.4 by Bernhard Jenny, 6 June 2011
*/
#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(natearth, "Natural Earth") "\n\tPCyl., Sph.";

#define A0 0.8707
#define A1 -0.131979
#define A2 -0.013791
#define A3 0.003971
#define A4 -0.001529
#define B0 1.007226
#define B1 0.015085
#define B2 -0.044475
#define B3 0.028874
#define B4 -0.005916
#define C0 B0
#define C1 (3 * B1)
#define C2 (7 * B2)
#define C3 (9 * B3)
#define C4 (11 * B4)
#define EPS 1e-11
#define MAX_Y (0.8707 * 0.52 * M_PI)


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double phi2, phi4;
    (void) P;

    phi2 = lp.phi * lp.phi;
    phi4 = phi2 * phi2;
    xy.x = lp.lam * (A0 + phi2 * (A1 + phi2 * (A2 + phi4 * phi2 * (A3 + phi2 * A4))));
    xy.y = lp.phi * (B0 + phi2 * (B1 + phi4 * (B2 + B3 * phi2 + B4 * phi4)));
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double yc, tol, y2, y4, f, fder;
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
        f = (yc * (B0 + y2 * (B1 + y4 * (B2 + B3 * y2 + B4 * y4)))) - xy.y;
        fder = C0 + y2 * (C1 + y4 * (C2 + C3 * y2 + C4 * y4));
        yc -= tol = f / fder;
        if (fabs(tol) < EPS) {
            break;
        }
    }
    lp.phi = yc;

    /* longitude */
    y2 = yc * yc;
    lp.lam = xy.x / (A0 + y2 * (A1 + y2 * (A2 + y2 * y2 * y2 * (A3 + y2 * A4))));

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


PJ *PROJECTION(natearth) {
    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_natearth_selftest (void) {return 0;}
#else

int pj_natearth_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=natearth   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 194507.265257889288,  112508.737358294515},
        { 194507.265257889288, -112508.737358294515},
        {-194507.265257889288,  112508.737358294515},
        {-194507.265257889288, -112508.737358294515},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00205638349586440223,  0.000888823913291242177},
        { 0.00205638349586440223, -0.000888823913291242177},
        {-0.00205638349586440223,  0.000888823913291242177},
        {-0.00205638349586440223, -0.000888823913291242177},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
