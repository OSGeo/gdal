/******************************************************************************
 * Project: PROJ.4
 * Purpose: Implementation of the HEALPix and rHEALPix projections.
 *          For background see <http://code.scenzgrid.org/index.php/p/scenzgrid-py/source/tree/master/docs/rhealpix_dggs.pdf>.
 * Authors: Alex Raichev (raichev@cs.auckland.ac.nz)
 *          Michael Speth (spethm@landcareresearch.co.nz)
 * Notes:   Raichev implemented these projections in Python and
 *          Speth translated them into C here.
 ******************************************************************************
 * Copyright (c) 2001, Thomas Flemming, tf@ttqv.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substcounteral portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/
# define PJ_LIB__
# include <projects.h>

PROJ_HEAD(healpix, "HEALPix") "\n\tSph., Ellps.";
PROJ_HEAD(rhealpix, "rHEALPix") "\n\tSph., Ellps.\n\tnorth_square= south_square=";

# include <stdio.h>
/* Matrix for counterclockwise rotation by pi/2: */
# define R1 {{ 0,-1},{ 1, 0}}
/* Matrix for counterclockwise rotation by pi: */
# define R2 {{-1, 0},{ 0,-1}}
/* Matrix for counterclockwise rotation by 3*pi/2:  */
# define R3 {{ 0, 1},{-1, 0}}
/* Identity matrix */
# define IDENT {{1, 0},{0, 1}}
/* IDENT, R1, R2, R3, R1 inverse, R2 inverse, R3 inverse:*/
# define ROT {IDENT, R1, R2, R3, R3, R2, R1}
/* Fuzz to handle rounding errors: */
# define EPS 1e-15

struct pj_opaque {
    int north_square;
    int south_square;
    double qp;
    double *apa;
};

typedef struct {
    int cn;         /* An integer 0--3 indicating the position of the polar cap. */
    double x, y;    /* Coordinates of the pole point (point of most extreme latitude on the polar caps). */
    enum Region {north, south, equatorial} region;
} CapMap;

double rot[7][2][2] = ROT;

/**
 * Returns the sign of the double.
 * @param v the parameter whose sign is returned.
 * @return 1 for positive number, -1 for negative, and 0 for zero.
 **/
double pj_sign (double v) {
    return v > 0 ? 1 : (v < 0 ? -1 : 0);
}


/**
 * Return the index of the matrix in ROT.
 * @param index ranges from -3 to 3.
 */
static int get_rotate_index(int index) {
    switch(index) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 3;
    case -1:
        return 4;
    case -2:
        return 5;
    case -3:
        return 6;
    }
    return 0;
}


/**
 * Return 1 if point (testx, testy) lies in the interior of the polygon
 * determined by the vertices in vert, and return 0 otherwise.
 * See http://paulbourke.net/geometry/polygonmesh/ for more details.
 * @param nvert the number of vertices in the polygon.
 * @param vert the (x, y)-coordinates of the polygon's vertices
 **/
static int pnpoly(int nvert, double vert[][2], double testx, double testy) {
    int i, c = 0;
    int counter = 0;
    double xinters;
    XY p1, p2;

    /* Check for boundrary cases */
    for (i = 0; i < nvert; i++) {
        if (testx == vert[i][0] && testy == vert[i][1]) {
            return 1;
        }
    }

    p1.x = vert[0][0];
    p1.y = vert[0][1];

    for (i = 1; i < nvert; i++) {
        p2.x = vert[i % nvert][0];
        p2.y = vert[i % nvert][1];
        if (testy > MIN(p1.y, p2.y)  &&
            testy <= MAX(p1.y, p2.y) &&
            testx <= MAX(p1.x, p2.x) &&
            p1.y != p2.y)
        {
            xinters = (testy-p1.y)*(p2.x-p1.x)/(p2.y-p1.y)+p1.x;
            if (p1.x == p2.x || testx <= xinters)
                counter++;
        }
        p1 = p2;
    }

    if (counter % 2 == 0) {
        return 0;
    } else {
        return 1;
    }
    return c;
}


/**
 * Return 1 if (x, y) lies in (the interior or boundary of) the image of the
 * HEALPix projection (in case proj=0) or in the image the rHEALPix projection
 * (in case proj=1), and return 0 otherwise.
 * @param north_square the position of the north polar square (rHEALPix only)
 * @param south_square the position of the south polar square (rHEALPix only)
 **/
