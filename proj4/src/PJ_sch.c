/******************************************************************************
 * $Id$
 *
 * Project:  SCH Coordinate system
 * Purpose:  Implementation of SCH Coordinate system
 * References :
 *      1. Hensley. Scott. SCH Coordinates and various transformations. June 15, 2000.
 *      2. Buckley, Sean Monroe. Radar interferometry measurement of land subsidence. 2000..
 *         PhD Thesis. UT Austin. (Appendix)
 *      3. Hensley, Scott, Elaine Chapin, and T. Michel. "Improved processing of AIRSAR
 *         data based on the GeoSAR processor." Airsar earth science and applications
 *         workshop, March. 2002. (http://airsar.jpl.nasa.gov/documents/workshop2002/papers/T3.pdf)
 *
 * Author:   Piyush Agram (piyush.agram@jpl.nasa.gov)
 * Copyright (c) 2015 California Institute of Technology.
 * Government sponsorship acknowledged.
 *
 * NOTE:  The SCH coordinate system is a sensor aligned coordinate system
 * developed at JPL for radar mapping missions. Details pertaining to the
 * coordinate system have been release in the public domain (see references above).
 * This code is an independent implementation of the SCH coordinate system
 * that conforms to the PROJ.4 conventions and uses the details presented in these
 * publicly released documents. All credit for the development of the coordinate
 * system and its use should be directed towards the original developers at JPL.
 ******************************************************************************
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#define PJ_LIB__
#include <projects.h>
#include "geocent.h"

struct pj_opaque {
    double plat; /*Peg Latitude */
    double plon; /*Peg Longitude*/
    double phdg; /*Peg heading  */
    double h0;   /*Average altitude */
    double transMat[9];
    double xyzoff[3];
    double rcurv;
    GeocentricInfo sph;
    GeocentricInfo elp_0;
};

PROJ_HEAD(sch, "Spherical Cross-track Height") "\n\tMisc\n\tplat_0 = ,plon_0 = , phdg_0 = ,[h_0 = ]";

static LPZ inverse3d(XYZ xyz, PJ *P) {
    LPZ lpz = {0.0, 0.0, 0.0};
    struct pj_opaque *Q = P->opaque;
    double temp[3];
    double pxyz[3];

    /* Local lat,lon using radius */
    pxyz[0] = xyz.y * P->a / Q->rcurv;
    pxyz[1] = xyz.x * P->a / Q->rcurv;
    pxyz[2] = xyz.z;

    if( pj_Convert_Geodetic_To_Geocentric( &(Q->sph), pxyz[0], pxyz[1], pxyz[2],
                temp, temp+1, temp+2) != 0)
            I3_ERROR;

    /* Apply rotation */
    pxyz[0] = Q->transMat[0] * temp[0] + Q->transMat[1] * temp[1] + Q->transMat[2] * temp[2];
    pxyz[1] = Q->transMat[3] * temp[0] + Q->transMat[4] * temp[1] + Q->transMat[5] * temp[2];
    pxyz[2] = Q->transMat[6] * temp[0] + Q->transMat[7] * temp[1] + Q->transMat[8] * temp[2];

    /* Apply offset */
    pxyz[0] += Q->xyzoff[0];
    pxyz[1] += Q->xyzoff[1];
    pxyz[2] += Q->xyzoff[2];

    /* Convert geocentric coordinates to lat lon */
    pj_Convert_Geocentric_To_Geodetic( &(Q->elp_0), pxyz[0], pxyz[1], pxyz[2],
            temp, temp+1, temp+2);


    lpz.lam = temp[1] ;
    lpz.phi = temp[0] ;
    lpz.z = temp[2];

#if 0
    printf("INVERSE: \n");
    printf("XYZ: %f %f %f \n", xyz.x, xyz.y, xyz.z);
    printf("LPZ: %f %f %f \n", lpz.lam, lpz.phi, lpz.z);
#endif
    return lpz;
}

