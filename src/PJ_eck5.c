#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(eck5, "Eckert V") "\n\tPCyl, Sph.";

#define XF  0.44101277172455148219
#define RXF 2.26750802723822639137
#define YF  0.88202554344910296438
#define RYF 1.13375401361911319568


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;
    xy.x = XF * (1. + cos(lp.phi)) * lp.lam;
    xy.y = YF * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;
    lp.lam = RXF * xy.x / (1. + cos( lp.phi = RYF * xy.y));

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


PJ *PROJECTION(eck5) {
    P->es = 0.0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_eck5_selftest (void) {return 0;}
#else

int pj_eck5_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck5   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 197031.39213406085,  98523.198847226551},
        { 197031.39213406085, -98523.198847226551},
        {-197031.39213406085,  98523.198847226551},
        {-197031.39213406085, -98523.198847226551},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.002029978749734037,  0.001014989374787388},
        {0.002029978749734037,  -0.001014989374787388},
        {-0.002029978749734037,  0.001014989374787388},
        {-0.002029978749734037,  -0.001014989374787388},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
