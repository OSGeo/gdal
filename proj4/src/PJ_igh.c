#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(igh, "Interrupted Goode Homolosine") "\n\tPCyl, Sph.";

C_NAMESPACE PJ *pj_sinu(PJ *), *pj_moll(PJ *);

/* 40d 44' 11.8" [degrees] */
/*
static const double d4044118 = (40 + 44/60. + 11.8/3600.) * DEG_TO_RAD;
has been replaced by this define, to eliminate portability issue:
Initializer element not computable at load time
*/
#define d4044118 ((40 + 44/60. + 11.8/3600.) * DEG_TO_RAD)

static const double d10  =  10 * DEG_TO_RAD;
static const double d20  =  20 * DEG_TO_RAD;
static const double d30  =  30 * DEG_TO_RAD;
static const double d40  =  40 * DEG_TO_RAD;
static const double d50  =  50 * DEG_TO_RAD;
static const double d60  =  60 * DEG_TO_RAD;
static const double d80  =  80 * DEG_TO_RAD;
static const double d90  =  90 * DEG_TO_RAD;
static const double d100 = 100 * DEG_TO_RAD;
static const double d140 = 140 * DEG_TO_RAD;
static const double d160 = 160 * DEG_TO_RAD;
static const double d180 = 180 * DEG_TO_RAD;

static const double EPSLN = 1.e-10; /* allow a little 'slack' on zone edge positions */

struct pj_opaque {
    struct PJconsts* pj[12]; \
    double dy0;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    int z;

    if (lp.phi >=  d4044118) {          /* 1|2 */
      z = (lp.lam <= -d40 ? 1: 2);
    }
    else if (lp.phi >=  0) {            /* 3|4 */
      z = (lp.lam <= -d40 ? 3: 4);
    }
    else if (lp.phi >= -d4044118) {     /* 5|6|7|8 */
           if (lp.lam <= -d100) z =  5; /* 5 */
      else if (lp.lam <=  -d20) z =  6; /* 6 */
      else if (lp.lam <=   d80) z =  7; /* 7 */
      else z = 8;                       /* 8 */
    }
    else {                              /* 9|10|11|12 */
           if (lp.lam <= -d100) z =  9; /* 9 */
      else if (lp.lam <=  -d20) z = 10; /* 10 */
      else if (lp.lam <=   d80) z = 11; /* 11 */
      else z = 12;                      /* 12 */
    }

    lp.lam -= Q->pj[z-1]->lam0;
    xy = Q->pj[z-1]->fwd(lp, Q->pj[z-1]);
    xy.x += Q->pj[z-1]->x0;
    xy.y += Q->pj[z-1]->y0;

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    const double y90 = Q->dy0 + sqrt(2); /* lt=90 corresponds to y=y0+sqrt(2) */

    int z = 0;
    if (xy.y > y90+EPSLN || xy.y < -y90+EPSLN) /* 0 */
      z = 0;
    else if (xy.y >=  d4044118)       /* 1|2 */
      z = (xy.x <= -d40? 1: 2);
    else if (xy.y >=  0)              /* 3|4 */
      z = (xy.x <= -d40? 3: 4);
    else if (xy.y >= -d4044118) {     /* 5|6|7|8 */
           if (xy.x <= -d100) z =  5; /* 5 */
      else if (xy.x <=  -d20) z =  6; /* 6 */
      else if (xy.x <=   d80) z =  7; /* 7 */
      else z = 8;                     /* 8 */
    }
    else {                            /* 9|10|11|12 */
           if (xy.x <= -d100) z =  9; /* 9 */
      else if (xy.x <=  -d20) z = 10; /* 10 */
      else if (xy.x <=   d80) z = 11; /* 11 */
      else z = 12;                    /* 12 */
    }

