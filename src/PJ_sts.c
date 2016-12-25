#define PJ_LIB__
# include	<projects.h>

PROJ_HEAD(kav5,    "Kavraisky V")         "\n\tPCyl., Sph.";
PROJ_HEAD(qua_aut, "Quartic Authalic")    "\n\tPCyl., Sph.";
PROJ_HEAD(fouc,    "Foucaut")             "\n\tPCyl., Sph.";
PROJ_HEAD(mbt_s,   "McBryde-Thomas Flat-Polar Sine (No. 1)") "\n\tPCyl., Sph.";


struct pj_opaque {
	double C_x, C_y, C_p; \
	int tan_mode;
};


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double c;

	xy.x = Q->C_x * lp.lam * cos(lp.phi);
	xy.y = Q->C_y;
	lp.phi *= Q->C_p;
	c = cos(lp.phi);
	if (Q->tan_mode) {
		xy.x *= c * c;
		xy.y *= tan (lp.phi);
	} else {
		xy.x /= c;
		xy.y *= sin (lp.phi);
	}
	return xy;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
	double c;

	xy.y /= Q->C_y;
	c = cos (lp.phi = Q->tan_mode ? atan (xy.y) : aasin (P->ctx, xy.y));
	lp.phi /= Q->C_p;
	lp.lam = xy.x / (Q->C_x * cos(lp.phi));
	if (Q->tan_mode)
		lp.lam /= c * c;
	else
		lp.lam *= c;
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


static PJ *setup(PJ *P, double p, double q, int mode) {
	P->es  = 0.;
	P->inv = s_inverse;
	P->fwd = s_forward;
	P->opaque->C_x = q / p;
	P->opaque->C_y = p;
	P->opaque->C_p = 1/ q;
	P->opaque->tan_mode = mode;
	return P;
}





PJ *PROJECTION(fouc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;
    return setup(P, 2., 2., 1);
}


#ifndef PJ_SELFTEST
int pj_fouc_selftest (void) {return 0;}
#else
int pj_fouc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=fouc   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=fouc   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222588.12067589167,  111322.31670069379},
        {222588.12067589167,  -111322.31670069379},
        {-222588.12067589167,  111322.31670069379},
        {-222588.12067589167,  -111322.31670069379},
    };

    XY s_fwd_expect[] = {
        {223351.10900341379,  111703.9077217125},
        {223351.10900341379,  -111703.9077217125},
        {-223351.10900341379,  111703.9077217125},
        {-223351.10900341379,  -111703.9077217125},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966305685702751,  0.00089831528410111959},
        {0.0017966305685702751,  -0.00089831528410111959},
        {-0.0017966305685702751,  0.00089831528410111959},
        {-0.0017966305685702751,  -0.00089831528410111959},
    };

    LP s_inv_expect[] = {
        {0.0017904931101116717,  0.00089524655487369749},
        {0.0017904931101116717,  -0.00089524655487369749},
        {-0.0017904931101116717,  0.00089524655487369749},
        {-0.0017904931101116717,  -0.00089524655487369749},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}
#endif






PJ *PROJECTION(kav5) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    return setup(P, 1.50488, 1.35439, 0);
}


#ifndef PJ_SELFTEST
int pj_kav5_selftest (void) {return 0;}
#else
int pj_kav5_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=kav5   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=kav5   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {200360.90530882866,  123685.08247699818},
        {200360.90530882866,  -123685.08247699818},
        {-200360.90530882866,  123685.08247699818},
        {-200360.90530882866,  -123685.08247699818},
    };

    XY s_fwd_expect[] = {
        {201047.7031108776,  124109.05062917093},
        {201047.7031108776,  -124109.05062917093},
        {-201047.7031108776,  124109.05062917093},
        {-201047.7031108776,  -124109.05062917093},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0019962591348533314,  0.00080848256185253912},
        {0.0019962591348533314,  -0.00080848256185253912},
        {-0.0019962591348533314,  0.00080848256185253912},
        {-0.0019962591348533314,  -0.00080848256185253912},
    };

    LP s_inv_expect[] = {
        {0.0019894397264987643,  0.00080572070962591153},
        {0.0019894397264987643,  -0.00080572070962591153},
        {-0.0019894397264987643,  0.00080572070962591153},
        {-0.0019894397264987643,  -0.00080572070962591153},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}
#endif





PJ *PROJECTION(qua_aut) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;
    return setup(P, 2., 2., 0);
}

#ifndef PJ_SELFTEST
int pj_qua_aut_selftest (void) {return 0;}
#else
int pj_qua_aut_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=qua_aut   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=qua_aut   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222613.54903309655,  111318.07788798446},
        {222613.54903309655,  -111318.07788798446},
        {-222613.54903309655,  111318.07788798446},
        {-222613.54903309655,  -111318.07788798446},
    };

    XY s_fwd_expect[] = {
        {223376.62452402918,  111699.65437918637},
        {223376.62452402918,  -111699.65437918637},
        {-223376.62452402918,  111699.65437918637},
        {-223376.62452402918,  -111699.65437918637},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017966305684046586,  0.00089831528412872229},
        {0.0017966305684046586,  -0.00089831528412872229},
        {-0.0017966305684046586,  0.00089831528412872229},
        {-0.0017966305684046586,  -0.00089831528412872229},
    };

    LP s_inv_expect[] = {
        {0.0017904931099477471,  0.00089524655490101819},
        {0.0017904931099477471,  -0.00089524655490101819},
        {-0.0017904931099477471,  0.00089524655490101819},
        {-0.0017904931099477471,  -0.00089524655490101819},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}
#endif





PJ *PROJECTION(mbt_s) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;
    return setup(P, 1.48875, 1.36509, 0);
}

#ifndef PJ_SELFTEST
int pj_mbt_s_selftest (void) {return 0;}
#else
int pj_mbt_s_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=mbt_s   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=mbt_s   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {204131.51785027285,  121400.33022550763},
        {204131.51785027285,  -121400.33022550763},
        {-204131.51785027285,  121400.33022550763},
        {-204131.51785027285,  -121400.33022550763},
    };

    XY s_fwd_expect[] = {
        {204831.24057099217,  121816.46669603503},
        {204831.24057099217,  -121816.46669603503},
        {-204831.24057099217,  121816.46669603503},
        {-204831.24057099217,  -121816.46669603503},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0019593827209883237,  0.00082369854658027549},
        {0.0019593827209883237,  -0.00082369854658027549},
        {-0.0019593827209883237,  0.00082369854658027549},
        {-0.0019593827209883237,  -0.00082369854658027549},
    };

    LP s_inv_expect[] = {
        {0.0019526892859206603,  0.00082088471512331508},
        {0.0019526892859206603,  -0.00082088471512331508},
        {-0.0019526892859206603,  0.00082088471512331508},
        {-0.0019526892859206603,  -0.00082088471512331508},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}
#endif
