/******************************************************************************
 * $Id: geo_trans.c,v 1.4 1999/09/17 01:19:51 warmerda Exp $
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
 ******************************************************************************
 *
 * $Log: geo_trans.c,v $
 * Revision 1.4  1999/09/17 01:19:51  warmerda
 * Fixed bug in use of transform matrix.
 *
 * Revision 1.3  1999/09/16 21:25:40  warmerda
 * Added tiepoint, and transformation matrix based translation.  Note
 * that we don't try to invert the transformation matrix for
 * GTIFPCSToImage().
 *
 * Revision 1.2  1999/09/07 20:00:40  warmerda
 * Fixed count/tiepoint_count bug in GTIFPCSToImage().
 *
 * Revision 1.1  1999/05/04 03:07:57  warmerda
 * New
 *
 */
 
#include "geotiff.h"
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */
#include "geokeys.h"

/************************************************************************/
/*                       GTIFTiepointTranslate()                        */
/************************************************************************/

int GTIFTiepointTranslate( int gcp_count, double * gcps_in, double * gcps_out,
                           double x_in, double y_in,
                           double *x_out, double *y_out )

{
    int		i;
    double	sum, epsilon = 1e-15;

#define X_IN(i)  (gcps_in[(i) * 6])    
#define Y_IN(i)  (gcps_in[(i) * 6 + 1])    
#define X_OUT(i) (gcps_in[(i) * 6 + 3])    
#define Y_OUT(i) (gcps_in[(i) * 6 + 4])    
    
/* -------------------------------------------------------------------- */
/*      Compute a sum of the inverse distances to gcps so that we       */
/*      can use them in a weighted average.  If we find an exact hit    */
/*      we will just return it immediately to avoid divide by zero      */
/*      errors.                                                         */
/* -------------------------------------------------------------------- */
    sum = 0.0;
    for( i = 0; i < gcp_count; i++ )
    {
        double 		x_dist, y_dist, distance;
        
        x_dist = ABS(X_IN(i) - x_in);
        y_dist = ABS(Y_IN(i) - y_in);
        distance = sqrt( x_dist * x_dist + y_dist * y_dist );

        if( distance < epsilon )
        {
            *x_out = X_OUT(i);
            *y_out = Y_OUT(i);
            return TRUE;
        }

        sum += 1.0 / distance;
    }
    
/* -------------------------------------------------------------------- */
/*      Apply the vector to each GCP in proportion to it's              */
/*      contribution to the sum of inverse distances.                   */
/* -------------------------------------------------------------------- */
    *x_out = 0.0;
    *y_out = 0.0;
       
    
    for( i = 0; i < gcp_count; i++ )
    {
        double 		x_dist, y_dist, distance, ratio;
        
        x_dist = ABS(X_IN(i) - x_in);
        y_dist = ABS(Y_IN(i) - y_in);
        distance = sqrt( x_dist * x_dist + y_dist * y_dist );

        ratio = (1.0/distance) / sum;

        *x_out += X_OUT(i) * ratio;
        *y_out += Y_OUT(i) * ratio;
    }

    return TRUE;
}


/************************************************************************/
/*                           GTIFImageToPCS()                           */
/************************************************************************/

/**
 * Translate a pixel/line coordinate to projection coordinates.
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
    double 	*tiepoints;
    int 	tiepoint_count, count, transform_count;
    double	*pixel_scale;
    tiff_t *tif=gtif->gt_tif;
    double      *transform;

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
        return GTIFTiepointTranslate( tiepoint_count / 6,
                                      tiepoints, tiepoints + 3,
                                      *x, *y, x, y );
    }
    
/* -------------------------------------------------------------------- */
/*	If we have a transformation matrix, use it. 			*/
/* -------------------------------------------------------------------- */
    if( transform_count == 16 )
    {
        double		x_in = *x, y_in = *y;

        *x = x_in * transform[0] + y_in * transform[1] + transform[3];
        *y = x_in * transform[4] + y_in * transform[5] + transform[7];
        
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      For now we require one tie point, and a valid pixel scale.      */
/* -------------------------------------------------------------------- */
    if( count < 3 || tiepoint_count < 6 )
        return FALSE;

    *x = (*x - tiepoints[0]) * pixel_scale[0] + tiepoints[3];
    *y = (*y - tiepoints[1]) * (-1 * pixel_scale[1]) + tiepoints[4];

    return TRUE;
}

/************************************************************************/
/*                           GTIFPCSToImage()                           */
/************************************************************************/

int GTIFPCSToImage( GTIF *gtif, double *x, double *y )

{
    double 	*tiepoints;
    int 	tiepoint_count, count;
    double	*pixel_scale;
    tiff_t *tif=gtif->gt_tif;

    if (!(gtif->gt_methods.get)(tif, GTIFF_TIEPOINTS,
                              &tiepoint_count, &tiepoints ))
        tiepoint_count = 0;

    if (!(gtif->gt_methods.get)(tif, GTIFF_PIXELSCALE, &count, &pixel_scale ))
        count = 0;

/* -------------------------------------------------------------------- */
/*      If the pixelscale count is zero, but we have tiepoints use      */
/*      the tiepoint based approach.                                    */
/* -------------------------------------------------------------------- */
    if( tiepoint_count > 6 && count == 0 )
    {
        return GTIFTiepointTranslate( tiepoint_count / 6,
                                      tiepoints + 3, tiepoints,
                                      *x, *y, x, y );
    }
    
/* -------------------------------------------------------------------- */
/*      For now we require one tie point, and a valid pixel scale.      */
/* -------------------------------------------------------------------- */
    if( count < 3 || tiepoint_count < 6 )
        return FALSE;

    *x = (*x - tiepoints[3]) / pixel_scale[0] + tiepoints[0];
    *y = (*y - tiepoints[4]) / (-1 * pixel_scale[1]) + tiepoints[1];

    return TRUE;
}

