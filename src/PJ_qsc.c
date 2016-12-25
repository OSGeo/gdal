/*
 * This implements the Quadrilateralized Spherical Cube (QSC) projection.
 *
 * Copyright (c) 2011, 2012  Martin Lambers <marlam@marlam.de>
 *
 * The QSC projection was introduced in:
 * [OL76]
 * E.M. O'Neill and R.E. Laubscher, "Extended Studies of a Quadrilateralized
 * Spherical Cube Earth Data Base", Naval Environmental Prediction Research
 * Facility Tech. Report NEPRF 3-76 (CSC), May 1976.
 *
 * The preceding shift from an ellipsoid to a sphere, which allows to apply
 * this projection to ellipsoids as used in the Ellipsoidal Cube Map model,
 * is described in
 * [LK12]
 * M. Lambers and A. Kolb, "Ellipsoidal Cube Maps for Accurate Rendering of
 * Planetary-Scale Terrain Data", Proc. Pacfic Graphics (Short Papers), Sep.
 * 2012
 *
 * You have to choose one of the following projection centers,
 * corresponding to the centers of the six cube faces:
 * phi0 = 0.0, lam0 = 0.0       ("front" face)
 * phi0 = 0.0, lam0 = 90.0      ("right" face)
 * phi0 = 0.0, lam0 = 180.0     ("back" face)
 * phi0 = 0.0, lam0 = -90.0     ("left" face)
 * phi0 = 90.0                  ("top" face)
 * phi0 = -90.0                 ("bottom" face)
 * Other projection centers will not work!
 *
 * In the projection code below, each cube face is handled differently.
 * See the computation of the face parameter in the PROJECTION(qsc) function
 * and the handling of different face values (FACE_*) in the forward and
 * inverse projections.
 *
 * Furthermore, the projection is originally only defined for theta angles
 * between (-1/4 * PI) and (+1/4 * PI) on the current cube face. This area
 * of definition is named AREA_0 in the projection code below. The other
 * three areas of a cube face are handled by rotation of AREA_0.
 */

#define PJ_LIB__
#include <projects.h>

struct pj_opaque {
        int face;
        double a_squared;
        double b;
        double one_minus_f;
        double one_minus_f_squared;
};
PROJ_HEAD(qsc, "Quadrilateralized Spherical Cube") "\n\tAzi, Sph.";

#define EPS10 1.e-10

/* The six cube faces. */
#define FACE_FRONT  0
#define FACE_RIGHT  1
#define FACE_BACK   2
#define FACE_LEFT   3
#define FACE_TOP    4
#define FACE_BOTTOM 5

/* The four areas on a cube face. AREA_0 is the area of definition,
 * the other three areas are counted counterclockwise. */
#define AREA_0 0
#define AREA_1 1
#define AREA_2 2
#define AREA_3 3

/* Helper function for forward projection: compute the theta angle
 * and determine the area number. */
static double qsc_fwd_equat_face_theta(double phi, double y, double x, int *area) {
    double theta;
    if (phi < EPS10) {
        *area = AREA_0;
        theta = 0.0;
    } else {
        theta = atan2(y, x);
        if (fabs(theta) <= M_FORTPI) {
            *area = AREA_0;
        } else if (theta > M_FORTPI && theta <= M_HALFPI + M_FORTPI) {
            *area = AREA_1;
            theta -= M_HALFPI;
        } else if (theta > M_HALFPI + M_FORTPI || theta <= -(M_HALFPI + M_FORTPI)) {
            *area = AREA_2;
            theta = (theta >= 0.0 ? theta - M_PI : theta + M_PI);
        } else {
            *area = AREA_3;
            theta += M_HALFPI;
        }
    }
    return theta;
}