int in_image(double x, double y, int proj, int north_square, int south_square) {
    if (proj == 0) {
        double healpixVertsJit[][2] = {
            {-M_PI - EPS,  M_FORTPI},
            {-3*M_FORTPI,  M_HALFPI + EPS},
            {-M_HALFPI,    M_FORTPI + EPS},
            {-M_FORTPI,    M_HALFPI + EPS},
            {0.0,          M_FORTPI + EPS},
            {M_FORTPI,     M_HALFPI + EPS},
            {M_HALFPI,     M_FORTPI + EPS},
            {3*M_FORTPI,   M_HALFPI + EPS},
            {M_PI + EPS,   M_FORTPI},
            {M_PI + EPS,  -M_FORTPI},
            {3*M_FORTPI,  -M_HALFPI - EPS},
            {M_HALFPI,    -M_FORTPI - EPS},
            {M_FORTPI,    -M_HALFPI - EPS},
            {0.0,         -M_FORTPI - EPS},
            {-M_FORTPI,   -M_HALFPI - EPS},
            {-M_HALFPI,   -M_FORTPI - EPS},
            {-3*M_FORTPI, -M_HALFPI - EPS},
            {-M_PI - EPS, -M_FORTPI}
        };
        return pnpoly((int)sizeof(healpixVertsJit)/
                      sizeof(healpixVertsJit[0]), healpixVertsJit, x, y);
    } else {
        /**
         * Assigning each element by index to avoid warnings such as
         * 'initializer element is not computable at load time'.
         * Before C99 this was not allowed and to keep as portable as
         * possible we do it the C89 way here.
         * We need to assign the array this way because the input is
         * dynamic (north_square and south_square vars are unknow at
         * compile time).
         **/
        double rhealpixVertsJit[12][2];
        rhealpixVertsJit[0][0]  = -M_PI - EPS;
        rhealpixVertsJit[0][1]  =  M_FORTPI + EPS;
        rhealpixVertsJit[1][0]  = -M_PI + north_square*M_HALFPI- EPS;
        rhealpixVertsJit[1][1]  =  M_FORTPI + EPS;
        rhealpixVertsJit[2][0]  = -M_PI + north_square*M_HALFPI- EPS;
        rhealpixVertsJit[2][1]  =  3*M_FORTPI + EPS;
        rhealpixVertsJit[3][0]  = -M_PI + (north_square + 1.0)*M_HALFPI + EPS;
        rhealpixVertsJit[3][1]  =  3*M_FORTPI + EPS;
        rhealpixVertsJit[4][0]  = -M_PI + (north_square + 1.0)*M_HALFPI + EPS;
        rhealpixVertsJit[4][1]  =  M_FORTPI + EPS;
        rhealpixVertsJit[5][0]  =  M_PI + EPS;
        rhealpixVertsJit[5][1]  =  M_FORTPI + EPS;
        rhealpixVertsJit[6][0]  =  M_PI + EPS;
        rhealpixVertsJit[6][1]  = -M_FORTPI - EPS;
        rhealpixVertsJit[7][0]  = -M_PI + (south_square + 1.0)*M_HALFPI + EPS;
        rhealpixVertsJit[7][1]  = -M_FORTPI - EPS;
        rhealpixVertsJit[8][0]  = -M_PI + (south_square + 1.0)*M_HALFPI + EPS;
        rhealpixVertsJit[8][1]  = -3*M_FORTPI - EPS;
        rhealpixVertsJit[9][0]  = -M_PI + south_square*M_HALFPI - EPS;
        rhealpixVertsJit[9][1]  = -3*M_FORTPI - EPS;
        rhealpixVertsJit[10][0] = -M_PI + south_square*M_HALFPI - EPS;
        rhealpixVertsJit[10][1] = -M_FORTPI - EPS;
        rhealpixVertsJit[11][0] = -M_PI - EPS;
        rhealpixVertsJit[11][1] = -M_FORTPI - EPS;

        return pnpoly((int)sizeof(rhealpixVertsJit)/
                      sizeof(rhealpixVertsJit[0]), rhealpixVertsJit, x, y);
    }
}


