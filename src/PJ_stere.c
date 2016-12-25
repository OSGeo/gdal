#define PJ_LIB__
#include    <projects.h>

PROJ_HEAD(stere, "Stereographic") "\n\tAzi, Sph&Ell\n\tlat_ts=";
PROJ_HEAD(ups, "Universal Polar Stereographic") "\n\tAzi, Sph&Ell\n\tsouth";


struct pj_opaque {
    double phits;
    double sinX1;
    double cosX1;
    double akm1;
    int mode;
};

#define sinph0  P->opaque->sinX1
#define cosph0  P->opaque->cosX1
#define EPS10   1.e-10
#define TOL 1.e-8
#define NITER   8
#define CONV    1.e-10
#define S_POLE  0
#define N_POLE  1
#define OBLIQ   2
#define EQUIT   3

static double ssfn_ (double phit, double sinphi, double eccen) {
    sinphi *= eccen;
    return (tan (.5 * (M_HALFPI + phit)) *
       pow ((1. - sinphi) / (1. + sinphi), .5 * eccen));
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double coslam, sinlam, sinX = 0.0, cosX = 0.0, X, A, sinphi;

    coslam = cos (lp.lam);
    sinlam = sin (lp.lam);
    sinphi = sin (lp.phi);
    if (Q->mode == OBLIQ || Q->mode == EQUIT) {
        sinX = sin (X = 2. * atan(ssfn_(lp.phi, sinphi, P->e)) - M_HALFPI);
        cosX = cos (X);
    }

    switch (Q->mode) {
    case OBLIQ:
        A = Q->akm1 / (Q->cosX1 * (1. + Q->sinX1 * sinX +
           Q->cosX1 * cosX * coslam));
        xy.y = A * (Q->cosX1 * sinX - Q->sinX1 * cosX * coslam);
        goto xmul; /* but why not just  xy.x = A * cosX; break;  ? */

    case EQUIT:
        A = 2. * Q->akm1 / (1. + cosX * coslam);
        xy.y = A * sinX;
xmul:
        xy.x = A * cosX;
        break;

    case S_POLE:
        lp.phi = -lp.phi;
        coslam = - coslam;
        sinphi = -sinphi;
    case N_POLE:
        xy.x = Q->akm1 * pj_tsfn (lp.phi, sinphi, P->e);
        xy.y = - xy.x * coslam;
        break;
    }

    xy.x = xy.x * sinlam;
    return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  sinphi, cosphi, coslam, sinlam;

    sinphi = sin(lp.phi);
    cosphi = cos(lp.phi);
    coslam = cos(lp.lam);
    sinlam = sin(lp.lam);

    switch (Q->mode) {
    case EQUIT:
        xy.y = 1. + cosphi * coslam;
        goto oblcon;
    case OBLIQ:
        xy.y = 1. + sinph0 * sinphi + cosph0 * cosphi * coslam;
oblcon:
        if (xy.y <= EPS10) F_ERROR;
        xy.x = (xy.y = Q->akm1 / xy.y) * cosphi * sinlam;
        xy.y *= (Q->mode == EQUIT) ? sinphi :
           cosph0 * sinphi - sinph0 * cosphi * coslam;
        break;
    case N_POLE:
        coslam = - coslam;
        lp.phi = - lp.phi;
    case S_POLE:
        if (fabs (lp.phi - M_HALFPI) < TOL) F_ERROR;
        xy.x = sinlam * ( xy.y = Q->akm1 * tan (M_FORTPI + .5 * lp.phi) );
        xy.y *= coslam;
        break;
    }
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double cosphi, sinphi, tp=0.0, phi_l=0.0, rho, halfe=0.0, halfpi=0.0;
    int i;

    rho = hypot (xy.x, xy.y);

    switch (Q->mode) {
    case OBLIQ:
    case EQUIT:
        cosphi = cos ( tp = 2. * atan2 (rho * Q->cosX1 , Q->akm1) );
        sinphi = sin (tp);
                if ( rho == 0.0 )
            phi_l = asin (cosphi * Q->sinX1);
                else
            phi_l = asin (cosphi * Q->sinX1 + (xy.y * sinphi * Q->cosX1 / rho));

        tp = tan (.5 * (M_HALFPI + phi_l));
        xy.x *= sinphi;
        xy.y = rho * Q->cosX1 * cosphi - xy.y * Q->sinX1* sinphi;
        halfpi = M_HALFPI;
        halfe = .5 * P->e;
        break;
    case N_POLE:
        xy.y = -xy.y;
    case S_POLE:
        phi_l = M_HALFPI - 2. * atan (tp = - rho / Q->akm1);
        halfpi = -M_HALFPI;
        halfe = -.5 * P->e;
        break;
    }

    for (i = NITER;  i--;  phi_l = lp.phi) {
        sinphi = P->e * sin(phi_l);
        lp.phi = 2. * atan (tp * pow ((1.+sinphi)/(1.-sinphi), halfe)) - halfpi;
        if (fabs (phi_l - lp.phi) < CONV) {
            if (Q->mode == S_POLE)
                lp.phi = -lp.phi;
            lp.lam = (xy.x == 0. && xy.y == 0.) ? 0. : atan2 (xy.x, xy.y);
            return lp;
        }
    }
    I_ERROR;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double  c, rh, sinc, cosc;

    sinc = sin (c = 2. * atan ((rh = hypot (xy.x, xy.y)) / Q->akm1));
    cosc = cos (c);
    lp.lam = 0.;

    switch (Q->mode) {
    case EQUIT:
        if (fabs (rh) <= EPS10)
            lp.phi = 0.;
        else
            lp.phi = asin (xy.y * sinc / rh);
        if (cosc != 0. || xy.x != 0.)
            lp.lam = atan2 (xy.x * sinc, cosc * rh);
        break;
    case OBLIQ:
        if (fabs (rh) <= EPS10)
            lp.phi = P->phi0;
        else
            lp.phi = asin (cosc * sinph0 + xy.y * sinc * cosph0 / rh);
        if ((c = cosc - sinph0 * sin (lp.phi)) != 0. || xy.x != 0.)
            lp.lam = atan2 (xy.x * sinc * cosph0, c * rh);
        break;
    case N_POLE:
        xy.y = -xy.y;
    case S_POLE:
        if (fabs (rh) <= EPS10)
            lp.phi = P->phi0;
        else
            lp.phi = asin (Q->mode == S_POLE ? - cosc : cosc);
        lp.lam = (xy.x == 0. && xy.y == 0.) ? 0. : atan2 (xy.x, xy.y);
        break;
    }
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


static PJ *setup(PJ *P) {                   /* general initialization */
    double t;
    struct pj_opaque *Q = P->opaque;

    if (fabs ((t = fabs (P->phi0)) - M_HALFPI) < EPS10)
        Q->mode = P->phi0 < 0. ? S_POLE : N_POLE;
    else
        Q->mode = t > EPS10 ? OBLIQ : EQUIT;
    Q->phits = fabs (Q->phits);

    if (P->es) {
        double X;

        switch (Q->mode) {
        case N_POLE:
        case S_POLE:
            if (fabs (Q->phits - M_HALFPI) < EPS10)
                Q->akm1 = 2. * P->k0 /
                   sqrt (pow (1+P->e,1+P->e) * pow (1-P->e,1-P->e));
            else {
                Q->akm1 = cos (Q->phits) /
                   pj_tsfn (Q->phits, t = sin (Q->phits), P->e);
                t *= P->e;
                Q->akm1 /= sqrt(1. - t * t);
            }
            break;
        case EQUIT:
        case OBLIQ:
            t = sin (P->phi0);
            X = 2. * atan (ssfn_(P->phi0, t, P->e)) - M_HALFPI;
            t *= P->e;
            Q->akm1 = 2. * P->k0 * cos (P->phi0) / sqrt(1. - t * t);
            Q->sinX1 = sin (X);
            Q->cosX1 = cos (X);
            break;
        }
        P->inv = e_inverse;
        P->fwd = e_forward;
    } else {
        switch (Q->mode) {
        case OBLIQ:
            sinph0 = sin (P->phi0);
            cosph0 = cos (P->phi0);
        case EQUIT:
            Q->akm1 = 2. * P->k0;
            break;
        case S_POLE:
        case N_POLE:
            Q->akm1 = fabs (Q->phits - M_HALFPI) >= EPS10 ?
               cos (Q->phits) / tan (M_FORTPI - .5 * Q->phits) :
               2. * P->k0 ;
            break;
        }

        P->inv = s_inverse;
        P->fwd = s_forward;
    }
    return P;
}


PJ *PROJECTION(stere) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->phits = pj_param (P->ctx, P->params, "tlat_ts").i ?
               pj_param (P->ctx, P->params, "rlat_ts").f : M_HALFPI;

    return setup(P);
}


