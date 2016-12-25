#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(hatano, "Hatano Asymmetrical Equal Area") "\n\tPCyl, Sph.";

#define NITER   20
#define EPS 1e-7
#define ONETOL 1.000001
#define CN  2.67595
#define CS  2.43763
#define RCN 0.37369906014686373063
#define RCS 0.41023453108141924738
#define FYCN    1.75859
#define FYCS    1.93052
#define RYCN    0.56863737426006061674
#define RYCS    0.51799515156538134803
#define FXC 0.85
#define RXC 1.17647058823529411764


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    double th1, c;
    int i;
    (void) P;

    c = sin(lp.phi) * (lp.phi < 0. ? CS : CN);
    for (i = NITER; i; --i) {
        lp.phi -= th1 = (lp.phi + sin(lp.phi) - c) / (1. + cos(lp.phi));
        if (fabs(th1) < EPS) break;
    }
    xy.x = FXC * lp.lam * cos(lp.phi *= .5);
    xy.y = sin(lp.phi) * (lp.phi < 0. ? FYCS : FYCN);

    return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    double th;

    th = xy.y * ( xy.y < 0. ? RYCS : RYCN);
    if (fabs(th) > 1.) {
        if (fabs(th) > ONETOL) {
            I_ERROR;
        } else {
            th = th > 0. ? M_HALFPI : - M_HALFPI;
        }
    } else {
        th = asin(th);
    }

    lp.lam = RXC * xy.x / cos(th);
    th += th;
    lp.phi = (th + sin(th)) * (xy.y < 0. ? RCS : RCN);
    if (fabs(lp.phi) > 1.) {
        if (fabs(lp.phi) > ONETOL) {
           I_ERROR;
        } else {
            lp.phi = lp.phi > 0. ? M_HALFPI : - M_HALFPI;
        }
    } else {
        lp.phi = asin(lp.phi);
    }

    return (lp);
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


PJ *PROJECTION(hatano) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}

#ifndef PJ_SELFTEST
int pj_hatano_selftest (void) {return 0;}
#else

int pj_hatano_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=hatano   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 189878.87894652804,  131409.8024406255},
        { 189881.08195244463, -131409.14227607418},
        {-189878.87894652804,  131409.8024406255},
        {-189881.08195244463, -131409.14227607418},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0021064624821817597,  0.00076095689425791926},
        { 0.0021064624821676096, -0.00076095777439265377},
        {-0.0021064624821817597,  0.00076095689425791926},
        {-0.0021064624821676096, -0.00076095777439265377},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