/**
 * Return the authalic latitude of latitude alpha (if inverse=0) or
 * return the approximate latitude of authalic latitude alpha (if inverse=1).
 * P contains the relavent ellipsoid parameters.
 **/
double auth_lat(PJ *P, double alpha, int inverse) {
    struct pj_opaque *Q = P->opaque;
    if (inverse == 0) {
        /* Authalic latitude. */
        double q = pj_qsfn(sin(alpha), P->e, 1.0 - P->es);
        double qp = Q->qp;
        double ratio = q/qp;

        if (fabsl(ratio) > 1) {
            /* Rounding error. */
            ratio = pj_sign(ratio);
        }
        return asin(ratio);
    } else {
        /* Approximation to inverse authalic latitude. */
        return pj_authlat(alpha, Q->apa);
    }
}


/**
 * Return the HEALPix projection of the longitude-latitude point lp on
 * the unit sphere.
**/
XY healpix_sphere(LP lp) {
    double lam = lp.lam;
    double phi = lp.phi;
    double phi0 = asin(2.0/3.0);
    XY xy;

    /* equatorial region */
    if ( fabsl(phi) <= phi0) {
        xy.x = lam;
        xy.y = 3*M_PI/8*sin(phi);
    } else {
        double lamc;
        double sigma = sqrt(3*(1 - fabsl(sin(phi))));
        double cn = floor(2*lam / M_PI + 2);
        if (cn >= 4) {
            cn = 3;
        }
        lamc = -3*M_FORTPI + M_HALFPI*cn;
        xy.x = lamc + (lam - lamc)*sigma;
        xy.y = pj_sign(phi)*M_FORTPI*(2 - sigma);
    }
    return xy;
}


/**
 * Return the inverse of healpix_sphere().
**/
LP healpix_sphere_inverse(XY xy) {
    LP lp;
    double x = xy.x;
    double y = xy.y;
    double y0 = M_FORTPI;

    /* Equatorial region. */
    if (fabsl(y) <= y0) {
        lp.lam = x;
        lp.phi = asin(8*y/(3*M_PI));
    } else if (fabsl(y) < M_HALFPI) {
        double cn = floor(2*x/M_PI + 2);
        double xc, tau;
        if (cn >= 4) {
            cn = 3;
        }
        xc = -3*M_FORTPI + M_HALFPI*cn;
        tau = 2.0 - 4*fabsl(y)/M_PI;
        lp.lam = xc + (x - xc)/tau;
        lp.phi = pj_sign(y)*asin(1.0 - pow(tau, 2)/3.0);
    } else {
        lp.lam = -M_PI;
        lp.phi = pj_sign(y)*M_HALFPI;
    }
    return (lp);
}


/**
 * Return the vector sum a + b, where a and b are 2-dimensional vectors.
 * @param ret holds a + b.
 **/
static void vector_add(double a[2], double b[2], double *ret) {
    int i;
    for(i = 0; i < 2; i++) {
        ret[i] = a[i] + b[i];
    }
}


/**
 * Return the vector difference a - b, where a and b are 2-dimensional vectors.
 * @param ret holds a - b.
 **/
static void vector_sub(double a[2], double b[2], double*ret) {
    int i;
    for(i = 0; i < 2; i++) {
        ret[i] = a[i] - b[i];
    }
}


/**
 * Return the 2 x 1 matrix product a*b, where a is a 2 x 2 matrix and
 * b is a 2 x 1 matrix.
 * @param ret holds a*b.
 **/
static void dot_product(double a[2][2], double b[2], double *ret) {
    int i, j;
    int length = 2;
    for(i = 0; i < length; i++) {
        ret[i] = 0;
        for(j = 0; j < length; j++) {
            ret[i] += a[i][j]*b[j];
        }
    }
}


/**
 * Return the number of the polar cap, the pole point coordinates, and
 * the region that (x, y) lies in.
 * If inverse=0, then assume (x,y) lies in the image of the HEALPix
 * projection of the unit sphere.
 * If inverse=1, then assume (x,y) lies in the image of the
 * (north_square, south_square)-rHEALPix projection of the unit sphere.
 **/
