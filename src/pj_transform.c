/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Perform overall coordinate system to coordinate system
 *           transformations (pj_transform() function) including reprojection
 *           and datum shifting.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#include <projects.h>
#include <string.h>
#include <math.h>
#include "geocent.h"

static int pj_adjust_axis( projCtx ctx, const char *axis, int denormalize_flag,
                           long point_count, int point_offset,
                           double *x, double *y, double *z );

#ifndef SRS_WGS84_SEMIMAJOR
#define SRS_WGS84_SEMIMAJOR 6378137.0
#endif

#ifndef SRS_WGS84_ESQUARED
#define SRS_WGS84_ESQUARED 0.0066943799901413165
#endif

#define Dx_BF (defn->datum_params[0])
#define Dy_BF (defn->datum_params[1])
#define Dz_BF (defn->datum_params[2])
#define Rx_BF (defn->datum_params[3])
#define Ry_BF (defn->datum_params[4])
#define Rz_BF (defn->datum_params[5])
#define M_BF  (defn->datum_params[6])

/*
** This table is intended to indicate for any given error code in
** the range 0 to -44, whether that error will occur for all locations (ie.
** it is a problem with the coordinate system as a whole) in which case the
** value would be 0, or if the problem is with the point being transformed
** in which case the value is 1.
**
** At some point we might want to move this array in with the error message
** list or something, but while experimenting with it this should be fine.
*/

