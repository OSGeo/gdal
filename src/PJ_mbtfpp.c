#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(mbtfpp, "McBride-Thomas Flat-Polar Parabolic") "\n\tCyl., Sph.";

#define CS  .95257934441568037152
#define FXC .92582009977255146156
#define FYC 3.40168025708304504493
#define C23 .66666666666666666666
#define C13 .33333333333333333333
#define ONEEPS  1.0000001


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    lp.phi = asin(CS * sin(lp.phi));
    xy.x = FXC * lp.lam * (2. * cos(C23 * lp.phi) - 1.);
    xy.y = FYC * sin(C13 * lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};

    lp.phi = xy.y / FYC;
    if (fabs(lp.phi) >= 1.) {
        if (fabs(lp.phi) > ONEEPS)
            I_ERROR
        else
            lp.phi = (lp.phi < 0.) ? -M_HALFPI : M_HALFPI;
    } else
        lp.phi = asin(lp.phi);

    lp.lam = xy.x / ( FXC * (2. * cos(C23 * (lp.phi *= 3.)) - 1.) );
    if (fabs(lp.phi = sin(lp.phi) / CS) >= 1.) {
        if (fabs(lp.phi) > ONEEPS)
            I_ERROR
        else
            lp.phi = (lp.phi < 0.) ? -M_HALFPI : M_HALFPI;
    } else
        lp.phi = asin(lp.phi);

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


PJ *PROJECTION(mbtfpp) {

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_mbtfpp_selftest (void) {return 0;}
#else

int pj_mbtfpp_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=mbtfpp   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {206804.786929820373,  120649.762565792524},
        {206804.786929820373,  -120649.762565792524},
        {-206804.786929820373,  120649.762565792524},
        {-206804.786929820373,  -120649.762565792524},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.00193395359462902698,  0.00082883725477665357},
        {0.00193395359462902698,  -0.00082883725477665357},
        {-0.00193395359462902698,  0.00082883725477665357},
        {-0.00193395359462902698,  -0.00082883725477665357},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
