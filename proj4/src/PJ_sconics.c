#define PJ_LIB__
#include    <projects.h>


struct pj_opaque {
    double  n;
    double  rho_c;
    double  rho_0;
    double  sig;
    double  c1, c2;
    int     type;
};


#define EULER 0
#define MURD1 1
#define MURD2 2
#define MURD3 3
#define PCONIC 4
#define TISSOT 5
#define VITK1 6
#define EPS10   1.e-10
#define EPS 1e-10
#define LINE2 "\n\tConic, Sph\n\tlat_1= and lat_2="

PROJ_HEAD(euler, "Euler")                LINE2;
PROJ_HEAD(murd1, "Murdoch I")            LINE2;
PROJ_HEAD(murd2, "Murdoch II")           LINE2;
PROJ_HEAD(murd3, "Murdoch III")          LINE2;
PROJ_HEAD(pconic, "Perspective Conic")   LINE2;
PROJ_HEAD(tissot, "Tissot")              LINE2;
PROJ_HEAD(vitk1, "Vitkovsky I")          LINE2;



/* get common factors for simple conics */
static int phi12(PJ *P, double *del) {
    double p1, p2;
    int err = 0;

    if (!pj_param(P->ctx, P->params, "tlat_1").i ||
        !pj_param(P->ctx, P->params, "tlat_2").i) {
        err = -41;
    } else {
        p1 = pj_param(P->ctx, P->params, "rlat_1").f;
        p2 = pj_param(P->ctx, P->params, "rlat_2").f;
        *del = 0.5 * (p2 - p1);
        P->opaque->sig = 0.5 * (p2 + p1);
        err = (fabs(*del) < EPS || fabs(P->opaque->sig) < EPS) ? -42 : 0;
        *del = *del;
    }
    return err;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0, 0.0};
    struct pj_opaque *Q = P->opaque;
    double rho;

    switch (Q->type) {
    case MURD2:
        rho = Q->rho_c + tan (Q->sig - lp.phi);
        break;
    case PCONIC:
        rho = Q->c2 * (Q->c1 - tan (lp.phi - Q->sig));
        break;
    default:
        rho = Q->rho_c - lp.phi;
        break;
    }

    xy.x = rho * sin ( lp.lam *= Q->n );
    xy.y = Q->rho_0 - rho * cos (lp.lam);
    return xy;
}


