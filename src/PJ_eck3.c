#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(eck3, "Eckert III") "\n\tPCyl, Sph.";
PROJ_HEAD(putp1, "Putnins P1") "\n\tPCyl, Sph.";
PROJ_HEAD(wag6, "Wagner VI") "\n\tPCyl, Sph.";
PROJ_HEAD(kav7, "Kavraisky VII") "\n\tPCyl, Sph.";

struct pj_opaque {
    double C_x, C_y, A, B;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    xy.y = Q->C_y * lp.phi;
    xy.x = Q->C_x * lp.lam * (Q->A + asqrt(1. - Q->B * lp.phi * lp.phi));
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    lp.phi = xy.y / Q->C_y;
    lp.lam = xy.x / (Q->C_x * (Q->A + asqrt(1. - Q->B * lp.phi * lp.phi)));
    return lp;
}


static void *freeup_new (PJ *P) {               /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


static PJ *setup(PJ *P) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;
    return P;
}


PJ *PROJECTION(eck3) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 0.42223820031577120149;
    Q->C_y = 0.84447640063154240298;
    Q->A = 1.0;
    Q->B = 0.4052847345693510857755;

    return setup(P);
}


PJ *PROJECTION(kav7) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    /* Defined twice in original code - Using 0.866...,
     * but leaving the other one here as a safety measure.
     * Q->C_x = 0.2632401569273184856851; */
    Q->C_x = 0.8660254037844;
    Q->C_y = 1.;
    Q->A = 0.;
    Q->B = 0.30396355092701331433;

    return setup(P);
}


PJ *PROJECTION(wag6) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = Q->C_y = 0.94745;
    Q->A = 0.0;
    Q->B = 0.30396355092701331433;

    return setup(P);
}


PJ *PROJECTION(putp1) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->C_x = 1.89490;
    Q->C_y = 0.94745;
    Q->A = -0.5;
    Q->B = 0.30396355092701331433;

    return setup(P);
}


#ifndef PJ_SELFTEST
int pj_eck3_selftest (void) {return 0;}
#else

int pj_eck3_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck3   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 188652.01572153764,  94328.919337031271},
        { 188652.01572153764, -94328.919337031271},
        {-188652.01572153764,  94328.919337031271},
        {-188652.01572153764, -94328.919337031271},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0021202405520236059,  0.0010601202759750307},
        { 0.0021202405520236059, -0.0010601202759750307},
        {-0.0021202405520236059,  0.0010601202759750307},
        {-0.0021202405520236059, -0.0010601202759750307},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif

#ifndef PJ_SELFTEST
int pj_kav7_selftest (void) {return 0;}
#else

int pj_kav7_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=kav7   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 193462.9749437288,  111701.07212763709},
        { 193462.9749437288, -111701.07212763709},
        {-193462.9749437288,  111701.07212763709},
        {-193462.9749437288, -111701.07212763709}
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0020674833579085268,  0.00089524655489191132},
        { 0.0020674833579085268, -0.00089524655489191132},
        {-0.0020674833579085268,  0.00089524655489191132},
        {-0.0020674833579085268, -0.00089524655489191132}
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif

#ifndef PJ_SELFTEST
int pj_wag6_selftest (void) {return 0;}
#else

int pj_wag6_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag6   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 211652.56216440981,  105831.18078732977},
        { 211652.56216440981, -105831.18078732977},
        {-211652.56216440981,  105831.18078732977},
        {-211652.56216440981, -105831.18078732977}
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0018898022163257513,  0.000944901108123818},
        { 0.0018898022163257513, -0.000944901108123818},
        {-0.0018898022163257513,  0.000944901108123818},
        {-0.0018898022163257513, -0.000944901108123818}
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif


#ifndef PJ_SELFTEST
int pj_putp1_selftest (void) {return 0;}
#else

int pj_putp1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=putp1   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 211642.76275416015,  105831.18078732977},
        { 211642.76275416015, -105831.18078732977},
        {-211642.76275416015,  105831.18078732977},
        {-211642.76275416015, -105831.18078732977}
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0018898022164038663,  0.000944901108123818},
        { 0.0018898022164038663, -0.000944901108123818},
        {-0.0018898022164038663,  0.000944901108123818},
        {-0.0018898022164038663, -0.000944901108123818}
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