static CapMap get_cap(double x, double y, int north_square, int south_square,
                      int inverse) {
    CapMap capmap;
    double c;

    capmap.x = x;
    capmap.y = y;
    if (inverse == 0) {
        if (y > M_FORTPI) {
            capmap.region = north;
            c = M_HALFPI;
        } else if (y < -M_FORTPI) {
            capmap.region = south;
            c = -M_HALFPI;
        } else {
            capmap.region = equatorial;
            capmap.cn = 0;
            return capmap;
        }
        /* polar region */
        if (x < -M_HALFPI) {
            capmap.cn = 0;
            capmap.x = (-3*M_FORTPI);
            capmap.y = c;
        } else if (x >= -M_HALFPI && x < 0) {
            capmap.cn = 1;
            capmap.x = -M_FORTPI;
            capmap.y = c;
        } else if (x >= 0 && x < M_HALFPI) {
            capmap.cn = 2;
            capmap.x = M_FORTPI;
            capmap.y = c;
        } else {
            capmap.cn = 3;
            capmap.x = 3*M_FORTPI;
            capmap.y = c;
        }
    } else {
        if (y > M_FORTPI) {
            capmap.region = north;
            capmap.x = -3*M_FORTPI + north_square*M_HALFPI;
            capmap.y = M_HALFPI;
            x = x - north_square*M_HALFPI;
        } else if (y < -M_FORTPI) {
            capmap.region = south;
            capmap.x = -3*M_FORTPI + south_square*M_HALFPI;
            capmap.y = -M_HALFPI;
            x = x - south_square*M_HALFPI;
        } else {
            capmap.region = equatorial;
            capmap.cn = 0;
            return capmap;
        }
        /* Polar Region, find the HEALPix polar cap number that
           x, y moves to when rHEALPix polar square is disassembled. */
        if (capmap.region == north) {
            if (y >= -x - M_FORTPI - EPS && y < x + 5*M_FORTPI - EPS) {
                capmap.cn = (north_square + 1) % 4;
            } else if (y > -x -M_FORTPI + EPS && y >= x + 5*M_FORTPI - EPS) {
                capmap.cn = (north_square + 2) % 4;
            } else if (y <= -x -M_FORTPI + EPS && y > x + 5*M_FORTPI + EPS) {
                capmap.cn = (north_square + 3) % 4;
            } else {
                capmap.cn = north_square;
            }
        } else if (capmap.region == south) {
            if (y <= x + M_FORTPI + EPS && y > -x - 5*M_FORTPI + EPS) {
                capmap.cn = (south_square + 1) % 4;
            } else if (y < x + M_FORTPI - EPS && y <= -x - 5*M_FORTPI + EPS) {
                capmap.cn = (south_square + 2) % 4;
            } else if (y >= x + M_FORTPI - EPS && y < -x - 5*M_FORTPI - EPS) {
                capmap.cn = (south_square + 3) % 4;
            } else {
                capmap.cn = south_square;
            }
        }
    }
    return capmap;
}


/**
 * Rearrange point (x, y) in the HEALPix projection by
 * combining the polar caps into two polar squares.
 * Put the north polar square in position north_square and
 * the south polar square in position south_square.
 * If inverse=1, then uncombine the polar caps.
 * @param north_square integer between 0 and 3.
 * @param south_square integer between 0 and 3.
 **/
static XY combine_caps(double x, double y, int north_square, int south_square,
                       int inverse) {
    XY xy;
    double v[2];
    double a[2];
    double c[2];
    double vector[2];
    double v_min_c[2];
    double ret_dot[2];
    double (*tmpRot)[2];
    int pole = 0;

    CapMap capmap = get_cap(x, y, north_square, south_square, inverse);
    if (capmap.region == equatorial) {
        xy.x = capmap.x;
        xy.y = capmap.y;
        return xy;
    }

    v[0] = x; v[1] = y;
    c[0] = capmap.x; c[1] = capmap.y;

    if (inverse == 0) {
        /* Rotate (x, y) about its polar cap tip and then translate it to
           north_square or south_square. */
        a[0] =  -3*M_FORTPI + pole*M_HALFPI;
        a[1] =  M_HALFPI;
        if (capmap.region == north) {
            pole = north_square;
            tmpRot = rot[get_rotate_index(capmap.cn - pole)];
        } else {
            pole = south_square;
            tmpRot = rot[get_rotate_index(-1*(capmap.cn - pole))];
        }
    } else {
        /* Inverse function.
         Unrotate (x, y) and then translate it back. */
        a[0] = -3*M_FORTPI + capmap.cn*M_HALFPI;
        a[1] = M_HALFPI;
        /* disassemble */
        if (capmap.region == north) {
            pole = north_square;
            tmpRot = rot[get_rotate_index(-1*(capmap.cn - pole))];
        } else {
            pole = south_square;
            tmpRot = rot[get_rotate_index(capmap.cn - pole)];
        }
    }

    vector_sub(v, c, v_min_c);
    dot_product(tmpRot, v_min_c, ret_dot);
    vector_add(ret_dot, a, vector);

    xy.x = vector[0];
    xy.y = vector[1];
    return xy;
}


