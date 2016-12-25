#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(calcofi,
    "Cal Coop Ocean Fish Invest Lines/Stations") "\n\tCyl, Sph&Ell";

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <proj_api.h>
#include <errno.h>

/* Conversions for the California Cooperative Oceanic Fisheries Investigations
Line/Station coordinate system following the algorithm of:
Eber, L.E., and  R.P. Hewitt. 1979. Conversion algorithms for the CALCOFI
station grid. California Cooperative Oceanic Fisheries Investigations Reports
20:135-137. (corrected for typographical errors).
http://www.calcofi.org/publications/calcofireports/v20/Vol_20_Eber___Hewitt.pdf
They assume 1 unit of CalCOFI Line == 1/5 degree in longitude or
meridional units at reference point O, and similarly 1 unit of CalCOFI
Station == 1/15 of a degree at O.
By convention, CalCOFI Line/Station conversions use Clarke 1866 but we use
whatever ellipsoid is provided. */


#define EPS10 1.e-10
#define DEG_TO_LINE 5
#define DEG_TO_STATION 15
#define LINE_TO_RAD 0.0034906585039886592
#define STATION_TO_RAD 0.0011635528346628863
#define PT_O_LINE 80 /* reference point O is at line 80,  */
#define PT_O_STATION 60 /* station 60,  */
#define PT_O_LAMBDA -2.1144663887911301 /* lon -121.15 and */
#define PT_O_PHI 0.59602993955606354 /* lat 34.15 */
#define ROTATION_ANGLE 0.52359877559829882 /*CalCOFI angle of 30 deg in rad */


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    double oy; /* pt O y value in Mercator */
    double l1; /* l1 and l2 are distances calculated using trig that sum
               to the east/west distance between point O and point xy */
    double l2;
    double ry; /* r is the point on the same station as o (60) and the same
               line as xy xy, r, o form a right triangle */
    /* if the user has specified +lon_0 or +k0 for some reason,
    we're going to ignore it so that xy is consistent with point O */
    lp.lam = lp.lam + P->lam0;
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10) F_ERROR;
    xy.x = lp.lam;
    xy.y = -log(pj_tsfn(lp.phi, sin(lp.phi), P->e)); /* Mercator transform xy*/
    oy = -log(pj_tsfn(PT_O_PHI, sin(PT_O_PHI), P->e));
    l1 = (xy.y - oy) * tan(ROTATION_ANGLE);
    l2 = -xy.x - l1 + PT_O_LAMBDA;
    ry = l2 * cos(ROTATION_ANGLE) * sin(ROTATION_ANGLE) + xy.y;
    ry = pj_phi2(P->ctx, exp(-ry), P->e); /*inverse Mercator*/
    xy.x = PT_O_LINE - RAD_TO_DEG *
        (ry - PT_O_PHI) * DEG_TO_LINE / cos(ROTATION_ANGLE);
    xy.y = PT_O_STATION + RAD_TO_DEG *
        (ry - lp.phi) * DEG_TO_STATION / sin(ROTATION_ANGLE);
    /* set a = 1, x0 = 0, and y0 = 0 so that no further unit adjustments
    are done */
    P->a = 1;
    P->x0 = 0;
    P->y0 = 0;
    return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double oy;
    double l1;
    double l2;
    double ry;
    lp.lam = lp.lam + P->lam0;
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10) F_ERROR;
    xy.x = lp.lam;
    xy.y = log(tan(M_FORTPI + .5 * lp.phi));
    oy = log(tan(M_FORTPI + .5 * PT_O_PHI));
    l1 = (xy.y - oy) * tan(ROTATION_ANGLE);
    l2 = -xy.x - l1 + PT_O_LAMBDA;
    ry = l2 * cos(ROTATION_ANGLE) * sin(ROTATION_ANGLE) + xy.y;
    ry = M_HALFPI - 2. * atan(exp(-ry));
    xy.x = PT_O_LINE - RAD_TO_DEG *
        (ry - PT_O_PHI) * DEG_TO_LINE / cos(ROTATION_ANGLE);
    xy.y = PT_O_STATION + RAD_TO_DEG *
        (ry - lp.phi) * DEG_TO_STATION / sin(ROTATION_ANGLE);
    P->a = 1;
    P->x0 = 0;
    P->y0 = 0;
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    double ry; /* y value of point r */
    double oymctr; /* Mercator-transformed y value of point O */
    double rymctr; /* Mercator-transformed ry */
    double xymctr; /* Mercator-transformed xy.y */
    double l1;
    double l2;
    /* turn x and y back into Line/Station */
    xy.x /= P->ra;
    xy.y /= P->ra;
    ry = PT_O_PHI - LINE_TO_RAD * (xy.x - PT_O_LINE) *
        cos(ROTATION_ANGLE);
    lp.phi = ry - STATION_TO_RAD * (xy.y - PT_O_STATION) * sin(ROTATION_ANGLE);
    oymctr = -log(pj_tsfn(PT_O_PHI, sin(PT_O_PHI), P->e));
    rymctr = -log(pj_tsfn(ry, sin(ry), P->e));
    xymctr = -log(pj_tsfn(lp.phi, sin(lp.phi), P->e));
    l1 = (xymctr - oymctr) * tan(ROTATION_ANGLE);
    l2 = (rymctr - xymctr) / (cos(ROTATION_ANGLE) * sin(ROTATION_ANGLE));
    lp.lam = PT_O_LAMBDA - (l1 + l2);
    P->over = 1;
    return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double ry;
    double oymctr;
    double rymctr;
    double xymctr;
    double l1;
    double l2;
    xy.x /= P->ra;
    xy.y /= P->ra;
    ry = PT_O_PHI - LINE_TO_RAD * (xy.x - PT_O_LINE) *
        cos(ROTATION_ANGLE);
    lp.phi = ry - STATION_TO_RAD * (xy.y - PT_O_STATION) * sin(ROTATION_ANGLE);
    oymctr = log(tan(M_FORTPI + .5 * PT_O_PHI));
    rymctr = log(tan(M_FORTPI + .5 * ry));
    xymctr = log(tan(M_FORTPI + .5 * lp.phi));
    l1 = (xymctr - oymctr) * tan(ROTATION_ANGLE);
    l2 = (rymctr - xymctr) / (cos(ROTATION_ANGLE) * sin(ROTATION_ANGLE));
    lp.lam = PT_O_LAMBDA - (l1 + l2);
    P->over = 1;
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc (P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(calcofi) {
    P->opaque = 0;

    if (P->es) { /* ellipsoid */
        P->inv = e_inverse;
        P->fwd = e_forward;
    } else { /* sphere */
        P->inv = s_inverse;
        P->fwd = s_forward;
    }
    return P;
}


#ifndef PJ_SELFTEST
int pj_calcofi_selftest (void) {return 0;}
#else

int pj_calcofi_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=calcofi   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=calcofi   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {508.44487214981905,  -1171.7648604175156},
        {514.99916815188112,  -1145.8219814677668},
        {500.68538412539851,  -1131.4453779204598},
        {507.36971913666355,  -1106.1782014834275},
    };

    XY s_fwd_expect[] = {
        {507.09050748781806,  -1164.7273751978314},
        {513.68613637462886,  -1138.9992682173072},
        {499.33626147591531,  -1124.4351309968195},
        {506.0605703929898,  -1099.3756650673038},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {-110.36330792469906,  12.032056975840137},
        {-98.455008863288782,  18.698723642506803},
        {-207.4470245036909,  81.314089278595247},
        {-62.486322854481287,  87.980755945261919},
    };

    LP s_inv_expect[] = {
        {-110.30519040955151,  12.032056975840137},
        {-98.322360950234085,  18.698723642506803},
        {-207.54490681381429,  81.314089278595247},
        {-62.576950371885275,  87.980755945261919},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
