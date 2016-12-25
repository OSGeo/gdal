#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(denoy, "Denoyer Semi-Elliptical") "\n\tPCyl., no inv., Sph.";

#define C0  0.95
#define C1 -0.08333333333333333333
#define C3  0.00166666666666666666
#define D1  0.9
#define D5  0.03


static XY s_forward (LP lp, PJ *P) {            /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
    (void) P;
    xy.y = lp.phi;
    xy.x = lp.lam;
    lp.lam = fabs(lp.lam);
    xy.x *= cos((C0 + lp.lam * (C1 + lp.lam * lp.lam * C3)) *
            (lp.phi * (D1 + D5 * lp.phi * lp.phi * lp.phi * lp.phi)));
    return xy;
}


static void *freeup_new (PJ *P) {               /* Destructor */
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(denoy) {
    P->es = 0.0;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_denoy_selftest (void) {return 0;}
#else

int pj_denoy_selftest (void) {
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=denoy   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223377.422876954137,  111701.07212763709},
        { 223377.422876954137, -111701.07212763709},
        {-223377.422876954137,  111701.07212763709},
        {-223377.422876954137, -111701.07212763709},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, 0, 4, 4, fwd_in, 0, s_fwd_expect, 0, 0, 0);
}


#endif