    if (z) {
        int ok = 0;

        xy.x -= Q->pj[z-1]->x0;
        xy.y -= Q->pj[z-1]->y0;
        lp = Q->pj[z-1]->inv(xy, Q->pj[z-1]);
        lp.lam += Q->pj[z-1]->lam0;

        switch (z) {
        case  1: ok = (lp.lam >= -d180-EPSLN && lp.lam <=  -d40+EPSLN) ||
                     ((lp.lam >=  -d40-EPSLN && lp.lam <=  -d10+EPSLN) &&
                      (lp.phi >=   d60-EPSLN && lp.phi <=   d90+EPSLN)); break;
        case  2: ok = (lp.lam >=  -d40-EPSLN && lp.lam <=  d180+EPSLN) ||
                     ((lp.lam >= -d180-EPSLN && lp.lam <= -d160+EPSLN) &&
                      (lp.phi >=   d50-EPSLN && lp.phi <=   d90+EPSLN)) ||
                     ((lp.lam >=  -d50-EPSLN && lp.lam <=  -d40+EPSLN) &&
                      (lp.phi >=   d60-EPSLN && lp.phi <=   d90+EPSLN)); break;
        case  3: ok = (lp.lam >= -d180-EPSLN && lp.lam <=  -d40+EPSLN); break;
        case  4: ok = (lp.lam >=  -d40-EPSLN && lp.lam <=  d180+EPSLN); break;
        case  5: ok = (lp.lam >= -d180-EPSLN && lp.lam <= -d100+EPSLN); break;
        case  6: ok = (lp.lam >= -d100-EPSLN && lp.lam <=  -d20+EPSLN); break;
        case  7: ok = (lp.lam >=  -d20-EPSLN && lp.lam <=   d80+EPSLN); break;
        case  8: ok = (lp.lam >=   d80-EPSLN && lp.lam <=  d180+EPSLN); break;
        case  9: ok = (lp.lam >= -d180-EPSLN && lp.lam <= -d100+EPSLN); break;
        case 10: ok = (lp.lam >= -d100-EPSLN && lp.lam <=  -d20+EPSLN); break;
        case 11: ok = (lp.lam >=  -d20-EPSLN && lp.lam <=   d80+EPSLN); break;
        case 12: ok = (lp.lam >=   d80-EPSLN && lp.lam <=  d180+EPSLN); break;
        }
      z = (!ok? 0: z); /* projectable? */
    }

    if (!z) lp.lam = HUGE_VAL;
    if (!z) lp.phi = HUGE_VAL;

    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    int i;
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    for (i = 0; i < 12; ++i) {
        if (P->opaque->pj[i])
            pj_dealloc(P->opaque->pj[i]);
    }

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}



/*
  Zones:

    -180            -40                       180
      +--------------+-------------------------+    Zones 1,2,9,10,11 & 12:
      |1             |2                        |      Mollweide projection
      |              |                         |
      +--------------+-------------------------+    Zones 3,4,5,6,7 & 8:
      |3             |4                        |      Sinusoidal projection
      |              |                         |
    0 +-------+------+-+-----------+-----------+
      |5      |6       |7          |8          |
      |       |        |           |           |
      +-------+--------+-----------+-----------+
      |9      |10      |11         |12         |
      |       |        |           |           |
      +-------+--------+-----------+-----------+
    -180    -100      -20         80          180
*/

#define SETUP(n, proj, x_0, y_0, lon_0) \
    if (!(Q->pj[n-1] = pj_##proj(0))) E_ERROR_0; \
    if (!(Q->pj[n-1] = pj_##proj(Q->pj[n-1]))) E_ERROR_0; \
    Q->pj[n-1]->x0 = x_0; \
    Q->pj[n-1]->y0 = y_0; \
    Q->pj[n-1]->lam0 = lon_0;


PJ *PROJECTION(igh) {
    XY xy1, xy3;
    LP lp = { 0, d4044118 };
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;


    /* sinusoidal zones */
    SETUP(3, sinu, -d100, 0, -d100);
    SETUP(4, sinu,   d30, 0,   d30);
    SETUP(5, sinu, -d160, 0, -d160);
    SETUP(6, sinu,  -d60, 0,  -d60);
    SETUP(7, sinu,   d20, 0,   d20);
    SETUP(8, sinu,  d140, 0,  d140);

    /* mollweide zones */
    SETUP(1, moll, -d100, 0, -d100);

    /* y0 ? */
    xy1 = Q->pj[0]->fwd(lp, Q->pj[0]); /* zone 1 */
    xy3 = Q->pj[2]->fwd(lp, Q->pj[2]); /* zone 3 */
    /* y0 + xy1.y = xy3.y for lt = 40d44'11.8" */
    Q->dy0 = xy3.y - xy1.y;

    Q->pj[0]->y0 = Q->dy0;

    /* mollweide zones (cont'd) */
    SETUP( 2, moll,   d30,  Q->dy0,   d30);
    SETUP( 9, moll, -d160, -Q->dy0, -d160);
    SETUP(10, moll,  -d60, -Q->dy0,  -d60);
    SETUP(11, moll,   d20, -Q->dy0,   d20);
    SETUP(12, moll,  d140, -Q->dy0,  d140);

    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;

    return P;
}


#ifndef PJ_SELFTEST
int pj_igh_selftest (void) {return 0;}
#else

int pj_igh_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=igh   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 223878.49745627123,  111701.07212763709},
        { 223708.37131305804, -111701.07212763709},
        {-222857.74059699223,  111701.07212763709},
        {-223027.86674020503, -111701.07212763709},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.001790489447892545,   0.00089524655489191132},
        { 0.0017904906685957927, -0.00089524655489191132},
        {-0.001790496772112032,   0.00089524655489191132},
        {-0.0017904955514087843, -0.00089524655489191132},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