static XY s_healpix_forward(LP lp, PJ *P) { /* sphere  */
    (void) P;
    return healpix_sphere(lp);
}


static XY e_healpix_forward(LP lp, PJ *P) { /* ellipsoid  */
    lp.phi = auth_lat(P, lp.phi, 0);
    return healpix_sphere(lp);
}


static LP s_healpix_inverse(XY xy, PJ *P) { /* sphere */
    /* Check whether (x, y) lies in the HEALPix image */
    if (in_image(xy.x, xy.y, 0, 0, 0) == 0) {
        LP lp = {HUGE_VAL, HUGE_VAL};
        pj_ctx_set_errno(P->ctx, -15);
        return lp;
    }
    return healpix_sphere_inverse(xy);
}


static LP e_healpix_inverse(XY xy, PJ *P) { /* ellipsoid */
    LP lp = {0.0,0.0};

    /* Check whether (x, y) lies in the HEALPix image. */
    if (in_image(xy.x, xy.y, 0, 0, 0) == 0) {
        lp.lam = HUGE_VAL;
        lp.phi = HUGE_VAL;
        pj_ctx_set_errno(P->ctx, -15);
        return lp;
    }
    lp = healpix_sphere_inverse(xy);
    lp.phi = auth_lat(P, lp.phi, 1);
    return lp;
}


static XY s_rhealpix_forward(LP lp, PJ *P) { /* sphere */
    struct pj_opaque *Q = P->opaque;

    XY xy = healpix_sphere(lp);
    return combine_caps(xy.x, xy.y, Q->north_square, Q->south_square, 0);
}


static XY e_rhealpix_forward(LP lp, PJ *P) { /* ellipsoid */
    struct pj_opaque *Q = P->opaque;
    XY xy;
    lp.phi = auth_lat(P, lp.phi, 0);
    xy = healpix_sphere(lp);
    return combine_caps(xy.x, xy.y, Q->north_square, Q->south_square, 0);
}


static LP s_rhealpix_inverse(XY xy, PJ *P) { /* sphere */
    struct pj_opaque *Q = P->opaque;

    /* Check whether (x, y) lies in the rHEALPix image. */
    if (in_image(xy.x, xy.y, 1, Q->north_square, Q->south_square) == 0) {
        LP lp = {HUGE_VAL, HUGE_VAL};
        pj_ctx_set_errno(P->ctx, -15);
        return lp;
    }
    xy = combine_caps(xy.x, xy.y, Q->north_square, Q->south_square, 1);
    return healpix_sphere_inverse(xy);
}


static LP e_rhealpix_inverse(XY xy, PJ *P) { /* ellipsoid */
    struct pj_opaque *Q = P->opaque;
    LP lp = {0.0,0.0};

    /* Check whether (x, y) lies in the rHEALPix image. */
    if (in_image(xy.x, xy.y, 1, Q->north_square, Q->south_square) == 0) {
        lp.lam = HUGE_VAL;
        lp.phi = HUGE_VAL;
        pj_ctx_set_errno(P->ctx, -15);
        return lp;
    }
    xy = combine_caps(xy.x, xy.y, Q->north_square, Q->south_square, 1);
    lp = healpix_sphere_inverse(xy);
    lp.phi = auth_lat(P, lp.phi, 1);
    return lp;
}


