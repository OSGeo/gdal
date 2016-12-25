#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(gn_sinu, "General Sinusoidal Series") "\n\tPCyl, Sph.\n\tm= n=";
PROJ_HEAD(sinu, "Sinusoidal (Sanson-Flamsteed)") "\n\tPCyl, Sph&Ell";
PROJ_HEAD(eck6, "Eckert VI") "\n\tPCyl, Sph.";
PROJ_HEAD(mbtfps, "McBryde-Thomas Flat-Polar Sinusoidal") "\n\tPCyl, Sph.";

#define EPS10    1e-10
#define MAX_ITER 8
#define LOOP_TOL 1e-7

struct pj_opaque {
    double *en;
    double m, n, C_x, C_y;
};


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    double s, c;

    xy.y = pj_mlfn(lp.phi, s = sin(lp.phi), c = cos(lp.phi), P->opaque->en);
    xy.x = lp.lam * c / sqrt(1. - P->es * s * s);
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    double s;

    if ((s = fabs(lp.phi = pj_inv_mlfn(P->ctx, xy.y, P->es, P->opaque->en))) < M_HALFPI) {
        s = sin(lp.phi);
        lp.lam = xy.x * sqrt(1. - P->es * s * s) / cos(lp.phi);
    } else if ((s - EPS10) < M_HALFPI) {
        lp.lam = 0.;
    } else {
        I_ERROR;
    }

    return lp;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    if (!Q->m)
        lp.phi = Q->n != 1. ? aasin(P->ctx,Q->n * sin(lp.phi)): lp.phi;
    else {
        double k, V;
        int i;

        k = Q->n * sin(lp.phi);
        for (i = MAX_ITER; i ; --i) {
            lp.phi -= V = (Q->m * lp.phi + sin(lp.phi) - k) /
                (Q->m + cos(lp.phi));
            if (fabs(V) < LOOP_TOL)
                break;
        }
        if (!i)
            F_ERROR
    }
    xy.x = Q->C_x * lp.lam * (Q->m + cos(lp.phi));
    xy.y = Q->C_y * lp.phi;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;

    xy.y /= Q->C_y;
    lp.phi = Q->m ? aasin(P->ctx,(Q->m * xy.y + sin(xy.y)) / Q->n) :
        ( Q->n != 1. ? aasin(P->ctx,sin(xy.y) / Q->n) : xy.y );
    lp.lam = xy.x / (Q->C_x * (Q->m + cos(xy.y)));
    return lp;
}


static void *freeup_new (PJ *P) {              /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    if (P->opaque->en)
        pj_dalloc(P->opaque->en);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


/* for spheres, only */
static void setup(PJ *P) {
    struct pj_opaque *Q = P->opaque;
    P->es = 0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    Q->C_x = (Q->C_y = sqrt((Q->m + 1.) / Q->n))/(Q->m + 1.);
}


PJ *PROJECTION(sinu) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (!(Q->en = pj_enfn(P->es)))
        E_ERROR_0;

    if (P->es) {
        P->inv = e_inverse;
        P->fwd = e_forward;
    } else {
        Q->n = 1.;
        Q->m = 0.;
        setup(P);
    }
    return P;
}


PJ *PROJECTION(eck6) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->m = 1.;
    Q->n = 2.570796326794896619231321691;
    setup(P);

    return P;
}


PJ *PROJECTION(mbtfps) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->m = 0.5;
    Q->n = 1.785398163397448309615660845;
    setup(P);

    return P;
}


PJ *PROJECTION(gn_sinu) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (pj_param(P->ctx, P->params, "tn").i && pj_param(P->ctx, P->params, "tm").i) {
        Q->n = pj_param(P->ctx, P->params, "dn").f;
        Q->m = pj_param(P->ctx, P->params, "dm").f;
    } else
        E_ERROR(-99)

    setup(P);

    return P;
}


#ifndef PJ_SELFTEST
int pj_sinu_selftest (void) {return 0;}
#else

int pj_sinu_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=sinu   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=sinu   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222605.29953946592,  110574.38855415257},
        { 222605.29953946592, -110574.38855415257},
        {-222605.29953946592,  110574.38855415257},
        {-222605.29953946592, -110574.38855415257},
    };

    XY s_fwd_expect[] = {
        { 223368.11902663155,  111701.07212763709},
        { 223368.11902663155, -111701.07212763709},
        {-223368.11902663155,  111701.07212763709},
        {-223368.11902663155, -111701.07212763709},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305684613522,  0.00090436947707945409},
        { 0.0017966305684613522, -0.00090436947707945409},
        {-0.0017966305684613522,  0.00090436947707945409},
        {-0.0017966305684613522, -0.00090436947707945409},
    };

    LP s_inv_expect[] = {
        { 0.0017904931100023887,  0.00089524655489191132},
        { 0.0017904931100023887, -0.00089524655489191132},
        {-0.0017904931100023887,  0.00089524655489191132},
        {-0.0017904931100023887, -0.00089524655489191132},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif

#ifndef PJ_SELFTEST
int pj_eck6_selftest (void) {return 0;}
#else

int pj_eck6_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck6   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 197021.60562899226,  126640.42073317352},
        { 197021.60562899226, -126640.42073317352},
        {-197021.60562899226,  126640.42073317352},
        {-197021.60562899226, -126640.42073317352},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.002029978749734037,  0.00078963032910382171},
        { 0.002029978749734037, -0.00078963032910382171},
        {-0.002029978749734037,  0.00078963032910382171},
        {-0.002029978749734037, -0.00078963032910382171},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif

#ifndef PJ_SELFTEST
int pj_mbtfps_selftest (void) {return 0;}
#else

int pj_mbtfps_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=mbtfps   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 204740.11747857218,  121864.72971934026},
        { 204740.11747857218, -121864.72971934026},
        {-204740.11747857218,  121864.72971934026},
        {-204740.11747857218, -121864.72971934026},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0019534152166442065,  0.00082057965689633387},
        { 0.0019534152166442065, -0.00082057965689633387},
        {-0.0019534152166442065,  0.00082057965689633387},
        {-0.0019534152166442065, -0.00082057965689633387},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif


#ifndef PJ_SELFTEST
int pj_gn_sinu_selftest (void) {return 0;}
#else

int pj_gn_sinu_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=gn_sinu   +a=6400000    +lat_1=0.5 +lat_2=2 +m=1 +n=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223385.13250469571,  111698.23644718733},
        { 223385.13250469571, -111698.23644718733},
        {-223385.13250469571,  111698.23644718733},
        {-223385.13250469571, -111698.23644718733},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0017904931098931057,  0.00089524655491012516},
        { 0.0017904931098931057, -0.00089524655491012516},
        {-0.0017904931098931057,  0.00089524655491012516},
        {-0.0017904931098931057, -0.00089524655491012516},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