/* Helper function: shift the longitude. */
static double qsc_shift_lon_origin(double lon, double offset) {
    double slon = lon + offset;
    if (slon < -M_PI) {
        slon += M_TWOPI;
    } else if (slon > +M_PI) {
        slon -= M_TWOPI;
    }
    return slon;
}


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double lat, lon;
    double theta, phi;
    double t, mu; /* nu; */
    int area;

    /* Convert the geodetic latitude to a geocentric latitude.
     * This corresponds to the shift from the ellipsoid to the sphere
     * described in [LK12]. */
    if (P->es) {
        lat = atan(Q->one_minus_f_squared * tan(lp.phi));
    } else {
        lat = lp.phi;
    }

    /* Convert the input lat, lon into theta, phi as used by QSC.
     * This depends on the cube face and the area on it.
     * For the top and bottom face, we can compute theta and phi
     * directly from phi, lam. For the other faces, we must use
     * unit sphere cartesian coordinates as an intermediate step. */
    lon = lp.lam;
    if (Q->face == FACE_TOP) {
        phi = M_HALFPI - lat;
        if (lon >= M_FORTPI && lon <= M_HALFPI + M_FORTPI) {
            area = AREA_0;
            theta = lon - M_HALFPI;
        } else if (lon > M_HALFPI + M_FORTPI || lon <= -(M_HALFPI + M_FORTPI)) {
            area = AREA_1;
            theta = (lon > 0.0 ? lon - M_PI : lon + M_PI);
        } else if (lon > -(M_HALFPI + M_FORTPI) && lon <= -M_FORTPI) {
            area = AREA_2;
            theta = lon + M_HALFPI;
        } else {
            area = AREA_3;
            theta = lon;
        }
    } else if (Q->face == FACE_BOTTOM) {
        phi = M_HALFPI + lat;
        if (lon >= M_FORTPI && lon <= M_HALFPI + M_FORTPI) {
            area = AREA_0;
            theta = -lon + M_HALFPI;
        } else if (lon < M_FORTPI && lon >= -M_FORTPI) {
            area = AREA_1;
            theta = -lon;
        } else if (lon < -M_FORTPI && lon >= -(M_HALFPI + M_FORTPI)) {
            area = AREA_2;
            theta = -lon - M_HALFPI;
        } else {
            area = AREA_3;
            theta = (lon > 0.0 ? -lon + M_PI : -lon - M_PI);
        }
    } else {
        double q, r, s;
        double sinlat, coslat;
        double sinlon, coslon;

        if (Q->face == FACE_RIGHT) {
            lon = qsc_shift_lon_origin(lon, +M_HALFPI);
        } else if (Q->face == FACE_BACK) {
            lon = qsc_shift_lon_origin(lon, +M_PI);
        } else if (Q->face == FACE_LEFT) {
            lon = qsc_shift_lon_origin(lon, -M_HALFPI);
        }
        sinlat = sin(lat);
        coslat = cos(lat);
        sinlon = sin(lon);
        coslon = cos(lon);
        q = coslat * coslon;
        r = coslat * sinlon;
        s = sinlat;

        if (Q->face == FACE_FRONT) {
            phi = acos(q);
            theta = qsc_fwd_equat_face_theta(phi, s, r, &area);
        } else if (Q->face == FACE_RIGHT) {
            phi = acos(r);
            theta = qsc_fwd_equat_face_theta(phi, s, -q, &area);
        } else if (Q->face == FACE_BACK) {
            phi = acos(-q);
            theta = qsc_fwd_equat_face_theta(phi, s, -r, &area);
        } else if (Q->face == FACE_LEFT) {
            phi = acos(-r);
            theta = qsc_fwd_equat_face_theta(phi, s, q, &area);
        } else {
            /* Impossible */
            phi = theta = 0.0;
            area = AREA_0;
        }
    }

    /* Compute mu and nu for the area of definition.
     * For mu, see Eq. (3-21) in [OL76], but note the typos:
     * compare with Eq. (3-14). For nu, see Eq. (3-38). */
    mu = atan((12.0 / M_PI) * (theta + acos(sin(theta) * cos(M_FORTPI)) - M_HALFPI));
    t = sqrt((1.0 - cos(phi)) / (cos(mu) * cos(mu)) / (1.0 - cos(atan(1.0 / cos(theta)))));
    /* nu = atan(t);        We don't really need nu, just t, see below. */

    /* Apply the result to the real area. */
    if (area == AREA_1) {
        mu += M_HALFPI;
    } else if (area == AREA_2) {
        mu += M_PI;
    } else if (area == AREA_3) {
        mu += M_PI_HALFPI;
    }

    /* Now compute x, y from mu and nu */
    /* t = tan(nu); */
    xy.x = t * cos(mu);
    xy.y = t * sin(mu);
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    struct pj_opaque *Q = P->opaque;
    double mu, nu, cosmu, tannu;
    double tantheta, theta, cosphi, phi;
    double t;
    int area;

    /* Convert the input x, y to the mu and nu angles as used by QSC.
     * This depends on the area of the cube face. */
    nu = atan(sqrt(xy.x * xy.x + xy.y * xy.y));
    mu = atan2(xy.y, xy.x);
    if (xy.x >= 0.0 && xy.x >= fabs(xy.y)) {
        area = AREA_0;
    } else if (xy.y >= 0.0 && xy.y >= fabs(xy.x)) {
        area = AREA_1;
        mu -= M_HALFPI;
    } else if (xy.x < 0.0 && -xy.x >= fabs(xy.y)) {
        area = AREA_2;
        mu = (mu < 0.0 ? mu + M_PI : mu - M_PI);
    } else {
        area = AREA_3;
        mu += M_HALFPI;
    }

    /* Compute phi and theta for the area of definition.
     * The inverse projection is not described in the original paper, but some
     * good hints can be found here (as of 2011-12-14):
     * http://fits.gsfc.nasa.gov/fitsbits/saf.93/saf.9302
     * (search for "Message-Id: <9302181759.AA25477 at fits.cv.nrao.edu>") */
    t = (M_PI / 12.0) * tan(mu);
    tantheta = sin(t) / (cos(t) - (1.0 / sqrt(2.0)));
    theta = atan(tantheta);
    cosmu = cos(mu);
    tannu = tan(nu);
    cosphi = 1.0 - cosmu * cosmu * tannu * tannu * (1.0 - cos(atan(1.0 / cos(theta))));
    if (cosphi < -1.0) {
        cosphi = -1.0;
    } else if (cosphi > +1.0) {
        cosphi = +1.0;
    }

    /* Apply the result to the real area on the cube face.
     * For the top and bottom face, we can compute phi and lam directly.
     * For the other faces, we must use unit sphere cartesian coordinates
     * as an intermediate step. */
    if (Q->face == FACE_TOP) {
        phi = acos(cosphi);
        lp.phi = M_HALFPI - phi;
        if (area == AREA_0) {
            lp.lam = theta + M_HALFPI;
        } else if (area == AREA_1) {
            lp.lam = (theta < 0.0 ? theta + M_PI : theta - M_PI);
        } else if (area == AREA_2) {
            lp.lam = theta - M_HALFPI;
        } else /* area == AREA_3 */ {
            lp.lam = theta;
        }
    } else if (Q->face == FACE_BOTTOM) {
        phi = acos(cosphi);
        lp.phi = phi - M_HALFPI;
        if (area == AREA_0) {
            lp.lam = -theta + M_HALFPI;
        } else if (area == AREA_1) {
            lp.lam = -theta;
        } else if (area == AREA_2) {
            lp.lam = -theta - M_HALFPI;
        } else /* area == AREA_3 */ {
            lp.lam = (theta < 0.0 ? -theta - M_PI : -theta + M_PI);
        }
    } else {
        /* Compute phi and lam via cartesian unit sphere coordinates. */
        double q, r, s, t;
        q = cosphi;
        t = q * q;
        if (t >= 1.0) {
            s = 0.0;
        } else {
            s = sqrt(1.0 - t) * sin(theta);
        }
        t += s * s;
        if (t >= 1.0) {
            r = 0.0;
        } else {
            r = sqrt(1.0 - t);
        }
        /* Rotate q,r,s into the correct area. */
        if (area == AREA_1) {
            t = r;
            r = -s;
            s = t;
        } else if (area == AREA_2) {
            r = -r;
            s = -s;
        } else if (area == AREA_3) {
            t = r;
            r = s;
            s = -t;
        }
        /* Rotate q,r,s into the correct cube face. */
        if (Q->face == FACE_RIGHT) {
            t = q;
            q = -r;
            r = t;
        } else if (Q->face == FACE_BACK) {
            q = -q;
            r = -r;
        } else if (Q->face == FACE_LEFT) {
            t = q;
            q = r;
            r = -t;
        }
        /* Now compute phi and lam from the unit sphere coordinates. */
        lp.phi = acos(-s) - M_HALFPI;
        lp.lam = atan2(r, q);
        if (Q->face == FACE_RIGHT) {
            lp.lam = qsc_shift_lon_origin(lp.lam, -M_HALFPI);
        } else if (Q->face == FACE_BACK) {
            lp.lam = qsc_shift_lon_origin(lp.lam, -M_PI);
        } else if (Q->face == FACE_LEFT) {
            lp.lam = qsc_shift_lon_origin(lp.lam, +M_HALFPI);
        }
    }

    /* Apply the shift from the sphere to the ellipsoid as described
     * in [LK12]. */
    if (P->es) {
        int invert_sign;
        double tanphi, xa;
        invert_sign = (lp.phi < 0.0 ? 1 : 0);
        tanphi = tan(lp.phi);
        xa = Q->b / sqrt(tanphi * tanphi + Q->one_minus_f_squared);
        lp.phi = atan(sqrt(P->a * P->a - xa * xa) / (Q->one_minus_f * xa));
        if (invert_sign) {
            lp.phi = -lp.phi;
        }
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


PJ *PROJECTION(qsc) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    P->inv = e_inverse;
    P->fwd = e_forward;
    /* Determine the cube face from the center of projection. */
    if (P->phi0 >= M_HALFPI - M_FORTPI / 2.0) {
        Q->face = FACE_TOP;
    } else if (P->phi0 <= -(M_HALFPI - M_FORTPI / 2.0)) {
        Q->face = FACE_BOTTOM;
    } else if (fabs(P->lam0) <= M_FORTPI) {
        Q->face = FACE_FRONT;
    } else if (fabs(P->lam0) <= M_HALFPI + M_FORTPI) {
        Q->face = (P->lam0 > 0.0 ? FACE_RIGHT : FACE_LEFT);
    } else {
        Q->face = FACE_BACK;
    }
    /* Fill in useful values for the ellipsoid <-> sphere shift
     * described in [LK12]. */
    if (P->es) {
        Q->a_squared = P->a * P->a;
        Q->b = P->a * sqrt(1.0 - P->es);
        Q->one_minus_f = 1.0 - (P->a - Q->b) / P->a;
        Q->one_minus_f_squared = Q->one_minus_f * Q->one_minus_f;
    }

    return P;
}


