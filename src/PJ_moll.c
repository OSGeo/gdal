#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(moll, "Mollweide") "\n\tPCyl., Sph.";
PROJ_HEAD(wag4, "Wagner IV") "\n\tPCyl., Sph.";
PROJ_HEAD(wag5, "Wagner V") "\n\tPCyl., Sph.";

#define MAX_ITER    10
#define LOOP_TOL    1e-7

struct pj_opaque {
    double  C_x, C_y, C_p;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double k, V;
    int i;

    k = Q->C_p * sin(lp.phi);
    for (i = MAX_ITER; i ; --i) {
        lp.phi -= V = (lp.phi + sin(lp.phi) - k) /
            (1. + cos(lp.phi));
        if (fabs(V) < LOOP_TOL)
            break;
    }
    if (!i)
        lp.phi = (lp.phi < 0.) ? -M_HALFPI : M_HALFPI;
    else
        lp.phi *= 0.5;
    xy.x = Q->C_x * lp.lam * cos(lp.phi);
    xy.y = Q->C_y * sin(lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    lp.phi = aasin(P->ctx, xy.y / Q->C_y);
    lp.lam = xy.x / (Q->C_x * cos(lp.phi));
        if (fabs(lp.lam) < M_PI) {
            lp.phi += lp.phi;
            lp.phi = aasin(P->ctx, (lp.phi + sin(lp.phi)) / Q->C_p);
        } else {
            lp.lam = lp.phi = HUGE_VAL;
        }
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
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


static PJ * setup(PJ *P, double p) {
    struct pj_opaque *Q = P->opaque;
    double r, sp, p2 = p + p;

    P->es = 0;
    sp = sin(p);
    r = sqrt(M_TWOPI * sp / (p2 + sin(p2)));

    Q->C_x = 2. * r / M_PI;
    Q->C_y = r / sp;
    Q->C_p = p2 + sin(p2);

    P->inv = s_inverse;
    P->fwd = s_forward;
    return P;
}


PJ *PROJECTION(moll) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    return setup(P, M_HALFPI);
}


PJ *PROJECTION(wag4) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    return setup(P, M_PI/3.);
}

PJ *PROJECTION(wag5) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    P->es = 0;
    Q->C_x = 0.90977;
    Q->C_y = 1.65014;
    Q->C_p = 3.00896;

    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_moll_selftest (void) {return 0;}
#else

int pj_moll_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=moll   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        {201113.698641813244,  124066.283433859542},
        {201113.698641813244,  -124066.283433859542},
        {-201113.698641813244,  124066.283433859542},
        {-201113.698641813244,  -124066.283433859542},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.00198873782220854774,  0.000806005080362811612},
        {0.00198873782220854774,  -0.000806005080362811612},
        {-0.00198873782220854774,  0.000806005080362811612},
        {-0.00198873782220854774,  -0.000806005080362811612},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif


#ifndef PJ_SELFTEST
int pj_wag4_selftest (void) {return 0;}
#else

int pj_wag4_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag4   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 192801.218662384286,  129416.216394802992},
        { 192801.218662384286, -129416.216394802992},
        {-192801.218662384286,  129416.216394802992},
        {-192801.218662384286, -129416.216394802992},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.00207450259783523421, 0.000772682950537716476},
        { 0.00207450259783523421, -0.000772682950537716476},
        {-0.00207450259783523421,  0.000772682950537716476},
        {-0.00207450259783523421, -0.000772682950537716476},
   };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}

#endif

#ifndef PJ_SELFTEST
int pj_wag5_selftest (void) {return 0;}
#else

int pj_wag5_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=wag5   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };


    XY s_fwd_expect[] = {
        { 203227.05192532466,  138651.631442713202},
        { 203227.05192532466, -138651.631442713202},
        {-203227.05192532466,  138651.631442713202},
        {-203227.05192532466, -138651.631442713202},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };




    LP s_inv_expect[] = {
        { 0.00196807227086416396,  0.00072121615041701424},
        { 0.00196807227086416396, -0.00072121615041701424},
        {-0.00196807227086416396,  0.00072121615041701424},
        {-0.00196807227086416396, -0.00072121615041701424},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