PJ *PROJECTION(ups) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

	/* International Ellipsoid */
	P->phi0 = pj_param(P->ctx, P->params, "bsouth").i ? - M_HALFPI: M_HALFPI;
	if (!P->es) E_ERROR(-34);
	P->k0 = .994;
	P->x0 = 2000000.;
	P->y0 = 2000000.;
	Q->phits = M_HALFPI;
	P->lam0 = 0.;

    return setup(P);
}


#ifndef PJ_SELFTEST
int pj_stere_selftest (void) {return 0;}
#else

int pj_stere_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=stere   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=stere   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 445289.70910023432,  221221.76694834774},
        { 445289.70910023432, -221221.76694835056},
        {-445289.70910023432,  221221.76694834774},
        {-445289.70910023432, -221221.76694835056},
    };

    XY s_fwd_expect[] = {
        { 223407.81025950745,  111737.938996443},
        { 223407.81025950745, -111737.938996443},
        {-223407.81025950745,  111737.938996443},
        {-223407.81025950745, -111737.938996443},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017966305682022392,  0.00090436947502443507},
        { 0.0017966305682022392, -0.00090436947502443507},
        {-0.0017966305682022392,  0.00090436947502443507},
        {-0.0017966305682022392, -0.00090436947502443507},
    };

    LP s_inv_expect[] = {
        { 0.001790493109747395,  0.00089524655465513144},
        { 0.001790493109747395, -0.00089524655465513144},
        {-0.001790493109747395,  0.00089524655465513144},
        {-0.001790493109747395, -0.00089524655465513144},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif





#ifndef PJ_SELFTEST
int pj_ups_selftest (void) {return 0;}
#else

int pj_ups_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=ups   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {2433455.5634384668,  -10412543.301512826},
        {2448749.1185681992,  -10850493.419804076},
        {1566544.4365615332,  -10412543.301512826},
        {1551250.8814318008,  -10850493.419804076},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {-44.998567498074834,  64.9182362867341},
        {-44.995702709112308,  64.917020250675748},
        {-45.004297076028529,  64.915804280954518},
        {-45.001432287066002,  64.914588377560719},
    };

    return pj_generic_selftest (e_args, 0, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, 0, inv_in, e_inv_expect, 0);
}


#endif
