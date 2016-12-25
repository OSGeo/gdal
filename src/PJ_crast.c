#define PJ_LIB__
# include   <projects.h>

PROJ_HEAD(crast, "Craster Parabolic (Putnins P4)") "\n\tPCyl., Sph.";

#define XM  0.97720502380583984317
#define RXM 1.02332670794648848847
#define YM  3.06998012383946546542
#define RYM 0.32573500793527994772
#define THIRD   0.333333333333333333


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;
    lp.phi *= THIRD;
    xy.x = XM * lp.lam * (2. * cos(lp.phi + lp.phi) - 1.);
    xy.y = YM * sin(lp.phi);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;
    lp.phi = 3. * asin(xy.y * RYM);
    lp.lam = xy.x * RXM / (2. * cos((lp.phi + lp.phi) * THIRD) - 1);
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
   return pj_dealloc(P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(crast) {
    P->es = 0.0;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_crast_selftest (void) {return 0;}
#else

int pj_crast_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=crast   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };


    XY s_fwd_expect[] = {
        {218280.142056780722,  114306.045604279774},
        {218280.142056780722,  -114306.045604279774},
        {-218280.142056780722,  114306.045604279774},
        {-218280.142056780722,  -114306.045604279774},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        {0.00183225941982580187,  0.00087483943098902331},
        {0.00183225941982580187,  -0.00087483943098902331},
        {-0.00183225941982580187,  0.00087483943098902331},
        {-0.00183225941982580187,  -0.00087483943098902331},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
