#define PJ_LIB__
# include   <projects.h>

PROJ_HEAD(eck2, "Eckert II") "\n\tPCyl. Sph.";

#define FXC     0.46065886596178063902
#define FYC     1.44720250911653531871
#define C13     0.33333333333333333333
#define ONEEPS  1.0000001


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    (void) P;

    xy.x = FXC * lp.lam * (xy.y = sqrt(4. - 3. * sin(fabs(lp.phi))));
    xy.y = FYC * (2. - xy.y);
    if ( lp.phi < 0.) xy.y = -xy.y;

    return (xy);
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    (void) P;

    lp.lam = xy.x / (FXC * ( lp.phi = 2. - fabs(xy.y) / FYC) );
    lp.phi = (4. - lp.phi * lp.phi) * C13;
    if (fabs(lp.phi) >= 1.) {
        if (fabs(lp.phi) > ONEEPS)  I_ERROR
        else
            lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
    } else
        lp.phi = asin(lp.phi);
    if (xy.y < 0)
        lp.phi = -lp.phi;
    return (lp);
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    return pj_dealloc (P);
}


static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(eck2) {
    P->es = 0.;
    P->inv = s_inverse;
    P->fwd = s_forward;

    return P;
}


#ifndef PJ_SELFTEST
int pj_eck2_selftest (void) {return 0;}
#else

int pj_eck2_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char s_args[] = {"+proj=eck2   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY s_fwd_expect[] = {
        { 204472.87090796008,  121633.73497524235},
        { 204472.87090796008, -121633.73497524235},
        {-204472.87090796008,  121633.73497524235},
        {-204472.87090796008, -121633.73497524235},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP s_inv_expect[] = {
        { 0.0019434150820034624,  0.00082480429919795412},
        { 0.0019434150820034624, -0.00082480429919795412},
        {-0.0019434150820034624,  0.00082480429919795412},
        {-0.0019434150820034624, -0.00082480429919795412},
    };

    return pj_generic_selftest (0, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, 0, s_fwd_expect, inv_in, 0, s_inv_expect);
}


#endif
