/******************************************************************************
 * $Id: geo_trans.c 2678 2015-10-28 19:06:57Z rouault $
 *
 * Project:  libgeotiff
 * Purpose:  Code to abstract translation between pixel/line and PCS
 *           coordinates.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "geotiff.h"
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */
#include "geokeys.h"

/************************************************************************/
/*                          inv_geotransform()                          */
/*                                                                      */
/*      Invert a 6 term geotransform style matrix.                      */
/************************************************************************/

static int inv_geotransform( double *gt_in, double *gt_out )

{
    double	det, inv_det;

    /* we assume a 3rd row that is [0 0 1] */

    /* Compute determinate */

    det = gt_in[0] * gt_in[4] - gt_in[1] * gt_in[3];

    if( fabs(det) < 0.000000000000001 )
        return 0;

    inv_det = 1.0 / det;

    /* compute adjoint, and divide by determinate */

    gt_out[0] =  gt_in[4] * inv_det;
    gt_out[3] = -gt_in[3] * inv_det;

    gt_out[1] = -gt_in[1] * inv_det;
    gt_out[4] =  gt_in[0] * inv_det;

    gt_out[2] = ( gt_in[1] * gt_in[5] - gt_in[2] * gt_in[4]) * inv_det;
    gt_out[5] = (-gt_in[0] * gt_in[5] + gt_in[2] * gt_in[3]) * inv_det;

    return 1;
}

/************************************************************************/
/*                       GTIFTiepointTranslate()                        */
/************************************************************************/

static
int GTIFTiepointTranslate( int gcp_count, double * gcps_in, double * gcps_out,
                           double x_in, double y_in,
                           double *x_out, double *y_out )

{
    (void) gcp_count;
    (void) gcps_in;
    (void) gcps_out;
    (void) x_in;
    (void) y_in;
    (void) x_out;
    (void) y_out;

    /* I would appreciate a _brief_ block of code for doing second order
       polynomial regression here! */
    return FALSE;
}


/************************************************************************/
/*                           GTIFImageToPCS()                           */
/************************************************************************/

/**
 * Translate a pixel/line coordinate to projection coordinates.
 *
 * At this time this function does not support image to PCS translations for
 * tiepoints-only definitions,  only pixelscale and transformation matrix
 * formulations.
 *
 * @param gtif The handle from GTIFNew() indicating the target file.
 * @param x A pointer to the double containing the pixel offset on input,
 * and into which the easting/longitude will be put on completion.
 * @param y A pointer to the double containing the line offset on input,
 * and into which the northing/latitude will be put on completion.
 *
 * @return TRUE if the transformation succeeds, or FALSE if it fails.  It may
 * fail if the file doesn't have properly setup transformation information,
 * or it is in a form unsupported by this function.
 */

int GTIFImageToPCS( GTIF *gtif, double *x, double *y )