static XYZ forward3d(LPZ lpz, PJ *P) {
    XYZ xyz = {0.0, 0.0, 0.0};
    struct pj_opaque *Q = P->opaque;
    double temp[3];
    double pxyz[3];


    /* Convert lat lon to geocentric coordinates */
    if( pj_Convert_Geodetic_To_Geocentric( &(Q->elp_0), lpz.phi, lpz.lam, lpz.z,
                temp, temp+1, temp+2 ) != 0 )
        F3_ERROR;


    /* Adjust for offset */
    temp[0] -= Q->xyzoff[0];
    temp[1] -= Q->xyzoff[1];
    temp[2] -= Q->xyzoff[2];


    /* Apply rotation */
    pxyz[0] = Q->transMat[0] * temp[0] + Q->transMat[3] * temp[1] + Q->transMat[6] * temp[2];
    pxyz[1] = Q->transMat[1] * temp[0] + Q->transMat[4] * temp[1] + Q->transMat[7] * temp[2];
    pxyz[2] = Q->transMat[2] * temp[0] + Q->transMat[5] * temp[1] + Q->transMat[8] * temp[2];

    /* Convert to local lat,lon */
    pj_Convert_Geocentric_To_Geodetic( &(Q->sph), pxyz[0], pxyz[1], pxyz[2],
            temp, temp+1, temp+2);


    /* Scale by radius */
    xyz.x = temp[1] * Q->rcurv / P->a;
    xyz.y = temp[0] * Q->rcurv / P->a;
    xyz.z = temp[2];

#if 0
    printf("FORWARD: \n");
    printf("LPZ: %f %f %f \n", lpz.lam, lpz.phi, lpz.z);
    printf("XYZ: %f %f %f \n", xyz.x, xyz.y, xyz.z);
#endif
    return xyz;
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

static PJ *setup(PJ *P) { /* general initialization */
    struct pj_opaque *Q = P->opaque;
    double reast, rnorth;
    double chdg, shdg;
    double clt, slt;
    double clo, slo;
    double temp;
    double pxyz[3];

    temp = P->a * sqrt(1.0 - P->es);

    /* Setup original geocentric system */
    if ( pj_Set_Geocentric_Parameters(&(Q->elp_0), P->a, temp) != 0)
            E_ERROR(-37);

    clt = cos(Q->plat);
    slt = sin(Q->plat);
    clo = cos(Q->plon);
    slo = sin(Q->plon);

    /* Estimate the radius of curvature for given peg */
    temp = sqrt(1.0 - (P->es) * slt * slt);
    reast = (P->a)/temp;
    rnorth = (P->a) * (1.0 - (P->es))/pow(temp,3);

    chdg = cos(Q->phdg);
    shdg = sin(Q->phdg);

    Q->rcurv = Q->h0 + (reast*rnorth)/(reast * chdg * chdg + rnorth * shdg * shdg);

#if 0
    printf("North Radius: %f \n", rnorth);
    printf("East Radius: %f \n", reast);
    printf("Effective Radius: %f \n", Q->rcurv);
#endif

    /* Set up local sphere at the given peg point */
    if ( pj_Set_Geocentric_Parameters(&(Q->sph), Q->rcurv, Q->rcurv) != 0)
        E_ERROR(-37);

    /* Set up the transformation matrices */
    Q->transMat[0] = clt * clo;
    Q->transMat[1] = -shdg*slo - slt*clo * chdg;
    Q->transMat[2] =  slo*chdg - slt*clo*shdg;
    Q->transMat[3] =  clt*slo;
    Q->transMat[4] =  clo*shdg - slt*slo*chdg;
    Q->transMat[5] = -clo*chdg - slt*slo*shdg;
    Q->transMat[6] =  slt;
    Q->transMat[7] =  clt*chdg;
    Q->transMat[8] =  clt*shdg;


    if( pj_Convert_Geodetic_To_Geocentric( &(Q->elp_0), Q->plat, Q->plon, Q->h0,
                                           pxyz, pxyz+1, pxyz+2 ) != 0 )
    {
        E_ERROR(-14)
    }


    Q->xyzoff[0] = pxyz[0] - (Q->rcurv) * clt * clo;
    Q->xyzoff[1] = pxyz[1] - (Q->rcurv) * clt * slo;
    Q->xyzoff[2] = pxyz[2] - (Q->rcurv) * slt;

#if 0
    printf("Offset: %f %f %f \n", Q->xyzoff[0], Q->xyzoff[1], Q->xyzoff[2]);
#endif

    P->fwd3d = forward3d;
    P->inv3d = inverse3d;
    return P;
}


PJ *PROJECTION(sch) {
    struct pj_opaque *Q = pj_calloc (1, sizeof (struct pj_opaque));
    if (0==Q)
        return freeup_new (P);
    P->opaque = Q;

    Q->h0 = 0.0;

    /* Check if peg latitude was defined */
    if (pj_param(P->ctx, P->params, "tplat_0").i)
        Q->plat = pj_param(P->ctx, P->params, "rplat_0").f;
    else
        E_ERROR(-37);

    /* Check if peg longitude was defined */
    if (pj_param(P->ctx, P->params, "tplon_0").i)
        Q->plon = pj_param(P->ctx, P->params, "rplon_0").f;
    else
        E_ERROR(-37);

    /* Check if peg latitude is defined */
    if (pj_param(P->ctx, P->params, "tphdg_0").i)
        Q->phdg = pj_param(P->ctx, P->params, "rphdg_0").f;
    else
        E_ERROR(-37);


    /* Check if average height was defined - If so read it in */
    if (pj_param(P->ctx, P->params, "th_0").i)
        Q->h0 = pj_param(P->ctx, P->params, "dh_0").f;

    /* Completed reading in the projection parameters */
#if 0
    printf("PSA: Lat = %f Lon = %f Hdg = %f \n", Q->plat, Q->plon, Q->phdg);
#endif

    return setup(P);
}

/* Skipping sef-test since the test system is not capable of handling
 * 3D coordinate systems for the time being. Relying on tests in ../nad/
 */
int pj_sch_selftest (void) {return 0;}