#ifndef PJ_SELFTEST
int pj_qsc_selftest (void) {return 0;}
#else

int pj_qsc_selftest (void) {
    double tolerance_lp = 1e-10;
    double tolerance_xy = 1e-7;

    char e_args[] = {"+proj=qsc   +ellps=GRS80  +lat_1=0.5 +lat_2=2"};
    char s_args[] = {"+proj=qsc   +a=6400000    +lat_1=0.5 +lat_2=2"};

    LP fwd_in[] = {
        { 2, 1},
        { 2,-1},
        {-2, 1},
        {-2,-1}
    };

    XY e_fwd_expect[] = {
        { 304638.450843852363,  164123.870923793991},
        { 304638.450843852363, -164123.870923793991},
        {-304638.450843852363,  164123.870923793962},
        {-304638.450843852421, -164123.870923793904},
    };

    XY s_fwd_expect[] = {
        { 305863.792402890511,  165827.722754715243},
        { 305863.792402890511, -165827.722754715243},
        {-305863.792402890511,  165827.722754715243},
        {-305863.792402890569, -165827.722754715156},
    };

    XY inv_in[] = {
        { 200, 100},
        { 200,-100},
        {-200, 100},
        {-200,-100}
    };

    LP e_inv_expect[] = {
        { 0.00132134098471627126,  0.000610652900922527926},
        { 0.00132134098471627126, -0.000610652900922527926},
        {-0.00132134098471627126,  0.000610652900922527926},
        {-0.00132134098471627126, -0.000610652900922527926},
    };

    LP s_inv_expect[] = {
        { 0.00131682718763827234,  0.000604493198178676161},
        { 0.00131682718763827234, -0.000604493198178676161},
        {-0.00131682718763827234,  0.000604493198178676161},
        {-0.00131682718763827234, -0.000604493198178676161},
    };

    return pj_generic_selftest (e_args, s_args, tolerance_xy, tolerance_lp, 4, 4, fwd_in, e_fwd_expect, s_fwd_expect, inv_in, e_inv_expect, s_inv_expect);
}


#endif
