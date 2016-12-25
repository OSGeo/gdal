/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Apply datum shifts based on grid shift files (normally NAD27 to
 *           NAD83 or the reverse).  This module is responsible for keeping
 *           a list of loaded grids, and calling with each one that is 
 *           allowed for a given datum (expressed as the nadgrids= parameter).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
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
/*                         pj_apply_gridshift()                         */
/*                                                                      */
/*      This is the externally callable interface - part of the         */
/*      public API - though it is not used internally any more and I    */
/*      doubt it is used by any other applications.  But we preserve    */
/*      it to honour our public api.                                    */
/************************************************************************/

int pj_apply_gridshift( projCtx ctx, const char *nadgrids, int inverse, 
                        long point_count, int point_offset,
                        double *x, double *y, double *z )

{
    PJ_GRIDINFO **gridlist;
    int           grid_count;
    int           ret;
    
    gridlist = pj_gridlist_from_nadgrids( ctx, nadgrids, &grid_count );

    if( gridlist == NULL || grid_count == 0 )
        return ctx->last_errno;

    ret = pj_apply_gridshift_3( ctx, gridlist, grid_count, inverse, 
                                point_count, point_offset, x, y, z );

    /* 
    ** Note this frees the array of grid list pointers, but not the grids
    ** which is as intended.  The grids themselves live on.
    */
    pj_dalloc( gridlist );

    return ret;
}

/************************************************************************/
/*                        pj_apply_gridshift_2()                        */
/*                                                                      */
/*      This implmentation takes uses the gridlist from a coordinate    */
/*      system definition.  If the gridlist has not yet been            */
/*      populated in the coordinate system definition we set it up      */
/*      now.                                                            */
/************************************************************************/

int pj_apply_gridshift_2( PJ *defn, int inverse, 
                          long point_count, int point_offset,
                          double *x, double *y, double *z )

{
    if( defn->catalog_name != NULL )
        return pj_gc_apply_gridshift( defn, inverse, point_count, point_offset,
                                      x, y, z );
                                      
    if( defn->gridlist == NULL )
    {
        defn->gridlist = 
            pj_gridlist_from_nadgrids( pj_get_ctx( defn ),
                                       pj_param(defn->ctx, defn->params,"snadgrids").s,
                                       &(defn->gridlist_count) );

        if( defn->gridlist == NULL || defn->gridlist_count == 0 )
            return defn->ctx->last_errno;
    }
     
    return pj_apply_gridshift_3( pj_get_ctx( defn ),
                                 defn->gridlist, defn->gridlist_count, inverse, 
                                 point_count, point_offset, x, y, z );
}


/************************************************************************/
/*                        pj_apply_gridshift_3()                        */
/*                                                                      */
/*      This is the real workhorse, given a gridlist.                   */
/************************************************************************/

int pj_apply_gridshift_3( projCtx ctx, PJ_GRIDINFO **tables, int grid_count,
                          int inverse, long point_count, int point_offset,
                          double *x, double *y, double *z )

{
    int  i;
    static int debug_count = 0;
    (void) z;

    if( tables == NULL || grid_count == 0 )
    {
        pj_ctx_set_errno( ctx, -38);
        return -38;
    }

    ctx->last_errno = 0;

    for( i = 0; i < point_count; i++ )
    {
        long io = i * point_offset;
        LP   input, output;
        int  itable;

        input.phi = y[io];
        input.lam = x[io];
        output.phi = HUGE_VAL;
        output.lam = HUGE_VAL;

        /* keep trying till we find a table that works */
        for( itable = 0; itable < grid_count; itable++ )
        {
            PJ_GRIDINFO *gi = tables[itable];
            struct CTABLE *ct = gi->ct;
            double epsilon = (fabs(ct->del.phi)+fabs(ct->del.lam))/10000.0;

            /* skip tables that don't match our point at all.  */
            if( ct->ll.phi - epsilon > input.phi 
                || ct->ll.lam - epsilon > input.lam
                || (ct->ll.phi + (ct->lim.phi-1) * ct->del.phi + epsilon 
                    < input.phi)
                || (ct->ll.lam + (ct->lim.lam-1) * ct->del.lam + epsilon 
                    < input.lam) )
                continue;

            /* If we have child nodes, check to see if any of them apply. */
            while( gi->child )
            {
                PJ_GRIDINFO *child;

                for( child = gi->child; child != NULL; child = child->next )
                {
                    struct CTABLE *ct1 = child->ct;
                    double epsilon = 
                        (fabs(ct1->del.phi)+fabs(ct1->del.lam))/10000.0;

                    if( ct1->ll.phi - epsilon > input.phi 
                        || ct1->ll.lam - epsilon > input.lam
                        || (ct1->ll.phi+(ct1->lim.phi-1)*ct1->del.phi + epsilon 
                            < input.phi)
                        || (ct1->ll.lam+(ct1->lim.lam-1)*ct1->del.lam + epsilon 
                            < input.lam) )
                        continue;

                    break;
                }

                /* If we didn't find a child then nothing more to do */

                if( child == NULL ) break;

                /* Otherwise use the child, first checking it's children */

                gi = child;
                ct = child->ct;
            }

            /* load the grid shift info if we don't have it. */
            if( ct->cvs == NULL && !pj_gridinfo_load( ctx, gi ) )
            {
                pj_ctx_set_errno( ctx, -38 );
                return -38;
            }
            
            output = nad_cvt( input, inverse, ct );
            if( output.lam != HUGE_VAL )
            {
                if( debug_count++ < 20 )
                    pj_log( ctx, PJ_LOG_DEBUG_MINOR,
                            "pj_apply_gridshift(): used %s", ct->id );
                break;
            }
        }

        if( output.lam == HUGE_VAL )
        {
            if( ctx->debug_level >= PJ_LOG_DEBUG_MAJOR )
            {
                pj_log( ctx, PJ_LOG_DEBUG_MAJOR,
                    "pj_apply_gridshift(): failed to find a grid shift table for\n"
                    "                      location (%.7fdW,%.7fdN)",
                    x[io] * RAD_TO_DEG, 
                    y[io] * RAD_TO_DEG );
                for( itable = 0; itable < grid_count; itable++ )
                {
                    PJ_GRIDINFO *gi = tables[itable];
                    if( itable == 0 )
                        pj_log( ctx, PJ_LOG_DEBUG_MAJOR,
                                "   tried: %s", gi->gridname );
                    else
                        pj_log( ctx, PJ_LOG_DEBUG_MAJOR,
                                ",%s", gi->gridname );
                }
            }

            /* 
             * We don't actually have any machinery currently to set the 
             * following macro, so this is mostly kept here to make it clear 
             * how we ought to operate if we wanted to make it super clear 
             * that an error has occured when points are outside our available
             * datum shift areas.  But if this is on, we will find that "low 
             * value" points on the fringes of some datasets will completely 
             * fail causing lots of problems when it is more or less ok to 
             * just not apply a datum shift.  So rather than deal with
             * that we just fallback to no shift. (see also bug #45).
             */
#ifdef ERR_GRID_AREA_TRANSIENT_SEVERE
            y[io] = HUGE_VAL;
            x[io] = HUGE_VAL;
#else
            /* leave x/y unshifted. */
#endif
        }
        else
        {
            y[io] = output.phi;
            x[io] = output.lam;
        }
    }

    return 0;
}

