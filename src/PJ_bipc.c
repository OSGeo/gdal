#define PJ_LIB__
#include <projects.h>

PROJ_HEAD(bipc, "Bipolar conic of western hemisphere") "\n\tConic Sph.";

# define EPS    1e-10
# define EPS10  1e-10
# define ONEEPS 1.000000001
# define NITER  10
# define lamB   -.34894976726250681539
# define n  .63055844881274687180
# define F  1.89724742567461030582
# define Azab   .81650043674686363166
# define Azba   1.82261843856185925133
# define T  1.27246578267089012270
# define rhoc   1.20709121521568721927
# define cAzc   .69691523038678375519
# define sAzc   .71715351331143607555
# define C45    .70710678118654752469
# define S45    .70710678118654752410
# define C20    .93969262078590838411
# define S20    -.34202014332566873287
# define R110   1.91986217719376253360
# define R104   1.81514242207410275904


struct pj_opaque {
    int noskew;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double cphi, sphi, tphi, t, al, Az, z, Av, cdlam, sdlam, r;
    int tag;

    cphi = cos(lp.phi);
    sphi = sin(lp.phi);
    cdlam = cos(sdlam = lamB - lp.lam);
    sdlam = sin(sdlam);
    if (fabs(fabs(lp.phi) - M_HALFPI) < EPS10) {
        Az = lp.phi < 0. ? M_PI : 0.;
        tphi = HUGE_VAL;
    } else {
        tphi = sphi / cphi;
        Az = atan2(sdlam , C45 * (tphi - cdlam));
    }
    if( (tag = (Az > Azba)) ) {
        cdlam = cos(sdlam = lp.lam + R110);
        sdlam = sin(sdlam);
        z = S20 * sphi + C20 * cphi * cdlam;
        if (fabs(z) > 1.) {
            if (fabs(z) > ONEEPS) F_ERROR
            else z = z < 0. ? -1. : 1.;
        } else
            z = acos(z);
        if (tphi != HUGE_VAL)
            Az = atan2(sdlam, (C20 * tphi - S20 * cdlam));
        Av = Azab;
        xy.y = rhoc;
    } else {
        z = S45 * (sphi + cphi * cdlam);
        if (fabs(z) > 1.) {
            if (fabs(z) > ONEEPS) F_ERROR
            else z = z < 0. ? -1. : 1.;
        } else
            z = acos(z);
        Av = Azba;
        xy.y = -rhoc;
    }
    if (z < 0.) F_ERROR;
    r = F * (t = pow(tan(.5 * z), n));
    if ((al = .5 * (R104 - z)) < 0.) F_ERROR;
    al = (t + pow(al, n)) / T;
    if (fabs(al) > 1.) {
        if (fabs(al) > ONEEPS) F_ERROR
        else al = al < 0. ? -1. : 1.;
    } else
        al = acos(al);
    if (fabs(t = n * (Av - Az)) < al)
        r /= cos(al + (tag ? t : -t));
    xy.x = r * sin(t);
    xy.y += (tag ? -r : r) * cos(t);
    if (Q->noskew) {
        t = xy.x;
        xy.x = -xy.x * cAzc - xy.y * sAzc;
        xy.y = -xy.y * cAzc + t * sAzc;
    }
    return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double t, r, rp, rl, al, z, fAz, Az, s, c, Av;
    int neg, i;

    if (Q->noskew) {
        t = xy.x;
        xy.x = -xy.x * cAzc + xy.y * sAzc;
        xy.y = -xy.y * cAzc - t * sAzc;
    }
    if( (neg = (xy.x < 0.)) ) {
        xy.y = rhoc - xy.y;
        s = S20;
        c = C20;
        Av = Azab;
    } else {
        xy.y += rhoc;
        s = S45;
        c = C45;
        Av = Azba;
    }
    rl = rp = r = hypot(xy.x, xy.y);
    fAz = fabs(Az = atan2(xy.x, xy.y));
    for (i = NITER; i ; --i) {
        z = 2. * atan(pow(r / F,1 / n));
        al = acos((pow(tan(.5 * z), n) +
           pow(tan(.5 * (R104 - z)), n)) / T);
        if (fAz < al)
            r = rp * cos(al + (neg ? Az : -Az));
        if (fabs(rl - r) < EPS)
            break;
        rl = r;
    }
    if (! i) I_ERROR;
    Az = Av - Az / n;
    lp.phi = asin(s * cos(z) + c * sin(z) * cos(Az));
    lp.lam = atan2(sin(Az), c / tan(z) - s * cos(Az));
    if (neg)
        lp.lam -= R110;
    else
        lp.lam = lamB - lp.lam;
    return (lp);
}


static void *freeup_new (PJ *P) {                        /* Destructor */
    if (0==P)
        return 0;
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(bipc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->noskew = pj_param(P->ctx, P->params, "bns").i;
    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0.;
    return P;
}


#ifndef PJ_SELFTEST
int pj_bipc_selftest (void) {return 0;}
#else

int pj_bipc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=bipc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=bipc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {2452160.2177257561,  -14548450.759654747},
        {2447915.213725341,  -14763427.21279873},
        {2021695.5229349085,  -14540413.695283702},
        {2018090.5030046992,  -14755620.651414108},
    };

    XY s_fwd_expect[] = {
        {2460565.7409749646,  -14598319.9893308},
        {2456306.1859352002,  -14814033.339502094},
        {2028625.4978190989,  -14590255.375482792},
        {2025008.1205891429,  -14806200.018759441},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {-73.038700284978702,  17.248118466239116},
        {-73.03730373933017,  17.249414978178777},
        {-73.03589317304332,  17.245536403008771},
        {-73.034496627213585,  17.246832895573739},
    };

    LP s_inv_expect[] = {
        {-73.038693104942126,  17.248116270440242},
        {-73.037301330021322,  17.24940835333777},
        {-73.035895582251086,  17.245543027866539},
        {-73.034503807150301,  17.246835091521532},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
