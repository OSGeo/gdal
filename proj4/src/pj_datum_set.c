/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Apply datum definition to PJ structure from initialization string.
 * Author:   Frank Warmerdam, warmerda@home.com
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

/* SEC_TO_RAD = Pi/180/3600 */
#define SEC_TO_RAD 4.84813681109535993589914102357e-6

/************************************************************************/
/*                            pj_datum_set()                            */
/************************************************************************/

int pj_datum_set(projCtx ctx, paralist *pl, PJ *projdef)

{
    const char *name, *towgs84, *nadgrids, *catalog;

    projdef->datum_type = PJD_UNKNOWN;

/* -------------------------------------------------------------------- */
/*      Is there a datum definition in the parameters list?  If so,     */
/*      add the defining values to the parameter list.  Note that       */
/*      this will append the ellipse definition as well as the          */
/*      towgs84= and related parameters.  It should also be pointed     */
/*      out that the addition is permanent rather than temporary        */
/*      like most other keyword expansion so that the ellipse           */
/*      definition will last into the pj_ell_set() function called      */
/*      after this one.                                                 */
/* -------------------------------------------------------------------- */
    if( (name = pj_param(ctx, pl,"sdatum").s) != NULL )
    {
        paralist *curr;
        const char *s;
        int i;

        /* find the end of the list, so we can add to it */
        for (curr = pl; curr && curr->next ; curr = curr->next) {}
        
        /* find the datum definition */
        for (i = 0; (s = pj_datums[i].id) && strcmp(name, s) ; ++i) {}

        if (!s) { pj_ctx_set_errno(ctx, -9); return 1; }

        if( pj_datums[i].ellipse_id && strlen(pj_datums[i].ellipse_id) > 0 )
        {
            char	entry[100];
            
            strcpy( entry, "ellps=" );
            strncat( entry, pj_datums[i].ellipse_id, 80 );
            curr = curr->next = pj_mkparam(entry);
        }
        
        if( pj_datums[i].defn && strlen(pj_datums[i].defn) > 0 )
            curr = curr->next = pj_mkparam(pj_datums[i].defn);
    }

/* -------------------------------------------------------------------- */
/*      Check for nadgrids parameter.                                   */
/* -------------------------------------------------------------------- */
    if( (nadgrids = pj_param(ctx, pl,"snadgrids").s) != NULL )
    {
        /* We don't actually save the value separately.  It will continue
           to exist int he param list for use in pj_apply_gridshift.c */

        projdef->datum_type = PJD_GRIDSHIFT;
    }

/* -------------------------------------------------------------------- */
/*      Check for grid catalog parameter, and optional date.            */
/* -------------------------------------------------------------------- */
    else if( (catalog = pj_param(ctx, pl,"scatalog").s) != NULL )
    {
        const char *date;

        projdef->datum_type = PJD_GRIDSHIFT;
        projdef->catalog_name = strdup(catalog);

        date = pj_param(ctx, pl, "sdate").s;
        if( date != NULL) 
            projdef->datum_date = pj_gc_parsedate( ctx, date);
    }

/* -------------------------------------------------------------------- */
/*      Check for towgs84 parameter.                                    */
/* -------------------------------------------------------------------- */
    else if( (towgs84 = pj_param(ctx, pl,"stowgs84").s) != NULL )
    {
        int    parm_count = 0;
        const char *s;

        memset( projdef->datum_params, 0, sizeof(double) * 7);

        /* parse out the parameters */
        for( s = towgs84; *s != '\0' && parm_count < 7; ) 
        {
            projdef->datum_params[parm_count++] = pj_atof(s);
            while( *s != '\0' && *s != ',' )
                s++;
            if( *s == ',' )
                s++;
        }

        if( projdef->datum_params[3] != 0.0 
            || projdef->datum_params[4] != 0.0 
            || projdef->datum_params[5] != 0.0 
            || projdef->datum_params[6] != 0.0 )
        {
            projdef->datum_type = PJD_7PARAM;

            /* transform from arc seconds to radians */
            projdef->datum_params[3] *= SEC_TO_RAD;
            projdef->datum_params[4] *= SEC_TO_RAD;
            projdef->datum_params[5] *= SEC_TO_RAD;
            /* transform from parts per million to scaling factor */
            projdef->datum_params[6] = 
                (projdef->datum_params[6]/1000000.0) + 1;
        }
        else 
            projdef->datum_type = PJD_3PARAM;

        /* Note that pj_init() will later switch datum_type to 
           PJD_WGS84 if shifts are all zero, and ellipsoid is WGS84 or GRS80 */
    }

    return 0;
}