static LP s_inverse (XY xy, PJ *P) {  /* Spheroidal, (and ellipsoidal?) inverse */
    LP lp = {0.0, 0.0};
    struct pj_opaque *Q = P->opaque;
    double rho;

    rho = hypot (xy.x,  xy.y = Q->rho_0 - xy.y);
    if (Q->n < 0.) {
        rho = - rho;
        xy.x = - xy.x;
        xy.y = - xy.y;
    }

    lp.lam = atan2 (xy.x, xy.y) / Q->n;

    switch (Q->type) {
    case PCONIC:
        lp.phi = atan (Q->c1 - rho / Q->c2) + Q->sig;
        break;
    case MURD2:
        lp.phi = Q->sig - atan(rho - Q->rho_c);
        break;
    default:
        lp.phi = Q->rho_c - rho;
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


static PJ *setup(PJ *P, int type) {
    double del, cs;
    int i;
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;
    Q->type = type;

    i = phi12 (P, &del);
    if(i)
        E_ERROR(i);
    switch (Q->type) {

    case TISSOT:
        Q->n = sin (Q->sig);
        cs = cos (del);
        Q->rho_c = Q->n / cs + cs / Q->n;
        Q->rho_0 = sqrt ((Q->rho_c - 2 * sin (P->phi0)) / Q->n);
        break;

    case MURD1:
        Q->rho_c = sin(del)/(del * tan(Q->sig)) + Q->sig;
        Q->rho_0 = Q->rho_c - P->phi0;
        Q->n = sin(Q->sig);
        break;

    case MURD2:
        Q->rho_c = (cs = sqrt (cos (del))) / tan (Q->sig);
        Q->rho_0 = Q->rho_c + tan (Q->sig - P->phi0);
        Q->n = sin (Q->sig) * cs;
        break;

    case MURD3:
        Q->rho_c = del / (tan(Q->sig) * tan(del)) + Q->sig;
        Q->rho_0 = Q->rho_c - P->phi0;
        Q->n = sin (Q->sig) * sin (del) * tan (del) / (del * del);
        break;

    case EULER:
        Q->n = sin (Q->sig) * sin (del) / del;
        del *= 0.5;
        Q->rho_c = del / (tan (del) * tan (Q->sig)) + Q->sig;
        Q->rho_0 = Q->rho_c - P->phi0;
        break;

    case PCONIC:
        Q->n = sin (Q->sig);
        Q->c2 = cos (del);
        Q->c1 = 1./tan (Q->sig);
        if (fabs (del = P->phi0 - Q->sig) - EPS10 >= M_HALFPI)
            E_ERROR(-43);
        Q->rho_0 = Q->c2 * (Q->c1 - tan (del));
        break;

    case VITK1:
        Q->n = (cs = tan (del)) * sin (Q->sig) / del;
        Q->rho_c = del / (cs * tan (Q->sig)) + Q->sig;
        Q->rho_0 = Q->rho_c - P->phi0;
        break;
    }

    P->inv = s_inverse;
    P->fwd = s_forward;
    P->es = 0;
    return (P);
}


PJ *PROJECTION(euler) {
    return setup(P, EULER);
}


PJ *PROJECTION(tissot) {
    return setup(P, TISSOT);
}


PJ *PROJECTION(murd1) {
    return setup(P, MURD1);
}


PJ *PROJECTION(murd2) {
    return setup(P, MURD2);
}


PJ *PROJECTION(murd3) {
    return setup(P, MURD3);
}


PJ *PROJECTION(pconic) {
    return setup(P, PCONIC);
}


PJ *PROJECTION(vitk1) {
    return setup(P, VITK1);
}


#ifndef PJ_SELFTEST
int pj_euler_selftest (void) {return 0;}
#else

int pj_euler_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=euler   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=euler   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222597.63465910763,  111404.24054991946},
        {222767.16563187627,  -111234.6764910177},
        {-222597.63465910763,  111404.24054991946},
        {-222767.16563187627,  -111234.6764910177},
    };

    XY s_fwd_expect[] = {
        {223360.65559869423,  111786.11238979101},
        {223530.76769031584,  -111615.96709862351},
        {-223360.65559869423,  111786.11238979101},
        {-223530.76769031584,  -111615.96709862351},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017962807023075235,  0.0008983146697688839},
        {0.0017962794738334226,  -0.00089831589842987965},
        {-0.0017962807023075235,  0.0008983146697688839},
        {-0.0017962794738334226,  -0.00089831589842987965},
    };

    LP s_inv_expect[] = {
        {0.0017901444369360026,  0.00089524594522202015},
        {0.001790143216840731,  -0.00089524716533368484},
        {-0.0017901444369360026,  0.00089524594522202015},
        {-0.001790143216840731,  -0.00089524716533368484},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif






#ifndef PJ_SELFTEST
int pj_murd1_selftest (void) {return 0;}
#else

int pj_murd1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=murd1   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=murd1   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222600.81347355421,  111404.24418054636},
        {222770.3492878644,  -111234.6728566746},
        {-222600.81347355421,  111404.24418054636},
        {-222770.3492878644,  -111234.6728566746},
    };

    XY s_fwd_expect[] = {
        {223363.84530949194,  111786.11603286299},
        {223533.96225925098,  -111615.96345182261},
        {-223363.84530949194,  111786.11603286299},
        {-223533.96225925098,  -111615.96345182261},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017962550410516366,  0.0008983146697688839},
        {0.0017962538125775522,  -0.00089831589842987965},
        {-0.0017962550410516366,  0.0008983146697688839},
        {-0.0017962538125775522,  -0.00089831589842987965},
    };

    LP s_inv_expect[] = {
        {0.0017901188633413715,  0.00089524594522202015},
        {0.0017901176432461162,  -0.00089524716492657387},
        {-0.0017901188633413715,  0.00089524594522202015},
        {-0.0017901176432461162,  -0.00089524716492657387},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif














#ifndef PJ_SELFTEST
int pj_murd2_selftest (void) {return 0;}
#else

int pj_murd2_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=murd2   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=murd2   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222588.09975123021,  111426.14002741246},
        {222757.72626701824,  -111341.43131750476},
        {-222588.09975123021,  111426.14002741246},
        {-222757.72626701824,  -111341.43131750476},
    };

    XY s_fwd_expect[] = {
        {223351.08800702673,  111808.08693438848},
        {223521.2959691704,  -111723.08785967289},
        {-223351.08800702673,  111808.08693438848},
        {-223521.2959691704,  -111723.08785967289},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017963574947305447,  0.00089788747830845382},
        {0.0017963562661689487,  -0.00089788809264252983},
        {-0.0017963574947305447,  0.00089788747830845382},
        {-0.0017963562661689487,  -0.00089788809264252983},
    };

    LP s_inv_expect[] = {
        {0.0017902209670287586,  0.00089482021163422854},
        {0.0017902197468465887,  -0.00089482082161134206},
        {-0.0017902209670287586,  0.00089482021163422854},
        {-0.0017902197468465887,  -0.00089482082161134206},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif










#ifndef PJ_SELFTEST
int pj_murd3_selftest (void) {return 0;}
#else

int pj_murd3_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=murd3   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=murd3   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222600.81407757697,  111404.24660137216},
        {222770.35473389886,  -111234.67043217793},
        {-222600.81407757697,  111404.24660137216},
        {-222770.35473389886,  -111234.67043217793},
    };

    XY s_fwd_expect[] = {
        {223363.84591558515,  111786.11846198692},
        {223533.96772395336,  -111615.96101901523},
        {-223363.84591558515,  111786.11846198692},
        {-223533.96772395336,  -111615.96101901523},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017962550166583809,  0.0008983146697688839},
        {0.0017962537881492445,  -0.00089831589842987965},
        {-0.0017962550166583809,  0.0008983146697688839},
        {-0.0017962537881492445,  -0.00089831589842987965},
    };

    LP s_inv_expect[] = {
        {0.0017901188390313859,  0.00089524594522202015},
        {0.0017901176189013177,  -0.00089524716533368484},
        {-0.0017901188390313859,  0.00089524594522202015},
        {-0.0017901176189013177,  -0.00089524716533368484},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif











#ifndef PJ_SELFTEST
int pj_pconic_selftest (void) {return 0;}
#else

int pj_pconic_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=pconic   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=pconic   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222588.09884161691,  111416.60477006658},
        {222757.71809109033,  -111331.88153107995},
        {-222588.09884161691,  111416.60477006658},
        {-222757.71809109033,  -111331.88153107995},
    };

    XY s_fwd_expect[] = {
        {223351.08709429545,  111798.5189920546},
        {223521.28776521701,  -111713.50533845725},
        {-223351.08709429545,  111798.5189920546},
        {-223521.28776521701,  -111713.50533845725},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017963575313784969,  0.0008979644089172499},
        {0.0017963563027642206,  -0.00089796502355327969},
        {-0.0017963575313784969,  0.0008979644089172499},
        {-0.0017963563027642206,  -0.00089796502355327969},
    };

    LP s_inv_expect[] = {
        {0.0017902210035514285,  0.0008948968793741558},
        {0.0017902197833169374,  -0.00089489748965381963},
        {-0.0017902210035514285,  0.0008948968793741558},
        {-0.0017902197833169374,  -0.00089489748965381963},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif















#ifndef PJ_SELFTEST
int pj_tissot_selftest (void) {return 0;}
#else

int pj_tissot_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=tissot   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=tissot   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222641.07869963095,  54347.828487281469},
        {222810.61451394114,  -168291.08854993948},
        {-222641.07869963095,  54347.828487281469},
        {-222810.61451394114,  -168291.08854993948},
    };

    XY s_fwd_expect[] = {
        {223404.24855684943,  54534.122161157939},
        {223574.36550660848,  -168867.95732352766},
        {-223404.24855684943,  54534.122161157939},
        {-223574.36550660848,  -168867.95732352766},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017962807107425871,  0.51344495513064536},
        {0.0017962794822333915,  0.51164832456244658},
        {-0.0017962807107425871,  0.51344495513064536},
        {-0.0017962794822333915,  0.51164832456244658},
    };

    LP s_inv_expect[] = {
        {0.0017901444453421915,  0.51344188640609856},
        {0.001790143225212064,  0.51165139329554277},
        {-0.0017901444453421915,  0.51344188640609856},
        {-0.001790143225212064,  0.51165139329554277},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif








#ifndef PJ_SELFTEST
int pj_vitk1_selftest (void) {return 0;}
#else

int pj_vitk1_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=vitk1   +ellps=GRS80  +lat_1=0.5 +lat_2=2 +n=0.5"};
    char s_args[] = {"+proj=vitk1   +a=6400000    +lat_1=0.5 +lat_2=2 +n=0.5"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        {222607.17121145778,  111404.25144243463},
        {222776.71670959776,  -111234.66558744459},
        {-222607.17121145778,  111404.25144243463},
        {-222776.71670959776,  -111234.66558744459},
    };

    XY s_fwd_expect[] = {
        {223370.22484047143,  111786.12331964359},
        {223540.3515072545,  -111615.9561576751},
        {-223370.22484047143,  111786.12331964359},
        {-223540.3515072545,  -111615.9561576751},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        {0.0017962037198570686,  0.0008983146697688839},
        {0.0017962024913830157,  -0.00089831589842987965},
        {-0.0017962037198570686,  0.0008983146697688839},
        {-0.0017962024913830157,  -0.00089831589842987965},
    };

    LP s_inv_expect[] = {
        {0.0017900677174648159,  0.00089524594522202015},
        {0.0017900664973695916,  -0.00089524716533368484},
        {-0.0017900677174648159,  0.00089524594522202015},
        {-0.0017900664973695916,  -0.00089524716533368484},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