static void *freeup_new (PJ *P) {                       /* Destructor */
    if (0==P)
        return 0;
    if (0==P->opaque)
        return pj_dealloc (P);

    if (P->opaque->apa)
        pj_dealloc(P->opaque->apa);

    pj_dealloc (P->opaque);
    return pj_dealloc(P);
}

static void freeup (PJ *P) {
    freeup_new (P);
    return;
}


PJ *PROJECTION(healpix) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    if (P->es) {
        Q->apa = pj_authset(P->es);             /* For auth_lat(). */
        Q->qp = pj_qsfn(1.0, P->e, P->one_es);  /* For auth_lat(). */
        P->a = P->a*sqrt(0.5*Q->qp);            /* Set P->a to authalic radius. */
        P->ra = 1.0/P->a;
        P->fwd = e_healpix_forward;
        P->inv = e_healpix_inverse;
    } else {
        P->fwd = s_healpix_forward;
        P->inv = s_healpix_inverse;
    }

    return P;
}


PJ *PROJECTION(rhealpix) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->north_square = pj_param(P->ctx, P->params,"inorth_square").i;
    Q->south_square = pj_param(P->ctx, P->params,"isouth_square").i;

    /* Check for valid north_square and south_square inputs. */
    if (Q->north_square < 0 || Q->north_square > 3) {
        E_ERROR(-47);
    }
    if (Q->south_square < 0 || Q->south_square > 3) {
        E_ERROR(-47);
    }
    if (P->es) {
        Q->apa = pj_authset(P->es); /* For auth_lat(). */
        Q->qp = pj_qsfn(1.0, P->e, P->one_es); /* For auth_lat(). */
        P->a = P->a*sqrt(0.5*Q->qp); /* Set P->a to authalic radius. */
        P->ra = 1.0/P->a;
        P->fwd = e_rhealpix_forward;
        P->inv = e_rhealpix_inverse;
    } else {
        P->fwd = s_rhealpix_forward;
        P->inv = s_rhealpix_inverse;
    }

    return P;
}


#ifndef PJ_SELFTEST
int pj_healpix_selftest (void) {return 0;}
#else

int pj_healpix_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=healpix   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=healpix   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222390.10394923863,  130406.58866448226},
        { 222390.10394923863, -130406.58866448054},
        {-222390.10394923863,  130406.58866448226},
        {-222390.10394923863, -130406.58866448054},
    };

    XY s_fwd_expect[] = {
        { 223402.14425527418,  131588.04444199943},
        { 223402.14425527418, -131588.04444199943},
        {-223402.14425527418,  131588.04444199943},
        {-223402.14425527418, -131588.04444199943},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017986411845524453,  0.00076679453057823619},
        { 0.0017986411845524453, -0.00076679453057823619},
        {-0.0017986411845524453,  0.00076679453057823619},
        {-0.0017986411845524453, -0.00076679453057823619},
    };

    LP s_inv_expect[] = {
        { 0.0017904931097838226,  0.00075990887733981202},
        { 0.0017904931097838226, -0.00075990887733981202},
        {-0.0017904931097838226,  0.00075990887733981202},
        {-0.0017904931097838226, -0.00075990887733981202},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif

#ifndef PJ_SELFTEST
int pj_rhealpix_selftest (void) {return 0;}
#else

int pj_rhealpix_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=rhealpix   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=rhealpix   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 222390.10394923863,  130406.58866448226},
        { 222390.10394923863, -130406.58866448054},
        {-222390.10394923863,  130406.58866448226},
        {-222390.10394923863, -130406.58866448054},
    };

    XY s_fwd_expect[] = {
        { 223402.14425527418,  131588.04444199943},
        { 223402.14425527418, -131588.04444199943},
        {-223402.14425527418,  131588.04444199943},
        {-223402.14425527418, -131588.04444199943},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.0017986411845524453,  0.00076679453057823619},
        { 0.0017986411845524453, -0.00076679453057823619},
        {-0.0017986411845524453,  0.00076679453057823619},
        {-0.0017986411845524453, -0.00076679453057823619},
    };

    LP s_inv_expect[] = {
        { 0.0017904931097838226,  0.00075990887733981202},
        { 0.0017904931097838226, -0.00075990887733981202},
        {-0.0017904931097838226,  0.00075990887733981202},
        {-0.0017904931097838226, -0.00075990887733981202},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
