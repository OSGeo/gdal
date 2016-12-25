#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(mbt_fps, "McBryde-Thomas Flat-Pole Sine (No. 2)") "\n\tCyl., Sph.";

#define MAX_ITER    10
#define LOOP_TOL    1e-7
#define C1 0.45503
#define C2 1.36509
#define C3 1.41546
#define C_x 0.22248
#define C_y 1.44492
#define C1_2 0.33333333333333333333333333

static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double k, V, t;
    int i;
    (void) P;

    k = C3 * sin(lp.phi);
    for (i = MAX_ITER; i ; --i) {
        t = lp.phi / C2;
        lp.phi -= V = (C1 * sin(t) + sin(lp.phi) - k) /
            (C1_2 * cos(t) + cos(lp.phi));
        if (fabs(V) < LOOP_TOL)
            break;
    }
    t = lp.phi / C2;
    xy.x = C_x * lp.lam * (1. + 3. * cos(lp.phi)/cos(t) );
    xy.y = C_y * sin(t);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double t;

    lp.phi = C2 * (t = aasin(P->ctx,xy.y / C_y));
    lp.lam = xy.x / (C_x * (1. + 3. * cos(lp.phi)/cos(t)));
    lp.phi = aasin(P->ctx,(C1 * sin(t) + sin(lp.phi)) / C3);
    return (lp);
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


PJ *PROJECTION(mbt_fps) {

    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_mbt_fps_selftest (void) {return 0;}
#else

int pj_mbt_fps_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=mbt_fps   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 198798.176129849948,  125512.017254530627},
        { 198798.176129849948, -125512.017254530627},
        {-198798.176129849948,  125512.017254530627},
        {-198798.176129849948, -125512.017254530627},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00201197086238270742,  0.000796711850174446003},
        { 0.00201197086238270742, -0.000796711850174446003},
        {-0.00201197086238270742,  0.000796711850174446003},
        {-0.00201197086238270742, -0.000796711850174446003},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
