#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(gall, "Gall (Gall Stereographic)") "\n\tCyl, Sph";

#define YF  1.70710678118654752440
#define XF  0.70710678118654752440
#define RYF 0.58578643762690495119
#define RXF 1.41421356237309504880


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = XF * lp.lam;
    xy.y = YF * tan(.5 * lp.phi);

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;

    lp.lam = RXF * xy.x;
    lp.phi = 2. * atan(xy.y * RYF);

    return lp;
}


static void *freeup_new (PJ *P) {              /* Destructor */
    if (0==P)
        return 0;

    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(gall) {
    P->es = 0.0;

    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_gall_selftest (void) {return 0;}
#else

int pj_gall_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=gall   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 157969.17113451968,  95345.249178385886},
        { 157969.17113451968, -95345.249178385886},
        {-157969.17113451968,  95345.249178385886},
        {-157969.17113451968, -95345.249178385886},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0025321396391918614,  0.001048846580346495},
        { 0.0025321396391918614, -0.001048846580346495},
        {-0.0025321396391918614,  0.001048846580346495},
        {-0.0025321396391918614, -0.001048846580346495},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
