#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(mbtfpq, "McBryde-Thomas Flat-Polar Quartic") "\n\tCyl., Sph.";

#define NITER   20
#define EPS 1e-7
#define ONETOL 1.000001
#define C   1.70710678118654752440
#define RC  0.58578643762690495119
#define FYC 1.87475828462269495505
#define RYC 0.53340209679417701685
#define FXC 0.31245971410378249250
#define RXC 3.20041258076506210122


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double th1, c;
    int i;
    (void) P;

    c = C * sin(lp.phi);
    for (i = NITER; i; --i) {
        lp.phi -= th1 = (sin(.5*lp.phi) + sin(lp.phi) - c) /
            (.5*cos(.5*lp.phi)  + cos(lp.phi));
        if (fabs(th1) < EPS) break;
    }
    xy.x = FXC * lp.lam * (1.0 + 2. * cos(lp.phi)/cos(0.5 * lp.phi));
    xy.y = FYC * sin(0.5 * lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double t;

    lp.phi = RYC * xy.y;
    if (fabs(lp.phi) > 1.) {
        if (fabs(lp.phi) > ONETOL)  I_ERROR
        else if (lp.phi < 0.) { t = -1.; lp.phi = -M_PI; }
        else { t = 1.; lp.phi = M_PI; }
    } else
        lp.phi = 2. * asin(t = lp.phi);
    lp.lam = RXC * xy.x / (1. + 2. * cos(lp.phi)/cos(0.5 * lp.phi));
    lp.phi = RC * (t + sin(lp.phi));
    if (fabs(lp.phi) > 1.)
        if (fabs(lp.phi) > ONETOL)  I_ERROR
        else            lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
    else
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


PJ *PROJECTION(mbtfpq) {

    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_mbtfpq_selftest (void) {return 0;}
#else

int pj_mbtfpq_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=mbtfpq   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 209391.854738393013,  119161.040199054827},
        { 209391.854738393013, -119161.040199054827},
        {-209391.854738393013,  119161.040199054827},
        {-209391.854738393013, -119161.040199054827},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00191010555824111571,  0.000839185447792341723},
        { 0.00191010555824111571, -0.000839185447792341723},
        {-0.00191010555824111571,  0.000839185447792341723},
        {-0.00191010555824111571, -0.000839185447792341723},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