{
    int     res = FALSE;
    int     tiepoint_count, count, transform_count;
    tiff_t *tif=gtif->gt_tif;
    double *tiepoints   = 0;
    double *pixel_scale = 0;
    double *transform   = 0;


    if (!(gtif->gt_methods.get)(tif, GTIFF_TIEPOINTS,
                              &tiepoint_count, &tiepoints ))
        tiepoint_count = 0;

    if (!(gtif->gt_methods.get)(tif, GTIFF_PIXELSCALE, &count, &pixel_scale ))
        count = 0;

    if (!(gtif->gt_methods.get)(tif, GTIFF_TRANSMATRIX,
                                &transform_count, &transform ))
        transform_count = 0;

/* -------------------------------------------------------------------- */
/*      If the pixelscale count is zero, but we have tiepoints use      */
/*      the tiepoint based approach.                                    */
/* -------------------------------------------------------------------- */
    if( tiepoint_count > 6 && count == 0 )
    {
        res = GTIFTiepointTranslate( tiepoint_count / 6,
                                     tiepoints, tiepoints + 3,
                                     *x, *y, x, y );
    }

/* -------------------------------------------------------------------- */
/*	If we have a transformation matrix, use it. 			*/
/* -------------------------------------------------------------------- */
    else if( transform_count == 16 )
    {
        double x_in = *x, y_in = *y;

        *x = x_in * transform[0] + y_in * transform[1] + transform[3];
        *y = x_in * transform[4] + y_in * transform[5] + transform[7];

        res = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      For now we require one tie point, and a valid pixel scale.      */
/* -------------------------------------------------------------------- */
    else if( count < 3 || tiepoint_count < 6 )
    {
        res = FALSE;
    }

    else
    {
        *x = (*x - tiepoints[0]) * pixel_scale[0] + tiepoints[3];
        *y = (*y - tiepoints[1]) * (-1 * pixel_scale[1]) + tiepoints[4];

        res = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if(tiepoints)
        _GTIFFree(tiepoints);
    if(pixel_scale)
        _GTIFFree(pixel_scale);
    if(transform)
        _GTIFFree(transform);

    return res;
}

/************************************************************************/
/*                           GTIFPCSToImage()                           */
/************************************************************************/

/**
 * Translate a projection coordinate to pixel/line coordinates.
 *
 * At this time this function does not support PCS to image translations for
 * tiepoints-only based definitions, only matrix and pixelscale/tiepoints
 * formulations are supposed.
 *
 * @param gtif The handle from GTIFNew() indicating the target file.
 * @param x A pointer to the double containing the pixel offset on input,
 * and into which the easting/longitude will be put on completion.
 * @param y A pointer to the double containing the line offset on input,
 * and into which the northing/latitude will be put on completion.
 *
 * @return TRUE if the transformation succeeds, or FALSE if it fails.  It may
 * fail if the file doesn't have properly setup transformation information,
 * or it is in a form unsupported by this function.
 */

int GTIFPCSToImage( GTIF *gtif, double *x, double *y )

{
    double 	*tiepoints = NULL;
    int 	tiepoint_count, count, transform_count = 0;
    double	*pixel_scale = NULL;
    double 	*transform   = NULL;
    tiff_t 	*tif=gtif->gt_tif;
    int		result = FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch tiepoints and pixel scale.                                */
/* -------------------------------------------------------------------- */
    if (!(gtif->gt_methods.get)(tif, GTIFF_TIEPOINTS,
                              &tiepoint_count, &tiepoints ))
        tiepoint_count = 0;

    if (!(gtif->gt_methods.get)(tif, GTIFF_PIXELSCALE, &count, &pixel_scale ))
        count = 0;

    if (!(gtif->gt_methods.get)(tif, GTIFF_TRANSMATRIX,
                                &transform_count, &transform ))
        transform_count = 0;

/* -------------------------------------------------------------------- */
/*      If the pixelscale count is zero, but we have tiepoints use      */
/*      the tiepoint based approach.                                    */
/* -------------------------------------------------------------------- */
    if( tiepoint_count > 6 && count == 0 )
    {
        result = GTIFTiepointTranslate( tiepoint_count / 6,
                                        tiepoints + 3, tiepoints,
                                        *x, *y, x, y );
    }

/* -------------------------------------------------------------------- */
/*      Handle matrix - convert to "geotransform" format, invert and    */
/*      apply.                                                          */
/* -------------------------------------------------------------------- */
    else if( transform_count == 16 )
    {
        double  x_in = *x, y_in = *y;
        double	gt_in[6], gt_out[6];

        gt_in[0] = transform[0];
        gt_in[1] = transform[1];
        gt_in[2] = transform[3];
        gt_in[3] = transform[4];
        gt_in[4] = transform[5];
        gt_in[5] = transform[7];

        if( !inv_geotransform( gt_in, gt_out ) )
            result = FALSE;
        else
        {
            *x = x_in * gt_out[0] + y_in * gt_out[1] + gt_out[2];
            *y = x_in * gt_out[3] + y_in * gt_out[4] + gt_out[5];

            result = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      For now we require one tie point, and a valid pixel scale.      */
/* -------------------------------------------------------------------- */
    else if( count >= 3 && tiepoint_count >= 6 )
    {
        *x = (*x - tiepoints[3]) / pixel_scale[0] + tiepoints[0];
        *y = (*y - tiepoints[4]) / (-1 * pixel_scale[1]) + tiepoints[1];

        result = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    if(tiepoints)
        _GTIFFree(tiepoints);
    if(pixel_scale)
        _GTIFFree(pixel_scale);
    if(transform)
        _GTIFFree(transform);

    return result;
}
