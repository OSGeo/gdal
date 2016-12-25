/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Some utility functions we don't want to bother putting in
 *           their own source files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#define PJ_LIB__

#include <projects.h>
#include <string.h>
#include <math.h>

/************************************************************************/
/*                           pj_is_latlong()                            */
/*                                                                      */
/*      Returns TRUE if this coordinate system object is                */
/*      geographic.                                                     */
/************************************************************************/

int pj_is_latlong( PJ *pj )

{
    return pj == NULL || pj->is_latlong;
}

/************************************************************************/
/*                           pj_is_geocent()                            */
/*                                                                      */
/*      Returns TRUE if this coordinate system object is geocentric.    */
/************************************************************************/

int pj_is_geocent( PJ *pj )

{
    return pj != NULL && pj->is_geocent;
}

/************************************************************************/
/*                        pj_latlong_from_proj()                        */
/*                                                                      */
/*      Return a PJ* definition defining the lat/long coordinate        */
/*      system on which a projection is based.  If the coordinate       */
/*      system passed in is latlong, a clone of the same will be        */
/*      returned.                                                       */
/************************************************************************/

PJ *pj_latlong_from_proj( PJ *pj_in )

{
    char	defn[512];
    int		got_datum = FALSE;

    pj_errno = 0;
    strcpy( defn, "+proj=latlong" );

    if( pj_param(pj_in->ctx, pj_in->params, "tdatum").i )
    {
        got_datum = TRUE;
        sprintf( defn+strlen(defn), " +datum=%s",
                 pj_param(pj_in->ctx, pj_in->params,"sdatum").s );
    }
    else if( pj_param(pj_in->ctx, pj_in->params, "tellps").i )
    {
        sprintf( defn+strlen(defn), " +ellps=%s",
                 pj_param(pj_in->ctx, pj_in->params,"sellps").s );
    }
    else if( pj_param(pj_in->ctx,pj_in->params, "ta").i )
    {
        sprintf( defn+strlen(defn), " +a=%s",
                 pj_param(pj_in->ctx,pj_in->params,"sa").s );

        if( pj_param(pj_in->ctx,pj_in->params, "tb").i )
            sprintf( defn+strlen(defn), " +b=%s",
                     pj_param(pj_in->ctx,pj_in->params,"sb").s );
        else if( pj_param(pj_in->ctx,pj_in->params, "tes").i )
            sprintf( defn+strlen(defn), " +es=%s",
                     pj_param(pj_in->ctx,pj_in->params,"ses").s );
        else if( pj_param(pj_in->ctx,pj_in->params, "tf").i )
            sprintf( defn+strlen(defn), " +f=%s",
                     pj_param(pj_in->ctx,pj_in->params,"sf").s );
        else
        {
            char* ptr = defn+strlen(defn);
            sprintf( ptr, " +es=%.16g",  pj_in->es );
            for(; *ptr; ptr++)
            {
                if( *ptr == ',' )
                    *ptr = '.';
            }
        }
    }
    else
    {
        pj_ctx_set_errno( pj_in->ctx, -13 );

        return NULL;
    }

    if( !got_datum )
    {
        if( pj_param(pj_in->ctx,pj_in->params, "ttowgs84").i )
            sprintf( defn+strlen(defn), " +towgs84=%s",
                     pj_param(pj_in->ctx,pj_in->params,"stowgs84").s );

        if( pj_param(pj_in->ctx,pj_in->params, "tnadgrids").i )
            sprintf( defn+strlen(defn), " +nadgrids=%s",
                     pj_param(pj_in->ctx,pj_in->params,"snadgrids").s );
    }

    /* copy over some other information related to ellipsoid */
    if( pj_param(pj_in->ctx,pj_in->params, "tR").i )
        sprintf( defn+strlen(defn), " +R=%s",
                 pj_param(pj_in->ctx,pj_in->params,"sR").s );

    if( pj_param(pj_in->ctx,pj_in->params, "tR_A").i )
        sprintf( defn+strlen(defn), " +R_A" );

    if( pj_param(pj_in->ctx,pj_in->params, "tR_V").i )
        sprintf( defn+strlen(defn), " +R_V" );

    if( pj_param(pj_in->ctx,pj_in->params, "tR_a").i )
        sprintf( defn+strlen(defn), " +R_a" );

    if( pj_param(pj_in->ctx,pj_in->params, "tR_lat_a").i )
        sprintf( defn+strlen(defn), " +R_lat_a=%s",
                 pj_param(pj_in->ctx,pj_in->params,"sR_lat_a").s );

    if( pj_param(pj_in->ctx,pj_in->params, "tR_lat_g").i )
        sprintf( defn+strlen(defn), " +R_lat_g=%s",
                 pj_param(pj_in->ctx,pj_in->params,"sR_lat_g").s );

    /* copy over prime meridian */
    if( pj_param(pj_in->ctx,pj_in->params, "tpm").i )
        sprintf( defn+strlen(defn), " +pm=%s",
                 pj_param(pj_in->ctx,pj_in->params,"spm").s );

    return pj_init_plus_ctx( pj_in->ctx, defn );
}

/************************************************************************/
/*                        pj_get_spheroid_defn()                        */
/*                                                                      */
/*      Fetch the internal definition of the spheroid.  Note that       */
/*      you can compute "b" from eccentricity_squared as:               */
/*                                                                      */
/*      b = a * sqrt(1 - es)                                            */
/************************************************************************/

void pj_get_spheroid_defn(projPJ defn, double *major_axis, double *eccentricity_squared)
{
	if ( major_axis )
		*major_axis = defn->a;

	if ( eccentricity_squared )
		*eccentricity_squared = defn->es;
}
