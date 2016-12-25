#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(eck4, "Eckert IV") "\n\tPCyl, Sph.";

#define C_x .42223820031577120149
#define C_y 1.32650042817700232218
#define RC_y    .75386330736002178205
#define C_p 3.57079632679489661922
#define RC_p    .28004957675577868795
#define EPS 1e-7
#define NITER   6


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double p, V, s, c;
    int i;
    (void) P;

    p = C_p * sin(lp.phi);
    V = lp.phi * lp.phi;
    lp.phi *= 0.895168 + V * ( 0.0218849 + V * 0.00826809 );
    for (i = NITER; i ; --i) {
        c = cos(lp.phi);
        s = sin(lp.phi);
        lp.phi -= V = (lp.phi + s * (c + 2.) - p) /
            (1. + c * (c + 2.) - s * s);
        if (fabs(V) < EPS)
            break;
    }
    if (!i) {
        xy.x = C_x * lp.lam;
        xy.y = lp.phi < 0. ? -C_y : C_y;
    } else {
        xy.x = C_x * lp.lam * (1. + cos(lp.phi));
        xy.y = C_y * sin(lp.phi);
    }
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double c;

    lp.phi = aasin(P->ctx,xy.y / C_y);
    lp.lam = xy.x / (C_x * (1. + (c = cos(lp.phi))));
    lp.phi = aasin(P->ctx,(lp.phi + sin(lp.phi) * (c + 2.)) / C_p);
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


PJ *PROJECTION(eck4) {
    P->es = 0.0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_eck4_selftest (void) {return 0;}
#else

int pj_eck4_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck4   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 188646.38935641639,  132268.54017406539},
        { 188646.38935641639, -132268.54017406539},
        {-188646.38935641639,  132268.54017406539},
        {-188646.38935641639, -132268.54017406539},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0021202405520236059, 0.00075601458836610643},
        { 0.0021202405520236059, -0.00075601458836610643},
        {-0.0021202405520236059, 0.00075601458836610643},
        {-0.0021202405520236059, -0.00075601458836610643},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
