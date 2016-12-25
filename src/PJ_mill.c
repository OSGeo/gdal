#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(mill, "Miller Cylindrical") "\n\tCyl, Sph";

static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = lp.lam;
    xy.y = log(tan(M_FORTPI + lp.phi * .4)) * 1.25;

    return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;

    lp.lam = xy.x;
    lp.phi = 2.5 * (atan(exp(.8 * xy.y)) - M_FORTPI);

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


PJ *PROJECTION(mill) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_mill_selftest (void) {return 0;}
#else

int pj_mill_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=mill   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223402.144255274179,  111704.701754393827},
        { 223402.144255274179, -111704.701754396243},
        {-223402.144255274179,  111704.701754393827},
        {-223402.144255274179, -111704.701754396243},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00179049310978382265,  0.000895246554873922024},
        { 0.00179049310978382265, -0.000895246554873922024},
        {-0.00179049310978382265,  0.000895246554873922024},
        {-0.00179049310978382265, -0.000895246554873922024},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