static const int transient_error[50] = {
    /*             0  1  2  3  4  5  6  7  8  9   */
    /* 0 to 9 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 10 to 19 */ 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
    /* 20 to 29 */ 1, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    /* 30 to 39 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 40 to 49 */ 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

/************************************************************************/
/*                            pj_transform()                            */
/*                                                                      */
/*      Currently this function doesn't recognise if two projections    */
/*      are identical (to short circuit reprojection) because it is     */
/*      difficult to compare PJ structures (since there are some        */
/*      projection specific components).                                */
/************************************************************************/

int pj_transform( PJ *srcdefn, PJ *dstdefn, long point_count, int point_offset,
                  double *x, double *y, double *z )

{
    long      i;
    int       err;

    srcdefn->ctx->last_errno = 0;
    dstdefn->ctx->last_errno = 0;

    if( point_offset == 0 )
        point_offset = 1;

/* -------------------------------------------------------------------- */
/*      Transform unusual input coordinate axis orientation to          */
/*      standard form if needed.                                        */
/* -------------------------------------------------------------------- */
    if( strcmp(srcdefn->axis,"enu") != 0 )
    {
        int err;

        err = pj_adjust_axis( srcdefn->ctx, srcdefn->axis,
                              0, point_count, point_offset, x, y, z );
        if( err != 0 )
            return err;
    }

/* -------------------------------------------------------------------- */
/*      Transform Z to meters if it isn't already.                      */
/* -------------------------------------------------------------------- */
    if( srcdefn->vto_meter != 1.0 && z != NULL )
    {
        for( i = 0; i < point_count; i++ )
            z[point_offset*i] *= srcdefn->vto_meter;
    }

/* -------------------------------------------------------------------- */
/*      Transform geocentric source coordinates to lat/long.            */
/* -------------------------------------------------------------------- */
    if( srcdefn->is_geocent )
    {
        if( z == NULL )
        {
            pj_ctx_set_errno( pj_get_ctx(srcdefn), PJD_ERR_GEOCENTRIC);
            return PJD_ERR_GEOCENTRIC;
        }

        if( srcdefn->to_meter != 1.0 )
        {
            for( i = 0; i < point_count; i++ )
            {
                if( x[point_offset*i] != HUGE_VAL )
                {
                    x[point_offset*i] *= srcdefn->to_meter;
                    y[point_offset*i] *= srcdefn->to_meter;
                }
            }
        }

        err = pj_geocentric_to_geodetic( srcdefn->a_orig, srcdefn->es_orig,
                                         point_count, point_offset,
                                         x, y, z );
        if( err != 0 )
            return err;
    }

/* -------------------------------------------------------------------- */
/*      Transform source points to lat/long, if they aren't             */
/*      already.                                                        */
/* -------------------------------------------------------------------- */
    else if( !srcdefn->is_latlong )
    {

        /* Check first if projection is invertible. */
        if( (srcdefn->inv3d == NULL) && (srcdefn->inv == NULL))
        {
            pj_ctx_set_errno( pj_get_ctx(srcdefn), -17 );
            pj_log( pj_get_ctx(srcdefn), PJ_LOG_ERROR,
                    "pj_transform(): source projection not invertable" );
            return -17;
        }

        /* If invertible - First try inv3d if defined */
        if (srcdefn->inv3d != NULL)
        {
            /* Three dimensions must be defined */
            if ( z == NULL)
            {
                pj_ctx_set_errno( pj_get_ctx(srcdefn), PJD_ERR_GEOCENTRIC);
                return PJD_ERR_GEOCENTRIC;
            }

            for (i=0; i < point_count; i++)
            {
                XYZ projected_loc;
                XYZ geodetic_loc;

                projected_loc.u = x[point_offset*i];
                projected_loc.v = y[point_offset*i];
                projected_loc.w = z[point_offset*i];

                if (projected_loc.u == HUGE_VAL)
                    continue;

                geodetic_loc = pj_inv3d(projected_loc, srcdefn);
                if( srcdefn->ctx->last_errno != 0 )
                {
                    if( (srcdefn->ctx->last_errno != 33 /*EDOM*/
                         && srcdefn->ctx->last_errno != 34 /*ERANGE*/ )
                        && (srcdefn->ctx->last_errno > 0
                            || srcdefn->ctx->last_errno < -44 || point_count == 1
                            || transient_error[-srcdefn->ctx->last_errno] == 0 ) )
                        return srcdefn->ctx->last_errno;
                    else
                    {
                        geodetic_loc.u = HUGE_VAL;
                        geodetic_loc.v = HUGE_VAL;
                        geodetic_loc.w = HUGE_VAL;
                    }
                }

                x[point_offset*i] = geodetic_loc.u;
                y[point_offset*i] = geodetic_loc.v;
                z[point_offset*i] = geodetic_loc.w;

            }

        }
        else
        {
            /* Fallback to the original PROJ.4 API 2d inversion - inv */
            for( i = 0; i < point_count; i++ )
            {
                XY         projected_loc;
                LP	       geodetic_loc;

                projected_loc.u = x[point_offset*i];
                projected_loc.v = y[point_offset*i];

                if( projected_loc.u == HUGE_VAL )
                    continue;

                geodetic_loc = pj_inv( projected_loc, srcdefn );
                if( srcdefn->ctx->last_errno != 0 )
                {
                    if( (srcdefn->ctx->last_errno != 33 /*EDOM*/
                         && srcdefn->ctx->last_errno != 34 /*ERANGE*/ )
                        && (srcdefn->ctx->last_errno > 0
                            || srcdefn->ctx->last_errno < -44 || point_count == 1
                            || transient_error[-srcdefn->ctx->last_errno] == 0 ) )
                        return srcdefn->ctx->last_errno;
                    else
                    {
                        geodetic_loc.u = HUGE_VAL;
                        geodetic_loc.v = HUGE_VAL;
                    }
                }

                x[point_offset*i] = geodetic_loc.u;
                y[point_offset*i] = geodetic_loc.v;
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      But if they are already lat long, adjust for the prime          */
/*      meridian if there is one in effect.                             */
/* -------------------------------------------------------------------- */
    if( srcdefn->from_greenwich != 0.0 )
    {
        for( i = 0; i < point_count; i++ )
        {
            if( x[point_offset*i] != HUGE_VAL )
                x[point_offset*i] += srcdefn->from_greenwich;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need to translate from geoid to ellipsoidal vertical      */
/*      datum?                                                          */
/* -------------------------------------------------------------------- */
    if( srcdefn->has_geoid_vgrids && z != NULL )
    {
        if( pj_apply_vgridshift( srcdefn, "sgeoidgrids",
                                 &(srcdefn->vgridlist_geoid),
                                 &(srcdefn->vgridlist_geoid_count),
                                 0, point_count, point_offset, x, y, z ) != 0 )
            return pj_ctx_get_errno(srcdefn->ctx);
    }

/* -------------------------------------------------------------------- */
/*      Convert datums if needed, and possible.                         */
/* -------------------------------------------------------------------- */
    if( pj_datum_transform( srcdefn, dstdefn, point_count, point_offset,
                            x, y, z ) != 0 )
    {
        if( srcdefn->ctx->last_errno != 0 )
            return srcdefn->ctx->last_errno;
        else
            return dstdefn->ctx->last_errno;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to translate from geoid to ellipsoidal vertical      */
/*      datum?                                                          */
/* -------------------------------------------------------------------- */
    if( dstdefn->has_geoid_vgrids && z != NULL )
    {
        if( pj_apply_vgridshift( dstdefn, "sgeoidgrids",
                                 &(dstdefn->vgridlist_geoid),
                                 &(dstdefn->vgridlist_geoid_count),
                                 1, point_count, point_offset, x, y, z ) != 0 )
            return dstdefn->ctx->last_errno;
    }

/* -------------------------------------------------------------------- */
/*      But if they are staying lat long, adjust for the prime          */
/*      meridian if there is one in effect.                             */
/* -------------------------------------------------------------------- */
    if( dstdefn->from_greenwich != 0.0 )
    {
        for( i = 0; i < point_count; i++ )
        {
            if( x[point_offset*i] != HUGE_VAL )
                x[point_offset*i] -= dstdefn->from_greenwich;
        }
    }


/* -------------------------------------------------------------------- */
/*      Transform destination latlong to geocentric if required.        */
/* -------------------------------------------------------------------- */
    if( dstdefn->is_geocent )
    {
        if( z == NULL )
        {
            pj_ctx_set_errno( dstdefn->ctx, PJD_ERR_GEOCENTRIC );
            return PJD_ERR_GEOCENTRIC;
        }

        pj_geodetic_to_geocentric( dstdefn->a_orig, dstdefn->es_orig,
                                   point_count, point_offset, x, y, z );

        if( dstdefn->fr_meter != 1.0 )
        {
            for( i = 0; i < point_count; i++ )
            {
                if( x[point_offset*i] != HUGE_VAL )
                {
                    x[point_offset*i] *= dstdefn->fr_meter;
                    y[point_offset*i] *= dstdefn->fr_meter;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Transform destination points to projection coordinates, if      */
/*      desired.                                                        */
/* -------------------------------------------------------------------- */
    else if( !dstdefn->is_latlong )
    {

        if( dstdefn->fwd3d != NULL)
        {
            for( i = 0; i < point_count; i++ )
            {
                XYZ projected_loc;
                LPZ geodetic_loc;

                geodetic_loc.u = x[point_offset*i];
                geodetic_loc.v = y[point_offset*i];
                geodetic_loc.w = z[point_offset*i];

                if (geodetic_loc.u == HUGE_VAL)
                    continue;

                projected_loc = pj_fwd3d( geodetic_loc, dstdefn);
                if( dstdefn->ctx->last_errno != 0 )
                {
                    if( (dstdefn->ctx->last_errno != 33 /*EDOM*/
                         && dstdefn->ctx->last_errno != 34 /*ERANGE*/ )
                        && (dstdefn->ctx->last_errno > 0
                            || dstdefn->ctx->last_errno < -44 || point_count == 1
                            || transient_error[-dstdefn->ctx->last_errno] == 0 ) )
                        return dstdefn->ctx->last_errno;
                    else
                    {
                        projected_loc.u = HUGE_VAL;
                        projected_loc.v = HUGE_VAL;
                        projected_loc.w = HUGE_VAL;
                    }
                }

                x[point_offset*i] = projected_loc.u;
                y[point_offset*i] = projected_loc.v;
                z[point_offset*i] = projected_loc.w;
            }

        }
        else
        {
            for( i = 0; i < point_count; i++ )
            {
                XY         projected_loc;
                LP	       geodetic_loc;

                geodetic_loc.u = x[point_offset*i];
                geodetic_loc.v = y[point_offset*i];

                if( geodetic_loc.u == HUGE_VAL )
                    continue;

                projected_loc = pj_fwd( geodetic_loc, dstdefn );
                if( dstdefn->ctx->last_errno != 0 )
                {
                    if( (dstdefn->ctx->last_errno != 33 /*EDOM*/
                         && dstdefn->ctx->last_errno != 34 /*ERANGE*/ )
                        && (dstdefn->ctx->last_errno > 0
                            || dstdefn->ctx->last_errno < -44 || point_count == 1
                            || transient_error[-dstdefn->ctx->last_errno] == 0 ) )
                        return dstdefn->ctx->last_errno;
                    else
                    {
                        projected_loc.u = HUGE_VAL;
                        projected_loc.v = HUGE_VAL;
                    }
                }

                x[point_offset*i] = projected_loc.u;
                y[point_offset*i] = projected_loc.v;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If a wrapping center other than 0 is provided, rewrap around    */
/*      the suggested center (for latlong coordinate systems only).     */
/* -------------------------------------------------------------------- */
    else if( dstdefn->is_latlong && dstdefn->is_long_wrap_set )
    {
        for( i = 0; i < point_count; i++ )
        {
            if( x[point_offset*i] == HUGE_VAL )
                continue;

            while( x[point_offset*i] < dstdefn->long_wrap_center - M_PI )
                x[point_offset*i] += M_TWOPI;
            while( x[point_offset*i] > dstdefn->long_wrap_center + M_PI )
                x[point_offset*i] -= M_TWOPI;
        }
    }

/* -------------------------------------------------------------------- */
/*      Transform Z from meters if needed.                              */
/* -------------------------------------------------------------------- */
    if( dstdefn->vto_meter != 1.0 && z != NULL )
    {
        for( i = 0; i < point_count; i++ )
            z[point_offset*i] *= dstdefn->vfr_meter;
    }

/* -------------------------------------------------------------------- */
/*      Transform normalized axes into unusual output coordinate axis   */
/*      orientation if needed.                                          */
/* -------------------------------------------------------------------- */
    if( strcmp(dstdefn->axis,"enu") != 0 )
    {
        int err;

        err = pj_adjust_axis( dstdefn->ctx, dstdefn->axis,
                              1, point_count, point_offset, x, y, z );
        if( err != 0 )
            return err;
    }

    return 0;
}

/************************************************************************/
/*                     pj_geodetic_to_geocentric()                      */
/************************************************************************/

int pj_geodetic_to_geocentric( double a, double es,
                               long point_count, int point_offset,
                               double *x, double *y, double *z )

{
    double b;
    int    i;
    GeocentricInfo gi;
    int ret_errno = 0;

    if( es == 0.0 )
        b = a;
    else
        b = a * sqrt(1-es);

    if( pj_Set_Geocentric_Parameters( &gi, a, b ) != 0 )
    {
        return PJD_ERR_GEOCENTRIC;
    }

    for( i = 0; i < point_count; i++ )
    {
        long io = i * point_offset;

        if( x[io] == HUGE_VAL  )
            continue;

        if( pj_Convert_Geodetic_To_Geocentric( &gi, y[io], x[io], z[io],
                                               x+io, y+io, z+io ) != 0 )
        {
            ret_errno = -14;
            x[io] = y[io] = HUGE_VAL;
            /* but keep processing points! */
        }
    }

    return ret_errno;
}

/************************************************************************/
/*                     pj_geodetic_to_geocentric()                      */
/************************************************************************/

int pj_geocentric_to_geodetic( double a, double es,
                               long point_count, int point_offset,
                               double *x, double *y, double *z )

{
    double b;
    int    i;
    GeocentricInfo gi;

    if( es == 0.0 )
        b = a;
    else
        b = a * sqrt(1-es);

    if( pj_Set_Geocentric_Parameters( &gi, a, b ) != 0 )
    {
        return PJD_ERR_GEOCENTRIC;
    }

    for( i = 0; i < point_count; i++ )
    {
        long io = i * point_offset;

        if( x[io] == HUGE_VAL )
            continue;

        pj_Convert_Geocentric_To_Geodetic( &gi, x[io], y[io], z[io],
                                           y+io, x+io, z+io );
    }

    return 0;
}

/************************************************************************/
/*                         pj_compare_datums()                          */
/*                                                                      */
/*      Returns TRUE if the two datums are identical, otherwise         */
/*      FALSE.                                                          */
/************************************************************************/

int pj_compare_datums( PJ *srcdefn, PJ *dstdefn )

{
    if( srcdefn->datum_type != dstdefn->datum_type )
    {
        return 0;
    }
    else if( srcdefn->a_orig != dstdefn->a_orig
             || ABS(srcdefn->es_orig - dstdefn->es_orig) > 0.000000000050 )
    {
        /* the tolerence for es is to ensure that GRS80 and WGS84 are
           considered identical */
        return 0;
    }
    else if( srcdefn->datum_type == PJD_3PARAM )
    {
        return (srcdefn->datum_params[0] == dstdefn->datum_params[0]
                && srcdefn->datum_params[1] == dstdefn->datum_params[1]
                && srcdefn->datum_params[2] == dstdefn->datum_params[2]);
    }
    else if( srcdefn->datum_type == PJD_7PARAM )
    {
        return (srcdefn->datum_params[0] == dstdefn->datum_params[0]
                && srcdefn->datum_params[1] == dstdefn->datum_params[1]
                && srcdefn->datum_params[2] == dstdefn->datum_params[2]
                && srcdefn->datum_params[3] == dstdefn->datum_params[3]
                && srcdefn->datum_params[4] == dstdefn->datum_params[4]
                && srcdefn->datum_params[5] == dstdefn->datum_params[5]
                && srcdefn->datum_params[6] == dstdefn->datum_params[6]);
    }
    else if( srcdefn->datum_type == PJD_GRIDSHIFT )
    {
        return strcmp( pj_param(srcdefn->ctx, srcdefn->params,"snadgrids").s,
                       pj_param(dstdefn->ctx, dstdefn->params,"snadgrids").s ) == 0;
    }
    else
        return 1;
}

/************************************************************************/
/*                       pj_geocentic_to_wgs84()                        */
/************************************************************************/

int pj_geocentric_to_wgs84( PJ *defn,
                            long point_count, int point_offset,
                            double *x, double *y, double *z )

{
    int       i;

    if( defn->datum_type == PJD_3PARAM )
    {
        for( i = 0; i < point_count; i++ )
        {
            long io = i * point_offset;

            if( x[io] == HUGE_VAL )
                continue;

            x[io] = x[io] + Dx_BF;
            y[io] = y[io] + Dy_BF;
            z[io] = z[io] + Dz_BF;
        }
    }
    else if( defn->datum_type == PJD_7PARAM )
    {
        for( i = 0; i < point_count; i++ )
        {
            long io = i * point_offset;
            double x_out, y_out, z_out;

            if( x[io] == HUGE_VAL )
                continue;

            x_out = M_BF*(       x[io] - Rz_BF*y[io] + Ry_BF*z[io]) + Dx_BF;
            y_out = M_BF*( Rz_BF*x[io] +       y[io] - Rx_BF*z[io]) + Dy_BF;
            z_out = M_BF*(-Ry_BF*x[io] + Rx_BF*y[io] +       z[io]) + Dz_BF;

            x[io] = x_out;
            y[io] = y_out;
            z[io] = z_out;
        }
    }

    return 0;
}

/************************************************************************/
/*                      pj_geocentic_from_wgs84()                       */
/************************************************************************/

int pj_geocentric_from_wgs84( PJ *defn,
                              long point_count, int point_offset,
                              double *x, double *y, double *z )

{
    int       i;

    if( defn->datum_type == PJD_3PARAM )
    {
        for( i = 0; i < point_count; i++ )
        {
            long io = i * point_offset;

            if( x[io] == HUGE_VAL )
                continue;

            x[io] = x[io] - Dx_BF;
            y[io] = y[io] - Dy_BF;
            z[io] = z[io] - Dz_BF;
        }
    }
    else if( defn->datum_type == PJD_7PARAM )
    {
        for( i = 0; i < point_count; i++ )
        {
            long io = i * point_offset;
            double x_tmp, y_tmp, z_tmp;

            if( x[io] == HUGE_VAL )
                continue;

            x_tmp = (x[io] - Dx_BF) / M_BF;
            y_tmp = (y[io] - Dy_BF) / M_BF;
            z_tmp = (z[io] - Dz_BF) / M_BF;

            x[io] =        x_tmp + Rz_BF*y_tmp - Ry_BF*z_tmp;
            y[io] = -Rz_BF*x_tmp +       y_tmp + Rx_BF*z_tmp;
            z[io] =  Ry_BF*x_tmp - Rx_BF*y_tmp +       z_tmp;
        }
    }

    return 0;
}

/************************************************************************/
/*                         pj_datum_transform()                         */
/*                                                                      */
/*      The input should be long/lat/z coordinates in radians in the    */
/*      source datum, and the output should be long/lat/z               */
/*      coordinates in radians in the destination datum.                */
/************************************************************************/

int pj_datum_transform( PJ *srcdefn, PJ *dstdefn,
                        long point_count, int point_offset,
                        double *x, double *y, double *z )

{
    double      src_a, src_es, dst_a, dst_es;
    int         z_is_temp = FALSE;

/* -------------------------------------------------------------------- */
/*      We cannot do any meaningful datum transformation if either      */
/*      the source or destination are of an unknown datum type          */
/*      (ie. only a +ellps declaration, no +datum).  This is new        */
/*      behavior for PROJ 4.6.0.                                        */
/* -------------------------------------------------------------------- */
    if( srcdefn->datum_type == PJD_UNKNOWN
        || dstdefn->datum_type == PJD_UNKNOWN )
        return 0;

/* -------------------------------------------------------------------- */
/*      Short cut if the datums are identical.                          */
/* -------------------------------------------------------------------- */
    if( pj_compare_datums( srcdefn, dstdefn ) )
        return 0;

    src_a = srcdefn->a_orig;
    src_es = srcdefn->es_orig;

    dst_a = dstdefn->a_orig;
    dst_es = dstdefn->es_orig;

/* -------------------------------------------------------------------- */
/*      Create a temporary Z array if one is not provided.              */
/* -------------------------------------------------------------------- */
    if( z == NULL )
    {
        int	bytes = sizeof(double) * point_count * point_offset;
        z = (double *) pj_malloc(bytes);
        memset( z, 0, bytes );
        z_is_temp = TRUE;
    }

#define CHECK_RETURN(defn) {if( defn->ctx->last_errno != 0 && (defn->ctx->last_errno > 0 || transient_error[-defn->ctx->last_errno] == 0) ) { if( z_is_temp ) pj_dalloc(z); return defn->ctx->last_errno; }}

/* -------------------------------------------------------------------- */
/*	If this datum requires grid shifts, then apply it to geodetic   */
/*      coordinates.                                                    */
/* -------------------------------------------------------------------- */
    if( srcdefn->datum_type == PJD_GRIDSHIFT )
    {
        pj_apply_gridshift_2( srcdefn, 0, point_count, point_offset, x, y, z );
        CHECK_RETURN(srcdefn);

        src_a = SRS_WGS84_SEMIMAJOR;
        src_es = SRS_WGS84_ESQUARED;
    }

    if( dstdefn->datum_type == PJD_GRIDSHIFT )
    {
        dst_a = SRS_WGS84_SEMIMAJOR;
        dst_es = SRS_WGS84_ESQUARED;
    }

/* ==================================================================== */
/*      Do we need to go through geocentric coordinates?                */
/* ==================================================================== */
    if( src_es != dst_es || src_a != dst_a
        || srcdefn->datum_type == PJD_3PARAM
        || srcdefn->datum_type == PJD_7PARAM
        || dstdefn->datum_type == PJD_3PARAM
        || dstdefn->datum_type == PJD_7PARAM)
    {
/* -------------------------------------------------------------------- */
/*      Convert to geocentric coordinates.                              */
/* -------------------------------------------------------------------- */
        srcdefn->ctx->last_errno =
            pj_geodetic_to_geocentric( src_a, src_es,
                                       point_count, point_offset, x, y, z );
        CHECK_RETURN(srcdefn);

/* -------------------------------------------------------------------- */
/*      Convert between datums.                                         */
/* -------------------------------------------------------------------- */
        if( srcdefn->datum_type == PJD_3PARAM
            || srcdefn->datum_type == PJD_7PARAM )
        {
            pj_geocentric_to_wgs84( srcdefn, point_count, point_offset,x,y,z);
            CHECK_RETURN(srcdefn);
        }

        if( dstdefn->datum_type == PJD_3PARAM
            || dstdefn->datum_type == PJD_7PARAM )
        {
            pj_geocentric_from_wgs84( dstdefn, point_count,point_offset,x,y,z);
            CHECK_RETURN(dstdefn);
        }

/* -------------------------------------------------------------------- */
/*      Convert back to geodetic coordinates.                           */
/* -------------------------------------------------------------------- */
        dstdefn->ctx->last_errno =
            pj_geocentric_to_geodetic( dst_a, dst_es,
                                       point_count, point_offset, x, y, z );
        CHECK_RETURN(dstdefn);
    }

/* -------------------------------------------------------------------- */
/*      Apply grid shift to destination if required.                    */
/* -------------------------------------------------------------------- */
    if( dstdefn->datum_type == PJD_GRIDSHIFT )
    {
        pj_apply_gridshift_2( dstdefn, 1, point_count, point_offset, x, y, z );
        CHECK_RETURN(dstdefn);
    }

    if( z_is_temp )
        pj_dalloc( z );

    return 0;
}

/************************************************************************/
/*                           pj_adjust_axis()                           */
/*                                                                      */
/*      Normalize or de-normalized the x/y/z axes.  The normal form     */
/*      is "enu" (easting, northing, up).                               */
/************************************************************************/
static int pj_adjust_axis( projCtx ctx,
                           const char *axis, int denormalize_flag,
                           long point_count, int point_offset,
                           double *x, double *y, double *z )

{
    double x_in, y_in, z_in = 0.0;
    int i, i_axis;

    if( !denormalize_flag )
    {
        for( i = 0; i < point_count; i++ )
        {
            x_in = x[point_offset*i];
            y_in = y[point_offset*i];
            if( z )
                z_in = z[point_offset*i];

            for( i_axis = 0; i_axis < 3; i_axis++ )
            {
                double value;

                if( i_axis == 0 )
                    value = x_in;
                else if( i_axis == 1 )
                    value = y_in;
                else
                    value = z_in;

                switch( axis[i_axis] )
                {
                  case 'e':
                    x[point_offset*i] = value; break;
                  case 'w':
                    x[point_offset*i] = -value; break;
                  case 'n':
                    y[point_offset*i] = value; break;
                  case 's':
                    y[point_offset*i] = -value; break;
                  case 'u':
                    if( z ) z[point_offset*i] = value; break;
                  case 'd':
                    if( z ) z[point_offset*i] = -value; break;
                  default:
                    pj_ctx_set_errno( ctx, PJD_ERR_AXIS );
                    return PJD_ERR_AXIS;
                }
            } /* i_axis */
        } /* i (point) */
    }

    else /* denormalize */
    {
        for( i = 0; i < point_count; i++ )
        {
            x_in = x[point_offset*i];
            y_in = y[point_offset*i];
            if( z )
                z_in = z[point_offset*i];

            for( i_axis = 0; i_axis < 3; i_axis++ )
            {
                double *target;

                if( i_axis == 2 && z == NULL )
                    continue;

                if( i_axis == 0 )
                    target = x;
                else if( i_axis == 1 )
                    target = y;
                else
                    target = z;

                switch( axis[i_axis] )
                {
                  case 'e':
                    target[point_offset*i] = x_in; break;
                  case 'w':
                    target[point_offset*i] = -x_in; break;
                  case 'n':
                    target[point_offset*i] = y_in; break;
                  case 's':
                    target[point_offset*i] = -y_in; break;
                  case 'u':
                    target[point_offset*i] = z_in; break;
                  case 'd':
                    target[point_offset*i] = -z_in; break;
                  default:
                    pj_ctx_set_errno( ctx, PJD_ERR_AXIS );
                    return PJD_ERR_AXIS;
                }
            } /* i_axis */
        } /* i (point) */
    }

    return 0;
}
